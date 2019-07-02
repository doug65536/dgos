#pragma once
#include "types.h"
#include "except_asm.h"
#include "functional.h"

struct __exception_context_t;

struct __exception_context_t {
    __exception_jmp_buf_t __exception_state;
    uint32_t __exception_code;
    uint32_t flags;
    __exception_context_t *__next;
};

extern "C" long __exception_handler_remove(void);
extern "C" long __exception_handler_invoke(long exception_code);

__exception_jmp_buf_t *__exception_handler_add(void);

__exception_jmp_buf_t *
__exception_handler_add_owned(__exception_context_t *ectx);

template<typename _T, typename _C>
class __exception_catcher_t
{
public:
    __exception_catcher_t(_T&& __guarded, _C&& __catcher)
        : __guarded(std::forward<_T>(__guarded))
        , __catcher(std::forward<_C>(__catcher))
    {
    }

    ~__exception_catcher_t()
    {
        __exception_handler_remove();
    }

    _T __guarded;
    _C __catcher;
    __exception_context_t ectx;
};

template<typename _T, typename _C>
void try_catch(_T&& __guarded, _C&& __catcher)
{
    __exception_catcher_t ec(std::forward<_T>(__guarded),
                             std::forward<_C>(__catcher));

    if (__setjmp(__exception_handler_add_owned(&ec.ectx)) == 0) {
        ec.__guarded();
    } else {
        // __exception_setjmp returned again, true this time, run catch
        ec.__catcher();
    }
}

//#define __try if (__exception_setjmp(__exception_handler_add()) == 0)
//
//#define __catch if (__exception_handler_remove() >= 0)
//#define __catch_code(__code) if ((__code = __exception_handler_remove()) >= 0)
