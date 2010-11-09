#!/bin/bash

LTP_print_info() {
    if [ "$DEBUG" != "" ]; then
	echo "[$$] ($TEST_STEP) $@"
    fi
}

LTP_print_step_info() {
    if [ "$DEBUG" != "" ]; then
	TEST_STEP=$[$TEST_STEP+1]
	echo "[$$] ($TEST_STEP) $@"
    fi
}

print_success() {
    local r=$1

    if [ "$r" -eq 0 ]; then
	echo "[$$] OK"
    else
	echo "[$$] :-("
    fi

    return $r
}


setup_tmp_dir() {
    LTP_print_step_info "Creating tmpfs directory..."

    TMPDIR=`mktemp -d`

    mount -t tmpfs tmpfs $TMPDIR
    if [ $? -ne 0 ]; then
	echo "Mount tmpfs failed"
	exit 1
    fi

    TMPSUBDIR=`mktemp -d -p $TMPDIR`
    FILE=$TMPSUBDIR/file

    touch $FILE
    chmod 644 $FILE
}

cleanup_tmp_dir() {
    umount $TMPDIR
    rmdir $TMPDIR
}

# Description:
#               - Export global variables
#
# Return        - zero on success
#               - non zero on failure. return value from commands ($?)
setup()
{
    krgcapset -d -DISTANT_FORK
    krgcapset -e -DISTANT_FORK

    # Parse options
    while getopts "hd" flag
    do
	case $flag in
	    h ) echo " -d : debug mode (more verbose)"
		exit 0
		;;
	    d ) DEBUG="yes";;
	    ? ) echo "*** unknow options"; exit 1;;
	esac
    done

    return 0
}

