/*
 *  Kerrighed/modules/arch/i386/krg_i386.c
 *
 *  Copyright (C) 2007 Louis Rilling - Kerlabs
 */

#define MODULE_NAME "arch"

#include "debug_i386.h"

int init_arch(void)
{
	init_arch_i386_debug();

	return 0;
}

void cleanup_arch(void)
{
}
