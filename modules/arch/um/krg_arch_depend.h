#ifndef __KRG_ARCH_DEPEND__
#define __KRG_ARCH_DEPEND__

#include <asm/processor-generic.h>

#define SIZEOF_CPUINFO (sizeof(struct cpuinfo_um))
#define SIZEOF_ARCH_DEPEND (sizeof(struct arch_nodeinfo))

struct arch_nodeinfo {
};

#endif
