### Makefile.am for libkerrighed
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009, All rights reserved
###
### Author:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
EXTRA_DIST = kerrighed.pc.in

INCLUDES = \
	-I$(top_srcdir)/libs/include \
	-I@kernelsrc@/include \
	-I@kerrighed_module@

lib_LTLIBRARIES = libkerrighed.la

libkerrighed_la_SOURCES = \
	libkerrighed_tools.c \
	libproc.c \
	libhotplug.c
if ENABLE_CAPABILITY
libkerrighed_la_SOURCES += libcapability.c
endif

libkerrighed_la_LDFLAGS = -version-info 1:0:0

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = kerrighed.pc
