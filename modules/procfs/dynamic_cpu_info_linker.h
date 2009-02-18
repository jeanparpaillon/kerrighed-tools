/** Dynamic per CPU informations management.
 *  @file dynamic_cpu_info_linker.h
 *
 *  @author Renaud Lottiaux
 */

#ifndef DYNAMIC_CPU_INFO_LINKER_H
#define DYNAMIC_CPU_INFO_LINKER_H

#include "static_cpu_info_linker.h"

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/* Dynamic CPU informations */

typedef struct {
	cputime64_t user;
	cputime64_t nice;
	cputime64_t system;
	cputime64_t idle;
	cputime64_t iowait;
	cputime64_t irq;
	cputime64_t softirq;
	cputime64_t steal;
	u64 total_intr;
} krg_dynamic_cpu_info_t;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                            EXTERN VARIABLES                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/



extern struct iolinker_struct dynamic_cpu_info_io_linker;
extern struct kddm_set *dynamic_cpu_info_ctnr;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/



int init_dynamic_cpu_info_ctnr(void);

/** Helper function to get dynamic CPU info
 *  @author Renaud Lottiaux
 *
 *  @param node_id   Id of the node hosting the CPU we want informations on.
 *  @param cpu_id    Id of the CPU we want informations on.
 *
 *  @return  Structure containing information on the requested CPU.
 */
static inline krg_dynamic_cpu_info_t *get_dynamic_cpu_info(int node_id,
							   int cpu_id)
{
	return _kddm_get_object_no_lock(dynamic_cpu_info_ctnr,
					node_id * NR_CPUS + cpu_id);
}

#endif // DYNAMIC_CPU_INFO LINKER_H
