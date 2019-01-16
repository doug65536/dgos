#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
//#include <typeinfo>
//#include <cxxabi.h>

static char emergency_space[4096];
static size_t emergency_ptr = sizeof(emergency_space);

namespace std { void terminate(); }

/// Effects: Allocates memory to hold the exception to be thrown.
/// thrown_size is the size of the exception object.
/// Can allocate additional memory to hold private data.
/// If memory can not be allocated, call std::terminate().
///
/// Returns: A pointer to the memory allocated for the exception object.
extern "C" void *__cxa_allocate_exception(size_t thrown_size) throw()
{
    if (emergency_ptr > thrown_size)
        return emergency_space + (emergency_ptr -= thrown_size);
    std::terminate();
    __builtin_unreachable();
}

/// Effects: Frees memory allocated by __cxa_allocate_exception.
void __cxa_free_exception(void * thrown_exception) throw()
{
}

#if 0
/// Effects: Allocates memory to hold a "dependent" exception to be thrown.
/// thrown_size is the size of the exception object.
/// Can allocate additional memory to hold private data.
/// If memory can not be allocated, call std::terminate().
///
/// Returns: A pointer to the memory allocated for the exception object.
void* __cxa_allocate_dependent_exception(size_t thrown_size) throw()
{

}

/// Effects: Frees memory allocated by __cxa_allocate_dependent_exception.
void __cxa_free_dependent_exception (void* dependent_exception) throw()
{

}

void __cxa_throw(void* thrown_exception,
                 std::type_info * tinfo, void (*dest)(void*))
{

}

/// Returns: The adjusted pointer to the exception object.
/// (The adjusted pointer is typically computed by the personality
/// routine during phase 1 and saved in the exception object.)
void* __cxa_get_exception_ptr(void* exceptionObject) throw()
{

}

/// Effects:
///
///  - Increment's the exception's handler count.
///  - Places the exception on the stack of currently-caught exceptions
///    if it is not already there, linking the exception to the previous
///    top of the stack.
///  - Decrements the uncaught_exception count.
///
/// If the initialization of the catch parameter is trivial (e,g., there
/// is no formal catch parameter, or the parameter has no copy constructor),
/// the calls to __cxa_get_exception_ptr() and __cxa_begin_catch() may be
/// combined into a single call to __cxa_begin_catch().
///
/// When the personality routine encounters a termination condition, it will
/// call __cxa_begin_catch() to mark the exception as handled and then call
/// terminate(), which shall not return to its caller.
///
/// Returns: The adjusted pointer to the exception object.
void* __cxa_begin_catch(void* exceptionObject) throw()
{

}

/// Effects: Locates the most recently caught exception and decrements
/// its handler count. Removes the exception from the caughtÃ“exception
/// stack, if the handler count goes to zero. Destroys the exception if
/// the handler count goes to zero, and the exception was not re-thrown
/// by throw. Collaboration between __cxa_rethrow() and __cxa_end_catch()
/// is necessary to handle the last point. Though implementation-defined,
/// one possibility is for __cxa_rethrow() to set a flag in the handlerCount
/// member of the exception header to mark an exception being rethrown.
void __cxa_end_catch()
{

}

/// Returns: the type of the currently handled exception, or null if there
/// are no caught exceptions.
std::type_info* __cxa_current_exception_type()
{

}

/// Effects: Marks the exception object on top of the caughtExceptions
/// stack (in an implementation-defined way) as being rethrown. If the
/// caughtExceptions stack is empty, it calls terminate()
/// (see [C++FDIS] [except.throw], 15.1.8). It then returns to the handler
/// that called it, which must call __cxa_end_catch(), perform any necessary
/// cleanup, and finally call _Unwind_Resume() to continue unwinding.
void __cxa_rethrow()
{

}

/// Effects: Increments the ownership count of the currently handled
/// exception (if any) by one.
/// Returns: the type of the currently handled exception, or null if there
/// are no caught exceptions.
void* __cxa_current_primary_exception() throw()
{

}

/// Effects: Decrements the ownership count of the exception by 1, and on
/// zero calls _Unwind_DeleteException with the exception object.
void __cxa_decrement_exception_refcount(void* primary_exception) throw()
{

}

/// Returns: A pointer to the __cxa_eh_globals structure for the current
/// thread, initializing it if necessary.
__cxa_eh_globals* __cxa_get_globals() throw()
{

}

/// Requires: At least one prior call to __cxa_get_globals has been made from
/// the current thread.
/// Returns: A pointer to the __cxa_eh_globals structure for
/// the current thread.
__cxa_eh_globals* __cxa_get_globals_fast() throw()
{

}

void __cxa_increment_exception_refcount(void* primary_exception) throw()
{

}

void __cxa_rethrow_primary_exception(void* primary_exception)
{

}

bool __cxa_uncaught_exception() throw()
{

}

_Unwind_Reason_Code __gxx_personality_v0 (
        int, _Unwind_Action, _Unwind_Exception_Class,
        struct _Unwind_Exception *, struct _Unwind_Context *)
{

}

int __cxa_guard_acquire(uint64_t* guard_object)
{

}

void __cxa_guard_release(uint64_t*)
{

}

void __cxa_guard_abort(uint64_t*)
{

}

void* __cxa_vec_new(size_t element_count, size_t element_size,
                    size_t padding_size, void (*constructor)(void*),
                    void (*destructor)(void*) )
{

}

void* __cxa_vec_new2(size_t element_count, size_t element_size,
                     size_t padding_size, void (*constructor)(void*),
                     void (*destructor)(void*), void* (*alloc)(size_t),
                     void (*dealloc)(void*) )
{

}

void* __cxa_vec_new3(size_t element_count, size_t element_size,
                     size_t padding_size, void (*constructor)(void*),
                     void (*destructor)(void*), void* (*alloc)(size_t),
                     void (*dealloc)(void*, size_t) )
{

}

void __cxa_vec_ctor(void* array_address, size_t element_count,
                    size_t element_size, void (*constructor)(void*),
                    void (*destructor)(void*) )
{

}

void __cxa_vec_dtor(void* array_address, size_t element_count,
                    size_t element_size, void (*destructor)(void*) )
{

}

void __cxa_vec_cleanup(void* array_address, size_t element_count,
                       size_t element_size, void (*destructor)(void*) )
{

}

void __cxa_vec_delete(void* array_address, size_t element_size,
                      size_t padding_size, void (*destructor)(void*) )
{

}

void __cxa_vec_delete2(void* array_address, size_t element_size,
                       size_t padding_size, void (*destructor)(void*),
                       void (*dealloc)(void*) )
{

}

void __cxa_vec_delete3(void* __array_address, size_t element_size,
                       size_t padding_size, void (*destructor)(void*),
                       void (*dealloc) (void*, size_t))
{

}

void __cxa_vec_cctor(void* dest_array, void* src_array,
                     size_t element_count, size_t element_size,
                     void (*constructor) (void*, void*),
                     void (*destructor)(void*) )
{

}

void (*__cxa_new_handler)();

void (*__cxa_terminate_handler)();

void (*__cxa_unexpected_handler)();

__attribute__((noreturn)) void __cxa_bad_cast()
{

}

__attribute__((noreturn)) void __cxa_bad_typeid();

void __cxa_pure_virtual(void)
{

}

__attribute__((noreturn)) void __cxa_call_unexpected (void*)
{

}

char* __cxa_demangle(const char* mangled_name, char* output_buffer,
                     size_t* length, int* status)
{

}

void* __dynamic_cast(const void* __src_ptr,
                     const __class_type_info* __src_type,
                     const __class_type_info* __dst_type,
                     ptrdiff_t __src2dst)
{

}
#endif
