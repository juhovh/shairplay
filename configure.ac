#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([shairplay], [0.9.0], [juhovh@iki.fi])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/shairplay.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
AC_PROG_CC
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL

# Checks for libraries.
LT_LIB_M
LT_LIB_DLLOAD

# Checks for header files.
AC_HEADER_STDC
if test no = "$libltdl_cv_func_dlopen" && test no = "$libltdl_cv_lib_dl_dlopen"
then
  AC_CHECK_HEADERS([dns_sd.h], [],
                   [AC_MSG_ERROR([Could not find dns_sd.h header, please install libavahi-compat-libdnssd-dev or equivalent.])])
fi

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_LIB([socket],[connect])
AC_CHECK_LIB([pthread],[pthread_create])

# Custom check for os, similar to webkit
AC_MSG_CHECKING([for native Win32])
case "$host" in
     *-*-mingw*)
       os_win32=yes
       ;;
     *)
       os_win32=no
       ;;
esac
AC_MSG_RESULT([$os_win32])

case "$host" in
     *-*-linux*)
       os_linux=yes
       ;;
     *-*-freebsd*)
       os_freebsd=yes
       ;;
     *-*-darwin*)
       os_darwin=yes
       ;;
esac

# OS conditionals
AM_CONDITIONAL([OS_WIN32], [test x"$os_win32" = x"yes"])
AM_CONDITIONAL([OS_UNIX], [test x"$os_win32" = x"no"])
AM_CONDITIONAL([OS_LINUX], [test x"$os_linux" = x"yes"])
AM_CONDITIONAL([OS_DARWIN], [test x"$os_darwin" = x"yes"])
AM_CONDITIONAL([OS_FREEBSD], [test x"$os_freebsd" = x"yes"])

# Custom check for libao
PKG_CHECK_MODULES([libao], [ao >= 1.1.0], [have_libao=1], [have_libao=0])
AM_CONDITIONAL([HAVE_LIBAO], [test "$have_libao" -eq 1])
AM_COND_IF(HAVE_LIBAO,
  [AC_MSG_NOTICE([libao audio library found: Building both shairplay binary and libshairplay library.])],
  [AC_MSG_WARN([libao audio library not found: Building shairplay library only.])])

# Optional playfair support
AC_ARG_WITH([playfair],
	AS_HELP_STRING([--with-playfair], [Build with playfair support]))
AM_CONDITIONAL([WITH_PLAYFAIR], [test x"$with_playfair" = x"yes"])
AM_COND_IF([WITH_PLAYFAIR],
	[AC_DEFINE([HAVE_FAIRPLAY], [1],
	           [Define if you have the libdl library or equivalent.])])


AC_CONFIG_FILES(
	[Makefile]
	[include/Makefile]
	[src/Makefile]
	[src/lib/Makefile]
	[src/lib/alac/Makefile]
	[src/lib/crypto/Makefile]
	[src/lib/curve25519/Makefile]
	[src/lib/ed25519/Makefile]
	[src/lib/playfair/Makefile]
)
AC_OUTPUT
