#ifndef __KRG_I386__
#define __KRG_I386__

#include <comm/warehouse.h>

void *alloc_cpuinfo(int nr);

void *alloc_arch_nodeinfo(void);
int arch_nodeinfo_init(void *_arg);
int arch_nodeinfo_pack(struct gimli_desc *desc);
int arch_nodeinfo_unpack(struct gimli_desc *desc, void *item);

int aragorn_do_fork(unsigned long clone_flags,
		    unsigned long stack_start,
		    struct pt_regs *regs, unsigned long stack_size);

pid_t process_duplication(struct task_struct *tsk,
			  struct pt_regs *regs,
			  unsigned short l_fs, unsigned short l_gs);

#endif
