### Makefile.am for kerrighed service
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Author:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
EXTRA_DIST = default distros init.d update-rc.d kerrighed.completion
CLEANFILES = default/all

initscript = initd
initdefault = default

if USER_INSTALL
local_sysconfdir = $(sysconfdir)
else
local_sysconfdir = /etc
endif

edit = sed \
        -e 's|@bindir[@]|$(bindir)|g' \
        -e 's|@pkgdatadir[@]|$(pkgdatadir)|g' \
        -e 's|@prefix[@]|$(prefix)|g'

lsbid = @DIST_ID@
lsbrelease = @DIST_RELEASE@
lsbpath = $(shell \
			  if test -d $(srcdir)/distros/$(lsbid)/$(lsbrelease); then \
          echo $(srcdir)/distros/$(lsbid)/$(lsbrelease) \
        ; else \
          echo $(srcdir)/distros/$(lsbid) \
        ; fi )
lsbinit = $(patsubst %, $(lsbpath)/%, $(initscript))
lsbdefault = $(patsubst %, $(lsbpath)/%, $(initdefault))

CHROOT = $(if $(DESTDIR), -c $(DESTDIR))

all-local: default/all

install-data-local: $(lsbinit) $(lsbdefault) kerrighed.completion default/all
	$(install_sh_SCRIPT) $(lsbinit) $(DESTDIR)$(local_sysconfdir)/init.d/kerrighed
	$(install_sh_SCRIPT) $(lsbdefault) $(DESTDIR)$(local_sysconfdir)/default/kerrighed
	$(lsbpath)/update-rc.d -i $(CHROOT)
	$(install_sh_SCRIPT) $(srcdir)/kerrighed.completion $(DESTDIR)$(local_sysconfdir)/bash_completion.d/kerrighed

uninstall-local:
	$(lsbpath)/update-rc.d -r $(CHROOT)
	rm -f $(DESTDIR)$(local_sysconfdir)/init.d/kerrighed
	rm -f $(DESTDIR)$(local_sysconfdir)/default/kerrighed
	rm -f $(DESTDIR)$(local_sysconfdir)/bash_completion.d/kerrighed

dist-hook:
	rm -rf $(find $(distdir) -name '*~')

default/all: Makefile
	rm -f $@ $@.tmp
	srcdir=''; \
	  test -f ./$@.in || srcdir=$(srcdir)/; \
	  $(edit) $${srcdir}$@.in >$@.tmp
	chmod go-w $@.tmp
	mv $@.tmp $@

default/all: $(srcdir)/default/all.in
