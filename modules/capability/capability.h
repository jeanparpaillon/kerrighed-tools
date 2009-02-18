#ifndef __KRG_CAPABILITY_H__
#define __KRG_CAPABILITY_H__

#ifdef CONFIG_KRG_CAP

#include <kerrighed/capabilities.h>

struct task_struct;
struct caller_creds;

int krg_set_cap(struct task_struct *tsk,
		struct caller_creds *requester_creds,
		krg_cap_t *requested_cap);

#endif /* CONFIG_KRG_CAP */

#endif /* __KRG_CAPABILITY_H__ */
