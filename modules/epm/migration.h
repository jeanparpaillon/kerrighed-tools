/** Migration interface API.
 *  @file migration.h
 *
 *  Implementation of migration functions.
 *
 *  @author Geoffroy Vallee
 */

#ifndef __MIGRATION_H__
#define __MIGRATION_H__

#ifdef CONFIG_KRG_EPM

#include <linux/types.h>
#include <kerrighed/sys/types.h>
#ifdef CONFIG_KRG_SCHED
#include <kerrighed/module_hook.h>
#endif

struct task_struct;
struct caller_creds;
struct pt_regs;
struct epm_action;

#ifdef CONFIG_KRG_SCHED
extern struct module_hook_desc kmh_migration_start;
extern struct module_hook_desc kmh_migration_aborted;
extern struct module_hook_desc kmh_migration_end;
#endif

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

int __may_migrate(struct task_struct *task, struct caller_creds *creds);
int may_migrate(struct task_struct *task, struct caller_creds *creds);

pid_t send_task(struct task_struct *tsk,
		struct pt_regs *task_regs,
		kerrighed_node_t remote_node,
		struct epm_action *action);

int do_migrate_process(struct task_struct *tsk,
		       kerrighed_node_t destination_node_id);

void migration_aborted(struct task_struct *tsk);

#endif /* CONFIG_KRG_EPM */

#endif /* __MIGRATION_H__ */
