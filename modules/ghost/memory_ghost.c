/** Memory Ghost interface.
 *  @memory memory_ghost.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#define MODULE_NAME "MEMORY GHOST"
#include "debug_ghost.h"

#ifndef MEMORY_NONE
#  if defined(MEMORY_MEMORY_GHOST) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

#include <ctnr/kddm.h>
#include <linux/syscalls.h>
#include "ghost.h"
#include "memory_ghost.h"

/*--------------------------------------------------------------------------*
 *                                                                          *
 *  Functions to implement ghost interface                                  *
 *                                                                          *
 *--------------------------------------------------------------------------*/

/** Read data from a memory ghost.
 *  @author Matthieu Fertré
 *
 *  @param  ghost   Ghost to read data from.
 *  @param  buff    Buffer to store data.
 *  @param  length  Size of data to read.
 *
 *  @return        0 if everything ok
 *                 Negative value otherwise.
 */
int memory_ghost_read(ghost_t *ghost, void *buff, size_t length)
{
	memory_ghost_data_t *ghost_data ;
	kddm_set_id_t ctnr_id;

	struct page * page;
	void * addr;
	loff_t page_offset, buff_offset;
	size_t relative_length;
	objid_t objid = 0;

	BUG_ON (ghost == NULL);
	BUG_ON (buff == NULL);

	ghost_data = (memory_ghost_data_t *)ghost->data ;

	ctnr_id = ghost_data->ctnr_id ;

	// read the data from the pages
	buff_offset = 0;
	do {
		page_offset = ghost_data->offset % PAGE_SIZE;
		objid = ghost_data->offset / PAGE_SIZE;
		relative_length = min((loff_t)length, (loff_t)PAGE_SIZE-page_offset);
		length -= relative_length;

		page = kddm_get_object(kddm_def_ns, ctnr_id, objid);
		BUG_ON(page == NULL);
		addr = kmap_atomic(page, KM_USER0);

		memcpy(buff + buff_offset, addr + page_offset, relative_length);

		ghost_data->offset += relative_length;
		buff_offset += relative_length;

		kunmap_atomic(addr, KM_USER0);

		kddm_put_object(kddm_def_ns, ctnr_id, objid);
	} while (length > 0);

	return 0;
}



/** Write data to a memory ghost.
 *  @author Matthieu Fertré
 *
 *  @param  ghost   Ghost to write data to.
 *  @param  buff    Buffer to write in the ghost.
 *  @param  length  Size of data to write.
 *
 *  @return        0 if everything ok
 *                 Negative value otherwise.
 */
int memory_ghost_write(struct ghost *ghost, const void *buff, size_t length)
{
	memory_ghost_data_t *ghost_data ;
	kddm_set_id_t ctnr_id;

	struct page * page;
	void * addr;
	loff_t page_offset, buff_offset;
	size_t relative_length;
	objid_t objid = 0;

	BUG_ON (ghost == NULL);
	BUG_ON (buff == NULL);

	ghost_data = (memory_ghost_data_t *)ghost->data ;

	ctnr_id = ghost_data->ctnr_id;

	// read the data from the pages
	buff_offset = 0;

	do {
		page_offset = ghost_data->offset % PAGE_SIZE;
		objid = ghost_data->offset / PAGE_SIZE;
		relative_length = min((loff_t)length, (loff_t)(PAGE_SIZE-page_offset));
		length -= relative_length;

		page = kddm_grab_object(kddm_def_ns, ctnr_id, objid );
		BUG_ON(page == NULL);
		addr = kmap_atomic(page, KM_USER0);

		memcpy(addr + page_offset, buff + buff_offset, relative_length);

		ghost_data->offset += relative_length;
		buff_offset += relative_length;

		kunmap_atomic(addr, KM_USER0);

		kddm_put_object(kddm_def_ns, ctnr_id, objid);

		// flush only if page is full
		if (ghost_data->offset / PAGE_SIZE != objid)
			kddm_flush_object (kddm_def_ns, ctnr_id, objid,
					   krgnode_next_online_in_ring(kerrighed_node_id));

	} while (length > 0);

	return 0;
}

/** Close a memory ghost.
 *  @author Matthieu Fertré
 *
 *  @param  ghost   Ghost to close.
 *
 *  @return        0 if everything ok
 *                 Negative value otherwise.
 */
int memory_ghost_close(struct ghost *ghost)
{
	memory_ghost_data_t *ghost_data ;
	ghost_data = (memory_ghost_data_t *)ghost->data ;

	// flush the last page when writing
	if (ghost->access == GHOST_WRITE) {
		objid_t objid = ghost_data->offset / PAGE_SIZE;
		kddm_flush_object (kddm_def_ns, ghost_data->ctnr_id, objid,
				   krgnode_next_online_in_ring(kerrighed_node_id));
	}

	return 0;
}


/** File ghost operations
 */
struct ghost_operations ghost_memory_ops = {
read  : &memory_ghost_read,
write : &memory_ghost_write,
close : &memory_ghost_close
} ;


/*--------------------------------------------------------------------------*
 *                                                                          *
 * Macros and functions used to manage memory ghost creation                *
 *                                                                          *
 *--------------------------------------------------------------------------*/

static inline ghost_t *__create_read_memory_ghost(long app_id,
						  int chkpt_sn,
						  int obj_id,
						  const char * label)
{
	memory_ghost_data_t *ghost_data ;
	ghost_t *ghost;
	int err;

	ghost = create_ghost (GHOST_MEMORY, GHOST_READ);
	if (IS_ERR(ghost))
		return ghost;

	ghost_data = kmalloc(sizeof(memory_ghost_data_t), GFP_KERNEL);
	if (ghost_data == NULL)
		return ERR_PTR(-ENOMEM);

	/* read ghost ctnrid from a file */
	{
		char * tmp_label;
		ghost_t *file_ghost;

		tmp_label = kmalloc (32*sizeof(char), GFP_KERNEL);

		sprintf (tmp_label, "%s_%s", "tmp", label);

		file_ghost = create_file_ghost(GHOST_READ, app_id, chkpt_sn, obj_id, tmp_label);
		kfree(tmp_label);

		if (IS_ERR(file_ghost))
			return file_ghost;

		err = ghost_read(file_ghost, &ghost_data->ctnr_id, sizeof(kddm_set_id_t));
		if (err)
			return ERR_PTR(err);
		ghost_close(file_ghost);
	}

	ghost_data->offset = 0;
	ghost_data->app_id = app_id;
	ghost_data->chkpt_sn = chkpt_sn;
	ghost_data->obj_id = obj_id;
	sprintf (ghost_data->label, "%s", label);

	ghost->data = ghost_data;
	ghost->ops = &ghost_memory_ops ;

	{
		/* Check it works ;-) */
		long c_app_id;
		int c_chkpt_sn, c_obj_id;

		err = ghost_read(ghost, &c_app_id, sizeof(long));
		if (err)
			return ERR_PTR(err);
		BUG_ON(c_app_id != app_id);
		err = ghost_read(ghost, &c_chkpt_sn, sizeof(int));
		if (err)
			return ERR_PTR(err);
		BUG_ON(c_chkpt_sn != chkpt_sn);
		err = ghost_read(ghost, &c_obj_id, sizeof(int));
		if (err)
			return ERR_PTR(err);
		BUG_ON(c_obj_id != obj_id);
	}

	return ghost ;
}

static inline ghost_t *__create_write_memory_ghost(long app_id,
						   int chkpt_sn,
						   int obj_id,
						   const char * label)
{
	struct kddm_set * container;
	memory_ghost_data_t *ghost_data ;
	ghost_t *ghost;
	int r;

	WARNING("There's currently no way to free the memory used !!\n");

	/* Create the ctnr hosting the checkpoint */

	container = create_new_kddm_set (kddm_def_ns, 0, MEMORY_LINKER,
					 kerrighed_node_id, PAGE_SIZE, 0);

	ghost = create_ghost (GHOST_MEMORY, GHOST_WRITE);
	if (IS_ERR(ghost))
		return ghost;

	ghost_data = kmalloc(sizeof(memory_ghost_data_t), GFP_KERNEL);
	if (ghost_data == NULL)
		return ERR_PTR(-ENOMEM);

	ghost_data->ctnr_id = container->id;
	ghost_data->offset = 0;
	ghost_data->app_id = app_id;
	ghost_data->chkpt_sn = chkpt_sn;
	ghost_data->obj_id = obj_id;
	sprintf (ghost_data->label, "%s", label);

	ghost->data = ghost_data;
	ghost->ops = &ghost_memory_ops ;


	/* write ghost ctnr_id in a file */
	{
		char * tmp_label;
		ghost_t *file_ghost;

		tmp_label = kmalloc (32*sizeof(char), GFP_KERNEL);

		sprintf (tmp_label, "%s_%s", "tmp", label);

		file_ghost = create_file_ghost(GHOST_WRITE, app_id, chkpt_sn, obj_id, tmp_label);

		kfree(tmp_label);

		if (IS_ERR(file_ghost))
			return file_ghost;

		r = ghost_write(file_ghost, &ghost_data->ctnr_id, sizeof(kddm_set_id_t));
		if (r)
			goto err_write;
		ghost_close(file_ghost);
	}

	/* To check if it works ;-) */
	r = ghost_write(ghost, &app_id, sizeof(long));
	if (r)
		goto err_write;
	r = ghost_write(ghost, &chkpt_sn, sizeof(int));
	if (r)
		goto err_write;
	r = ghost_write(ghost, &obj_id, sizeof(int));
	if (r)
		goto err_write;

exit:
	return ghost ;

err_write:
	ghost = ERR_PTR(r);
	goto exit;
}

/** Create a new memory ghost.
 *  @author Matthieu Fertré
 *
 *  @return        ghost_t if everything ok
 *                 NULL otherwise.
 */
ghost_t *create_memory_ghost ( int access,
			       long app_id,
			       int chkpt_sn,
			       int obj_id,
			       const char * label)
{


	if (access == GHOST_READ)
		return __create_read_memory_ghost(app_id, chkpt_sn, obj_id, label);
	else if (access == GHOST_WRITE)
		return __create_write_memory_ghost(app_id, chkpt_sn, obj_id, label);

	return ERR_PTR(-EPERM);
}


/** Delete a memory ghost.
 *  @author Matthieu Fertré
 *
 *  @param  ghost    Ghost file to delete
 */
void delete_memory_ghost(ghost_t *ghost)
{
	// free the memory pages
	memory_ghost_data_t* ghost_data = ghost->data;
	destroy_kddm_set(kddm_def_ns, ghost_data->ctnr_id);

	{
		char * tmp_label, * filename;

		tmp_label = kmalloc (32*sizeof(char), GFP_KERNEL);
		sprintf (tmp_label, "%s_%s", "tmp", ghost_data->label);

		filename = get_chkpt_filebase(ghost_data->app_id, ghost_data->chkpt_sn, ghost_data->obj_id, tmp_label);
		sys_unlink(filename);

		kfree(filename);
		kfree(tmp_label);
	}

	free_ghost(ghost);
}



