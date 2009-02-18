/** Process checkpoint interface.
 *  @file checkpoint.h
 *
 *  Definition of process checkpointing interface.
 *  @author Geoffroy Vallée, Renaud Lottiaux
 */

#ifndef __CHECKPOINT_H__
#define __CHECKPOINT_H__

#ifdef CONFIG_KRG_EPM

#include <linux/types.h>

struct task_struct;
struct credentials;

typedef enum {
	CHKPT_NO_OPTION,
	CHKPT_ONLY_STOP
} chkpt_option_t;

#define si_media(info)	(*(media_t *) &(info)._sifields._pad)
#define si_option(info)  (*(chkpt_option_t *) &(info)._sifields._pad[1])

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

int can_be_checkpointed(struct task_struct *task_to_checkpoint,
			const struct credentials *user_creds);

#endif /* CONFIG_KRG_EPM */

#endif /* __CHECKPOINT_H__ */
