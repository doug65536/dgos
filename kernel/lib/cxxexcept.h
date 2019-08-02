#pragma once

#include "types.h"
#include <unwind.h>
#include "string.h"

__BEGIN_NAMESPACE_STD
void terminate();
class type_info;
__END_NAMESPACE_STD

__BEGIN_DECLS

_noreturn
void abort();

struct __cxa_exception;


_noreturn
void __cxa_throw_bad_array_new_length();

void *__cxa_allocate_exception(size_t thrown_size);
void __cxa_free_exception(void *thrown_exception);

struct __cxa_eh_globals;

__cxa_eh_globals *__cxa_get_globals(void);
__cxa_eh_globals *__cxa_get_globals_fast(void);
void __cxa_throw(void* __thrown_exception,
                 std::type_info * __tinfo,
                 void (*__destructor)(void*));
void *__cxa_begin_catch(void *__thrown_exception);
void __cxa_end_catch();
void __cxa_rethrow();
__cxa_exception *__cxa_get_exception_ptr();
std::type_info *__cxa_current_exception_type();

_Unwind_Reason_Code __gxx_personality_v0(
        int version, _Unwind_Action actions,
        _Unwind_Exception_Class exceptionClass,
        _Unwind_Exception* unwind_exception, _Unwind_Context* context);

__END_DECLS

typedef void (*unexpected_handler)();
typedef void (*terminate_handler)();

// http://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html#cxx-data
struct __cxa_exception {
    std::type_info *exceptionType;
    void (*exceptionDestructor) (void *);
    unexpected_handler unexpectedHandler;
    terminate_handler terminateHandler;
    __cxa_exception *nextException;

    int handlerCount;
    int	handlerSwitchValue;
    char const *actionRecord;
    char const *languageSpecificData;
    void *catchTemp;
    void *adjustedPtr;

    _Unwind_Exception unwindHeader;
};

struct __cxa_eh_globals {
    __cxa_exception *caughtExceptions = nullptr;
    uintptr_t uncaughtExceptions = 0;
};

// itanium-cxx-abi, interface to compiler generated type info
__BEGIN_NAMESPACE_STD
class type_info {
public:
    virtual ~type_info();
    bool operator==(type_info const& __rhs) const;
    bool operator!=(type_info const& __rhs) const;
    bool before(type_info const& __rhs) const;
    char const *name() const;
private:
    type_info (type_info const& rhs) = delete;
    type_info& operator= (const type_info& rhs) = delete;
private:
     char const *__type_name;
};
__END_NAMESPACE_STD

namespace __cxxabiv1 {

class __class_type_info : public std::type_info
{
public:
    virtual ~__class_type_info();
};

class __si_class_type_info : public __class_type_info
{
public:
    ~__si_class_type_info();
    __class_type_info const *__base_type;
};

struct __base_class_type_info {
public:
    __class_type_info const *__base_type;
    long __offset_flags;

    enum __offset_flags_masks {
        __virtual_mask = 0x1,
        __public_mask = 0x2,
        __offset_shift = 8
    };
};

class __vmi_class_type_info : public __class_type_info {
public:
    ~__vmi_class_type_info();
    unsigned int __flags;
    unsigned int __base_count;
    __base_class_type_info __base_info[1];

    enum __flags_masks {
        __non_diamond_repeat_mask = 0x1,
        __diamond_shaped_mask = 0x2
    };
};
}
