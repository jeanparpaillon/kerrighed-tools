#ifndef __DEBUG_GHOST_H__
#define __DEBUG_GHOST_H__

#include <tools/debug.h>
#include <tools/debug_tools2.h>

#define DEBUG_FILE_GHOST	"file_ghost"
#define DEBUG_GHOST_API		"ghost_api"
#define DEBUG_NETWORK_GHOST	"network_ghost"

static inline struct dentry * init_ghost_debug(void)
{
#ifndef CONFIG_KRG_DEBUG
	return NULL;
#else
	struct dentry *d = debug_define("ghost", 0);

	DEBUG_MASK("ghost", DEBUG_FILE_GHOST);
	DEBUG_MASK("ghost", DEBUG_GHOST_API);
	DEBUG_MASK("ghost", DEBUG_NETWORK_GHOST);

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
	if (match_debug("ghost", mask, level)) {			\
		printk(KERN_DEBUG DEBUG_NORMAL				\
		       MODULE_NAME " (%s) %d: "	format,			\
		       __PRETTY_FUNCTION__, current->pid, ## args);	\
	}
#endif

#endif /* __DEBUG_GHOST_H__ */
