### Makefile.am for kerrighed service
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Author:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
EXTRA_DIST = default distros init.d update-rc.d kerrighed.completion

initscript = initd
initdefault = default

if USER_INSTALL
local_sysconfdir = $(sysconfdir)
else
local_sysconfdir = /etc
endif

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

install-data-local: $(lsbinit) $(lsbdefault) kerrighed.completion
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
