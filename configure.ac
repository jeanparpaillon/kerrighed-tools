dnl configure.in for Kerrighed project.
dnl Copyright 2006 IRISA, All rights reserved
dnl Copyright 2007-2009 Kerlabs, All rights reserved
dnl
dnl AUTHORS:
dnl  Jean Parpaillon <jean.parpaillon@kerlabs.com>,
dnl  Louis Rilling <louis.rilling@kerlabs.com>
dnl
dnl Process this file with autoconf to produce a configure script.

dnl autoconf version
AC_PREREQ(2.59)

dnl mandatory stuff
AC_INIT([kerrighed], [2.5.0], [kerrighed.users@irisa.fr])

dnl check host and target
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE([-Wno-portability])
# http://sources.redhat.com/automake/automake.html#maintainer_002dmode
#AM_MAINTAINER_MODE
AC_CONFIG_MACRO_DIR([m4])

dnl Add strictness options to the compiler
CFLAGS="$CFLAGS -Wall -Werror"

AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)

dnl check tools
srcdir=`cd $srcdir && pwd`
builddir=`pwd`

AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_PROG_INSTALL
AC_HEADER_STDC

###
### BEGIN KERNEL OPTIONS
###
AC_ARG_ENABLE([kernel],
	      [AS_HELP_STRING([--disable-kernel],
			      [Disable kernel preparation (fetch linux sources and apply Kerrighed patch) @<:@default=enable@:>@])],
	      [],
	      [enable_kernel=yes])
AM_CONDITIONAL([ENABLE_KERNEL], [test "$enable_kernel" = "yes"])

dnl Linux vanilla version on which is based Kerrighed
vanilla_linux_version=2.6.29-rc4
AC_SUBST([vanilla_linux_version])

AC_ARG_WITH([kernel-mirror],
	    [AS_HELP_STRING([--with-kernel-mirror],
			    [kernel.org mirror used to get vanilla kernel @<:@default=ftp.eu.kernel.org@:>@])],
	    [],
	    [kernel_mirror="ftp.eu.kernel.org"])
AC_SUBST([kernel_mirror])

if test "x$enable_kernel" = "xyes"; then
   AC_CHECK_PROG(bzip2, bzip2, yes, no)
   if test -z "$bzip2"; then
      AC_MSG_ERROR([
            	*** You need bzip2 tool])
   fi
   AC_CHECK_PROG(patch, patch, yes, no)
   if test -z "$patch"; then
      AC_MSG_ERROR([
              *** You need patch tool])
   fi
   AC_CHECK_PROG(wget, wget, yes, no)
   if test -z "$wget"; then
      AC_MSG_ERROR([
              *** You need wget tool])
   fi
fi
###
### END KERNEL OPTIONS
###

###
### BEGIN LIBRARIES CHECK
###
AC_ARG_ENABLE([libkerrighed],
	      [AS_HELP_STRING([--disable-libkerrighed],
			      [Disable libkerrighed (disable also tools compilation) @<:@default=enable@:>@])],
	      [],
	      [enable_libkerrighed=yes])
AM_CONDITIONAL([ENABLE_LIBKERRIGHED], [test "$enable_libkerrighed" = "yes"])

AH_TEMPLATE([CAPABILITY], [Capabilities part of libkerrighed])
AC_ARG_ENABLE([capability],
	      [AS_HELP_STRING([--disable-capability],
			      [Disable capability part of libkerrighed @<:@default=enable@:>@])],
	      [],
	      [enable_capability=yes])
AM_CONDITIONAL([ENABLE_CAPABILITY], [test "x$enable_capability" = "xyes"])
if test "x$enable_capability" = "xyes"; then
   AC_DEFINE([CAPABILITY])
fi
###
### END LIBRARIES CHECK
###

###
### BEGIN TOOLS CHECK
###
AC_ARG_ENABLE([tools],
	      [AS_HELP_STRING([--disable-tools],
			      [Disable tools @<:@default=enable@:>@])],
	      [],
	      [enable_tools=yes])
if test "x$enable_libkerrighed" != "xyes"; then
   enable_tools=no
   AC_MSG_WARN([Disabling libkerrighed also disable tools compilation])
fi

AC_ARG_ENABLE([service],
              [AS_HELP_STRING([--disable-service],
                              [Do not install kerrighed service @<:@default: enable if lsb found@:>@])],
              [],
              [enable_service="yes"])
if test "$enable_service" = "yes"; then
   AC_CHECK_PROG([lsb], [lsb_release], [yes], [no])
   if test "$lsb" = "no"; then
   AC_MSG_ERROR([
        *** You need lsb tool (or --disable-service)])
   fi
else
   lsb="no"
fi
if test "$lsb" = "yes"; then
   AC_MSG_CHECKING([distribution])
   DIST_ID=`lsb_release -s -i`
   DIST_RELEASE=`lsb_release -s -r`
   AC_MSG_RESULT([$DIST_ID $DIST_RELEASE])

   AC_SUBST([DIST_ID])
   AC_SUBST([DIST_RELEASE])

   AC_MSG_CHECKING([if distribution is supported])
   if test -d "$srcdir/scripts/distros/$DIST_ID"; then
      AC_MSG_RESULT([yes])
      distro="yes"
   else
      AC_MSG_RESULT([no])
      distro="no"
      AC_MSG_ERROR([
        *** Your distribution is not supported. Helper scripts won't work. Just launch configure with --disable-service.])
   fi
fi
AM_CONDITIONAL([ENABLE_SERVICE], [test "$lsb" = "yes" -a "$distro" = "yes"])
###
### END TOOLS CHECK
###


###
### BEGIN TESTS CHECK
###
AC_ARG_ENABLE([tests],
 	      [AS_HELP_STRING([--enable-tests],
 			      [Globally enable tests @<:@default=disable@:>@])],
 	      [],
 	      [enable_tests=no])
AM_CONDITIONAL([ENABLE_TESTS], [test "x$enable_tests" = "xyes"])

AC_ARG_ENABLE([tests-ktp],
	      [AS_HELP_STRING([--disable-tests-ktp],
			      [Disable 'ktp' tests @<:@default: enable@:>@])],
	      [],
	      [enable_tests_ktp=yes])
AC_ARG_ENABLE([tests-apps],
	      [AS_HELP_STRING([--disable-tests-apps],
			      [Disable 'apps' tests @<:@default: enable@:>@])],
	      [],
	      [enable_tests_apps=yes])
AC_ARG_ENABLE([tests-proc],
	      [AS_HELP_STRING([--disable-tests-proc],
			      [Disable 'proc' tests @<:@default: enable@:>@])],
	      [],
	      [enable_tests_proc=yes])
AC_ARG_ENABLE([tests-benchmark],
	      [AS_HELP_STRING([--disable-tests-benchmark],
			      [Disable 'benchmark' tests @<:@default: enable@:>@])],
	      [],
	      [enable_tests_benchmark=yes])

AM_CONDITIONAL([ENABLE_KTP], [test "$enable_tests_ktp" = "yes" -a "x$enable_tests" = "xyes"])
AM_CONDITIONAL([ENABLE_APPS], [test "$enable_tests_apps" = "yes" -a "x$enable_tests" = "xyes"])
AM_CONDITIONAL([ENABLE_PROC], [test "$enable_tests_proc" = "yes" -a "x$enable_tests" = "xyes"])
AM_CONDITIONAL([ENABLE_BENCHMARK], [test "$enable_tests_benchmark" = "yes" -a "x$enable_tests" = "xyes"])

AC_ARG_WITH([ltp-base],
	    [AS_HELP_STRING([--with-ltp-base=PATH],
			    [Path to ltp base dir @<:@default=/usr/lib/debian-test/tests/linux@:>@])],
	    [ltpbase="$withval"],
	    [ltpbase="/usr/lib/debian-test/tests/linux"])
AC_SUBST([ltpbase])
###
### END TESTS CHECK
###

###
### BEGIN CHECK FOR DOCBOOK TOOLS
###
AC_CHECK_PROG(xsltproc, xsltproc, yes, no)
###
### END CHECK FOR DOCBOOK TOOLS
###

AC_CONFIG_HEADERS([config.h])

dnl files to generate with automake
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([kernel/Makefile])
AC_CONFIG_FILES([libs/Makefile libs/include/Makefile libs/libkerrighed/kerrighed.pc libs/libkerrighed/Makefile])
AC_CONFIG_FILES([tools/Makefile])
AC_CONFIG_FILES([scripts/Makefile])
AC_CONFIG_FILES([man/Makefile])
AC_CONFIG_FILES([tests/Makefile tests/include/Makefile tests/apps/Makefile tests/proc/Makefile])
AC_CONFIG_FILES([tests/ktp/Makefile tests/ktp/cr/Makefile tests/benchmark/Makefile])
AC_CONFIG_FILES([tests/ktp/krgltp-smp.sh])

dnl write all stuff
AC_OUTPUT

###
### Display results
###
echo "********************************************************************"
echo "  Kerrighed configuration is now complete"
echo "********************************************************************"
echo ""

echo "    - Sources dir              : $srcdir"
if test "$srcdir" != "$builddir"; then
   echo "    - Build dir                : $builddir"
fi
echo ""
echo "    - Kernel preparation       : $enable_kernel"
echo ""
echo "    - libkerrighed             : $enable_libkerrighed"
echo ""
echo "    - Kerrighed tools          : $enable_tools"
if test "$enable_service" = "no"; then
   echo "    - Kerrighed service not installed (disabled)"
else
   if test "$lsb" = "yes"; then
      echo "    - Detected distribution    : $DIST_ID $DIST_RELEASE"
      if test "$distro" = "yes"; then
         echo "    - Kerrighed service will be installed"
      else
         echo "    - Kerrighed service won't be installed (distribution not supported)."
         echo "      You can send a request at $PACKAGE_BUGREPORT."
      fi
   else
      echo "    - Kerrighed service not installed (missing lsb tools)"
      echo "      Please install lsb_release"
   fi
fi
echo ""
echo "    - Manpages build           : $xsltproc (only needed in maintainer mode)"
echo ""
echo "    - Kerrighed tests          : $enable_tests"
if test "x$enable_tests" = "xyes"; then
echo "    - Kerrighed apps tests     : $enable_tests_apps"
echo "    - Kerrighed proc tests     : $enable_tests_proc"
echo "    - Kerrighed ktp tests      : $enable_tests_ktp"
echo "    - Kerrighed benchmark tests: $enable_tests_benchmark"
fi

echo ""
echo "********************************************************************"
echo ""
echo " To build the tools:"

echo " - run 'make' to build tools, libs, tests and so on"
echo " - as root, run 'make install'"
echo ""
if test "$enable_kernel" = "yes"; then
echo " To build the Kerrighed kernel, cd into kernel/linux-$vanilla_linux_version"
echo " and build a Linux kernel as usual."
echo ""
fi
echo "********************************************************************"
