#ifndef __KRGNODEMASK_U__
#define __KRGNODEMASK_U__

#include <string.h>

/* be carefull that krgnode_set is not atomic in the userspace version */

#define KERRIGHED_HARD_MAX_NODES 256
#define LONGS_PER_KRGNODEMASK (KERRIGHED_HARD_MAX_NODES/(sizeof(unsigned long)*8))
#define BYTES_PER_KRGNODEMASK (LONGS_PER_KRGNODEMASK*sizeof(unsigned long))

struct krgnodemask {
	unsigned long bits[LONGS_PER_KRGNODEMASK];
};

/*
  Warning: krgnodemask_t from user space is different from krgnodemask_t from
  kernel space. In user space, the size of the vector is related to
  KERRIGHED_HARD_MAX_NODES (and not to KERRIGHED_MAX_NODES)
*/
typedef struct krgnodemask krgnodemask_t;

static inline void __set_bit(int nr, volatile unsigned long * addr)
{
	__asm__ ( 
		"btsl %1,%0"
		:"+m" ((*(volatile long *) addr))
		:"Ir" (nr));
}

#define krgnode_set(node, dst) __krgnode_set((node), &(dst))
static inline void __krgnode_set(int node, volatile krgnodemask_t *dstp)
{
	__set_bit(node, dstp->bits);
}


#define krgnodes_clear(dst) __krgnodes_clear(&(dst))
static inline void __krgnodes_clear(krgnodemask_t *dstp)
{
	memset(dstp->bits, 0, BYTES_PER_KRGNODEMASK);
}

#endif /* __KRGNODEMASK_U__ */
