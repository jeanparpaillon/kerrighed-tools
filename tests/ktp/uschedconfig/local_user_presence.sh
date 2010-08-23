#!/bin/bash
#
# File :        local_user_presence.sh
#
# Description:  Test case for the local_user_presence and local_user_notifier
#		kernel modules.
#
# Author:       Alexandre Lissy, alexandre.lissy@etu.univ-tours.fr
#
# History:      21 feb 2009 - Created - Alexandre Lissy
#


# Function:     setup
#
# Description:  - Setup the tests
#		- Check that /proc/kerrighed/interactive_user exists
#
# Return        - zero on success
#               - non zero on failure. return value from commands ($RC)
setup()
{
	##	This variable is required by the LTP command line harness APIs
	##	to keep track of the number of testcases in this file. This
	##	variable is not local to this file.

	export TST_TOTAL=9  # Total number of test cases in this file.

	# Set up LTPTMP (temporary directory used by the tests).

	##	Export PATH variable to point to the ltp-yyyymmdd/testcases/bin
	##	directory before running the tests manually, path is set up
	##	automatically when you use the runltp script.
	##	LTPTMP is user defined variables used only in this test case.
	##	These variables are local to this file.
	##	The TMP variable is exported by the runltp script.

	export LTPTMP=${TMP}       # Temporary directory to create files, etc.


	##	The TCID and TST_COUNT variables are required by the LTP
	##	command line harness APIs, these variables are not local
	##	to this program.

	export TCID="setup" # Test case identifier
	export TST_COUNT=0  # Set up is initialized as test 0
	export MODROOT="/proc/kerrighed/interactive_user"

	# Initialize cleanup function to execute on program exit.
	# This function will be called before the test program exits.
	trap "cleanup" 0

	# Initialize return code to zero.

	##	RC is used to store the return code from commands that are
	##	used in this test. This variable is local to this file.

	# Silently "normal" fail if the probe isn't enabled
	RC=0                # Exit values of system commands used
	grep "CONFIG_KRG_SCHED_LOCAL_USER_PRESENCE=m" /boot/config-$(uname -r) >$LTPTMP/tst_config_presence.out; RC=$?
	if [ $RC -ne 0 ]; then
		tst_brk TCONF $LTPTMP/tst_config_presence.out NULL "local_user_presence module isn't enabled."
		exit 0
	fi

	grep "CONFIG_KRG_SCHED_LOCAL_USER_NOTIFIER=m" /boot/config-$(uname -r) >$LTPTMP/tst_config_notifier.out; RC=$?
	if [ $RC -ne 0 ]; then
		tst_brk TCONF $LTPTMP/tst_config_notifier.out NULL "local_user_notifier module isn't enabled."
		exit 0
	fi

	# Load kerrighed-user-local-presence module
	tst_resm TINFO "INIT: Loading local_user_presence kernel module"
	modprobe local_user_presence >$LTPTMP/tst_mod_presence.out; RC=$?
	if [ $RC -ne 0 ]
	then
		tst_brk TBROK $LTPTMP/tst_mod_presence.out NULL "Cannot load local_user_presence kernel module. Reason: "
		return $RC
	fi

	# Load local_user_notifier module
	tst_resm TINFO "INIT: Loading local_user_notifier kernel module"
	modprobe local_user_notifier >$LTPTMP/tst_mod_notifier.out; RC=$?
	if [ $RC -ne 0 ]
	then
		tst_brk TBROK $LTPTMP/tst_mod_notifier.out NULL "Cannot load local_user_notifier kernel module. Reason: "
		return $RC
	fi

	# Check if $MODROOT exists
	tst_resm TINFO "INIT: Checking for kernel module /proc entry : $MODROOT"
	ls $MODROOT >$LTPTMP/tst_ls.out; RC=$?
	if [ $RC -ne 0 ]
	then
		tst_brk TBROK $LTPTMP/tst_ls.out NULL "Are you sure modules local_user_notifier and local_user_presence are loaded ? Reason: "
		return $RC
	fi

	return $RC
}

# Function:     cleanup
#
# Description   - remove temporary files and directories.
#
# Return        - zero on success
#               - non zero on failure. return value from commands ($RC)
cleanup()
{
	## Clean up code goes here
	rm -f \
		$LTPTMP/tst_config_presence.out \
		$LTPTMP/tst_config_notifier.out \
		$LTPTMP/tst_mod_presence.out \
		$LTPTMP/tst_mod_notifier.out \
		$LTPTMP/tst_ls.out \
		$LTPTMP/tst_get.out \
		$LTPTMP/tst_isfree.out \
		$LTPTMP/tst_isused.out \
		$LTPTMP/tst_simpleconnection*.out \
		$LTPTMP/tst_simpledisconnection*.out \
		$LTPTMP/tst_complexconnection*.out \
		$LTPTMP/tst_complexdisconnection*.out
}

# Function:	read_entry
#
# Description	- Make a cat on a specified file.
#
# Return	- What cat returned.
read_entry()
{
	ENTRYNAME=$1
	OUTPUT=$2
	RC=`(cat $MODROOT/$ENTRYNAME) 2>$OUTPUT`
	return $RC
}

# Function:	write_entry
#
# Description	- Make a echo on a specified file.
#
# Return	- What echo returned.
write_entry()
{
	ENTRYNAME=$1
	OUTPUT=$2
	(echo 1 > $MODROOT/$ENTRYNAME) 2>$OUTPUT
	return $?
}

# Function:	local_user_get
# Description	- Test if a freshly loaded module returns on /get 0
#
local_user_get()
{
	TCID="local_user_get"
	TST_COUNT=1
	RC=0

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: cat $MODROOT/get = 0"

	read_entry get $LTPTMP/tst_get.out
	RC=$?
	if [ $RC -ne 0 ]
	then
		tst_resm TFAIL $LTPTMP/tst_get.out \
		"Test #$TST_COUNT: Cannot get 0 from $MODROOT/get Reason:"
		return $RC
	else
		tst_resm TPASS \
		"Test #$TST_COUNT: Reading /get works."
	fi
	return $RC
}

# Function:	local_user_free
# Description	- Test if a freshly loaded module returns 1 on /isfree
#
local_user_isfree()
{
	TCID="local_user_free"
	TST_COUNT=2
	RC=0

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: cat $MODROOT/isfree = 1"

	read_entry isfree $LTPTMP/tst_isfree.out
	RC=$?
	if [ $RC -ne 1 ]
	then
		tst_res TFAIL $LTPTMP/tst_isfree.out \
		"Test #$TST_COUNT: Cannot get 1 from $MODROOT/isfree Reason:"
		RC=1
		return $RC
	else
		tst_resm TPASS \
		"Test #$TST_COUNT: Reading /isfree works."
		RC=0
	fi
	return 0
}

# Function:	local_user_used
# Description	- Test if a freshly loaded module returns on 0 /isused
#
local_user_isused()
{
	TCID="local_user_used"
	TST_COUNT=3
	RC=0

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: cat $MODROOT/isused = 0"

	read_entry isused $LTPTMP/tst_isused.out
	RC=$?
	if [ $RC -ne 0 ]
	then
		tst_res TFAIL $LTPTMP/tst_isused.out \
		"Test #$TST_COUNT: Cannot get 0 from $MODROOT/isused Reason:"
		return $RC
	else
		tst_resm TPASS \
		"Test #$TST_COUNT: Reading /isused works."
	fi
	return 0
}

# Function:	local_user_simpleconnection
# Description	- Write to /connection
#		- Check that we have 1 for /get
#
local_user_simpleconnection()
{
	TCID="local_user_simpleconnection"
	TST_COUNT=4
	RC=0

	write_entry connection $LTPTMP/tst_simpleconnection.out

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: cat $MODROOT/get = 1"

	read_entry get $LTPTMP/tst_simpleconnection2.out
	RC=$?
	if [ $RC -ne 1 ]
	then
		tst_res TFAIL $LTPTMP/tst_simpleconnection2.out \
		"Test #$TST_COUNT: Cannot get 1 from $MODROOT/get Reason:"
		RC=1
		return $RC
	else
		tst_resm TPASS \
		"Test #$TST_COUNT: User is counted."
		RC=0
	fi
	return 0
}

# Function:	local_user_simplefree
# Description	- Check that /isfree returns 0
#
local_user_simplefree()
{
	TCID="local_user_simplefree"
	TST_COUNT=5
	RC=0

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: cat $MODROOT/isfree = 0"

	read_entry isfree $LTPTMP/tst_simplefree.out
	RC=$?
	if [ $RC -ne 0 ]
	then
		tst_res TFAIL $LTPTMP/tst_simplefree.out \
		"Test #$TST_COUNT: Cannot get 0 from $MODROOT/isfree Reason:"
		return $RC
	else
		tst_resm TPASS \
		"Test #$TST_COUNT: Node is no more free."
	fi
	return 0
}

# Function:	local_user_isused
# Description	- Check that /isused returns 1
#
local_user_simpleused()
{
	TCID="local_user_isused"
	TST_COUNT=6
	RC=0

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: cat $MODROOT/isused = 1"

	read_entry isused $LTPTMP/tst_simpleused.out
	RC=$?
	if [ $RC -ne 1 ]
	then
		tst_res TFAIL $LTPTMP/tst_simpleused.out \
		"Test #$TST_COUNT: Cannot get 1 from $MODROOT/isused Reason:"
		RC=1
		return $RC
	else
		tst_resm TPASS \
		"Test #$TST_COUNT: Node is used."
		RC=0
	fi
	return 0
}

# Function:	local_user_simpledisconnection
# Description	- Write to /disconnection
#		- Check that we have 0 for /get
#
local_user_simpledisconnection()
{
	TCID="local_user_disconnection"
	TST_COUNT=7
	RC=0

	write_entry disconnection $LTPTMP/tst_simpledisconnection.out

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: cat $MODROOT/get = 0"

	read_entry get $LTPTMP/tst_simpledisconnection2.out
	RC=$?
	if [ $RC -ne 0 ]
	then
		tst_res TFAIL $LTPTMP/tst_simpledisconnection2.out \
		"Test #$TST_COUNT: Cannot get 0 from $MODROOT/get Reason:"
		return $RC
	else
		tst_resm TPASS \
		"Test #$TST_COUNT: User is disconnected."
	fi
	return 0
}

# Function:	local_user_complexconnection
# Description	- Make 3 writes to /connection
#		- Check that /get returns 3
#
local_user_complexconnection()
{
	TCID="local_user_complexconnection"
	TST_COUNT=8
	RC=0

	write_entry connection $LTPTMP/tst_complexconnection1.out
	write_entry connection $LTPTMP/tst_complexconnection2.out
	write_entry connection $LTPTMP/tst_complexconnection3.out

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: cat $MODROOT/get = 3"

	read_entry get $LTPTMP/tst_complexconnection_get.out
	RC=$?
	if [ $RC -ne 3 ]
	then
		tst_res TFAIL $LTPTMP/tst_complexconnection_get.out \
		"Test #$TST_COUNT: Cannot get 3 from $MODROOT/get Reason:"
		RC=1
		return $RC
	else
		tst_resm TPASS \
		"Test #$TST_COUNT: We can count 3 connected users."
		RC=0
	fi
	return 0
}

# Function:	local_user_complexdisconnection
# Description	- Make 5 writes to /disconnection
#		- Check that /get return 0 now
#
local_user_complexdisconnection()
{
	TCID="local_user_complexdisconnection"
	TST_COUNT=9
	RC=0

	write_entry disconnection $LTPTMP/tst_complexdisconnection1.out
	write_entry disconnection $LTPTMP/tst_complexdisconnection2.out
	write_entry disconnection $LTPTMP/tst_complexdisconnection3.out
	write_entry disconnection $LTPTMP/tst_complexdisconnection4.out
	write_entry disconnection $LTPTMP/tst_complexdisconnection5.out

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: cat $MODROOT/get = 0"

	read_entry get $LTPTMP/tst_complexdisconnection_get.out
	RC=$?
	if [ $RC -ne 0 ]
	then
		tst_res TFAIL $LTPTMP/tst_complexdisconnection_get.out \
		"Test #$TST_COUNT: Cannot get 0 from $MODROOT/get Reason:"
		return $RC
	else
		tst_resm TPASS \
		"Test #$TST_COUNT: We didn't went down zero while more disconnections occured than connections."
	fi
	return 0
}

# Function:     main
#
# Description:  - Execute all tests, exit with test status.
#
# Exit:         - zero on success
#               - non-zero on failure.
#
RC=0    # Return value from setup, and test functions.

setup  || exit $RC

local_user_get || exit $RC

local_user_isfree || exit $RC

local_user_isused || exit $RC

local_user_simpleconnection || exit $RC

local_user_simplefree || exit $RC

local_user_simpleused || exit $RC

local_user_simpledisconnection || exit $RC

local_user_complexconnection || exit $RC

local_user_complexdisconnection || exit $RC

