AM_CXX=clang++
AM_CC=clang
AM_AR=gcc-ar
AM_RANLIB=gcc-ranlib
AC_SUBST([AM_RANLIB])

AC_INIT([dgos], [0.8])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_FILES([Makefile])
: ${CFLAGS=""}
: ${CXXFLAGS=""}
AM_INIT_AUTOMAKE([subdir-objects 1.11 -Wall])
AX_PROG_CC_FOR_BUILD
AX_PROG_CXX_FOR_BUILD
AC_PROG_CXX
AM_PROG_AS
AM_PROG_AR
AC_PROG_RANLIB


AC_CHECK_PROGS([MKISOFS], [mkisofs genisoimage], [NOT_FOUND])
if test x"$MKISOFS" == x"NOT_FOUND"
then
	AC_MSG_WARN([Install mkisofs (or genisoimage) to build CD images])
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

AX_CHECK_COMPILE_FLAG([-Wall -Wextra -Wpedantic -Werror],
	[COMPILER_WERROR="-Wall -Wextra -Wpedantic -Werror"],
	[COMPILER_WERROR=""])

AX_CHECK_COMPILE_FLAG([-ggdb],
	[COMPILER_FLAGS+=" -ggdb"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-fno-integrated-as],
	[COMPILER_FLAGS+=" -fno-integrated-as"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wno-unused-command-line-argument],
	[COMPILER_FLAGS+=" -Wno-unused-command-line-argument"], 
	[], [$COMPILER_WERROR])

AX_CHECK_COMPILE_FLAG([-Wa,-g],
	[COMPILER_FLAGS+=" -Wa,-g"], 
	[], [$COMPILER_WERROR])

AX_CHECK_COMPILE_FLAG([-Wall],
	[COMPILER_FLAGS+=" -Wall"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wextra],
	[COMPILER_FLAGS+=" -Wextra"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wpedantic],
	[COMPILER_FLAGS+=" -Wpedantic"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wdouble-promotion],
	[COMPILER_FLAGS+=" -Wdouble-promotion"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wformat],
	[COMPILER_FLAGS+=" -Wformat"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wfloat-equal],
	[COMPILER_FLAGS+=" -Wfloat-equal"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wundef],
	[COMPILER_FLAGS+=" -Wundef"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wwrite-strings],
	[COMPILER_FLAGS+=" -Wwrite-strings"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wredundant-decls],
	[COMPILER_FLAGS+=" -Wredundant-decls"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wno-shadow],
	[COMPILER_FLAGS+=" -Wno-shadow"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wno-strict-overflow],
	[COMPILER_FLAGS+=" -Wno-strict-overflow"], 
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wno-overlength-strings],
	[COMPILER_FLAGS+=" -Wno-overlength-strings"], 
	[], [$COMPILER_WERROR])

AC_SUBST([COMPILER_FLAGS])

AX_CHECK_COMPILE_FLAG([-Oz],
	[SMALLEST_CODE_FLAGS=-Oz],
	[SMALLEST_CODE_FLAGS=-Os],
	[$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-falign-loops=0],
	[SMALLEST_CODE_FLAGS+=" -falign-loops=0"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-falign-functions=0],
	[SMALLEST_CODE_FLAGS+=" -falign-functions=0"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-falign-labels=0],
	[SMALLEST_CODE_FLAGS+=" -falign-labels=0"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-falign-jumps=0],
	[SMALLEST_CODE_FLAGS+=" -falign-jumps=0"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-mno-align-stringops],
	[SMALLEST_CODE_FLAGS+=" -mno-align-stringops"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-Wno-ignored-optimization-argument],
	[SMALLEST_CODE_FLAGS+=" -Wno-ignored-optimization-argument"]
	[], [$COMPILER_WERROR])

AC_SUBST([SMALLEST_CODE_FLAGS])

AX_CHECK_COMPILE_FLAG([-m32],
	[REAL_MODE_FLAGS+=" -m32"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-mno-sse],
	[REAL_MODE_FLAGS+=" -mno-sse"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-mno-sse2],
	[REAL_MODE_FLAGS+=" -mno-sse2"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-mno-avx],
	[REAL_MODE_FLAGS+=" -mno-avx"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-mno-avx2],
	[REAL_MODE_FLAGS+=" -mno-avx2"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-mfpmath=387],
	[REAL_MODE_FLAGS+=" -mfpmath=387"]
	[], [$COMPILER_WERROR])

AC_SUBST([REAL_MODE_FLAGS])

AX_CHECK_COMPILE_FLAG([-mrtd],
	[REGPARM_FLAGS+=" -mrtd"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-mregparm=3],
	[REGPARM_FLAGS+=" -mregparm=3"]
	[], [$COMPILER_WERROR])

AC_SUBST([REGPARM_FLAGS])

AX_CHECK_COMPILE_FLAG([-fno-pie],
	[NO_PIC_FLAGS+=" -fno-pie"]
	[], [$COMPILER_WERROR])
AX_CHECK_COMPILE_FLAG([-fno-pic],
	[NO_PIC_FLAGS+=" -fno-pic"]
	[], [$COMPILER_WERROR])

AC_SUBST([NO_PIC_FLAGS])

DISABLE_FRAMEPOINTER_FLAGS=
AX_CHECK_COMPILE_FLAG([-fno-omit-frame-pointer],
	[DISABLE_FRAMEPOINTER_FLAGS=" -fomit-frame-pointer"]
	[], [$COMPILER_WERROR])

AC_SUBST([DISABLE_FRAMEPOINTER_FLAGS])

ENABLE_FRAMEPOINTER_FLAGS=
AX_CHECK_COMPILE_FLAG([-fomit-frame-pointer],
	[ENABLE_FRAMEPOINTER_FLAGS=" -fno-omit-frame-pointer"]
	[], [$COMPILER_WERROR])
	
AX_CHECK_COMPILE_FLAG([-mno-red-zone],
	[NO_REDZONE_FLAGS=" -mno-red-zone"]
	[], [$COMPILER_WERROR])

AC_SUBST([NO_REDZONE_FLAGS])

AX_CHECK_COMPILE_FLAG([-fno-stack-protector],
	[NO_STACKPROTECTOR_FLAGS=" -fno-stack-protector"]
	[], [$COMPILER_WERROR])

AC_SUBST([NO_STACKPROTECTOR_FLAGS])

AX_CHECK_COMPILE_FLAG([-fno-asynchronous-unwind-tables],
	[NO_UNWIND_TABLES_FLAGS=" -fno-asynchronous-unwind-tables"]
	[], [$COMPILER_WERROR])

AC_SUBST([NO_UNWIND_TABLES_FLAGS])

AX_CHECK_COMPILE_FLAG([-fno-exceptions],
	[NO_EXCEPTIONS_FLAGS=" -fno-exceptions"]
	[], [$COMPILER_WERROR])

AC_SUBST([NO_EXCEPTIONS_FLAGS])

AX_CHECK_COMPILE_FLAG([-fno-common],
	[NO_COMMON_FLAGS=" -fno-common"]
	[], [$COMPILER_WERROR])

AC_SUBST([NO_COMMON_FLAGS])

AC_ARG_ENABLE(optimize,
	AS_HELP_STRING([--enable-optimize], [Enable size/speed optimizations]),
	[
		echo Enabling optimizations
		OPTIMIZE_SIZE_FLAGS="$SMALLEST_CODE_FLAGS $DISABLE_FRAMEPOINTER_FLAGS"
		OPTIMIZE_SPEED_FLAGS="-O2 $DISABLE_FRAMEPOINTER_FLAGS"
	],[
		echo Disabling optimizations
		OPTIMIZE_SIZE_FLAGS="-O0 $ENABLE_FRAMEPOINTER_FLAGS"
		OPTIMIZE_SPEED_FLAGS="-O0 $ENABLE_FRAMEPOINTER_FLAGS"
	])

AC_SUBST([DISABLE_OPTIMIZATION_FLAGS], ["-O0 $ENABLE_FRAMEPOINTER_FLAGS"])

AC_SUBST([OPTIMIZE_SIZE_FLAGS])
AC_SUBST([OPTIMIZE_SPEED_FLAGS])

AC_OUTPUT
