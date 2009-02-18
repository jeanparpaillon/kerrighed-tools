/** KerMM module initialization.
 *  @file mm.c
 *
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 *  Copyright (C) 2006-2009, Renaud Lottiaux, Kerlabs.
 *
 *  Implementation of functions used to initialize and finalize the
 *  kermm module.
 */

#include <linux/mm.h>
#include <asm/mman.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <kerrighed/krgsyms.h>
#include <kerrighed/mm.h>

#include "debug_kermm.h"

#ifdef MODULE_DEBUG
#define DEBUG_THIS_MODULE
#endif

#include <hotplug/hotplug.h>
#include <ctnr/kddm.h>
#include "mm_struct.h"
#include "memory_int_linker.h"
#include "memory_io_linker.h"
#include "mm_struct_io_linker.h"

#ifndef CONFIG_KRG_MONOLITHIC
MODULE_AUTHOR ("Renaud Lottiaux");
MODULE_DESCRIPTION ("Kerrighed Distributed Shared Memory");
MODULE_LICENSE ("GPL");
#endif



/** Initialisation of the DSM module.
 *  @author Renaud Lottiaux
 *
 *  Start object server, object manager and kddm set manager threads.
 *  Register kermm services in the /proc/kerrighed/services.
 */
int init_kermm (void)
{
	printk ("KerMM initialisation : start\n");
	
	krgsyms_register (KRGSYMS_VM_OPS_NULL, &null_vm_ops);
	krgsyms_register (KRGSYMS_VM_OPS_SHMEM, &shmem_vm_ops);
	krgsyms_register (KRGSYMS_VM_OPS_FILE_GENERIC, &generic_file_vm_ops);
	syscall_vm_ops_krgsyms_register ();
	krgsyms_register (KRGSYMS_VM_OPS_MEMORY_KDDM_VMOPS, 
			  &anon_memory_kddm_vmops);
	
	krgsyms_register (KRGSYMS_ARCH_UNMAP_AREA, arch_unmap_area);
	krgsyms_register (KRGSYMS_ARCH_UNMAP_AREA_TOPDOWN,
			  arch_unmap_area_topdown);
	krgsyms_register (KRGSYMS_ARCH_GET_UNMAP_AREA, arch_get_unmapped_area);
	krgsyms_register (KRGSYMS_ARCH_GET_UNMAP_AREA_TOPDOWN,
			  arch_get_unmapped_area_topdown);
	
	register_io_linker (MEMORY_LINKER, &memory_linker);
	register_io_linker (MM_STRUCT_LINKER, &mm_struct_io_linker);
	
	mm_struct_init ();

	INIT_MM_DEBUG();
	
	printk ("KerMM initialisation done\n");
	
	return 0;
}



/** Cleanup of the DSM module.
 *  @author Renaud Lottiaux
 *
 *  Kill object manager, object server and kddm set manager threads.
 */
void cleanup_kermm (void)
{
	printk ("KerMM termination : start\n");
	
	mm_struct_finalize ();
	
	krgsyms_unregister (KRGSYMS_VM_OPS_SHMEM);
	krgsyms_unregister (KRGSYMS_VM_OPS_FILE_GENERIC);
	syscall_vm_ops_krgsyms_unregister ();
	krgsyms_unregister (KRGSYMS_VM_OPS_MEMORY_KDDM_VMOPS);
	krgsyms_unregister (KRGSYMS_ARCH_UNMAP_AREA);
	krgsyms_unregister (KRGSYMS_ARCH_UNMAP_AREA_TOPDOWN);
	krgsyms_unregister (KRGSYMS_ARCH_GET_UNMAP_AREA);
	krgsyms_unregister (KRGSYMS_ARCH_GET_UNMAP_AREA_TOPDOWN);
	
	printk ("KerMM termination done\n");
}

#ifndef CONFIG_KRG_MONOLITHIC
module_init (init_kermm);
module_exit (cleanup_kermm);
#endif
