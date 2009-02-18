/** Container File System address space management.
 *  @file address_space.c
 *  
 *  @author Renaud Lottiaux 
 */

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/pagemap.h>

#include "debug_kerfs.h"

#define MODULE_NAME "Ctnr FS AS      "

#ifdef KERFS_ADDRESS_SPACE_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <tools/debug.h>
#include <ctnr/container.h>
#include <ctnr/container_mgr.h>
#include "file_io_linker.h"
#include <ctnr/object_server.h>
#include <ctnr/protocol_action.h>



static int kerfs_readpage (struct file *file,
                           struct page *page)
{
  container_t *ctnr = file->f_dentry->d_inode->i_mapping->a_ctnr;

  DEBUG (2, "KERFS : readpage (%ld;%ld)\n", ctnr->ctnrid, page->index);

  if (!PageLocked (page))
    PAGE_BUG (page);

  ASSERT (ctnr != NULL);
  ASSERT (page->mapping != NULL);
  ASSERT (page->mapping->a_ctnr != NULL);
  ASSERT (page->mapping->a_ctnr->ctnrid == ctnr->ctnrid);

  __async_ctnr_get_object (ctnr->ctnrid, page->index, page);

  DEBUG (2, "KERFS : readpage (%ld;%ld) : done\n", ctnr->ctnrid, page->index);

  return 0;
}



static int kerfs_writepage (struct page *page, struct writeback_control *wbc)
{
  container_t *ctnr = page->mapping->a_ctnr;
  struct page *ctnr_page;

  DEBUG (2, "KERFS : writepage (%ld;%ld)\n", ctnr->ctnrid, page->index);

  ctnr_page = ctnr_get_object (ctnr->ctnrid, page->index);

  ctnr_sync_object (ctnr->ctnrid, page->index);

  ctnr_put_object (ctnr->ctnrid, page->index);

  unlock_page(page);

  DEBUG (2, "KERFS : writepage (%ld;%ld) : done\n", ctnr->ctnrid, page->index);

  return 0;
}


static int kerfs_prepare_write (struct file *file,
                                struct page *page,
                                unsigned offset,
                                unsigned _to)
{
  container_t *ctnr = file->f_dentry->d_inode->i_mapping->a_ctnr;
  struct page *ctnr_page;

  DEBUG (2, "KERFS : prepare_write of page (%ld;%ld) (from %d to %d)"
         "(count = %d)\n",
         ctnr->ctnrid, page->index, offset, _to, page_count(page));

  ASSERT (page->mapping != NULL);

  if (!PageLocked (page))
    PAGE_BUG (page);

  ctnr_page = __ctnr_grab_object (ctnr->ctnrid, page->index, page);

  if (ctnr_page != page)
    PANIC ("Page (%ld;%ld) : bad physical page\n", ctnr->ctnrid, page->index);

  ASSERT (ctnr_page != NULL);
  ASSERT (ctnr_page->mapping != NULL);

  if (!PageLocked (page))
    lock_page(page);

  set_page_dirty (page);

  DEBUG (2, "KERFS : prepare_write for page (%ld;%ld) (count = %d) : done\n",
         ctnr->ctnrid, page->index, page_count(page));

  return 0;
}



static int kerfs_commit_write (struct file *file,
                               struct page *page,
                               unsigned offset,
                               unsigned _to)
{
  container_t *ctnr = file->f_dentry->d_inode->i_mapping->a_ctnr;
  struct inode *inode = page->mapping->host;
  loff_t pos = ((loff_t) page->index << PAGE_CACHE_SHIFT) + _to;

  DEBUG (2, "KERFS : commit_write for page (%ld;%ld) (from %d to %d)"
         " (count = %d)\n",
         ctnr->ctnrid, page->index, offset, _to, page_count(page));

  if (pos > inode->i_size)
    {
      inode->i_size = pos;
      update_physical_inode_attr (ctnr, inode, ATTR_SIZE);
    }

  ctnr_sync_object(ctnr->ctnrid, page->index);
  clear_page_dirty(page) ;
  ctnr_put_object (ctnr->ctnrid, page->index);

  if (!PageLocked (page))
    PAGE_BUG (page);

  DEBUG (2, "KERFS : commit_write for page (%ld;%ld) (count = %d) : done\n",
         ctnr->ctnrid, page->index, page_count(page));

  return 0;
}



void kerfs_removepage (struct page *page)
{
  ctnrObj_t *objEntry;
  container_t *ctnr;

  ASSERT (page->mapping != NULL);
  ASSERT (page->mapping->a_ctnr != NULL);

  ctnr = page->mapping->a_ctnr;
  objEntry = __get_ctnr_object_entry (ctnr, page->index);

  DEBUG (2, "KERFS : remove page (%ld;%ld)\n", ctnr->ctnrid, page->index);

  if (!object_frozen (objEntry, ctnr))  /* If the page removal come from the kernel,
                                         * not from Kerrighed, the Frozen flag is not.
                                         * Set. In this case, we have to invalidate the
                                         * local copy.
                                         */
    ctnr_remove_object (ctnr->ctnrid, page->index);
  else
    DEBUG (2, "KERFS : remove page (%ld;%ld) : loopback call... "
           "Nothing to do\n", ctnr->ctnrid, page->index);
}



struct address_space_operations kerfs_aops = {
  readpage:      kerfs_readpage,
  writepage:     kerfs_writepage,
  prepare_write: kerfs_prepare_write,
  commit_write:  kerfs_commit_write,
  //  removepage:    kerfs_removepage,
};
