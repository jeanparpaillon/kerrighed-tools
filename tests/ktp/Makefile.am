### Makefile.am for kerrighed tests
###
### Copyright 2007 INRIA, All rights reserved
###
### Authors:
###   Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
SUBDIRS = cr
bin_SCRIPTS = krgltp-smp.sh slab-check

EXTRA_DIST = krgltp-smp.sh.in slab-check

CLEANFILES = krgltp-smp.sh

edit = sed \
        -e 's|@bindir[@]|$(bindir)|g' \
        -e 's|@sbindir[@]|$(sbindir)|g' \
        -e 's|@pkgdatadir[@]|$(pkgdatadir)|g' \
        -e 's|@prefix[@]|$(prefix)|g' \
        -e 's|@sysconfdir[@]|$(sysconfdir)|g' \
        -e 's|@ltpbase[@]|$(ltpbase)|g'

krgltp-smp.sh: Makefile
	rm -f $@ $@.tmp
	srcdir=''; \
	  test -f ./$@.in || srcdir=$(srcdir)/; \
	  $(edit) $${srcdir}$@.in >$@.tmp
	chmod +x $@.tmp
	chmod go-w $@.tmp
	mv $@.tmp $@

krgltp-smp.sh: krgltp-smp.sh.in
