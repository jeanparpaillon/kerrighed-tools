#!/bin/bash
###############################################################################
##
## Copyright (c) Kerlabs, 2010
##
## This program is free software;  you can redistribute it and#or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation; either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful, but
## WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
## or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
## for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program;  if not, write to the Free Software
## Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
##
###############################################################################
#
# File :        lib_futex.sh
#
# Description:  Futex "library" file
#
# Author:       Matthieu Fertré, matthieu.fertre@kerlabs.com
#

LTP_print_info()
{
    if [ "$DEBUG" != "" ]; then
	tst_resm TINFO "[$$] ($TEST_STEP) $@"
    fi
}

LTP_print_step_info()
{
    if [ "$DEBUG" != "" ]; then
	TEST_STEP=$[$TEST_STEP+1]
	tst_resm TINFO "[$$] ($TEST_STEP) $@"
    fi
}

print_success()
{
    local r=$1

    if [ "$r" -eq 0 ]; then
	tst_resm TPASS \
	    "[$$] OK"
    else
	tst_brkm TFAIL NULL \
	    "[$$] :-("
    fi

    return $r
}

futex_wait()
{
    local key=$1
    local bits=$2

    futex-shm-tool -q -k$key -b $bits wait &

    LTP_print_step_info "Process $! is waiting on key $key"
}

futex_wake()
{
    local key=$1
    local bits=$2
    local nrwake=$3
    local awaited=$4

    local result=0
    futex-shm-tool -q -k$key -b $bits -w$nrwake wake
    result=$?

    if [ $result -ne $awaited ]; then
	tst_brkm TFAIL NULL \
	    "futex_wake $key: incorrect number of processes woken up ($result != $awaited)"
	return 1
    fi

    LTP_print_step_info \
	"futex_wake $key: $result processe(s) woken up on futex $key"

    return 0
}

futex_requeue()
{
    local key1=$1
    local key2=$2
    local nrwake=$3
    local nrrequeue=$4
    local awaited=$5

    local result=0
    futex-shm-tool -q -k$key1 -K$key2 -w$nrwake -r$nrrequeue requeue
    result=$?

    if [ $result -ne $awaited ]; then
	tst_brkm TFAIL NULL \
	    "futex_requeue $key1 $key2: incorrect number of processes woken up or requeued($result != $awaited)"
	return 1
    fi

    LTP_print_step_info \
	"futex_requeue $key1 $key2: $result processe(s) woken up or requeued"

    return 0
}

futex_delete()
{
    local key=$1

    futex-shm-tool -q -k$key delete
    LTP_print_step_info \
	"futex $key deleted"
}

# Description:
#               - Export global variables
#
# Return        - zero on success
#               - non zero on failure. return value from commands ($?)
futex_setup()
{
    export TST_TOTAL=1  # Total number of test cases in this file.
    LTPTMP=${TMP}       # Temporary directory to create files, etc.
    export TCID="setup" # Test case identifier
    export TST_COUNT=0  # Set up is initialized as test 0

    # Initialize cleanup function to execute on program exit.
    # This function will be called before the test program exits.
    # trap "futex_internal_cleanup" 0

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
