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
# JP: case..esac replace some known tag with sha1 for mirrors where tag are not pushed
get_sha1 = case $(orig_version) in \
						 2.6.30) \
							 echo 07a2039b8eb0af4ff464efd3dfd95de5c02648c6; \
							 ;; \
						 *) \
							 echo v$(orig_version); \
						 ;; \
					 esac
orig_tag = $(shell $(get_sha1))
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
	rm -rf $@
	mv $(vanilla_dir) $@

linux-$(orig_version).tar.bz2:
	if test -e /usr/src/$@; then \
	  ln -s /usr/src/$@; \
	else \
	  wget -O $@ -c $(vanilla_url); \
	fi

$(krgpatch):
	if [ -d "$(kernel_srcdir)" ]; then \
		if [ ! -d "$(kernel_srcdir)/.git" ]; then \
	    echo "E: Can not find git informations in kernel source dir. Can not create patch..."; \
	    false; \
		fi; \
	else \
			$(MAKE) $(kernel_srcdir)/.git; \
	fi
	cd $(kernel_srcdir) && git diff $(orig_tag)..$(KERRIGHED_TAG) > $(krgpatch)

$(kernel_srcdir)/.git:
	git clone --no-checkout $(KERRIGHED_REPOS) $(kernel_srcdir)
	touch $(kernel_srcdir)/.MAINTAINERCLEAN

maintainer-clean-local:
	rm -f $(krgpatch)
	[ -e $(kernel_srcdir)/.MAINTAINERCLEAN ] && rm -rf $(kernel_srcdir) || true

clean-local:
	-rm -rf kernel.git

.PHONY: kernel-src
