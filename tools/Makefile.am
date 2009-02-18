### Makefile.am for kerrighed tools
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Author:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
bin_PROGRAMS = migrate checkpoint restart krgcapset krgadm
INCLUDES = -I@top_srcdir@/libs/include
LDADD = @top_builddir@/libs/libkerrighed/libkerrighed.la

migrate_SOURCES = migrate.c
checkpoint_SOURCES = checkpoint.c
restart_SOURCES = restart.c
krgcapset_SOURCES = krgcapset.c
krgadm_SOURCES = krgadm.c
