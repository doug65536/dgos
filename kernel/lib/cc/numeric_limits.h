#pragma once
#include "type_traits.h"

enum float_round_style {
    round_indeterminate       = -1,
    round_toward_zero         = 0,
    round_to_nearest          = 1,
    round_toward_infinity     = 2,
    round_toward_neg_infinity = 3
};

template<typename T> class numeric_limits;
template<> class numeric_limits<bool>;
template<> class numeric_limits<char>;
template<> class numeric_limits<signed char>;
template<> class numeric_limits<unsigned char>;
template<> class numeric_limits<wchar_t>;
template<> class numeric_limits<char16_t>;
template<> class numeric_limits<char32_t>;
template<> class numeric_limits<short>;
template<> class numeric_limits<unsigned short>;
template<> class numeric_limits<int>;
template<> class numeric_limits<unsigned int>;
template<> class numeric_limits<long>;
template<> class numeric_limits<unsigned long>;
template<> class numeric_limits<long long>;
template<> class numeric_limits<unsigned long long>;
template<> class numeric_limits<float>;
template<> class numeric_limits<double>;
template<> class numeric_limits<long double>;

template<typename T> class numeric_limits
{
public:
    static constexpr bool is_specialized = false;
    static constexpr bool is_signed = false;
    static constexpr bool is_integer = false;
    static constexpr bool is_exact = false;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr bool has_denorm = false;
    static constexpr bool has_denorm_loss = false;
    static constexpr float_round_style round_style = round_indeterminate;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = false;
    static constexpr bool is_modulo = false;
    static constexpr int digits = 0;
    static constexpr int digits10 = 0;
    static constexpr int max_digits10 = 0;
    static constexpr int radix = 0;
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
};

template<typename T>
class __basic_integral_numeric_limits
{
public:
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = ::is_signed<T>::value;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = false;
    static constexpr bool has_quiet_NaN = false;
    static constexpr bool has_signaling_NaN = false;
    static constexpr bool has_denorm = false;
    static constexpr bool has_denorm_loss = false;
    static constexpr float_round_style round_style = round_toward_neg_infinity;
    static constexpr bool is_iec559 = false;
    static constexpr bool is_bounded = true;
    static constexpr bool is_modulo = true;
    static constexpr int digits = sizeof(T) * 8 - ::is_signed<T>::value;
    static constexpr int digits10 = digits * __builtin_log10(2);
    static constexpr int max_digits10 = 0;
    static constexpr int radix = 2;
    static constexpr int min_exponent = 0;
    static constexpr int min_exponent10 = 0;
    static constexpr int max_exponent = 0;
    static constexpr int max_exponent10 = 0;
    static constexpr bool traps = true;
    static constexpr bool tinyness_before = false;
};

class __basic_fp_numeric_limits
{
public:
    static constexpr bool is_specialized = true;
    static constexpr bool is_signed = true;
    static constexpr bool is_integer = true;
    static constexpr bool is_exact = true;
    static constexpr bool has_infinity = true;
    static constexpr bool has_quiet_NaN = true;
    static constexpr bool has_signaling_NaN = true;
    static constexpr bool has_denorm = true;
    static constexpr bool has_denorm_loss = true;
    static constexpr float_round_style round_style = round_to_nearest;
    static constexpr bool is_iec559 = true;
    static constexpr bool is_bounded = false;
    static constexpr bool is_modulo = false;
    //static constexpr int digits = 0;
    //static constexpr int digits10 = 0;
    //static constexpr int max_digits10 = 0;
    static constexpr int radix = __FLT_RADIX__;
    //static constexpr int min_exponent = 0;
    //static constexpr int min_exponent10 = 0;
    //static constexpr int max_exponent = 0;
    //static constexpr int max_exponent10 = 0;
    static constexpr bool traps = false;
    static constexpr bool tinyness_before = false;
};

template<> class numeric_limits<bool>
        : public __basic_integral_numeric_limits<bool>
{
    static constexpr bool traps = false;
};

template<> class numeric_limits<wchar_t>
        : public __basic_integral_numeric_limits<wchar_t>
{
};

template<> class numeric_limits<char16_t>
        : public __basic_integral_numeric_limits<char16_t>
{
};

template<> class numeric_limits<char32_t>
        : public __basic_integral_numeric_limits<char32_t>
{
};

template<> class numeric_limits<char>
        : public __basic_integral_numeric_limits<char>
{
};

template<> class numeric_limits<unsigned char>
        : public __basic_integral_numeric_limits<unsigned char>
{
};

template<> class numeric_limits<signed char>
        : public __basic_integral_numeric_limits<signed char>
{
};

template<> class numeric_limits<unsigned short>
        : public __basic_integral_numeric_limits<unsigned short>
{
};

template<> class numeric_limits<signed short>
        : public __basic_integral_numeric_limits<signed short>
{
};

template<> class numeric_limits<unsigned int>
        : public __basic_integral_numeric_limits<unsigned int>
{
};

template<> class numeric_limits<signed int>
        : public __basic_integral_numeric_limits<signed int>
{
};

template<> class numeric_limits<unsigned long>
        : public __basic_integral_numeric_limits<unsigned long>
{
};

template<> class numeric_limits<signed long>
        : public __basic_integral_numeric_limits<signed long>
{
};

template<> class numeric_limits<unsigned long long>
        : public __basic_integral_numeric_limits<unsigned long long>
{
};

template<> class numeric_limits<signed long long>
        : public __basic_integral_numeric_limits<signed long long>
{
};

template<> class numeric_limits<float>
        : public __basic_fp_numeric_limits
{
    static constexpr int digits = __FLT_DIG__;
    static constexpr int digits10 = __FLT_DECIMAL_DIG__;
    static constexpr int max_digits10 = __FLT_MAX_10_EXP__;
    static constexpr int min_exponent = __FLT_MIN_EXP__;
    static constexpr int min_exponent10 = __FLT_MIN_10_EXP__;
    static constexpr int max_exponent = __FLT_MAX_EXP__;
    static constexpr int max_exponent10 = __FLT_MAX_10_EXP__;
};

template<> class numeric_limits<double>
        : public __basic_fp_numeric_limits
{
    static constexpr int digits = __DBL_DIG__;
    static constexpr int digits10 = __DBL_DECIMAL_DIG__;
    static constexpr int max_digits10 = __DBL_MAX_10_EXP__;
    static constexpr int min_exponent = __DBL_MIN_EXP__;
    static constexpr int min_exponent10 = __DBL_MIN_10_EXP__;
    static constexpr int max_exponent = __DBL_MAX_EXP__;
    static constexpr int max_exponent10 = __DBL_MAX_10_EXP__;
};

template<> class numeric_limits<long double>
        : public __basic_fp_numeric_limits
{
    static constexpr int digits = __LDBL_DIG__;
    static constexpr int digits10 = __LDBL_DECIMAL_DIG__;
    static constexpr int max_digits10 = __LDBL_MAX_10_EXP__;
    static constexpr int min_exponent = __LDBL_MIN_EXP__;
    static constexpr int min_exponent10 = __LDBL_MIN_10_EXP__;
    static constexpr int max_exponent = __LDBL_MAX_EXP__;
    static constexpr int max_exponent10 = __LDBL_MAX_10_EXP__;
};
