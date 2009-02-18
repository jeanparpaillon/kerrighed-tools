#ifndef __KRG_SYSCALLS_H__
#define __KRG_SYSCALLS_H__

#include <linux/sched.h>

struct caller_creds {
	uid_t caller_uid;
	uid_t caller_euid;
};

static inline int permissions_ok(struct task_struct *task_to_act_on,
				 struct caller_creds *requester_creds)
{
	return ((requester_creds->caller_uid == task_to_act_on->uid) ||
		(requester_creds->caller_euid == task_to_act_on->euid) ||
		(requester_creds->caller_uid == 0) ||
		(requester_creds->caller_euid == 0));
}

#endif /* __KRG_SYSCALLS_H__ */
