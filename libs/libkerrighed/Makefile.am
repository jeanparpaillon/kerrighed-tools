### Makefile.am for libkerrighed
### 
### Copyright 2006-2007 INRIA-IRISA
###        Jean Parpaillon <jean.parpaillon@irisa.fr>
###
EXTRA_DIST = kerrighed.pc.in

INCLUDES = \
	-I$(top_srcdir)/include \
	-I@kernelsrc@/include \
	-I@with_kerrighed@

lib_LTLIBRARIES = libkerrighed.la

libkerrighed_la_SOURCES = \
	libkerrighed_tools.c \
	libproc.c \
	libcapability.c \
	libhotplug.c

libkerrighed_la_LDFLAGS = -version-info 1:0:0

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = kerrighed.pc
