/** Container file IO linker.
 *  @file file_io_linker.c
 *  
 *  @author Renaud Lottiaux 
 */

#include <linux/slab.h>
#include <linux/file.h>
#include <linux/pagemap.h>

#include "debug_kerfs.h"

#define MODULE_NAME "File IO linker  "

#ifdef FILE_IO_LINKER_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <tools/kerrighed.h>
#include <ctnr/container_mgr.h>
#include <ctnr/container.h>
#include <ctnr/io_linker.h>
#include "file_io_linker.h"
#include <ctnr/object_server.h>
#include "physical_fs.h"
#include "dir.h"
#include "inode.h"
#include "dfs_server.h"
#include "super.h"
#include <ctnr/protocol_action.h>



/*****************************************************************************/
/*                                                                           */
/*                          FILE CONTAINER CREATION                          */
/*                                                                           */
/*****************************************************************************/



/** Instantiate a container with a file linker.
 *  @author Renaud Lottiaux
 *
 *  @param ctnr          Container to instantiate
 *  @param private_data  File to link to the container.
 *
 *  @return error code or 0 if everything was ok.
 */
int file_instantiate (container_t * ctnr,
                      void *private_data,
                      int master)
{
	file_iolinker_data_t *file_data;
	struct inode *inode = NULL;
	struct file *physical_file;
	int physical_mode;
	int r = 0;

	file_data = kmalloc(sizeof (file_iolinker_data_t), GFP_KERNEL);
	ASSERT(file_data != NULL);

	memcpy(file_data, private_data, sizeof (file_iolinker_data_t));

	DEBUG(2, "Instantiate ctnr %ld with file : %s\n", ctnr->ctnrid,
	      file_data->file_name);

	physical_mode = (file_data->mode & ~S_IFMT) | S_IFREG;

	/* Create logical inode and mapping hosting the container pages */

	file_data->logical_inode = kerfs_get_inode(kerfs_sb, file_data->mode,
						   ctnr);

	/* Create link with the physical file */

	if (ctnr->linked_node == kerrighed_node_id) {
		DEBUG(3, "Try to open physical file : %s\n",
		      file_data->file_name);

		physical_file = open_physical_file(file_data->file_name,
						   O_RDONLY | file_data->flags,
						   physical_mode,
						   file_data->fsuid,
						   file_data->fsgid);

		if (IS_ERR(physical_file))
			goto exit_err;

		file_data->physical_file = physical_file;
		inode = physical_file->f_dentry->d_inode;
		inode->i_objid = ctnr->ctnrid;
		kerfs_init_krg_inode_struct(ctnr->ctnrid, inode);
	} else {
		DEBUG(4, "Nothing to do to instantiate ctnr %ld to file %s\n",
		      ctnr->ctnrid, file_data->file_name);

		physical_file = NULL;
	}

	file_data->physical_file = physical_file;

	increment_container_usage(ctnr);

	if (inode) {
		if (!(file_data->logical_inode->i_state & I_DIRTY)
		    && (inode->i_state & I_DIRTY))
			__mark_inode_dirty(file_data->logical_inode,
					   inode->i_state & I_DIRTY);

		kerfs_copy_inode(file_data->logical_inode, inode);

	}

	ctnr->iolinker_data = file_data;

	DEBUG(2, "Instantiate ctnr %ld with file : %s done with err %d\n",
	      ctnr->ctnrid, file_data->file_name, r);

	return r;

 exit_err:
	DEBUG(2, "Instantiate ctnr %ld with file : %s done with err %ld\n",
	      ctnr->ctnrid, file_data->file_name, PTR_ERR(physical_file));

	kfree(file_data);

	return PTR_ERR(physical_file);
}



/** Uninstantiate a file container.
 *  @author Renaud Lottiaux
 *
 *  @param ctnr          Container to uninstantiate
 */
void file_uninstantiate(container_t * ctnr, int destroy)
{
	file_iolinker_data_t *file_data;

	file_data = (file_iolinker_data_t *) ctnr->iolinker_data;

	if (file_data == NULL)
		return;

	DEBUG(2, "Uninstantiate ctnr %ld\n", ctnr->ctnrid);

	/* Close / destroy physical file */

	if (file_data->physical_file) {
		if (destroy)
			remove_physical_file(file_data->physical_file);
		else
			close_physical_file(file_data->physical_file);
	}

	/* Put logical inode */

	if (file_data->logical_inode) {
		d_prune_aliases(file_data->logical_inode);

		if (atomic_read(&file_data->logical_inode->i_count) != 0)
			iput(file_data->logical_inode);

		file_data->logical_inode->i_mapping->a_ctnr = NULL;
	}

	kfree(file_data);
	ctnr->iolinker_data = NULL;

	DEBUG(2, "Uninstantiate ctnr %ld : done\n", ctnr->ctnrid);
}


/** Create a file container from an existing file.
 *  @author Renaud Lottiaux
 *
 *  @param dentry        dentry of the file to create a container for.
 *  @param mnt           Mount point of the dentry.
 *  @param in_kerfs      True if the dentry is in the kerfs tree.
 *  @param linked_node   Node hosting the file.
 *  @param flags         Flags to open the file with.
 *  @param mode          Mode used to create the file.
 *  @param uid, gid      Uid and Gid of the file owner.
 *
 *  @return The newly created container.
 */
container_t *create_file_container(struct dentry *dentry,
				   struct vfsmount *mnt,
				   int in_kerfs,
				   kerrighed_node_t linked_node,
				   int flags,
				   int mode,
				   uid_t uid,
				   gid_t gid)
{
	char *tmp = (char *) __get_free_page(GFP_KERNEL), *file_name;
	file_iolinker_data_t *file_data;
	container_t *ctnr = NULL;

	DEBUG(2, "BEGIN\n");

	/* Allocate space for the file_data structure */
	file_data = kmalloc(sizeof (file_iolinker_data_t), GFP_KERNEL);
	ASSERT(file_data != NULL);

	/* Let's find the name of the file ... */
	if (in_kerfs)
		file_name = d_path_from_kerfs(dentry, mnt, tmp, PAGE_SIZE);
	else
		file_name = physical_d_path(dentry, mnt, tmp);

	if (!file_name)
		goto exit;

	/* ... and copy it into file_data */
	strncpy(file_data->file_name, file_name, 1024);

	/* Enpack the directory IO linker data */

	file_data->mode = mode;
	file_data->flags = flags;
	file_data->size = 0;
	file_data->fsuid = uid;
	file_data->fsgid = gid;

	/* file_data is now correctly filled in */
	/* We create the container */
	ctnr = create_new_container(FILE_LINKER, linked_node, PAGE_SIZE,
				    file_data, sizeof(file_iolinker_data_t), 0);

	/* We can free all the memory allocated */
	kfree(file_data);

 exit:
	free_page((unsigned long) tmp);
	return ctnr;
}

container_t *create_container_from_file(struct file * file)
{
	return create_file_container(file->f_dentry, file->f_vfsmnt, 0,
				     kerrighed_node_id,
				     file->f_flags,
				     file->f_dentry->d_inode->i_mode,
				     file->f_uid, file->f_gid);
}


/*****************************************************************************/
/*                                                                           */
/*                        FILE CONTAINER IO FUNCTIONS                        */
/*                                                                           */
/*****************************************************************************/

void add_ctnr_page_in_cache(struct page *page, container_t * ctnr);

/** Allocate an object
 *  @author Renaud Lottiaux
 */
void *file_alloc_object(ctnrObj_t * objEntry, container_t * ctnr,
			objid_t objid)
{
	void *addr;
	addr = alloc_page(GFP_KERNEL);
	return addr;
}

/** Import an object
 *  @author Renaud Lottiaux
 *
 *  @param  object    The object to import data in.
 *  @param  buffer    Data to import in the object.
 */
int file_import_object(ctnrObj_t * objEntry, char *buffer,
		       struct gimli_desc *desc)
{
	copy_buff_to_highpage((struct page *) objEntry->object, buffer);
	return 0;
}

/** Export an object
 *  @author Renaud Lottiaux
 *
 *  @param  buffer    Buffer to export object data in.
 *  @param  object    The object to export data from.
 */
int file_export_object(char *buffer, struct gimli_desc *desc,
		       ctnrObj_t * objEntry)
{
	copy_highpage_to_buff(buffer, (struct page *) objEntry->object);
	return 0;
}

/** First access to a container file page.
 *  @author Renaud Lottiaux
 *
 *  @param  ctnr       Container descriptor
 *  @param  objid     Id of the page to create.
 *  @param  objEntry  Container page descriptor.
 *
 *  @return  0 if everything is ok. Negative value otherwise.
 */
int file_first_touch(ctnrObj_t * objEntry, container_t * ctnr, objid_t objid)
{
	file_iolinker_data_t *file_data = ctnr->iolinker_data;
	struct address_space *phys_mapping;
	struct page *phys_page, *ctnr_page;
	struct file *phys_file;
	int res;

	ASSERT(file_data != NULL);
	phys_file = file_data->physical_file;
	ASSERT(phys_file != NULL);
	phys_mapping = phys_file->f_dentry->d_inode->i_mapping;
	ASSERT(phys_mapping != NULL);

	DEBUG(3, "First touch file page (%ld;%ld) from %s (size %d)\n",
	      ctnr->ctnrid, objid, phys_file->f_dentry->d_name.name,
	      (int) phys_file->f_dentry->d_inode->i_size);

	ctnr_page = ctnr_io_alloc_object(objEntry, ctnr, objid);

	if (ctnr_page == NULL)
		return -ENOMEM;

	DEBUG (3, "Page (%ld;%ld) will be stored in ctnr at %p (@ %p)\n",
	       ctnr->ctnrid, objid, ctnr_page, page_address(ctnr_page));

	ctnr_page->index = objid;

	/* Insert the page read from physical file in the "logical file" cache */

	add_ctnr_page_in_cache(ctnr_page, ctnr);

	set_object_frozen(objEntry, ctnr); /* Pit the object in local memory */

 retry:
	res = page_cache_read(phys_file, objid);
	if (res < 0)
		return res;

	DEBUG(3, "Page (%ld;%ld) read from mapping %p with error %d\n",
	      ctnr->ctnrid, objid, phys_mapping, res);

	phys_page = find_get_page(phys_mapping, objid);
	if (!phys_page)	{
		WARNING ("Hum... Cannot find the page I've just read....\n");
		goto retry;
	}

	if (!PageUptodate(phys_page)) {
		DEBUG(3, "Wait for page (%ld;%ld) read to complete\n",
		      ctnr->ctnrid, objid);
		wait_on_page_locked(phys_page);
	}

	DEBUG(3, "Copying phys page (%ld;%ld) in container page\n",
	      ctnr->ctnrid, objid);

	DEBUG(3, "Found phys (%ld;%ld) at %p\n", ctnr->ctnrid, objid, ctnr_page);

	copy_highpage(ctnr_page, phys_page);

	/* Insert the page read from physical file in the "logical file" cache */

	if (PageLocked (ctnr_page))
		ClearPageLocked(ctnr_page);

	object_clear_frozen(objEntry, ctnr);
	SetPageUptodate(ctnr_page);

	put_page(phys_page);

	DEBUG(3, "First touch file page (%ld;%ld) : done (count = %d)\n",
	      ctnr->ctnrid, objid, page_count(ctnr_page));

	return 0;
}



/** Invalidate a container file page.
 *  @author Renaud Lottiaux
 *
 *  @param  ctnr      Container descriptor
 *  @param  padeid    Id of the page to invalidate
 */
int file_invalidate_page(ctnrObj_t * objEntry, container_t * ctnr,
                          objid_t objid)
{
	file_iolinker_data_t *file_data = ctnr->iolinker_data;
	//  dsm_state_t old_obj_state;
	struct address_space *logical_mapping;
	struct page *page;
	int res = 0;

	ASSERT(file_data != NULL);
	logical_mapping = file_data->logical_inode->i_mapping;
	ASSERT(logical_mapping != NULL);

	DEBUG(3, "Invalidate page (%ld;%ld)\n", ctnr->ctnrid, objid);

	if (objEntry->object != NULL) {
		page = objEntry->object;

		DEBUG(3, "Page (%ld;%ld) (count = %d)\n", ctnr->ctnrid, objid,
		      page_count (page));

		if (page->mapping != NULL) {
			set_object_frozen(objEntry, ctnr);
			atomic_inc(&page->_count);
			res = TestSetPageLocked(page);
			ASSERT(res == 0); /* Cannot invalidate locked pages */

			clear_page_dirty(page);
			printk("2) Page (%ld;%ld) locked : %d, count = %d\n",
			       ctnr->ctnrid, objid, PageLocked(page),
			       page_count(page));

			if (!invalidate_complete_page(logical_mapping, page))
				WARNING("Page (%ld;%ld) badly removed from file cache - "
					"mapping %p/%p - Page dirty %d, Private %d\n",
					ctnr->ctnrid, objid,
					logical_mapping, page->mapping,
					PageDirty(page), PagePrivate(page));

			printk("3) Page (%ld;%ld) locked : %d\n", ctnr->ctnrid, objid,
			       PageLocked(page));

			unlock_page(page);
			object_clear_frozen(objEntry, ctnr);
			page_cache_release(page);
		}
		DEBUG(3, "Invalidate page (%ld;%ld) : done (count = %d)\n",
		      ctnr->ctnrid, objid, page_count (page));
	}
	return res;
}

/** Handle a container file page free
 *  @author Renaud Lottiaux
 *
 *  @param  ctnr      Container descriptor
 *  @param  padeid    Id of the page to flush
 */
int file_remove_page(ctnrObj_t * objEntry, container_t * ctnr, objid_t objid)
{
	struct page *page;

	DEBUG(3, "Remove page (%ld;%ld)\n", ctnr->ctnrid, objid);

	if (objEntry->object != NULL) {
		page = objEntry->object;

		if (PageLocked(page)) {
			/* If the page is removed from the kernel (through
			 * mapping->a_ops->removepage) we have nothing to do, the page will
			 * be removed from cache by the kernel. */
			if (page_count (page) != 2)
				printk("Arglh : file_remove_page - page (%ld;%ld) has count %d\n",
				       ctnr->ctnrid, objid, page_count (page));
			objEntry->countx = 2; /* Just for page count debugging... */
		} else {
			/* If the page is removed from container initiative (during a 
			 * container destruction for instance), we have to remove the
			 * page from the file cache by hand. */
			file_invalidate_page(objEntry, ctnr, objid);
		}
	}
	DEBUG(3, "Remove page (%ld;%ld) : done\n", ctnr->ctnrid, objid);
	return 0;
}

/** Synchronize a file page on disk.
 *  @author Renaud Lottiaux
 *
 *  @param  ctnr      Container descriptor
 *  @param  padeid    Id of the page to synchronize.
 *  @param  page      Descriptor of the page to synchronize.
 */
int file_sync_page(ctnrObj_t * objEntry, container_t * ctnr,
                    objid_t objid, void *object)
{
	file_iolinker_data_t *file_data = ctnr->iolinker_data;
	struct page *phys_page, *ctnr_page = object;
	struct address_space *phys_mapping;
	struct file *phys_file;
	int status;
	unsigned long index, file_size, file_index, end;

	phys_file = file_data->physical_file;
	phys_mapping = phys_file->f_dentry->d_inode->i_mapping;

	index = objid;

	DEBUG(3, "Sync file page (%ld;%ld) from %s (size %d) (inode %p)\n",
	      ctnr->ctnrid, index, phys_file->f_dentry->d_name.name,
	      (int) phys_mapping->host->i_size, phys_mapping->host);

	file_size = phys_mapping->host->i_size;
	file_index = file_size / PAGE_SIZE;
	if (file_index > index)
		end = PAGE_SIZE;
	else
		end = file_size % PAGE_SIZE;

	phys_page = find_lock_page(phys_mapping, index);
	if (!phys_page) {
		phys_page = page_cache_alloc(phys_mapping);
		if (!phys_page)
			return -ENOMEM;
		add_to_page_cache_lru(phys_page, phys_mapping, index, GFP_KERNEL);
	}

	if (!PageLocked(phys_page))
		PAGE_BUG(phys_page);

	status = phys_mapping->a_ops->prepare_write(phys_file, phys_page, 0, end);

	if (status) {
		WARNING("I got status %d\n", status);
		page_cache_release(phys_page);
		goto exit;
	}

	copy_highpage(phys_page, ctnr_page);

	status = phys_mapping->a_ops->commit_write(phys_file, phys_page, 0, end);
	if (status) {
		WARNING ("I got status %d\n", status);
		page_cache_release(phys_page);
		goto exit;
	}

	/* Mark it unlocked again and drop the page.. */
	SetPageReferenced(phys_page);
	ClearPageLocked(phys_page);
	page_cache_release(phys_page);

 exit:

	{
		int nr_try = 3;

		if (status == 0)
			while (!PageUptodate(ctnr_page) && nr_try) {
				wait_on_page_locked(ctnr_page);
				nr_try--;
			}
	}

	DEBUG(3, "Sync file page (%ld;%ld) from %s (size %d) up to %ld : "
	      "done with err %d\n",
	      ctnr->ctnrid, index, phys_file->f_dentry->d_name.name,
	      (int) phys_mapping->host->i_size, end, status);

	return status;
}



void add_ctnr_page_in_cache(struct page *page, container_t * ctnr)
{
	file_iolinker_data_t *file_data = ctnr->iolinker_data;
	struct address_space *mapping = file_data->logical_inode->i_mapping;
	objid_t objid = page->index;
	int r;

	DEBUG(3, "Add page %p (%ld;%ld) to cache\n", page, ctnr->ctnrid, objid);

	r = add_to_page_cache_lru(page, mapping, objid, GFP_KERNEL);

	if (page->mapping == NULL)
		WARNING("Page (%ld;%ld) badly inserted in file cache\n",
			ctnr->ctnrid, objid);

	DEBUG(3, "Add page (%ld;%ld) : done (count = %d - mapping %p) err = %d\n",
	      ctnr->ctnrid, objid, page_count (page), mapping, r);
	return;
}

/** Insert a new container page in the file cache.
 *  @author Renaud Lottiaux
 *
 *  @param  objEntry  Descriptor of the page to insert.
 *  @param  ctnr       Container descriptor
 *  @param  padeid     Id of the page to insert.
 */
int file_insert_page(ctnrObj_t * objEntry, container_t * ctnr, objid_t objid)
{
	struct page *page;

	DEBUG(3, "Insert page %p (%ld;%ld)\n", objEntry->object, ctnr->ctnrid,
	      objid);

	page = objEntry->object;
	page->index = objid;

	add_ctnr_page_in_cache(page, ctnr);

	SetPageUptodate(page);

	if (PageLocked(page))
		unlock_page(page);

	DEBUG (3, "Insert page (%ld;%ld) %p (@ %p) : done (count = %d)\n",
	       ctnr->ctnrid, objid, page, page_address(page), page_count(page));
	return 0;
}


/** Release an inode struct. Update the container inode data.
 *  @author Renaud Lottiaux
 *
 *  @param  objEntry  Descriptor of the page to insert.
 *  @param  ctnr       Container descriptor
 *  @param  padeid     Id of the page to insert.
 */
int file_put_object(ctnrObj_t * objEntry, container_t * ctnr, objid_t objid)
{
	struct page *page;

	page = objEntry->object;
	if (!page)
		return 0;

	DEBUG(3, "Put page (%ld;%ld) %p (@ %p) : done (count = %d)\n",
	      ctnr->ctnrid, objid, page, page_address(page), page_count(page));  
	return 0;
}


/****************************************************************************/

/* Init the file IO linker */

struct iolinker_struct file_io_linker = {
	instantiate:       file_instantiate,
	uninstantiate:     file_uninstantiate,
	first_touch:       file_first_touch,
	remove_object:     file_remove_page,
	invalidate_object: file_invalidate_page,
	sync_object:       file_sync_page,
	insert_object:     file_insert_page,
	put_object:        file_put_object,
	linker_name:       "file",
	linker_id:         FILE_LINKER,
	alloc_object:      file_alloc_object,
	export_object:     file_export_object,
	import_object:     file_import_object
};


/*****************************************************************************/
/*                                                                           */
/*                             CONTAINER TOOLS                               */
/*                                                                           */
/*****************************************************************************/

int do_update_physical_inode_attr(container_t * ctnr, struct iattr *attrs)
{
	file_iolinker_data_t *file_data;
	struct dentry *dentry, *parent;
	struct file *file;

	ASSERT(ctnr != NULL);
	ASSERT(ctnr->iolinker_data != NULL);

	file_data = ctnr->iolinker_data;
	file = file_data->physical_file;
	dentry = file->f_dentry;

	DEBUG(2, "Update inode for container %ld (file %s) (inode %p)\n",
	      ctnr->ctnrid, dentry->d_name.name, dentry->d_inode);

	if (ctnr->iolinker->linker_id == DIR_LINKER)
		parent = dentry->d_parent;
	else
		parent = dentry;

	down(&parent->d_inode->i_sem);

	if (dentry->d_inode == NULL)
		goto exit; /* The directory has been destroyed in the mean time... */

	if (attrs->ia_valid & ATTR_SIZE) {
		if (dentry->d_inode->i_size == attrs->ia_size)
			attrs->ia_valid = attrs->ia_valid & (~ATTR_SIZE);
	}

	DEBUG(2, "Update inode for container %ld (file %s - size %d) (inode %p)\n",
	      ctnr->ctnrid, dentry->d_name.name, (int) dentry->d_inode->i_size,
	      dentry->d_inode);

	/* Special case for directory container : we have to update the physical
	 * directory and the meta-data file.
	 */
	if (ctnr->iolinker->linker_id == DIR_LINKER) {
		/* Do not change the physical file type */

		attrs->ia_mode = (dentry->d_inode->i_mode & S_IFMT) |
			(attrs->ia_mode & (~S_IFMT));
		notify_change(dentry, attrs);

		DEBUG(2, "Update inode for directory file %s\n", parent->d_name.name);

		/* Do not change the physical directory size and type */

		attrs->ia_valid = attrs->ia_valid & (~ATTR_SIZE);
		attrs->ia_mode = (parent->d_inode->i_mode & S_IFMT) |
			(attrs->ia_mode & (~S_IFMT));
		notify_change(parent, attrs);
	} else {
		/* Update the physical file inode attributes */

		if (S_ISLNK(attrs->ia_mode))
			attrs->ia_mode = (attrs->ia_mode & ~S_IFLNK) | S_IFREG;

		notify_change(dentry, attrs);
	}

 exit:

	up(&parent->d_inode->i_sem);

	if (dentry->d_inode != NULL)
		DEBUG(2, "Update inode for container %ld (file %s - size %d) : done\n",
		      ctnr->ctnrid, dentry->d_name.name, (int) dentry->d_inode->i_size);
	else
		DEBUG(2, "Update inode for container %ld (file %s - destroyed) : done\n",
		      ctnr->ctnrid, dentry->d_name.name);

	return 0;
}

int update_physical_inode_attr(container_t * ctnr, struct inode *inode,
			       unsigned int ia_valid)
{
	ctnr_file_io_msg_t msg;

	DEBUG(2, "Request update inode for container %ld\n", ctnr->ctnrid);

	ASSERT(ctnr != NULL);
	ASSERT(inode != NULL);

	msg.ctnrid = ctnr->ctnrid;
	msg.attrs.ia_mode = inode->i_mode;
	msg.attrs.ia_uid = inode->i_uid;
	msg.attrs.ia_gid = inode->i_gid;
	msg.attrs.ia_size = inode->i_size;
	msg.attrs.ia_atime = inode->i_atime;
	msg.attrs.ia_mtime = inode->i_mtime;
	msg.attrs.ia_ctime = inode->i_ctime;
	msg.attrs.ia_valid = ia_valid | ATTR_ATIME | ATTR_MTIME | ATTR_CTIME;

	if (ctnr->linked_node == kerrighed_node_id)
		do_update_physical_inode_attr(ctnr, &msg.attrs);
	else {
		if (ctnr->linked_node == CTNR_FULL_LINKED)
			async_cluster_wide_service_call(REQ_UPDATE_PHYS_INODE_ATTR,
							FILE_IO_CHAN, &msg,
							sizeof(ctnr_file_io_msg_t));
		else
			async_remote_service_call(ctnr->linked_node,
						  REQ_UPDATE_PHYS_INODE_ATTR,
						  FILE_IO_CHAN, &msg,
						  sizeof(ctnr_file_io_msg_t));
	}

	DEBUG(2, "Request update inode for container %ld : done\n", ctnr->ctnrid);
	return 0;
}

/** Handler for updating a physical inode attributes.
 *  @author Renaud Lottiaux
 *
 *  @param sender    Identifier of the remote requesting machine.
 *  @param msg       Identifier of the container.
 */
int handle_update_physical_inode_attr(kerrighed_node_t sender, void *_msg)
{
	ctnr_file_io_msg_t *msg = _msg;
	container_t *ctnr;
	int r = 0;

	DEBUG(4, "Update inode for container %ld\n", msg->ctnrid);

	ctnr = find_container(msg->ctnrid);
	
	if (!ctnr) {
		r = -ENOENT;
		goto exit;
	}

	r = do_update_physical_inode_attr(ctnr, &msg->attrs);

 exit:
	DEBUG(4, "Update inode for container %ld : done with err %d\n",
	      msg->ctnrid, r);
	return r;
}

/** Handler for renaming a KerFS file.
 *  @author Renaud Lottiaux
 *
 *  @param sender    Identifier of the remote requesting machine.
 *  @param msg       Identifier of the container.
 */
int handle_kerfs_rename(kerrighed_node_t sender, void *_msg)
{
	kerfs_rename_msg_t *msg = _msg;
	file_iolinker_data_t *src_dir_file_data, *dst_dir_file_data,
		*file_file_data;
	container_t *src_dir_ctnr, *dst_dir_ctnr, *file_ctnr;
	struct dentry *src_dir_dentry, *dst_dir_dentry, *file_dentry;
	struct vfsmount *src_dir_mnt, *dst_dir_mnt;
	char *tmp = (char *) __get_free_page(GFP_KERNEL), *src_dir_file_name;
	char *tmp2 = (char *) __get_free_page(GFP_KERNEL), *dst_dir_file_name;
	char *oldname, *newname;
	int r = -ENOENT;

	DEBUG(4, "Rename file\n");

	src_dir_ctnr = find_container(msg->old_dir);
	if (!src_dir_ctnr)
		goto exit;

	dst_dir_ctnr = find_container(msg->new_dir);
	if (!dst_dir_ctnr)
		goto exit;

	file_ctnr = find_container(msg->file);
	if (!file_ctnr)
		goto exit;

	src_dir_file_data = src_dir_ctnr->iolinker_data;
	src_dir_dentry = src_dir_file_data->physical_file->f_dentry->d_parent;
	src_dir_mnt = src_dir_file_data->physical_file->f_vfsmnt;

	dst_dir_file_data = dst_dir_ctnr->iolinker_data;
	dst_dir_dentry = dst_dir_file_data->physical_file->f_dentry->d_parent;
	dst_dir_mnt = dst_dir_file_data->physical_file->f_vfsmnt;

	file_file_data = file_ctnr->iolinker_data;
	file_dentry = file_file_data->physical_file->f_dentry;
	if (file_dentry->d_name.len == 3 &&
	    strncmp (file_dentry->d_name.name, "...", 3) == 0)
		file_dentry = file_dentry->d_parent;

	src_dir_file_name = physical_d_path(src_dir_dentry, src_dir_mnt, tmp);
	dst_dir_file_name = physical_d_path(dst_dir_dentry, dst_dir_mnt, tmp2);

	r = -ENOMEM;

	oldname = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!oldname)
		goto exit;

	newname = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!newname) {
		kfree(oldname);
		goto exit;
	}

	sprintf(oldname, "%s/%s", src_dir_file_name, file_dentry->d_name.name);
	sprintf(newname, "%s/%s", dst_dir_file_name, &msg->new_name);

	DEBUG(3, "Move file %s to %s\n", oldname, newname);

	sys_rename(oldname, newname);

	kfree(oldname);
	kfree(newname);

	r = 0;

 exit:
	free_page((unsigned long) tmp);
	free_page((unsigned long) tmp2);

	DEBUG(4, "Rename file : done\n");

	return r;
}

int kerfs_physical_rename(ctnrid_t old_dir, ctnrid_t new_dir,
                           ctnrid_t file, char *new_name)
{
	kerfs_rename_msg_t *msg;
	container_t *ctnr;
	int size, err;

	DEBUG(4, "Rename file\n");

	ctnr = find_container(file);
	if (!ctnr)
		return -ENOENT;

	size = sizeof(kerfs_rename_msg_t) + strlen(new_name);
	if (size > PAGE_SIZE)
		return -ENAMETOOLONG;

	msg = kmalloc(size, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	msg->old_dir = old_dir;
	msg->new_dir = new_dir;
	msg->file = file;
	strcpy(&msg->new_name, new_name);

	err = ctnr_service_call_on_linked_nodes(ctnr, NULL, REQ_KERFS_RENAME,
						 FILE_IO_CHAN, msg, size);
	kfree(msg);
	DEBUG(4, "Rename file : done\n");
	return err;
}

/* Containers mechanisms initialisation.*/

void file_container_init(void)
{
	register_node_service(FILE_IO_CHAN, REQ_UPDATE_PHYS_INODE_ATTR,
			      handle_update_physical_inode_attr,
			      sizeof(ctnr_file_io_msg_t));

	register_node_service(FILE_IO_CHAN, REQ_KERFS_RENAME,
			      handle_kerfs_rename, PAGE_SIZE);
}



/* Containers mechanisms finalisation.*/

void file_container_finalize(void)
{
}
