AM_CXX=clang++
AM_CC=clang
AM_AR=gcc-ar
AM_RANLIB=gcc-ranlib
AC_SUBST([AM_RANLIB])


AC_INIT([dgos], [0.8])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_FILES([Makefile])
: ${CFLAGS="-g -O0"}
: ${CXXFLAGS="-g -O0"}
AM_INIT_AUTOMAKE([subdir-objects 1.11 -Wall])
AC_PROG_CXX
AM_PROG_AS
AM_PROG_AR
AC_PROG_RANLIB

AC_CHECK_PROGS([MKISOFS], [mkisofs genisoimage], [NOT_FOUND])
if test x"$MKISOFS" == x"NOT_FOUND"
then
	AC_MSG_ERROR([Please install mkisofs (or genisoimage)])
fi

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

AX_CHECK_COMPILE_FLAG([-Wall],
			  [COMPILER_FLAGS+=" -Wall"], [])
AX_CHECK_COMPILE_FLAG([-Wextra],
			  [COMPILER_FLAGS+=" -Wextra"], [])
AX_CHECK_COMPILE_FLAG([-Wpedantic],
			  [COMPILER_FLAGS+=" -Wpedantic"], [])
AX_CHECK_COMPILE_FLAG([-Wdouble-promotion],
			  [COMPILER_FLAGS+=" -Wdouble-promotion"], [])
AX_CHECK_COMPILE_FLAG([-Wformat],
			  [COMPILER_FLAGS+=" -Wformat"], [])
AX_CHECK_COMPILE_FLAG([-Wfloat-equal],
			  [COMPILER_FLAGS+=" -Wfloat-equal"], [])
AX_CHECK_COMPILE_FLAG([-Wundef],
			  [COMPILER_FLAGS+=" -Wundef"], [])
AX_CHECK_COMPILE_FLAG([-Wwrite-strings],
			  [COMPILER_FLAGS+=" -Wwrite-strings"], [])
AX_CHECK_COMPILE_FLAG([-Wredundant-decls],
			  [COMPILER_FLAGS+=" -Wredundant-decls"], [])
AX_CHECK_COMPILE_FLAG([-Wshadow],
			  [COMPILER_FLAGS+=" -Wshadow"], [])
AX_CHECK_COMPILE_FLAG([-Wno-strict-overflow],
			  [COMPILER_FLAGS+=" -Wno-strict-overflow"], [])
AX_CHECK_COMPILE_FLAG([-Wno-overlength-strings],
			  [COMPILER_FLAGS+=" -Wno-overlength-strings"], [])

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
AX_CHECK_COMPILE_FLAG([-Wno-ignored-optimization-argument],
			  [SMALLEST_CODE_FLAGS+=" -Wno-ignored-optimization-argument"])

AC_SUBST([SMALLEST_CODE_FLAGS])

AX_CHECK_COMPILE_FLAG([-m16],
			  [REAL_MODE_FLAGS+=" -m16"])
AX_CHECK_COMPILE_FLAG([-mno-sse],
			  [REAL_MODE_FLAGS+=" -mno-sse"])
AX_CHECK_COMPILE_FLAG([-mno-sse2],
			  [REAL_MODE_FLAGS+=" -mno-sse2"])
AX_CHECK_COMPILE_FLAG([-mno-avx],
			  [REAL_MODE_FLAGS+=" -mno-avx"])
AX_CHECK_COMPILE_FLAG([-mno-avx2],
			  [REAL_MODE_FLAGS+=" -mno-avx2"])
AX_CHECK_COMPILE_FLAG([-mfpmath=387],
			  [REAL_MODE_FLAGS+=" -mfpmath=387"])

AC_SUBST([REAL_MODE_FLAGS])

AX_CHECK_COMPILE_FLAG([-mrtd],
			  [REGPARM_FLAGS+=" -mrtd"])
AX_CHECK_COMPILE_FLAG([-mregparm=3],
			  [REGPARM_FLAGS+=" -mregparm=3"])

AC_SUBST([REGPARM_FLAGS])

AX_CHECK_COMPILE_FLAG([-fno-pie],
			  [NO_PIC_FLAGS+=" -fno-pie"])
AX_CHECK_COMPILE_FLAG([-fno-pic],
			  [NO_PIC_FLAGS+=" -fno-pic"])

AC_SUBST([NO_PIC_FLAGS])

AC_OUTPUT
