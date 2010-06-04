#!/bin/bash
###############################################################################
##
## Copyright (c) INRIA, 2007-2008
## Copyright (c) Kerlabs, 2009
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
# File :        lib_cr.sh
#
# Description:  Sh "library" file to test Kerrighed checkpoint/restart of single
#               process.
#
# Author:       Matthieu Fertré, matthieu.fertre@irisa.fr
#

TESTCMD="bi-cr"
TESTCMD_OPTIONS="-q"
TEST_STEP=0

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

check_if_command_exists()
{
    local r=0

    which $1 > $LTPTMP/tst_template.out

    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TBROK NULL "INIT: Command $1 not found"
    fi
    return $r
}

###############################################################################

move_task_file_to_make_restart_fail()
{
    local _pid=$1
    local r=0

    local version=`awk '$1=="Version:" {print $2}' /tmp/chkpt_result${_pid}`
    local filechkpt=/var/chkpt/${_pid}/v${version}/task_${_pid}.bin

    mv $filechkpt $filechkpt.old
    r=$?

    return $r
}

move_task_back_file_to_make_restart_ok()
{
    local _pid=$1
    local r=0

    local version=`awk '$1=="Version:" {print $2}' /tmp/chkpt_result${_pid}`
    local filechkpt=/var/chkpt/${_pid}/v${version}/task_${_pid}.bin

    mv $filechkpt.old $filechkpt
    r=$?

    return $r
}

check_written_files()
{
    local _pid=$1
    local r=0

    # check if checkpoint is really written on disk
    # TODO: factorize(1)
    local version=`awk '$1=="Version:" {print $2}' /tmp/chkpt_result${_pid}`
    local filechkpt=/var/chkpt/${_pid}/v${version}/task_${_pid}.bin
    stat $filechkpt > /dev/null 2>&1

    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL \
	    "checkpoint has not be written on the disk! ($filechkpt)"
    fi
    return $r
}

checkpoint_process()
{
    local _pid=$1
    local _name=$2
    local _options=$3
    local r=0

    checkpoint $_options $_pid > /tmp/chkpt_result${_pid}

    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL \
	    "checkpoint: unable to checkpoint process $_pid ($_name)"
	return $r
    fi

    LTP_print_step_info "checkpoint $_pid ($_name): $r"

    check_written_files ${_pid}
    r=$?
    if [ $r -ne 0 ]; then
	return $r
    fi

    #rm -rf /tmp/chkpt_result${_pid}

    return $r
}

checkpoint_process_w_signal()
{
    local _pid=$1
    local _name=$2
    local _signal=$3
    local r=0

    checkpoint -f $_pid > /dev/null 2>&1
    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL \
	    "checkpoint: unable to freeze process $_pid ($_name)"
	return $r
    fi

    checkpoint -c $_pid > /tmp/chkpt_result${_pid}
    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL \
	    "checkpoint: unable to checkpoint process $_pid ($_name)"
	return $r
    fi

    checkpoint -u${_signal} $_pid > /dev/null 2>&1
    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL \
	    "checkpoint: unable to unfreeze process $_pid ($_name)"
	return $r
    fi


    LTP_print_step_info "checkpoint $_pid ($_name): $r"

    check_written_files ${_pid}

    r=$?
    if [ $r -ne 0 ]; then
	return $r
    fi

    rm -rf /tmp/chkpt_result${_pid}

    return $r
}

checkpoint_process_must_fail()
{
    local _pid=$1
    local _name=$2
    local _options=$3
    local r=0

    checkpoint $_options $_pid > /dev/null 2>&1

    r=$?
    if [ $r -eq 0 ]; then
	tst_brkm TFAIL NULL "checkpoint_must_fail: checkpoint $_pid ($_name) should have failed"
	r=1
	return $r
    fi

    LTP_print_step_info \
	"checkpoint_must_fail: $r - PID: $_pid, error: $r"

    r=0
    return $r
}

freeze_process()
{
    local _pid=$1
    local _name=$2
    local r=0

    checkpoint -f $_pid > /dev/null

    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL \
	    "checkpoint: unable to freeze process $_pid ($_name)"
	return $r
    fi

    LTP_print_step_info "freeze $_pid ($_name): $r"

    return $r
}

freeze_process_must_fail()
{
    local _pid=$1
    local _name=$2
    local r=0

    checkpoint -f $_pid > /dev/null 2>&1

    r=$?
    if [ $r -eq 0 ]; then
	tst_brkm TFAIL NULL \
	    "freeze_process_must_fail: freeze process $_pid ($_name) should have failed"
	r=1
	return $r
    fi

    LTP_print_step_info \
	"freeze_must_fail: PID: $_pid, error: $r"

    r=0
    return $r
}

checkpoint_frozen_process()
{
    local _pid=$1
    local _name=$2
    local r=0

    checkpoint -c $_pid > /tmp/chkpt_result${_pid}

    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL \
	    "checkpoint: unable to checkpoint frozen process $_pid ($_name)"
	return $r
    fi

    LTP_print_step_info "checkpoint $_pid ($_name): $r"

    check_written_files ${_pid}
    r=$?
    if [ $r -ne 0 ]; then
	return $r
    fi

    #rm -rf /tmp/chkpt_result${_pid}

    return $r
}

checkpoint_frozen_process_must_fail()
{
    local _pid=$1
    local _name=$2
    local r=0

    checkpoint -c $_pid > /tmp/chkpt_result${_pid} 2>&1

    r=$?
    if [ $r -eq 0 ]; then
	tst_brkm TFAIL NULL "checkpoint_frozen_must_fail: checkpoint $_pid ($_name) should have failed"
	r = 1
	return $r
    fi

    LTP_print_step_info \
	"checkpoint_frozen_must_fail: PID: $_pid, error: $r"

    r=0
    return $r
}

unfreeze_process()
{
    local _pid=$1
    local _name=$2
    local r=0

    checkpoint -u $_pid > /dev/null

    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL \
	    "checkpoint: unable to unfreeze process $_pid ($_name)"
	return $r
    fi

    LTP_print_step_info "unfreeze $_pid ($_name): $r"

    return $r
}

unfreeze_process_must_fail()
{
    local _pid=$1
    local _name=$2
    local r=0

    checkpoint -u $_pid > /dev/null 2>&1

    r=$?
    if [ $r -eq 0 ]; then
	tst_brkm TFAIL NULL \
	    "unfreeze_process_must_fail: unfreeze process $_pid ($_name) should have failed"
	r=1
	return $r
    fi

    LTP_print_step_info \
	"unfreeze_must_fail: PID: $_pid, error: $r"

    r=0
    return $r
}

###############################################################################

check_group_exists_in_ps()
{
    local _pgrp=$1
    local _name=`expr substr $2 1 15`
    local _number=$3
    local r=1

    local count=`pgrep -g $_pgrp $_name|wc -l`

    if [ $count -ge 1 ]; then
	if [ "$_number" = "" ] || [ $_number -eq $count ]; then
	    r=0
	fi
    fi


    if [ $r -ne 0 ]; then
	#try again in 5 seconds
	count=`pgrep -g $_pgrp $_name|wc -l`

	if [ $count -ge 1 ]; then
	    if [ "$_number" = "" ] || [ $_number -eq $count ]; then
		r=0
	    fi
	fi
    fi

    return $r
}

check_group_not_exists_in_ps()
{
    local _pgrp=$1
    local _name=`expr substr $2 1 15`
    local r=0

    pgrep -g $_pgrp $_name > /dev/null

    r=$?
    if [ $r -eq 1 ]; then # No processes matched
	r=0
    else
	r=1
    fi
    return $r
}

###############################################################################

kill_group()
{
    local _pgrp=$1
    local _name=$2
    local _cmd=`expr substr $_name 1 15`
    local r=0

    LTP_print_step_info \
	"kill_group $_pgrp $_name"

    # Kill
    pkill -g $_pgrp $_cmd

    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "kill_group (pkill): unable to kill group $_pgrp $_name (error $r)"
	return $r
    fi

    # Check no processes are visible anymore
    check_group_not_exists_in_ps $_pgrp $_name
    r=$?

    nb_try=0
    while [ $r -ne 0 ] && [ $nb_try -lt 20 ]; do
	sleep 1
	check_group_not_exists_in_ps $_pgrp $_name
	r=$?
	nb_try=$[$nb_try+1]
    done

    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "kill_group: $_name still visible with command pgrep (timeout too short ?)"
	return $r
    fi

    LTP_print_info \
	"kill_group $_pgrp $_name: $r"

    return $r
}

###############################################################################

restart_process()
{
    local _pid=$1
    local _version=$2
    local _name=$3
    local r=0

    # Restart process
    restart -q -t $_pid $_version

    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL \
	    "restart: failed to restart $_pid $version"
	return $r
    fi

    sleep 1

    # Check process is visible with ps
    check_group_exists_in_ps $_pid $_name

    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "ps: $_pid ($_name) is not visible with command ps"
	return $r
    fi

    LTP_print_step_info \
	"restart $_pid $_name: $r"

    return $r
}

restart_process_must_fail()
{
    local _pid=$1
    local _version=$2
    local _name=$3
    local r=0

    # Restart process
    restart $_pid $version > /dev/null 2>&1

    r=$?
    if [ $r -eq 0 ]; then
	tst_brkm TFAIL NULL \
	"restart_must_fail: restart $_pid $version should have failed"
	r=1
	return $r
    fi
    r=0

    LTP_print_step_info \
	"restart_must_fail: $r - PID: $_pid $version"

    return $r
}

###############################################################################

skip_test_if_only_one_node()
{
    local nrnodes=0

    if [ -e /proc/nodes/nrnodes ]; then
	nrnodes=`cat /proc/nodes/nrnodes|grep ONLINE|cut -d: -f2`
    fi

    if [ $nrnodes -lt 2 ]; then
	tst_resm TWARN "Kerrighed is running on only one node. Skipping test."
	return 0
    fi

    return 1
}

get_cpu_hosting_process()
{
    local _pid=$1

    ps axf -o pid,psr > /tmp/ps_${_pid}
    awk "\$1==$_pid { print \$2 }" /tmp/ps_${_pid}
}

cpu2node()
{
    local cpu=$1
    local NRCPUS=0

    if [ -e /sys/devices/system/cpu/kernel_max ]; then
	local cpu_max_id=`cat /sys/devices/system/cpu/kernel_max`
	NRCPUS=$[$cpu_max_id+1]
    else
        # WARNING: it depends on the kernel configuration :'(
	echo "WARNING: Fail to detect the number of CPUS (file /sys/devices/system/cpu/kernel_max does not exist)" >&2
	NRCPUS=8
    fi

    local node=$[$cpu/$NRCPUS]
    echo "$node"
}

get_node_hosting_process()
{
    local _pid=$1
    local cpu=`get_cpu_hosting_process ${_pid}`
    cpu2node $cpu
}

really_migrate_process()
{
    local _pid=$1
    local _fromnode=$2
    local _tonode=$3
    local r=0

    # migration request
    migrate $_pid $_tonode

    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL \
	"migrate: migrate $_pid $_tonode (error $r)"
	return $r
    fi

    # wait for migration to really been done
    local node=`get_node_hosting_process ${_pid}`
    local nb_try=0
    while [ "$node" = "$_fromnode" ] && [ $nb_try -lt 30 ]; do
	sleep 1
	node=`get_node_hosting_process ${_pid}`
	nb_try=$[$nb_try+1]
    done

    if [ "$node" = "$_fromnode" ]; then
	r=1
	tst_brkm TFAIL NULL \
	"migration of $_pid ($node) from $_fromnode to $_tonode has not yet been done"
    fi

    return $r
}

choose_another_node()
{
    local node=$1
    local another=0

    for another in /proc/nodes/node*; do
	local n=`echo $another | sed "s,/proc/nodes/node,,"`
	if [ "$n" != "$node" ]; then
	    echo "$$ - $node -> $n" >&2
	    echo "$n"
	    break
	fi
    done
}

migrate_process()
{
    local _pid=$1
    local _name=$2

    local _tonode=1
    local _fromcpu=`get_cpu_hosting_process ${_pid}`

    if [ "$_fromcpu" = "" ]; then
	r=1
	tst_brkm TFAIL NULL \
	    "migrate_process: no such process $_pid"
	return $r
    fi

    local _fromnode=`cpu2node $_fromcpu`
    local r=0

    # choose another node
    _tonode=`choose_another_node $_fromnode`

    LTP_print_step_info \
	" ask to migrate $_pid from $_fromnode ($_fromcpu) to $_tonode"

    really_migrate_process $_pid $_fromnode $_tonode

    r=$?

    LTP_print_info \
	"migrate: $r ($_pid from node $_fromnode to node $_tonode)"

    return $r
}

migrate_process_comeback()
{
    local _pid=$1
    local _name=$2
    local r=0;

    # WARNING: should be rewritten...

    migrate_process $_pid $_name
    r=$?

    LTP_print_info \
	"migrate_comeback: $r ($_pid)"

    return $r
}

###############################################################################

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

runcommand()
{
    local r=0
    local krg_cap=$1 # Kerrighed capabilities needed for the processes
    local nb_processes=$2 #number of awaited processes
    local nosync=$3

    if [ "$krg_cap" = "" ]; then
	echo "runcommand(): No Kerrighed capabilities given, if you really want \
to do that, give --" 1>&2
	return 1
    fi

    if [ "$nb_processes" = "" ]; then
	nb_processes=1
    fi

    # let the application run in its own session
    local sessionfile="/tmp/sid$$"

    # parse capabilities
    local optcap=""
    echo $krg_cap | grep -q CHECKPOINTABLE
    r=$?
    if [ $r -ne 0 ]; then
	optcap="-n"
    fi

    local oldcap=`krgcapset -s|grep "Inheritable Effective Capabilities: "|sed "s/Inheritable Effective Capabilities: //"`
    local captoset=`echo $krg_cap | sed "s/CHECKPOINTABLE//" | tr -s "," | sed "s/^+,/+/" | sed "s/,$//" | sed "s/^+$//" | sed "s/^--$//"`

    if [ "$captoset" != "" ]; then
	krgcapset -d $captoset;
	r=$?
	if [ $r -ne 0 ]; then
	    tst_brkm TFAIL NULL "Fail to set relevant capabilities ($captoset)"
	    return $r
	fi
    fi

    local sync=""
    if [ "$nosync" == "" ]; then
	sync="-s"
    fi

    krgcr-run $optcap -b -o $sessionfile -q $sync -- ${TESTCMD} ${TESTCMD_OPTIONS}
    r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to run the application with relevant capabilities"
	return $r
    fi

    # give old capabilities
    krgcapset -d $oldcap

    # give time to setsid to create the new session...
    local pid=`cat $sessionfile 2> /dev/null`

    if [ "$pid" = "" ]; then
	tst_brkm TFAIL NULL "Fail to read new session id"
	r=1
	return $r
    fi
    rm -rf "$sessionfile"

    PID=$pid

    # Check the exec has been done
    local cmd=`expr substr $TESTCMD 1 15`
    local count=`pgrep -g $pid $cmd|wc -l`

    if [ "$count" != "$nb_processes" ]; then
	pkill -g $pid
	tst_brkm TFAIL NULL "Error running \"${TESTCMD} ${TESTCMD_OPTIONS}\": invalid number of processes ($count/$nb_processes)"
	r=1
	return $r
    fi

    LTP_print_info \
	"RUN: pid=$pid"

    return $r
}

###############################################################################

CR_init_check()
{
    local cmd=""

    # Check if kerrighed tools are installed
    local krg_tools="krgcapset checkpoint restart migrate"
    for cmd in $krg_tools; do
	check_if_command_exists $cmd || return $?
    done

    # Check if bi command exists :-)
    check_if_command_exists $TESTCMD || return $?

    # Check if standard unix command exists
    local unix_tools="ps pgrep kill killall pkill grep rm mv cut wc awk sed"
    for cmd in $unix_tools; do
	check_if_command_exists $cmd || return $?
    done

    # Check if directory /var/chkpt exists
    mkdir -p /var/chkpt
    chmod 777 /var/chkpt
    chmod u+t /var/chkpt

    return 0
}

# Description:  - Check if required commands exits
#               - Export global variables
#               - Check if required files/directory exits
#               - Create temporary files and directories
#
# Return        - zero on success
#               - non zero on failure. return value from commands ($?)
CR_setup()
{
    export TST_TOTAL=1  # Total number of test cases in this file.
    LTPTMP=${TMP}       # Temporary directory to create files, etc.
    export TCID="setup" # Test case identifier
    export TST_COUNT=0  # Set up is initialized as test 0

    # Initialize cleanup function to execute on program exit.
    # This function will be called before the test program exits.
    trap "CR_internal_cleanup" 0

    # Parse options
    while getopts "hdc:" flag
    do
	case $flag in
	    h ) echo " -d : debug mode (more verbose)"
		echo " -c command : program to run (the one which will be checkpointed)"
		exit 0
		;;
	    c ) TESTCMD="$OPTARG";;
	    d ) DEBUG="yes";;
	    ? ) echo "*** unknow options"; exit 1;;
	esac
    done

    krgcapset -e -CHECKPOINTABLE

    return 0
}

# Function:     internal_cleanup
#
# Description   - ??
#
CR_internal_cleanup()
{
    local cmd=`expr substr $TESTCMD 1 15`
    pkill -g $PID $cmd 2> /dev/null
}

# Function:     cleanup
#
# Description   - remove temporary files and directories.
#
# Return        - zero on success
#
CR_cleanup()
{
    killall `echo "$TESTCMD" | cut -f1 -d" "` > /dev/null 2>&1
    rm -rf /var/chkpt/* > /dev/null 2>&1
    rm -rf /tmp/chkpt_result* > /dev/null 2>&1
    rm -rf /tmp/ps_* > /dev/null 2>&1
    rm -rf /tmp/ktp_sync_* > /dev/null 2>&1
    rm -rf /tmp/java_tmp_root_* > /dev/null 2>&1

    return 0
}
