### Makefile.am for kerrighed tests
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Authors:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
INCLUDES = -I$(srcdir)/../include -I$(top_srcdir)/libs/include
LDADD = $(top_builddir)/libs/libkerrighed/libkerrighed.la -lm

bin_PROGRAMS = up-gs_migr up-gs fork_test

up_gs_migr_SOURCES = up-gs_migr.c
up_gs_SOURCES = up-gs.c
fork_test_SOURCES = fork_test.c
