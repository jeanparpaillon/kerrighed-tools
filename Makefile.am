#
# Kerrighed Makefile.am
#
# Copyright 2006-2007 INRIA, All rights reserved
#
# Author:
#   Jean PARPAILLON <jean.parpaillon@kerlabs.com>
#
SUBDIRS = libs tools
LINUX_SRC = @kernelsrc@
LINUX_BUILD = @kernelbuild@
KERNEL_CONFIG = @kernelconfig@

EXTRA_DIST = \
	README \
	INSTALL \
	linux_version.sh

ABS_DISTDIR = $(shell cd $(distdir); /bin/pwd)

kernel:
	@if [ ! -e $(LINUX_BUILD)/.config ]; then \
	  echo "***"; \
	  echo "*** Kernel sources are not configured. "; \
	  echo "*** Optionally, run 'make defconfig' to get a default configuration"; \
	  echo "*** Then, run one of 'make *config' in the kernel source tree"; \
	  echo "***"; \
	  exit 1; \
	fi
	$(MAKE) -C $(LINUX_BUILD) bzImage
	$(MAKE) -C $(LINUX_BUILD) modules

kernel_install: kernel-install
kernel-install: kernel
	test -z "$(DESTDIR)" || mkdir -p $(DESTDIR)/boot
	$(MAKE) -C $(LINUX_BUILD) install INSTALL_PATH=$(DESTDIR)/boot
	$(MAKE) -C $(LINUX_BUILD) modules_install INSTALL_MOD_PATH=$(DESTDIR)

kernel_config: kernel-config
kernel-config: patch
	@if test ! -n "$(KERNEL_CONFIG)"; then \
	  echo "***"; \
	  echo "*** Kernel is not configured. Please point KERNEL_CONFIG variable"; \
	  echo "*** to a .config file or run make *config in the kernel source tree."; \
	  echo "***"; \
	  exit 1; \
	fi
	cp $(KERNEL_CONFIG) $(LINUX_BUILD)/.config

defconfig:
	$(MAKE) -C $(LINUX_BUILD) defconfig

patch:
	if test ! -d "$(LINUX_SRC)/include/kerrighed"; then \
	  $(MAKE) $(LINUX_SRC)/include/kerrighed; \
	fi

$(LINUX_SRC)/include/kerrighed:
	linux_version=`$(top_srcdir)/linux_version.sh $(LINUX_SRC)/Makefile` && \
	for patch in `ls $(top_srcdir)/patches/$$linux_version/*.bz2`; do \
	  bunzip2 -c $$patch | patch -p1 -d $(LINUX_SRC); \
	done

dist-patch:
	$(MAKE) -C patches dist-patch

install-data-hook: print_notice

print_notice:
	@echo -e "****************************************************************"
	@echo -e "***"
	@echo -e "***   Please visit the Kerrighed website for support at "
	@echo -e "***   \033[34mhttp://kerrighed.org/\033[0m"
	@echo -e "***   You may also register your cluster at "
	@echo -e "***   \033[34mhttp://kerrighed.org/php/clusterslist.php\033[0m"
	@echo -e "***"
	@echo -e "****************************************************************"

.PHONY: kernel kernel_install kernel-install kernel_config kernel-config patch dist-patch
