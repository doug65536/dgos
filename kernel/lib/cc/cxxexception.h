#pragma once
#include "types.h"

__BEGIN_NAMESPACE_EXT

class exception;

class exception {
public:
    exception() noexcept = default;
    exception(exception const& rhs) noexcept = default;
    exception& operator=(exception const& rhs) = default;
    virtual ~exception();
    virtual char const* what() const noexcept;
};

class bad_alloc : public exception {
public:
    bad_alloc() = default;

    char const *what() const noexcept override;
};

class bad_cast : public exception {
public:
    bad_cast() = default;

    char const *what() const noexcept override;
};

class bad_function_call : public exception {
public:
    bad_function_call() = default;

    char const *what() const noexcept override;
};

class bad_array_new_length : public bad_alloc {
public:
    bad_array_new_length() = default;

    char const *what() const noexcept override;
};

class bad_exception : public exception {
public:
    bad_exception() = default;

    char const *what() const noexcept override;
};

class logic_error {
    // fixme
};

class out_of_range : public logic_error {
public:
    out_of_range(/*fixme: string const& __message*/);

    out_of_range(char const *__message);
};


__END_NAMESPACE_EXT

__BEGIN_NAMESPACE_EXT

class cpu_exception : public exception {};

class gpf_exception : public cpu_exception {
public:
    gpf_exception(int err_code)
        : cpu_exception()
        , err_code(err_code)
    {
    }

    ~gpf_exception() = default;
    char const *what() const noexcept override;
    int const err_code;
};

__END_NAMESPACE_EXT

// Not making a mess to resolve circular dependency design mess
// that pulls in too much stuff
#if 0

__BEGIN_NAMESPACE_EXT

class logic_error;
class invalid_argument;
class domain_error;
class invalid_argument_error;
class length_error;
class out_of_range_error;
class future_error;
class runtime_error;
class underflow_error;
class overflow_error;
class system_error;
class bad_typeid;
class bad_cast;
class bad_function_call;
class bad_alloc;
class bad_array_new_length;
class bad_exception;
// ===

class logic_error : public exception {
public:
    logic_error(string const& __message);

    logic_error(char const *__message);

    char const *what() const noexcept;

    string __message;
};

class invalid_argument : public logic_error {
public:
    invalid_argument(string const& __message);

    invalid_argument(char const *__message);
};

class domain_error : public logic_error {
public:
    domain_error(string const& __message);

    domain_error(char const *__message);
};

class length_error  : public logic_error {
    length_error(string const& __message);

    length_error(char const *__message);
};

class future_error : public logic_error {
public:
    future_error(string const& __message);

    future_error(char const *__message);
};

// ===

class runtime_error : public exception {
public:
    runtime_error(string const& __message);

    runtime_error(char const *__message);

    char const *what() const noexcept;

    string __message;
};

class underflow_error : public runtime_error {
public:
    underflow_error(string const& __message);

    underflow_error(char const *__message);
};

class overflow_error : public runtime_error {
public:
    overflow_error(string const& __message);

    overflow_error(char const *__message);
};

class system_error : public runtime_error {
public:
    system_error(string const& __message);

    system_error(char const *__message);
};

// ===

#endif
