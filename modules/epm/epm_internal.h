#ifndef __EPM_INTERNAL_H__
#define __EPM_INTERNAL_H__

#ifdef CONFIG_KRG_EPM

#include <asm/signal.h>

#define KRG_SIG_MIGRATE		SIGRTMIN
#define KRG_SIG_CHECKPOINT	(SIGRTMIN + 1)
#ifdef CONFIG_KRG_FD
#define KRG_SIG_FORK_DELAY_STOP	(SIGRTMIN + 2)
#endif

int epm_procfs_start(void);
void epm_procfs_exit(void);

void register_krg_ptrace_hooks(void);

void register_migration_hooks(void);
int epm_migration_start(void);
void epm_migration_exit(void);

void register_checkpoint_hooks(void);

#ifdef CONFIG_KRG_FD
void register_fork_delay_hooks(void);
int epm_fork_delay_start(void);
void epm_fork_delay_exit(void);
#endif

#endif /* CONFIG_KRG_EPM */

#endif /* __EPM_INTERNAL_H__ */
