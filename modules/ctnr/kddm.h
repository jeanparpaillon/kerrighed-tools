/** KDDM interface.
 *  @file kddm.h
 *
 *  Definition of KDDM interface.
 *  @author Renaud Lottiaux
 */

#ifndef __KDDM__
#define __KDDM__

#include "kddm_types.h"
#include "io_linker.h"
#include "object.h"
#include "kddm_set.h"
#include "kddm_find_object.h"
#include "kddm_put_object.h"
#include "kddm_get_object.h"
#include "kddm_grab_object.h"
#include "kddm_set_object.h"
#include "kddm_flush_object.h"
#include "kddm_remove_object.h"
#include "kddm_sync_object.h"



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                             MACRO CONSTANTS                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/



/** Print an error message concerning a problem in the state machine */
#define STATE_MACHINE_ERROR(set_id, objid, obj_entry) \
{ \
  if (OBJ_STATE_INDEX(OBJ_STATE(obj_entry)) < NB_OBJ_STATE) \
    PANIC ("Receive a object on %s object (%ld;%ld) \n", \
	   STATE_NAME(OBJ_STATE(obj_entry)), set_id, objid) ; \
  else \
    PANIC( "Object (%ld;%ld) : unknown object state\n", set_id, objid) ; \
}



/*--------------------------------------------------------------------------*
 *                                                                          *
 *                            EXTERN VARIABLES                              *
 *                                                                          *
 *--------------------------------------------------------------------------*/



extern event_counter_t total_get_object_counter;
extern event_counter_t total_grab_object_counter;
extern event_counter_t total_remove_object_counter;
extern event_counter_t total_flush_object_counter;



/*********************** KDDM set Counter tools ************************/

int initialize_kddm_info_struct (struct task_struct *task);


static inline void inc_get_object_counter(struct kddm_set *set)
{
	total_get_object_counter++;
	set->get_object_counter++;
	if (!current->kddm_info)
		initialize_kddm_info_struct(current);
	current->kddm_info->get_object_counter++;
}

static inline void inc_grab_object_counter(struct kddm_set *set)
{
	total_grab_object_counter++;
	set->grab_object_counter++;
	if (!current->kddm_info)
		initialize_kddm_info_struct(current);
	current->kddm_info->grab_object_counter++;
}

static inline void inc_remove_object_counter(struct kddm_set *set)
{
	total_remove_object_counter++;
	set->remove_object_counter++;
	if (!current->kddm_info)
		initialize_kddm_info_struct(current);
	current->kddm_info->remove_object_counter++;
}

static inline void inc_flush_object_counter(struct kddm_set *set)
{
	total_flush_object_counter++;
	set->flush_object_counter++;
	if (!current->kddm_info)
		initialize_kddm_info_struct(current);
	current->kddm_info->flush_object_counter++;
}

#endif
