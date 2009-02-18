### Makefile.am for kerrighed tests
### 
### Copyright 2006-2007 INRIA-IRISA
###                     Jean Parpaillon <jean.parpaillon@irisa.fr>
###
if ENABLE_APPS
APPS = apps
endif
if ENABLE_PROC
PROC = proc
endif
if ENABLE_KTP
KTP = ktp
endif
if ENABLE_BENCHMARK
BENCHMARK = benchmark
endif

TESTS_SUBDIRS = ${APPS} ${PROC} ${KTP}

SUBDIRS = include ${TESTS_SUBDIRS} ${BENCHMARK}
