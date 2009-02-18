/** Container struct IO linker.
 *  @file file_struct_io_linker.c
 *
 *  @author Renaud Lottiaux 
 */

#include <linux/mm.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/string.h>
#include <linux/slab.h>

#include "debug_kerfs.h"

#define MODULE_NAME "File Struct IO linker "

#ifdef STRUCT_IO_LINKER_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <tools/kerrighed.h>
#include <ctnr/container.h>
#include <ctnr/container_mgr.h>
#include "file_struct_io_linker.h"



/*****************************************************************************/
/*                                                                           */
/*                     FILE_STRUCT CONTAINER IO FUNCTIONS                    */
/*                                                                           */
/*****************************************************************************/



/** Handle a container file_struct object first touch
 *  @author Renaud Lottiaux
 *
 *  @param  ctnr       Container descriptor
 *  @param  objid     Id of the object to create.
 *  @param  objEntry  Container object descriptor.
 *
 *  @return  0 if everything is ok. Negative value otherwise.
 */
int file_struct_first_touch (ctnrObj_t * objEntry,
                             container_t * ctnr,
                             objid_t objid)
{
  int res = 0;

  if (objEntry->object == NULL)
    objEntry->object = alloc_page (GFP_KERNEL);
  if (objEntry->object == NULL)
    res = -ENOMEM;

  return res;
}



/****************************************************************************/

/* Init the file_struct IO linker */

struct iolinker_struct file_struct_io_linker = {
  first_touch:   file_struct_first_touch,
  linker_name:   "str ",
  linker_id:     FILE_STRUCT_LINKER,
};
