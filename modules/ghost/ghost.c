/*
 *  Copyright (C) 2001-2006, INRIA, Universite de Rennes 1, EDF.
 */

#define MODULE_NAME "ghost"

#include "debug_ghost.h"
#include "ghost.h"

int init_ghost(void){
	int r;

	init_ghost_debug();

	r = nazgul_ghost_init();
	return 0;
}

void cleanup_ghost(void){
	nazgul_ghost_finalize();
}
