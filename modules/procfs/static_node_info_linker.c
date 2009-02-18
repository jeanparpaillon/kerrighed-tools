/** Static node information management.
 *  @file static_node_info_linker.c
 *  
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */

#define MODULE_NAME "Static Node Info"
#include "debug_procfs.h"

#ifndef FILE_NONE
#  if defined(FILE_STATIC_NODE_INFO) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

#include <tools/debug.h>

#include <linux/swap.h>

#include <ctnr/kddm.h>

#include "static_node_info_linker.h"

struct kddm_set *static_node_info_ctnr = NULL;


/*****************************************************************************/
/*                                                                           */
/*                   STATIC NODE INFO CONTAINER IO FUNCTIONS                 */
/*                                                                           */
/*****************************************************************************/

/****************************************************************************/

/* Init the static node info IO linker */

struct iolinker_struct static_node_info_io_linker = {
      linker_name:"stat_node_nfo",
      linker_id:STATIC_NODE_INFO_LINKER,
};

int init_static_node_info_ctnr()
{
	krg_static_node_info_t *static_node_info;

	register_io_linker(STATIC_NODE_INFO_LINKER,
			   &static_node_info_io_linker);

	/* Create the static node info container */

	static_node_info_ctnr = create_new_kddm_set(kddm_def_ns,
						    STATIC_NODE_INFO_KDDM_ID,
						    STATIC_NODE_INFO_LINKER,
						    KDDM_RR_DEF_OWNER,
						    sizeof
						    (krg_static_node_info_t),
						    0);
	if (IS_ERR(static_node_info_ctnr))
		OOM;

	static_node_info = _kddm_grab_object(static_node_info_ctnr,
					     kerrighed_node_id);

	static_node_info->nr_cpu = num_online_cpus();
	static_node_info->totalram = totalram_pages;
	static_node_info->totalhigh = totalhigh_pages;

	_kddm_put_object(static_node_info_ctnr, kerrighed_node_id);

	return 0;
}
