#ifndef __KRG_CPU_PROBE_H__
#define __KRG_CPU_PROBE_H__

//#include <linux/spinlock.h>

#define CPU_PROBE_NAME "cpu_probe"
//#define CPU_PROBE_ID 0x10000001

typedef struct cpu_probe_data {
  //spinlock_t lock;

  clock_t cpu_used;
  clock_t cpu_total;

} cpu_probe_data_t;

#endif /* __KRG_CPU_PROBE_H__ */
