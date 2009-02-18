#!/usr/bin/python
# coding=utf8

#####################################################################
# Copyright (c) 2008 Kerlabs
#                    Jean Parpaillon <jean.parpaillon@kerlabs.com>
#                    All Rights Reserved
#####################################################################

#####################################################################
#
# Description:
#   Output Rmax value from hpl benchmark output
#
#####################################################################

import sys
import re

version = '''extractrmax 0.1

Copyright (C) 2008 Kerlabs

This is free software.  You may redistribute copies of it under the terms of
the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.
There is NO WARRANTY, to the extent permitted by law.

Written by Jean Parpaillon.'''

def usage():
    """ Print command usage
    """
    print "Usage: " + sys.argv[0] + " <file>|-"

def main():
    infile = None
    
    if len(sys.argv[1:]) > 0:
        if sys.argv[1].strip() == '-':
            infile = sys.stdin
        else:
            try:
                infile = open(sys.argv[1])
            except Exception, e:
                print e
                raise SystemExit(2)
    else:
        usage()
        raise SystemExit(1)

    resultRe = re.compile(r'^WR.*')
    maxRes = 0
    for line in infile:
        if resultRe.match(line):
            fields = line.split()
            if len(fields) == 7:
                f = float(fields[6])
                if f > maxRes:
                    maxRes = f

    print "Rmax: %e" % maxRes

    infile.close()

if __name__ == "__main__":
    main()
