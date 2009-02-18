/** Dynamic node informations management.
 *  @file dynamic_node_info_linker.h
 *
 *  @author Renaud Lottiaux
 */

#ifndef DYNAMIC_NODE_INFO_LINKER_H
#define DYNAMIC_NODE_INFO_LINKER_H

#include <kerrighed/sys/types.h>
#include <linux/procfs_internal.h>
#include <ctnr/kddm.h>

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/

/* Node related informations */

typedef struct {
	struct timespec idletime;
	struct timespec uptime;
	unsigned long avenrun[3];	/* Load averages */
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
	unsigned long mosix_load;
	unsigned long mosix_single_process_load;
#endif
	int last_pid;
	int nr_threads;
	unsigned long nr_running;
	unsigned long long nr_context_switches;
	unsigned long jif;
	unsigned long total_forks;
	unsigned long nr_iowait;

	/* Dynamic memory informations */

	unsigned long totalram;
	unsigned long freeram;
	unsigned long bufferram;
	unsigned long totalhigh;
	unsigned long freehigh;
	unsigned long totalswap;
	unsigned long freeswap;

	unsigned long nr_file_pages;
	unsigned long nr_file_dirty;
	unsigned long nr_writeback;
	unsigned long nr_anon_pages;
	unsigned long nr_file_mapped;
	unsigned long nr_page_table_pages;
	unsigned long nr_slab_reclaimable;
	unsigned long nr_slab_unreclaimable;
	unsigned long nr_unstable_nfs;
	unsigned long nr_bounce;

	struct vmalloc_info vmi;
	unsigned long vmalloc_total;

	unsigned long active;
	unsigned long inactive;
	unsigned long allowed;
	unsigned long commited;

	unsigned long swapcache_pages;

} krg_dynamic_node_info_t;

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                            EXTERN VARIABLES                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/

extern struct kddm_set *dynamic_node_info_ctnr;

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

int init_dynamic_node_info_ctnr(void);

/** Helper function to get dynamic node informations.
 *  @author Renaud Lottiaux
 *
 *  @param node_id   Id of the node we want informations on.
 *
 *  @return  Structure containing information on the requested node.
 */
static inline
krg_dynamic_node_info_t *get_dynamic_node_info(kerrighed_node_t nodeid)
{
	return _kddm_get_object_no_lock(dynamic_node_info_ctnr, nodeid);
}

#endif // DYNAMIC_NODE_INFO_LINKER_H
