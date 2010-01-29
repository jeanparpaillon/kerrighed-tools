#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h>

#define MAX_MSG_SIZE 8192

typedef struct msgbuf {
	long    type;
	char    text[MAX_MSG_SIZE];
} message_buf; /* Message structure */

char buffer[MAX_MSG_SIZE];

enum operation{
	SEND,
	RECEIVE,
	NOTHING
};

enum operation todo = NOTHING;
int create = 0;
int type = 1;
int use_id = 0;
int quiet = 0;
int blocking = 0;

void print_msg(const char *format, ...)
{
	if (quiet)
		return;

	va_list params;
	va_start(params, format);
	vprintf(format, params);
	va_end(params);
}

key_t get_key(const char *path)
{
	key_t key;
	key = ftok(path, 'R');
	if (key == -1)
		perror("ftok");

	return key;
}

/* return -1 in case of error */
int create_msg_queue(const char *path)
{
	key_t key;
	int msgid = -1;

	key = get_key(path);
	if (key == -1)
		return -1;

	msgid = msgget(key, IPC_CREAT | IPC_EXCL | 0600);
	if (msgid == -1)
		fprintf(stderr, "create_msg_queue(%s)::msgget: %s\n", path,
			strerror(errno));

	return msgid;
}

/* return -1 in case of error */
int get_msg_queue(const char *path)
{
	key_t key;
	int msgid = -1;

	key = get_key(path);
	if (key == -1)
		return -1;

	msgid = msgget(key, 0);
	if (msgid == -1)
		fprintf(stderr, "get_msg_queue(%s)::msgget: %s\n", path,
			strerror(errno));

	return msgid;
}

int delete_msg_queue(int msgid)
{
	int r;
	if (msgid == -1) {
		fprintf(stderr, "delete_msg_queue(%d)::msgctl: invalid id: %s\n", msgid,
			strerror(-EINVAL));
		return -EINVAL;
	}

	r = msgctl(msgid, IPC_RMID, NULL);
	if (r)
		fprintf(stderr, "delete_msg_queue(%d)::msgctl: %s\n", msgid,
			strerror(errno));

	return r;
}

int send_message(int msgid, int type, const char *message, int blocking)
{
	int r, flag;
	message_buf buf;
	size_t buf_len;

	buf.type = type;
	strncpy(buf.text, message, MAX_MSG_SIZE);
	buf_len = strlen(buf.text) + 1;

	if (blocking)
		flag = 0;
	else
		flag = IPC_NOWAIT;

	print_msg("%d:%s\n", msgid, message);

	r = msgsnd(msgid, &buf, buf_len, flag);
	if (r)
		fprintf(stderr, "send_message(%d)::msgsnd: %s\n", msgid,
			strerror(errno));

	return r;
}

int receive_message(int msgid, int type, int blocking)
{
	int r, flag;
	message_buf buf;

	if (blocking)
		flag = 0;
	else
		flag = IPC_NOWAIT;

	r = msgrcv(msgid, &buf, MAX_MSG_SIZE, type, flag);
	if (r <= 0) {
		fprintf(stderr, "receive_message(%d)::msgrcv: %s\n", msgid,
			strerror(errno));
		goto error;
	}

	print_msg("%s\n", buf.text);

error:
	return r;
}

void print_usage()
{
	printf("Usage: ipcmsg-tool OPERATIONS [OPTIONS] path\n"
	       "* Operations\n"
	       " -h                     : show this help\n"
	       " -c                     : create a message queue\n"
	       " -d                     : delete the message queue\n"
	       " -r                     : receive a message\n"
	       " -s message             : send a message\n"
	       "* Options (only for send and receive operations)\n"
	       " -b                     : make send or receive operation\n"
	       "                          blocking\n"
	       " -t type                : specify message type\n"
	       "* Various options\n"
	       " -i                     : use message queue identifier instead of path\n"
	       " -q                     : be quiet\n"
		);
}

void parse_args(int argc, char *argv[])
{
	int c;

	while (1) {

		c = getopt(argc, argv, "hcdrs:bt:iq");
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_usage();
			exit(EXIT_SUCCESS);
			break;
		case 'c':
			create = 1;
			break;
		case 'd':
			create = -1;
			break;
		case 'r':
			todo = RECEIVE;
			break;
		case 's':
			strncpy(buffer, optarg, MAX_MSG_SIZE);
			todo = SEND;
			break;
		case 'b':
			blocking = 1;
			break;
		case 't':
			type = atoi(optarg);
			break;
		case 'i':
			use_id = 1;
			break;
		case 'q':
			quiet = 1;
			break;
		default:
			exit(EXIT_FAILURE);
			break;
		}
	}
}


int main(int argc, char* argv[])
{
	int r = 0, msgid;

	parse_args(argc, argv);

	if (argc - optind != 1) {
		print_usage();
		exit(EXIT_FAILURE);
	}

	if (use_id && create == 1) {
		fprintf(stderr, "** incompatible options used: -c and -i\n");
		exit(EXIT_FAILURE);
	}

	if (create == 1) {
		msgid = create_msg_queue(argv[optind]);
		if (msgid == -1)
			r = msgid;
	} else {
		if (use_id) {
			msgid = atoi(argv[optind]);
		} else {
			msgid = get_msg_queue(argv[optind]);
		}

		if (create == -1) { /* user wants to remove the MSGQ object */
			int r;
			r = delete_msg_queue(msgid);
			if (r)
				exit(EXIT_FAILURE);

			exit(EXIT_SUCCESS);
		}
	}

	if (msgid == -1)
		exit(EXIT_FAILURE);

	if (todo == SEND)
		r = send_message(msgid, type, buffer, blocking);
	else if (todo == RECEIVE)
		r = receive_message(msgid, type, blocking);
	else if (create == 0) {
		print_usage();
		r = -1;
	}

	if (r)
		exit(EXIT_FAILURE);

	exit(EXIT_SUCCESS);
}
