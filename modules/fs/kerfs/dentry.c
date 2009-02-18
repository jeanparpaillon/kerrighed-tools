/** Container File System dentry management.
 *  @file dentry.c
 *  
 *  @author Renaud Lottiaux 
 */

#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/module.h>

#include "debug_kerfs.h"

#define MODULE_NAME "Ctnr FS dentry   "

#ifdef KERFS_DENTRY_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <ctnr/container.h>
#include <ctnr/container_mgr.h>
#include "inode.h"
#include "dentry.h"
#include "super.h"



/*****************************************************************************/
/*                                                                           */
/*                         DENTRY OPERATION FUNCTIONS                        */
/*                                                                           */
/*****************************************************************************/



static int kerfs_lookup_revalidate (struct dentry *dentry,
                                    struct nameidata *nd)
{
  return 1;
}



struct dentry_operations kerfs_dentry_operations = {
  d_revalidate: kerfs_lookup_revalidate,
};
