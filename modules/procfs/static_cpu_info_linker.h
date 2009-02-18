/** Static CPU information management.
 *  @file static_cpu_info_linker.h
 *
 *  @author Renaud Lottiaux
 */

#ifndef STATIC_CPU_INFO_LINKER_H
#define STATIC_CPU_INFO_LINKER_H

#include <kerrighed/cpu_id.h>
#include <asm/cpuinfo.h>

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/

/* Static CPU informations */

typedef struct {
	cpuinfo_t info;
} krg_static_cpu_info_t;

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                            EXTERN VARIABLES                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/

extern struct iolinker_struct static_cpu_info_io_linker;
extern struct kddm_set *static_cpu_info_ctnr;

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

int init_static_cpu_info_ctnr(void);

/** Helper function to get static CPU informations.
 *  @author Renaud Lottiaux
 *
 *  @param node_id   Id of the node hosting the CPU we want informations on.
 *  @param cpu_id    Id of the CPU we want informations on.
 *
 *  @return  Structure containing information on the requested CPU.
 */
static inline krg_static_cpu_info_t *get_static_cpu_info(int node_id,
							 int cpu_id)
{
	return _kddm_get_object_no_lock(static_cpu_info_ctnr,
					__krg_cpu_id(node_id, cpu_id));
}

#endif // STATIC_CPU_INFO LINKER_H
