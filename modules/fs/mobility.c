/** Implementation of DFS mobility mechanisms.
 *  @file dfs_mobility.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 *
 *  Implementation of functions used to migrate, duplicate and checkpoint
 *  DFS data, process memory and file structures.
 */
#include "debug_fs.h"

#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/dcache.h>
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/mnt_namespace.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/rcupdate.h>
#include <kerrighed/fs.h>

#include <ctnr/kddm.h>
#include <ghost/ghost.h>
#include <epm/action.h>
#include <epm/application/app_shared.h>
#include "mobility.h"
#include "physical_fs.h"
#include "regular_file_mgr.h"
#include "file.h"
#include "file_struct_io_linker.h"
#include "faf/faf.h"
#include "faf/faf_file_mgr.h"
#include "faf/faf_hooks.h"


#define VM_FILE_NONE 0
#define VM_FILE_CTNR 1
#define VM_FILE_PHYS 2


struct dvfs_mobility_operations *dvfs_mobility_ops[MAX_DVFS_MOBILITY_OPS];



void free_ghost_files (struct task_struct *ghost)
{
	struct fdtable *fdt;

	DEBUG("mobility", 3, -1, -1, -1, -1UL, NULL, NULL, DVFS_LOG_ENTER, 0);

	BUG_ON (ghost->files == NULL);

	fdt = files_fdtable(ghost->files);

	BUG_ON (fdt->close_on_exec == NULL);
	BUG_ON (fdt->open_fds == NULL);

	exit_files(ghost);
	put_fs_struct(ghost->fs);

	DEBUG("mobility", 3, -1, -1, -1, -1UL, NULL, NULL, DVFS_LOG_EXIT, 0);
}



/*****************************************************************************/
/*                                                                           */
/*                              HELPER FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/



static inline int populate_fs_struct (ghost_t * ghost,
                                      char *buffer,
                                      struct dentry **dentry_place,
                                      struct vfsmount **mnt_place)
{
	struct nameidata nd;
	int len, r;

	r = ghost_read (ghost, &len, sizeof (int));
	if (r)
		goto err_read;

	if (len == 0) {
		*dentry_place = NULL;
		*mnt_place = NULL;
		return 0;
	}

	r = ghost_read (ghost, buffer, len);
	if (r)
		goto err_read;

	r = open_namei (AT_FDCWD, buffer, 0, 0, &nd);

	if (r == 0) {
		*dentry_place = dget (nd.dentry);
		*mnt_place = mntget (nd.mnt);
		put_filp(nd.intent.open.file);
		path_release(&nd);
	}
err_read:
	return r;
}



/*****************************************************************************/
/*                                                                           */
/*                              EXPORT FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/



/** Generic function to export an open file into a ghost.
 *  @author Renaud Lottiaux
 *
 *  @param ghost    Ghost where data should be stored.
 *  @param tsk      Task we are exporting.
 *  @parem index    Index of the exported file in the open files array.
 *  @param file     Struct of the file to export.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int export_one_open_file (struct epm_action *action,
			  ghost_t *ghost,
                          struct task_struct *tsk,
                          int index,
                          struct file *file)
{
	struct dvfs_mobility_operations *ops;
	unsigned short i_mode;
	int r;
	int write = 1;

	DEBUG("mobility", 2, index, faf_srv_id(file),faf_srv_fd(file),
	      file->f_objid, tsk, file, DVFS_LOG_ENTER_EXPORT_FILE,
	      epm_target_node(action));

	if (action->type != EPM_CHECKPOINT
	    && !file->f_objid)
		create_ctnr_file_object(file);

	if (action->type == EPM_CHECKPOINT &&
	    action->checkpoint.shared == CR_FILES_ADD)
		write = 0;

#ifdef CONFIG_KRG_FAF
	check_activate_faf (tsk, index, file, action);

	if (file->f_flags & (O_FAF_SRV | O_FAF_CLT))
		i_mode = S_IFAF;
	else
#endif
		i_mode = file->f_dentry->d_inode->i_mode & S_IFMT;

	check_file_struct_sharing (index, file, action);

	if (write) {
		r = ghost_write(ghost, &i_mode, sizeof (unsigned short));
		if (r)
			goto err_write;
		r = ghost_write(ghost, &file->f_objid,
				sizeof (unsigned long));
		if (r)
			goto err_write;
	}

	ops = get_dvfs_mobility_ops (i_mode);
	BUG_ON(ops == NULL);

	r = ops->file_export (action, ghost, tsk, index, file);

	DEBUG("mobility", 2, index, faf_srv_id(file),faf_srv_fd(file),
	      file->f_objid, tsk, file, DVFS_LOG_EXIT_EXPORT_FILE, 0);

err_write:
	return r;
}

static int get_file_size(struct file *file, loff_t *size)
{
	int r = 0;

	if (file->f_flags & O_FAF_CLT) {
		struct kstat stat;
		r = kh_faf_fstat(file, &stat);
		if (r)
			goto exit;

		*size = stat.size;
	} else
		*size = i_size_read(file->f_dentry->d_inode);

exit:
	return r;
}

int export_vma_phys_file(struct epm_action *action,
			 ghost_t *ghost,
			 struct task_struct *tsk,
			 struct vm_area_struct *vma)
{
	int r;

	r = export_one_open_file(action, ghost, tsk, -1,
				 vma->vm_file);

	if (r)
		goto exit;

	if (action->type == EPM_CHECKPOINT
	    && vma->vm_flags & VM_EXEC) {

		/* to check it is the same file when restarting */

		loff_t file_size;
		r = get_file_size(vma->vm_file, &file_size);
		if (r)
			goto exit;

		r = ghost_write(ghost, &file_size, sizeof(loff_t));
		if (r)
			goto exit;
	}
exit:
	return r;
}

/** Export the file associated to a VMA.
 *  @author Renaud Lottiaux
 *
 *  @param ghost    Ghost where data should be stored.
 *  @param vma      The VMA hosting the file to export.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int export_vma_file (struct epm_action *action,
		     ghost_t *ghost,
                     struct task_struct *tsk,
                     struct vm_area_struct *vma)
{
	int vm_file_type;
	int r;

	DEBUG("mobility", 2, -1, faf_srv_id(vma->vm_file),
	      faf_srv_fd(vma->vm_file), dvfs_objid(vma->vm_file), tsk,
	      vma->vm_file, DVFS_LOG_ENTER_EXPORT_VMA, vma);

	/* Creation of the vm_file ghost */

	if (vma->vm_file == NULL)
		vm_file_type = VM_FILE_NONE;
	else
		vm_file_type = VM_FILE_PHYS;

	r = ghost_write (ghost, &vm_file_type, sizeof (int));
	if (r)
		goto err_write;

	switch (vm_file_type) {
	  case VM_FILE_NONE:
		  break;

	  case VM_FILE_PHYS:
		  r = export_vma_phys_file(action, ghost, tsk, vma);
		  break;

 	  default:
		  BUG();
	}

	DEBUG("mobility", 2, -1, faf_srv_id(vma->vm_file),
	      faf_srv_fd(vma->vm_file), dvfs_objid(vma->vm_file),
	      tsk, vma->vm_file, DVFS_LOG_EXIT, 0);

err_write:
	return r;
}



/** Export the open files array of a process
 *  @author  Geoffroy Vallee, Renaud Lottiaux
 *
 *  @param ghost  Ghost where file data should be stored.
 *  @param tsk    Task to export file data from.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int export_open_files (struct epm_action *action,
		       ghost_t *ghost,
                       struct task_struct *tsk,
		       struct fdtable *fdt,
		       int last_open_fd)
{
	struct file *file;
	int i, r = 0;

	BUG_ON (!tsk);

	DEBUG("mobility", 3, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_ENTER, 0);

	if (action->type == EPM_CHECKPOINT) {
		if (action->checkpoint.shared == CR_SAVE_LATER)
			action->checkpoint.shared = CR_FILES_ADD;
		else if (action->checkpoint.shared == CR_SAVE_NOW)
			action->checkpoint.shared = CR_FILES_WRITEID;
	}

	/* Export files opened by the process */
	for (i = 0; i < last_open_fd; i++) {
		if (FD_ISSET (i, fdt->open_fds)) {
			BUG_ON (!fdt->fd[i]);
			file = fdt->fd[i];

			r = export_one_open_file (action, ghost, tsk, i, file);

			if (r != 0)
				goto exit;
		}
		else {
			if (fdt->fd[i] != NULL)
				printk ("Entry %d : %p\n", i, fdt->fd[i]);
			BUG_ON (fdt->fd[i] != NULL);
		}
	}

exit:
	if (action->type == EPM_CHECKPOINT) {
		if (action->checkpoint.shared == CR_FILES_ADD)
			action->checkpoint.shared = CR_SAVE_LATER;
		else if (action->checkpoint.shared == CR_FILES_WRITEID)
			action->checkpoint.shared = CR_SAVE_NOW;
	}

	DEBUG("mobility", 3, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_EXIT, r);

	return r;
}

static int cr_export_later_files_struct(struct epm_action *action,
					ghost_t *ghost,
					struct task_struct *task)
{
	int r;
	long key;
	int last_open_fd;
	struct fdtable *fdt;


	BUG_ON(action->type != EPM_CHECKPOINT);
	BUG_ON(action->checkpoint.shared != CR_SAVE_LATER);

	key = (long)(task->files);

	r = ghost_write(ghost, &key, sizeof(long));
	if (r)
		goto err;

	r = add_to_shared_objects_list(&task->application->shared_objects,
				       FILES_STRUCT, key, 1 /*is_local*/,
				       task, NULL);
	if (r)
		goto err_add;

	/* now we need to check the files to see if they are shared */
	rcu_read_lock();
	fdt = files_fdtable(task->files);

	last_open_fd = count_open_files(fdt);
	r = export_open_files (action, ghost, task, fdt, last_open_fd);
	rcu_read_unlock();

err_add:
	if (r == -ENOKEY)
		r = 0;
err:
	return r;
}

/** Export the files_struct of a process
 *  @author Renaud Lottiaux
 *
 *  @param ghost  Ghost where file data should be stored.
 *  @param tsk    Task to export file data from.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int export_files_struct (struct epm_action *action,
			 ghost_t *ghost,
                         struct task_struct *tsk)
{
	int r = 0, export_fdt;
	int last_open_fd;
	struct fdtable *fdt;

	BUG_ON (!tsk);

	DEBUG("mobility", 3, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_ENTER, 0);

	{
		int magic = 780574;

		r = ghost_write (ghost, &magic, sizeof (int));
		if (r)
			goto err;
	}

	if (action->type == EPM_CHECKPOINT &&
	    action->checkpoint.shared == CR_SAVE_LATER) {

		r = cr_export_later_files_struct(action, ghost, tsk);
		return r;
	}

	/* Export the main files structure */

	r = ghost_write (ghost, tsk->files, sizeof (struct files_struct));
	if (r)
		goto err;

	/* Export the bit vector close_on_exec */

	rcu_read_lock();
	fdt = files_fdtable(tsk->files);

	last_open_fd = count_open_files(fdt);
	r = ghost_write (ghost, &last_open_fd, sizeof (int));
	if (r)
		goto exit_unlock;

	export_fdt = (fdt != &tsk->files->fdtab);
	r = ghost_write (ghost, &export_fdt, sizeof (int));
	if (r)
		goto exit_unlock;

	if (export_fdt) {
		int nr = last_open_fd / BITS_PER_BYTE;
		r = ghost_write (ghost, fdt->close_on_exec, nr);
		if (r)
			goto exit_unlock;

		r = ghost_write (ghost, fdt->open_fds, nr);
		if (r)
			goto exit_unlock;

	}

	{
		int magic = 280574;

		r = ghost_write (ghost, &magic, sizeof (int));
		if (r)
			goto exit_unlock;
	}

	r = export_open_files (action, ghost, tsk, fdt, last_open_fd);
	if (r)
		goto exit_unlock;

	{
		int magic = 380574;

		r = ghost_write (ghost, &magic, sizeof (int));
	}

exit_unlock:
	rcu_read_unlock();

err:
	DEBUG("mobility", 3, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_EXIT, r);

	return r;
}


static int cr_export_later_fs_struct(struct epm_action *action,
				     ghost_t *ghost,
				     struct task_struct *task)
{
	int r;
	long key;

	BUG_ON(action->type != EPM_CHECKPOINT);
	BUG_ON(action->checkpoint.shared != CR_SAVE_LATER);

	key = (long)(task->fs);

	r = ghost_write(ghost, &key, sizeof(long));
	if (r)
		goto err;

	r = add_to_shared_objects_list(&task->application->shared_objects,
				       FS_STRUCT, key, 1 /*is_local*/, task,
				       NULL);

	if (r == -ENOKEY) /* the fs_struct was already in the list */
		r = 0;
err:
	return r;
}


/** Export the fs_struct of a process
 *  @author Renaud Lottiaux
 *
 *  @param ghost  Ghost where file data should be stored.
 *  @param tsk    Task to export file data from.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int export_fs_struct (struct epm_action *action,
		      ghost_t *ghost,
                      struct task_struct *tsk)
{
	char *tmp = (char *) __get_free_page (GFP_KERNEL), *file_name;
	int r, len;

	DEBUG("mobility", 3, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_ENTER, 0);

	if (action->type == EPM_CHECKPOINT &&
	    action->checkpoint.shared == CR_SAVE_LATER) {
		int r;
		r = cr_export_later_fs_struct(action, ghost, tsk);
		return r;
	}

	{
		int magic = 55611;

		r = ghost_write (ghost, &magic, sizeof (int));
		if (r)
			goto err_write;
	}

	/* Export the umask value */

	r = ghost_write (ghost, &tsk->fs->umask, sizeof (int));
	if (r)
			goto err_write;

	/* Export the root path name */

	file_name = physical_d_path (tsk->fs->root, tsk->fs->rootmnt, tmp);

	len = strlen (file_name) + 1;
	r = ghost_write (ghost, &len, sizeof (int));
	if (r)
			goto err_write;
	r = ghost_write (ghost, file_name, len);
	if (r)
			goto err_write;

	/* Export the pwd path name */

	file_name = physical_d_path (tsk->fs->pwd, tsk->fs->pwdmnt, tmp);

	len = strlen (file_name) + 1;
	r = ghost_write (ghost, &len, sizeof (int));
	if (r)
			goto err_write;
	r = ghost_write (ghost, file_name, len);
	if (r)
			goto err_write;

	/* Export the altroot path name */

	if (tsk->fs->altroot == NULL)
		len = 0;
	else {
		file_name = physical_d_path (tsk->fs->altroot,
					     tsk->fs->altrootmnt, tmp);
		len = strlen (file_name) + 1;
	}

	r = ghost_write (ghost, &len, sizeof (int));
	if (r)
			goto err_write;
	if (len != 0) {
		r = ghost_write (ghost, file_name, len);
		if (r)
			goto err_write;
	}

	free_page ((unsigned long) tmp);

	{
		int magic = 180574;

		r = ghost_write (ghost, &magic, sizeof (int));
	}

	DEBUG("mobility", 3, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_EXIT, 0);

err_write:
	return r;
}



int export_mnt_namespace(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *tsk)
{
	/* Nothing done right now... */
	return 0;
}



/*****************************************************************************/
/*                                                                           */
/*                              IMPORT FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/



/** Generic function to import an open file from a ghost.
 *  @author Renaud Lottiaux
 *
 *  @param ghost   Ghost where data should be read from.
 *  @param task    the task to import the file for.
 *  @param file    The resulting imported file structure.
 *
 *  @return   0 if everything ok.
 *            Negative value otherwise.
 */
int import_one_open_file (struct epm_action *action,
			  ghost_t *ghost,
                          struct task_struct *task,
			  int index,
                          struct file **returned_file)
{
	struct dvfs_file_struct *dvfs_file = NULL;
	struct dvfs_mobility_operations *ops;
	struct file *file = NULL;
	unsigned short i_mode;
	unsigned long objid;
	int first_import = 0;
	int r = 0;

	r = ghost_read(ghost, &i_mode, sizeof (unsigned short));
	if (r)
		goto err_read;
	r = ghost_read(ghost, &objid, sizeof (unsigned long));
	if (r)
		goto err_read;

	DEBUG("mobility", 2, index, -1, -1, objid, task, NULL,
	      DVFS_LOG_ENTER_IMPORT_FILE, 0);

	ops = get_dvfs_mobility_ops (i_mode);

	/* We need to import the file, to avoid leaving unused data in
	 * the ghost... We can probably do better...
	 */
	r = ops->file_import (action, ghost, task, returned_file);
	if (r || action->type == EPM_CHECKPOINT)
		goto exit;

	/* Check if the file struct is already present */
	file = begin_import_dvfs_file(objid, &dvfs_file);

	DEBUG("mobility", 2, index, faf_srv_id(*returned_file),
	      faf_srv_fd(*returned_file), objid, task, file,
	      DVFS_LOG_IMPORT_INFO, *returned_file);

	/* If a file struct was alreay present, use it and discard the one we
	 * have just created. If f_count == 0, someone else is being freeing
	 * the structure.
	 */
	if (file) {
		/* The file has already been imported on this node */
#ifdef CONFIG_KRG_FAF
		free_faf_file_private_data(*returned_file);
#endif
		fput(*returned_file);
		*returned_file = file;
	}
	else
		first_import = 1;

	r = end_import_dvfs_file(objid, dvfs_file, *returned_file, first_import);

exit:
	DEBUG("mobility", 2, index, r ? -1 : faf_srv_id(*returned_file),
	      r ? -1 : faf_srv_fd(*returned_file), objid, task,
	      r ? NULL : *returned_file, DVFS_LOG_EXIT_IMPORT_FILE, r);
err_read:
	return r;
}



/** Imports the open files of the process
 *  @author  Geoffroy Vallee, Renaud Lottiaux
 *
 *  @param ghost  Ghost where open files data are stored.
 *  @param tsk    Task to load open files data in.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int import_open_files (struct epm_action *action,
		       ghost_t *ghost,
                       struct task_struct *tsk,
		       struct files_struct *files,
		       struct fdtable *fdt,
		       int last_open_fd)
{
	int i, j, r = 0;

	DEBUG("mobility", 2, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_ENTER, 0);

	BUG_ON(action->type == EPM_CHECKPOINT
	       && action->restart.shared == CR_LINK_ONLY);

	if (action->type == EPM_CHECKPOINT)
		action->restart.shared = CR_LINK_ONLY;

	/* Reception of the files list and their names */

	for (i = 0; i < last_open_fd; i++) {
		if (FD_ISSET (i, fdt->open_fds)) {
			r = import_one_open_file (action, ghost, tsk, i,
						  (void *) &fdt->fd[i]);
			if (r != 0)
				goto err;
			BUG_ON (!fdt->fd[i]);
		}
		else
			fdt->fd[i] = NULL;
	}

	DEBUG("mobility", 2, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_EXIT, r);

exit:
	if (action->type == EPM_CHECKPOINT)
		action->restart.shared = CR_LOAD_NOW;

	return r;

err:
	for (j = 0; j < i; j++) {
		if (fdt->fd[j])
			filp_close(fdt->fd[j], files);
	}

	goto exit;
}

static int cr_link_to_files_struct(struct epm_action *action,
				   ghost_t *ghost,
				   struct task_struct *tsk)
{
	int r;
	long key;
	struct files_struct *files;

	r = ghost_read(ghost, &key, sizeof(long));
	if (r)
		goto err;

	files = get_imported_shared_object(&action->restart.app->shared_objects,
					   FILES_STRUCT, key);

	if (!files) {
		r = -E_CR_BADDATA;
		goto err;
	}

	/* the task is not yet hashed, no need to lock */
	atomic_inc(&files->count);
	tsk->files = files;
err:
	return r;
}


/** Imports the files informations of the process
 *  @author Renaud Lottiaux
 *
 *  @param ghost  Ghost where files data are stored.
 *  @param tsk    Task to load files data in.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int import_files_struct (struct epm_action *action,
			 ghost_t *ghost,
                         struct task_struct *tsk)
{
	int import_fdt;
	int last_open_fd;
	int r = -ENOMEM;
	struct files_struct *files;
	struct fdtable *fdt;

	{
		int magic = 0;

		r = ghost_read (ghost, &magic, sizeof (int));

		BUG_ON (!r && magic != 780574);
	}

	if (action->type == EPM_CHECKPOINT
	    && action->restart.shared == CR_LINK_ONLY) {
		r = cr_link_to_files_struct(action, ghost, tsk);
		return r;
	}

	DEBUG("mobility", 2, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_ENTER, 0);

	/* Import the main files structure */

	files = kmem_cache_alloc (files_cachep, GFP_KERNEL);
	if (files == NULL)
		return -ENOMEM;

	r = ghost_read (ghost, files, sizeof (struct files_struct));
	if (r)
		goto exit_free_files;

	atomic_set (&files->count, 1);
	spin_lock_init (&files->file_lock);

	r = ghost_read (ghost, &last_open_fd, sizeof (int));
	if (r)
		goto exit_free_files;
	r = ghost_read (ghost, &import_fdt, sizeof (int));
	if (r)
		goto exit_free_files;

	/* Import the open files table structure */

	if (import_fdt) {
		unsigned int cpy, set;

		fdt = alloc_fdtable(last_open_fd);
		if (fdt == NULL)
			goto exit_free_files;

		cpy = last_open_fd * sizeof(struct file *);
		set = (fdt->max_fds - last_open_fd) * sizeof(struct file *);
		memset((char *)(fdt->fd) + cpy, 0, set);

		cpy = last_open_fd / BITS_PER_BYTE;
		set = (fdt->max_fds - last_open_fd) / BITS_PER_BYTE;

		r = ghost_read (ghost, fdt->close_on_exec, cpy);
		if (r)
			goto exit_free_files;
		memset((char *)(fdt->close_on_exec) + cpy, 0, set);

		r = ghost_read (ghost, fdt->open_fds, cpy);
		if (r)
			goto exit_free_files;
		memset((char *)(fdt->open_fds) + cpy, 0, set);
	}
	else {
		fdt = &files->fdtab;
		INIT_RCU_HEAD(&fdt->rcu);
		fdt->next = NULL;
		fdt->close_on_exec = (fd_set *)&files->close_on_exec_init;
		fdt->open_fds = (fd_set *)&files->open_fds_init;
		fdt->fd = &files->fd_array[0];
	}

	rcu_assign_pointer(files->fdt, fdt);

	tsk->files = files;

	{
		int magic = 0;

		r = ghost_read (ghost, &magic, sizeof (int));

		BUG_ON (!r && magic != 280574);
	}

	r = import_open_files (action, ghost, tsk, files, fdt, last_open_fd);
	if (r)
		goto exit_free_fdt;

	{
		int magic = 0;

		r = ghost_read (ghost, &magic, sizeof (int));

		BUG_ON (!r && magic != 380574);
	}

	DEBUG("mobility", 2, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_EXIT, r);

	return 0;

exit_free_fdt:
	if (import_fdt)
		free_fdtable(fdt);

exit_free_files:
	kmem_cache_free(files_cachep, files);
	return r;
}


int import_vma_phys_file(struct epm_action *action,
			 ghost_t *ghost,
			 struct task_struct *tsk,
			 struct vm_area_struct *vma)
{
	int r;

	r = import_one_open_file(action, ghost, tsk, -1, &vma->vm_file);
	if (r)
		goto exit;

	if (action->type == EPM_CHECKPOINT
	    && vma->vm_flags & VM_EXEC) {

		/* to check it is the same file */

		loff_t old_file_size;
		loff_t current_file_size;

		r = ghost_read(ghost, &old_file_size, sizeof(loff_t));
		if (r)
			goto exit;

		r = get_file_size(vma->vm_file, &current_file_size);
		if (r)
			goto exit;

		if (old_file_size != current_file_size)
			r = -ENOEXEC;
	}
exit:
	return r;
}

/** Import the file associated to a VMA.
 *  @author Renaud Lottiaux
 *
 *  @param ghost    Ghost where data are be stored.
 *  @param tsk      The task to import VMA for.
 *  @param vma      The VMA to import the file in.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int import_vma_file (struct epm_action *action,
		     ghost_t *ghost,
                     struct task_struct *tsk,
                     struct vm_area_struct *vma)
{
	int vm_file_type;
	int r;

	DEBUG("mobility", 2, -1, -1, -1, -1UL, tsk, NULL,
	      DVFS_LOG_ENTER_IMPORT_VMA, vma);

	/* Import the file type flag */
	r = ghost_read (ghost, &vm_file_type, sizeof (int));
	if (r)
		goto err_read;

	switch (vm_file_type) {
	  case VM_FILE_NONE:
		  vma->vm_file = NULL;
		  break;

	  case VM_FILE_PHYS:
		  r = import_vma_phys_file(action, ghost, tsk, vma);
		  if (r)
			  goto err_read;
		  BUG_ON (!vma->vm_file);
		  break;

	  default:
		  BUG();
	}

	DEBUG("mobility", 2, -1, -1, -1, -1UL, tsk, vma->vm_file,
	      DVFS_LOG_EXIT, 0);

err_read:
	return r;
}



static int cr_link_to_fs_struct(struct epm_action *action,
				ghost_t *ghost,
				struct task_struct *tsk)
{
	int r;
	long key;
	struct fs_struct *fs;

	r = ghost_read(ghost, &key, sizeof(long));
	if (r)
		goto err;

	fs = get_imported_shared_object(&action->restart.app->shared_objects,
					FS_STRUCT, key);

	if (!fs) {
		r = -E_CR_BADDATA;
		goto err;
	}

	/* the task is not yet hashed, no need to lock */
	atomic_inc(&fs->count);
	tsk->fs = fs;
err:
	return r;
}


/** Import the fs_struct of a process
 *  @author Renaud Lottiaux
 *
 *  @param ghost  Ghost where file data are stored.
 *  @param tsk    Task to import file data in.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int import_fs_struct (struct epm_action *action,
		      ghost_t *ghost,
                      struct task_struct *tsk)
{
	struct fs_struct *fs;
	char *buffer = (char *) __get_free_page (GFP_KERNEL);
	int r;

	DEBUG("mobility", 2, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_ENTER, 0);

	if (action->type == EPM_CHECKPOINT
	    && action->restart.shared == CR_LINK_ONLY) {
		r = cr_link_to_fs_struct(action, ghost, tsk);
		return r;
	}

	{
		int magic = 0;

		r = ghost_read (ghost, &magic, sizeof (int));
		BUG_ON (!r && magic != 55611);
	}

	fs = kmem_cache_alloc (fs_cachep, GFP_KERNEL);
	if (fs == NULL)
		return -ENOMEM;

	atomic_set (&fs->count, 1);
	rwlock_init (&fs->lock);

	/* Import the umask value */

	r = ghost_read (ghost, &fs->umask, sizeof (int));
	if (r)
		goto exit_free_fs;

	/* Import the root path name */

	r = populate_fs_struct (ghost, buffer, &fs->root, &fs->rootmnt);
	if (r)
		goto exit_free_fs;

	/* Import the pwd path name */

	r = populate_fs_struct (ghost, buffer, &fs->pwd, &fs->pwdmnt);
	if (r)
		goto exit_put_root;

	/* Import the altroot path name */

	r = populate_fs_struct (ghost, buffer, &fs->altroot, &fs->altrootmnt);
	if (r)
		goto exit_put_pwd;

	{
		int magic = 0;

		r = ghost_read (ghost, &magic, sizeof (int));
		BUG_ON (!r && magic != 180574);
	}

	tsk->fs = fs;

exit:
	free_page ((unsigned long) buffer);

	DEBUG("mobility", 2, -1, -1, -1, -1UL, tsk, NULL, DVFS_LOG_EXIT, r);

	return r;

exit_put_pwd:
	dput(fs->pwd);
	mntput(fs->pwdmnt);

exit_put_root:
	dput(fs->root);
	mntput(fs->rootmnt);

exit_free_fs:
	kmem_cache_free (fs_cachep, fs);
	goto exit;
}



int import_mnt_namespace(struct epm_action *action,
			 ghost_t *ghost, struct task_struct *tsk)
{
	if (tsk->nsproxy->mnt_ns != NULL)
	{
		get_mnt_ns(tsk->nsproxy->mnt_ns);
		tsk->nsproxy->mnt_ns = current->nsproxy->mnt_ns;
	}

	return 0;
}



/*****************************************************************************/
/*                                                                           */
/*                            UNIMPORT FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/



void unimport_mnt_namespace(struct task_struct *tsk)
{
	exit_mnt_ns(tsk);
}



void unimport_files_struct(struct task_struct *tsk)
{
	exit_files(tsk);
}



void unimport_fs_struct(struct task_struct *tsk)
{
	exit_fs(tsk);
}



/*****************************************************************************/
/*                                                                           */
/*                              INITIALIZATION                               */
/*                                                                           */
/*****************************************************************************/



int dvfs_mobility_init(void)
{
	int i;

	for (i = 0; i < MAX_DVFS_MOBILITY_OPS; i++)
		dvfs_mobility_ops[i] = NULL;

#ifdef CONFIG_KRG_FAF
	register_dvfs_mobility_ops (S_IFAF, &dvfs_mobility_faf_ops);
	register_dvfs_mobility_ops (S_IFIFO, &dvfs_mobility_faf_ops);
	register_dvfs_mobility_ops (S_IFSOCK, &dvfs_mobility_faf_ops);
	register_dvfs_mobility_ops (S_IFBLK, &dvfs_mobility_faf_ops);
#endif
	register_dvfs_mobility_ops (S_IFREG, &dvfs_mobility_regular_ops);
	register_dvfs_mobility_ops (S_IFDIR, &dvfs_mobility_regular_ops);
	register_dvfs_mobility_ops (S_IFCHR, &dvfs_mobility_regular_ops);

	return 0;
}



void dvfs_mobility_finalize (void)
{
}


static int cr_export_now_files_struct(struct epm_action *action, ghost_t *ghost,
				      struct task_struct *task,
				      union export_args *args)
{
	int r;
	r = export_files_struct(action, ghost, task);
	return r;
}


static int cr_import_now_files_struct(struct epm_action *action, ghost_t *ghost,
				      struct task_struct *fake,
				      void ** returned_data)
{
	int r;
	BUG_ON(*returned_data != NULL);

	r = import_files_struct(action, ghost, fake);
	if (r)
		goto err;

	*returned_data = fake->files;
err:
	return r;
}

static int cr_import_complete_files_struct(struct task_struct *fake,
					   void *_files)
{
	struct files_struct *files = _files;

	fake->files = files;
	exit_files(fake);

	return 0;
}

static int cr_delete_files_struct(struct task_struct *fake, void *_files)
{
	struct files_struct *files = _files;

	fake->files = files;
	exit_files(fake);

	return 0;
}

struct shared_object_operations cr_shared_files_struct_ops = {
        .restart_data_size = 0,
        .export_now        = cr_export_now_files_struct,
	.import_now        = cr_import_now_files_struct,
	.import_complete   = cr_import_complete_files_struct,
	.delete            = cr_delete_files_struct,
};

static int cr_export_now_fs_struct(struct epm_action *action, ghost_t *ghost,
				   struct task_struct *task,
				   union export_args *args)
{
	int r;
	r = export_fs_struct(action, ghost, task);
	return r;
}


static int cr_import_now_fs_struct(struct epm_action *action, ghost_t *ghost,
				   struct task_struct *fake,
				   void ** returned_data)
{
	int r;
	BUG_ON(*returned_data != NULL);

	r = import_fs_struct(action, ghost, fake);
	if (r)
		goto err;

	*returned_data = fake->fs;
err:
	return r;
}

static int cr_import_complete_fs_struct(struct task_struct *fake, void *_fs)
{
	struct fs_struct *fs = _fs;

	fake->fs = fs;
	exit_fs(fake);

	return 0;
}

static int cr_delete_fs_struct(struct task_struct *fake, void *_fs)
{
	struct fs_struct *fs = _fs;

	fake->fs = fs;
	exit_fs(fake);

	return 0;
}

struct shared_object_operations cr_shared_fs_struct_ops = {
        .restart_data_size = 0,
        .export_now        = cr_export_now_fs_struct,
	.import_now        = cr_import_now_fs_struct,
	.import_complete   = cr_import_complete_fs_struct,
	.delete            = cr_delete_fs_struct,
};
