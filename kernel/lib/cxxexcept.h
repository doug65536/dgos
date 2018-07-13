#pragma once

#include "types.h"
#include <unwind.h>

namespace __cxxabiv1 {
class __class_type_info;
}

class type_info {
public:
    virtual ~type_info();

    char const* name() const
    { return __name; }

    bool before(const type_info& __arg) const;

    bool operator==(const type_info& __arg) const;
    bool operator!=(const type_info& __arg) const
    { return !operator==(__arg); }

    // Return true if this is a pointer type of some kind
    virtual bool __is_pointer_p() const;

    // Return true if this is a function type
    virtual bool __is_function_p() const;

    virtual bool __do_catch(const type_info *__thr_type, void **__thr_obj,
                            unsigned __outer) const;

    virtual bool __do_upcast(const __cxxabiv1::__class_type_info *__target,
                             void **__obj_ptr) const;

protected:
    char const *__name;

    explicit type_info(char const *__n): __name(__n) { }

private:
    /// Assigning type_info is not supported.
    type_info& operator=(const type_info&);
    type_info(const type_info&);
};

extern "C" {

_noreturn
void abort();

_noreturn
void __cxa_throw_bad_array_new_length();

void *__cxa_allocate_exception(size_t thrown_size);
void __cxa_free_exception(void *thrown_exception);

_Unwind_Reason_Code __gxx_personality_v0(
        int version, _Unwind_Action actions, uint64_t exceptionClass,
        _Unwind_Exception* unwind_exception, _Unwind_Context* context);

}
