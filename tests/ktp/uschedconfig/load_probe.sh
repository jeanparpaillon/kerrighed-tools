#!/bin/bash
#
# File :        load_probe.sh
#
# Description:	Script testing the load probe module
#
# Author:       Alexandre Lissy, alexandre.lissy@etu.univ-tours.fr
#
# History:      13 apr 2009 - Created - Alexandre Lissy
#		18 oct 2010 - Modified - Alexandre Lissy
#			- limit testing to cat not returning error in order to
#			  avoid race conditions in the number of processes.
#


# Function:     setup
#
# Description:  - Setup the tests
#		- Check that /config/krg_scheduler/probes/load_probe exists
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

	# Initialize cleanup function to execute on program exit.
	# This function will be called before the test program exits.
	trap "cleanup" 0

	# Initialize return code to zero.

	##	RC is used to store the return code from commands that are
	##	used in this test. This variable is local to this file.

	RC=0                # Exit values of system commands used

	#####
	## Setup test here
	#####

	# Silently "normal" fail if the probe isn't enabled
	grep "CONFIG_KRG_SCHED_LOAD_PROBE=m" /boot/config-$(uname -r) >$LTPTMP/tst_config.out; RC=$?
	if [ $RC -ne 0 ]; then
		tst_brk TCONF $LTPTMP/tst_config.out NULL "load_probe module isn't enabled."
		exit 0
	fi

	ls /config/krg_scheduler/probes/load_probe >$LTPTMP/tst_ls.out; RC=$?
	if [ $RC -ne 0 ]; then
		tst_brk TBROK $LTPTMP/tst_ls.out NULL "load_probe isn't loaded."
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
	rm -f $LTPTMP/tst_config.out
	rm -f $LTPTMP/tst_ls.out
	rm -f $LTPTMP/tst_cat.out
}

# Function:	test_active_tasks
# Description	Test the number of active tasks.
#
test_active_tasks()
{
	TCID="active_tasks"
	TST_COUNT=1
	RC=0

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: Test load_probe/active_tasks"

	VALUE=`(cat /config/krg_scheduler/probes/load_probe/active_tasks/value) 2>$LTPTMP/tst_cat.out`
	RC=$?
	if [ $RC -ne 0 ]
	then
		tst_resm TFAIL \
		"Test #$TST_COUNT: failure to cat"
	else
		tst_resm TPASS \
		"Test #$TST_COUNT: cat worked."
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

test_active_tasks || exit $RC

