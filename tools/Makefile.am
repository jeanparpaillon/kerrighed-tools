### Makefile.am for kerrighed tools
### 
### Copyright 2006-2007 INRIA-IRISA
###        Jean Parpaillon <jean.parpaillon@irisa.fr>
###
if ENABLE_MAN
MANDIR = man
else
MANDIR = 
endif

if ENABLE_SERVICE
SCRIPTSDIR = scripts
else
SCRIPTSDIR = 
endif

SUBDIRS = src $(MANDIR) $(SCRIPTSDIR)

bin_SCRIPTS = krg_legacy_scheduler
