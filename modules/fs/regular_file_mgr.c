/** Global management of regular files.
 *  @file regular_file_mgr.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */

#include <linux/mutex.h>
#include <linux/ipc.h>
#include <linux/file.h>
#include <linux/shm.h>
#include <linux/msg.h>
#include <ipc/shm_memory_linker.h>
#include <epm/action.h>
#include <epm/application/app_shared.h>

#include <ctnr/kddm.h>

#include "file.h"
#include "regular_file_mgr.h"
#include "mobility.h"
#include "physical_fs.h"
#include "debug_fs.h"



/*****************************************************************************/
/*                                                                           */
/*                             REGULAR FILES CREATION                        */
/*                                                                           */
/*****************************************************************************/



struct file *reopen_file_entry_from_krg_desc (struct epm_action *action,
					      struct task_struct *task,
                                              regular_file_krg_desc_t *desc)
{
	struct file *file = NULL;

	BUG_ON (!task);
	BUG_ON (!desc);

	DEBUG("reg_file_mgr", 3, -1, -1, -1, -1UL, task, NULL,
	      DVFS_LOG_ENTER, 0);

	file = open_physical_file (desc->filename, desc->flags, desc->mode,
				   desc->uid, desc->gid);

	if (IS_ERR (file))
		return file;

	file->f_pos = desc->pos;

	if (action->type == EPM_CHECKPOINT && desc->file_objid) {
		/* the file was shared before the checkpoint */
		create_ctnr_file_object(file);
	}

	DEBUG("reg_file_mgr", 3, -1, -1, -1, -1UL, task, NULL,
	      DVFS_LOG_EXIT, 0);

	return file;
}



struct file *create_file_entry_from_krg_desc (struct task_struct *task,
                                              regular_file_krg_desc_t *desc)
{
	struct file *file = NULL;

	BUG_ON (!task);
	BUG_ON (!desc);

	DEBUG("reg_file_mgr", 2, -1, -1, -1, -1UL, task, NULL,
	      DVFS_LOG_ENTER, 0);

	/* Set FS uid & gid to the user ones */

	current->fsuid = task->fsuid;
	current->fsgid = task->fsgid;

	file = filp_open(desc->filename, desc->flags, desc->mode);

	if (IS_ERR (file))
		return file;

	file->f_pos = desc->pos;
	file->f_dentry->d_inode->i_mode |= desc->mode;

	current->fsuid = current->uid;
	current->fsgid = current->gid;

	DEBUG("reg_file_mgr", 3, -1, -1, -1, -1UL, task, NULL,
	      DVFS_LOG_EXIT, 0);

	return file;
}



#ifdef CONFIG_KRG_IPC
struct file *reopen_shm_file_entry_from_krg_desc (struct task_struct *task,
						regular_file_krg_desc_t *desc)
{
	int shmid;
	int err = 0;
	struct shmid_kernel *shp;
	struct file *file = NULL;

	BUG_ON (!task);
	BUG_ON (!desc);

	shmid = desc->shmid;

	DEBUG("reg_file_mgr", 2, -1, -1, -1, -1UL, task, NULL,
	      DVFS_LOG_ENTER, 0);

	shp = shm_lock(&init_ipc_ns, shmid);
	if(shp == NULL) {
		err = -EINVAL;
		goto out;
	}

	file = shp->shm_file;
	get_file (file);
	shp->shm_nattch++;
	shm_unlock(shp);

out:
	DEBUG("reg_file_mgr", 3, -1, -1, -1, -1UL, task, NULL,
	      DVFS_LOG_EXIT, 0);

	if (err)
		file = ERR_PTR(err);

	return file;
}
#endif



/** Create a regular file struct from a Kerrighed file descriptor.
 *  @author Renaud Lottiaux, Matthieu Fertré
 *
 *  @param action  EPM action.
 *  @param task    Task to create the file for.
 *  @param desc    Kerrighed file descriptor.
 *
 *  @return   0 if everything ok.
 *            Negative value otherwise.
 */
struct file *import_regular_file_from_krg_desc (struct epm_action *action,
						struct task_struct *task,
                                                void *_desc)
{
	regular_file_krg_desc_t *desc = _desc;

	BUG_ON (!task);
	BUG_ON (!desc);

#ifdef CONFIG_KRG_IPC
	if (desc->sysv)
		return reopen_shm_file_entry_from_krg_desc (task, desc);
#endif

	if (desc->ctnrid != KDDM_SET_UNUSED)
		return create_file_entry_from_krg_desc (task, desc);
	else
		return reopen_file_entry_from_krg_desc (action, task, desc);
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
int get_shm_file_krg_desc (struct file *file,
			   void **desc,
			   int *desc_size)
{
	regular_file_krg_desc_t *data;
	int size, r = -ENOENT;

	DEBUG("reg_file_mgr", 2, -1, faf_srv_id(file), faf_srv_fd(file),
	      file->f_objid, NULL, file, DVFS_LOG_ENTER, 0);

	size = sizeof (regular_file_krg_desc_t);

	data = kmalloc (size, GFP_KERNEL);
	if (!data) {
		r = -ENOMEM;
		goto exit;
	}

	data->shmid = file->f_dentry->d_inode->i_ino ;
	data->sysv = 1;
	*desc = data;
	*desc_size = size;

	r = 0;
exit:
	DEBUG("reg_file_mgr", 2, -1, faf_srv_id(file), faf_srv_fd(file),
	      file->f_objid, NULL, file, DVFS_LOG_EXIT, r);

	return r;
}



int check_flush_file (struct epm_action *action,
		      fl_owner_t id,
		      struct file *file)
{
	int err = 0;

	switch (action->type) {
	case EPM_REMOTE_CLONE:
	case EPM_MIGRATE:
	case EPM_CHECKPOINT:
		  if (file->f_dentry) {
			  if (file->f_op && file->f_op->flush)
				  err = file->f_op->flush(file, id);
		  }

		  break;

	  default:
		  break;
	}

	return err;
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
int get_regular_file_krg_desc (struct file *file,
                               void **desc,
                               int *desc_size)
{
	char *tmp = (char *) __get_free_page (GFP_KERNEL), *file_name;
	regular_file_krg_desc_t *data;
	int size = 0, name_len;
	int r = -ENOENT;

	DEBUG("reg_file_mgr", 4, -1, faf_srv_id(file), faf_srv_fd(file),
	      file->f_objid, NULL, file, DVFS_LOG_ENTER, 0);

#ifdef CONFIG_KRG_IPC
	if (file->f_op == &krg_shmem_file_operations) {
		r = get_shm_file_krg_desc (file, desc, desc_size);
		goto exit;
	}
#endif

	file_name = physical_d_path (file->f_dentry, file->f_vfsmnt,
				     tmp);

	if (!file_name)
		goto exit;

	name_len = strlen (file_name);
	size = sizeof (regular_file_krg_desc_t) + name_len;

	data = kmalloc (size, GFP_KERNEL);
	if (!data) {
		r = -ENOMEM;
		goto exit;
	}

	strncpy (data->filename, file_name, name_len + 1);

	data->file_objid = file->f_objid;
	data->flags = file->f_flags;
	data->mode = file->f_dentry->d_inode->i_mode & S_IRWXUGO;
	data->pos = file->f_pos;
	data->sysv = 0;
	data->uid = file->f_uid;
	data->gid = file->f_gid;
	if (file->f_dentry->d_inode->i_mapping->kddm_set)
		data->ctnrid = file->f_dentry->d_inode->i_mapping->kddm_set->id;
	else
		data->ctnrid = KDDM_SET_UNUSED;

	*desc = data;
	*desc_size = size;

	r = 0;

exit:
	DEBUG("reg_file_mgr", 4, -1, faf_srv_id(file), faf_srv_fd(file),
	      file->f_objid, NULL, file, DVFS_LOG_EXIT, r);

	free_page ((unsigned long) tmp);

	return r;
}

/*****************************************************************************/

int ghost_read_file_krg_desc(ghost_t *ghost, void **desc)
{
       int r;
       int desc_size;

       r = ghost_read(ghost, &desc_size, sizeof (int));
       if (r)
               goto error;

       *desc = kmalloc(desc_size, GFP_KERNEL);
       if (!(*desc)) {
                 r = -ENOMEM;
                goto error;
       }

       r = ghost_read(ghost, *desc, desc_size);
       if (r) {
               kfree(*desc);
               *desc = NULL;
       }
error:
       return r;
}

int ghost_write_file_krg_desc(ghost_t *ghost, void *desc, int desc_size)
{
	int r;

	r = ghost_write (ghost, &desc_size, sizeof (int));
	if (r)
		goto error;

	r = ghost_write (ghost, desc, desc_size);
error:
	return r;
}

static int ghost_write_regular_file_krg_desc(ghost_t *ghost, struct file *file)
{
	int r;
	void *desc;
	int desc_size;

	r = get_regular_file_krg_desc(file, &desc, &desc_size);
	if (r)
		goto error;

	r = ghost_write_file_krg_desc(ghost, desc, desc_size);
	kfree (desc);
error:
	return r;
}

/*****************************************************************************/


static void cr_get_file_type_and_key(const struct file *file,
				     enum shared_obj_type *type,
				     long *key)
{
	if (file->f_flags & (O_FAF_CLT | O_FAF_SRV | O_KRG_SHARED)) {
		*type = REGULAR_DVFS_FILE;
		*key = file->f_objid;
	} else {
		*type = REGULAR_FILE;
		*key = (long)file;
	}
}

static int cr_add_file_to_shared_table(struct task_struct *task,
				       int index, struct file *file)
{
	int r;
	long key;
	enum shared_obj_type type;
	union export_args args;

	cr_get_file_type_and_key(file, &type, &key);

	args.file_args.index = index;
	args.file_args.file = file;

	r = add_to_shared_objects_list(&task->application->shared_objects,
				       type, key, 1 /*is_local*/, task,
				       &args);

	if (r == -ENOKEY) /* the file was already in the list */
               r = 0;

	return r;
}

static int cr_write_regular_file_id(ghost_t *ghost,
				    struct file *file)
{
	int r;
	long key;
	enum shared_obj_type type;

	cr_get_file_type_and_key(file, &type, &key);

  	r = ghost_write(ghost, &type, sizeof(enum shared_obj_type));
  	if (r)
  		goto error;

	r = ghost_write(ghost, &key, sizeof(long));
	if (r)
		goto error;

	if (type == REGULAR_DVFS_FILE)
		r = ghost_write_regular_file_krg_desc(ghost, file);

error:
	return r;
}

static int cr_link_to_local_regular_file(struct epm_action *action,
					 ghost_t *ghost,
					 struct task_struct *task,
					 struct file ** returned_file,
					 long key)
{
	int r = 0;

	/* look in the table to find the new allocated data
	 imported in import_shared_objects */

	*returned_file = get_imported_shared_object(
		&action->restart.app->shared_objects,
		REGULAR_FILE, key);

	if ( !(*returned_file)) {
		r = -E_CR_BADDATA;
		goto error;
	}

	get_file(*returned_file);

error:
	return r;
}

struct file *begin_import_dvfs_file(unsigned long dvfs_objid,
				    struct dvfs_file_struct **dvfs_file)
{
       struct file *file = NULL;

       /* Check if the file struct is already present */
       *dvfs_file = grab_dvfs_file_struct(dvfs_objid);
       file = (*dvfs_file)->file;
       if (file)
               get_file(file);

       return file;
}

int end_import_dvfs_file(unsigned long dvfs_objid,
			 struct dvfs_file_struct *dvfs_file,
			 struct file *file, int first_import)
{
       int r = 0;

       if (IS_ERR(file)) {
               r = PTR_ERR (file);
               goto error;
       }

       if (first_import) {
	       /* This is the first time the file is imported on this node
		* Setup the DVFS file field and inc the DVFS counter.
		*/
               file->f_objid = dvfs_objid;
               dvfs_file->file = file;

               dvfs_file->count++;
       }

error:
       put_dvfs_file_struct(dvfs_objid);
       return r;
}


static int cr_link_to_dvfs_regular_file(struct epm_action *action,
					ghost_t *ghost,
					struct task_struct *task,
					struct file ** returned_file,
					long key)
{
	int r = 0;
	unsigned long objid;
	struct dvfs_file_struct *dvfs_file = NULL;
	struct file *file = NULL;
	void *desc = NULL;
	int first_import = 0;

	objid = (long)get_imported_shared_object(
		&action->restart.app->shared_objects,
		REGULAR_DVFS_FILE, key);

	if (!objid) {
		r = -E_CR_BADDATA;
		goto exit_no_put;
	}

	/* anyway, read description from the ghost */
	r = ghost_read_file_krg_desc(ghost, &desc);
	if (r)
		goto exit_no_put;

	/* Check if the file struct is already present */
	file = begin_import_dvfs_file(objid, &dvfs_file);

	/* reopen the file if needed */
	if (!file) {
		file = import_regular_file_from_krg_desc(action, task, desc);
		first_import = 1;
	}

	r = end_import_dvfs_file(objid, dvfs_file, file, first_import);
	if (r)
		goto err_free_desc;

	check_flush_file(action, task->files, file);
	*returned_file = file;

err_free_desc:
	DEBUG("mobility", 2, 0, faf_srv_id(*returned_file),
	      faf_srv_fd(*returned_file), objid, task, file,
	      DVFS_LOG_IMPORT_INFO, *returned_file);

	kfree (desc);
exit_no_put:
	return r;
}

static int cr_link_to_regular_file(struct epm_action *action,
				   ghost_t *ghost,
				   struct task_struct *task,
				   struct file ** returned_file)
{
	int r;
 	long key;
	enum shared_obj_type type;

	BUG_ON(action->type != EPM_CHECKPOINT);
	BUG_ON(action->restart.shared != CR_LINK_ONLY);

	r = ghost_read(ghost, &type, sizeof(enum shared_obj_type));
	if (r)
		goto error;

	if (type != REGULAR_FILE && type != REGULAR_DVFS_FILE) {
		r = -E_CR_BADDATA;
		goto error;
	}

	r = ghost_read(ghost, &key, sizeof(long));
	if (r)
		goto error;

	switch (type) {
	case REGULAR_FILE:
		r = cr_link_to_local_regular_file(action, ghost, task,
						  returned_file, key);
		break;
	case REGULAR_DVFS_FILE:
		r = cr_link_to_dvfs_regular_file(action, ghost, task,
						 returned_file, key);
		break;
	default:
		BUG();
		break;
	}

error:
	return r;
}

/*****************************************************************************/
/*                                                                           */
/*                          REGULAR FILES IMPORT/EXPORT                      */
/*                                                                           */
/*****************************************************************************/



/** Export a regular file descriptor into the given ghost.
 *  @author Renaud Lottiaux
 *
 *  @param ghost      the ghost to write data to.
 *  @param file       The file to export.
 *
 *  @return   0 if everything ok.
 *            Negative value otherwise.
 */
int regular_file_export (struct epm_action *action,
			 ghost_t *ghost,
                         struct task_struct *task,
                         int index,
                         struct file *file)
{
	int r = 0;

	DEBUG("reg_file_mgr", 2, index, faf_srv_id(file), faf_srv_fd(file),
	      file->f_objid, NULL, file, DVFS_LOG_EXPORT_REG_FILE, 0);

	if (action->type == EPM_CHECKPOINT) {
		BUG_ON(action->checkpoint.shared == CR_SAVE_LATER);

		if (action->checkpoint.shared == CR_FILES_WRITEID) {
			r = cr_write_regular_file_id(ghost, file);
			return r;
		} else if (action->checkpoint.shared == CR_FILES_ADD) {
			r = cr_add_file_to_shared_table(task, index, file);
			return r;
		}
	}

	check_flush_file(action, task->files, file);

	r = ghost_write_regular_file_krg_desc(ghost, file);

	DEBUG("reg_file_mgr", 3, index, faf_srv_id(file), faf_srv_fd(file),
	      file->f_objid, NULL, file, DVFS_LOG_EXIT, r);

	return r;
}



/** Import a regular file descriptor from the given ghost.
 *  @author Renaud Lottiaux
 *
 *  @param ghost          The ghost to read data from.
 *  @param task           The task data are imported for.
 *  @param returned_file  The file struct where data should be imported to.
 *
 *  @return   0 if everything ok.
 *            Negative value otherwise.
 */
int regular_file_import (struct epm_action *action,
			 ghost_t *ghost,
                         struct task_struct *task,
                         struct file **returned_file)
{
	void *desc;
	struct file *file;
	int r = 0;

	DEBUG("reg_file_mgr", 2, -1, -1, -1, -1UL, NULL, NULL,
	      DVFS_LOG_ENTER, 0);

	if (action->type == EPM_CHECKPOINT
  	    && action->restart.shared == CR_LINK_ONLY) {
  		r = cr_link_to_regular_file(action, ghost, task,
 					    returned_file);
  		return r;
  	}

	r = ghost_read_file_krg_desc(ghost, &desc);
	if (r)
		goto exit;

	file = import_regular_file_from_krg_desc (action, task, desc);
	if (IS_ERR(file)) {
		r = PTR_ERR (file);
		goto exit_free_desc;
	}

	check_flush_file(action, task->files, file);
	*returned_file = file;
exit_free_desc:
	kfree (desc);
exit:
	return r;
}



struct dvfs_mobility_operations dvfs_mobility_regular_ops = {
	.file_export = regular_file_export,
	.file_import = regular_file_import,
};

static int cr_export_now_regular_file(struct epm_action *action,
				      ghost_t *ghost,
				      struct task_struct *task,
				      union export_args *args)
{
	int r;

	r = regular_file_export(action, ghost,
				task,
				args->file_args.index,
				args->file_args.file);
	return r;
}


static int cr_import_now_regular_file(struct epm_action *action,
				      ghost_t *ghost,
				      struct task_struct *fake,
				      void ** returned_data)
{
	int r;
	struct file *f;
	BUG_ON(*returned_data != NULL);

	r = regular_file_import(action, ghost, fake, &f);
	if (r)
		goto err;

	if (f->f_objid)
		/* this is a dvfs file */
		*returned_data = (void*)(f->f_objid);
	else
		*returned_data = f;
err:
	return r;
}

static int cr_import_complete_regular_file(struct task_struct *fake,
					   void *_f)
{
 	struct file *f = _f;

	BUG_ON(atomic_read(&f->f_count) <= 1);
	fput(f);

	return 0;
}

static int cr_delete_regular_file(struct task_struct *fake, void *_f)
{
	struct file *f = _f;

	BUG_ON(atomic_read(&f->f_count) <= 1);
	fput(f);

	return 0;
}

static int cr_import_complete_dvfs_regular_file(struct task_struct *fake,
						void *_f_objid)
{
	unsigned long dvfs_objid = (unsigned long)_f_objid;
	struct dvfs_file_struct *dvfs_file = NULL;
	struct file *f = NULL;

	dvfs_file = grab_dvfs_file_struct (dvfs_objid);
	f = dvfs_file->file;
	put_dvfs_file_struct (dvfs_objid);

	BUG_ON(atomic_read(&f->f_count) <= 1);
	if (f)
		fput(f);

	return 0;
}

static int cr_delete_dvfs_regular_file(struct task_struct *fake,
				       void *_f_objid)
{
	unsigned long dvfs_objid = (unsigned long)_f_objid;
	struct dvfs_file_struct *dvfs_file = NULL;
	struct file *f = NULL;

	dvfs_file = grab_dvfs_file_struct (dvfs_objid);
	f = dvfs_file->file;
	put_dvfs_file_struct (dvfs_objid);

	BUG_ON(atomic_read(&f->f_count) != 1);
	if (f)
		fput(f);

	return 0;
}

struct shared_object_operations cr_shared_regular_file_ops = {
        .restart_data_size = 0,
	.export_now        = cr_export_now_regular_file,
	.import_now        = cr_import_now_regular_file,
	.import_complete   = cr_import_complete_regular_file,
	.delete            = cr_delete_regular_file,
};

struct shared_object_operations cr_shared_dvfs_regular_file_ops = {
        .restart_data_size = 0,
        .export_now        = cr_export_now_regular_file,
	.import_now        = cr_import_now_regular_file,
	.import_complete   = cr_import_complete_dvfs_regular_file,
	.delete            = cr_delete_dvfs_regular_file,
};
