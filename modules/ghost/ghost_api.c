/** Ghost interface.
 *  @file ghost_api.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#include <linux/slab.h>
#include <kerrighed/unique_id.h>

#define MODULE_NAME "GHOST API "
#include "debug_ghost.h"

#ifndef FILE_NONE
#  if defined(FILE_GHOST_API) || defined(FILE_ALL)
#     define DEBUG_THIS_MODULE
#  endif
#endif

#include "ghost.h"


static struct kmem_cache *ghost_cachep;
static unique_id_root_t ghost_unique_id_root;


/** Create a new ghost struct. */

ghost_t *create_ghost ( ghost_type_t type, int access )
{
  ghost_t *ghost ;

  ghost = kmem_cache_alloc(ghost_cachep, GFP_KERNEL);
  if (ghost == NULL)
    goto outofmemory ;

  ghost->type = type ;
  ghost->ghost_id = get_unique_id (&ghost_unique_id_root) ;
  ghost->size = 0 ;
  ghost->ops = NULL ;
  ghost->data = NULL ;
  ghost->access = access ;

  return ghost ;

 outofmemory:
  return ERR_PTR(-ENOMEM);
}



/** Free ghost data structures. */

int free_ghost (ghost_t *ghost)
{
  if (ghost->data != NULL)
    kfree (ghost->data);

  kmem_cache_free(ghost_cachep, ghost);

  return 0 ;
}

/*****************************************************************************/
/*                                                                           */
/*                              INITIALIZATION                               */
/*                                                                           */
/*****************************************************************************/



/* Ghost initialisation.*/

int nazgul_ghost_init()
{
  unsigned long cache_flags = SLAB_PANIC;

  DEBUG (DEBUG_GHOST_API, 1,"Ghost init\n");

#ifdef CONFIG_DEBUG_SLAB
  cache_flags |= SLAB_POISON;
#endif
  ghost_cachep = kmem_cache_create("ghost",
				   sizeof(ghost_t),
				   0, cache_flags,
				   NULL, NULL);
  init_unique_id_root(&ghost_unique_id_root);

  DEBUG (DEBUG_GHOST_API, 1,"Ghost init : done\n");

  return 0;
}

/* Ghost Finalization. */

void nazgul_ghost_finalize()
{
}
