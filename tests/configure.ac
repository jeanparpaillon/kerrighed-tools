dnl configure.in for kerrighed tests tools
dnl Copyright 2006-2007 INRIA-IRISA
dnl                     Jean Parpaillon <jean.parpaillon@irisa.fr>
dnl Copyright 2009
dnl		Jean Parpaillon <jean.parpaillon@kerlabs.com>
dnl
dnl Process this file with autoconf to produce a configure script.

dnl autoconf version
AC_PREREQ(2.59)

dnl mandatory stuff
AC_INIT([Kerrighed Tests], [2.4.0], [kerrighed.users@irisa.fr])

dnl check host and target
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

AC_PROG_LIBTOOL

dnl options
srcdir=`cd $srcdir && pwd`
AC_ARG_ENABLE([external-kerrighed],
	      [AS_HELP_STRING([--enable-external-kerrighed],
			      [Compile with external libkerrighed @<:@default: use internal if present@:>@])],
	      [use_external="$enableval"],
	      [use_external="auto"])
if test "x$use_external" = "xauto"; then
   AC_MSG_CHECKING([presence of internal libkerrighed])
   if test -f "$srcdir/../libs/include/kerrighed.h"; then
      use_external="no"
      AC_MSG_RESULT([yes])
   else
      use_external="yes"
      AC_MSG_RESULT([no])
   fi
fi
AM_CONDITIONAL([BUILD_LIBS], [test "$use_external" = "no"])

AC_ARG_ENABLE([ktp],
	      [AS_HELP_STRING([--disable-ktp],
			      [Disable 'ktp' part @<:@default: enable@:>@])],
	      [],
	      [enable_ktp=yes])
AM_CONDITIONAL([ENABLE_KTP], [test "$enable_ktp" = "yes"])

AC_ARG_WITH([ltp-base],
	    [AS_HELP_STRING([--with-ltp-base=PATH],
			    [Path to ltp base dir @<:@default=/usr/lib/debian-test/tests/linux@:>@])],
	    [ltpbase="$withval"],
	    [ltpbase="/usr/lib/debian-test/tests/linux"])
AC_SUBST([ltpbase])

AC_ARG_ENABLE([apps],
	      [AS_HELP_STRING([--disable-apps],
			      [Disable 'apps' part @<:@default: enable@:>@])],
	      [],
	      [enable_apps=yes])
AM_CONDITIONAL([ENABLE_APPS], [test "$enable_apps" = "yes"])

AC_ARG_ENABLE([proc],
	      [AS_HELP_STRING([--disable-proc],
			      [Disable 'proc' part @<:@default: enable@:>@])],
	      [],
	      [enable_proc=yes])
AM_CONDITIONAL([ENABLE_PROC], [test "$enable_proc" = "yes"])

AC_ARG_ENABLE([benchmark],
	      [AS_HELP_STRING([--disable-benchmark],
			      [Disable 'benchmark' part @<:@default: enable@:>@])],
	      [],
	      [enable_benchmark=yes])
AM_CONDITIONAL([ENABLE_BENCHMARK], [test "$enable_benchmark" = "yes"])

dnl check tools AC_PROG_CC AC_PROG_LIBTOOL AC_PROG_MAKE_SET
AC_PROG_LN_S AC_PROG_INSTALL

dnl check headers
AC_HEADER_STDC
if test "x$use_external" = "xyes"; then
   AC_CHECK_LIB(kerrighed,
		krg_capset,
		[],
		[AC_MSG_ERROR([
***
*** libkerrighed can not be found
***])])
fi

dnl files to generate with automake
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([include/Makefile])
AC_CONFIG_FILES([apps/Makefile])
AC_CONFIG_FILES([proc/Makefile])
AC_CONFIG_FILES([ktp/Makefile])
AC_CONFIG_FILES([ktp/cr/Makefile])
AC_CONFIG_FILES([ktp/krgltp-smp.sh])
AC_CONFIG_FILES([benchmark/Makefile])

dnl write all stuff
AC_OUTPUT

###
### Display results
###
echo "********************************************************************"
echo "  Kerrighed tests configuration is now complete"
echo "********************************************************************"
echo ""
echo "    - apps                               : $enable_apps"
echo "    - proc                               : $enable_proc"
echo "    - ktp                                : $enable_ktp"
echo "    - benchmark                          : $enable_benchmark"
