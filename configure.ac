AM_CXX=clang++
AM_CC=clang
AM_AR=gcc-ar
AM_RANLIB=gcc-ranlib
AC_SUBST([AM_CC])
AC_SUBST([AM_CXX])
AC_SUBST([AM_AR])
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

COMPILER_WERROR=
for flag in Wall Wextra Wpedantic Werror; do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[COMPILER_WERROR+=" -$flag"])
done

COMPILER_FLAGS=
for flag in \
		ggdb fno-integrated-as Wno-unused-command-line-argument \
		Wa,-g Wall Wextra Wpedantic Wdouble-promotion Wformat \
		Wfloat-equal Wundef Wwrite-strings Wredundant-decls \
		Wno-shadow Wno-strict-overflow Wno-overlength-strings
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[COMPILER_FLAGS+=" -$flag"],
		[], [$COMPILER_WERROR])
done
AC_SUBST([COMPILER_FLAGS])

SMALLEST_CODE_FLAGS=
AX_CHECK_COMPILE_FLAG([-Oz],
	[SMALLEST_CODE_FLAGS=-Oz],
	[AX_CHECK_COMPILE_FLAG([-Os],
		[SMALLEST_CODE_FLAGS=-Os],
		[$COMPILER_WERROR])],
	[$COMPILER_WERROR])
for flag in falign-loops=0 falign-functions=0 falign-labels=0 falign-jumps=0 \
		mno-align-stringops Wno-ignored-optimization-argument
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[SMALLEST_CODE_FLAGS+=" -$flag"]
		[], [$COMPILER_WERROR])
done
AC_SUBST([SMALLEST_CODE_FLAGS])

REAL_MODE_FLAGS=
for flag in m32 mno-sse mno-sse2 mno-avx mno-avx2 mfpmath=387
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[REAL_MODE_FLAGS+=" -$flag"]
		[], [$COMPILER_WERROR])
done
AC_SUBST([REAL_MODE_FLAGS])

REGPARM_FLAGS=
for flag in mrtd mregparm=3
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[REGPARM_FLAGS+=" -$flag"]
		[], [$COMPILER_WERROR])
done
AC_SUBST([REGPARM_FLAGS])

NO_PIC_FLAGS=
for flag in fno-pie fno-pic
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[NO_PIC_FLAGS+=" -$flag"]
		[], [$COMPILER_WERROR])
done
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
AC_SUBST([ENABLE_FRAMEPOINTER_FLAGS])

NO_REDZONE_FLAGS=
AX_CHECK_COMPILE_FLAG([-mno-red-zone],
	[NO_REDZONE_FLAGS=" -mno-red-zone"]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_REDZONE_FLAGS])

NO_STACKPROTECTOR_FLAGS=
AX_CHECK_COMPILE_FLAG([-fno-stack-protector],
	[NO_STACKPROTECTOR_FLAGS=" -fno-stack-protector"]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_STACKPROTECTOR_FLAGS])

STACK_CHECK_FLAGS=
AX_CHECK_COMPILE_FLAG([-fstack-check],
	[STACK_CHECK_FLAGS=" -fstack-check"]
	[], [$COMPILER_WERROR])
AC_SUBST([STACK_CHECK_FLAGS])

NO_STACK_CHECK_FLAGS=
AX_CHECK_COMPILE_FLAG([-fno-stack-check],
	[NO_STACK_CHECK_FLAGS=" -fno-stack-check"]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_STACK_CHECK_FLAGS])

NO_RTTI_FLAGS=
AX_CHECK_COMPILE_FLAG([-fno-rtti],
	[NO_RTTI_FLAGS=" -fno-rtti"]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_RTTI_FLAGS])

NO_UNWIND_TABLES_FLAGS=
AX_CHECK_COMPILE_FLAG([-fno-asynchronous-unwind-tables],
	[NO_UNWIND_TABLES_FLAGS=" -fno-asynchronous-unwind-tables"]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_UNWIND_TABLES_FLAGS])

NO_EXCEPTIONS_FLAGS=
AX_CHECK_COMPILE_FLAG([-fno-exceptions],
	[NO_EXCEPTIONS_FLAGS=" -fno-exceptions"]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_EXCEPTIONS_FLAGS])

NO_COMMON_FLAGS=
AX_CHECK_COMPILE_FLAG([-fno-common],
	[NO_COMMON_FLAGS=" -fno-common"]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_COMMON_FLAGS])

VECTORIZE_FLAGS=
AX_CHECK_COMPILE_FLAG([-ftree-vectorize],
	[VECTORIZE_FLAGS=" -ftree-vectorize"]
	[], [$COMPILER_WERROR])
AC_SUBST([VECTORIZE_FLAGS])

TUNE_FLAGS=
AX_CHECK_COMPILE_FLAG([-mtune=znver1],
	[TUNE_FLAGS=" -mtune=znver1"]
	[], [$COMPILER_WERROR])
AC_SUBST([TUNE_FLAGS])

AC_ARG_ENABLE(optimize,
	AS_HELP_STRING([--enable-optimize], [Enable size/speed optimizations]),
	[
		echo Enabling optimizations
		OPTIMIZE_SIZE_FLAGS="$SMALLEST_CODE_FLAGS $DISABLE_FRAMEPOINTER_FLAGS"
		OPTIMIZE_SPEED_FLAGS="-O2 $VECTORIZE_FLAGS $DISABLE_FRAMEPOINTER_FLAGS"
		OPTIMIZE_SPEED_FLAGS+=" $TUNE_FLAGS"
	],[
		echo Disabling optimizations
		OPTIMIZE_SIZE_FLAGS="-O0 $ENABLE_FRAMEPOINTER_FLAGS"
		OPTIMIZE_SPEED_FLAGS="-O0 $ENABLE_FRAMEPOINTER_FLAGS"
	])

AC_SUBST([DISABLE_OPTIMIZATION_FLAGS], ["-O0 $ENABLE_FRAMEPOINTER_FLAGS"])

AC_SUBST([OPTIMIZE_SIZE_FLAGS])
AC_SUBST([OPTIMIZE_SPEED_FLAGS])

AC_OUTPUT
