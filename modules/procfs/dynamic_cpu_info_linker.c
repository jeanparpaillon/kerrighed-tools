/** Dynamic CPU information management.
 *  @file dynamic_cpu_info_linker.c
 *  
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */

#define MODULE_NAME "CPU Dyn Info"
#include "debug_procfs.h"

#ifndef FILE_NONE
#  if defined(FILE_DYNAMIC_CPU_INFO) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

#include <tools/debug.h>

#include <linux/swap.h>
#include <linux/kernel_stat.h>

#include <tools/workqueue.h>
#include <ctnr/kddm.h>

#include "dynamic_cpu_info_linker.h"

struct kddm_set *dynamic_cpu_info_ctnr = NULL;



/*****************************************************************************/
/*                                                                           */
/*                   DYNAMIC CPU INFO CONTAINER IO FUNCTIONS                 */
/*                                                                           */
/*****************************************************************************/



/****************************************************************************/

/* Init the dynamic cpu info IO linker */

struct iolinker_struct dynamic_cpu_info_io_linker = {
      linker_name:"dyn_cpu_nfo",
      linker_id:DYNAMIC_CPU_INFO_LINKER,
};

static void update_dynamic_cpu_info_worker(struct work_struct *data);
static DECLARE_DELAYED_WORK(update_dynamic_cpu_info_work, update_dynamic_cpu_info_worker);

/** Update dynamic CPU informations for all local CPU.
 *  @author Renaud Lottiaux
 */
static void update_dynamic_cpu_info_worker(struct work_struct *data)
{
	krg_dynamic_cpu_info_t *dynamic_cpu_info;
	int i, j, cpu_id;

	for_each_online_cpu(i) {
		cpu_id = kerrighed_node_id * NR_CPUS + i;
		dynamic_cpu_info =
			_kddm_grab_object(dynamic_cpu_info_ctnr, cpu_id);

		/* Compute data for stat proc file */

		dynamic_cpu_info->user = kstat_cpu(i).cpustat.user;
		dynamic_cpu_info->nice = kstat_cpu(i).cpustat.nice;
		dynamic_cpu_info->system = kstat_cpu(i).cpustat.system;
		dynamic_cpu_info->idle = kstat_cpu(i).cpustat.idle;
		dynamic_cpu_info->iowait = kstat_cpu(i).cpustat.iowait;
		dynamic_cpu_info->irq = kstat_cpu(i).cpustat.irq;
		dynamic_cpu_info->softirq = kstat_cpu(i).cpustat.softirq;
		dynamic_cpu_info->steal = kstat_cpu(i).cpustat.steal;
		dynamic_cpu_info->total_intr = 0;

		for (j = 0; j < NR_IRQS; j++)
			dynamic_cpu_info->total_intr += kstat_cpu(i).irqs[j];

		_kddm_put_object(dynamic_cpu_info_ctnr, cpu_id);
	}

	queue_delayed_work(krg_wq, &update_dynamic_cpu_info_work, HZ);
}

int init_dynamic_cpu_info_ctnr(void)
{
	register_io_linker(DYNAMIC_CPU_INFO_LINKER,
			   &dynamic_cpu_info_io_linker);

	/* Create the CPU info container */

	dynamic_cpu_info_ctnr = create_new_kddm_set(kddm_def_ns,
						    DYNAMIC_CPU_INFO_KDDM_ID,
						    DYNAMIC_CPU_INFO_LINKER,
						    KDDM_RR_DEF_OWNER,
						    sizeof
						    (krg_dynamic_cpu_info_t),
						    0);
	if (IS_ERR(dynamic_cpu_info_ctnr))
		OOM;

	queue_delayed_work(krg_wq, &update_dynamic_cpu_info_work, 0);
	return 0;
}
