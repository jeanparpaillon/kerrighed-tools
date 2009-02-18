/** Memory ghost interface
 *  @file memory_ghost.h
 *
 *  Definition of memory ghost structures and functions.
 *  @author Matthieu Fertré
 */

#ifndef __MEMORY_GHOST__
#define __MEMORY_GHOST__

#include <ctnr/kddm_types.h>


/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/** Memory ghost private data
 */
typedef struct memory_ghost_data {
	kddm_set_id_t ctnr_id;          /**< Memory to save/load data to/from */
	loff_t offset;
	long app_id;
	int chkpt_sn;
	int obj_id;
	char label[16];
} memory_ghost_data_t ;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/

/** Create a new memory ghost.
 *  @author Matthieu Fertré, Renaud Lottiaux
 *
 *  @param  access   Ghost access (READ/WRITE)
 *
 *  @return        ghost_t if everything ok
 *                 NULL otherwise.
 */
ghost_t *create_memory_ghost ( int access,
			       long app_id,
			       int chkpt_sn,
			       int obj_id,
			       const char * label);

/** Delete a ghost memory
 *  @author Matthieu Fertré
 *
 *  @param  ghost    Ghost memory to delete
 */
void delete_memory_ghost(ghost_t *ghost);


#endif // __MEMORY_GHOST__
