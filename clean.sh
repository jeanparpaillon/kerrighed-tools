#!/bin/sh
#
# Copyright 2006-2007 INRIA, All rights reserved
# Copyright 2009 Kerlabs, All rights reserved
#
# Author:
#   Jean Parpaillon <jean.parpaillon@kerlabs.com>
#
set -e

AUTOGEN_FILES="aclocal.m4 autom4te.cache configure config.guess config.log config.sub config.status depcomp install-sh compile libtool ltmain.sh missing mkinstalldirs config.h config.h.in"

make distclean || true

echo "Clean autogen generated files"
for file in $AUTOGEN_FILES; do
    ( cd $(dirname $0) && rm -rf $file )
done
