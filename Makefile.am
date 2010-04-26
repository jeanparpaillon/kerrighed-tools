#
# Kerrighed Makefile.am
#
# Copyright 2006-2007 INRIA, All rights reserved
# Copyright 2009, Kerlabs, All rights reserved
#
# Authors:
#   Jean PARPAILLON <jean.parpaillon@kerlabs.com>
#
ACLOCAL_AMFLAGS = -I m4
DISTCHECK_CONFIGURE_FLAGS=--enable-user-install --enable-tests

# Let '.' in first subdirs to build kernel stuff before others
SUBDIRS = . patches
if ENABLE_LIBKERRIGHED
SUBDIRS += libs
endif
if ENABLE_TOOLS
SUBDIRS += tools
endif
if ENABLE_HOST_TOOLS
SUBDIRS += tools-host
endif
if ENABLE_TESTS
SUBDIRS += tests
endif
SUBDIRS += man

EXTRA_DIST = README INSTALL NEWS ChangeLog linux_version.sh autogen.sh clean.sh m4/

kernel_srcdir = $(abspath @kernel_srcdir@)
kernel_builddir = $(abspath $(top_builddir)/kernel)

kernel_config = @kernel_config@
kernel_config_file = @kernel_config_file@

if USER_INSTALL
kerneldir = ${prefix}/boot
modulesdir = ${prefix}
else
kerneldir = /boot
modulesdir =
endif

apttool ?= $(shell which aptitude)

# out-of-source-build
oosb = $(if $(filter $(kernel_builddir),$(kernel_srcdir)),n,y)

K_V = $(K_V_$(V))
K_V_ = $(AM_DEFAULT_VERBOSITY)
K_V_0 = 0
K_V_1 = 1

MAKE_KERNEL=$(MAKE) -C $(kernel_builddir) V=$(K_V) EXTRAVERSION=-krg$(VERSION) INSTALL_PATH=$(DESTDIR)$(kerneldir) INSTALL_MOD_PATH=$(DESTDIR)$(modulesdir)

if ENABLE_KERNEL
all-local: kernel
install-exec-local: kernel-install
uninstall-local: kernel-uninstall
clean-local: kernel-clean
distclean-local: kernel-distclean
endif

kernel: kernel-config
	+$(MAKE_KERNEL) bzImage
	+$(MAKE_KERNEL) modules

kernel-prepare: $(kernel_builddir)-$(oosb)

$(kernel_builddir)-y: $(kernel_builddir)/Makefile.oosb

$(kernel_builddir)/Makefile.oosb:
	$(MAKE) $(AM_MAKEFLAGS) kernel-src
	-$(MAKE_KERNEL) mrproper
	$(MKDIR_P) $(@D)
	$(MAKE) -C $(kernel_srcdir) outputmakefile O="$(abspath $(kernel_builddir))" V=$(K_V)
	$(LN_S) Makefile $@

$(kernel_builddir)-n: kernel-src

kernel-src:
	$(MAKE) -C patches $(AM_MAKEFLAGS) kernel-src

kernel-config: $(kernel_builddir)/.config

$(kernel_builddir)/.config: $(kernel_config_file)
	$(MAKE) $(AM_MAKEFLAGS) kernel-prepare
	+@if test -n "$(kernel_config_file)"; then \
	  $(AM_V_GEN)cp $(kernel_config_file) $@; \
	else \
	  $(MAKE_KERNEL) $(kernel_config); \
	fi

kernel-install:
	$(MKDIR_P) $(DESTDIR)$(kerneldir)
	+$(MAKE_KERNEL) install
	+$(MAKE_KERNEL) modules_install

kernel-uninstall:
	kernelrelease=$$($(MAKE) --quiet -C $(kernel_builddir) kernelrelease | sed '/^make/d'); \
	  rm -f $(DESTDIR)$(kerneldir)/vmlinuz-$$kernelrelease; \
	  rm -f $(DESTDIR)$(kerneldir)/initrd.img-$$kernelrelease; \
	  rm -f $(DESTDIR)$(kerneldir)/System.map-$$kernelrelease; \
	  rm -f $(DESTDIR)$(kerneldir)/config-$$kernelrelease; \
	  rm -rf $(DESTDIR)$(modulesdir)/lib/modules/$$kernelrelease

kernel-clean:
	-$(MAKE_KERNEL) clean

kernel-distclean:
	-$(MAKE_KERNEL) mrproper
	[ ! -d $(kernel_srcdir)/.git ] && rm -rf $(kernel_srcdir) || true
	if test ! "$(kernel_builddir)" = "$(kernel_srcdir)"; then \
	  rm -rf $(kernel_builddir); \
	fi

build-patch:
	$(MAKE) -C patches build-patch

build-reqs-deb:
	[ -n "$(apttool)" ] && \
		$(apttool) install build-essential bzip2 wget automake autoconf libtool pkg-config rsync libncurses5-dev xsltproc docbook-xsl

print_notice:
	@echo -e "****************************************************************"
	@echo -e "***"
	@echo -e "***   Please visit the Kerrighed website for support at "
	@echo -e "***   \033[34mhttp://kerrighed.org/\033[0m"
	@echo -e "***   You may also register your cluster at "
	@echo -e "***   \033[34mhttp://kerrighed.org/php/clusterslist.php\033[0m"
	@echo -e "***"
	@echo -e "****************************************************************"

.PHONY: kernel kernel-prepare kernel-config kernel-install print_notice
