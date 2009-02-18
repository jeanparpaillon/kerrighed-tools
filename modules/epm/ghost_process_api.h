/** Elrond mobililty interface
 *  @file process_mobility.h
 *
 *  Definition of process mobility function interface.
 *  @author Geoffroy Vallée
 */

#ifndef __GHOST_PROCESS_API_H__
#define __GHOST_PROCESS_API_H__

#ifdef CONFIG_KRG_EPM

#include <ghost/ghost_types.h>

struct task_struct;
struct pt_regs;
struct epm_action;

/** Export a process into a ghost.
 *  @author  Geoffroy Vallée
 *
 *  @param task    Task to export.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int export_process(struct epm_action *action,
		   ghost_t *ghost,
		   struct task_struct *task,
		   struct pt_regs *regs);

/** Import a process from a ghost.
 *  @author  Geoffroy Vallée
 *
 *  @return  Pointer to the imported task struct.
 */
struct task_struct *import_process(struct epm_action *action,
				   ghost_t *ghost);

#endif /* CONFIG_KRG_EPM */

#endif /* __GHOST_PROCESS_API_H__ */
