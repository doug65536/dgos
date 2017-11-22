#pragma once
#include "types.h"

// Prototypes mostly for compiler builtins

/// http://en.cppreference.com/w/c/numeric/math/abs
/// http://en.cppreference.com/w/c/numeric/math/labs
/// http://en.cppreference.com/w/c/numeric/math/llabs
/// http://en.cppreference.com/w/c/numeric/math/imaxabs
/// http://en.cppreference.com/w/c/numeric/math/fabs
/// http://en.cppreference.com/w/c/numeric/math/div
/// http://en.cppreference.com/w/c/numeric/math/ldiv
/// http://en.cppreference.com/w/c/numeric/math/lldiv
/// http://en.cppreference.com/w/c/numeric/math/imaxdiv
/// http://en.cppreference.com/w/c/numeric/math/fmod
/// http://en.cppreference.com/w/c/numeric/math/remainder
/// http://en.cppreference.com/w/c/numeric/math/remquo
/// http://en.cppreference.com/w/c/numeric/math/fma
/// http://en.cppreference.com/w/c/numeric/math/fmax
/// http://en.cppreference.com/w/c/numeric/math/fmin
/// http://en.cppreference.com/w/c/numeric/math/fdim
/// http://en.cppreference.com/w/c/numeric/math/nan
/// http://en.cppreference.com/w/c/numeric/math/nanf
/// http://en.cppreference.com/w/c/numeric/math/nanl
/// http://en.cppreference.com/w/c/numeric/math/exp
/// http://en.cppreference.com/w/c/numeric/math/exp2
/// http://en.cppreference.com/w/c/numeric/math/expm1
/// http://en.cppreference.com/w/c/numeric/math/log
/// http://en.cppreference.com/w/c/numeric/math/log10
/// http://en.cppreference.com/w/c/numeric/math/log1p
/// http://en.cppreference.com/w/c/numeric/math/log2
/// http://en.cppreference.com/w/c/numeric/math/sqrt
/// http://en.cppreference.com/w/c/numeric/math/cbrt
/// http://en.cppreference.com/w/c/numeric/math/hypot
/// http://en.cppreference.com/w/c/numeric/math/pow
/// http://en.cppreference.com/w/c/numeric/math/sin
/// http://en.cppreference.com/w/c/numeric/math/cos
/// http://en.cppreference.com/w/c/numeric/math/tan
/// http://en.cppreference.com/w/c/numeric/math/asin
/// http://en.cppreference.com/w/c/numeric/math/acos
/// http://en.cppreference.com/w/c/numeric/math/atan
/// http://en.cppreference.com/w/c/numeric/math/atan2
/// http://en.cppreference.com/w/c/numeric/math/sinh
/// http://en.cppreference.com/w/c/numeric/math/cosh
/// http://en.cppreference.com/w/c/numeric/math/tanh
/// http://en.cppreference.com/w/c/numeric/math/asinh
/// http://en.cppreference.com/w/c/numeric/math/acosh
/// http://en.cppreference.com/w/c/numeric/math/atanh
/// http://en.cppreference.com/w/c/numeric/math/erf
/// http://en.cppreference.com/w/c/numeric/math/erfc
/// http://en.cppreference.com/w/c/numeric/math/lgamma
/// http://en.cppreference.com/w/c/numeric/math/tgamma
/// http://en.cppreference.com/w/c/numeric/math/ceil
/// http://en.cppreference.com/w/c/numeric/math/floor
/// http://en.cppreference.com/w/c/numeric/math/roundl
/// http://en.cppreference.com/w/c/numeric/math/roundll
/// http://en.cppreference.com/w/c/numeric/math/round
/// http://en.cppreference.com/w/c/numeric/math/trunc
/// http://en.cppreference.com/w/c/numeric/math/nearbyint
/// http://en.cppreference.com/w/c/numeric/math/rintl
/// http://en.cppreference.com/w/c/numeric/math/rintll
/// http://en.cppreference.com/w/c/numeric/math/rint
/// http://en.cppreference.com/w/c/numeric/math/ldexp
/// http://en.cppreference.com/w/c/numeric/math/scalbn
/// http://en.cppreference.com/w/c/numeric/math/scalbln
/// http://en.cppreference.com/w/c/numeric/math/ilogb
int ilogbf(float);
int ilogb(double);
int ilogbl(long double n);

/// http://en.cppreference.com/w/c/numeric/math/logb
/// http://en.cppreference.com/w/c/numeric/math/frexp
/// http://en.cppreference.com/w/c/numeric/math/modf
/// http://en.cppreference.com/w/c/numeric/math/nextafter

long double nextafterl(long double n, long double t);

/// http://en.cppreference.com/w/c/numeric/math/nexttoward
/// http://en.cppreference.com/w/c/numeric/math/copysign
/// http://en.cppreference.com/w/c/numeric/math/fpclassify
/// http://en.cppreference.com/w/c/numeric/math/isfinite
/// http://en.cppreference.com/w/c/numeric/math/isinf

#define isinf(n) __builtin_isinf((n))

/// http://en.cppreference.com/w/c/numeric/math/isnan

#define isnan(n) __builtin_isnan((n))

/// http://en.cppreference.com/w/c/numeric/math/isnormal
/// http://en.cppreference.com/w/c/numeric/math/signbit
/// http://en.cppreference.com/w/c/numeric/math/isgreater
/// http://en.cppreference.com/w/c/numeric/math/isgreaterequal
/// http://en.cppreference.com/w/c/numeric/math/isless
/// http://en.cppreference.com/w/c/numeric/math/islessequal
/// http://en.cppreference.com/w/c/numeric/math/islessgreater
/// http://en.cppreference.com/w/c/numeric/math/isunordered
/// http://en.cppreference.com/w/c/numeric/math/div_t
/// http://en.cppreference.com/w/c/numeric/math/ldiv_t
/// http://en.cppreference.com/w/c/numeric/math/lldiv_t
/// http://en.cppreference.com/w/c/numeric/math/imaxdiv_t
/// http://en.cppreference.com/w/c/numeric/math/float_tdouble_t
/// http://en.cppreference.com/w/c/numeric/math/HUGE_VALF
/// http://en.cppreference.com/w/c/numeric/math/HUGE_VAL
/// http://en.cppreference.com/w/c/numeric/math/HUGE_VALL

#define HUGE_VALF __builtin_huge_valf()
#define HUGE_VAL __builtin_huge_val()
#define HUGE_VALL __builtin_huge_vall()

/// http://en.cppreference.com/w/c/numeric/math/FP_FAST_FMAF
/// FP_FAST_FMA
/// FP_FAST_FMAL
/// http://en.cppreference.com/w/c/numeric/math/math_errhandling
/// http://en.cppreference.com/w/c/numeric/math/MATH_ERRNOMATH_ERRNOEXCEPT
/// http://en.cppreference.com/w/c/numeric/math/INFINITY
/// http://en.cppreference.com/w/c/numeric/math/NAN
/// http://en.cppreference.com/w/c/numeric/math/FP_ILOGB0FP_ILOGBNAN
/// http://en.cppreference.com/w/c/numeric/math/FP_NORMALFP_SUBNORMALFP_ZEROFP_INFINITEFP_NAN
/// http://en.cppreference.com/w/c/numeric/math/abs
/// http://en.cppreference.com/w/c/numeric/math/labs
/// http://en.cppreference.com/w/c/numeric/math/llabs

#define abs(n) __builtin_abs((n))
#define labs(n) __builtin_labs((n))
#define llabs(n) __builtin_llabs((n))

/// http://en.cppreference.com/w/c/numeric/math/div
/// http://en.cppreference.com/w/c/numeric/math/ldiv
/// http://en.cppreference.com/w/c/numeric/math/lldiv
/// http://en.cppreference.com/w/c/numeric/math/imaxabs
/// http://en.cppreference.com/w/c/numeric/math/imaxdiv
/// http://en.cppreference.com/w/c/numeric/math/fabs
/// http://en.cppreference.com/w/c/numeric/math/fabsf
/// http://en.cppreference.com/w/c/numeric/math/fabsl
/// http://en.cppreference.com/w/c/numeric/math/fmod
/// http://en.cppreference.com/w/c/numeric/math/fmodf
/// http://en.cppreference.com/w/c/numeric/math/fmodl
/// http://en.cppreference.com/w/c/numeric/math/remainder
/// http://en.cppreference.com/w/c/numeric/math/remainderf
/// http://en.cppreference.com/w/c/numeric/math/remainderl
/// http://en.cppreference.com/w/c/numeric/math/remquo
/// http://en.cppreference.com/w/c/numeric/math/remquof
/// http://en.cppreference.com/w/c/numeric/math/remquol
/// http://en.cppreference.com/w/c/numeric/math/fma
/// http://en.cppreference.com/w/c/numeric/math/fmaf
/// http://en.cppreference.com/w/c/numeric/math/fmal
/// http://en.cppreference.com/w/c/numeric/math/fmax
/// http://en.cppreference.com/w/c/numeric/math/fmaxf
/// http://en.cppreference.com/w/c/numeric/math/fmaxl
/// http://en.cppreference.com/w/c/numeric/math/fmin
/// http://en.cppreference.com/w/c/numeric/math/fminf
/// http://en.cppreference.com/w/c/numeric/math/fminl
/// http://en.cppreference.com/w/c/numeric/math/fdim
/// http://en.cppreference.com/w/c/numeric/math/fdimf
/// http://en.cppreference.com/w/c/numeric/math/fdiml
/// http://en.cppreference.com/w/c/numeric/math/nan
/// http://en.cppreference.com/w/c/numeric/math/nanf
/// http://en.cppreference.com/w/c/numeric/math/nanl
/// http://en.cppreference.com/w/c/numeric/math/exp
/// http://en.cppreference.com/w/c/numeric/math/expf
/// http://en.cppreference.com/w/c/numeric/math/expl
/// http://en.cppreference.com/w/c/numeric/math/exp2
/// http://en.cppreference.com/w/c/numeric/math/exp2f
/// http://en.cppreference.com/w/c/numeric/math/exp2l
/// http://en.cppreference.com/w/c/numeric/math/expm1
/// http://en.cppreference.com/w/c/numeric/math/expm1f
/// http://en.cppreference.com/w/c/numeric/math/expm1l
/// http://en.cppreference.com/w/c/numeric/math/log
/// http://en.cppreference.com/w/c/numeric/math/logf
/// http://en.cppreference.com/w/c/numeric/math/logl
/// http://en.cppreference.com/w/c/numeric/math/log10
/// http://en.cppreference.com/w/c/numeric/math/log10f
/// http://en.cppreference.com/w/c/numeric/math/log10l
/// http://en.cppreference.com/w/c/numeric/math/log2
/// http://en.cppreference.com/w/c/numeric/math/log2f
/// http://en.cppreference.com/w/c/numeric/math/log2l
/// http://en.cppreference.com/w/c/numeric/math/log1p
/// http://en.cppreference.com/w/c/numeric/math/log1pf
/// http://en.cppreference.com/w/c/numeric/math/log1pl
/// http://en.cppreference.com/w/c/numeric/math/pow
/// http://en.cppreference.com/w/c/numeric/math/powf
/// http://en.cppreference.com/w/c/numeric/math/powl
/// http://en.cppreference.com/w/c/numeric/math/sqrt
/// http://en.cppreference.com/w/c/numeric/math/sqrtf
/// http://en.cppreference.com/w/c/numeric/math/sqrtl

#if defined(__x86_64__) || defined(__x86__)
#define sqrtf(n) (__builtin_ia32_sqrtss(__f32_vec4{ (n), 0, 0, 0 })[0])
#else
#define sqrtf(n) __builtin_sqrtf((n))
#endif

#if defined(__x86_64__) || defined(__x86__)
#define sqrt(n) (__builtin_ia32_sqrtsd((__fvec2){ (n), 0 })[0])
#else
#define sqrt(n) __builtin_sqrt((n))
#endif

/// http://en.cppreference.com/w/c/numeric/math/cbrt
/// http://en.cppreference.com/w/c/numeric/math/cbrtf
/// http://en.cppreference.com/w/c/numeric/math/cbrtl
/// http://en.cppreference.com/w/c/numeric/math/hypot
/// http://en.cppreference.com/w/c/numeric/math/hypotf
/// http://en.cppreference.com/w/c/numeric/math/hypotl
/// http://en.cppreference.com/w/c/numeric/math/sin
/// http://en.cppreference.com/w/c/numeric/math/sinf
/// http://en.cppreference.com/w/c/numeric/math/sinl
/// http://en.cppreference.com/w/c/numeric/math/cos
/// http://en.cppreference.com/w/c/numeric/math/cosf
/// http://en.cppreference.com/w/c/numeric/math/cosl
/// http://en.cppreference.com/w/c/numeric/math/tan
/// http://en.cppreference.com/w/c/numeric/math/tanf
/// http://en.cppreference.com/w/c/numeric/math/tanl
/// http://en.cppreference.com/w/c/numeric/math/asin
/// http://en.cppreference.com/w/c/numeric/math/asinf
/// http://en.cppreference.com/w/c/numeric/math/asinl
/// http://en.cppreference.com/w/c/numeric/math/acos
/// http://en.cppreference.com/w/c/numeric/math/acosf
/// http://en.cppreference.com/w/c/numeric/math/acosl
/// http://en.cppreference.com/w/c/numeric/math/atan
/// http://en.cppreference.com/w/c/numeric/math/atanf
/// http://en.cppreference.com/w/c/numeric/math/atanl
/// http://en.cppreference.com/w/c/numeric/math/atan2
/// http://en.cppreference.com/w/c/numeric/math/atan2f
/// http://en.cppreference.com/w/c/numeric/math/atan2l
/// http://en.cppreference.com/w/c/numeric/math/sinh
/// http://en.cppreference.com/w/c/numeric/math/sinhf
/// http://en.cppreference.com/w/c/numeric/math/sinhl
/// http://en.cppreference.com/w/c/numeric/math/cosh
/// http://en.cppreference.com/w/c/numeric/math/coshf
/// http://en.cppreference.com/w/c/numeric/math/coshl
/// http://en.cppreference.com/w/c/numeric/math/tanh
/// http://en.cppreference.com/w/c/numeric/math/tanhf
/// http://en.cppreference.com/w/c/numeric/math/tanhl
/// http://en.cppreference.com/w/c/numeric/math/asinh
/// http://en.cppreference.com/w/c/numeric/math/asinhf
/// http://en.cppreference.com/w/c/numeric/math/asinhl
/// http://en.cppreference.com/w/c/numeric/math/acosh
/// http://en.cppreference.com/w/c/numeric/math/acoshf
/// http://en.cppreference.com/w/c/numeric/math/acoshl
/// http://en.cppreference.com/w/c/numeric/math/atanh
/// http://en.cppreference.com/w/c/numeric/math/atanhf
/// http://en.cppreference.com/w/c/numeric/math/atanhl
/// http://en.cppreference.com/w/c/numeric/math/erf
/// http://en.cppreference.com/w/c/numeric/math/erff
/// http://en.cppreference.com/w/c/numeric/math/erfl
/// http://en.cppreference.com/w/c/numeric/math/erfc
/// http://en.cppreference.com/w/c/numeric/math/erfcf
/// http://en.cppreference.com/w/c/numeric/math/erfcl
/// http://en.cppreference.com/w/c/numeric/math/tgamma
/// http://en.cppreference.com/w/c/numeric/math/tgammaf
/// http://en.cppreference.com/w/c/numeric/math/tgammal
/// http://en.cppreference.com/w/c/numeric/math/lgamma
/// http://en.cppreference.com/w/c/numeric/math/lgammaf
/// http://en.cppreference.com/w/c/numeric/math/lgammal
/// http://en.cppreference.com/w/c/numeric/math/ceil
/// http://en.cppreference.com/w/c/numeric/math/ceilf
/// http://en.cppreference.com/w/c/numeric/math/ceill
/// http://en.cppreference.com/w/c/numeric/math/floor
/// http://en.cppreference.com/w/c/numeric/math/floorf
/// http://en.cppreference.com/w/c/numeric/math/floorl
/// http://en.cppreference.com/w/c/numeric/math/trunc
/// http://en.cppreference.com/w/c/numeric/math/truncf
/// http://en.cppreference.com/w/c/numeric/math/truncl
/// http://en.cppreference.com/w/c/numeric/math/round
/// http://en.cppreference.com/w/c/numeric/math/lround
/// http://en.cppreference.com/w/c/numeric/math/llround
/// http://en.cppreference.com/w/c/numeric/math/nearbyint
/// http://en.cppreference.com/w/c/numeric/math/nearbyintf
/// http://en.cppreference.com/w/c/numeric/math/nearbyintl
/// http://en.cppreference.com/w/c/numeric/math/rint
/// http://en.cppreference.com/w/c/numeric/math/rintf
/// http://en.cppreference.com/w/c/numeric/math/rintl
/// http://en.cppreference.com/w/c/numeric/math/lrint
/// http://en.cppreference.com/w/c/numeric/math/lrintf
/// http://en.cppreference.com/w/c/numeric/math/lrintl
/// http://en.cppreference.com/w/c/numeric/math/llrint
/// http://en.cppreference.com/w/c/numeric/math/llrintf
/// http://en.cppreference.com/w/c/numeric/math/llrintl
/// http://en.cppreference.com/w/c/numeric/math/frexp
/// http://en.cppreference.com/w/c/numeric/math/frexpf
/// http://en.cppreference.com/w/c/numeric/math/frexpl
/// http://en.cppreference.com/w/c/numeric/math/ldexp
/// http://en.cppreference.com/w/c/numeric/math/ldexpf
/// http://en.cppreference.com/w/c/numeric/math/ldexpl
/// http://en.cppreference.com/w/c/numeric/math/modf
/// http://en.cppreference.com/w/c/numeric/math/modff
/// http://en.cppreference.com/w/c/numeric/math/modfl
/// http://en.cppreference.com/w/c/numeric/math/scalbn
/// http://en.cppreference.com/w/c/numeric/math/scalbnf
/// http://en.cppreference.com/w/c/numeric/math/scalbnl
/// http://en.cppreference.com/w/c/numeric/math/scalbln
/// http://en.cppreference.com/w/c/numeric/math/scalblnf
/// http://en.cppreference.com/w/c/numeric/math/scalblnl
/// http://en.cppreference.com/w/c/numeric/math/ilogb
/// http://en.cppreference.com/w/c/numeric/math/ilogbf
/// http://en.cppreference.com/w/c/numeric/math/ilogbl
/// http://en.cppreference.com/w/c/numeric/math/logb
/// http://en.cppreference.com/w/c/numeric/math/logbf
/// http://en.cppreference.com/w/c/numeric/math/logbl
/// http://en.cppreference.com/w/c/numeric/math/nextafter
/// http://en.cppreference.com/w/c/numeric/math/nextafterf
/// http://en.cppreference.com/w/c/numeric/math/nextafterl
/// http://en.cppreference.com/w/c/numeric/math/nexttoward
/// http://en.cppreference.com/w/c/numeric/math/nexttowardf
/// http://en.cppreference.com/w/c/numeric/math/nexttowardl
/// http://en.cppreference.com/w/c/numeric/math/copysign
/// http://en.cppreference.com/w/c/numeric/math/copysignf
/// http://en.cppreference.com/w/c/numeric/math/copysignl
/// http://en.cppreference.com/w/c/numeric/math/fpclassify
/// http://en.cppreference.com/w/c/numeric/math/isfinite
/// http://en.cppreference.com/w/c/numeric/math/isinf
/// http://en.cppreference.com/w/c/numeric/math/isnan
/// http://en.cppreference.com/w/c/numeric/math/isnormal
/// http://en.cppreference.com/w/c/numeric/math/signbit
/// http://en.cppreference.com/w/c/numeric/math/isgreater
/// http://en.cppreference.com/w/c/numeric/math/isgreaterequal
/// http://en.cppreference.com/w/c/numeric/math/isless
/// http://en.cppreference.com/w/c/numeric/math/islessequal
/// http://en.cppreference.com/w/c/numeric/math/islessgreater
/// http://en.cppreference.com/w/c/numeric/math/isunordered
/// http://en.cppreference.com/w/c/numeric/math/div_t
/// http://en.cppreference.com/w/c/numeric/math/ldiv_t
/// http://en.cppreference.com/w/c/numeric/math/lldiv_t
/// http://en.cppreference.com/w/c/numeric/math/imaxdiv_t
/// http://en.cppreference.com/w/c/numeric/math/float_t
/// http://en.cppreference.com/w/c/numeric/math/double_t
/// http://en.cppreference.com/w/c/numeric/math/HUGE_VALF
/// http://en.cppreference.com/w/c/numeric/math/HUGE_VAL
/// http://en.cppreference.com/w/c/numeric/math/HUGE_VALL
/// http://en.cppreference.com/w/c/numeric/math/INFINITY
/// http://en.cppreference.com/w/c/numeric/math/NAN
/// http://en.cppreference.com/w/c/numeric/math/FP_FAST_FMAF
/// http://en.cppreference.com/w/c/numeric/math/FP_FAST_FMA
/// http://en.cppreference.com/w/c/numeric/math/FP_FAST_FMAL
/// http://en.cppreference.com/w/c/numeric/math/FP_ILOGB0
/// http://en.cppreference.com/w/c/numeric/math/FP_ILOGBNAN
/// http://en.cppreference.com/w/c/numeric/math/math_errhandling
/// http://en.cppreference.com/w/c/numeric/math/MATH_ERRNO
/// http://en.cppreference.com/w/c/numeric/math/MATH_ERREXCEPT
/// http://en.cppreference.com/w/c/numeric/math/FP_NORMAL
/// http://en.cppreference.com/w/c/numeric/math/FP_SUBNORMAL
/// http://en.cppreference.com/w/c/numeric/math/FP_ZERO
/// http://en.cppreference.com/w/c/numeric/math/FP_INFINITE
/// http://en.cppreference.com/w/c/numeric/math/FP_NAN
/// http://en.cppreference.com/w/c/numeric/math/
