#ifndef __DEBUG_KERMM_H__

#define __DEBUG_KERMM_H__

#include <tools/debug.h>
#include <tools/debug_tools2.h>

#ifndef CONFIG_KRG_DEBUG
#	define INIT_MM_DEBUG() do {} while (0)
#else
#       define INIT_MM_DEBUG()   			\
	do {						\
		debug_define("mm", 0);	        	\
		DEBUG_MASK("mm", "io_linker");		\
		DEBUG_MASK("mm", "int_linker");		\
		DEBUG_MASK("mm", "mm_struct");          \
		DEBUG_MASK("mm", "mobility");		\
	} while (0)
#endif

#ifdef DEBUG
#undef DEBUG
#endif

#ifndef CONFIG_KRG_DEBUG
#	define DEBUG(mask, level, format, args...) do {} while(0)
#else
#	define DEBUG(mask, level, format, args...)			\
        do {                                                            \
	if (match_debug("mm", mask, level)) {			        \
		printk(DEBUG_NORMAL		  	                \
		       "%6d - %30.30s - "				\
		       format,						\
		       current->pid, __PRETTY_FUNCTION__,		\
		       ## args) ;					\
	}} while (0)
#endif

#endif // __DEBUG_KERMM_H__
