#!/bin/bash
#
# File :        cpuspeed_probe.sh
#
# Description:	Tests cpuspeed_probe
#
# Author:       Alexandre Lissy, alexandre.lissy@etu.univ-tours.fr
#
# History:      13 apr 2009 - Created - Alexandre Lissy
#


# Function:     setup
#
# Description:  - Setup the tests
#		- Check that /config/krg_scheduler/probes/cpuspeed_probe exists
#
# Return        - zero on success
#               - non zero on failure. return value from commands ($RC)
setup()
{
	##	This variable is required by the LTP command line harness APIs
	##	to keep track of the number of testcases in this file. This
	##	variable is not local to this file.

	export TST_TOTAL=2  # Total number of test cases in this file.

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
	grep "CONFIG_KRG_SCHED_CPUSPEED_PROBE=m" /boot/config-$(uname -r) >$LTPTMP/tst_config.out; RC=$?
	if [ $RC -ne 0 ]; then
		tst_brk TCONF $LTPTMP/tst_config.out NULL "cpuspeed_probe module isn't enabled."
		return $RC
	fi

	RC=0
	ls /config/krg_scheduler/probes/cpuspeed_probe >$LTPTMP/tst_ls.out; RC=$?
	if [ $RC -ne 0 ]; then
		tst_brk TBROK $LTPTMP/tst_ls.out NULL "cpuspeed_probe isn't loaded."
	fi

	RC=0
	krgcapset -d +SEE_LOCAL_PROC_STAT >$LTPTMP/tst_locproc.out; RC=$?
	if [ $RC -ne 0 ]; then
		tst_brk TBROK $LTPTMP/tst_locproc.out NULL "Cannot set +SEE_LOCAL_PROC_STAT capability."
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
	rm $LTPTMP/tst_locproc.out
	rm $LTPTMP/tst_connected.out
	rm $LTPTMP/tst_speed.out
}

# Function:	num_cpus
# Description	Returns the number of CPUs.
num_cpus()
{
	return `grep -c 'processor' /proc/cpuinfo`
}

# Function:	test_connected
# Description	Test the 'connected' probe source of cpuspeed_probe
#
test_connected()
{
	TCID="connected"
	TST_COUNT=1
	RC=0

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: Test cpuspeed_probe/connected"

	# exec test
	CPUS=`grep -c 'processor' /proc/cpuinfo`
	VALUE=`(cat /config/krg_scheduler/probes/cpuspeed_probe/connected/value) 2>$LTPTMP/tst_connected.out`
	RC=$?

	if [ $RC -ne 0 ]
	then
		tst_resm TFAIL "Test #$TST_COUNT: failure"
		return $RC
	else
		if [ $VALUE -ne $CPUS ]
		then
			tst_resm TFAIL "Test #$TST_COUNT: failure: bad numer of CPUs"
		else
			tst_resm TPASS "Test #$TST_COUNT: ok"
		fi
	fi

	return $RC
}

# Function:	test_cpu_speed
# Description	Test the 'cpu_speed' probe source of cpuspeed_probe
#
test_cpu_speed()
{
	TCID="cpu_speed"
	TST_COUNT=2
	RC=0

	# Print test assertion.
	tst_resm TINFO \
	"Test #$TST_COUNT: Testing local CPU speed"

	count=1
	for line in $(grep "cpu MHz" /proc/cpuinfo|sed -e "s/ //g" -e "s/\t//g")
	do
		# exec test
		CPU=`echo "$line"|cut -d':' -f 2`
		CPUKHZ=`(head -n$count /config/krg_scheduler/probes/cpuspeed_probe/speed/value|tail -n+$count|cut -d' ' -f 2|cut -d'k' -f 1) 2>$LTPTMP/tst_speed.out`
		CPUKHZ=$(($CPUKHZ/1000))
		CPUMHZ=`echo "$CPU"|cut -d'.' -f1`

		if [ "$CPUKHZ" != "$CPUMHZ" ]; then
			tst_resm TFAIL "Test #$TST_COUNT: CPU ($count) speed doesn't match: probe=$CPUKHZ and cpuinfo=$CPUMHZ"
			return $RC
		else
			tst_resm TPASS "Test #$TST_COUNT: CPU ($count) speed match."
		fi
		count=$(($count+1))
	done

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

test_connected || exit $RC

test_cpu_speed || exit $RC

