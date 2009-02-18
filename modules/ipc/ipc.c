/** KerIPC module initialization.
 *  @file ipc.c
 *
 *  Implementation of functions used to initialize and finalize the
 *  keripc module.
 *
 *  Copyright (C) 2006-2007, Renaud Lottiaux, Kerlabs.
 *  Copyright (C) 2007-2008, Matthieu Fertré, INRIA.
 */

#include "debug_keripc.h"
#define MODULE_NAME "Module          "

#ifdef MODULE_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <ctnr/kddm.h>
#include <kerrighed/krgsyms.h>

#include "debug_keripc.h"
#include "ipc_handler.h"
#include "shm_handler.h"
#include "msg_handler.h"
#include "sem_handler.h"
#include "shm_memory_linker.h"
#include "shmid_io_linker.h"
#include "ipcmap_io_linker.h"

#ifndef CONFIG_KRG_MONOLITHIC
MODULE_AUTHOR ("Renaud Lottiaux");
MODULE_DESCRIPTION ("Kerrighed Distributed IPC");
MODULE_LICENSE ("GPL");
#endif



/** Initialisation of the distributed IPC module.
 *  @author Renaud Lottiaux
 *
 *  Register kermm services in the /proc/kerrighed/services.
 */
int init_keripc (void)
{
	printk ("KerIPC initialisation : start\n");

	init_ipc_debug();

	ipcmap_object_cachep = kmem_cache_create("ipcmap_object",
						 sizeof(ipcmap_object_t),
						 0, SLAB_PANIC, NULL, NULL);

	register_io_linker (IPCMAP_LINKER, &ipcmap_linker);
	krgsyms_register (KRGSYMS_VM_OPS_SHM_KDDM_VMOPS, &krg_shmem_vmops);

#ifndef NO_CLUSTER_SHM
	ipc_handler_init();
	shm_handler_init();
	msg_handler_init();
	sem_handler_init();
	krg_shmem_vmops = _krg_shmem_vmops;
#endif

	printk ("KerIPC initialisation done\n");

	return 0;
}



/** Cleanup of the DSM module.
 *  @author Renaud Lottiaux
 *
 *  Kill object manager, object server and container manager threads.
 */
void cleanup_keripc (void)
{
	printk ("KerIPC termination : start\n");

#ifndef NO_CLUSTER_SHM
	ipc_handler_finalize();
	shm_handler_finalize();
	msg_handler_finalize();
	sem_handler_finalize();
#endif

	krgsyms_unregister (KRGSYMS_VM_OPS_SHM_KDDM_VMOPS);

	printk ("KerIPC termination done\n");
}

#ifndef CONFIG_KRG_MONOLITHIC
module_init (init_keripc);
module_exit (cleanup_keripc);
#endif
