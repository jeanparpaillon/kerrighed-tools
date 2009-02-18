/** Global management of faf files.
 *  @file faf_file_mgr.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */
#include "debug_faf.h"

#include <linux/file.h>
#include <linux/wait.h>

#include <kerrighed/file.h>

#include <ctnr/kddm.h>
#include <fs/file.h>
#include <fs/mobility.h>
#include <fs/physical_fs.h>
#include <epm/action.h>
#include <epm/application/app_shared.h>
#include <rpc/rpc.h>

#include "faf.h"
#include "faf_hooks.h"
#include "../regular_file_mgr.h"

struct kmem_cache *faf_client_data_cachep;



/** Create a faf file struct from a Kerrighed file descriptor.
 *  @author Renaud Lottiaux
 *
 *  @param task    Task to create the file for.
 *  @param desc    Kerrighed file descriptor.
 *
 *  @return   0 if everything ok.
 *            Negative value otherwise.
 */
struct file *create_faf_file_from_krg_desc (struct task_struct *task,
                                            void *_desc)
{
	faf_client_data_t *desc = _desc, *data;
	struct file *file = NULL;

	DEBUG("mobility", 3, -1, desc->server_id, desc->server_fd, -1UL,
	      task, NULL, FAF_LOG_ENTER, 0);

	data = kmem_cache_alloc (faf_client_data_cachep, GFP_KERNEL);
	if (!data)
		return NULL;

	file = get_empty_filp ();

	if (!file) {
		kmem_cache_free (faf_client_data_cachep, data);
		goto exit;
	}

	*data = *desc;
	init_waitqueue_head(&data->poll_wq);

	file->f_dentry = NULL;
	file->f_op = &faf_file_ops;
	file->f_flags = desc->flags | O_FAF_CLT;
	file->f_mode = desc->mode;
	file->f_pos = desc->pos;
	file->private_data = data;

exit:
	DEBUG("mobility", 3, -1, desc->server_id, desc->server_fd, -1UL,
	      task, NULL, FAF_LOG_EXIT, 0);

	return file;
}



/** Return a kerrighed descriptor corresponding to the given file.
 *  @author Renaud Lottiaux
 *
 *  @param file       The file to get a Kerrighed descriptor for.
 *  @param desc       The returned descriptor.
 *  @param desc_size  Size of the returned descriptor.
 *
 *  @return   0 if everything ok.
 *            Negative value otherwise.
 */
int get_ckp_faf_file_krg_desc (int index,
			       struct file *file,
			       void **desc,
			       int *desc_size)
{
	char * tmp = (char*)__get_free_page(GFP_KERNEL), *file_name;
	regular_file_krg_desc_t *data;
	int name_len, size, r = -ENOENT;

	file_name = faf_d_path(file, tmp, PAGE_SIZE);
	if (!file_name)
		goto exit;

	DEBUG("mobility", 4, index, faf_srv_id(file), faf_srv_fd(file), -1UL,
	      NULL, file, FAF_LOG_ENTER, 0);

	name_len = strlen (file_name);
	size = sizeof (regular_file_krg_desc_t) + name_len;

	data = kmalloc (size, GFP_KERNEL);
	if (!data) {
		r = -ENOMEM;
		goto exit;
	}

	strncpy (data->filename, file_name, name_len + 1);

	data->file_objid = file->f_objid;
	data->flags = file->f_flags & (~(O_FAF_SRV | O_FAF_CLT));
	data->mode = file->f_mode;
	data->pos = file->f_pos;
	data->sysv = 0;
	data->ctnrid = KDDM_SET_UNUSED;

	*desc = data;
	*desc_size = size;

	r = 0;
exit:
	free_page((unsigned long)tmp);

	DEBUG("mobility", 4, index, faf_srv_id(file), faf_srv_fd(file), -1UL,
	      NULL, file, FAF_LOG_EXIT, 0);

	return r ;
}

static void __fill_faf_file_krg_desc(faf_client_data_t *data,
				     struct file *file)
{
	data->flags = file->f_flags & (~O_FAF_SRV);
	data->mode = file->f_mode;
	data->pos = file->f_pos;
	data->server_id = kerrighed_node_id;
	data->server_fd = file->f_faf_srv_index;
}


/** Return a kerrighed descriptor corresponding to the given file.
 *  @author Renaud Lottiaux
 *
 *  @param file       The file to get a Kerrighed descriptor for.
 *  @param desc       The returned descriptor.
 *  @param desc_size  Size of the returned descriptor.
 *
 *  @return   0 if everything ok.
 *            Negative value otherwise.
 */
int get_faf_file_krg_desc (struct file *file,
                           void **desc,
                           int *desc_size)
{
	faf_client_data_t *data, *ldata;

	DEBUG("mobility", 4, 0, faf_srv_id(file), faf_srv_fd(file), -1UL,
	      NULL, file, FAF_LOG_ENTER, 0);

	data = kmalloc(sizeof(faf_client_data_t), GFP_KERNEL);
	if (data == NULL)
		return -ENOMEM;

	/* The file descriptor is already a FAF client desc */

	if (file->f_flags & O_FAF_CLT) {
		ldata = file->private_data;
		*data = *ldata;
		goto done;
	}

	BUG_ON (!(file->f_flags & O_FAF_SRV));

	__fill_faf_file_krg_desc(data, file);

done:
	*desc = data;
	*desc_size = sizeof (faf_client_data_t);

	DEBUG("mobility", 4, 0, faf_srv_id(file), faf_srv_fd(file), -1UL,
	      NULL, file, FAF_LOG_EXIT, 0);

	return 0;
}

static int cr_add_file_to_shared_table(struct task_struct *task,
				       int index, struct file *file)
{
	int r;
	union export_args args;
	long key = file->f_objid;
	enum shared_obj_type type = FAF_FILE;

	BUG_ON(!(file->f_flags & (O_FAF_CLT | O_FAF_SRV)));

	args.file_args.index = index;
	args.file_args.file = file;

	r = add_to_shared_objects_list(&task->application->shared_objects,
				       type, key, 0 /* !is_local */,
				       task, &args);

	if (r == -ENOKEY) /* the file was already in the list */
		r = 0;

	return r;
}

static int cr_write_faf_file_id(ghost_t *ghost, struct file *file)
{
	int r;
	long key = file->f_objid;
	enum shared_obj_type type = FAF_FILE;


	r = ghost_write(ghost, &type, sizeof(enum shared_obj_type));
	if (r)
		goto error;

	r = ghost_write(ghost, &key, sizeof(long));

error:
	return r;
}

struct cr_faf_link {
	unsigned long dvfs_objid;
	faf_client_data_t desc;
};

static int cr_link_to_faf_file(struct epm_action *action,
			       ghost_t *ghost,
			       struct task_struct *task,
			       struct file ** returned_file)
{
	int r = 0;
 	long key;
	enum shared_obj_type type;
	struct cr_faf_link *faf_link;
	struct dvfs_file_struct *dvfs_file = NULL;
	struct file *file = NULL;
	int first_import = 0;

	BUG_ON(action->type != EPM_CHECKPOINT);
	BUG_ON(action->restart.shared != CR_LINK_ONLY);

	r = ghost_read(ghost, &type, sizeof(enum shared_obj_type));
	if (r)
		goto exit_no_put;

	if (type != FAF_FILE) {
		BUG();
		r = -E_CR_BADDATA;
		goto exit_no_put;
	}

	r = ghost_read(ghost, &key, sizeof(long));
	if (r)
		goto exit_no_put;

	faf_link = (struct cr_faf_link *)get_imported_shared_object(
		&action->restart.app->shared_objects,
		FAF_FILE, key);

	if (!faf_link) {
		BUG();
		r = -E_CR_BADDATA;
		goto exit;
	}

	/* Check if the file struct is already present */
	file = begin_import_dvfs_file(faf_link->dvfs_objid, &dvfs_file);

	/* reopen the file if needed */
	if (!file) {
		file = create_faf_file_from_krg_desc(task, &faf_link->desc);
		first_import = 1;
	}

	r = end_import_dvfs_file(faf_link->dvfs_objid, dvfs_file, file,
				 first_import);

	if (r)
		goto exit;

	*returned_file = file;

	DEBUG("mobility", 2, 0, faf_srv_id(*returned_file),
	      faf_srv_fd(*returned_file), faf_link->dvfs_objid, task, file,
	      DVFS_LOG_IMPORT_INFO, *returned_file);

exit:
	put_dvfs_file_struct(faf_link->dvfs_objid);

exit_no_put:

	return r;
}


/*****************************************************************************/
/*                                                                           */
/*                            FAF FILES IMPORT/EXPORT                        */
/*                                                                           */
/*****************************************************************************/



/** Export a faf file descriptor into the given ghost.
 *  @author Renaud Lottiaux
 *
 *  @param ghost    The ghost to write data to.
 *  @param tsk      Task we are exporting.
 *  @parem index    Index of the exported file in the open files array.
 *  @param file     The file to export.
 *
 *  @return   0 if everything ok.
 *            Negative value otherwise.
 */
int faf_file_export (struct epm_action *action,
		     ghost_t *ghost,
		     struct task_struct *task,
		     int index,
		     struct file *file)
{
	void *desc;
	int desc_size;
	int r = 0;

	DEBUG("mobility", 2, index, faf_srv_id(file), faf_srv_fd(file),
	      file->f_objid, task, file, FAF_LOG_EXPORT_FAF_FILE, 0);

	BUG_ON(!(file->f_flags & (O_FAF_CLT | O_FAF_SRV)));

	if (action->type == EPM_CHECKPOINT) {
		BUG_ON(action->checkpoint.shared == CR_SAVE_LATER);

		if (action->checkpoint.shared == CR_FILES_ADD) {
			r = cr_add_file_to_shared_table(task, index, file);
			return r;
		} else if  (action->checkpoint.shared == CR_FILES_WRITEID) {
			r = cr_write_faf_file_id(ghost, file);
			return r;
		}

		r = get_ckp_faf_file_krg_desc(index, file, &desc, &desc_size);
	} else
		r = get_faf_file_krg_desc(file, &desc, &desc_size);

	if (r)
		goto error;

	r = ghost_write_file_krg_desc(ghost, desc, desc_size);
	kfree(desc);

error:
	DEBUG("mobility", 3, index, faf_srv_id(file), faf_srv_fd(file),
	      file->f_objid, task, file, FAF_LOG_EXIT, 0);
	return r;
}



/** Import a faf file descriptor from the given ghost.
 *  @author Renaud Lottiaux
 *
 *  @param ghost          The ghost to read data from.
 *  @param task           The task data are imported for.
 *  @param returned_file  The file struct where data should be imported to.
 *
 *  @return   0 if everything ok.
 *            Negative value otherwise.
 */
int faf_file_import (struct epm_action *action,
		     ghost_t *ghost,
		     struct task_struct *task,
		     struct file **returned_file)
{
	void *desc;
	struct file *file;
	int r;

	DEBUG("mobility", 2, -1, -1, -1, 0, task, NULL, FAF_LOG_ENTER, 0);

 	if (action->type == EPM_CHECKPOINT
 	    && action->restart.shared == CR_LINK_ONLY) {
 		r = cr_link_to_faf_file(action, ghost, task,
 					returned_file);
 		return r;
 	}

	r = ghost_read_file_krg_desc(ghost, &desc);
	if (r)
		goto exit;

	if (action->type == EPM_CHECKPOINT)
		file = import_regular_file_from_krg_desc (action, task, desc);
	else
		file = create_faf_file_from_krg_desc (task, desc);

	if (IS_ERR(file)) {
		r = PTR_ERR(file);
		goto exit_free_desc;
	}
	*returned_file = file;

	DEBUG("mobility", 3, -1, faf_srv_id(file), faf_srv_fd(file), 0, task,
	      file, FAF_LOG_EXIT, 0);

exit_free_desc:
	kfree(desc);
exit:
	return r;
}



struct dvfs_mobility_operations dvfs_mobility_faf_ops = {
	.file_export = faf_file_export,
	.file_import = faf_file_import,
};

static int cr_export_now_faf_file(struct epm_action *action,
				  ghost_t *ghost,
				  struct task_struct *task,
				  union export_args *args)
{
	int r;

	r = faf_file_export(action, ghost, task,
			    args->file_args.index,
			    args->file_args.file);
	return r;
}

static int cr_import_now_faf_file(struct epm_action *action,
				  ghost_t *ghost,
				  struct task_struct *fake,
				  void ** returned_data)
{
	int r;
	struct file *file;
	struct cr_faf_link *faf_link = *returned_data;

	r = faf_file_import(action, ghost, fake, &file);
	if (r)
		goto err;

	r = setup_faf_file(file);
	if (r)
		goto err;

	faf_link->dvfs_objid = file->f_objid;
	__fill_faf_file_krg_desc(&(faf_link->desc), file);
err:
	return r;
}

int cr_import_complete_faf_file(struct task_struct *fake,
				void *_faf_link)
{
	struct cr_faf_link *faf_link = _faf_link;
	struct dvfs_file_struct *dvfs_file = NULL;
	struct file *file = NULL;

	dvfs_file = grab_dvfs_file_struct(faf_link->dvfs_objid);
	file = dvfs_file->file;
	put_dvfs_file_struct (faf_link->dvfs_objid);
	if (file)
		fput(file);

	return 0;
}

int cr_delete_faf_file(struct task_struct *fake, void *_faf_link)
{
	return cr_import_complete_faf_file(fake, _faf_link);
}

struct shared_object_operations cr_shared_faf_file_ops = {
        .restart_data_size = sizeof(struct cr_faf_link),
        .export_now        = cr_export_now_faf_file,
	.import_now        = cr_import_now_faf_file,
	.import_complete   = cr_import_complete_faf_file,
	.delete            = cr_delete_faf_file,
};
