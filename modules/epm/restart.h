/** Process restart interface.
 *  @file restart.h
 *
 *  Definition of process restart interface.
 *  @author Geoffroy Vallée, Matthieu Fertré
 */

#ifndef __RESTART_H__
#define __RESTART_H__

#ifdef CONFIG_KRG_EPM

#include <linux/types.h>
#include <kerrighed/sys/checkpoint.h>

struct credentials;

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

struct task_struct * restart_process(media_t media, pid_t pid,
				     long app_id, int chkpt_sn,
				     const struct credentials * user_creds);

#endif /* CONFIG_KRG_EPM */

#endif /* __RESTART_H__ */
