#!/bin/bash
#
# File :        processsize_probe.sh
#
# Description:	Tests skeletton script
#
# Author:       Alexandre Lissy, alexandre.lissy@etu.univ-tours.fr
#
# History:      13 apr 2009 - Created - Alexandre Lissy
#


# Function:     setup
#
# Description:  - Setup the tests
#		- Check that /config/krg_scheduler/probes/processsize_probe exists
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
	RC=0
	grep "CONFIG_KRG_SCHED_PROCESSSIZE_PROBE=m" /boot/config-$(uname -r) >$LTPTMP/tst_config.out; RC=$?
	if [ $RC -ne 0 ]; then
		tst_brk TCONF $LTPTMP/tst_config.out NULL "processsize_probe module isn't enabled."
		return $RC
	fi

	RC=0
	ls /config/krg_scheduler/probes/processsize_probe >$LTPTMP/tst_ls.out; RC=$?
	if [ $RC -ne 0 ]; then
		tst_brk TBROK $LTPTMP/tst_ls.out NULL "processsize_probe isn't loaded."
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
	rm $LTPTMP/tst_proc.out
}

# Function:	test_init_process
# Description	Test number 1
#
test_init_process()
{
	TCID="test_init_process"
	TST_COUNT=1
	RC=0

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: Checking init process size"

	# exec test
	PID=1
	PAGES=$(((`grep VmSize /proc/$PID/status |awk '{ print $2 }'`*1024)/(`getconf PAGESIZE`)))
	PROBE=`(cat /config/krg_scheduler/probes/processsize_probe/total_vm/value|strings|grep "Process $PID:"|awk '{ print $3 }') 2>$LTPTMP/tst_proc.out`
	RC=$?
	if [ $RC -ne 0 ]
	then
		tst_resm TFAIL "Test #$TST_COUNT: init VmSize unreadable."
		return $RC
	else
		if [ $PAGES -eq $PROBE ]; then
			tst_resm TPASS "Test #$TST_COUNT: correct init VmSize."
		else
			tst_resm TFAIL "Test #$TST_COUNT: error. init VmSize:$PAGES. probe:$PROBE"
			return $RC
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

test_init_process || exit $RC

