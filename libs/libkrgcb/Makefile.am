### Makefile.am for libkrgcb
###
### Copyright 2009 Eugen Feller <eugen.feller@uni-duesseldorf.de>
###		   Jean Parpaillon <jean.parpaillon@irisa.fr>
###
EXTRA_DIST = krgcb.pc.in

INCLUDES = \
	-I$(top_srcdir)/libs/include

lib_LTLIBRARIES = libkrgcb.la

libkrgcb_la_SOURCES = \
	libkrgcb.c

libkrgcb_la_LIBADD = @top_builddir@/libs/libkerrighed/libkerrighed.la
libkrgcb_la_LDFLAGS = -lpthread -version-info 1:0:0

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = krgcb.pc
