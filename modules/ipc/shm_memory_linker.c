/** Container Shared memory linker.
 *  @file shm_memory_linker.c
 *  
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */

#include <linux/mm.h>
#include <linux/shm.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/string.h>
#include <linux/rmap.h>
#include <linux/pagemap.h>
#include <linux/ipc.h>
#include <linux/msg.h>
#include <linux/mm_inline.h>
#include <linux/kernel.h>

#include "debug_keripc.h"

#define MODULE_NAME "Shm Mem linker  "

#ifdef SHM_MEM_LINKER_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <ctnr/kddm.h>


extern int memory_first_touch (struct kddm_obj * obj_entry,
			       struct kddm_set * set, objid_t objid,int flags);

extern int memory_invalidate_page (struct kddm_obj * objEntry,
				   struct kddm_set * ctnr, objid_t objid);

void memory_change_state (struct kddm_obj * objEntry, struct kddm_set * ctnr,
			  objid_t objid, kddm_obj_state_t state);

extern int memory_remove_page (void *object,
			       struct kddm_set * ctnr, objid_t objid);
extern int memory_alloc_object (struct kddm_obj * objEntry,
				struct kddm_set * ctnr, objid_t objid);

extern int memory_import_object (struct kddm_obj *objEntry,
				 struct rpc_desc *desc);
extern int memory_export_object (struct rpc_desc *desc,
				 struct kddm_obj *objEntry);



/*****************************************************************************/
/*                                                                           */
/*                        SHM CONTAINER IO FUNCTIONS                         */
/*                                                                           */
/*****************************************************************************/



/** Insert a new shm memory page in the corresponding mapping.
 *  @author Renaud Lottiaux
 *
 *  @param  objEntry  Descriptor of the page to insert.
 *  @param  ctnr      Container descriptor
 *  @param  padeid    Id of the page to insert.
 */
int shm_memory_insert_page (struct kddm_obj * objEntry,
			    struct kddm_set * ctnr,
			    objid_t objid)
{
	struct page *page;
	struct shmid_kernel *shp;
	struct address_space *mapping = NULL;
	int shm_id;

	DEBUG (DBG_KERIPC_PAGE_FAULTS, 3, "Insert page %p (%d;%ld;%ld) count"
	       " %d\n", objEntry->object, ctnr->ns->id, ctnr->id, objid,
	       page_count(objEntry->object));
	
	shm_id = *(int *) ctnr->private_data;

	shp = local_shm_lock(&init_ipc_ns, shm_id);
	
	if(shp == NULL)
		return -EINVAL;
	
	mapping = shp->shm_file->f_dentry->d_inode->i_mapping;

	local_shm_unlock(shp);

	page = objEntry->object;
	page->index = objid;
	add_to_page_cache_lru(page, mapping, objid, GFP_ATOMIC);
	unlock_page(page);

	DEBUG (DBG_KERIPC_PAGE_FAULTS, 3, "Insert page (%ld;%ld) %p (@ %p) : "
	       "done (count = %d)\n", ctnr->id, objid, page,
	       page_address(page), page_count (page));

	return 0;
}



/** Invalidate a container memory page.
 *  @author Renaud Lottiaux
 *
 *  @param  ctnr      Container descriptor
 *  @param  objid    Id of the page to invalidate
 */
int shm_memory_invalidate_page (struct kddm_obj * objEntry,
				struct kddm_set * ctnr,
				objid_t objid)
{
	int res ;
	
	DEBUG (DBG_KERIPC_PAGE_FAULTS, 3, "Invalidate page (%ld;%ld)\n", 
	       ctnr->id, objid);

	if (objEntry->object != NULL) {
		struct page *page = (struct page *) objEntry->object;
		DEBUG (DBG_KERIPC_PAGE_FAULTS, 3, "Page (%ld;%ld) (count = "
		       "%d;%d) - flags : 0x%08lx\n", ctnr->id, objid,
		       page_count (page), page_mapcount(page), page->flags);

		BUG_ON (page->mapping == NULL);
		BUG_ON (TestSetPageLocked(page));

		SetPageToInvalidate(page);
		res = try_to_unmap(page, 0);

		ClearPageToInvalidate(page);
		remove_from_page_cache (page);
		page_cache_release (page);

		if (PageDirty(page)) {
			printk ("Check why the page is dirty...\n");
			ClearPageDirty(page);
		}
		unlock_page(page);
		
		if (TestClearPageLRU(page))
			del_page_from_lru(page_zone(page), page);

		page_cache_release (page);

#ifdef DEBUG_PAGEALLOC
		int extra_count = 0;

		if (PageInVec(page))
			extra_count = 1;

		BUG_ON (page_mapcount(page) != 0);
		
		if ((page_count (page) != objEntry->countx + extra_count)) {
			WARNING ("Hum... page %p (%ld;%ld) has count %d;%d "
				 "(against %d)\n", page, ctnr->id, objid,
				 page_count (page), page_mapcount(page),
				 objEntry->countx + extra_count);
		}
		
		if (PageActive(page)) {
			WARNING ("Hum. page %p (%ld;%ld) has Active bit set\n",
				 page, ctnr->id, objid);
			while (1)
				schedule();
		}
#endif
	}
	
	DEBUG (DBG_KERIPC_PAGE_FAULTS, 3, "Invalidate page (%ld;%ld) : done\n",
	       ctnr->id, objid);
	
	return 0;
}



/****************************************************************************/

/* Init the memory IO linker */

struct iolinker_struct shm_memory_linker = {
	first_touch:       memory_first_touch,
	remove_object:     memory_remove_page,
	invalidate_object: shm_memory_invalidate_page,
	change_state:      memory_change_state,
	insert_object:     shm_memory_insert_page,
	linker_name:       "shm",
	linker_id:         SHM_MEMORY_LINKER,
	alloc_object:      memory_alloc_object,
	export_object:     memory_export_object,
	import_object:     memory_import_object
};



/*****************************************************************************/
/*                                                                           */
/*                              SHM VM OPERATIONS                            */
/*                                                                           */
/*****************************************************************************/



/** Handle a nopage fault on an anonymous VMA.
 * @author Renaud Lottiaux
 *
 *  @param  vma           vm_area of the faulting address area
 *  @param  address       address of the page fault
 *  @param  write_access  0 = read fault, 1 = write fault
 *  @return               Physical address of the page
 */
struct page *shmem_memory_nopage (struct vm_area_struct *vma,
				  unsigned long address,
				  int *res)
{
	struct inode *inode = vma->vm_file->f_dentry->d_inode;
	struct page *page;
	struct kddm_set *ctnr;
	objid_t objid;
	int write_access = vma->last_fault;

	DEBUG (DBG_KERIPC_PAGE_FAULTS, 1, "no page activated at 0x%08lx for "
	       "process %s (%d - %p). Access : %d in vma [0x%08lx:0x%08lx] "
	       "(0x%08lx) - file: 0x%p, anon_vma : 0x%p\n", address,
	       current->comm, current->pid, current, write_access,
	       vma->vm_start, vma->vm_end, (unsigned long) vma, vma->vm_file,
	       vma->anon_vma);
	
	address = address & PAGE_MASK ;
		
	ctnr = inode->i_mapping->kddm_set;
	
	BUG_ON (ctnr == NULL);
	objid = vma->vm_pgoff + (address - vma->vm_start) / PAGE_SIZE;
	
	if (write_access)
		page = kddm_grab_object (kddm_def_ns, ctnr->id, objid);
	else
		page = kddm_get_object (kddm_def_ns, ctnr->id, objid);
	
	page_cache_get(page);

	if (page->mapping == NULL) {
		printk ("Hum... NULL mapping in shmem_memory_nopage\n");
		page->mapping = inode->i_mapping;
	}
	
	kddm_put_object (kddm_def_ns, ctnr->id, objid);

	DEBUG (DBG_KERIPC_PAGE_FAULTS, 4, "Done: page (%ld;%ld) at %p "
	       "(count %d;%d) - mapping %p (anon : %d)\n", ctnr->id, objid,
	       page, page_count (page), page_mapcount(page), page->mapping,
	       PageAnon(page));
	
	return page;
}



/** Handle a wppage fault on a memory container.
 *  @author Renaud Lottiaux
 *
 *  @param  vma       vm_area of the faulting address area
 *  @param  virtaddr  Virtual address of the page fault
 *  @return           Physical address of the page
 */
struct page *shmem_memory_wppage (struct vm_area_struct *vma,
				  unsigned long address)
{
	struct inode *inode = vma->vm_file->f_dentry->d_inode;
	struct page *page;
	struct kddm_set *ctnr;
	objid_t objid;
	
	BUG_ON (vma == NULL);
	
	DEBUG (DBG_KERIPC_PAGE_FAULTS, 1, "wp page activated at 0x%08lx in"
	       "vma [0x%08lx:0x%08lx] (0x%08lx)\n", address,
	       vma->vm_start, vma->vm_end, (long) vma);

	ctnr = inode->i_mapping->kddm_set;
	
	BUG_ON (ctnr == NULL);	
	objid = vma->vm_pgoff + (address - vma->vm_start) / PAGE_SIZE;
	
	page = kddm_grab_object (kddm_def_ns, ctnr->id, objid);

	if (page->mapping == NULL)
		page->mapping = inode->i_mapping;

	kddm_put_object (kddm_def_ns, ctnr->id, objid);

	DEBUG (DBG_KERIPC_PAGE_FAULTS, 1, "Done : page at 0x%p has count %d/%d"
	       "\n", page, page_count (page), page_mapcount (page));
	
	return page;
}



static void shmem_memory_open (struct vm_area_struct *vma)
{
	DEBUG(DBG_KERIPC_SHM_MAP, 2, "Open vma [0x%08lx:0x%08lx]\n",
	      vma->vm_start, vma->vm_end);

	shm_inc (&init_ipc_ns, vma->vm_file->f_dentry->d_inode->i_ino);

	DEBUG(DBG_KERIPC_SHM_MAP, 2, "Open VMA [0x%08lx:0x%08lx]: done\n",
	      vma->vm_start, vma->vm_end);

}



/****************************************************************************/

/* Init the Kerrighed SHM file operations structure */

struct vm_operations_struct _krg_shmem_vmops = {
	open: shmem_memory_open,
	close:  shm_close,
	nopage: shmem_memory_nopage,
	wppage: shmem_memory_wppage,
};



/****************************************************************************/

static int krg_shmem_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct ipc_namespace *ns;

	DEBUG(DBG_KERIPC_SHM_MAP, 2, "mmap shm %ld to vma [0x%08lx:0x%08lx]\n",
	      file->f_dentry->d_inode->i_ino, vma->vm_start, vma->vm_end);

	ns = shm_file_ns(file);
	BUG_ON (ns != &init_ipc_ns);

        file_accessed(file);
	vma->vm_ops = &krg_shmem_vmops;
	vma->vm_flags |= VM_CONTAINER;
	shm_inc(ns, file->f_dentry->d_inode->i_ino);

	DEBUG(DBG_KERIPC_SHM_MAP, 2, "mmap to vma [0x%08lx:0x%08lx] : done\n",
	      vma->vm_start, vma->vm_end);

	return 0;
}

/* Init the Kerrighed SHM file operations structure */

struct file_operations krg_shmem_file_operations = {
	.mmap = krg_shmem_mmap,
};
