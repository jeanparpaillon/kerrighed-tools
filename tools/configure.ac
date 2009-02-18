dnl configure.in for kerrighed tools
dnl Copyright 2006-2007 INRIA-IRISA
dnl			Jean Parpaillon <jean.parpaillon@irisa.fr>
dnl Copyright 2009 kerlabs
dnl 	      	   Jean Parpaillon <jean.parpaillon@kerlabs.com>
dnl 
dnl Process this file with autoconf to produce a configure script.

dnl autoconf version
AC_PREREQ(2.59)

dnl mandatory stuff
AC_INIT([Kerrighed Tools], [2.4.0], [kerrighed.users@irisa.fr])

dnl check host and target
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

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
if test "x$use_external" = "xyes"; then
   KERRIGHED_L=`pkg-config --libs-only-L kerrighed`
   KERRIGHED_I=`pkg-config --cflags-only-I kerrighed`
   AC_SUBST(KERRIGHED_L)
   AC_SUBST(KERRIGHED_I)
else
   KERRIGHED_L="-L`pwd`/src"
   KERRIGHED_I="-I$srcdir/../libs/include"
   AC_SUBST(KERRIGHED_L)
   AC_SUBST(KERRIGHED_I)
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
### BEGIN CHECK FOR DOCBOOK TOOLS
###
AC_ARG_ENABLE([man],
	      [AS_HELP_STRING([--disable-man],
			      [Disable manpages @<:@default=enable@:>@])],
	      [],
	      [enable_man=yes])
if test "$enable_man" = "yes"; then
   AC_CHECK_PROG(DB2MAN, xmlto, xmlto, )
   if test -z "$DB2MAN"; then
      AC_MSG_ERROR([
	  *** You need xmlto tool (or --disable-man)])
   fi
fi
AC_SUBST(DB2MAN)
AM_CONDITIONAL([ENABLE_MAN], [test -n "$DB2MAN"])
###
### END CHECK FOR DOCBOOK TOOLS
###

dnl check tools
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_PROG_INSTALL

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

dnl Add strictness options to the compiler
CFLAGS="$CFLAGS -Wall -Werror"
AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)

dnl files to generate with automake
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([man/Makefile])
AC_CONFIG_FILES([scripts/Makefile])

dnl write all stuff
AC_OUTPUT

###
### Display results
###
echo "********************************************************************"
echo "  Kerrighed tools configuration is now complete"
echo ""
echo -n "  - Use "
if test "$use_external" = "yes"; then
   echo -n "external"
else
   echo -n "internal"
fi
echo " libkerrighed"

echo -n "  - Documentation (manpages): "
if test -n "$DB2MAN"; then
   echo "yes"
else
   echo "no"
fi

if test "$enable_service" = "no"; then
   echo "  - Kerrighed service not installed (disabled)"
else
   if test "$lsb" = "yes"; then
      echo "  - Detected distribution: $DIST_ID $DIST_RELEASE"
      if test "$distro" = "yes"; then
	 echo "  - Kerrighed service will be installed"
      else
      	 echo "  - Kerrighed service won't be installed (distribution not supported)."
	 echo "    You can send a request at $PACKAGE_BUGREPORT."
      fi
   else 
      echo "  - Kerrighed service not installed (missing lsb tools)"
      echo "    Please install lsb_release"
   fi
fi
echo ""
echo "********************************************************************"
