/* Management of incompatibilities between Kerrighed actions and
 * some Linux facilities
 */

#ifndef __EPM_ACTION_H__
#define __EPM_ACTION_H__

#ifdef CONFIG_KRG_EPM

#include <linux/types.h>
#include <kerrighed/sys/types.h>
#include <kerrighed/sys/checkpoint.h>

typedef enum {
	EPM_NO_ACTION,
	EPM_MIGRATE,
	EPM_REMOTE_CLONE,
	EPM_CHECKPOINT,
	EPM_ACTION_MAX	   /* Always in last position */
} krg_epm_action_t;

typedef enum {
	CR_SAVE_NOW,
	CR_SAVE_LATER,
	CR_FILES_ADD,
	CR_FILES_WRITEID
} c_shared_obj_option_t;

typedef enum {
	CR_LOAD_NOW,
	CR_LINK_ONLY
} r_shared_obj_option_t;

struct task_struct;
struct completion;

struct epm_action {
	krg_epm_action_t type;
	union {
		struct {
			pid_t pid;
			kerrighed_node_t target;
		} migrate;
		struct {
			pid_t from_pid;
			pid_t from_tgid;
			kerrighed_node_t target;
			unsigned long clone_flags;
			unsigned long stack_start;
			unsigned long stack_size;
			int *parent_tidptr;
			int *child_tidptr;
			struct completion *vfork;
		} remote_clone;
		struct {
			media_t media;
			c_shared_obj_option_t shared;
		} checkpoint;
		struct {
			media_t media;
			r_shared_obj_option_t shared;
			struct app_struct * app;
		} restart;
	};
};

static inline kerrighed_node_t epm_target_node(struct epm_action *action)
{
	switch (action->type) {
	case EPM_MIGRATE:
		return action->migrate.target;
	case EPM_REMOTE_CLONE:
		return action->remote_clone.target;
	case EPM_CHECKPOINT:
		return KERRIGHED_NODE_ID_NONE;
	default:
		BUG();
	}
}

int krg_action_disable(struct task_struct *task, krg_epm_action_t action,
		       int inheritable);
int krg_action_enable(struct task_struct *task, krg_epm_action_t action,
		      int inheritable);

int krg_action_start(struct task_struct *task, krg_epm_action_t action);
int krg_action_stop(struct task_struct *task, krg_epm_action_t action);

int krg_action_pending(struct task_struct *task, krg_epm_action_t action);

#endif /* CONFIG_KRG_EPM */

#endif /* __EPM_ACTION_H__ */
