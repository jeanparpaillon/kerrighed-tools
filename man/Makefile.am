### Makefile.am for kerrighed manpages
###
### Copyright 2006-2007 INRIA, All rights reserved
### Copyright 2009 Kerlabs, All rights reserved
###
### Authors:
###        Jean Parpaillon <jean.parpaillon@kerlabs.com>
###
man_MANS = \
	krgadm.1 \
	kerrighed.7 \
	kerrighed_nodes.5 \
	kerrighed_capabilities.7 \
	krgcapset.1 \
	krgcapset.2 \
	migrate.1 \
	migrate.2 \
	migrate_self.2 \
	restart.1 \
	checkpoint.1 \
	krgcr-run.1
html_MANS = $(patsubst %,%.html,$(man_MANS))
man_sources = $(patsubst %,%.xml,$(man_MANS))

EXTRA_DIST = $(man_MANS) $(man_sources) manual-man.xsl manual-xhtml.xsl
CLEANFILES = $(html_MANS)
MAINTAINERCLEANFILES = $(man_MANS)

html: $(html_MANS)

$(man_MANS): %: %.xml manual-man.xsl
	xsltproc --nonet --param man.endnotes.are.numbered 0 $(srcdir)/manual-man.xsl $<

$(html_MANS): %.html: %.xml manual-xhtml.xsl $<
	xsltproc --nonet --output $@ $(srcdir)/manual-xhtml.xsl $<

.PHONY: html