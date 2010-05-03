/** Capabilities related interface functions.
 *  @file libiluvatar.c
 *  
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#include <sys/types.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>

#include <types.h>
#include <krgnodemask.h>

#include <capabilities.h>

#include <kerrighed_tools.h>


/*****************************************************************************/
/*                                                                           */
/*                              EXPORTED FUNCTIONS                           */
/*                                                                           */
/*****************************************************************************/



/** User interface to set capabilities for the current process */

int krg_capset (const krg_cap_t * new_caps)
{
  return call_kerrighed_services(KSYS_SET_CAP,
				 (void *)new_caps) ;
}



/** User interface to get capabilities for the current process */

int krg_capget (krg_cap_t * old_caps)
{
  return call_kerrighed_services(KSYS_GET_CAP,
				 old_caps) ;
}


/** User interface to set capabilities for a given process */
int krg_pid_capset (pid_t pid, const krg_cap_t * new_caps) 
{
  krg_cap_pid_t desc ;
  desc.pid = pid ;
  desc.caps = (krg_cap_t *)new_caps ;
  return call_kerrighed_services(KSYS_SET_PID_CAP,
				 &desc) ;
}


/** User interface to get capabilities for a given process */
int krg_pid_capget (pid_t pid, krg_cap_t * old_caps) 
{
  krg_cap_pid_t desc ;
  desc.pid = pid ;
  desc.caps = old_caps ;
  return call_kerrighed_services(KSYS_GET_PID_CAP,
				 &desc) ;
  
}

/**
 * Functions to access krg_cap_t struct members
 */
int krg_cap_geteffective (const krg_cap_t * cap) 
{
  return cap->krg_cap_effective;
}

int krg_cap_getpermitted (const krg_cap_t * cap)
{
  return cap->krg_cap_permitted;
}

int krg_cap_getinheritable_permitted (const krg_cap_t * cap) 
{
  return cap->krg_cap_inheritable_permitted;
}

int krg_cap_getinheritable_effective (const krg_cap_t * cap) 
{
  return cap->krg_cap_inheritable_effective;
}

/* Get the set of capabilities supported by the kernel */
int krg_cap_get_supported(int *set)
{
	return call_kerrighed_services(KSYS_GET_SUPPORTED_CAP, set);
}

/*****************************************************************************/
/*                                                                           */
/*                        KERRIGHED DEVELOPERS FUNCTIONS                     */
/*                                                                           */
/*****************************************************************************/



/** User interface to set capabilities for the father process */

int krg_father_capset (const krg_cap_t * new_caps)
{
  return call_kerrighed_services(KSYS_SET_FATHER_CAP,
				 (void *)new_caps) ;
}



/** User interface to get capabilities for the father process */

int krg_father_capget (krg_cap_t * old_caps)
{
  return call_kerrighed_services(KSYS_GET_FATHER_CAP,
				 old_caps) ;
}
