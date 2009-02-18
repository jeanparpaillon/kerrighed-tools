/** Common code for IPC mechanism accross the cluster
 *  @file ipc_handler.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */

#ifndef NO_IPC

#include "debug_keripc.h"

#define MODULE_NAME "IPC"

#include <linux/ipc.h>
#include <kerrighed/ipc.h>

#include <ctnr/kddm.h>
#include <hotplug/hotplug.h>
#include "ipcmap_io_linker.h"
#include "ipc_handler.h"

/*****************************************************************************/
/*                                                                           */
/*                                KERNEL HOOKS                               */
/*                                                                           */
/*****************************************************************************/


int kcb_ipc_get_max_id(struct ipc_namespace *ns, struct ipc_ids* ids)
{
	ipcmap_object_t *ipc_map;
	int max_id;

	BUG_ON (ns != &init_ipc_ns);

	DEBUG (DBG_KERIPC_IPC_MAP, 2, "looking kddm_set %ld\n", ids->map_ctnr);

	ipc_map = kddm_get_object(kddm_def_ns, ids->map_ctnr, 0);
	max_id = ipc_map->alloc_map - 1;
	kddm_put_object(kddm_def_ns, ids->map_ctnr, 0);

	DEBUG (DBG_KERIPC_IPC_MAP, 2, "Return max_id = %d\n", max_id);

	return max_id;
}



int kcb_get_ipc_id(struct ipc_ids* ids, int size)
{
	ipcmap_object_t *ipc_map, *max_id;
	int i = 1, id = -1, offset;

	max_id = kddm_grab_object(kddm_def_ns, ids->map_ctnr, 0);

	DEBUG (DBG_KERIPC_IPC_MAP, 2, "Ask for size %d\n", size);

	while ((i <= size / BITS_PER_LONG || (size < BITS_PER_LONG && i <= size)) 
	       && (id == -1)) {
		ipc_map = kddm_grab_object(kddm_def_ns, ids->map_ctnr, i);

		if (ipc_map->alloc_map != ULONG_MAX) {
			offset = find_first_zero_bit(&ipc_map->alloc_map, 
						     BITS_PER_LONG);

			DEBUG (DBG_KERIPC_IPC_MAP, 4, "Found bit %d in map "
			       "nr %d\n", offset, i);

			if (offset < BITS_PER_LONG) {
				DEBUG (DBG_KERIPC_IPC_MAP, 4, "Found free "
				       "bit at %d in map nr %d\n", offset, i);
				id = (i-1) * BITS_PER_LONG + offset;
				if (id < size) {
					set_bit(offset, &ipc_map->alloc_map);
					if (id >= max_id->alloc_map)
						max_id->alloc_map = id + 1;
				}
				else
					id = -1;
			}
			else
				DEBUG (DBG_KERIPC_IPC_MAP, 4, "No free bit "
				       "found in map nr %d\n", i);
		}

		kddm_put_object(kddm_def_ns, ids->map_ctnr, i);
		i++;
	}

	kddm_put_object(kddm_def_ns, ids->map_ctnr, 0);

	DEBUG (DBG_KERIPC_IPC_MAP, 2, "Allocated id : %d\n", id);

	return id;
}



int kcb_ipc_findkey(struct ipc_ids* ids, key_t key)
{
	long *key_index;
	int id = -1;
	
	if (ids->key_ctnr == KDDM_SET_UNUSED)
		return id;

	key_index = kddm_get_object_no_ft(kddm_def_ns, ids->key_ctnr, key);

	if (key_index)
		id = *key_index;

	kddm_put_object (kddm_def_ns, ids->key_ctnr, key);

	return id;
}



int free_ipc_id(struct ipc_ids* ids, int index)
{
	ipcmap_object_t *ipc_map, *max_id;
	int i, offset;

	/* Clear the corresponding entry in the bit field */

	DEBUG (DBG_KERIPC_IPC_MAP, 2, "Remove ipc id at index %d\n", index);

	i = 1 + index / BITS_PER_LONG;
	offset = index % BITS_PER_LONG;

	ipc_map = kddm_grab_object(kddm_def_ns, ids->map_ctnr, i);

	clear_bit(offset, &ipc_map->alloc_map);

	kddm_put_object(kddm_def_ns, ids->map_ctnr, i);

	/* Check if max_id must be adjusted */

	max_id = kddm_grab_object(kddm_def_ns, ids->map_ctnr, 0);

	DEBUG (DBG_KERIPC_IPC_MAP, 4, "Max id = %ld\n",
	       max_id->alloc_map - 1);

	if (max_id->alloc_map != index + 1)
		goto done;

	for (; i > 0; i--) {
		
		ipc_map = kddm_grab_object(kddm_def_ns, ids->map_ctnr, i);
		if (ipc_map->alloc_map != 0) {
			for (; offset >= 0; offset--) {
				if (test_bit (offset, &ipc_map->alloc_map)) {
					max_id->alloc_map = 1 + offset +
						(i - 1) * BITS_PER_LONG;
					kddm_put_object(kddm_def_ns, ids->map_ctnr, i);
					goto done;
				}
			}
		}
		offset = 31;
		kddm_put_object(kddm_def_ns, ids->map_ctnr, i);
	}

	max_id->alloc_map = 0;
done:
	DEBUG (DBG_KERIPC_IPC_MAP, 4, "Max id is now = %ld\n",
	       max_id->alloc_map - 1);

	kddm_put_object(kddm_def_ns, ids->map_ctnr, 0);

	return 0;
}


/*****************************************************************************/
/*                                                                           */
/*                              INITIALIZATION                               */
/*                                                                           */
/*****************************************************************************/



void ipc_handler_init (void)
{
	DEBUG (DBG_KERIPC_INITS, 1, "Start\n");

	hook_register(&kh_ipc_get_max_id, kcb_ipc_get_max_id) ;
	hook_register(&kh_get_ipc_id, kcb_get_ipc_id) ;
	hook_register(&kh_ipc_findkey, kcb_ipc_findkey) ;

	DEBUG (DBG_KERIPC_INITS, 1, "IPC Server configured\n");
}



void ipc_handler_finalize (void)
{
}

#endif
