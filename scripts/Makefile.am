### Makefile.am for kerrighed service
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Author:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
EXTRA_DIST = kerrighed.default.in kerrighed.init update-rc kerrighed.completion
CLEANFILES = kerrighed.default

if USER_INSTALL
local_sysconfdir = $(sysconfdir)
else
local_sysconfdir = /etc
endif

edit = sed \
        -e 's|@bindir[@]|$(bindir)|g' \
        -e 's|@pkgdatadir[@]|$(pkgdatadir)|g' \
        -e 's|@prefix[@]|$(prefix)|g'

CHROOT = $(if $(DESTDIR), --chroot $(DESTDIR))

all-local: kerrighed.default

install-data-local: kerrighed.completion kerrighed.default
	$(install_sh_SCRIPT) $(srcdir)/kerrighed.init $(DESTDIR)$(local_sysconfdir)/init.d/kerrighed
	$(install_sh_DATA) $(builddir)/kerrighed.default $(DESTDIR)$(local_sysconfdir)/default/kerrighed
	$(srcdir)/update-rc --install $(CHROOT)
	$(install_sh_DATA) $(srcdir)/kerrighed.completion $(DESTDIR)$(local_sysconfdir)/bash_completion.d/kerrighed

uninstall-local:
	$(srcdir)/update-rc --remove $(CHROOT)
	rm -f $(DESTDIR)$(local_sysconfdir)/init.d/kerrighed
	rm -f $(DESTDIR)$(local_sysconfdir)/default/kerrighed
	rm -f $(DESTDIR)$(local_sysconfdir)/bash_completion.d/kerrighed

dist-hook:
	rm -rf $(find $(distdir) -name '*~')

kerrighed.default: Makefile
	rm -f $@ $@.tmp
	srcdir=''; \
	  test -f ./$@.in || srcdir=$(srcdir)/; \
	  $(edit) $${srcdir}$@.in >$@.tmp
	chmod go-w $@.tmp
	mv $@.tmp $@

kerrighed.default: $(srcdir)/kerrighed.default.in
