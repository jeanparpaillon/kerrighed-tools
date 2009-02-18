/** Static CPU information management.
 *  @file static_cpu_info_linker.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */

#include <kerrighed/cpu_id.h>

#define MODULE_NAME "Static CPU Info"
#include "debug_procfs.h"

#ifndef FILE_NONE
#  if defined(FILE_STATIC_CPU_INFO) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

#include <tools/debug.h>

#include <linux/swap.h>

#include <ctnr/kddm.h>

#include "static_cpu_info_linker.h"

struct kddm_set *static_cpu_info_ctnr = NULL;

/*****************************************************************************/
/*                                                                           */
/*                    STATIC CPU INFO CONTAINER IO FUNCTIONS                 */
/*                                                                           */
/*****************************************************************************/

static kerrighed_node_t cpu_info_default_owner(struct kddm_set *set,
					       objid_t objid,
					       const krgnodemask_t *nodes,
					       int nr_nodes)
{
	return krg_cpu_node(objid);
}

/****************************************************************************/

/* Init the cpu info IO linker */

struct iolinker_struct static_cpu_info_io_linker = {
	.linker_name = "stat_cpu_info",
	.linker_id = STATIC_CPU_INFO_LINKER,
	.default_owner = cpu_info_default_owner
};

int init_static_cpu_info_ctnr(void)
{
	krg_static_cpu_info_t *static_cpu_info;
	int cpu_id, i;

	register_io_linker(STATIC_CPU_INFO_LINKER, &static_cpu_info_io_linker);

	/* Create the CPU info container */

	static_cpu_info_ctnr = create_new_kddm_set(kddm_def_ns,
						   STATIC_CPU_INFO_KDDM_ID,
						   STATIC_CPU_INFO_LINKER,
						   KDDM_CUSTOM_DEF_OWNER,
						   sizeof
						   (krg_static_cpu_info_t), 0);
	if (IS_ERR(static_cpu_info_ctnr))
		OOM;

	for_each_online_cpu (i) {
		cpu_id = krg_cpu_id(i);
		static_cpu_info =
			_kddm_grab_object(static_cpu_info_ctnr, cpu_id);

		static_cpu_info->info = cpu_data[i];
		static_cpu_info->info.krg_cpu_id = cpu_id;
#ifndef CONFIG_USERMODE
		static_cpu_info->info.cpu_khz = cpu_khz;
#endif

		_kddm_put_object(static_cpu_info_ctnr, cpu_id);
	}

	return 0;
}
