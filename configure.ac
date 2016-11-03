AC_INIT([xininfo], [0.15.5], [qball@gmpclient.org])

AC_CONFIG_SRCDIR([source/xininfo.c])
AC_CONFIG_HEADER([config.h])

##
# Setup automake to be silent and in foreign mode.
##
AM_INIT_AUTOMAKE([-Wall -Werror foreign subdir-objects dist-xz])
AM_SILENT_RULES([yes])

##
# Check for compiler
##
AC_PROG_CC([clang gcc cc])
AC_PROG_CC_C99
AM_PROG_CC_C_O

AC_USE_SYSTEM_EXTENSIONS

##
# Check dependencies
##
PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([xcb], [ xcb-aux xcb-randr xcb-xinerama xcb-dpms xcb-ewmh xcb-screensaver ])

AC_SUBST([EXTRA_CFLAGS], ["-Wall -Wextra -Wparentheses -Winline -pedantic"])

AC_CONFIG_FILES([Makefile ])
AC_OUTPUT
