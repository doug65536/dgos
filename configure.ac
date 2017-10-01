AM_CXX=clang++
AM_CC=clang
AM_AR=gcc-ar
AM_RANLIB=gcc-ranlib
AC_SUBST([AM_RANLIB])


AC_INIT([dgos], [0.8])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_FILES([Makefile])
AM_INIT_AUTOMAKE([subdir-objects 1.11 -Wall])
: ${CFLAGS="-g"}
AC_PROG_CXX
AM_PROG_AS
AM_PROG_AR
AC_PROG_RANLIB

AC_DEFUN([AX_CHECK_COMPILE_FLAG],
[AC_PREREQ(2.64)dnl for _AC_LANG_PREFIX and AS_VAR_IF
AS_VAR_PUSHDEF([CACHEVAR],[ax_cv_check_[]_AC_LANG_ABBREV[]flags_$4_$1])dnl
AC_CACHE_CHECK([whether _AC_LANG compiler accepts $1], CACHEVAR, [
  ax_check_save_flags=$[]_AC_LANG_PREFIX[]FLAGS
  _AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS $4 $1"
  AC_COMPILE_IFELSE([m4_default([$5],[AC_LANG_PROGRAM()])],
	[AS_VAR_SET(CACHEVAR,[yes])],
	[AS_VAR_SET(CACHEVAR,[no])])
  _AC_LANG_PREFIX[]FLAGS=$ax_check_save_flags])
AS_VAR_IF(CACHEVAR,yes,
  [m4_default([$2], :)],
  [m4_default([$3], :)])
AS_VAR_POPDEF([CACHEVAR])dnl
])dnl AX_CHECK_COMPILE_FLAGS

AX_CHECK_COMPILE_FLAG([-fno-integrated-as],
			  [COMPILER_FLAGS+=" -fno-integrated-as"], [])
AX_CHECK_COMPILE_FLAG([-Wno-unused-command-line-argument],
			  [COMPILER_FLAGS+=" -Wno-unused-command-line-argument"], [])
AC_SUBST([COMPILER_FLAGS])

AX_CHECK_COMPILE_FLAG([-Oz],
			 [SMALLEST_CODE_FLAGS=-Oz],
			  [SMALLEST_CODE_FLAGS=-Os])
AX_CHECK_COMPILE_FLAG([-falign-loops=0],
			  [SMALLEST_CODE_FLAGS+=" -falign-loops=0"])
AX_CHECK_COMPILE_FLAG([-falign-functions=0],
			  [SMALLEST_CODE_FLAGS+=" -falign-functions=0"])
AX_CHECK_COMPILE_FLAG([-falign-labels=0],
			  [SMALLEST_CODE_FLAGS+=" -falign-labels=0"])
AX_CHECK_COMPILE_FLAG([-falign-jumps=0],
			  [SMALLEST_CODE_FLAGS+=" -falign-jumps=0"])
AX_CHECK_COMPILE_FLAG([-mno-align-stringops],
			  [SMALLEST_CODE_FLAGS+=" -mno-align-stringops"])

AC_SUBST([SMALLEST_CODE_FLAGS])

AC_OUTPUT
