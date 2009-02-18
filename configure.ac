dnl configure.in for Kerrighed project.
dnl Copyright 2006 IRISA, All rights reserved
dnl Copyright 2007-2009 Kerlabs, All rights reserved
dnl
dnl AUTHORS:
dnl  Jean Parpaillon <jean.parpaillon@kerlabs.com>,
dnl  Louis Rilling <louis.rilling@kerlabs.com>
dnl
dnl Process this file with autoconf to produce a configure script.

###
### This configure script is the top level one for Kerrighed project.
### It propagates arguments to sub-configure.


dnl autoconf version
AC_PREREQ(2.59)

dnl mandatory stuff
AC_INIT([kerrighed-tools], [2.4.0], [kerrighed.users@irisa.fr])

dnl Linux versions compatible with Kerrighed
COMP_LINUX_VERSION=2.6.20
AC_SUBST([COMP_LINUX_VERSION])

dnl check host and target
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

dnl check tools
srcdir=`cd $srcdir && pwd`
builddir=`pwd`

AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_PROG_INSTALL

AC_CHECK_PROG(BZIP2, bzip2, bzip2, )
if test -z "$BZIP2"; then
   AC_MSG_ERROR([
   	*** You need bzip2 tool])
fi
AC_CHECK_PROG(PKGCONFIG, pkg-config, pkg-config, )
if test -z "$PKGCONFIG"; then
   AC_MSG_ERROR([
        *** You need pkg-config tool])
fi
AC_CHECK_PROG(RSYNC, rsync, rsync, )
if test -z "$RSYNC"; then
   AC_MSG_ERROR([
        *** You need rsync tool])
fi
AC_CHECK_PROG(LNDIR, lndir, lndir, )
if test -z "$LNDIR" -a "$srcdir" != "$builddir"; then
   AC_MSG_ERROR([
        *** You need lndir tool to compile out of source tree])
fi
AC_CHECK_PROG(LSDIFF, lsdiff, lsdiff, )
if test -z "$LSDIFF" -a "$srcdir" != "$builddir"; then
   AC_MSG_ERROR([
        *** You need lsdiff tool to compile out of source tree])
fi

###
### BEGIN LINUX SOURCE PATH CHECK
###
AC_ARG_ENABLE([linux-check],
	      [AS_HELP_STRING([--disable-linux-check],
	      		      [Check for the Linux sources to be compatible with Kerrighed (!!! disable at your own risk !!!) @<:@default=enable@:>@])],
	      [],
	      [enable_linux_check=yes])
AC_SUBST([enable_linux_check])

AC_ARG_WITH([kernel],
	    [AS_HELP_STRING([--with-kernel=PATH],
	      		    [Path to kernel source !!!MANDATORY])],
            [kernelsrc="$withval"],
	    [])
if test -z "$kernelsrc"; then
   AC_MSG_ERROR([
   *** Please specify the location of the kernel source tree with
   *** the '--with-kernel=SRCDIR' option])
fi
kernelsrc=$(cd $kernelsrc; pwd)
AC_SUBST([kernelsrc])

if test "$srcdir" = "$builddir"; then
   kernelbuild="$kernelsrc"
else
   kernelbuild="$builddir/kernel-build"
fi
AC_SUBST([kernelbuild])

dnl check paths are valid
AC_MSG_CHECKING([kernel source directory])
if test -z "$kernelsrc" -o -z "$kernelbuild"; then
   AC_MSG_RESULT([Not found])
   AC_MSG_ERROR([
   	*** Please specify the location of the kernel source with
	*** the '--with-kernel=SRCDIR' option])
else
  AC_MSG_RESULT([$kernelsrc])
  AC_MSG_CHECKING([kernel build directory])
  AC_MSG_RESULT([$kernelbuild])
fi

dnl check Linux version
AC_MSG_CHECKING([kernel source version])
linux_makefile="$kernelsrc/Makefile"
if test -d "$kernelsrc/include/linux"; then

   # Get Linux version
   LINUX_VERSION=`"$srcdir/linux_version.sh" $linux_makefile V.P.S`
   LINUX_EXTRAVERSION=`"$srcdir/linux_version.sh" $linux_makefile E`

   if test -d "$kernelsrc/include/kerrighed"; then
      # Sources have already been patched
      kernel_patched="yes"
      patched=" (already patched)"
   else
      kernel_patched="no"
      # check version if not disabled
      good_version=true
      if test "x$enable_linux_check" = "xyes"; then
	 if test "$COMP_LINUX_VERSION" != "$LINUX_VERSION$LINUX_EXTRAVERSION"; then	
	    good_version=false
	 fi
      fi
   fi
   AC_MSG_RESULT([$LINUX_VERSION$LINUX_EXTRAVERSION$patched])
   if test "$good_version" = "false"; then
      AC_MSG_ERROR([
*** 
*** This Linux version is not compatible with Kerrighed
*** Compatible versions: $COMP_LINUX_VERSIONS
*** 
*** You can override this with --disable-linux-check (!!! do it at your own risk !!!)
***])
   fi
else
   if test "$enable_linux_check" = "yes"; then
      AC_MSG_ERROR([*** $kernelsrc does not contain kernel sources])
   fi
fi
###
### END LINUX SOURCE PATH CHECK
###

###
### BEGIN LINUX CONFIG OPTION
###
AC_ARG_WITH([kernel-config],
	    [AS_HELP_STRING([--with-kernel-config=PATH],
			    [Path to a kernel .config file. Sample files are provided in 'samples' directory. Implies --disable-kernel-defconfig @<:@default=none@:>@])],
	    [kernelconfig="$withval"])
if test -n "$kernelconfig"; then
  kernelconfig=`(cd \`dirname $kernelconfig\`; pwd)`/`basename $kernelconfig`
fi
AC_SUBST([kernelconfig])
###
### END LINUX CONFIG
###

###
### BEGIN LIBRARIES CHECK
###
AC_ARG_ENABLE([libkerrighed],
	      [AS_HELP_STRING([--disable-libkerrighed],
			      [Disable libkerrighed (disable also tools compilation) @<:@default=enable@:>@])],
	      [],
	      [enable_libkerrighed=yes])
###
### END LIBRARIES CHECK

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
			      [Do not install kerrighed service @<:@default: enable@:>@])])
###
### END TOOLS CHECK
###


###
### BEGIN TESTS CHECK
###
AC_ARG_WITH([ltp-base],
	    [AS_HELP_STRING([--with-ltp-base=PATH],
			    [Path to ltp base dir @<:@default=/usr/lib/debian-test/tests/linux@:>@])],
	    [ltpbase="$withval"],
	    [ltpbase="/usr/lib/debian-test/tests/linux"])
AC_SUBST([ltpbase])

AC_ARG_ENABLE([tests],
 	      [AS_HELP_STRING([--enable-tests],
 			      [Enable tests @<:@default=disable@:>@])],
 	      [],
 	      [enable_tests=no])
###
### END TESTS CHECK
###

###
### BEGIN CHECK FOR DOCBOOK TOOLS
###
dnl Just pass it to tools' configure and display this option with --help
AC_ARG_ENABLE([man],
	      [AS_HELP_STRING([--disable-man],
			      [Disable manpages @<:@default=enable@:>@])],
	      [],
	      [enable_man=yes])
if test "x$enable_man" = "xyes"; then
   AC_CHECK_PROG(DB2MAN, xmlto, xmlto, )
fi
###
### END CHECK FOR DOCBOOK TOOLS
###

dnl files to generate with automake
AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([patches/Makefile])

dnl write all stuff
AC_OUTPUT

if test "$kernelsrc" != "$kernelbuild"; then
   AC_MSG_NOTICE([Creates Linux Makefile in output dir])
   mkdir -p "$kernelbuild"
   ( cd "$kernelbuild" && \
     make -C "$kernelsrc" outputmakefile O="$kernelbuild" 2> /dev/null > /dev/null || true )
fi

###
### Display results
###
echo "********************************************************************"
echo "  Kerrighed configuration is now complete"
echo "********************************************************************"
echo ""

echo "    - Sources dir            : $srcdir"
if test "$srcdir" != "$builddir"; then
   echo "    - Build dir              : $builddir"
fi
echo "    - Kernel source dir      : $kernelsrc"
if test "$kernelsrc" != "$kernelbuild"; then
   echo "    - Kernel build dir       : $kernelbuild"
fi
echo "    - Kernel version         : $LINUX_VERSION$LINUX_EXTRAVERSION$patched"
echo -n "    - Kernel configuration   : "
if test -n "$kernelconfig"; then
  echo "$kernelconfig"
else
  echo "none. Run 'make *config' in kernel source tree"
fi

echo "    - libkerrighed           : $enable_libkerrighed"
echo "    - Kerrighed tools        : $enable_tools"
echo "    - Kerrighed tests        : $enable_tests"
echo "    - Kerrighed manpages     : $enable_man"

echo ""
echo "********************************************************************"
echo ""
echo " To build the system:"

if test "x$kernel_patched" = "xno"; then
   echo " - run 'make patch' to patch the kernel sources"
fi

echo " - run 'make defconfig' to create a default configuration for the kernel"
echo " - configure the kernel for your needs by running one of 'make *config' in the kernel source tree"
echo " - run 'make kernel' to build your kernel"
echo ""
echo " - run 'make' to build tools, libs, tests and so on"
echo ""
echo " - as root, run 'make kernel-install'"
echo " - as root, run 'make install'"
echo ""
echo "********************************************************************"
