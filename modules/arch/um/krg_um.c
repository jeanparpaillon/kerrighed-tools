/*
 *  Kerrighed/modules/arch/um/krg_um.c
 *
 *  Copyright (C) 1999-2006 INRIA, Universite de Rennes 1, EDF
 */

#define MODULE_NAME "ARCH UM"

#include <linux/slab.h>
#include <asm/processor-generic.h>
#include <kerrighed/kernel_headers.h>

#include <tools/krg_arch.h>
#include "krg_arch_depend.h"

void *alloc_cpuinfo(int nr){
	return kmalloc(nr*sizeof(struct cpuinfo_um), GFP_KERNEL);
};

void *alloc_arch_nodeinfo(void){
	return kmalloc(sizeof(struct arch_nodeinfo), GFP_KERNEL);
};

int arch_nodeinfo_init(void* _arg){
	return 0;
};

int arch_nodeinfo_pack(struct gimli_desc *desc){
	return 0;
};

int arch_nodeinfo_unpack(struct gimli_desc *desc, void *_item){
	struct arch_nodeinfo *item;

	item = _item;
	return 0;
};

