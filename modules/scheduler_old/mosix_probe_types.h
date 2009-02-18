#ifndef __MOSIX_PROBE_TYPES_H__
#define __MOSIX_PROBE_TYPES_H__

#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE

#if 0
#include <linux/list.h>
#include <kerrighed/sys/types.h>
#endif
#include <asm/param.h>

/* Compute processor load every second */
#define CF HZ

/* Speed of the standard processor */
//#define STD_SPD 10000
// Not used. Set this to 1 avoid long overflow in load compuation.
#define STD_SPD 1

//#define PROCESS_MEAN_LOAD_SCALE 4
#define PROCESS_MEAN_LOAD_SCALE 1

/* Values taken from MOSIX */
#define MEAN_LOAD_SCALE 128

#define MEAN_LOAD_DECAY 5
#define MEAN_LOAD_NEW_DATA 3

#define UPPER_LOAD_DECAY 7
#define UPPER_LOAD_NEW_DATA 1

#define CPU_USE_DECAY 3
#define CPU_USE_NEW_DATA 1

#define ALARM_THRESHOLD (CF + CF/2)

struct mosix_probe_info {
	unsigned int load;    /**< Estimated load:
			       * approx. 4 * ticks used in CF ticks */
	unsigned int last_on; /**< load_ticks + 1 when last put on runqueue,
			       * 0 if not on runqueue */
	unsigned int ran;     /**< number of ticks used
			       * since last call to mp_calc_load */
};

/** Informations kept on other nodes
 */

#if 0
typedef struct ping_info {
	kerrighed_node_t node;	 /**< Id of the node */
	unsigned long load;	 /**< Processor load of the node */
	unsigned long cpu_speed; /**< Speed of each CPU of the node */
	unsigned short num_cpus; /**< Number of CPUs on the node */
	unsigned long cpu_use;   /**< amount of CPU efficiently used */
} ping_info_t;

typedef ping_info_t ping_info_msg_t;
#endif

#endif /* CONFIG_KRG_SCHED_MOSIX_PROBE */

#endif /* __MOSIX_PROBE_TYPES_H__ */
