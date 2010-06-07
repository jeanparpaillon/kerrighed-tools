#!/bin/bash
###############################################################################
##
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
# File :        lib_ipc.sh
#
# Description:  Sh "library" file to test Kerrighed IPC
#
# Author:       Matthieu Fertré, matthieu.fertre@kerlabs.com
#

wait_other_instances()
{
    local testid=$1
    local counterfunc=$2
    local syncfile="/tmp/ktp_sync_${testid}.${KTP_ROOT_PID}.${KTP_ITERATION}"

    LTP_print_step_info "waiting for every test to be ready"

    # waiting for everybody to be ready
    local nr_obj=`eval $counterfunc`
    local nr_try=0
    local MAX_NR_TRY=30
    while [ $nr_try -lt $MAX_NR_TRY ] && [ $nr_obj -lt $KTP_NR_PS ] && [ ! -f $syncfile ]; do
	sleep 1
	nr_obj=`eval $counterfunc`
	nr_try=$[$nr_try+1]
    done

    if [ $nr_try -ge $MAX_NR_TRY ]; then
	tst_brkm TFAIL NULL \
	   "Bad numbers of OBJ created : $nr_obj/$KTP_NR_PS (timeout to small ?)"
	return 1
    fi
    touch $syncfile

    LTP_print_step_info "every tests are ready!"

    return 0
}

###########################################################################
# SYSTEM V SHM
###########################################################################

generate_fixed_size_random_msg()
{
    local length=10
    local msg=""
    for i in `seq 1 $length`; do
	msg="$RANDOM$msg"
    done
    msg=`echo $msg|cut -b 10`
    echo $msg
}

create_sysv_shm()
{
    local shm_path=$1
    local msg_to_write=$2

    LTP_print_step_info "creating SYSV SHM from $shm_path"
    SHMID=`ipcshm-tool -c"$msg_to_write" $shm_path | cut -d':' -f1`
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to create SYSV SHM from $shm_path: $r"
    fi
    return $r
}

delete_sysv_shm()
{
    local shm_id=$1
    local shm_path=$2

    LTP_print_step_info "deleting SYSV SHM $shm_id ($shm_path)"
    ipcshm-tool -d -i $shm_id
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to delete SYSV SHM $shm_id ($shm_path): $r"
    fi
    return $r
}

dump_sysv_shm()
{
    local shm_id=$1
    local shm_path=$2
    local file=$3

    LTP_print_step_info "dumping SYSV SHM $shm_id ($shm_path)"
    ipccheckpoint -m $shm_id $file
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to checkpoint SYSV SHM $shm_id ($shm_path): $r"
    fi

    return $r
}

restore_sysv_shm()
{
    local shm_id=$1
    local shm_path=$2
    local file=$3

    LTP_print_step_info "restoring SYSV SHM $shm_id ($shm_path)"
    ipcrestart -m $file
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to restart SYSV SHM $shm_id ($shm_path): $r"
    fi
    return $r
}

check_sysv_shm_value()
{
    local shm_id=$1
    local shm_path=$2

    LTP_print_step_info "checking SYSV SHM $shm_id ($shm_path)"

    local written_msg=$3
    local read_msg=`ipcshm-tool -r1 -i $shm_id`
    if [ "$read_msg" != "$written_msg" ]; then
	tst_brkm TFAIL NULL \
	    "SYSV SHM $shm_id value has changed ('$read_msg' != '$written_msg')"
	return 1
    fi

    return 0
}

write_sysv_shm_value()
{
    local shm_id=$1
    local shm_path=$2
    local msg_to_write=$3

    LTP_print_step_info "updating SYSV SHM $shm_id ($shm_path): $msg_to_write"

    ipcshm-tool -q -w"$msg_to_write" -i $shm_id
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to update SYSV SHM $shm_id ($shm_path): $r"
    fi

    return $r
}

###########################################################################
# POSIX SHM
###########################################################################

create_posix_shm()
{
    local shm_name=$1
    local msg_to_write=$2

    LTP_print_step_info "creating POSIX SHM $shm_name"
    posixshm-tool -c"$msg_to_write" $shm_name > /dev/null
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to create POSIX SHM $shm_name: $r"
    fi
    return $r
}

delete_posix_shm()
{
    local shm_name=$1

    LTP_print_step_info "deleting POSIX SHM $shm_name"
    posixshm-tool -d $shm_name
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to delete POSIX SHM $shm_name: $r"
    fi
    return $r
}

dump_posix_shm()
{
    local shm_name=$1
    local file=$2

    LTP_print_step_info "dumping POSIX SHM $shm_name"
    cp /dev/shm/$shm_name $file
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to checkpoint POSIX SHM $shm_name: $r"
    fi

    return $r
}

restore_posix_shm()
{
    local shm_name=$1
    local file=$2

    LTP_print_step_info "restoring POSIX SHM $shm_name"
    cp $file /dev/shm/$shm_name
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to restart POSIX SHM $shm_name: $r"
    fi
    return $r
}

check_posix_shm_value()
{
    local shm_name=$1
    local written_msg=$2

    LTP_print_step_info "checking POSIX SHM $shm_name"


    local read_msg=`posixshm-tool -r1 $shm_name`
    if [ "$read_msg" != "$written_msg" ]; then
	tst_brkm TFAIL NULL \
	    "POSIX SHM $shm_name value has changed ('$read_msg' != '$written_msg')"
	return 1
    fi

    return 0
}

write_posix_shm_value()
{
    local shm_name=$1
    local msg_to_write=$2

    LTP_print_step_info "updating POSIX SHM $shm_name: $msg_to_write"

    posixshm-tool -q -w"$msg_to_write" $shm_name
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to update POSIX SHM $shm_id ($shm_path): $r"
    fi

    return $r
}

###########################################################################

create_sem()
{
    local sem_path=$1
    local nb=$2

    LTP_print_step_info "creating sem from $sem_path"
    SEMID=`ipcsem-tool -c$nb $sem_path | cut -d':' -f1`
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to create SEM from $sem_path: $r"
    fi
    return $r
}

delete_sem()
{
    local sem_id=$1
    local sem_path=$2

    LTP_print_step_info "deleting $sem_id ($sem_path)"
    ipcsem-tool -d -i $sem_id
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to delete SEM $sem_id ($sem_path): $r"
    fi
    return $r
}

save_sem_value()
{
    local sem_id=$1
    local sem_path=$2

    SEMVALUE=`ipcsem-tool -s -i $sem_id`
    local r=$?
    if [ $? -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to read values of SEM $sem_id ($sem_path): $r"
    fi
    return $r
}

check_sem_value()
{
    local sem_id=$1
    local sem_path=$2
    local original_value=$3

    LTP_print_step_info "checking $sem_id ($sem_path)"

    local value=`ipcsem-tool -s $sem_path`
    if [ "$original_value" != "$value" ]; then
	tst_brkm TFAIL NULL \
	    "SEM value has changed ('$original_value' != '$value')"
	return 1
    fi

    LTP_print_step_info "Check is OK: $value"

    return 0
}

check_sem_value_has_changed()
{
    local sem_id=$1
    local sem_path=$2
    local original_value=$3

    LTP_print_step_info "checking $sem_id ($sem_path)"

    local value=`ipcsem-tool -s $sem_path`
    if [ "$original_value" = "$value" ]; then
	tst_brkm TFAIL NULL \
	    "SEM value has not changed ('$original_value' != '$value')"
	return 1
    fi

    return 0
}

unlock_sem()
{
    local sem_id=$1
    local sem_path=$2
    local member=$3

    LTP_print_step_info "unlocking $sem_id ($sem_path)"
    ipcsem-tool -q -u$3 $sem_path
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to delete SEM $sem_id ($sem_path): $r"
    fi

    return $r
}

dump_sem()
{
    local sem_id=$1
    local sem_path=$2
    local file=$3

    LTP_print_step_info "dumping $sem_id ($sem_path)"
    ipccheckpoint -s $sem_id $file
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to checkpoint SEM $sem_id ($sem_path): $r"
    fi

    return $r
}

restore_sem()
{
    local sem_id=$1
    local sem_path=$2
    local file=$3

    LTP_print_step_info "restoring $sem_id ($sem_path)"
    ipcrestart -s $file
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to restart SEM $sem_id ($sem_path): $r"
    fi
    return $r
}

###########################################################################

create_msg()
{
    local msg_path=$1
    local msg_to_write=$2

    LTP_print_step_info "creating MSGQ from $msg_path"
    MSGID=`ipcmsg-tool -c -s"$msg_to_write" $msg_path | cut -d':' -f1`
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to create MSGQ from $msg_path: $r"
    fi
    return $r
}

delete_msg()
{
    local msg_id=$1
    local msg_path=$2

    LTP_print_step_info "deleting MSGQ $msg_id ($msg_path)"
    ipcmsg-tool -d -i $msg_id
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to delete SHM $msg_id ($msg_path): $r"
    fi
    return $r
}

check_msg_content()
{
    local msg_id=$1
    local msg_path=$2
    local original_content=$3

    LTP_print_step_info "checking MSGQ $msg_id ($msg_path)"

    local read_msg=`ipcmsg-tool -r -i $msg_id`
    if [ "$read_msg" != "$original_content" ]; then
	tst_brkm TFAIL NULL \
	    "MSGQ content has changed ('$read_msg' != '$original_content')"
	return 1
    fi

    return 0
}

send_message()
{
    local msg_id=$1
    local msg_path=$2
    local msg_to_write=$3

    LTP_print_step_info "sending message to MSGQ $msg_id ($msg_path)"
    ipcmsg-tool -q -s"$msg_to_write" -i $msg_id
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to send message to MSGQ $msg_id ($msg_path): $r"
    fi

    return $r
}

dump_msg()
{
    local msg_id=$1
    local msg_path=$2
    local file=$3

    LTP_print_step_info "dumping MSGQ $msg_id ($msg_path)"
    ipccheckpoint -q $msg_id $file
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to checkpoint MSGQ $msg_id ($msg_path): $r"
    fi

    return $r
}

restore_msg()
{
    local msg_id=$1
    local msg_path=$2
    local file=$3

    LTP_print_step_info "restoring MSGQ $msg_id ($msg_path)"
    ipcrestart -q $file
    local r=$?
    if [ $r -ne 0 ]; then
	tst_brkm TFAIL NULL "Fail to restart MSGQ $msg_id ($msg_path): $r"
    fi
    return $r
}
