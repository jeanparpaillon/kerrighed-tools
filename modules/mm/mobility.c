/** Implementation of process Virtual Memory mobility mechanisms.
 *  @file vm_mobility.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2009, Renaud Lottiaux, Kerlabs.
 *
 *  Implementation of functions used to migrate, duplicate and checkpoint
 *  process virtual memory.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/rmap.h>
#include <linux/swap.h>
#include <linux/vmalloc.h>
#include <linux/init_task.h>
#ifdef CONFIG_X86_64
#include <asm/ia32.h>
#endif
#include <linux/file.h>
#ifndef CONFIG_USERMODE
#include <asm/ldt.h>
#else
#include <asm/arch/ldt.h>
#endif
#include <asm/pgalloc.h>
#include <asm/tlbflush.h>
#include <asm/pgtable.h>
#include <kerrighed/krgsyms.h>
#include <kerrighed/krginit.h>
#include <rpc/rpcid.h>
#include <ctnr/kddm.h>
#include <fs/mobility.h>
#include <ghost/ghost.h>
#include <epm/action.h>
#include <epm/application/app_shared.h>

#include "memory_int_linker.h"
#include "memory_io_linker.h"
#include "mobility.h"
#include "mm_struct.h"

#include "debug_kermm.h"

void unimport_mm_struct(struct task_struct *task);



/*****************************************************************************/
/*                                                                           */
/*                               TOOLS FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/



void partial_init_vma(struct mm_struct *mm, struct vm_area_struct *vma)
{
	vma->vm_mm = mm;
	vma->vm_next = NULL;
	INIT_LIST_HEAD (&vma->anon_vma_node);
	vma->vm_truncate_count = 0;
	memset (&vma->shared, 0, sizeof (vma->shared));
	memset (&vma->vm_rb, 0, sizeof (vma->vm_rb));
}



void free_ghost_mm (struct task_struct *tsk)
{
	DEBUG ("mobility", 1, "starting...\n");

	/* if not NULL, mm_release will try to write in userspace... which
	 * does not exist anyway since we are in kernel thread context
	 */
	tsk->clear_child_tid = NULL;
	/* Do not notify end of vfork here */
	tsk->vfork_done = NULL;
	mmput (tsk->mm);

	/* exit_mm supposes current == tsk, and therefore, leaves one
	 * reference to tsk->mm because of mm->active_mm which will be dropped
	 * during schedule.
	 * The ghost mm will never be scheduled out because no real process is
	 * associated to it, thereofore, we take care of the active_mm case
	 * here
	 */
	if (!tsk->mm) {
		mmdrop (tsk->active_mm);
		tsk->active_mm = NULL;
	}

	DEBUG ("mobility", 1, "done...\n");
}



/*****************************************************************************/
/*                                                                           */
/*                              EXPORT FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/



/** Export one physical page of a process.
 *  @author Renaud Lottiaux
 *
 *  @param ghost    Ghost where data should be stored.
 *  @param vma      The memory area hosting the page.
 *  @param addr     Virtual address of the page.
 *
 *  @return  1 if a page has been exported.
 *           0 if no page has been exported.
 *           Negative value otherwise.
 */
static int export_one_page (ghost_t *ghost, struct vm_area_struct *vma,
			    unsigned long addr)
{
	struct kddm_set *set = NULL;
	unsigned long pfn;
	spinlock_t *ptl;
	struct page *page = NULL;
	char *page_addr ;
	objid_t objid = 0;
	pgprot_t prot ;
	pte_t *pte;
	int put_page = 0;
	int nr_exported = 0;
	int r;

	pte = get_locked_pte (vma->vm_mm, addr, &ptl);
	if (pte && pte_present (*pte)) {
		pfn = pte_pfn(*pte);
		page = pfn_to_page(pfn);
#ifndef CONFIG_X86_64
		prot = __pgprot(pte->pte_low & (~(PTE_MASK | _PAGE_ACCESSED)));
#else
		prot = __pgprot(pte->pte & (~(PTE_MASK | _PAGE_ACCESSED)));
#endif
		pte_unmap_unlock(pte, ptl);
		if (!page || !PageAnon(page))
			goto exit;
	}
	else {
		if (pte)
			pte_unmap_unlock(pte, ptl);

		set = vma->vm_mm->anon_vma_kddm_set;
		if (set) {
			objid = addr / PAGE_SIZE;
			page = kddm_get_object_no_ft(kddm_def_ns, set->id,
						     objid);
			prot = vma->vm_page_prot;
			put_page = 1;
		}
		if (page == NULL)
			goto exit;
	}

	page_addr = (char *)kmap(page);

	/* Export the virtual address of the page */
	r = ghost_write (ghost, &addr, sizeof (unsigned long));
	if (r)
		goto unmap;

	/* Export the page protection */
	r = ghost_write (ghost, &prot, sizeof(pgprot_t));
	if (r)
		goto unmap;

	/* Export the physical page content */
	r = ghost_write (ghost, (void*)page_addr, PAGE_SIZE);
	if (r)
		goto unmap;

unmap:
	kunmap(page);
	nr_exported = r ? r : 1;

exit:
	if (put_page)
		kddm_put_object(kddm_def_ns, set->id, objid);

	return nr_exported;
}



/** Export the physical pages hosted by a VMA.
 *  @author Renaud Lottiaux
 *
 *  @param ghost    Ghost where data should be stored.
 *  @param tsk      Task to export memory pages from.
 *  @param vma      The VMA to export pages from.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
static int export_vma_pages (ghost_t *ghost, struct vm_area_struct *vma)
{
	unsigned long addr ;
	int nr_pages_sent = 0 ;
	int r;

	if (!anon_vma(vma))
		goto done;

	for (addr = vma->vm_start; addr < vma->vm_end; addr += PAGE_SIZE) {
		r = export_one_page (ghost, vma, addr);
		if (r < 0)
			goto out;
		nr_pages_sent += r;
	}

done:
	/* Mark the end of the page exported */
	addr = 0;
	r = ghost_write (ghost, &addr, sizeof (unsigned long));
	if (r)
		goto out;

	r = ghost_write (ghost, &nr_pages_sent, sizeof (int)) ;

out:
	DEBUG ("mobility", 3, "%d pages sent r=%d\n", nr_pages_sent, r) ;
	return r;
}



/** This function exports the physical memory pages of a process
 *  @author Renaud Lottiaux, Matthieu Fertré
 *
 *  @param ghost       Ghost where pages should be stored.
 *  @param mm          mm_struct to export memory pages to.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int export_process_pages(struct epm_action *action,
			 ghost_t * ghost,
                         struct mm_struct *mm)
{
	struct vm_area_struct *vma;
	int r = 0;

	BUG_ON (mm == NULL);

	DEBUG ("mobility", 2, "Start exporting process pages\n");

	/* Export process VMAs */
	vma = mm->mmap;
	BUG_ON (vma == NULL);

	while (vma != NULL) {
		DEBUG ("mobility", 4, "Exporting pages from vma "
		       "[%#016lx:%#016lx]\n", vma->vm_start, vma->vm_end);

		r = export_vma_pages (ghost, vma);
		if (r)
			goto out;
		vma = vma->vm_next;
	}

	{
		int magic = 962134;
		r = ghost_write(ghost, &magic, sizeof(int));
	}

out:
	DEBUG ("mobility", 2, "Process pages export : done r=%d\n", r);

	return r;
}



/** Create the anonymous kddm_set for the given process.
 *  @author Renaud Lottiaux
 *
 *  @param ghost    Ghost where data should be stored.
 *  @param tsk      The task to create an anon kddm_set for.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 *
 *  The kddm_set is created empty. The caller must fill it with existing
 *  pages.
 */
static inline int anon_vma_kddm_set_creation (struct mm_struct *dst_mm,
					      struct mm_struct *src_mm,
					      int cow)
{
	struct kddm_set *set;

	set = alloc_anon_vma_kddm_set(src_mm, kerrighed_node_id, cow);
	if (IS_ERR(set))
		return PTR_ERR(set);

	set_anon_vma_kddm_set(dst_mm, set);

	/* The set is created for a distant operation. Do a pre-increment on
	 * the set for the future distant usage
	 */
	_increment_kddm_set_usage(set);

	return 0;
}



/** Export one VMA into the ghost.
 *  @author Renaud Lottiaux
 *
 *  @param ghost    Ghost where data should be stored.
 *  @param tsk      The task to export the VMA from.
 *  @param vma      The VMA to export.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
static int export_one_vma (struct epm_action *action,
			   ghost_t *ghost,
                           struct task_struct *tsk,
                           struct vm_area_struct *vma)
{
	krgsyms_val_t vm_ops_type, initial_vm_ops_type;
	int r;

	BUG_ON (vma->vm_private_data);

	DEBUG ("mobility", 3, "vma [%#016lx:%#016lx] has flags %#016lx\n",
	       vma->vm_start, vma->vm_end, vma->vm_flags);

	/* First, check if we need to link the VMA to the anon kddm_set */

	if (tsk->mm->anon_vma_kddm_set)
		check_link_vma_to_anon_memory_kddm_set (vma);

	/* Export the vm_area_struct */
	r = ghost_write (ghost, vma, sizeof (struct vm_area_struct));
	if (r)
		goto out;

#ifdef CONFIG_KRG_DVFS
	/* Export the associated file */
	r = export_vma_file (action, ghost, tsk, vma);
	if (r)
		goto out;
#endif
	/* Define and export the vm_ops type of the vma */

	vm_ops_type = krgsyms_export (vma->vm_ops);
	initial_vm_ops_type = krgsyms_export (vma->initial_vm_ops);

	DEBUG ("mobility", 4, "vm_ops type : %d\n", vm_ops_type);
	r = ghost_write (ghost, &vm_ops_type, sizeof (krgsyms_val_t));
	if (r)
		goto out;

	DEBUG ("mobility", 4, "initial_vm_ops type : %d\n",
	       initial_vm_ops_type);
	r = ghost_write (ghost, &initial_vm_ops_type, sizeof (krgsyms_val_t));

out:
	return r;
}



/** This function export the list of VMA to the ghost
 *  @author Renaud Lottiaux
 *
 *  @param ghost  Ghost where file data should be stored.
 *  @param tsk    Task to export vma data from.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int export_vmas (struct epm_action *action,
		 ghost_t *ghost,
                 struct task_struct *tsk)
{
	struct vm_area_struct *vma;
	int r;

	BUG_ON (tsk == NULL);
	BUG_ON (tsk->mm == NULL);

	DEBUG ("mobility", 4, "Number of vmas to ghost %d\n",
	       tsk->mm->map_count);

	/* Export process VMAs */

	r = ghost_write(ghost, &tsk->mm->map_count, sizeof(int));
	if (r)
		goto out;

	vma = tsk->mm->mmap;

	while (vma != NULL) {
		DEBUG ("mobility", 3, "Creating the ghost of vma "
		       "[%#016lx:%#016lx]\n", vma->vm_start, vma->vm_end);

		r = export_one_vma (action, ghost, tsk, vma);
		if (r)
			goto out;
		vma = vma->vm_next;
	}

	{
		int magic = 650874;

		r = ghost_write(ghost, &magic, sizeof(int));
	}

out:
	return r;
}



/** This function exports the context structure of a process
 *  @author Renaud Lottiaux
 *
 *  @param ghost  Ghost where data should be stored.
 *  @param mm     MM hosting context to export.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int export_context_struct (ghost_t * ghost,
                           struct mm_struct *mm)
{
#ifndef CONFIG_USERMODE
	if (mm->context.ldt)
		return ghost_write(ghost,
				   mm->context.ldt,
				   mm->context.size * LDT_ENTRY_SIZE);
#endif

	return 0;
}


static int cr_export_later_mm_struct(struct epm_action *action,
				     ghost_t *ghost,
				     struct task_struct *task)
{
	int r;
	long key;

	BUG_ON(action->type != EPM_CHECKPOINT);
	BUG_ON(action->checkpoint.shared != CR_SAVE_LATER);

	key = (long)(task->mm);

	r = ghost_write(ghost, &key, sizeof(long));
	if (r)
		goto err;

	r = add_to_shared_objects_list(&task->application->shared_objects,
				       MM_STRUCT, key, 1 /*is_local*/, task,
				       NULL);

	if (r == -ENOKEY) /* the mm_struct was already in the list */
		r = 0;
err:
	return r;
}



static inline int do_export_mm_struct(struct epm_action *action,
				      ghost_t *ghost,
				      struct mm_struct *mm)
{
	int r;

	switch (action->type) {
	  case EPM_CHECKPOINT:
		  krg_get_mm(mm->mm_id);
		  r = ghost_write(ghost, mm, sizeof(struct mm_struct));
		  krg_put_mm(mm->mm_id);
		  break;

	  default:
		  r = ghost_write(ghost, &mm->mm_id, sizeof(unique_id_t));
	}

	return r;
}



/** This function exports the virtual memory of a process
 *  @author Renaud Lottiaux
 *
 *  @param ghost  Ghost where VM data should be stored.
 *  @param tsk    Task to export memory data from.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int export_mm_struct(struct epm_action *action,
		     ghost_t *ghost,
		     struct task_struct *tsk)
{
	struct mm_struct *mm, *exported_mm;
	struct kddm_set *set = NULL;
	int r = 0;

	mm = tsk->mm;
	exported_mm = mm;

	switch (action->type) {
	  case EPM_CHECKPOINT:
		  if (action->checkpoint.shared == CR_SAVE_LATER) {
			  r = cr_export_later_mm_struct(action, ghost, tsk);
			  return r;
		  }
		  break;

	  case EPM_REMOTE_CLONE:
		  if (!(action->remote_clone.clone_flags & CLONE_VM)) {
			  /* Create the son mm struct */
			  exported_mm = alloc_fake_mm(mm);
			  if (!exported_mm)
				  return -ENOMEM;

			  /* Create an anon vma KDDM set for the son */

			  r = anon_vma_kddm_set_creation(exported_mm, mm,
							 /* cow */ 1);
			  if (r)
				  goto exit_put_mm;

			  set = exported_mm->anon_vma_kddm_set;

			  create_mm_struct_object(exported_mm);
			  /* MM not used locally -> drop the mm_users count
			   * (was setup to 1 in alloc and inc in
			   * create_mm_struct_object) */
			  atomic_dec(&exported_mm->mm_users);

			  break;
		  }
		  /* else fall through */

	  case EPM_MIGRATE:
		  if (!mm->anon_vma_kddm_set) {
			  r = anon_vma_kddm_set_creation(mm, mm, /* cow */ 0);
			  if (r)
				  goto exit_put_mm;
		  }

		  if (!mm->mm_id)
			  create_mm_struct_object(mm);

		  set = mm->anon_vma_kddm_set;

		  break;

	  default:
		  BUG();
        }

	/* Check some currently unsupported cases */
	BUG_ON(mm->core_startup_done != NULL);
	BUG_ON(mm->ioctx_list != NULL);

	r = do_export_mm_struct (action, ghost, exported_mm);
	if (r)
		goto up_mmap_sem;

	down_read(&mm->mmap_sem);
	r = export_context_struct(ghost, mm);
	if (r)
		goto up_mmap_sem;

	r = export_vmas(action, ghost, tsk);
	if (r)
		goto up_mmap_sem;

up_mmap_sem:
	up_read(&mm->mmap_sem);
	if (r)
		goto out;

	if (action->type == EPM_CHECKPOINT) {
		r = export_process_pages(action, ghost, mm);
		if (r)
			goto out;
	}

	if (set && (action->type == EPM_REMOTE_CLONE) &&
	    !(action->remote_clone.clone_flags & CLONE_VM)) {
	    	int ack;
		/*
		 * Wait for the ack of import_mm_struct(), so that we can drop
		 * our reference count on anon_set_id and let unimport_mm_struct
		 * drop it again if necessary.
		 * Note: if we fail to receive the ack, we assume that
		 * import_mm_struct is not being acquiring the reference. Either
		 * it failed before or during the acquire, or it failed after,
		 * but the request is not pending.
		 */
		r = ghost_read(ghost, &ack, sizeof(ack));
		_decrement_kddm_set_usage (set);
	}

out:
	return r;

exit_put_mm:
	mmput(exported_mm);
	return r;
}



/*****************************************************************************/
/*                                                                           */
/*                              IMPORT FUNCTIONS                             */
/*                                                                           */
/*****************************************************************************/



int import_vma_pages (ghost_t * ghost,
                      struct mm_struct *mm,
                      struct vm_area_struct *vma)
{
	void *page_addr;
	unsigned long address = 0;
	int nr_pages_received = 0;
	int nr_pages_sent;
	pgd_t *pgd;
	pgprot_t prot;
	int r;

	DEBUG ("mobility", 2, "Starting...\n");

	BUG_ON (vma == NULL);

	while (1) {
		struct page *new_page = NULL;
		pud_t *pud;
		pmd_t *pmd;
		pte_t *pte;

		DEBUG ("mobility", 5, "Receive page virtual address\n");
		r = ghost_read (ghost, &address, sizeof (unsigned long));
		if (r)
			goto err_read;

		if (address == 0)   /* We have reach the last VMA Page. */
			break;

		r = ghost_read (ghost, &prot, sizeof (pgprot_t));
		if (r)
			goto err_read;

		new_page = alloc_page (GFP_HIGHUSER);

		BUG_ON (new_page == 0);

		pgd = pgd_offset (mm, address);
		pud = pud_alloc (mm, pgd, address);
		pmd = pmd_alloc (mm, pud, address);
		BUG_ON(!pmd);

		pte = pte_alloc_map (mm, pmd, address);
		BUG_ON(!pte);
		set_pte (pte, mk_pte (new_page, prot));

		BUG_ON (unlikely(anon_vma_prepare(vma)));

		page_add_anon_rmap(new_page, vma, address);

		page_addr = kmap (new_page);
		r = ghost_read (ghost, page_addr, PAGE_SIZE);
		if (r)
			goto err_read;

		nr_pages_received++;

		kunmap (new_page);
	}

	r = ghost_read (ghost, &nr_pages_sent, sizeof (int));

	BUG_ON (nr_pages_sent != nr_pages_received);

err_read:
	DEBUG ("mobility", 2, "Done - r=%d\n", r);

	return r;
}



/** This function imports the physical memory pages of a process
 *  @author Renaud Lottiaux, Matthieu Fertré
 *
 *  @param ghost       Ghost where pages should be read from.
 *  @param mm          mm_struct to import memory pages in.
 *  @param incremental Tell whether or not the checkpoint is an incremental one
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int import_process_pages(struct epm_action *action,
			 ghost_t *ghost,
			 struct mm_struct *mm)
{
	struct vm_area_struct *vma;
	int r = 0;

	BUG_ON (mm == NULL);

	DEBUG ("mobility", 2, "Start importing process pages\n");

	vma = mm->mmap;
	BUG_ON (vma == NULL);

	while (vma != NULL) {
		DEBUG ("mobility", 4, "Importing pages from vma "
		       "[%#016lx:%#016lx]\n", vma->vm_start, vma->vm_end);

		r = import_vma_pages (ghost, mm, vma);
		if (r)
			goto exit;
		vma = vma->vm_next;
	}

	{
		int magic;

		r = ghost_read(ghost, &magic, sizeof(int));
		BUG_ON (!r && magic != 962134);
	}
exit:
	DEBUG ("mobility", 2, "Process pages import : done - r=%d\n", r);

	return r;
}



/** Import one VMA from the ghost.
 *  @author  Geoffroy Vallee, Renaud Lottiaux
 *
 *  @param ghost    Ghost where data are be stored.
 *  @param tsk      The task to import the VMA to.
 *  @param vma      The VMA to import.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
static int import_one_vma (struct epm_action *action,
			   ghost_t *ghost,
                           struct task_struct *tsk)
{
	struct vm_area_struct *vma;
	krgsyms_val_t vm_ops_type, initial_vm_ops_type;
	int r;

	DEBUG ("mobility", 2, "Receive a vma\n");

	vma = kmem_cache_alloc (vm_area_cachep, GFP_KERNEL);
	if (!vma)
		return -ENOMEM;

	/* Import the vm_area_struct */
	r = ghost_read (ghost, vma, sizeof (struct vm_area_struct));
	if (r)
		goto exit_free_vma;

	partial_init_vma(tsk->mm, vma);

#ifdef CONFIG_KRG_DVFS
	/* Import the associated file */
	r = import_vma_file (action, ghost, tsk, vma);
	if (r)
		goto exit_free_vma;
#endif

	DEBUG ("mobility", 3, "vm_ops reception\n");

	/* Import the vm_ops type of the vma */
	r = ghost_read (ghost, &vm_ops_type, sizeof (krgsyms_val_t));
	if (r)
		goto exit_free_vma;

	vma->vm_ops = krgsyms_import (vm_ops_type);
	DEBUG ("mobility", 4, "Type of the vm_ops %d (%p)\n", vm_ops_type,
	       vma->vm_ops);

	r = ghost_read (ghost, &initial_vm_ops_type, sizeof (krgsyms_val_t));
	if (r)
		goto exit_free_vma;

	vma->initial_vm_ops = krgsyms_import (initial_vm_ops_type);
	DEBUG ("mobility", 4, "Type of the initial_vm_ops %d (%p)\n",
	       initial_vm_ops_type,vma->initial_vm_ops );

	BUG_ON (vma->vm_ops == &generic_file_vm_ops && vma->vm_file == NULL);

	if (action->type == EPM_REMOTE_CLONE
	    && !(action->remote_clone.clone_flags & CLONE_VM))
		check_link_vma_to_anon_memory_kddm_set (vma);

	if (action->type == EPM_CHECKPOINT)
		restore_initial_vm_ops(vma);

	vma->anon_vma = NULL;

	DEBUG ("mobility", 4, "Insert the VMA [%#016lx:%#016lx] (%p) in "
	       "the mm struct %p\n", vma->vm_start, vma->vm_end, vma, tsk->mm);

	r = insert_vm_struct (tsk->mm, vma);
	if (r)
		goto exit_free_vma;

exit:
	return r;

exit_free_vma:
	kmem_cache_free(vm_area_cachep, vma);
	goto exit;
}



/** This function imports the list of VMA from the ghost
 *  @author Renaud Lottiaux
 *
 *  @param ghost  Ghost where file data should be stored.
 *  @param tsk    Task to import vma data to.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
static int import_vmas (struct epm_action *action,
			ghost_t *ghost,
			struct task_struct *tsk)
{
	int i;
	struct mm_struct *mm;
	int nr_vma = -1;
	int r;

	BUG_ON (tsk == NULL);

	DEBUG ("mobility", 1, "Starting...\n");

	mm = tsk->mm;

	r = ghost_read(ghost, &nr_vma, sizeof(int));
	if (r)
		goto exit;

	DEBUG ("mobility", 2, "Number of VMA to recevie : %d\n", nr_vma);

	for (i = 0; i < nr_vma; i++) {
		r = import_one_vma (action, ghost, tsk);
		if (r)
			/* import_mm_struct will cleanup */
			goto exit;
	}

	flush_tlb_all ();

	{
		int magic = 0;

		r = ghost_read(ghost, &magic, sizeof(int));
		BUG_ON (!r && magic != 650874);
	}

exit:
	DEBUG ("mobility", 1, "end - r=%d\n", r);
	return r;
}



/** This function imports the context structure of a process
 *  @author Renaud Lottiaux
 *
 *  @param ghost  Ghost where data are stored.
 *  @param mm     MM context to import data in.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int import_context_struct (ghost_t * ghost,
                           struct mm_struct *mm)
{
	int r = 0;

#ifndef CONFIG_USERMODE

	if (mm->context.ldt) {
		int orig_size = mm->context.size;

		mm->context.ldt = NULL;
		mm->context.size = 0;

		r = alloc_ldt (&mm->context, orig_size, 0);
		if (r < 0)
			return r;

		r = ghost_read(ghost, mm->context.ldt,
			       mm->context.size * LDT_ENTRY_SIZE);
		if (r)
			goto exit;
	}

	init_MUTEX (&mm->context.sem);
#endif
exit:
	return r;
}


static int cr_link_to_mm_struct(struct epm_action *action,
				ghost_t *ghost,
				struct task_struct *tsk)
{
	int r;
	long key;
	struct mm_struct *mm;

	r = ghost_read(ghost, &key, sizeof(long));
	if (r)
		goto err;

	mm = get_imported_shared_object(&action->restart.app->shared_objects,
					MM_STRUCT, key);

	if (!mm) {
		r = -E_CR_BADDATA;
		goto err;
	}

        /* the task is not yet hashed, no need to lock */
	atomic_inc(&mm->mm_users);

	tsk->mm = mm;
	tsk->active_mm = mm;
err:
	return r;
}



static inline int do_import_mm_struct(struct epm_action *action,
				      ghost_t *ghost,
				      struct mm_struct **returned_mm)
{
	struct mm_struct *mm;
	unique_id_t mm_id;
	int r = 0;

	switch(action->type) {
	  case EPM_CHECKPOINT:
		  mm = allocate_mm();
		  if (!mm)
			  goto done;

		  r = ghost_read (ghost, mm, sizeof (struct mm_struct));
		  if (r)
			  goto exit_free_mm;

		  r = reinit_mm(mm);
		  if (r)
			  goto exit_free_mm;

		  atomic_set(&mm->mm_ltasks, 0);
		  mm->mm_id = 0;
		  mm->anon_vma_kddm_set = NULL;
		  mm->anon_vma_kddm_id = KDDM_SET_UNUSED;
		  break;

	  default:
		  r = ghost_read (ghost, &mm_id, sizeof (unique_id_t));
		  if (r)
			  return r;
		  mm = krg_get_mm(mm_id);
		  if (mm)
			  atomic_inc(&mm->mm_users);
	}

done:
	if (!mm)
		return -ENOMEM;

	*returned_mm = mm;

	return r;

exit_free_mm:
	free_mm(mm);
	return r;
}



/** This function imports the mm_struct of a process
 *  @author  Geoffroy Vallee, Renaud Lottiaux
 *
 *  @param ghost  Ghost where file data should be loaded from.
 *  @param tsk    Task to import file data in.
 *
 *  @return  0 if everything ok.
 *           Negative value otherwise.
 */
int import_mm_struct (struct epm_action *action,
		      ghost_t *ghost,
                      struct task_struct *tsk)
{
	struct mm_struct *mm = NULL;
	struct kddm_set *set;
	int r;

	DEBUG ("mobility", 2, "Importing mm struct for process %d (%s - %p)\n",
	       tsk->pid, tsk->comm, tsk);

	if (action->type == EPM_CHECKPOINT
 	    && action->restart.shared == CR_LINK_ONLY) {
 		r = cr_link_to_mm_struct(action, ghost, tsk);
 		return r;
 	}

	r = do_import_mm_struct (action, ghost, &mm);
	if (r)
		return r;

	tsk->mm = mm;
	tsk->active_mm = mm;

	/* Import context */
	r = import_context_struct (ghost, mm);
	if (unlikely (r < 0))
		goto err;

	/* Just paranoia check */
	BUG_ON (mm->core_startup_done);

	r = import_vmas (action, ghost, tsk);
	if (r < 0)
		goto err;

	mm->hiwater_rss = get_mm_rss(mm);
	mm->hiwater_vm = mm->total_vm;

	if (action->type == EPM_CHECKPOINT) {
		r = import_process_pages(action, ghost, mm);
		if (r)
			goto err;
	}

	set = mm->anon_vma_kddm_set;

	krg_put_mm (mm->mm_id);

	if (set && (action->type == EPM_REMOTE_CLONE) &&
	    !(action->remote_clone.clone_flags & CLONE_VM)) {
		int ack = 0;

		_increment_kddm_set_usage(set);
		/*
		 * export_mm_struct() can now drop its reference on
		 * anon_vma_kddm_set
		 */
		r = ghost_write(ghost, &ack, sizeof(ack));
 		if (r)
			goto err_no_put;
 	}

	return 0;

err:
	krg_put_mm (mm->mm_id);
	unimport_mm_struct(tsk);
err_no_put:
	return r;
}



void unimport_mm_struct(struct task_struct *task)
{
	free_ghost_mm(task);
}



static int cr_export_now_mm_struct(struct epm_action *action, ghost_t *ghost,
				   struct task_struct *task,
				   union export_args *args)
{
	int r;
	r = export_mm_struct(action, ghost, task);
	return r;
}


static int cr_import_now_mm_struct(struct epm_action *action, ghost_t *ghost,
				   struct task_struct *fake,
				   void **returned_data)
{
	int r;
	BUG_ON(*returned_data != NULL);

	r = import_mm_struct(action, ghost, fake);
	if (r)
		goto err;

	*returned_data = fake->mm;
err:
	return r;
}

static int cr_import_complete_mm_struct(struct task_struct *fake, void *_mm)
{
	struct mm_struct *mm = _mm;
	mmput(mm);

	return 0;
}

static int cr_delete_mm_struct(struct task_struct *fake, void *_mm)
{
	struct mm_struct *mm = _mm;
	mmput(mm);

	return 0;
}

struct shared_object_operations cr_shared_mm_struct_ops = {
        .restart_data_size  = 0,
        .export_now         = cr_export_now_mm_struct,
	.import_now         = cr_import_now_mm_struct,
	.import_complete    = cr_import_complete_mm_struct,
	.delete             = cr_delete_mm_struct,
};
