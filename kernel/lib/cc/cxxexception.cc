#include "cxxexception.h"

ext::exception::~exception()
{
}

char const *ext::exception::what() const noexcept
{
    return "ext::exception";
}

char const *ext::bad_alloc::what() const noexcept
{
    return "ext::bad_alloc";
}

#if 0
std::logic_error::logic_error(string const&__message)
    : __message(__message)
{
}

std::logic_error::logic_error(char const *__message)
    : __message(__message)
{
}

char const *std::logic_error::what() const noexcept
{
    return __message.c_str();
}

std::invalid_argument::invalid_argument(string const& __message)
    : logic_error(__message)
{
}

std::invalid_argument::invalid_argument(char const *__message)
    : logic_error(__message)
{
}

std::domain_error::domain_error(string const& __message)
    : logic_error(__message)
{
}

std::domain_error::domain_error(char const *__message)
    : logic_error(__message)
{
}

std::length_error::length_error(string const& __message)
    : logic_error(__message)
{
}

std::length_error::length_error(char const *__message)
    : logic_error(__message)
{
}

ext::out_of_range::out_of_range(string const& __message)
    : logic_error(__message)
{
}

ext::out_of_range::out_of_range(char const *__message)
    : logic_error(__message)
{
}

std::future_error::future_error(string const& __message)
    : logic_error(__message)
{
}

std::future_error::future_error(char const *__message)
    : logic_error(__message)
{
}

std::runtime_error::runtime_error(string const& __message)
    : __message(__message)
{
}

std::runtime_error::runtime_error(char const *__message)
    : __message(__message)
{
}

char const *std::runtime_error::what() const noexcept
{
    return __message.c_str();
}

std::underflow_error::underflow_error(string const& __message)
    : runtime_error(__message)
{
}

std::underflow_error::underflow_error(char const *__message)
    : runtime_error(__message)
{
}

std::overflow_error::overflow_error(string const& __message)
    : runtime_error(__message)
{
}

std::overflow_error::overflow_error(char const *__message)
    : runtime_error(__message)
{
}

std::system_error::system_error(string const& __message)
    : runtime_error(__message)
{
}

std::system_error::system_error(char const *__message)
    : runtime_error(__message)
{
}

std::bad_typeid::bad_typeid(string const& __message)
    : __message(__message)
{
}

std::bad_typeid::bad_typeid(char const *__message)
    : __message(__message)
{
}

char const *std::bad_cast::what() const noexcept
{
    return "std::bad_cast";
}

char const *std::bad_function_call::what() const noexcept
{
    return "std::bad_function_call";
}

char const *std::bad_array_new_length::what() const noexcept
{
    return "std::bad_array_new_length";
}

char const *std::bad_exception::what() const noexcept
{
    return "std::bad_exception";
}
#endif

char const *ext::gpf_exception::what() const noexcept
{
    return "General Protection Fault";
}
