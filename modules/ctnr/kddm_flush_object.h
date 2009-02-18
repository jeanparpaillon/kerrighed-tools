/** KDDM flush object.
 *  @file kddm_flush_object.h
 *
 *  Definition of KDDM interface.
 *  @author Renaud Lottiaux
 */

#ifndef __KDDM_FLUSH_OBJECT__
#define __KDDM_FLUSH_OBJECT__

#include "kddm_set.h"


/*--------------------------------------------------------------------------*
 *                                                                          *
 *                              EXTERN FUNCTIONS                            *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/** Flush an object from local memory */
int kddm_flush_object(struct kddm_ns *ns, kddm_set_id_t set_id, objid_t objid,
		      kerrighed_node_t dest);

int _kddm_flush_object(struct kddm_set *set, objid_t objid,
		       kerrighed_node_t dest);

#endif