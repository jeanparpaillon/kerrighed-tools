/** Kerrighed Callback-Library Implementation
 *  @file libkrgcb.c
 *
 *  @author Eugen Feller, Matthieu Fertré, John Mehnert-Spahn
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include <asm/types.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <strings.h>
#include <kerrighed.h>
#include <libkrgcb.h>

#define CR_CB_ABORT(f, a...) do { \
	printf(f, ##a); exit(EXIT_FAILURE); \
	} while (0)

#ifdef DEBUG
#define CR_CB_DEBUG(f, a...) printf(f, ##a)
#else
#define CR_CB_DEBUG(f, a...) do {} while(0)
#endif

/* Signals */
#define SIG_CB_RUN_CHKPT 37
#define SIG_CB_RUN_RST 38
#define SIG_CB_RUN_CNT 39

/* Max path size */
#define CHKPT_PATH_SIZE 60

/* Max nb of callbacks */
#define CR_CB_MAX_CALLBACKS 40

/* Active CB bit */
#define CR_CB_ACTIV_CB (1 << 0)

/* Message codes */
enum cr_cb_status {
	CR_CB_CHKPT_APP = 1,
	CR_CB_ERR = 2
};

/* Hooks */
enum cr_cb_hook {
	CR_CB_CHECKPOINT = 1,
	CR_CB_RESTART = 2,
	CR_CB_CONTINUE = 3
};

enum cr_cb_context {
	SIGNAL_CONTEXT,
	THREAD_CONTEXT
};

/* struct holds function pointer + argument pointer */
struct cr_cb_callback_s {
	cr_cb_callback_t func;
	void *arg;
	int thread_cb;
};

/* callback count */
typedef struct cr_cb_count_s {
	int chkpt;
	int rst;
	int cnt;
} cr_cb_count_t;

/* buffer for message queue */
typedef struct cr_cb_msgbuf_s {
	long mtype;
	char mtext[2];
} cr_cb_msgbuf_t;

/* info structure */
typedef struct cr_cb_info_s {
	cr_cb_count_t cr_cb_count;
	struct cr_cb_callback_s cr_cb_chkpt[CR_CB_MAX_CALLBACKS];
	struct cr_cb_callback_s cr_cb_rst[CR_CB_MAX_CALLBACKS];
	struct cr_cb_callback_s cr_cb_cnt[CR_CB_MAX_CALLBACKS];
} cr_cb_info_t;

cr_cb_info_t *info = NULL;
int thread_running = 0;
pthread_t cb_thread;
enum cr_cb_hook current_hook;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static int cb_count_read(enum cr_cb_hook hook, cr_cb_count_t *cb)
{
	if (!info)
		CR_CB_ABORT("Called without first calling cr_init()\n");

	switch (hook) {
		case CR_CB_CHECKPOINT:
			return cb->chkpt;
		case CR_CB_RESTART:
			return cb->rst;
		case CR_CB_CONTINUE:
			return cb->cnt;
		default:
			break;
	}

	return -1;
}

static int cb_count_write(enum cr_cb_hook hook, int value,
			  cr_cb_count_t *cb)
{
	if (!info)
		CR_CB_ABORT("Called without first calling cr_init()\n");

	switch (hook) {
		case CR_CB_CHECKPOINT:
			cb->chkpt = value;
			return 0;
		case CR_CB_RESTART:
			cb->rst = value;
			return 0;
		case CR_CB_CONTINUE:
			cb->cnt = value;
			return 0;
		default:
			break;
	}

	return -1;
}

static int send_message(int msg)
{
	key_t key;
	cr_cb_msgbuf_t buf;
	int msqid;
	int r = 0;

	key = getpid();

	msqid = msgget(key, 0644);
	if (msqid < 0) {
		perror("libkrgcb.c::send_message::msgget");
		CR_CB_DEBUG("msgget failed\n");
		r = -1;
		goto err;
	}

	buf.mtype = msg;

	if (msgsnd(msqid, (cr_cb_msgbuf_t *)&buf, sizeof(buf), 0)) {
		perror("libkrgcb.c::send_message::msgsnd");
		CR_CB_DEBUG("msgsnd failed\n");
		r = -1;
	}
err:
	return r;
}

static int cr_detect_callbacks(long pid, short from_appid)
{
	int r = 0;
	__u64 udata = 0;

	if (from_appid)
		r = application_get_userdata_from_appid(pid, &udata);
	else
		r = application_get_userdata_from_pid(pid, &udata);

	if (r)
		goto err;

	if (!(udata & CR_CB_ACTIV_CB))
		r = -1;
err:
	return r;
}

int cr_execute_chkpt_callbacks(long pid, short from_appid)
{
	cr_cb_msgbuf_t buf;
	key_t key;
	int msqid;
	int r = 0;

	if (cr_detect_callbacks(pid, from_appid))
		goto err;

	key = pid;

	msqid = msgget(key, 0644 | IPC_CREAT);
	if (msqid < 0) {
		r = -1;
		goto err;
	}

	r = kill(pid, SIG_CB_RUN_CHKPT);
	if (r)
		goto err;

	if (msgrcv(msqid, (cr_cb_msgbuf_t *)&buf, sizeof(buf),
		   0, 0) < 0) {
		r = -1;
		goto err;
	}

	r = msgctl(msqid, IPC_RMID, NULL);
	if (r)
		goto err;

	if (buf.mtype == CR_CB_ERR)
		r = -1;
err:
	return r;
}

int cr_execute_restart_callbacks(long pid)
{
	int r = 0;

	if (cr_detect_callbacks(pid, 0))
		goto err;

	r = kill(pid, SIG_CB_RUN_RST);
	if (r)
		goto err;
err:
	return r;
}

int cr_execute_continue_callbacks(long pid, short from_appid)
{
	int r = 0;

	if (cr_detect_callbacks(pid, from_appid))
		goto err;

	r = kill(pid, SIG_CB_RUN_CNT);
	if (r)
		goto err;
err:
	return r;
}

static int run_callbacks(enum cr_cb_context context)
{
	struct cr_cb_callback_s *cr_cb;
	int idx;
	int r = 0;

	if (!info)
		CR_CB_ABORT("Called without first calling cr_init()\n");

	switch (current_hook) {
		case CR_CB_CHECKPOINT:
			cr_cb = info->cr_cb_chkpt;
			break;
		case CR_CB_RESTART:
			cr_cb = info->cr_cb_rst;
			break;
		case CR_CB_CONTINUE:
			cr_cb = info->cr_cb_cnt;
			break;
		default:
			CR_CB_DEBUG("Unknown hook: %d\n", current_hook);
			r = -1;
			goto err;
	}

	for (idx = 0; idx < cb_count_read(current_hook, &info->cr_cb_count);
	     idx++) {
		switch (context) {
			case SIGNAL_CONTEXT:
				if (cr_cb[idx].func && !cr_cb[idx].thread_cb) {
					r = (*cr_cb[idx].func)(cr_cb[idx].arg);
					if (r) {
						CR_CB_DEBUG("Callback %d \
							    returned %d:\n",
							    idx, r);
						goto err;
					}
				}
				break;
			case THREAD_CONTEXT:
				if (cr_cb[idx].func && cr_cb[idx].thread_cb) {
					r = (*cr_cb[idx].func)(cr_cb[idx].arg);
					if (r) {
						CR_CB_DEBUG("Callback %d \
							    returned %d:\n",
							    idx, r);
						goto err;
					}
				}
				break;
			default:
				CR_CB_DEBUG("Unknown context: %d\n", context);
				r = -1;
				goto err;
		}
	}
err:
	CR_CB_DEBUG("run_callbacks ret: %d\n", r);
	return r;
}

static void handle_signal(int signum)
{
	int r;

	if (!info)
		CR_CB_ABORT("Called without first calling cr_init()\n");

	switch (signum) {
		case SIG_CB_RUN_CHKPT:
			current_hook = CR_CB_CHECKPOINT;
			break;
		case SIG_CB_RUN_RST:
			current_hook = CR_CB_RESTART;
			break;
		case SIG_CB_RUN_CNT:
			current_hook = CR_CB_CONTINUE;
			break;
		default:
			CR_CB_DEBUG("Bad signal received: %d\n", signum);
			return;
	}

	r = run_callbacks(SIGNAL_CONTEXT);
	if (current_hook == CR_CB_CHECKPOINT) {
		if (r)
			r = send_message(CR_CB_ERR);
		else if (!thread_running)
			r = send_message(CR_CB_CHKPT_APP);

		CR_CB_DEBUG("handle_signal - send_message ret: %d\n", r);
	}

	CR_CB_DEBUG("Waking up worker thread\n");
	if (thread_running && pthread_mutex_unlock(&mutex))
		CR_CB_DEBUG("Error while releasing mutex");
}

static void* worker_thread(void *arg)
{
	int r = 0;

	if (!info)
		CR_CB_ABORT("Called without first calling cr_init()\n");

	while (thread_running) {
		CR_CB_DEBUG("Locking worker thread\n");
		if (pthread_mutex_lock(&mutex))
			CR_CB_DEBUG("Error while acquire mutex");

		CR_CB_DEBUG("Worker thread executing cb\n");
		r = run_callbacks(THREAD_CONTEXT);

		if (current_hook == CR_CB_CHECKPOINT) {
			if (r)
				r = send_message(CR_CB_ERR);
			else
				r = send_message(CR_CB_CHKPT_APP);

			CR_CB_DEBUG("worker_thread - send_message ret: \
				    %d\n", r);
		}
	}

	CR_CB_DEBUG("End of worker thread\n");

	return (void *)((long)r);
}

static void free_info_memory(void)
{
	if (info) {
		free(info);
		info = NULL;
	}

	thread_running = 0;
}

static int initialize_cb_thread(void)
{
	int r;

	pthread_mutex_lock(&mutex);

	thread_running = 1;

	/* Init worker thread for callback handling */
	r = pthread_create(&cb_thread, NULL, worker_thread, NULL);
	if (r)
		goto err;

	return r;

err:
	free_info_memory();
	return r;
}

int cr_callback_init(void)
{
	struct sigaction sa;
	__u64 udata = 0;
	int r = 0;

	info = (cr_cb_info_t *)malloc(sizeof(cr_cb_info_t));

	if (!info) {
		r = -ENOMEM;
		goto err;
	}

	memset(info, 0, sizeof(cr_cb_info_t));

	udata |= CR_CB_ACTIV_CB;

	r = application_set_userdata(udata);
	if (r)
		goto err;

	sa.sa_handler = handle_signal;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);

	r = sigaction(SIG_CB_RUN_CHKPT, &sa, NULL);
	if (r)
		goto err;

	r = sigaction(SIG_CB_RUN_RST, &sa, NULL);
	if (r)
		goto err;

	r = sigaction(SIG_CB_RUN_CNT, &sa, NULL);

err:
	if (r)
		free_info_memory();

	return r;
}

void cr_callback_exit(void)
{
	if (info)
		free_info_memory();
	else
		CR_CB_ABORT("Called without first calling cr_init()\n");
}

static int register_callback(enum cr_cb_hook hook, cr_cb_callback_t func,
			     void *arg, enum cr_cb_context context)
{
	int idx;
	struct cr_cb_callback_s *cr_cb;

	if (!info)
		CR_CB_ABORT("Called without first calling cr_init()\n");

	if (context == THREAD_CONTEXT) {
		if (!thread_running) {
			int r = initialize_cb_thread();
			if (r)
				return r;
		}
	}

	switch (hook) {
		case CR_CB_CHECKPOINT:
			cr_cb = info->cr_cb_chkpt;
			break;
		case CR_CB_RESTART:
			cr_cb = info->cr_cb_rst;
			break;
		case CR_CB_CONTINUE:
			cr_cb = info->cr_cb_cnt;
			break;
		default:
			return -1;
	}

	idx = cb_count_read(hook, &info->cr_cb_count);

	CR_CB_DEBUG("idx:%d\n", idx);

	if (idx < CR_CB_MAX_CALLBACKS) {
		cr_cb[idx].func = func;
		cr_cb[idx].arg = arg;

		if (context == SIGNAL_CONTEXT)
			cr_cb[idx].thread_cb = 0;
		else
			cr_cb[idx].thread_cb = 1;

		cb_count_write(hook, idx+1, &info->cr_cb_count);
	} else
		return -1;

	return 0;
}

// Signal handler context callback registration

int cr_register_chkpt_callback(cr_cb_callback_t func, void *arg)
{
	return register_callback(CR_CB_CHECKPOINT, func, arg,
				 SIGNAL_CONTEXT);
}

int cr_register_restart_callback(cr_cb_callback_t func, void *arg)
{
	return register_callback(CR_CB_RESTART, func, arg,
				 SIGNAL_CONTEXT);
}

int cr_register_continue_callback(cr_cb_callback_t func, void *arg)
{
	return register_callback(CR_CB_CONTINUE, func, arg,
				 SIGNAL_CONTEXT);
}

// Thread context callbacks registration

int cr_register_chkpt_thread_callback(cr_cb_callback_t func, void *arg)
{
	return register_callback(CR_CB_CHECKPOINT, func, arg,
				 THREAD_CONTEXT);
}

int cr_register_restart_thread_callback(cr_cb_callback_t func, void *arg)
{
	return register_callback(CR_CB_RESTART, func, arg,
				 THREAD_CONTEXT);
}

int cr_register_continue_thread_callback(cr_cb_callback_t func, void *arg)
{
	return register_callback(CR_CB_CONTINUE, func, arg,
				 THREAD_CONTEXT);
}
