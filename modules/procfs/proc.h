#ifndef KRG_PROCFS_H

#define KRG_PROCFS_H

#ifdef __KERNEL__

#include <linux/proc_fs.h>
#include <kerrighed/sys/types.h>

#endif				// __KERNEL__

#define PROC_STAT_DEPEND_ON_CAPABILITY (KERRIGHED_MAX_NODES + 1)

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

#ifdef __KERNEL__

int krg_procfs_init(void);
int krg_procfs_finalize(void);

int create_proc_node_info(kerrighed_node_t node);
int remove_proc_node_info(kerrighed_node_t node);

#endif /* __KERNEL__ */

#endif /* KRG_PROCFS_H */
