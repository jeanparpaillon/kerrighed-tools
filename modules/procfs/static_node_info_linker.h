/** Static node informations management.
 *  @file static_node_info_linker.h
 *
 *  @author Renaud Lottiaux
 */

#ifndef STATIC_NODE_INFO_LINKER_H
#define STATIC_NODE_INFO_LINKER_H

#include <ctnr/kddm.h>


/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/* Static node informations */

typedef struct {
	int nr_cpu;		/* Number of CPU on the node */
	unsigned long totalram;	/* Total usable main memory size */
	unsigned long totalhigh;	/* Total high memory size */
} krg_static_node_info_t;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                            EXTERN VARIABLES                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/



extern struct iolinker_struct static_node_info_io_linker;
extern struct kddm_set *static_node_info_ctnr;



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/



int init_static_node_info_ctnr(void);

/** Helper function to get static node informations.
 *  @author Renaud Lottiaux
 *
 *  @param node_id   Id of the node we want informations on.
 *
 *  @return  Structure containing information on the requested node.
 */
static inline krg_static_node_info_t *get_static_node_info(int node_id)
{
	return _kddm_get_object_no_lock(static_node_info_ctnr, node_id);
}

#endif // STATIC_NODE_INFO_LINKER_H
