/** KDDM memory IO linker.
 *  @file memory_io_linker.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 */

#include <linux/mm.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/string.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/rmap.h>
#include <linux/pagemap.h>
#include <linux/mm_inline.h>
#include <asm/tlbflush.h>

#include <rpc/rpc.h>
#include <ctnr/kddm.h>
#include "memory_io_linker.h"
#include "memory_int_linker.h"

#include "debug_kermm.h"



/*****************************************************************************/
/*                                                                           */
/*                       MEMORY KDDM SET IO FUNCTIONS                       */
/*                                                                           */
/*****************************************************************************/



/** Allocate an object
 *  @author Renaud Lottiaux
 */
int memory_alloc_object (struct kddm_obj * obj_entry,
			 struct kddm_set * set,
			 objid_t objid)
{
	obj_entry->object = alloc_page (GFP_HIGHUSER);
	if (obj_entry->object == NULL)
		return -ENOMEM;

	return 0;
}



/** Import an object
 *  @author Renaud Lottiaux
 *
 *  @param  object    The object to import data in.
 *  @param  buffer    Data to import in the object.
 */
int memory_import_object (struct kddm_obj *obj_entry,
                          struct rpc_desc *desc)
{
	struct page *page = obj_entry->object;
	char *data;

	data = (char *)kmap(page);
	rpc_unpack(desc, 0, data, PAGE_SIZE);
	kunmap(page);

//	copy_buff_to_highpage ((struct page *) obj_entry->object, buffer);
	return 0;
}



/** Export an object
 *  @author Renaud Lottiaux
 *
 *  @param  buffer    Buffer to export object data in.
 *  @param  object    The object to export data from.
 */
int memory_export_object (struct rpc_desc *desc,
                          struct kddm_obj *obj_entry)
{
	struct page *page = (struct page *)obj_entry->object;
	char *data;
	
	data = (char *)kmap_atomic(page, KM_USER0);
	rpc_pack(desc, 0, data, PAGE_SIZE);
	kunmap_atomic(data, KM_USER0);

//	copy_highpage_to_buff (buffer, (struct page *) obj_entry->object);
	return 0;
}



/** Handle a kddm set memory page first touch
 *  @author Renaud Lottiaux
 *
 *  @param  obj_entry  Kddm Set page descriptor.
 *  @param  set        Kddm Set descriptor
 *  @param  objid      Id of the page to create.
 *
 *  @return  0 if everything is ok. Negative value otherwise.
 */
int memory_first_touch (struct kddm_obj * obj_entry,
                        struct kddm_set * set,
                        objid_t objid,
			int flags)
{
	int res = 0;
	struct page *page;
	
	if (obj_entry->object == NULL) {
		page = alloc_page (GFP_HIGHUSER | __GFP_ZERO);

		if (page == NULL)
			res = -ENOMEM;
		
		obj_entry->object = page;
	}
	
	return res;
}



/** Insert a new kddm set page in the file cache.
 *  @author Renaud Lottiaux
 *
 *  @param  obj_entry  Descriptor of the page to insert.
 *  @param  set        Kddm Set descriptor
 *  @param  padeid     Id of the page to insert.
 */
int memory_insert_page (struct kddm_obj * obj_entry,
                        struct kddm_set * set,
                        objid_t objid)
{
	struct page *page;

	DEBUG ("io_linker", 3, "Insert page %p (%ld;%ld)\n",
	       obj_entry->object, set->id, objid);

	page = obj_entry->object;
	page->index = objid;
	
	return 0;
}



/** Invalidate a kddm set memory page.
 *  @author Renaud Lottiaux
 *
 *  @param  set      Kddm Set descriptor
 *  @param  objid    Id of the page to invalidate
 */
int memory_invalidate_page (struct kddm_obj * obj_entry,
                            struct kddm_set * set,
                            objid_t objid)
{
	int res ;
	
	DEBUG ("io_linker", 3, "Invalidate page (%ld;%ld)\n", 
	       set->id, objid);

	if (obj_entry->object != NULL) {
		struct page *page = (struct page *) obj_entry->object;

		DEBUG ("io_linker", 3, "Page (%ld;%ld) (count = %d;%d)"
		       " - flags : %#016lx\n", set->id, objid,
		       page_count (page), page_mapcount(page), page->flags);

		BUG_ON (TestSetPageLocked(page));
		
		if (page_mapped(page)) {
			if (page->mapping == NULL) {
				printk ("Try to unmap a not mapped page\n");
				while(1) schedule();
			}

			SetPageToInvalidate(page);
			res = try_to_unmap(page, 0);
			ClearPageToInvalidate(page);
		}
		
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

		if (PageAnon(page))
			page->mapping = NULL;

		if ((page_count (page) != obj_entry->countx + extra_count)) {
			WARNING ("Hum... page %p (%ld;%ld) has count %d;%d "
				 "(against %d)\n", page, set->id, objid,
				 page_count (page), page_mapcount(page),
				 obj_entry->countx + extra_count);
		}
		
		if (PageActive(page)) {
			WARNING ("Hum. page %p (%ld;%ld) has Active bit set\n",
				 page, set->id, objid);
			while (1)
				schedule();
		}
#endif
	}
	
	DEBUG ("io_linker", 3, "Invalidate page (%ld;%ld) : done\n",
	       set->id, objid);
	
	return 0;
}



void memory_change_state (struct kddm_obj * obj_entry,
			  struct kddm_set * set,
			  objid_t objid,
			  kddm_obj_state_t state)
{
	struct page *page = obj_entry->object;
	
	if (page == NULL)
		return ;
	
	DEBUG ("io_linker", 4, "Change acces on page (%ld;%ld) at %p "
	       "(count %d;%d) - mapping %p (anon : %d) to %s\n", set->id,
	       objid, page, page_count (page), page_mapcount(page),
	       page->mapping, PageAnon(page), STATE_NAME(state));

	switch (state) {
	  case READ_COPY :
	  case READ_OWNER :
		  BUG_ON (TestSetPageLocked(page));
		  
		  if (page_mapped(page)) {
			  if (page->mapping == NULL) {
				  printk ("Try to unmap a not mapped page\n");
				  while(1) schedule();
			  }
			  SetPageToSetReadOnly(page);
			  try_to_unmap(page, 0);
			  ClearPageToSetReadOnly(page);
		  }

		  unlock_page(page);
		  break ;

	  default:
		  break ;
	}

	DEBUG ("io_linker", 4, "Change acces on page (%ld;%ld) at %p "
	       "(count %d;%d) - mapping %p (anon : %d) to %s: done\n",
	       set->id, objid, page, page_count (page),
	       page_mapcount(page), page->mapping,
	       PageAnon(page), STATE_NAME(state));
}



/** Handle a kddm set memory page remove.
 *  @author Renaud Lottiaux
 *
 *  @param  set      Kddm Set descriptor
 *  @param  padeid   Id of the page to remove
 */
int memory_remove_page (void *object,
                        struct kddm_set * set,
                        objid_t objid)
{
	DEBUG ("io_linker", 3, "remove page (%ld;%ld)\n", set->id,
	       objid);
	
	if (object != NULL)
		page_cache_release ((struct page *) object);
	
	DEBUG ("io_linker", 3, "remove page (%ld;%ld) : done\n",
	       set->id, objid);

	return 0;
}



/****************************************************************************/

/* Init the memory IO linker */

struct iolinker_struct memory_linker = {
	first_touch:       memory_first_touch,
	remove_object:     memory_remove_page,
	invalidate_object: memory_invalidate_page,
	change_state:      memory_change_state,
	linker_name:       "mem ",
	linker_id:         MEMORY_LINKER,
	alloc_object:      memory_alloc_object,
	export_object:     memory_export_object,
	import_object:     memory_import_object,
};
