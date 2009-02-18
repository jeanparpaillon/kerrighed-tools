### Makefile.am for kerrighed tools
### 
### Copyright 2006-2007 INRIA-IRISA
###        Jean Parpaillon <jean.parpaillon@irisa.fr>
###
bin_PROGRAMS = migrate checkpoint restart krgcapset krgadm
AM_CFLAGS = @KERRIGHED_L@
INCLUDES = @KERRIGHED_I@

if BUILD_LIBS
LIBKERRIGHED = libkerrighed

CLEANFILES = libkerrighed*
endif

migrate_SOURCES = migrate.c
migrate_LDADD = -lkerrighed
migrate_DEPENDENCIES = $(LIBKERRIGHED)

checkpoint_SOURCES = checkpoint.c
checkpoint_LDADD = -lkerrighed
checkpoint_DEPENDENCIES = $(LIBKERRIGHED)

restart_SOURCES = restart.c
restart_LDADD = -lkerrighed
restart_DEPENDENCIES = $(LIBKERRIGHED)

krgcapset_SOURCES = krgcapset.c
krgcapset_LDADD = -lkerrighed
krgcapset_DEPENDENCIES = $(LIBKERRIGHED)

krgadm_SOURCES = krgadm.c
krgadm_LDADD = -lkerrighed
krgadm_DEPENDENCIES = $(LIBKERRIGHED)

if BUILD_LIBS
#EF# hack for avoiding the usage of an old /usr/lib/libkerrighed.so
libkerrighed: libkerrighed.la libkerrighed_uptodate

libkerrighed_uptodate:
	if ! $(MAKE) --question -C $(top_builddir)/../libs/libkerrighed > /dev/null 2>&1 ; then \
	  rm -f libkerrighed*; \
	  $(MAKE) libkerrighed.la; \
	fi

libkerrighed.la:
	$(MAKE) -C $(top_builddir)/../libs/libkerrighed install-libLTLIBRARIES libdir=$(PWD) DESTDIR="" && \
	sed -e 's,^libdir=.*$$,libdir=$$(pwd),' < libkerrighed.la > libkerrighed.la_mod && \
	mv libkerrighed.la_mod libkerrighed.la
	touch libkerrighed

endif
