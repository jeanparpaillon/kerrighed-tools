###
### Makefile for generating Kerrighed patch ang re-generating
### kernel sources from linux vanilla and patches
###
### Copyright 2006 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Authors:
###	Renaud LOTTIAUX - Paris Research Team - IRISA
###	Jean PARPAILLON - Kerlabs
###	Erich Focht     - NEC HPCE
###
### $Id: Makefile.am 1007 2006-12-11 10:39:24Z jparpail $
krgpatch = $(srcdir)/kerrighed.patch
orig_version = @vanilla_linux_version@
kernel_mirror = @kernel_mirror@
kernel_config = @kernel_config@

#KERRIGHED_REPOS ?= git://mirrors.git.kernel.org/cluster/kerrighed/kernel
KERRIGHED_REPOS ?= git://git-externe.kerlabs.com/kerrighed-kernel.git
orig_tag = v$(orig_version)
KERRIGHED_TAG ?=

vanilla_dir = linux-$(orig_version)
vanilla_archive = $(vanilla_dir).tar.bz2
vanilla_url = http://$(kernel_mirror)/pub/linux/kernel/v2.6$(if $(findstring rc,$(orig_version)),/testing)/$(vanilla_archive)

kernel_srcdir = $(abspath @kernel_srcdir@)
kernel_builddir = $(abspath $(top_builddir)/kernel)

EXTRA_DIST = $(krgpatch)

CLEANFILES =
MAINTAINERCLEANFILES = $(krg_patch)
DISTCLEANFILES = $(vanilla_archive)

kernel-src: $(kernel_srcdir)

$(kernel_srcdir):
	$(MAKE) $(AM_MAKEFLAGS) $(vanilla_archive) $(krgpatch)
	bzcat $(vanilla_archive) | tar xf -
	cd $(vanilla_dir) && patch -p1 < $(krgpatch)
	mv $(vanilla_dir) $@

linux-$(orig_version).tar.bz2:
	if test -e /usr/src/$@; then \
	  ln -s /usr/src/$@; \
	else \
	  wget -O $@ -c $(vanilla_url); \
	fi

$(krgpatch):
	git clone --bare --no-checkout $(KERRIGHED_REPOS) kernel.git
	cd kernel.git && git diff $(orig_tag)..$(KERRIGHED_TAG) > $(krgpatch)

maintainer-clean-local:
	rm -f $(krgpatch)

clean-local:
	-rm -rf kernel.git

.PHONY: kernel-src
