/** Container memory interface linker.
 *  @file memory_int_linker.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */

#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/string.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>
#include <linux/acct.h>
#include <kerrighed/pid.h>
#include <asm/tlb.h>

#include <ctnr/kddm.h>
#include "memory_int_linker.h"
#include "memory_io_linker.h"

#include "debug_kermm.h"



#define PTE_TABLE_MASK  ((PTRS_PER_PTE-1) * sizeof(pte_t))
#define PMD_TABLE_MASK  ((PTRS_PER_PMD-1) * sizeof(pmd_t))

struct vm_operations_struct null_vm_ops = {};


int insert_page_in_kddm_set (struct kddm_set * set,
			     objid_t objid,
			     struct page *page,
			     int cow)

{
	struct page *p;

	DEBUG ("int_linker", 4, "*** Add page (%ld;%ld) - in set "
	       "%ld\n", set->id, objid, set->id);

	p = _kddm_grab_object_manual_ft(set, objid);
	if (p != NULL)
		BUG();
	_kddm_set_object(set, objid, page);
	_kddm_put_object (set, objid);

	if (!cow) {
		/* Inc page counter to reflect kddm set belonging */
		atomic_inc (&page->_count);
	}

	DEBUG ("int_linker", 4, "*** Add page (%ld;%ld) - in set "
	       "%ld : done (count %d)\n", set->id, objid, set->id,
	       page_count(page));
	
	return 0;
}


int add_page_in_kddm_set_from_vma (struct kddm_set *set,
				   objid_t objid,
				   struct vm_area_struct *vma,
				   unsigned long addr,
				   int cow)
{
	struct page *page, *new_page;
	spinlock_t *ptl;
	int private_vma = !(vma->vm_flags & VM_SHARED);
	int page_dirty;
	int page_added = 0;
	pte_t *pte;

	page = NULL;
	pte = get_locked_pte (vma->vm_mm, addr, &ptl);
	if (pte && pte_present (*pte)) {
		page = pte_page (*pte);
		page_dirty = PageDirty(page) || pte_dirty(*pte) ;
	}

	if (page && (private_vma || page_dirty))
	{
		/*** Inset the page in the kddm set ***/          
		/*  Check if the VM page entry is the empty_zero_page.
		 *  If the page is the "empty_zero_page", replace it
		 *  by a newly allocated empty page.
		 */
		new_page = NULL;
		
		DEBUG ("int_linker", 4, "* Check page %p "
		       "(count %d;%d) - mapping %p (anon : %d)\n",
		       page, page_count (page), page_mapcount(page),
		       page->mapping, PageAnon(page));
		
		if (page == (ZERO_PAGE (NULL)))
		{
			DEBUG ("int_linker", 4, "** -> Page is"
			       "zero_page : Alloc new page\n");
			page = alloc_zeroed_user_highpage(vma, addr);
			if (!page)
				OOM;
		}
		else
			if ((!PageAnon(page)) ||
			    (page_mapcount(page) > 1) ||
			    cow)
			{
				DEBUG ("int_linker", 4, 
				       "* -> Non anon page: Alloc new page\n");
				new_page = alloc_page_vma(GFP_HIGHUSER,
							  vma, addr);
				copy_user_highpage(new_page, page, addr, vma);
			}
		
		if (new_page)
		{
			if (cow == 0)
			{
				/* Unmap the old page and map the new one */
				
				DEBUG ("int_linker", 4,
				       "** -> Replace page %p by page"
				       " %p\n", page, new_page);
				
				page_remove_rmap (page, vma);
				page_cache_release(page);
				
				if (unlikely(anon_vma_prepare(vma)))
					OOM;
				page_add_anon_rmap(new_page, vma, addr);
				
				set_pte (pte, pte_wrprotect (mk_pte (new_page,
							  vma->vm_page_prot)));
				flush_tlb_page (vma, addr);
			}
			
			page = new_page;
		}

		pte_unmap_unlock(pte, ptl);

		/* Insert the page in kddm set */
		insert_page_in_kddm_set (set, objid, page, cow);

		page_added = 1;

		DEBUG ("int_linker", 4, "* Page (%ld;%ld) at "
		       "%p (count %d;%d) - mapping %p (anon "
		       ": %d) has been inserted\n", set->id, 
		       objid, page, page_count (page),
		       page_mapcount(page), page->mapping,
		       PageAnon(page));
	}
	else
		pte_unmap_unlock(pte, ptl);

	return page_added;
}



int add_page_in_kddm_set_from_kddm_set (struct kddm_set *set,
					objid_t objid,
					struct kddm_set *src_set,
					struct vm_area_struct *vma,
					unsigned long addr)
{
	struct page *page, *new_page;
	int page_added = 0;
	
	BUG_ON (src_set == NULL);

	page = _kddm_get_object_no_ft (src_set, objid);
	
	if (page) {
		new_page = alloc_page_vma(GFP_HIGHUSER, vma, addr);
		copy_user_highpage(new_page, page, addr, vma);
		insert_page_in_kddm_set (set, objid, new_page, 1);
		page_added = 1;
	}
	
	_kddm_put_object (src_set, objid);

	return page_added;
}



/** Init a anon VMA kddm set with already existing pages from a VMA.
 *  @author Renaud Lottiaux
 *
 *  @param set      The kddm set to init.
 *  @param vma       VMA to get pages from.
 *
 *  @return error code or 0 if everything was ok.
 */
int fill_anon_vma_kddm_set (struct kddm_set *set,
			    struct kddm_set *src_set,
			    struct vm_area_struct *vma,
			    int cow)
{
	unsigned long addr;
	objid_t objid;

	DEBUG ("int_linker", 2, "Fill kddm set %ld with pages from "
	       "vma [%#016lx:%#016lx] file: 0x%p, anon_vma: 0x%p, offset: %ld,"
	       "flags : %#016lx\n", set->id, vma->vm_start, vma->vm_end,
	       vma->vm_file, vma->anon_vma, vma->vm_pgoff * PAGE_SIZE,
	       vma->vm_flags);
	
	objid = vma->vm_start / PAGE_SIZE;
	
	/* Initialize kddm set page table and page manager owner table */
	/* We initialize set page table and manager owner table depending
	   on already existing pages in the VMA */
	
	for (addr = vma->vm_start; addr < vma->vm_end; addr += PAGE_SIZE) {

		if (!add_page_in_kddm_set_from_vma (set, objid, vma, addr,cow)
		    && src_set)
			add_page_in_kddm_set_from_kddm_set (set, objid, src_set,
							    vma, addr);

		objid++;
	}
	
	DEBUG ("int_linker", 2, "Fill kddm set %ld with pages from "
	       "vma [%#016lx:%#016lx] :done\n", set->id, vma->vm_start,
	       vma->vm_end);

	return 0;
}



/** Init a anon VMA kddm set with already existing pages from the process.
 *  @author Renaud Lottiaux
 *
 *  @param tsk       Task to allocate a kddm set for.
 *  @param set      The kddm set to init.
 *
 *  @return	    0 if everything is OK.
 *                  Negative value otherwise.
 */
int init_anon_vma_kddm_set(struct mm_struct *mm,
			   struct kddm_set *set,
			   int cow)
{
	struct vm_area_struct *vma;
	
	for (vma = mm->mmap; vma != NULL; vma = vma->vm_next) {
		if (anon_vma(vma))
			fill_anon_vma_kddm_set(set, mm->anon_vma_kddm_set, vma,
					       cow);
	}

	return 0;
}



/** Alloc and init a kddm set to host anonymous pages of a process.
 *  @author Renaud Lottiaux
 *
 *  @param tsk       Task to allocate a kddm set for.
 *  @param mgr_node  Node who manage pages for the process.
 *
 *  @return	    The newly created kddm set.
 */
struct kddm_set *alloc_anon_vma_kddm_set(struct mm_struct *mm,
					 kerrighed_node_t mgr_node,
					 int cow)
{
	struct kddm_set *set;
	int r;
	
	set = create_new_kddm_set (kddm_def_ns, 0, MEMORY_LINKER, mgr_node,
				   PAGE_SIZE, 0);
	
	if (IS_ERR (set))
	{
		BUG();
		return set;
	}
	
	r = init_anon_vma_kddm_set (mm, set, cow);
	if (r)
		return ERR_PTR(r);

	return set;
}



/*****************************************************************************/
/*                                                                           */
/*                       MEMORY INTERFACE LINKER CREATION                    */
/*                                                                           */
/*****************************************************************************/



/** Link a VMA to the anon memory kddm set
 *  @author Renaud Lottiaux
 *
 *  @param  vma          vm_area to link with a kddm set.
 *
 *  @return   0 If everything OK,
 *            Negative value otherwise.
 */
int check_link_vma_to_anon_memory_kddm_set (struct vm_area_struct *vma)
{
	int r = 0;
	
	DEBUG ("int_linker", 2, "Link vma [%#016lx:%#016lx] with "
	       "offset %ld - file : %#016lx\n",
	       vma->vm_start, vma->vm_end, vma->vm_pgoff, (long) vma->vm_file);

	if (!anon_vma(vma))
		return r;

	/* Do not share the VDSO page as anonymous memory. Anyway it is always
	 * available on all nodes. */
	if (arch_vma_name(vma))
		return r;

	if (vma->vm_ops == &anon_memory_kddm_vmops)
		return r;
	
	/*** Make the VMA a kddm set VMA ***/
	
	BUG_ON(vma->vm_flags & VM_SHARED);
	
	BUG_ON(vma->initial_vm_ops == &anon_memory_kddm_vmops);
	if (vma->vm_ops == NULL)
		vma->initial_vm_ops = &null_vm_ops;
	else
		vma->initial_vm_ops = vma->vm_ops;
	vma->vm_ops = &anon_memory_kddm_vmops;
	vma->vm_flags |= VM_CONTAINER;
	
	DEBUG ("int_linker", 2, "Done\n");
	
	return r;
}



static inline void memory_kddm_readahead (struct kddm_set * set,
                                          objid_t start,
					  int upper_limit)
{
	int i, ra_restart_limit;
	int ra_start, ra_end;

	return ;

	/* Disable prefetching for threads */
	if (!thread_group_empty(current))
		return;

	ra_restart_limit = set->last_ra_start + (set->ra_window_size / 2);
	
	if (start <= set->last_ra_start - set->ra_window_size / 2)
	{
		ra_start = start;
		ra_end = min_t (int, set->last_ra_start - 1,
				ra_start + set->ra_window_size);
		
		goto do_prefetch;
	}
	
	if (start >= ra_restart_limit)
	{
		ra_end = start + set->ra_window_size;
		ra_start = max_t (int, start, set->last_ra_start + 
				  set->ra_window_size);
		
		goto do_prefetch;
	}
	
	return;
	
do_prefetch:
	set->last_ra_start = start;
	ra_end = min_t (int, ra_end, upper_limit);
	
	for (i = ra_start; i < ra_end; i++)
		_async_kddm_grab_object_no_ft (set, i);
}



/*****************************************************************************/
/*                                                                           */
/*                    MEMORY INTERFACE LINKER OPERATIONS                     */
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
struct page *anon_memory_nopage (struct vm_area_struct *vma,
				 unsigned long address,
				 int *res)
{
	struct page *page, *new_page;
	struct kddm_set *set;
	objid_t objid;
	int write_access = vma->last_fault;
	
	DEBUG ("int_linker", 1, "no page activated at %#016lx for "
	       "process %s (%d - %p). Access : %d in vma [%#016lx:%#016lx] "
	       "(%#016lx) - file: 0x%p, anon_vma : 0x%p\n", address,
	       current->comm, current->pid, current, write_access,
	       vma->vm_start, vma->vm_end, (unsigned long) vma, vma->vm_file,
	       vma->anon_vma);
	
	address = address & PAGE_MASK ;
	
	BUG_ON (vma == NULL);
	
	set = vma->vm_mm->anon_vma_kddm_set;
	
	BUG_ON (set == NULL);
	
	objid = address / PAGE_SIZE;

	if (thread_group_empty(current)) {
		write_access = 1;
		if (set->def_owner != kerrighed_node_id)
			memory_kddm_readahead (set, objid,
					       vma->vm_start / PAGE_SIZE);
	}

	if (vma->vm_file)
	{
		/* Mapped file VMA no page access */
		
		DEBUG ("int_linker", 4, "File mapped no page\n");

		/* First, try to check if the page already exist in the anon
		 * kddm set */
		if (write_access)
			page = _kddm_grab_object_manual_ft(set, objid);
		else
			page = _kddm_get_object_manual_ft (set, objid);
		
		if (page != NULL)
			goto done;
		
		DEBUG ("int_linker", 4, "Page not found : load it from"
		       "the file\n");
		
		/* Ok, the page is not present in the anon kddm set, let's
		 * load it */
		
		page = vma->initial_vm_ops->nopage(vma, address, res);
		if ((page == NOPAGE_SIGBUS) || (page == NOPAGE_OOM))
			goto exit_error;
		
		/* Copy the cache page into an anonymous page (copy on write
		 * will be done later on)
		 */
		new_page = alloc_page_vma(GFP_HIGHUSER, vma, address);
		if (!new_page)
		{
			page = NOPAGE_OOM;
			goto exit_error;
		}
		copy_user_highpage(new_page, page, address, vma);
		page_cache_release(page);
		page = new_page;
		
		_kddm_set_object(set, objid, page);
	}
	else
	{
		/* Memory VMA no page access */
	
		if (write_access)
			/* TODO: ensure that all work done by
			 * alloc_zeroed_user_highpage() is done on
			 * archs other than x86.
			 */
			page = _kddm_grab_object (set, objid);
		else
			page = _kddm_get_object (set, objid);
	}
	
done:
	atomic_inc (&page->_count);

	if (page->mapping) {
		if (page_mapcount(page) == 0) {
			printk ("Null mapping count, non null mapping address "
				": 0x%p\n", page->mapping);
			page->mapping = NULL;
		}
		else {
/********************* DEBUG ONLY *********************************/
			struct anon_vma *anon_vma;

			BUG_ON (!PageAnon(page));
			
			anon_vma = (void *)page->mapping - PAGE_MAPPING_ANON;
			if (anon_vma != vma->anon_vma) {
				printk ("Page mapping : %p - VMA anon : %p\n",
					anon_vma, vma->anon_vma);

				printk ("Fault af 0x%08lx for "
	       "process %s (%d - %p). Access : %d in vma [0x%08lx:0x%08lx] "
	       "(0x%08lx) - file: 0x%p, anon_vma : 0x%p\n", address,
	       current->comm, current->pid, current, write_access,
	       vma->vm_start, vma->vm_end, (unsigned long) vma, vma->vm_file,
	       vma->anon_vma);

				while(1) schedule();
			}
			
/********************* END DEBUG ONLY ******************************/
		}
	}
	
	_kddm_put_object (set, objid);
	
	DEBUG ("int_linker", 4, "Done: page (%ld;%ld) at %p "
	       "(count %d;%d) - mapping %p (anon : %d)\n", set->id, objid,
	       page, page_count (page), page_mapcount(page), page->mapping,
	       PageAnon(page));

exit_error:
	return page;
}



/** Handle a wppage fault on a memory kddm set.
 *  @author Renaud Lottiaux
 *
 *  @param  vma       vm_area of the faulting address area
 *  @param  virtaddr  Virtual address of the page fault
 *  @return           Physical address of the page
 */
struct page *anon_memory_wppage (struct vm_area_struct *vma,
				 unsigned long address)
{
	struct page *oldPage, *newPage;
	struct kddm_set *set;
	objid_t objid;
	struct kddm_obj *obj_entry = NULL;
	
	BUG_ON (vma == NULL);
	
	DEBUG ("int_linker", 1, "wp page activated at %#016lx in"
	       "vma [%#016lx:%#016lx] (%#016lx)\n", address,
	       vma->vm_start, vma->vm_end, (long) vma);
	
/*   printk("wp page activated at %#016lx in" */
/* 	 "vma [%#016lx:%#016lx] (%#016lx)\n", address, */
/* 	 vma->vm_start, vma->vm_end, (long)vma); */

	set = vma->vm_mm->anon_vma_kddm_set;
	
	BUG_ON (set == NULL);
	
	objid = address / PAGE_SIZE;

	if (set->def_owner != kerrighed_node_id)
		memory_kddm_readahead (set, objid, vma->vm_start / PAGE_SIZE);
		
	oldPage = _kddm_find_object (set, objid);

	if (oldPage != NULL)
	{
		obj_entry = __get_kddm_obj_entry (set, objid);

                /* Reflect page count increased within kernel */
		obj_entry->countx += 2;

		DEBUG ("int_linker", 4, "Page (%ld;%ld) already exist "
		       "at %p with count %d/%d countx is now %d)\n",
		       set->id, objid, oldPage, page_count (oldPage),
		       page_mapcount (oldPage), obj_entry->countx);

		kddm_obj_unlock (set, objid);

		_kddm_put_object (set, objid);
	}


  /*
     oldPage is used to check if the returned page is the same as the
     existing one, cause the existing one can be invalitaded during the
     grab_page...

     Before calling grab-page, we inc the old page count, to avoid the
     very unlikely case where the page is invalidated (so freed) and
     reallocated in the grab page. In this case, oldaddr = newaddr since
     the page have been invalidated and page count modified...

     If the the returned page is the same, the page count is ok. But if
     the return page is a new one, we have to inc the page count.
   */

	newPage = _kddm_grab_object (set, objid);

	obj_entry = __get_kddm_obj_entry (set, objid);

	if (oldPage == newPage)
		obj_entry->countx -= 2;
	
	DEBUG ("int_linker", 1, "Done : page at 0x%p has count %d/%d "
	       "(countx = %d)\n", newPage, page_count (newPage),
	       page_mapcount (newPage), obj_entry->countx);
	if (oldPage)
		DEBUG ("int_linker", 1, "     - old page at 0x%p has "
		       "count %d/%d (countx = %d)\n", oldPage,
		       page_count (oldPage), page_mapcount (oldPage),
		       obj_entry->countx);
	kddm_obj_unlock (set, objid);

	_kddm_put_object (set, objid);

	return newPage;
}



void anon_memory_close (struct vm_area_struct *vma)
{
}


/*
 * Virtual Memory Operation.
 *  Redefinition of some virtual memory operations. Used to handle page faults
 *  on a memory kddm set.
 *  @arg @c nopage is called when a page is touched for the first time
 * 	 (i.e. the page is not in memory and is not swap).
 *  @arg @c wppage is called when a page with read access is touch with a write
 *          access.
 *  @arg @c map is called when a vma is created or extended by do_mmap().
 */
struct vm_operations_struct anon_memory_kddm_vmops = {
	close:  anon_memory_close,
	nopage: anon_memory_nopage,
	wppage: anon_memory_wppage,
};
