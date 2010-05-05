### Makefile.am for kerrighed host tools
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Author:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
dist_sbin_SCRIPTS = krgboot_helper
sbin_PROGRAMS = krgboot

INCLUDES = -I@top_srcdir@/libs/include
LDADD = @top_builddir@/libs/libkerrighed/libkerrighed.la @top_builddir@/libs/libkrgcb/libkrgcb.la
AM_LDFLAGS=-Wl,--as-needed -static

krgboot_SOURCES = krgboot.c

EXTRA_DIST = \
	krgboot_helper.in \
	kerrighed-host.default.in \
	kerrighed-host.init

CLEANFILES = krgboot_helper kerrighed-host.default

edit = sed \
        -e 's|@bindir[@]|$(bindir)|g' \
        -e 's|@sbindir[@]|$(sbindir)|g' \
        -e 's|@pkgdatadir[@]|$(pkgdatadir)|g' \
        -e 's|@prefix[@]|$(prefix)|g' \
        -e 's|@sysconfdir[@]|$(sysconfdir)|g'

if ENABLE_PRESERVE_CONF
preserve = -p
endif

all-local: kerrighed-host.default

install-data-local: kerrighed-host.init kerrighed-host.default
	$(install_sh_SCRIPT) $(srcdir)/kerrighed-host.init $(DESTDIR)$(sysconfdir)/init.d/kerrighed-host
	$(install_sh_DATA) $(preserve) $(builddir)/kerrighed-host.default $(DESTDIR)$(sysconfdir)/default/kerrighed-host

uninstall-local:
	rm -f $(DESTDIR)$(sysconfdir)/init.d/kerrighed-host
	rm -f $(DESTDIR)$(sysconfdir)/default/kerrighed-host

krgboot_helper kerrighed-host.default: Makefile
	rm -f $@ $@.tmp
	srcdir=''; \
	  test -f ./$@.in || srcdir=$(srcdir)/; \
	  $(edit) $${srcdir}$@.in >$@.tmp
	chmod +x $@.tmp
	chmod go-w $@.tmp
	mv $@.tmp $@

krgboot_helper: krgboot_helper.in
kerrighed-host.default: kerrighed-host.default.in
