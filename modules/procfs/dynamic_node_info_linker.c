/** Dynamic node information management.
 *  @file dynamic_node_info_linker.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */

#define MODULE_NAME "Node Dyn Info"
#include "debug_procfs.h"

#ifndef FILE_NONE
#  if defined(FILE_DYNAMIC_NODE_INFO) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

#include <tools/debug.h>

#include <linux/swap.h>
#include <linux/kernel_stat.h>
#include <linux/pagemap.h>
#include <linux/hugetlb.h>
#include <linux/mman.h>
#include <linux/vmstat.h>
#include <linux/sched.h>
#include <linux/nsproxy.h>
#include <linux/pid_namespace.h>
#include <linux/workqueue.h>

#include <tools/workqueue.h>
#include <ctnr/kddm.h>
#include <scheduler_old/mosix_probe.h>

#include "dynamic_node_info_linker.h"

/* Container of node information locations */
struct kddm_set *dynamic_node_info_ctnr = NULL;

/*****************************************************************************/
/*                                                                           */
/*                    DYNAMIC NODE INFO CONTAINER IO FUNCTIONS               */
/*                                                                           */
/*****************************************************************************/

/****************************************************************************/

/* Init the dynamic node info IO linker */

static struct iolinker_struct dynamic_node_info_io_linker = {
      linker_name:"dyn_node_nfo",
      linker_id:DYNAMIC_NODE_INFO_LINKER,
};

static void update_dynamic_node_info_worker(struct work_struct *work);
static DECLARE_DELAYED_WORK(update_dynamic_node_info_work,
			    update_dynamic_node_info_worker);

/** Update the dynamic informations for the local node.
 *  @author Renaud Lottiaux
 */
static void update_dynamic_node_info_worker(struct work_struct *work)
{
	krg_dynamic_node_info_t *dynamic_node_info;
	cputime_t idletime = cputime_add(init_task.utime, init_task.stime);
	struct sysinfo sysinfo;
	unsigned long jif, dummy;

	dynamic_node_info = _kddm_grab_object(dynamic_node_info_ctnr,
					      kerrighed_node_id);

	/* Compute data for uptime proc file */

	cputime_to_timespec(idletime, &dynamic_node_info->idletime);
	do_posix_clock_monotonic_gettime(&dynamic_node_info->uptime);

	/* Compute data for loadavg proc file */

	dynamic_node_info->avenrun[0] = avenrun[0];
	dynamic_node_info->avenrun[1] = avenrun[1];
	dynamic_node_info->avenrun[2] = avenrun[2];
#ifdef CONFIG_KRG_SCHED_MOSIX_PROBE
	dynamic_node_info->mosix_load = mosix_norm_upper_load;
	dynamic_node_info->mosix_single_process_load = mosix_norm_single_process_load;
#endif
	dynamic_node_info->last_pid = current->nsproxy->pid_ns->last_pid;
	dynamic_node_info->nr_threads = nr_threads;
	dynamic_node_info->nr_running = nr_running();

	jif = -wall_to_monotonic.tv_sec;
	if (wall_to_monotonic.tv_nsec)
		--jif;

	dynamic_node_info->jif = (unsigned long)jif;
	dynamic_node_info->total_forks = total_forks;
	dynamic_node_info->nr_iowait = nr_iowait();
	dynamic_node_info->nr_context_switches = nr_context_switches();

	/* Compute data for meminfo proc file */

	si_meminfo(&sysinfo);
	si_swapinfo(&sysinfo);

	dynamic_node_info->totalram = sysinfo.totalram;
	dynamic_node_info->freeram = sysinfo.freeram;
	dynamic_node_info->bufferram = sysinfo.bufferram;
	dynamic_node_info->totalhigh = sysinfo.totalhigh;
	dynamic_node_info->freehigh = sysinfo.freehigh;
	dynamic_node_info->totalswap = sysinfo.totalswap;
	dynamic_node_info->freeswap = sysinfo.freeswap;
	dynamic_node_info->totalram = sysinfo.totalram;
	dynamic_node_info->swapcache_pages = total_swapcache_pages;

	dynamic_node_info->nr_file_pages = global_page_state(NR_FILE_PAGES);
	dynamic_node_info->nr_file_dirty = global_page_state(NR_FILE_DIRTY);
	dynamic_node_info->nr_writeback = global_page_state(NR_WRITEBACK);
	dynamic_node_info->nr_anon_pages = global_page_state(NR_ANON_PAGES);
	dynamic_node_info->nr_file_mapped = global_page_state(NR_FILE_MAPPED);
	dynamic_node_info->nr_bounce = global_page_state(NR_BOUNCE);
	dynamic_node_info->nr_page_table_pages =
		global_page_state(NR_PAGETABLE);
	dynamic_node_info->nr_slab_reclaimable =
		global_page_state(NR_SLAB_RECLAIMABLE);
	dynamic_node_info->nr_slab_unreclaimable =
		global_page_state(NR_SLAB_UNRECLAIMABLE);
	dynamic_node_info->nr_unstable_nfs =
		global_page_state(NR_UNSTABLE_NFS);

	get_zone_counts(&dynamic_node_info->active,
			&dynamic_node_info->inactive, &dummy);
	dynamic_node_info->allowed = ((totalram_pages - hugetlb_total_pages())
				      * sysctl_overcommit_ratio / 100) +
		                     total_swap_pages;

	dynamic_node_info->commited = atomic_read(&vm_committed_space);

	get_vmalloc_info(&dynamic_node_info->vmi);
	dynamic_node_info->vmalloc_total = VMALLOC_TOTAL;

	_kddm_put_object(dynamic_node_info_ctnr, kerrighed_node_id);

	queue_delayed_work(krg_wq, &update_dynamic_node_info_work, HZ);
}

int init_dynamic_node_info_ctnr(void)
{
	register_io_linker(DYNAMIC_NODE_INFO_LINKER,
			   &dynamic_node_info_io_linker);

	/* Create the node info container */

	dynamic_node_info_ctnr =
		create_new_kddm_set(kddm_def_ns, DYNAMIC_NODE_INFO_KDDM_ID,
				    DYNAMIC_NODE_INFO_LINKER,
				    KDDM_RR_DEF_OWNER,
				    sizeof(krg_dynamic_node_info_t), 0);
	if (IS_ERR(dynamic_node_info_ctnr))
		OOM;

	/* Start periodic updates */
	queue_delayed_work(krg_wq, &update_dynamic_node_info_work, 0);

	return 0;
}
