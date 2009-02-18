#ifndef __DEBUG_PROC_H__
#define __DEBUG_PROC_H__

#include <tools/debug.h>
#include <tools/debug_tools2.h>

#define DEBUG_GROUP	"proc"

#define DBG_MODULE	"module"
#define DBG_RSYSCALL	"rsyscall"
#define DBG_KRG_FORK	"krg_fork"
#define DBG_SIGHAND	"sighand"
#define DBG_SIGNAL	"signal"
#define DBG_TASK_KDDM	"task_kddm"
#define DBG_PID		"pid"
#define DBG_EXIT	"exit"
#define DBG_CHILDREN	"children"

static inline struct dentry * init_proc_debug(void)
{
#ifndef CONFIG_KRG_DEBUG
	return NULL;
#else
	struct dentry *d = debug_define(DEBUG_GROUP, 0);

	DEBUG_MASK(DEBUG_GROUP, DBG_MODULE);
	DEBUG_MASK(DEBUG_GROUP, DBG_RSYSCALL);
	DEBUG_MASK(DEBUG_GROUP, DBG_KRG_FORK);
	DEBUG_MASK(DEBUG_GROUP, DBG_SIGHAND);
	DEBUG_MASK(DEBUG_GROUP, DBG_SIGNAL);
	DEBUG_MASK(DEBUG_GROUP, DBG_TASK_KDDM);
	DEBUG_MASK(DEBUG_GROUP, DBG_PID);
	DEBUG_MASK(DEBUG_GROUP, DBG_EXIT);
	DEBUG_MASK(DEBUG_GROUP, DBG_CHILDREN);

	return d;
#endif
}

#ifdef DEBUG
#undef DEBUG
#endif

#ifndef CONFIG_KRG_DEBUG
#	define DEBUG(mask, level, format, args...) do {} while(0)
#else
#	define DEBUG(mask, level, format, args...)			\
	if (match_debug(DEBUG_GROUP, mask, level)) {				\
		printk(KERN_DEBUG DEBUG_NORMAL DEBUG_COLOR(GREEN)	\
		       MODULE_NAME " (%s) %d: " format,			\
		       __PRETTY_FUNCTION__, current->pid, ## args);	\
	}
#endif

#endif /* __DEBUG_PROC_H__ */
