/** Distributed management of the MM structure.
 *  @file mm_struct.c
 *
 *  Copyright (C) 2008-2009, Renaud Lottiaux, Kerlabs.
 */

#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <kerrighed/krginit.h>
#include <asm/uaccess.h>
#include <tools/krg_services.h>
#include <ctnr/kddm.h>
#include <hotplug/hotplug.h>
#include "memory_int_linker.h"
#include "memory_io_linker.h"
#include "mm_struct.h"

#include "debug_kermm.h"



int reinit_mm(struct mm_struct *mm)
{
	unique_id_t mm_id;

	DEBUG ("mm_struct", 2, "reinit mm %p\n", mm);

	/* Backup mm_id which is set to 0 in mm_init... */
	mm_id = mm->mm_id;
	if (!mm_init(mm))
		return -ENOMEM;

	mm->mm_id = mm_id;
	mm->locked_vm = 0;
	mm->mmap = NULL;
	mm->mmap_cache = NULL;
	mm->map_count = 0;
	cpus_clear (mm->cpu_vm_mask);
	mm->mm_rb = RB_ROOT;
	mm->nr_ptes = 0;
	mm->token_priority = 0;
	mm->last_interval = 0;
	mm->first_import = 0;
	/* Insert the new mm struct in the list of active mm */
	spin_lock (&mmlist_lock);
	list_add (&mm->mmlist, &init_mm.mmlist);
	spin_unlock (&mmlist_lock);

	DEBUG ("mm_struct", 2, "reinit mm %p: done\n", mm);

	return 0;
}



struct mm_struct *alloc_fake_mm(struct mm_struct *src_mm)
{
	struct mm_struct *mm;
	int r;

	DEBUG ("mm_struct", 2, "Alloc fake mm with src mm %p\n", src_mm);

	mm = allocate_mm();
	if (!mm)
		return NULL;

	if (src_mm == NULL) {
		memset(mm, 0, sizeof(*mm));
		if (!mm_init(mm))
			goto err_put_mm;
	}
	else {
		*mm = *src_mm;

		r = reinit_mm(mm);
		if (r)
			goto err_put_mm;
	}

	atomic_set(&mm->mm_ltasks, 0);

	DEBUG ("mm_struct", 2, "mm %p allocated\n", mm);

	return mm;

err_put_mm:
	mmput(mm);
	return NULL;
}



/* Unique mm_struct id generator root */
unique_id_root_t mm_struct_unique_id_root;

/* mm_struct KDDM set */
struct kddm_set *mm_struct_kddm_set = NULL;



void create_mm_struct_object(struct mm_struct *mm)
{
	struct mm_struct *_mm;

	DEBUG ("mm_struct", 2, "create object for object %p\n", mm);

	BUG_ON(atomic_read(&mm->mm_ltasks) > 1);

	atomic_inc(&mm->mm_users); // Get a reference count for the KDDM.

	mm->mm_id = get_unique_id(&mm_struct_unique_id_root);

	_mm = _kddm_grab_object_manual_ft(mm_struct_kddm_set, mm->mm_id);
	BUG_ON(_mm);
	_kddm_set_object(mm_struct_kddm_set, mm->mm_id, mm);

	krg_put_mm(mm->mm_id);

	DEBUG ("mm_struct", 2, "Object %ld created\n", mm->mm_id);
}



/*****************************************************************************/
/*                                                                           */
/*                                KERNEL HOOKS                               */
/*                                                                           */
/*****************************************************************************/



int kcb_copy_mm(unsigned long clone_flags,
		struct mm_struct *mm,
		struct mm_struct *oldmm)
{
	struct kddm_set *set;

	DEBUG ("mm_struct", 2, "Copy mm %p into %p\n", oldmm, mm);

	mm->mm_id = 0;
	krgnodes_clear (mm->copyset);

	if (oldmm->anon_vma_kddm_set == NULL)
		return 0;

	set = alloc_anon_vma_kddm_set(mm, kerrighed_node_id, /* cow */0);
	if (IS_ERR(set)) {
		BUG();
		return PTR_ERR(set);
	}

	_increment_kddm_set_usage (set);

	set_anon_vma_kddm_set(mm, set);

	create_mm_struct_object(mm);

	DEBUG ("mm_struct", 2, "Copy mm %p into %p: done\n", oldmm, mm);

	return 0;
}



void kcb_do_mmap(struct vm_area_struct *vma)
{
	if (vma->vm_mm->anon_vma_kddm_set)
		check_link_vma_to_anon_memory_kddm_set (vma);
}



void kcb_mm_get(struct mm_struct *mm)
{
	DEBUG ("mm_struct", 2, "get mm %p\n", mm);

	if (!mm)
		return;

	if (!mm->mm_id) {
		atomic_inc (&mm->mm_tasks);
		return;
	}

	krg_grab_mm(mm->mm_id);
	atomic_inc (&mm->mm_tasks);
	krg_put_mm(mm->mm_id);
	DEBUG ("mm_struct", 2, "get mm %p: done\n", mm);
}



void clean_up_mm_struct (struct mm_struct *mm)
{
	DEBUG ("mm_struct", 2, "Clean up mm %p\n", mm);

	/* Take the semaphore to avoid race condition with mm_remove_object */

	down_write(&mm->mmap_sem);
	exit_mmap(mm);
	mm->mmap = NULL;
	mm->mmap_cache = NULL;
	mm->map_count = 0;
	mm->mm_rb = RB_ROOT;
	up_write(&mm->mmap_sem);

	DEBUG ("mm_struct", 2, "Clean up mm %p : done\n", mm);
}



void kcb_mm_release(struct mm_struct *mm, int notify)
{
	DEBUG ("mm_struct", 2, "release mm %p (count %d - tasks %d - users %d)"
	       "\n", mm, atomic_read(&mm->mm_count),
	       atomic_read(&mm->mm_tasks), atomic_read(&mm->mm_users));

	if (!mm)
		return;

	BUG_ON(!mm->mm_id);

	if (!notify) {
		/* Not a real exit: clean up VMAs */
		BUG_ON (atomic_read(&mm->mm_ltasks) != 0);
		clean_up_mm_struct(mm);
		return;
	}

	krg_grab_mm(mm->mm_id);
	atomic_dec (&mm->mm_tasks);

	if (atomic_read(&mm->mm_tasks) == 0) {
		struct kddm_set *set = mm->anon_vma_kddm_set;
		unique_id_t mm_id = mm->mm_id;

		mm->mm_id = 0;

		_kddm_remove_frozen_object(mm_struct_kddm_set, mm_id);
		_destroy_kddm_set(set);
	}

	DEBUG ("mm_struct", 2, "release mm %p done (count %d - tasks %d - "
	       "users %d)\n", mm, atomic_read(&mm->mm_count),
	       atomic_read(&mm->mm_tasks), atomic_read(&mm->mm_users));
}



static void kcb_do_munmap(struct mm_struct *mm,
			  unsigned long start,
			  size_t len,
			  struct vm_area_struct *vma)
{
	struct page *page;
	unsigned long i;

	if ((!anon_vma(vma)) || (!mm->mm_id))
		return;

	for (i = start / PAGE_SIZE;
	     i < (start + len + PAGE_SIZE - 1) / PAGE_SIZE;
	     i++) {
		page = _kddm_grab_object_no_ft (mm->anon_vma_kddm_set, i);
		if (page)
			_kddm_remove_frozen_object(mm->anon_vma_kddm_set, i);
		else
			_kddm_put_object(mm->anon_vma_kddm_set, i);
	}
}



/*****************************************************************************/
/*                                                                           */
/*                              INITIALIZATION                               */
/*                                                                           */
/*****************************************************************************/



void mm_struct_init (void)
{
	init_unique_id_root (&mm_struct_unique_id_root);

	mm_struct_kddm_set = create_new_kddm_set(kddm_def_ns,
						 MM_STRUCT_KDDM_ID,
						 MM_STRUCT_LINKER,
						 KDDM_RR_DEF_OWNER,
						 sizeof (struct mm_struct),
						 KDDM_LOCAL_EXCLUSIVE);

	if (IS_ERR(mm_struct_kddm_set))
		OOM;

	hook_register(&kh_copy_mm, kcb_copy_mm);
	hook_register(&kh_mm_get, kcb_mm_get);
	hook_register(&kh_mm_release, kcb_mm_release);
	hook_register(&kh_do_munmap, kcb_do_munmap);
}



void mm_struct_finalize (void)
{
}
