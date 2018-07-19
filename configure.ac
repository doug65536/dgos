: ${CFLAGS=""}
: ${CXXFLAGS=""}

AC_INIT([dgos], [0.8])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIRS([m4])
AC_CONFIG_FILES([Makefile])
AM_INIT_AUTOMAKE([subdir-objects 1.11 -Wall -Wno-portability])
#AX_PROG_CC_FOR_BUILD
AX_PROG_CXX_FOR_BUILD
AC_PROG_CXX
AM_PROG_AS
#AM_PROG_AR
#AC_PROG_RANLIB
AC_LANG([C++])

AC_CHECK_TOOL([OBJDUMP], [objdump], [NOT_FOUND])
AS_IF([test x"$OBJDUMP" == x"NOT_FOUND"],
    [AC_MSG_FAILURE([Cannot find objdump])])
AC_SUBST([OBJDUMP])

AC_CHECK_TOOL([OBJCOPY], [objcopy], [NOT_FOUND])
AS_IF([test x"$OBJCOPY" == x"NOT_FOUND"],
	[AC_MSG_FAILURE([Cannot find objcopy])])
AC_SUBST([OBJCOPY])

AC_CHECK_TOOL([AR], [ar], [NOT_FOUND])
AS_IF([test x"$AR" == x"NOT_FOUND"],
    [AC_MSG_FAILURE([Cannot find ar])])
AC_SUBST([AR])

AC_CHECK_TOOL([RANLIB], [ranlib], [NOT_FOUND])
AS_IF([test x"$RANLIB" == x"NOT_FOUND"],
    [AC_MSG_FAILURE([Cannot find ranlib])])
AC_SUBST([RANLIB])

AC_CHECK_PROGS([QEMU], [qemu-system-x86_64], [NOT_FOUND])
AS_IF([test x"$QEMU" == x"NOT_FOUND"],
	[AC_MSG_WARN([Cannot find qemu-system-x86_64])])
AC_SUBST([QEMU])

AC_CHECK_PROGS([CP], [cp], [NOT_FOUND])
AS_IF([test x"$CP" == x"NOT_FOUND"],
	[AC_MSG_WARN([Cannot find cp])])
AC_SUBST([CP])

AC_CHECK_PROGS([LN], [ln], [NOT_FOUND])
AS_IF([test x"$LN" == x"NOT_FOUND"],
	[AC_MSG_WARN([Cannot find ln])])
AC_SUBST([LN])

AC_CHECK_PROGS([SED], [sed], [NOT_FOUND])
AS_IF([test x"$SED" == x"NOT_FOUND"],
	[AC_MSG_WARN([Cannot find sed])])
AC_SUBST([SED])

AC_CHECK_PROGS([MKISOFS], [mkisofs genisoimage], [NOT_FOUND])
AS_IF([test x"$MKISOFS" == x"NOT_FOUND"],
	[AC_MSG_WARN([Install mkisofs (or genisoimage) to build CD images])])
AC_SUBST([MKISOFS])

AC_CHECK_PROGS([GREP], [grep], [NOT_FOUND])
AS_IF([test x"$GREP" == x"NOT_FOUND"],
	[AC_MSG_FAILURE([Cannot find grep])])
AC_SUBST([GREP])

AC_CHECK_PROGS([SORT], [sort], [NOT_FOUND])
AS_IF([test x"$SORT" == x"NOT_FOUND"],
	[AC_MSG_FAILURE([Cannot find sort])])
AC_SUBST([SORT])

AC_CHECK_PROGS([GZIP], [gzip], [NOT_FOUND])
AS_IF([test x"$GZIP" == x"NOT_FOUND"],
	[AC_MSG_FAILURE([Cannot find gzip])])
AC_SUBST([GZIP])

AC_CHECK_PROGS([TRUNCATE], [truncate], [NOT_FOUND])
AS_IF([test x"$TRUNCATE" == x"NOT_FOUND"],
	[AC_MSG_FAILURE([Cannot find truncate])])
AC_SUBST([TRUNCATE])

AC_CHECK_PROGS([SHRED], [shred], [NOT_FOUND])
AS_IF([test x"$SHRED" == x"NOT_FOUND"],
	[AC_MSG_FAILURE([Cannot find shred])])
AC_SUBST([SHRED])

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

AS_VAR_SET([COMPILER_WERROR],[])
for flag in Wall Wextra Wpedantic Werror; do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([COMPILER_WERROR], [" -$flag"])])
done

AS_VAR_SET([INSTRUMENT_FUNCTIONS_FLAGS], [])
for flag in \
	finstrument-functions D_CALL_TRACE_ENABLED
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([INSTRUMENT_FUNCTIONS_FLAGS], [" -$flag"])],
		[], [$COMPILER_WERROR])
done
AC_SUBST([INSTRUMENT_FUNCTIONS_FLAGS])

AS_VAR_SET([ASAN_FLAGS], [])
AC_ARG_ENABLE(asan,
	AS_HELP_STRING([--enable-asan], [Enable Address Space SANitizer]), [
		for flag in \
			fsanitize=kernel-address D_ASAN_ENABLED
		do
			AX_CHECK_COMPILE_FLAG([-$flag],
				[AS_VAR_APPEND([ASAN_FLAGS], [" -$flag"])],
				[], [$COMPILER_WERROR])
		done
	]
)
AC_SUBST([ASAN_FLAGS])

AS_VAR_SET([COMPILER_FLAGS],[])
for flag in \
		std=c++14 fno-fat-lto-objects gdwarf-4 \
		fno-integrated-as Wno-unused-command-line-argument \
		Wall Wextra Wpedantic Wdouble-promotion Wformat \
		Wfloat-equal Wundef Wwrite-strings Wredundant-decls \
		Wno-shadow Wno-strict-overflow Wno-overlength-strings \
		Wno-unused-parameter Wtautological-compare \
		Wstrict-aliasing Wlogical-op \
		Werror=return-type Werror=array-bounds \
		Wmultistatement-macros Warray-bounds Wformat-overflow \
		Wsuggest-final-types Wsuggest-final-methods Wsuggest-override \
		Wsuggest-attribute=format \
		Wduplicated-branches Wduplicated-cond Wc++1z-compat Wuseless-cast \
		Wzero-as-null-pointer-constant Wsuggest-attribute=noreturn
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([COMPILER_FLAGS], [" -$flag"])],
		[], [$COMPILER_WERROR])
done
AC_SUBST([COMPILER_FLAGS])
#Wsuggest-attribute=noreturn
#Wsuggest-attribute=format
#Wsuggest-attribute=pure
#Wsuggest-attribute=const

# fipa-pta fsched-pressure fipa-bit-cp ftree-vectorize
# fgcse-after-reload fdeclone-ctor-dtor
# fdevirtualize-at-ltrans fsched-stalled-insns=0

# fgcse-las causes compiler segfaults
AS_VAR_SET([OPTIMIZATION_FLAGS], [])
for flag in \
	O2 fstrict-aliasing mtune=znver1
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([OPTIMIZATION_FLAGS], [" -$flag"])],
		[], [$COMPILER_WERROR])
done

AS_VAR_SET([DEBUG_INFO_FLAGS], [])
for flag in g gcolumn-info
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([DEBUG_INFO_FLAGS], [" -$flag"])],
		[], [$COMPILER_WERROR])
done
AC_SUBST([DEBUG_INFO_FLAGS])

AS_VAR_SET([ASM_DEBUG_INFO_FLAGS], [])
for flag in Wa,-g g gcolumn-info
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([ASM_DEBUG_INFO_FLAGS], [" -$flag"])],
		[], [$COMPILER_WERROR])
done
AC_SUBST([ASM_DEBUG_INFO_FLAGS])


AS_VAR_SET([SMALLEST_CODE_FLAGS],[])
AX_CHECK_COMPILE_FLAG([-Oz],
	[AS_VAR_SET([SMALLEST_CODE_FLAGS], [-Oz])],
	[AX_CHECK_COMPILE_FLAG([-Os],
		[AS_VAR_SET([SMALLEST_CODE_FLAGS],[-Os])],
		[$COMPILER_WERROR])],
	[$COMPILER_WERROR])
for flag in falign-loops=0 falign-functions=0 falign-labels=0 falign-jumps=0 \
		mno-align-stringops Wno-ignored-optimization-argument \
		fstack-usage
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([SMALLEST_CODE_FLAGS], [" -$flag"])]
		[], [$COMPILER_WERROR])
done
AC_SUBST([SMALLEST_CODE_FLAGS])

for flag in ffreestanding fbuiltin
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([FREESTANDING_FLAGS], [" -$flag"])]
		[], [$COMPILER_WERROR])
done
AC_SUBST([FREESTANDING_FLAGS])

AS_VAR_SET([NOFLOAT_FLAGS], [])
for flag in mgeneral-regs-only
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([NOFLOAT_FLAGS], [" -$flag"])]
		[], [$COMPILER_WERROR])
done
AC_SUBST([NOFLOAT_FLAGS])

ELF32_FLAGS=$NOFLOAT_FLAGS
for flag in \
	m32 \
	Wl,-m,elf_i386 \
	Wl,-N \
	Wl,--no-dynamic-linker \
	static
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([ELF32_FLAGS], [" -$flag"])]
		[], [$COMPILER_WERROR])
done
AC_SUBST([ELF32_FLAGS])

ELF64_FLAGS=$NOFLOAT_FLAGS
for flag in \
	m64 \
	Wl,-m,elf_x86_64 \
	Wl,-N \
	Wl,--no-dynamic-linker
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([ELF64_FLAGS], [" -$flag"])]
		[], [$COMPILER_WERROR])
done
AC_SUBST([ELF64_FLAGS])

PE32_FLAGS=$NOFLOAT_FLAGS
for flag in \
	m32 \
	D__efi \
	fshort-wchar \
	Wl,-m,i386pe \
	Wl,-pie \
	Wl,-N \
	Wl,--no-dynamic-linker \
	Wl,--subsystem,10
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([PE32_FLAGS], [" -$flag"])]
		[], [$COMPILER_WERROR])
done
AC_SUBST([PE32_FLAGS])

PE64_FLAGS=$NOFLOAT_FLAGS
for flag in \
	m64 \
	D__efi \
	fshort-wchar \
	Wl,-m,i386pep \
	Wl,-pie \
	Wl,-N \
	Wl,--no-dynamic-linker \
	Wl,--subsystem,10
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([PE64_FLAGS], [" -$flag"])]
		[], [$COMPILER_WERROR])
done
AC_SUBST([PE64_FLAGS])

AS_VAR_SET([NO_LTO_FLAGS], [])
for flag in fno-lto
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([NO_LTO_FLAGS], [" -$flag"])]
		[], [$COMPILER_WERROR])
done
AC_SUBST([NO_LTO_FLAGS])

AS_VAR_SET([REGPARM_FLAGS], [])
for flag in mrtd mregparm=3
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([REGPARM_FLAGS], [" -$flag"])]
		[], [$COMPILER_WERROR])
done
AC_SUBST([REGPARM_FLAGS])

AC_MSG_NOTICE([Using libgcc parameters: $REGPARM_FLAGS $BOOTLOADER_FLAGS])
LIBGCC_ELF32=$($CXX $REGPARM_FLAGS $ELF32_FLAGS -print-libgcc-file-name)
LIBGCC_ELF64=$($CXX $REGPARM_FLAGS $ELF64_FLAGS -print-libgcc-file-name)
LIBGCC_PE32=$($CXX $REGPARM_FLAGS $PE32_FLAGS -print-libgcc-file-name)
LIBGCC_PE64=$($CXX $REGPARM_FLAGS $PE64_FLAGS -print-libgcc-file-name)
AC_MSG_NOTICE([Using elf32 libgcc: $LIBGCC_ELF32])
AC_MSG_NOTICE([Using elf64 libgcc: $LIBGCC_ELF64])
AC_MSG_NOTICE([Using pe32 libgcc: $LIBGCC_PE32])
AC_MSG_NOTICE([Using pe64 libgcc: $LIBGCC_PE64])
AC_SUBST([LIBGCC_ELF32])
AC_SUBST([LIBGCC_ELF64])
AC_SUBST([LIBGCC_PE32])
AC_SUBST([LIBGCC_PE64])

AS_VAR_SET([NO_PIC_FLAGS], [])
for flag in fno-pie fno-PIE fno-pic fno-PIC
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([NO_PIC_FLAGS], [" -$flag"])],
		[], [$COMPILER_WERROR])
done
AC_SUBST([NO_PIC_FLAGS])

AS_VAR_SET([BMI_FLAGS], [])
for flag in mabm mbmi mbmi2
do
	AX_CHECK_COMPILE_FLAG([-$flag],
		[AS_VAR_APPEND([BMI_FLAGS], [" -$flag"])]
		[], [$COMPILER_WERROR])
done
AC_SUBST([BMI_FLAGS])

AS_VAR_SET([DISABLE_FRAMEPOINTER_FLAGS], [])
AX_CHECK_COMPILE_FLAG([-fno-omit-frame-pointer],
	[AS_VAR_SET([DISABLE_FRAMEPOINTER_FLAGS], [" -fomit-frame-pointer"])]
	[], [$COMPILER_WERROR])
AC_SUBST([DISABLE_FRAMEPOINTER_FLAGS])

AS_VAR_SET([ENABLE_FRAMEPOINTER_FLAGS], [])
AX_CHECK_COMPILE_FLAG([-fomit-frame-pointer],
	[AS_VAR_SET([ENABLE_FRAMEPOINTER_FLAGS], [" -fno-omit-frame-pointer"])]
	[], [$COMPILER_WERROR])
AC_SUBST([ENABLE_FRAMEPOINTER_FLAGS])

AS_VAR_SET([NO_REDZONE_FLAGS], [])
AX_CHECK_COMPILE_FLAG([-mno-red-zone],
	[AS_VAR_SET([NO_REDZONE_FLAGS], [" -mno-red-zone"])]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_REDZONE_FLAGS])

AS_VAR_SET([NO_STACKPROTECTOR_FLAGS], [])
AX_CHECK_COMPILE_FLAG([-fno-stack-protector],
	[AS_VAR_SET([NO_STACKPROTECTOR_FLAGS], [" -fno-stack-protector"])]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_STACKPROTECTOR_FLAGS])

AS_VAR_SET([STACK_CHECK_FLAGS], [])
AX_CHECK_COMPILE_FLAG([-fstack-check],
	[AS_VAR_SET([STACK_CHECK_FLAGS], [" -fstack-check"])]
	[], [$COMPILER_WERROR])
AC_SUBST([STACK_CHECK_FLAGS])

AS_VAR_SET([NO_STACK_CHECK_FLAGS], [])
AX_CHECK_COMPILE_FLAG([-fno-stack-check],
	[AS_VAR_SET([NO_STACK_CHECK_FLAGS], [" -fno-stack-check"])]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_STACK_CHECK_FLAGS])

AS_VAR_SET([NO_RTTI_FLAGS], [])
AX_CHECK_COMPILE_FLAG([-fno-rtti],
	[AS_VAR_SET([NO_RTTI_FLAGS], [" -fno-rtti"])]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_RTTI_FLAGS])

AS_VAR_SET([NO_UNWIND_TABLES_FLAGS], [])
AX_CHECK_COMPILE_FLAG([-fno-asynchronous-unwind-tables],
	[AS_VAR_SET([NO_UNWIND_TABLES_FLAGS], [" -fno-asynchronous-unwind-tables"])]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_UNWIND_TABLES_FLAGS])

AS_VAR_SET([NO_EXCEPTIONS_FLAGS], [])
AX_CHECK_COMPILE_FLAG([-fno-exceptions],
	[AS_VAR_SET([NO_EXCEPTIONS_FLAGS], [" -fno-exceptions"])]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_EXCEPTIONS_FLAGS])

AS_VAR_SET([NO_COMMON_FLAGS], [])
AX_CHECK_COMPILE_FLAG([-fno-common],
	[AS_VAR_SET([NO_COMMON_FLAGS], [" -fno-common"])]
	[], [$COMPILER_WERROR])
AC_SUBST([NO_COMMON_FLAGS])

AS_VAR_SET([VECTORIZE_FLAGS], [])
AX_CHECK_COMPILE_FLAG([-ftree-vectorize],
	[AS_VAR_SET([VECTORIZE_FLAGS], [" -ftree-vectorize"])]
	[], [$COMPILER_WERROR])
AC_SUBST([VECTORIZE_FLAGS])

AS_VAR_SET([TUNE_FLAGS], [])
#AX_CHECK_COMPILE_FLAG([AS_VAR_SET([-mtune], [znver1])],
#	[AS_VAR_SET([TUNE_FLAGS], [" -mtune=znver1"])]
#	[], [$COMPILER_WERROR])
#AC_SUBST([TUNE_FLAGS])

AS_VAR_SET([OPTIMIZE_SPEED_FLAGS], [])
AS_VAR_SET([OPTIMIZE_SIZE_FLAGS], [])
AC_ARG_ENABLE(lto,
	AS_HELP_STRING([--enable-lto],
		[Enable link-time optimizations]), [
		AC_MSG_NOTICE([Enabling LTO])

		AS_VAR_SET([AR],[])
		AC_CHECK_TOOL([AR], [gcc-ar], [NOT_FOUND])
		AS_IF([test x"$AR" == x"NOT_FOUND"],
			[AC_MSG_NOTICE([Cannot find gcc-ar, LTO probably won't work])])
		AC_SUBST([AR])

		AS_VAR_SET([RANLIB],[])
		AC_CHECK_TOOL([RANLIB], [gcc-ranlib], [NOT_FOUND])
		AS_IF([test x"$AR" == x"NOT_FOUND"],
			[AC_MSG_NOTICE([Cannot find gcc-ranlib, LTO probably won't work])])
		AC_SUBST([RANLIB])

		AS_VAR_APPEND([OPTIMIZE_SIZE_FLAGS], [" $SMALLEST_CODE_FLAGS"])
		AS_VAR_APPEND([OPTIMIZE_SIZE_FLAGS], [" $DISABLE_FRAMEPOINTER_FLAGS"])
		AS_VAR_APPEND([OPTIMIZE_SPEED_FLAGS], [" $OPTIMIZATION_FLAGS"])
		AS_VAR_APPEND([OPTIMIZE_SPEED_FLAGS], [" $VECTORIZE_FLAGS"])
		AS_VAR_APPEND([OPTIMIZE_SPEED_FLAGS], [" $ENABLE_FRAMEPOINTER_FLAGS"])

		# $DISABLE_FRAMEPOINTER_FLAGS"
		AS_VAR_APPEND([OPTIMIZE_SPEED_FLAGS], [" $TUNE_FLAGS"])

		# Disable debugging
		AS_VAR_APPEND([OPTIMIZE_SIZE_FLAGS], [" -g0"])
		AS_VAR_APPEND([OPTIMIZE_SPEED_FLAGS], [" -g0"])
	], [
		AC_ARG_ENABLE(optimize,
			AS_HELP_STRING([--enable-optimize],
				[Enable size/speed optimizations]), [
				AC_MSG_NOTICE([Enabling optimizations])
				AS_VAR_APPEND([OPTIMIZE_SIZE_FLAGS], [" $SMALLEST_CODE_FLAGS"])
				AS_VAR_APPEND([OPTIMIZE_SIZE_FLAGS],
					[" $DISABLE_FRAMEPOINTER_FLAGS"])
				AS_VAR_APPEND([OPTIMIZE_SPEED_FLAGS], [" $OPTIMIZATION_FLAGS"])
				AS_VAR_APPEND([OPTIMIZE_SPEED_FLAGS], [" $VECTORIZE_FLAGS"])
				AS_VAR_APPEND([OPTIMIZE_SPEED_FLAGS],
					[" $ENABLE_FRAMEPOINTER_FLAGS"])

				# $DISABLE_FRAMEPOINTER_FLAGS"
				AS_VAR_APPEND([OPTIMIZE_SPEED_FLAGS], [" $TUNE_FLAGS"])
			], [
				AC_MSG_NOTICE([Disabling optimizations])
				AS_VAR_APPEND([OPTIMIZE_SIZE_FLAGS], [" -g -O0"])
				AS_VAR_APPEND([OPTIMIZE_SIZE_FLAGS],
					[" $ENABLE_FRAMEPOINTER_FLAGS"])
				AS_VAR_APPEND([OPTIMIZE_SPEED_FLAGS], [" -g -O0"])
				AS_VAR_APPEND([OPTIMIZE_SPEED_FLAGS],
					[" $ENABLE_FRAMEPOINTER_FLAGS"])
			]
		)
	]
)

AS_VAR_SET([DISABLE_OPTIMIZATION_FLAGS], ["-O0 $ENABLE_FRAMEPOINTER_FLAGS"])

AC_SUBST([DISABLE_OPTIMIZATION_FLAGS])
AC_SUBST([OPTIMIZE_SIZE_FLAGS])
AC_SUBST([OPTIMIZE_SPEED_FLAGS])

AC_OUTPUT
