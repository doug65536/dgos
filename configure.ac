AC_INIT([dgos], [0.8])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
#ACLOCAL_AMFLAGS([-I m4])
AC_CONFIG_FILES([Makefile])
AM_INIT_AUTOMAKE([subdir-objects 1.11 -Wall])
AC_PROG_CXX
AM_PROG_AS
AM_PROG_AR
AC_PROG_RANLIB

dnl **********************************************************************
dnl DETECT_CLANG ([ACTION-IF-YES], [ACTION-IF-NO])
dnl
dnl check if compiler is clang, and if so run the ACTION-IF-YES sets the
dnl $CLANG variable to "yes" or "no"
dnl **********************************************************************
AC_DEFUN([DETECT_CLANG],
[
	AC_MSG_CHECKING([for clang in use])
	AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM([], [[
	#ifndef __clang__
		   not clang
	#endif
	]])],
	[CLANG=yes], [CLANG=no])
	AC_MSG_RESULT([$CLANG])
	AS_IF([test "$CLANG" = "yes"],[$1],[$2])
])
DETECT_CLANG([],[])

AC_OUTPUT

