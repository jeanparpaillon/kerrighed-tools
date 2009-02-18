#!/bin/bash
#
# Copyright 2006-2007 INRIA-IRISA
# 	Jean PARPAILLON <jean.parpaillon@irisa.fr>
#

version=0
major=0
minor=0
extraversion=0
makefile=""

function usage {
    echo "Usage: $0 Makefile [format]"
    echo "       Format is a string, with following special characters:"
    echo "       'V': version"
    echo "       'P': patchlevel"
    echo "       'S': sublevel"
    echo "       'E': extraversion"
}

function format_to_awk {
    echo $1 | sed 's/\([^VPSE]*\)/"\1"/g'
}

let $(( $# > 0  )) || {
    usage
    exit 0;
}

makefile=$1
test -f $makefile || {
    usage
    exit 1;
}

format="V.P.SE"
if test -n "$2"; then
    format=$2
fi
awk_format=`format_to_awk $format`

head $makefile | awk 'BEGIN { FS="[ ]?=[ ]?" }
/^VERSION =/ { V=$2 }
/^PATCHLEVEL =/ { P=$2 }
/^SUBLEVEL =/ { S=$2 }
/^EXTRAVERSION =/ { E=$2 }
END { print '$awk_format' }'

exit 0
