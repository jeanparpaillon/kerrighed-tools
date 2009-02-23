# =============================================
# Create a patched kernel source tree
#
# Copyright 2006 INRIA-IRISA
# Copyright 2009 Kerlabs
#
# Renaud LOTTIAUX - Paris Research Team - IRISA
# Jean PARPAILLON - Kerlabs
# Erich Focht     - NEC HPCE
#
# $Id: Makefile.am 1007 2006-12-11 10:39:24Z jparpail $
# =============================================
krgpatch = $(srcdir)/kerrighed.patch
orig_version = @vanilla_linux_version@
kernel_mirror = @kernel_mirror@

#KERRIGHED_REPOS ?= git://mirrors.git.kernel.org/cluster/kerrighed/kernel
KERRIGHED_REPOS ?= git://git-externe.kerlabs.com/kerrighed-kernel.git
orig_tag = v$(orig_version)

vanilla_dir = linux-$(orig_version)
vanilla_archive = $(vanilla_dir).tar.bz2
vanilla_url = http://$(kernel_mirror)/pub/linux/kernel/v2.6$(if $(findstring rc,$(orig_version)),/testing)/$(vanilla_archive)
patched_dir = $(vanilla_dir)-krg

EXTRA_DIST = $(krgpatch)

all-local: $(patched_dir)

$(patched_dir): $(vanilla_archive) $(krgpatch)
	bzcat $< | tar xf -
	cd $(vanilla_dir) && patch -p1 < $(krgpatch)
	mv $(vanilla_dir) $(patched_dir)

linux-$(orig_version).tar.bz2:
	if test -e /usr/src/$@; then \
	  ln -s /usr/src/$@; \
	else \
	  wget -O $@ -c $(vanilla_url); \
	fi

$(krgpatch):
	git clone --bare --no-checkout $(KERRIGHED_REPOS) kernel.git
	cd kernel.git && git diff $(orig_tag).. > $(krgpatch)

maintainer-clean-local:
	rm -f $(krgpatch)
	rm -rf $(patched_dir)
	if test -h $(vanilla_archive); then \
	  rm -f $(vanilla_archive); \
	fi

clean-local:
	-rm -rf kernel.git
