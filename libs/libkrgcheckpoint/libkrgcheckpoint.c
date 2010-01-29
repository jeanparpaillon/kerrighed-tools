/** Kerrighed Checkpoint-Library Interface
 *  @file libkrgcheckpoint.c
 *
 *  @author Matthieu Fertré
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include <types.h>
#include <kerrighed_tools.h>
#include <checkpoint.h>
#include <libkrgcb.h>
#include <libkrgcheckpoint.h>

int cr_disable(void)
{
	return call_kerrighed_services(KSYS_APP_CR_DISABLE, NULL);
}

int cr_enable(void)
{
	return call_kerrighed_services(KSYS_APP_CR_ENABLE, NULL);
}

/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:        the pointer to the member.
 * @type:       the type of the container struct this is embedded in.
 * @member:     the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({                      \
			const typeof( ((type *)0)->member ) *__mptr = (ptr); \
			(type *)( (char *)__mptr - offsetof(type,member) );})

struct cr_mm_region_excluded
{
	struct cr_mm_region region;

	cr_mm_excl_callback_t func;
	void *arg;
};

static
struct cr_mm_region_excluded *next_region(struct cr_mm_region_excluded *mm_region)
{
	struct cr_mm_region_excluded *next;
	struct cr_mm_region *intern;

	intern = mm_region->region.next;
	if (intern)
		next = container_of(intern,
				    struct cr_mm_region_excluded,
				    region);
	else
		next = NULL;

	return next;
}

struct child_process
{
	pid_t pid;
	struct child_process *next;
};

static struct cr_mm_region_excluded *first_mm_region_excluded = NULL;
static struct child_process *first_child = NULL;
static int initialized = 0;
static int pipefd[2];
int cr_mm_exclusion_init(void);

static void add_child(pid_t pid)
{
	struct child_process *child;
	int r;

	r = cr_mm_exclusion_init();
	if (r)
		return;

	child = malloc(sizeof(struct child_process));
	if (!child)
		return;

	child->pid = pid;
	if (first_child)
		child->next = first_child->next;
	else
		child->next = NULL;

	first_child = child;
}

static void remove_all_children()
{
	struct child_process *child, *next;

	child = first_child;
	while (child) {
		next = child->next;

		free(child);
		child = next;
	}
	first_child = NULL;
}

static void remove_child(pid_t pid)
{
	struct child_process *child, *prev;

	prev = NULL;
	child = first_child;
	while (child) {
		if (child->pid == pid) {
			if (!prev)
				first_child = child->next;
			else
				prev->next = child->next;

			free(child);
			return;
		}

		prev = child;
		child = child->next;
	}
}

static void handle_sigchild(int signum, siginfo_t *siginfo, void *context)
{
	remove_child(siginfo->si_pid);
}

static int initiliaze_sigchild_handler(void)
{
	int r;
	struct sigaction sa;

	sa.sa_sigaction = handle_sigchild;
	sa.sa_flags = SA_SIGINFO;

	r = sigaction(SIGCHLD, &sa, NULL);

	return r;
}

int cr_mm_exclusion_checkpoint_cb(void *arg)
{
	int r;
	struct cr_mm_region_excluded *mm_region;
	struct child_process *child;

	if (!first_mm_region_excluded)
		return 0;

	mm_region = first_mm_region_excluded;
	while (mm_region) {

		/* update pid is needed in case of fork */
		mm_region->region.pid = getpid();

		mm_region = next_region(mm_region);
	}

	r = call_kerrighed_services(KSYS_APP_CR_EXCLUDE,
				    first_mm_region_excluded);

	if (r)
		perror("cr_exclude");

	child = first_child;
	while (child) {
		cr_execute_chkpt_callbacks(child->pid, 0);
		child = child->next;
	}

	return r;
}

int cr_mm_exclusion_restart_cb(void *arg)
{
	struct cr_mm_region_excluded *mm_region;
	struct child_process *child;
	int ret = 0, r = 0;

	mm_region = first_mm_region_excluded;
	while (mm_region) {
		if (mm_region->func) {
			r = (*mm_region->func)(mm_region->arg);
			if (r)
				ret = r;
		}

		mm_region = next_region(mm_region);
	}

	child = first_child;
	while (child) {
		cr_execute_restart_callbacks(child->pid);
		child = child->next;
	}

	return ret;
}

void parent_before_fork(void)
{
	int r;
	r = pipe(pipefd);
	if (r)
		perror("pipe");
}

void parent_after_fork(void)
{
	int r;
	pid_t pid;

	r = read(pipefd[0], &pid, sizeof(pid_t));
	if (r == -1)
		perror("read");

	close(pipefd[0]);
	close(pipefd[1]);

	add_child(pid);
}

void child_after_fork(void)
{
	int r;
	pid_t pid = getpid();

	remove_all_children();

	close(pipefd[0]);
	r = write(pipefd[1], &pid, sizeof(pid_t));
	if (r == -1)
		perror("write");

	close(pipefd[1]);
}

int cr_mm_exclusion_init(void)
{
	int r;

	if (initialized)
		return 0;

	r = cr_callback_init();
	if (r)
		goto out;

	r = cr_register_chkpt_callback(&cr_mm_exclusion_checkpoint_cb,
				       NULL);
	if (r)
		goto out;

	r = cr_register_restart_callback(&cr_mm_exclusion_restart_cb,
					 NULL);
	if (r)
		goto out;

	r = initiliaze_sigchild_handler();
	if (r)
		goto out;

	r = pthread_atfork(parent_before_fork, parent_after_fork,
			   child_after_fork);
	if (r)
		goto out;

	initialized = 1;

out:
	return r;
}

void __attribute__ ((destructor)) cr_mm_exclusion_exit(void)
{
	struct cr_mm_region_excluded *mm_region, *next;

	mm_region = first_mm_region_excluded;
	while (mm_region) {
		next = next_region(mm_region);

		free(mm_region);
		mm_region = next;
	}

	remove_all_children();
}

int cr_exclude_on(void *data, size_t datasize,
		  cr_mm_excl_callback_t func, void *arg)
{
	struct cr_mm_region_excluded *mm_region, *element, *next;
	int r;

	mm_region = malloc(sizeof(struct cr_mm_region_excluded));
	if (!mm_region)
		return -1;

	mm_region->region.pid = 0;
	mm_region->region.addr = (unsigned long)data;
	mm_region->region.size = datasize;

	mm_region->func = func;
	mm_region->arg = arg;

	if (!first_mm_region_excluded)
		first_mm_region_excluded = mm_region;
	else {
		next = first_mm_region_excluded;

		do {
			element = next;
			if (element->region.addr == mm_region->region.addr)
				goto err_busy;

			next = next_region(element);
		} while (next);

		element->region.next = &mm_region->region;
	}

	r = cr_mm_exclusion_init();
	if (r)
		free(mm_region);

	return r;

err_busy:
	errno = EBUSY;
	free(mm_region);
	return -1;
}

int cr_exclude_off(void *data)
{
	struct cr_mm_region_excluded *prev_region, *mm_region;

	mm_region = first_mm_region_excluded;
	prev_region = NULL;

	while (mm_region) {
		if (mm_region->region.addr == (unsigned long)data) {
			if (prev_region)
				prev_region->region.next =
					mm_region->region.next;
			else
				first_mm_region_excluded = next_region(mm_region);

			free(mm_region);
			return 0;
		}
		prev_region = mm_region;
		mm_region = next_region(mm_region);
	}

	errno = EINVAL;
	return -1;
}
