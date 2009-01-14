### Makefile.am for kerrighed tools
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Author:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
dist_bin_SCRIPTS = krg_legacy_scheduler krgboot_helper krginit_helper \
	krg_schedsetnodes

bin_PROGRAMS = migrate checkpoint restart krgadm krgcapset krgcr-run krgboot krginit ipccheckpoint ipcrestart

INCLUDES = -I@top_srcdir@/libs/include
LDADD = @top_builddir@/libs/libkerrighed/libkerrighed.la @top_builddir@/libs/libkrgcb/libkrgcb.la

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

EXTRA_DIST = krginit_helper.conf

if USER_INSTALL
local_sysconfdir = $(sysconfdir)
else
local_sysconfdir = /etc
endif

install-data-local:
	$(install_sh_SCRIPT) $(srcdir)/krginit_helper.conf $(DESTDIR)$(local_sysconfdir)/kerrighed/krginit_helper.conf

uninstall-local:
	rm -f $(DESTDIR)$(local_sysconfdir)/kerrighed/krginit_helper.conf
