/** MM Struct Linker.
 *  @file mm_struct_io_linker.c
 *
 *  Copyright (C) 2008-2009, Renaud Lottiaux, Kerlabs.
 */
#include <linux/rmap.h>
#include <rpc/rpc.h>
#include <ctnr/kddm.h>

#include "mobility.h"
#include "mm_struct.h"



/*****************************************************************************/
/*                                                                           */
/*                       MM_STRUCT KDDM SET IO FUNCTIONS                     */
/*                                                                           */
/*****************************************************************************/



int mm_alloc_object (struct kddm_obj *obj_entry,
		     struct kddm_set *set,
		     objid_t objid)
{
	struct mm_struct *mm = NULL;

	mm = allocate_mm();
	if (!mm)
		return -ENOMEM;

	mm->first_import = 1;

	obj_entry->object = mm;

	return 0;
}



int mm_first_touch (struct kddm_obj *obj_entry,
		    struct kddm_set *set,
		    objid_t objid,
		    int flags)
{
	/* Should never be called */
	BUG();

	return 0;
}



int mm_remove_object (void *object,
		      struct kddm_set *set,
		      objid_t objid)
{
	struct mm_struct *mm = object;

	/* Take the mmap_sem to avoid race condition with clean_up_mm_struct */

	atomic_inc(&mm->mm_count);
	down_write(&mm->mmap_sem);

	mmput(mm);

	up_write(&mm->mmap_sem);

#ifdef CONFIG_KRG_DEBUG
	/* In case of a migrated process exiting before the source process
	 * has exited, the following BUG_ON is triggered off. However, in
	 * this very unlikely case, this is not a bug !
	 * These BUG_ON are there just to help debugging the common cases.
	 */
	BUG_ON(atomic_read(&mm->mm_ltasks) != 0);
	BUG_ON(atomic_read(&mm->mm_users) != 0);
#endif
	mm->mm_id = 0;

	mmdrop(mm);

	return 0;
}



/** Export an MM struct
 *  @author Renaud Lottiaux
 *
 *  @param  buffer    Buffer to export object data in.
 *  @param  obj_entry  Object entry of the object to export.
 */
int mm_export_object (struct rpc_desc *desc,
		      struct kddm_obj *obj_entry)
{
	struct mm_struct *mm;
	krgsyms_val_t unmap_id, get_unmap_id;

	mm = obj_entry->object;

	krgnode_set (desc->client, mm->copyset);

	rpc_pack(desc, 0, mm, sizeof(struct mm_struct));

	get_unmap_id = krgsyms_export(mm->get_unmapped_area);
	rpc_pack_type(desc, get_unmap_id);

	unmap_id = krgsyms_export(mm->unmap_area);
	rpc_pack_type(desc, unmap_id);

	return 0;
}



/** Import an MM struct
 *  @author Renaud Lottiaux
 *
 *  @param  obj_entry  Object entry of the object to import.
 *  @param  _buffer   Data to import in the object.
 */
int mm_import_object (struct kddm_obj *obj_entry,
		      struct rpc_desc *desc)
{
	struct mm_struct *mm, buffer;
	krgsyms_val_t unmap_id, get_unmap_id;
	struct kddm_set *set;
	int r;

	mm = obj_entry->object;

	if (mm->first_import) {
		r = rpc_unpack (desc, 0, mm, sizeof(struct mm_struct));
		if (r)
			return r;

		if (mm->anon_vma_kddm_id == KDDM_SET_UNUSED) {
			set = NULL;
			BUG(); /* Is this case still possible ??? */
		}

		set = _find_get_kddm_set (kddm_def_ns,
					  mm->anon_vma_kddm_id);
		BUG_ON (set == NULL);

		mm->anon_vma_kddm_set = NULL;
		r = reinit_mm(mm);
		if (r)
			return r;
		atomic_set(&mm->mm_ltasks, 0);

		put_kddm_set(set);

		set_anon_vma_kddm_set(mm, set);
	}
	else {
		r = rpc_unpack (desc, 0, &buffer, sizeof(struct mm_struct));
		if (r)
			return r;

		mm->mm_tasks = buffer.mm_tasks;
	}

	r = rpc_unpack_type(desc, get_unmap_id);
	if (r)
		return r;
	mm->get_unmapped_area = krgsyms_import (get_unmap_id);

	r = rpc_unpack_type(desc, unmap_id);
	if (r)
		return r;
	mm->unmap_area = krgsyms_import (unmap_id);

	return 0;
}



/****************************************************************************/

/* Init the mm_struct IO linker */

struct iolinker_struct mm_struct_io_linker = {
	alloc_object:      mm_alloc_object,
	first_touch:       mm_first_touch,
	export_object:     mm_export_object,
	import_object:     mm_import_object,
	remove_object:     mm_remove_object,
	linker_name:       "MM ",
	linker_id:         MM_STRUCT_LINKER,
};
