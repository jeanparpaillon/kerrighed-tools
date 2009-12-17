### Makefile.am for libkerrighed
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Authors:
###        Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
EXTRA_DIST = kerrighed.pc.in

DISTCLEANFILES = kerrighed.pc

INCLUDES = \
	-I$(srcdir)/../include \
	-I@kernel_srcdir@/include

lib_LTLIBRARIES = libkerrighed.la

libkerrighed_la_SOURCES = \
	libkerrighed_tools.c \
	libproc.c \
	libhotplug.c \
	libcapability.c \
	libipc.c

libkerrighed_la_LDFLAGS = -version-info 2:0:0

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = kerrighed.pc
