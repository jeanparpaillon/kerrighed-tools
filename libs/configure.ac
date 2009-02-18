dnl configure.in for libkerrighed
dnl Copyright 2006-2007 INRIA-IRISA
dnl	       Jean Parpaillon <jean.parpaillon@irisa.fr>
dnl Copyright 2009 Kerlabs
dnl 	      Jean Parpaillon <jean.parpaillon@kerlabs.com>
dnl
dnl Process this file with autoconf to produce a configure script.

dnl autoconf version
AC_PREREQ(2.59)

dnl mandatory stuff
AC_INIT([Kerrighed Libs], [2.4.0], [kerrighed.users@irisa.fr])

dnl origin check (svn or release)
runver=`uname -r`
if test -d "$srcdir/../kernel"; then
   # Build from SVN
   default_linux_source=`cd "$srcdir/.." && pwd`/kernel
   default_linux_build=`cd .. && pwd`/kernel
else
   # Build from release
   default_linux_source=/lib/modules/$runver/source
   default_linux_build=/lib/modules/$runver/build
fi
toplevel_src=`cd "$srcdir/.." && pwd`
kerrighed_module_src="$toplevel_src/modules"

dnl check host and target
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

dnl options
AC_ARG_WITH([kernel],
	    [AS_HELP_STRING([--with-kernel=PATH],
	      		    [Path to kernel kernel source @<:@default=/lib/modules/`uname -r`/source@:>@])],
            [kernelsrc="$withval"; kernelbuild="$withval"],
	    [kernelsrc="$default_linux_source"; kernelbuild="$default_linux_build"])
kernelsrc=`cd $kernelsrc && pwd`
AC_SUBST([kernelsrc])

AC_ARG_WITH([kerrighed],
	    [AS_HELP_STRING([--with-kerrighed=PATH],
	      		    [Path to Kerrighed module headers @<:@default=../kerrighed@:>@])],
            [],
	    [with_kerrighed=$kerrighed_module_src])
with_kerrighed=`cd $with_kerrighed && pwd`
AC_SUBST([with_kerrighed])

AC_ARG_ENABLE([libkerrighed],
	      [AS_HELP_STRING([--disable-libkerrighed],
			      [Disable libkerrighed @<:@default=enable@:>@])],
	      [],
	      [enable_libkerrighed=yes])
AM_CONDITIONAL([BUILD_LIBKERRIGHED], [test "$enable_libkerrighed" = "yes"])

dnl check tools
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_PROG_INSTALL

dnl check headers
AC_HEADER_STDC

dnl Add strictness options to the compiler
CFLAGS="$CFLAGS -Wall -Werror"
AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)

dnl files to generate with automake
AC_CONFIG_FILES([Makefile include/Makefile libkerrighed/kerrighed.pc libkerrighed/Makefile])

dnl write all stuff
AC_OUTPUT

dnl
dnl Display results
dnl
if test "x$enable_libkerrighed" = "xyes"; then
   echo "********************************************************************"
   echo " Following libs will be built:"

   if test "x$enable_libkerrighed" = "xyes"; then
      echo "  - libkerrighed"
   fi

   echo "  - Kernel sources are in: $kernelsrc"
   echo "  - Kerrighed sources: $with_kerrighed"

   echo ""
   echo "********************************************************************"
fi

