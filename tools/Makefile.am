### Makefile.am for kerrighed tools
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Author:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
dist_sbin_SCRIPTS = krginit_helper krg_legacy_scheduler
bin_PROGRAMS = migrate checkpoint restart krgcapset krgcr-run ipccheckpoint ipcrestart
sbin_PROGRAMS = krgadm krginit

INCLUDES = -I@top_srcdir@/libs/include
LDADD = @top_builddir@/libs/libkerrighed/libkerrighed.la @top_builddir@/libs/libkrgcb/libkrgcb.la
AM_LDFLAGS = -Wl,--as-needed -static

migrate_SOURCES = migrate.c
checkpoint_SOURCES = checkpoint.c
restart_SOURCES = restart.c
krgcapset_SOURCES = krgcapset.c
krgadm_SOURCES = krgadm.c
krgcr_run_SOURCES = krgcr-run.c
krginit_SOURCES = krginit.c
ipccheckpoint_SOURCES = ipccheckpoint.c
ipcrestart_SOURCES = ipcrestart.c

EXTRA_DIST = \
	krginit_helper.conf \
	krginit_helper.in \
	kerrighed.default \
	kerrighed.init \
	kerrighed.completion

CLEANFILES = krginit_helper

edit = sed \
        -e 's|@bindir[@]|$(bindir)|g' \
        -e 's|@sbindir[@]|$(sbindir)|g' \
        -e 's|@pkgdatadir[@]|$(pkgdatadir)|g' \
        -e 's|@prefix[@]|$(prefix)|g' \
        -e 's|@sysconfdir[@]|$(sysconfdir)|g'

if ENABLE_PRESERVE_CONF
preserve = -p
endif

install-data-local: krginit_helper.conf kerrighed.completion kerrighed.init kerrighed.default
	$(install_sh_DATA) $(preserve) $(srcdir)/krginit_helper.conf $(DESTDIR)$(sysconfdir)/kerrighed/krginit_helper.conf
	$(install_sh_DATA) $(srcdir)/kerrighed.completion $(DESTDIR)$(sysconfdir)/bash_completion.d/kerrighed
	$(install_sh_SCRIPT) $(srcdir)/kerrighed.init $(DESTDIR)$(sysconfdir)/init.d/kerrighed
	$(install_sh_DATA) $(preserve) $(srcdir)/kerrighed.default $(DESTDIR)$(sysconfdir)/default/kerrighed

uninstall-local:
	rm -f $(DESTDIR)$(sysconfdir)/kerrighed/krginit_helper.conf
	rm -f $(DESTDIR)$(sysconfdir)/init.d/kerrighed
	rm -f $(DESTDIR)$(sysconfdir)/default/kerrighed
	rm -f $(DESTDIR)$(sysconfdir)/bash_completion.d/kerrighed

krginit_helper: Makefile
	rm -f $@ $@.tmp
	srcdir=''; \
	  test -f ./$@.in || srcdir=$(srcdir)/; \
	  $(edit) $${srcdir}$@.in >$@.tmp
	chmod +x $@.tmp
	chmod go-w $@.tmp
	mv $@.tmp $@

krginit_helper: krginit_helper.in
