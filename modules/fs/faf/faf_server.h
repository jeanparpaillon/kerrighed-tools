/** Kerrighed FAF Server.
 *  @file faf_server.h
 *  
 *  @author Renaud Lottiaux 
 */

#ifndef __FAF_SERVER__
#define __FAF_SERVER__

/* Must match with the same macro defined in kernel/net/socket.c */
#define MAX_SOCK_ADDR 128


/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



typedef struct faf_rw_msg {
	int server_fd;
	size_t count;
} faf_rw_msg_t;

typedef struct faf_notify_msg {
	int server_fd;
	unsigned long objid;
} faf_notify_msg_t;

typedef struct faf_stat_msg {
	int server_fd;
	long flags;
} faf_stat_msg_t;

typedef struct faf_ctl_msg {
	int server_fd;
	unsigned int cmd;
	union {
		unsigned long arg;
		struct flock flock;
#if BITS_PER_LONG == 32
		struct flock64 flock64;
#endif
	};
} faf_ctl_msg_t;

typedef struct faf_seek_msg {
	int server_fd;
	off_t offset;
	unsigned int origin;
} faf_seek_msg_t;

typedef struct faf_llseek_msg {
	int server_fd;
	unsigned long offset_high;
	unsigned long offset_low;
	unsigned int origin;
} faf_llseek_msg_t;

typedef struct faf_bind_msg {
	int server_fd;
	int addrlen;
	char sa[MAX_SOCK_ADDR];
} faf_bind_msg_t;

typedef struct faf_listen_msg {
	int server_fd;
	int sub_chan;
	int backlog;
} faf_listen_msg_t;

typedef struct faf_send_msg {
	int server_fd;
	size_t len;
	unsigned flags;
} faf_send_msg_t;

typedef struct faf_sendto_msg {
	int server_fd;
	size_t len;
	unsigned flags;
	int addrlen;
	char sa[MAX_SOCK_ADDR];
} faf_sendto_msg_t;

typedef struct faf_shutdown_msg {
	int server_fd;
	int how;
} faf_shutdown_msg_t;

typedef struct faf_setsockopt_msg {
	int server_fd;
	int level;
	int optname;
	char __user *optval;
	int optlen;
} faf_setsockopt_msg_t;

typedef struct faf_getsockopt_msg {
	int server_fd;
	int level;
	int optname;
	char __user *optval;
	int __user *optlen;
} faf_getsockopt_msg_t;

typedef struct faf_sendmsg_msg {
	int server_fd;
	struct msghdr msghdr;
	unsigned int flags;
} faf_sendmsg_msg_t;

struct faf_poll_wait_msg {
	int server_fd;
	unsigned long objid;
	int wait;
};



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/



void faf_server_init (void);
void faf_server_finalize (void);


#endif // __FAF_SERVER__
