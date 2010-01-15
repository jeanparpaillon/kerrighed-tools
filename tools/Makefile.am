### Makefile.am for kerrighed tools
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Author:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
dist_bin_SCRIPTS = krg_legacy_scheduler krgboot_helper krginit_helper

bin_PROGRAMS = migrate checkpoint restart krgadm krgcapset krgcr-run krgboot krginit ipccheckpoint ipcrestart

INCLUDES = -I@top_srcdir@/libs/include
LDADD = @top_builddir@/libs/libkerrighed/libkerrighed.la @top_builddir@/libs/libkrgcb/libkrgcb.la
AM_LDFLAGS = -Wl,--as-needed

migrate_SOURCES = migrate.c
checkpoint_SOURCES = checkpoint.c
restart_SOURCES = restart.c
krgcapset_SOURCES = krgcapset.c
krgadm_SOURCES = krgadm.c
krgcr_run_SOURCES = krgcr-run.c
krgboot_SOURCES = krgboot.c
krginit_SOURCES = krginit.c
ipccheckpoint_SOURCES = ipccheckpoint.c
ipcrestart_SOURCES = ipcrestart.c

EXTRA_DIST = krginit_helper.conf krginit_helper.in krgboot_helper.in
CLEANFILES = krgboot_helper krginit_helper

edit = sed \
        -e 's|@bindir[@]|$(bindir)|g' \
        -e 's|@pkgdatadir[@]|$(pkgdatadir)|g' \
        -e 's|@prefix[@]|$(prefix)|g' \
        -e 's|@sysconfdir[@]|$(sysconfdir)|g'

install-data-local:
	$(install_sh_SCRIPT) $(srcdir)/krginit_helper.conf $(DESTDIR)$(sysconfdir)/kerrighed/krginit_helper.conf

uninstall-local:
	rm -f $(DESTDIR)$(sysconfdir)/kerrighed/krginit_helper.conf

krgboot_helper krginit_helper: Makefile
	rm -f $@ $@.tmp
	srcdir=''; \
	  test -f ./$@.in || srcdir=$(srcdir)/; \
	  $(edit) $${srcdir}$@.in >$@.tmp
	chmod +x $@.tmp
	chmod go-w $@.tmp
	mv $@.tmp $@

krgboot_helper: $(srcdir)/krgboot_helper.in
krginit_helper: $(srcdir)/krginit_helper.in
