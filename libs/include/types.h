/** Main kerrighed types.
 *  @file gtypes.h
 *
 *  Definition of the main types and structures.
 *  @author Renaud Lottiaux
 */

#ifndef __KERRIGHED_TYPES__
#define __KERRIGHED_TYPES__

/*--------------------------------------------------------------------------*
 *                                                                          *
 *                                  TYPES                                   *
 *                                                                          *
 *--------------------------------------------------------------------------*/

/** Type for node id           */
typedef short kerrighed_node_t ;

/** Boolean type */
typedef unsigned int bool_t ;

/** Event counter type */
typedef unsigned long event_counter_t ;

/** Physical address type */
typedef unsigned long physaddr_t ;

/** Network id */
typedef unsigned int kerrighed_network_t;

enum kerrighed_status {
  KRG_FIRST_START,
  KRG_FINAL_STOP,
  KRG_NODE_STARTING,
  KRG_NODE_STOPING,
  KRG_RUNNING_CLUSTER
};
typedef enum kerrighed_status kerrighed_status_t;


/*--------------------------------------------------------------------------*
 *                                                                          *
 *                             EXTERN VARIABLES                             *
 *                                                                          *
 *--------------------------------------------------------------------------*/

#endif /* __KERRIGHED_TYPES__*/
