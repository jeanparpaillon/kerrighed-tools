### Makefile.am for libkrgcheckpoint
###
### Copyright 2010 Matthieu Fertré <matthieu.fertre@kerlabs.com>
###		   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
EXTRA_DIST = krgcheckpoint.pc.in

INCLUDES = \
	-I$(top_srcdir)/libs/include

lib_LTLIBRARIES = libkrgcheckpoint.la

libkrgcheckpoint_la_SOURCES = \
	libkrgcheckpoint.c

libkrgcheckpoint_la_LIBADD = @top_builddir@/libs/libkerrighed/libkerrighed.la @top_builddir@/libs/libkrgcb/libkrgcb.la
libkrgcheckpoint_la_LDFLAGS = -lpthread -version-info 1:0:0

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = krgcheckpoint.pc
