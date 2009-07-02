### Makefile.am for kerrighed tests
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009, Kerlabs, All rights reserved
###
### Authors:
###   Jean PARPAILLON <jean.parpaillon@kerlabs.com>
###
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = include
if ENABLE_APPS
SUBDIRS += apps
endif
if ENABLE_PROC
SUBDIRS += proc
endif
if ENABLE_KTP
SUBDIRS += ktp
endif
if ENABLE_BENCHMARK
SUBDIRS += benchmark
endif
