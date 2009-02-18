/** All the code for sharing sys V shared memory segments accross the cluster
 *  @file shm_handler.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */

#ifndef NO_SHM

#include "debug_keripc.h"

#define MODULE_NAME "System V shm    "

#ifdef SHM_HANDLER_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <linux/shm.h>

#include <ctnr/kddm.h>
#include <hotplug/hotplug.h>
#include <kerrighed/shm.h>
#include "ipc_handler.h"
#include "shm_handler.h"
#include "shmid_io_linker.h"
#include "ipcmap_io_linker.h"
#include "shm_memory_linker.h"


/* Kddm set of SHM ids structures */
struct kddm_set *shmid_struct_kddm_set = NULL;
struct kddm_set *shmkey_struct_kddm_set = NULL;

/* Kddm set of IPC allocation bitmap structures */
struct kddm_set *shmmap_struct_kddm_set = NULL;

/*****************************************************************************/
/*                                                                           */
/*                                KERNEL HOOKS                               */
/*                                                                           */
/*****************************************************************************/

struct shmid_kernel *kcb_shm_lock(struct ipc_namespace *ns, int id)
{
	shmid_object_t *shp_object;
	struct shmid_kernel *shp;
	int index ;

	BUG_ON (ns != &init_ipc_ns);

	index = id % SEQ_MULTIPLIER;

	DEBUG (DBG_KERIPC_SHM_LOCK, 2, "Lock SHM %d(%d)\n", id, index);

	shp_object = _kddm_grab_object_no_ft(shmid_struct_kddm_set, index);

	if (!shp_object) {
		_kddm_put_object(shmid_struct_kddm_set, index);
		return NULL;
	}

	DEBUG (DBG_KERIPC_MSG_LOCK, 5, "shp_object: %p\n", shp_object);
	shp = shp_object->local_shp;

	BUG_ON(!shp);

	mutex_lock(&shp->shm_perm.mutex);

	if (shp->shm_perm.deleted) {
		mutex_unlock(&shp->shm_perm.mutex);
		shp = NULL;
	}

	DEBUG (DBG_KERIPC_SHM_LOCK, 2, "Lock SHM %d(%d) done: %p (natt %ld)\n",
	       id, index, shp, shp->shm_nattch);

	return shp;
}



void kcb_shm_unlock(struct shmid_kernel *shp)
{
	int index ;

	index = shp->id % SEQ_MULTIPLIER;

	DEBUG (DBG_KERIPC_SHM_LOCK, 2, "Unlock SHM %d(%d) - %p (natt %ld)\n",
	       shp->id, index, shp, shp->shm_nattch);

	if (shp->shm_perm.deleted)
		return;

	_kddm_put_object(shmid_struct_kddm_set, index);

	mutex_unlock(&shp->shm_perm.mutex);

	DEBUG (DBG_KERIPC_SHM_LOCK, 2, "Unlock SHM %d(%d) : done\n",
	       shp->id, index);
}



/** Notify the creation of a new shm segment to Kerrighed.
 *
 *  @author Renaud Lottiaux
 */
int kcb_shm_newseg (struct ipc_namespace *ns, struct shmid_kernel *shp)
{
	shmid_object_t *shp_object;
	struct kddm_set *ctnr;
	long *key_index;
	int index ;

	BUG_ON (ns != &init_ipc_ns);

	index = shp->id % SEQ_MULTIPLIER;

	DEBUG (DBG_KERIPC_SHM_NEWSEG, 2, "New SHM : id %d(%d) (%p)\n", shp->id,
	       index, shp);

	shp_object = _kddm_grab_object_manual_ft(shmid_struct_kddm_set, index);

	BUG_ON (shp_object != NULL);

	shp_object = kmem_cache_alloc (shmid_object_cachep, GFP_KERNEL);
	if (shp_object == NULL)
		return -ENOMEM;

	/* Create a container to host segment pages */
	ctnr = _create_new_kddm_set (kddm_def_ns, 0, SHM_MEMORY_LINKER,
				     kerrighed_node_id, PAGE_SIZE,
				     &shp->id, sizeof(int), 0);
	if (IS_ERR (ctnr))
	{
		shp->id = PTR_ERR (ctnr);
		return shp->id;
	}

	_increment_kddm_set_usage (ctnr);

	local_shm_lock(ns, shp->id);

	shp->shm_file->f_dentry->d_inode->i_mapping->kddm_set = ctnr;
	shp->shm_file->f_op = &krg_shmem_file_operations;

	shp_object->set_id = ctnr->id;

	shp_object->local_shp = shp;

	_kddm_set_object(shmid_struct_kddm_set, index, shp_object);

	local_shm_unlock(shp);

	_kddm_put_object(shmid_struct_kddm_set, index);

	if (shp->shm_perm.key != IPC_PRIVATE)
	{
		key_index = _kddm_grab_object(shmkey_struct_kddm_set,
					      shp->shm_perm.key);
		*key_index = index;
		_kddm_put_object (shmkey_struct_kddm_set, shp->shm_perm.key);
	}

	DEBUG (DBG_KERIPC_SHM_NEWSEG, 2, "New SHM (id %d(%d)) : done\n",
	       shp->id, index);

	return 0;
}

void kcb_shm_rmkey (struct ipc_namespace *ns, key_t key)
{
	_kddm_remove_object (shmkey_struct_kddm_set, key);
}



void kcb_shm_destroy (struct ipc_namespace *ns, struct shmid_kernel *shp)
{
	int index ;
	key_t key;

	index = shp->id % SEQ_MULTIPLIER;
	key = shp->shm_perm.key;

	DEBUG (DBG_KERIPC_SHM_NEWSEG, 2, "Destroy shm %d(%d)\n", shp->id,
	       index);

	if (key != IPC_PRIVATE)
		_kddm_remove_object (shmkey_struct_kddm_set, key);

	_kddm_remove_frozen_object (shmid_struct_kddm_set, index);

	free_ipc_id (&shm_ids(ns), index);
}



/*****************************************************************************/
/*                                                                           */
/*                              INITIALIZATION                               */
/*                                                                           */
/*****************************************************************************/



void shm_handler_init (void)
{
	DEBUG (DBG_KERIPC_INITS, 1, "Start\n");

	shmid_object_cachep = kmem_cache_create("shmid_object",
						sizeof(shmid_object_t),
						0, SLAB_PANIC, NULL, NULL);

	register_io_linker (SHM_MEMORY_LINKER, &shm_memory_linker);
	register_io_linker (SHMID_LINKER, &shmid_linker);
	register_io_linker (SHMKEY_LINKER, &shmkey_linker);

	shmid_struct_kddm_set = create_new_kddm_set (kddm_def_ns,
						     SHMID_KDDM_ID,
						     SHMID_LINKER,
						     KDDM_RR_DEF_OWNER,
						     sizeof(shmid_object_t),
						     0);

	BUG_ON (IS_ERR (shmid_struct_kddm_set));

	shmkey_struct_kddm_set = create_new_kddm_set (kddm_def_ns,
						      SHMKEY_KDDM_ID,
						      SHMKEY_LINKER,
						      KDDM_RR_DEF_OWNER,
						      sizeof(long), 0);

	BUG_ON (IS_ERR (shmkey_struct_kddm_set));

	shmmap_struct_kddm_set = create_new_kddm_set (kddm_def_ns,
						      SHMMAP_KDDM_ID,
						      IPCMAP_LINKER,
						      KDDM_RR_DEF_OWNER,
						      sizeof(ipcmap_object_t),
						      0);
	
	BUG_ON (IS_ERR (shmmap_struct_kddm_set));

	shm_ids(&init_ipc_ns).id_ctnr = SHMID_KDDM_ID;
	shm_ids(&init_ipc_ns).key_ctnr = SHMKEY_KDDM_ID;
	shm_ids(&init_ipc_ns).map_ctnr = SHMMAP_KDDM_ID;
	
	hook_register(&kh_shm_newseg, kcb_shm_newseg) ;
	hook_register(&kh_shm_lock, kcb_shm_lock) ;
	hook_register(&kh_shm_unlock, kcb_shm_unlock) ;
	hook_register(&kh_shm_destroy, kcb_shm_destroy) ;
	hook_register(&kh_shm_rmkey, kcb_shm_rmkey) ;

	DEBUG (DBG_KERIPC_INITS, 1, "Shm Server configured\n");
}



void shm_handler_finalize (void)
{
}

#endif
