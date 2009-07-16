#!/bin/sh
# Copyright 2006-2007 INRIA, All rights reserved
# Copyright 2009 Kerlabs, All rights reserved
#
# Authors:
#   Jean Parpaillon <jean.parpaillon@kerlabs.com>
#
set -e

$(dirname $0)/clean.sh

#
# check tools version number -- we require >= 1.10
#
find_tools() {
    tool=$1

    if `which $tool-1.10 > /dev/null 2>&1`; then
        TOOL=$tool-1.10
    elif `which $tool > /dev/null 2>&1`; then
        major=`$tool --version | grep $tool | awk {'print \$4'} | awk -F '.' {'print \$1'}`
        minor=`$tool --version | grep $tool | awk {'print \$4'} | awk -F '.' {'print \$2'}`
        if test "$major" -gt 1; then
            TOOL=$tool
        elif test "$major" -eq 1 -a "$minor" -ge 10; then
            TOOL=$tool
        else
            echo "Required: $tool version >= 1.10" >&2
            exit 1
        fi
    else
        echo "Required: $tool version >= 1.10" >&2
        exit 1
    fi

    echo "$TOOL"
}

ACLOCAL=$(find_tools aclocal)
AUTOMAKE=$(find_tools automake)

if test -f $(dirname $0)/configure.ac; then
    (
        cd $(dirname $0)
        echo "Regenerating autoconf/libtoolize files"
        $ACLOCAL -I m4
        libtoolize -c
        autoheader
        $AUTOMAKE --add-missing
        autoconf
        )
fi
