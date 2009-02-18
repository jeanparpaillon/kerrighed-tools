/** Define Kerrighed Capabilities
 * @author David Margery (c) Inria 2004
 */

#ifndef _KERRIGHED_CAPABILITIES_H
#define _KERRIGHED_CAPABILITIES_H

enum {
	CAP_CHANGE_KERRIGHED_CAP = 0,
	CAP_CAN_MIGRATE,
	CAP_DISTANT_FORK,
	CAP_FORK_DELAY,
	CAP_CHECKPOINTABLE,
	CAP_USE_REMOTE_MEMORY,			/* 5 */
	CAP_USE_INTRA_CLUSTER_KERSTREAMS,
	CAP_USE_INTER_CLUSTER_KERSTREAMS,
	CAP_USE_WORLD_VISIBLE_KERSTREAMS,
	CAP_SEE_LOCAL_PROC_STAT,
	CAP_DEBUG,
	CAP_SYSCALL_EXIT_HOOK,
	CAP_SIZE /* keep as last capability */
};

typedef struct krg_cap_struct krg_cap_t;
typedef struct krg_cap_pid_desc krg_cap_pid_t;

#ifndef __KERNEL__

struct krg_cap_struct
{
  int  krg_cap_effective ; 
  int  krg_cap_permitted ;
  int  krg_cap_inheritable_permitted ;
  int  krg_cap_inheritable_effective ; 
} ;

#endif /* __KERNEL__ */

#endif
