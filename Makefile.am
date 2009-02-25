#
# Kerrighed Makefile.am
#
# Copyright 2006-2007 INRIA, All rights reserved
#
# Author:
#   Jean PARPAILLON <jean.parpaillon@kerlabs.com>
#

# Handle new form of aclocal.m4
ACLOCAL_AMFLAGS = -I m4

if ENABLE_SERVICE
OPTSCRIPTS = scripts
endif
if ENABLE_LIBKERRIGHED
OPTLIBKERRIGHED = libs tools
endif
if ENABLE_TESTS
OPTTESTS = tests
endif
if ENABLE_KERNEL
OPTKERNEL = patches
endif

SUBDIRS =  $(OPTLIBKERRIGHED) $(OPTSCRIPTS) man $(OPTTESTS) $(OPTKERNEL)
EXTRA_DIST = README INSTALL NEWS ChangeLog

if ENABLE_KERNEL
all-local: kernel-prepare
endif

kernel kernel-prepare kernel-config kernel-install: %:
	$(MAKE) -C patches $@

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

.PHONY: kernel kernel-prepare kernel-install kernel-config print_notice
