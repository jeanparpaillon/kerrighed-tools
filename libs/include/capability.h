/** Capabilities related interface functions.
 *  @file libiluvatar.h
 *  
 *  @author David Margery
 */
#ifndef LIBILUVATAR_H
#define LIBILUVATAR_H

#include <sys/types.h>
#include "capabilities.h"

int krg_father_capset (const krg_cap_t * new_caps) ;
int krg_father_capget (krg_cap_t * old_caps) ;

int krg_capset (const krg_cap_t * new_caps) ;
int krg_capget (krg_cap_t * old_caps) ;

int krg_pid_capset (pid_t pid, const krg_cap_t * new_caps) ;
int krg_pid_capget (pid_t pid, krg_cap_t * old_caps) ;

int krg_cap_geteffective (const krg_cap_t * cap) ;
int krg_cap_getpermitted (const krg_cap_t * cap) ;
int krg_cap_getinheritable_permitted (const krg_cap_t * cap) ;
int krg_cap_getinheritable_effective (const krg_cap_t * cap) ;

#define cap_raise(cap_vector, cap) (cap_vector |= (1<<cap))
#define cap_raised(cap_vector, cap) (cap_vector & (1<<cap))

int krg_cap_get_supported (int *set) ;

#endif /* LIBILUVATAR_H */
