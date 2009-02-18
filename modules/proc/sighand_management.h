#ifndef __SIGHAND_MANAGEMENT_H__
#define __SIGHAND_MANAGEMENT_H__

#ifdef CONFIG_KRG_EPM

#include <ctnr/kddm_types.h>

struct sighand_struct;
struct task_struct;

struct sighand_struct *kcb_sighand_struct_writelock(objid_t id);
struct sighand_struct *kcb_sighand_struct_readlock(objid_t id);
void kcb_sighand_struct_unlock(objid_t id);

void kcb_sighand_struct_pin(struct sighand_struct *sig);
void kcb_sighand_struct_unpin(struct sighand_struct *sig);

struct sighand_struct *kcb_malloc_sighand_struct(struct task_struct *task,
						 int need_update);
struct sighand_struct *cr_malloc_sighand_struct(void);
void cr_free_sighand_struct(objid_t id);

#endif /* CONFIG_KRG_EPM */

#endif /* __SIGHAND_MANAGEMENT_H__ */
