/** File Ghost interface.
 *  @file file_ghost.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#define MODULE_NAME "FILE GHOST"
#include "debug_ghost.h"

#ifndef FILE_NONE
#  if defined(FILE_FILE_GHOST) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

#include <asm/uaccess.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/slab.h>
#include <linux/string.h>
#include "ghost.h"
#include "file_ghost.h"

/*--------------------------------------------------------------------------*
 *                                                                          *
 *  Functions to implement ghost interface                                  *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/** Read data from a file ghost.
 *  @author Renaud Lottiaux, Geoffroy Vallée
 *
 *  @param  ghost   Ghost to read data from.
 *  @param  buff    Buffer to store data.
 *  @param  length  Size of data to read.
 *
 *  @return        0 if everything ok
 *                 Negative value otherwise.
 */
int file_ghost_read(ghost_t *ghost, void *buff, size_t length)
{
	file_ghost_data_t *ghost_data ;
	struct file *file = NULL;
	int r = 0 ;

	BUG_ON(!ghost);
	BUG_ON(!buff);

	ghost_data = (file_ghost_data_t *)ghost->data;

	file = ghost_data->file;
	BUG_ON(!file);

	DEBUG(DEBUG_FILE_GHOST, 4, "Read ghost data from file %s\n",
	       file->f_dentry->d_name.name);

	r = file->f_op->read(file, (char*)buff, length, &file->f_pos);

	if (r == length)
		r = 0;
	else if (r >=0)
		r = -EFAULT;

	DEBUG(DEBUG_FILE_GHOST, 4, "Read done with error %d\n", r);
	return r ;
}



/** Write data to a file ghost.
 *  @author Renaud Lottiaux, Geoffroy Vallée
 *
 *  @param  ghost   Ghost to write data to.
 *  @param  buff    Buffer to write in the ghost.
 *  @param  length  Size of data to write.
 *
 *  @return        0 if everything ok
 *                 Negative value otherwise.
 */
int file_ghost_write(struct ghost *ghost, const void *buff, size_t length)
{
	file_ghost_data_t *ghost_data ;
	struct file *file = NULL;
	int r = 0 ;

	BUG_ON(!ghost);
	BUG_ON(!buff);

	ghost_data = (file_ghost_data_t *)ghost->data;
	BUG_ON(!ghost_data);

	file = ghost_data->file;
	BUG_ON(!file);

	DEBUG(DEBUG_FILE_GHOST, 4, "Write ghost data on file %s\n",
	       file->f_dentry->d_name.name);

	r = file->f_op->write(file, (char*)buff, length, &file->f_pos);

	if (r == length)
		r = 0;
	else if ( r >=0 )
		r = -EFAULT;

	DEBUG(DEBUG_FILE_GHOST, 4, "Write done with error %d\n", r);

	return r ;
}


/** Close a ghost file
 *  @author Matthieu Fertré
 *
 *  @param  ghost    Ghost file to close
 */
int file_ghost_close(ghost_t *ghost)
{
	if (ghost->access & GHOST_WRITE) {
#if 0
		do_fdatasync (ghost->data->file);
#else
		int r = ((file_ghost_data_t *)ghost->data)->file->f_op->
			fsync(((file_ghost_data_t *)ghost->data)->file,
			      ((file_ghost_data_t *)ghost->data)->file
			      ->f_dentry, 1);
		if (r)
			WARNING("Something wrong in the sync : %d\n", r);
#endif
	}

	if (((file_ghost_data_t *)ghost->data)->from_fd)
		fput(((file_ghost_data_t *)ghost->data)->file);
	else
		filp_close(((file_ghost_data_t *)ghost->data)->file,
			   current->files);

	return free_ghost(ghost);
}




/** File ghost operations
 */
struct ghost_operations ghost_file_ops = {
read  : &file_ghost_read,
write : &file_ghost_write,
close : &file_ghost_close
};


ghost_fs_t set_ghost_fs(uid_t uid, gid_t gid)
{
	ghost_fs_t oldfs;

	oldfs.fs = get_fs();
	oldfs.uid = current->fsuid;
	oldfs.gid = current->fsgid;

	/* Enable the process to read in kernel memory */
	set_fs(KERNEL_DS);
	current->fsuid = uid;
	current->fsgid = gid;

	return oldfs;
}

void unset_ghost_fs(ghost_fs_t *oldfs)
{
	set_fs(oldfs->fs);
	current->fsuid = oldfs->uid;
	current->fsgid = oldfs->gid;
}

/*--------------------------------------------------------------------------*
 *                                                                          *
 * Macros and functions used to manage file ghost creation                  *
 *                                                                          *
 *--------------------------------------------------------------------------*/

// Path where file ghost are saved
#define CHECKPOINT_PATH "/var/chkpt/"
#define CHECKPOINT_PATH_LENGTH 45

char checkpointRoot[CHECKPOINT_PATH_LENGTH] = CHECKPOINT_PATH;

char * get_chkpt_dir(long app_id,
		     unsigned int chkpt_sn)
{
	char *buff;
	char *dirname;

	buff = kmalloc(MAX_LENGHT_STRING*sizeof(char), GFP_KERNEL);
	dirname = kmalloc(MAX_LENGHT_STRING*sizeof(char), GFP_KERNEL);

	sprintf(dirname, "%s", checkpointRoot);

	if (app_id) {
		sprintf(buff,"%ld/", app_id);
		strcat(dirname, buff);
	}

	if (chkpt_sn) {
		sprintf(buff, "v%d/", chkpt_sn);
		strcat(dirname, buff);
	}

	kfree(buff);
	return dirname;
}


/** Returns a string with the name of the file
 *  The caller's responsibility to free it.
 *  checkPointRoot/AppId/vSN/prefixOBJECT_ID
 */
char * get_chkpt_filebase(long app_id,
			  unsigned int chkpt_sn,
			  int obj_id,
			  const char *obj_prefix)
{
	char *buff;
	char *filename;

	buff = kmalloc(MAX_LENGHT_STRING*sizeof(char), GFP_KERNEL);
	filename = kmalloc(MAX_LENGHT_STRING*sizeof(char), GFP_KERNEL);

	sprintf(filename, "%s", checkpointRoot);

	if (app_id) {
		sprintf(buff,"%ld/", app_id);
		strcat(filename, buff);
	}

	if (chkpt_sn) {
		sprintf(buff, "v%d/", chkpt_sn);
		strcat(filename, buff);
	}

	BUG_ON(!obj_prefix);

	strcat(filename, obj_prefix);

	if (obj_id != -1) {
		sprintf(buff,"_%d", obj_id);
                strcat(filename, buff);
	}

	strcat(filename, ".bin");

	kfree (buff);
	return filename;
}

int mkdir_chkpt_path(long app_id, unsigned int chkpt_sn)
{
	char *buff;
	char *dir_name;
	int r;

	buff = kmalloc(MAX_LENGHT_STRING*sizeof(char), GFP_KERNEL);
	dir_name = kmalloc(MAX_LENGHT_STRING*sizeof(char), GFP_KERNEL);

	sprintf(dir_name, "%s", checkpointRoot);

	if (app_id) {
		sprintf(buff,"%ld/", app_id);
		strcat(dir_name, buff);
	}

	r = sys_mkdir(dir_name, S_IRWXUGO|S_ISVTX);
	if (r && r != -EEXIST)
		goto err;

	/* really force the mode without looking at umask */
	r = sys_chmod(dir_name, S_IRWXUGO|S_ISVTX);
	if (r)
		goto err;

	if (chkpt_sn) {
		sprintf(buff, "v%d/", chkpt_sn);
		strcat(dir_name, buff);

		r = sys_mkdir(dir_name, S_IRWXU);
		if (r && r != -EEXIST)
			goto err;
		r = 0;
	}

err:
	kfree(buff);
	kfree(dir_name);

	return r;
}

/** Create a new file ghost.
 *  @author Renaud Lottiaux, Matthieu Fertré
 *
 *  @param  access   Ghost access (READ/WRITE)
 *  @param  file     File to read/write data to/from.
 *
 *  @return        ghost_t if everything ok
 *                 ERR_PTR otherwise.
 */
ghost_t *create_file_ghost(int access,
			   long app_id,
			   unsigned int chkpt_sn,
			   int obj_id,
			   const char *label)
{
	struct file *file ;
	char *file_name;

	file_ghost_data_t *ghost_data ;
	ghost_t *ghost;

	int r;

	/* A file ghost can only be used in uni-directional mode */
	BUG_ON(!(!(access & GHOST_READ) ^ !(access & GHOST_WRITE)));

	/* Create directory if not exist */
	if (access & GHOST_WRITE) {
		r = mkdir_chkpt_path(app_id, chkpt_sn);
		if (r)
			goto err;
	}

	/* Create a ghost to host the checkoint */
	file_name = get_chkpt_filebase(app_id, chkpt_sn, obj_id, label);
	if (IS_ERR(file_name)) {
		r = PTR_ERR(file_name);
		goto err;
	}

	DEBUG(DEBUG_FILE_GHOST,2, "Ghost file : -%s-\n", file_name);

	if (access & GHOST_WRITE)/* fail if already exists */
		file = filp_open(file_name, O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
	else
		file = filp_open(file_name, O_RDONLY, S_IRWXU);

	kfree(file_name);

	if (IS_ERR(file)) {
		r = PTR_ERR(file);
		goto err;
	}

	DEBUG(DEBUG_FILE_GHOST,2, "Ghost file successfully created\n");

	/* Create a ghost to host the checkoint */
	ghost = create_ghost(GHOST_FILE, access);
	if (IS_ERR(ghost)) {
		r = PTR_ERR(ghost);
		goto err_file;
	}

	ghost_data = kmalloc(sizeof(file_ghost_data_t), GFP_KERNEL);
	if (!ghost_data) {
		r = -ENOMEM;
		goto err_ghost;
	}


	ghost_data->file = file;
	ghost_data->from_fd = 0;

	ghost->data = ghost_data;
	ghost->ops = &ghost_file_ops;

	return ghost;

err_ghost:
	free_ghost(ghost);
err_file:
	filp_close(file, current->files);
err:
	return ERR_PTR(r);
}

/** Create a new file ghost.
 *  @author Matthieu Fertré
 *
 *  @param  access   Ghost access (READ/WRITE)
 *  @param  file     File descriptor to read/write data to/from.
 *
 *  @return        ghost_t if everything ok
 *                 ERR_PTR otherwise.
 */
ghost_t *create_file_ghost_from_fd(int access, unsigned int fd)
{
	struct file *file ;

	file_ghost_data_t *ghost_data ;
	ghost_t *ghost;

	int r;

	/* A file ghost can only be used in uni-directional mode */
	BUG_ON(!(!(access & GHOST_READ) ^ !(access & GHOST_WRITE)));

	file = fget(fd);
	if (!file) {
		r = -EBADF;
		goto err;
	}

	/* check the access right */
	if ((access & GHOST_WRITE && !(file->f_mode & FMODE_WRITE))
	    || (access & GHOST_READ && !(file->f_mode & FMODE_READ))) {
		r = -EACCES;
		goto err_file;
	}

	/* Create a ghost to host the checkoint */
	ghost = create_ghost(GHOST_FILE, access);
	if (IS_ERR(ghost)) {
		r = PTR_ERR(ghost);
		goto err_file;
	}

	ghost_data = kmalloc(sizeof(file_ghost_data_t), GFP_KERNEL);
	if (!ghost_data) {
		r = -ENOMEM;
		goto err_ghost;
	}

	ghost_data->file = file;
	ghost_data->from_fd = 1;

	ghost->data = ghost_data;
	ghost->ops = &ghost_file_ops;

	return ghost;

err_ghost:
	free_ghost(ghost);
err_file:
	fput(file);
err:
	return ERR_PTR(r);
}

