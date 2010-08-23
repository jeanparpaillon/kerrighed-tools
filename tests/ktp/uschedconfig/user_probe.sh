#!/bin/bash
#
# File :        user_probe.sh
#
# Description:	Tests the user probe module
#
# Author:       Alexandre Lissy, alexandre.lissy@etu.univ-tours.fr
#
# History:      13 apr 2009 - Created - Alexandre Lissy
#


# Function:     setup
#
# Description:  - Setup the tests
#		- Check that /proc/kerrighed/interactive_user and /config/krg_scheduler/probes/user_probe exists
#
# Return        - zero on success
#               - non zero on failure. return value from commands ($RC)
setup()
{
	##	This variable is required by the LTP command line harness APIs
	##	to keep track of the number of testcases in this file. This
	##	variable is not local to this file.

	export TST_TOTAL=1  # Total number of test cases in this file.

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

	RC=0                # Exit values of system commands used

	#####
	## Setup test here

	# Silently "normal" fail if the probe isn't enabled
	grep "CONFIG_KRG_SCHED_USER_PROBE=m" /boot/config-$(uname -r) >$LTPTMP/tst_config.out; RC=$?
	if [ $RC -ne 0 ]; then
		tst_brk TCONF $LTPTMP/tst_config.out NULL "user_probe module isn't enabled."
		exit 0
	fi

        # Check if $MODROOT exists
        tst_resm TINFO "INIT: Checking for kernel module /proc entry : $MODROOT"
        ls $MODROOT >$LTPTMP/tst_ls.out; RC=$?
        if [ $RC -ne 0 ]
        then
                tst_brk TBROK $LTPTMP/tst_ls.out NULL "Are you sure module user_probe is loaded ? Reason: "
                return $RC
        fi

        ls /config/krg_scheduler/probes/user_probe >$LTPTMP/tst_ls.out; RC=$?
        if [ $RC -ne 0 ]; then
                tst_brk TBROK $LTPTMP/tst_ls.out NULL "user_probe isn't loaded."
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
	rm $LTPTMP/tst_config.out
	rm $LTPTMP/tst_ls.out
	rm $LTPTMP/tst_connected.out
}

# Function:	test_connected
# Description	Test that the probe reports the correct amount of users
#
test_connected()
{
	TCID="test_connected"
	TST_COUNT=1
	RC=0

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: Test user_probe/connected"

	# exec test
	VALUE=$((cat /config/krg_scheduler/probes/user_probe/connected/value) 2>$LTPTMP/tst_connected.out)
	CORRECT=$(cat $MODROOT/get)
	RC=$?
	if [ $RC -ne 0 ]
	then
		tst_resm TFAIL $LTPTMP/tst_connected.out \
		"Test #$TST_COUNT: failure to cat"
		return $RC
	else
		if [ $VALUE -ne $CORRECT ]
		then
			tst_resm TFAIL $LTPTMP/tst_connected.out \
			"Test #$TST_COUNT: incoherent values"
			RC=1
			return $RC
		else
			tst_resm TPASS \
			"Test #$TST_COUNT: coherent values."
		fi
	fi

	return $RC
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

test_connected || exit $RC

