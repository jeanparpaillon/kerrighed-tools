/** Kerrighed tools user interface.
 *  @file libkerrighed.h
 *  
 *  @author David Margery
 */

#ifndef LIBKERRIGHED_H
#define LIBKERRIGHED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "capability.h"
#include "proc.h"
#include "hotplug.h"
#include "ipc.h"

void __attribute__ ((constructor)) init_krg_lib(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
