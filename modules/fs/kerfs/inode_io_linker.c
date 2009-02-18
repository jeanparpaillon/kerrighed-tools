/** Container inode IO linker.
 *  @file inode_io_linker.c
 *
 *  @author Renaud Lottiaux 
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/string.h>

#include "debug_kerfs.h"

#define MODULE_NAME "Inode IO linker    "

#ifdef INODE_IO_LINKER_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <tools/kerrighed.h>
#include <ctnr/container.h>
#include <ctnr/container_mgr.h>
#include "inode_io_linker.h"
#include "super.h"
#include "inode.h"



/*****************************************************************************/
/*                                                                           */
/*                       INODE CONTAINER IO FUNCTIONS                        */
/*                                                                           */
/*****************************************************************************/



/** Allocate an object
 *  @author Renaud Lottiaux
 */
void *inode_alloc_object (ctnrObj_t * objEntry,
                          container_t * ctnr,
                          objid_t objid)
{
  struct inode *inode;

  DEBUG (2, "Alloc inode (%ld;%ld)\n", ctnr->ctnrid, objid);

  inode = ilookup (kerfs_sb, objid);

  if (inode == NULL)
    PANIC ("Cannot find inode %ld\n", objid);

  return inode;
}



/** Import an object
 *  @author Renaud Lottiaux
 *
 *  @param  object    The object to import data in.
 *  @param  buffer    Data to import in the object.
 */
int inode_import_object (ctnrObj_t * objEntry,
                         char *buffer, struct gimli_desc *desc)
{
  struct inode *inode_dst = objEntry->object;
  struct inode *inode_src = (struct inode *) buffer;

  kerfs_copy_inode (inode_dst, inode_src);
  return 0;
}



/** Export an object
 *  @author Renaud Lottiaux
 *
 *  @param  buffer    Buffer to export object data in.
 *  @param  object    The object to export data from.
 */
int inode_export_object (char *buffer, struct gimli_desc *desc,
                         ctnrObj_t * objEntry)
{
  struct inode *inode_dst = (struct inode *) buffer;
  struct inode *inode_src = objEntry->object;

  kerfs_copy_inode (inode_dst, inode_src);
  return 0;
}



/** Handle a container inode first touch
 *  @author Renaud Lottiaux
 *
 *  @param  ctnr       Container descriptor
 *  @param  objid     Id of the page to create.
 *  @param  objEntry  Container page descriptor.
 *
 *  @return  0 if everything is ok. Negative value otherwise.
 */
int inode_first_touch (ctnrObj_t * objEntry,
                       container_t * ctnr,
                       objid_t objid)
{
  DEBUG (2, "First touch inode (%ld;%ld)\n", ctnr->ctnrid, objid);

  if (objEntry->object == NULL)
    objEntry->object = inode_alloc_object (objEntry, ctnr, objid);

  return 0;
}



/** Invalidate a container inode object.
 *  @author Renaud Lottiaux
 *
 *  @param  ctnr      Container descriptor
 *  @param  objid    Id of the page to invalidate
 */
int inode_invalidate_object (ctnrObj_t * objEntry,
                             container_t * ctnr,
                             objid_t objid)
{
  struct inode *inode;

  DEBUG (2, "Invalidate inode (%ld;%ld)\n", ctnr->ctnrid, objid);

  inode = ilookup (kerfs_sb, objid);
  if (inode == NULL)
    PANIC ("Cannot find inode %ld\n", objid);

  if (inode != objEntry->object)
    PANIC ("Invalid inode %ld - %p/%p\n", objid, inode, objEntry->object);

  if (inode->i_state & I_DIRTY_SYNC)
    {
      inode->i_state = inode->i_state & (~I_DIRTY_SYNC);
      if (!(inode->i_state & I_DIRTY))
        force_clean_inode (inode);
    }

  iput (inode);

  DEBUG (2, "Invalidate inode (%ld;%ld) : done\n", ctnr->ctnrid, objid);

  return 0;
}



/** Receive a fresh copy of an inode. Update the inode struct.
 *  @author Renaud Lottiaux
 *
 *  @param  objEntry  Descriptor of the page to insert.
 *  @param  ctnr       Container descriptor
 *  @param  padeid     Id of the page to insert.
 */
int inode_insert_object (ctnrObj_t * objEntry,
                         container_t * ctnr,
                         objid_t objid)
{
  struct inode *inode;

  DEBUG (2, "Insert inode (%ld;%ld)\n", ctnr->ctnrid, objid);

  inode = ilookup (kerfs_sb, objid);

  if (inode == NULL)
    PANIC ("Cannot find inode %ld\n", objid);

  if (inode != objEntry->object)
    PANIC ("Invalid inode %ld - %p/%p\n", objid, inode, objEntry->object);

  iput (inode);

  DEBUG (2, "Insert inode (%ld;%ld) : done\n", ctnr->ctnrid, objid);

  return 0;
}



/** Release an inode struct. Update the container inode data.
 *  @author Renaud Lottiaux
 *
 *  @param  objEntry  Descriptor of the page to insert.
 *  @param  ctnr       Container descriptor
 *  @param  padeid     Id of the page to insert.
 */
int inode_put_object (ctnrObj_t * objEntry,
                      container_t * ctnr,
                      objid_t objid)
{
  struct inode *inode;

  ASSERT (ctnr != NULL);

  DEBUG (2, "Put inode (%ld;%ld)\n", ctnr->ctnrid, objid);

  ASSERT (objEntry != NULL);

  inode = ilookup (kerfs_sb, objid);

  if (inode == NULL)
    {
      //      WARNING ("Cannot find inode %ld\n", objid);
      return 0;
    }

  if (inode != objEntry->object)
    PANIC ("Invalid inode %ld - %p/%p\n", objid, inode, objEntry->object);

  if (OBJ_STATE(objEntry) == WRITE_OWNER)
    mark_inode_dirty (inode);

  iput (inode);

  DEBUG (2, "Put inode (%ld;%ld) : done\n", ctnr->ctnrid, objid);

  return 0;
}



/****************************************************************************/

/* Init the inode IO linker */

struct iolinker_struct inode_io_linker = {
  first_touch:       inode_first_touch,
  invalidate_object: inode_invalidate_object,
  insert_object:     inode_insert_object,
  put_object:        inode_put_object,
  linker_name:       "ino ",
  linker_id:         INODE_LINKER,
  alloc_object:      inode_alloc_object,
  export_object:     inode_export_object,
  import_object:     inode_import_object
};
