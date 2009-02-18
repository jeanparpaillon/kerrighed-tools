### Makefile.am for kerrighed tests
### 
### Copyright 2006-2007 INRIA-IRISA
###                     Jean Parpaillon <jean.parpaillon@irisa.fr>
###
LIB_ORIG=@LIB_ORIG@

AM_CPPFLAGS = -I$(top_srcdir)/include

if BUILD_LIBS
AM_CFLAGS = -L$(srcdir)
AM_CPPFLAGS += -I$(top_srcdir)/../libs/include
else
AM_CFLAGS = $(shell pkg-config --libs kerrighed)
AM_CPPFLAGS += $(shell pkg-config --cflags kerrighed)
endif

include $(top_srcdir)/Makefile.libs

bin_PROGRAMS = up-gs_migr up-gs fork_test

LDADD = -lm -lkerrighed

up_gs_migr_SOURCES = up-gs_migr.c
up_gs_migr_DEPENDENCIES = $(LIBKERRIGHED)

up_gs_SOURCES = up-gs.c
up_gs_DEPENDENCIES = $(LIBKERRIGHED)

fork_test_SOURCES = fork_test.c
fork_test_DEPENDENCIES = $(LIBKERRIGHED)
