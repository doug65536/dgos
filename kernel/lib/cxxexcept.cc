#include "cxxexcept.h"
#include "printk.h"
#include "stdlib.h"
#include "likely.h"
#include <inttypes.h>
#include "assert.h"
#include "cpu/except_asm.h"
#include "hash.h"
#include "callout.h"
#include "export.h"

extern "C" void register_eh_frame();


// Register EH frames as soon as we have a working heap
void init_eh_frames(void*)
{
    register_eh_frame();
}
REGISTER_CALLOUT(init_eh_frames, nullptr, callout_type_t::heap_ready, "000");

void abort()
{
    panic("abort called");
}

void __cxa_throw_bad_array_new_length()
{
    panic("bad array new length");
}

void *__cxa_allocate_exception(size_t thrown_size)
{
    return calloc(1, thrown_size);
}

void __cxa_free_exception(void *thrown_exception)
{
    free(thrown_exception);
}

//namespace __cxxabiv1 {
//    class EXPORT __class_type_info {
//        EXPORT virtual ~__class_type_info() {}
//    };
//}

__BEGIN_NAMESPACE_STD
class type_info
{
    virtual ~type_info();

    char const *name() const noexcept
    {
        return __name;
    }

    bool before(type_info const& rhs)
    {
        return __name < rhs.__name;
    }

    bool operator==(type_info const& rhs)
    {
        return __builtin_strcmp(__name, rhs.__name) == 0;
    }

    bool operator!=(type_info const& rhs)
    {
        return __builtin_strcmp(__name, rhs.__name) != 0;
    }

    size_t hash_code() const
    {
        return hash_32(__name, __builtin_strlen(__name));
    }

    virtual bool __is_pointer_p() const = 0;
    virtual bool __is_function_p() const = 0;
    virtual bool __do_catch(const type_info *__thr_type, void **__thr_obj,
                            unsigned __outer) const;

    // Internally used during catch matching
    virtual bool __do_upcast(/*const __cxxabiv1::__class_type_info*/
                             void *__target,
                             void **__obj_ptr) const;

protected:
    char const *__name;
    explicit type_info(char const *__name) : __name(__name) {}

private:
    type_info &operator=(type_info const&) = delete;
    type_info(type_info const&) = delete;
};
__END_NAMESPACE_STD

typedef void (*unexpected_handler)(void);
typedef void (*terminate_handler)(void);

struct __cxa_exception {
	std::type_info *exceptionType;
	void (*exceptionDestructor)(void *);
	unexpected_handler unexpectedHandler;
	terminate_handler terminateHandler;
	__cxa_exception *nextException;

	int handlerCount;
	int handlerSwitchValue;
	const char *actionRecord;
	const char *languageSpecificData;
	void *catchTemp;
	void *adjustedPtr;

	_Unwind_Exception unwindHeader;
};

typedef uint8_t const *LSDA_ptr;

#if 0
class throw_me{};

void test_throw()
{
    try {
        throw throw_me();
    } catch (int) {
        printdbg("no way!\n");
    }
}
#endif

uintptr_t decode_uleb128(uint8_t const *&in)
{
    uintptr_t result = 0;
    uint8_t shift = 0;
    for (;;) {
        uint8_t byte = *in++;
        result |= uintptr_t(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0)
            break;
        shift += 7;
    }
    return result;
}

#if 0   // not used
intptr_t decode_sleb128(uint8_t const *&in)
{
    intptr_t result = 0;
    uint8_t shift = 0;
    for (;;) {
        uint8_t byte = *in++;
        result |= uintptr_t(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0 && (byte & 0x40)) {
            if (shift < sizeof(intptr_t)*8)
                result |= ~uintptr_t(0) << shift;
            break;
        }
        shift += 7;
    }
    return result;
}
#endif

_Unwind_Reason_Code __gxx_personality_v0(
        int version, _Unwind_Action actions,
        _Unwind_Exception_Class exceptionClass,
        _Unwind_Exception *unwind_exception, _Unwind_Context *context)
{
//    printdbg("gcc_personality_v0 called, version: %d"
//             ", exceptionClass=%" PRIx64
//             ", actions: %s%s%s%s%s"
//             "\n",
//             version, exceptionClass,
//             actions & _UA_SEARCH_PHASE  ? " search" : "",
//             actions & _UA_CLEANUP_PHASE ? " cleanup" : "",
//             actions & _UA_HANDLER_FRAME ? " handler" : "",
//             actions & _UA_FORCE_UNWIND  ? " force-unwind" : "",
//             actions & _UA_END_OF_STACK  ? " end-of-stack" : "");

    if (unlikely(version != 1))
        return _URC_FATAL_PHASE1_ERROR;

    uintptr_t throw_ip = _Unwind_GetIP(context) - 1;

    // Get a pointer to the raw memory address of the LSDA
    LSDA_ptr raw_lsda = (LSDA_ptr)_Unwind_GetLanguageSpecificData(context);

    // Header
    uint8_t start_encoding = *raw_lsda++;
    uint8_t type_encoding = *raw_lsda++;

    // offset from the end of the header to the types table
    uintptr_t type_table_offset = decode_uleb128(raw_lsda);

    // Type table begins after header
    LSDA_ptr type_table = raw_lsda;

    uintptr_t *action_table = (uintptr_t*)(type_table + type_table_offset);
    (void)action_table;

    assert(start_encoding == 0xFF);
    assert(type_encoding == 0xFF);
    assert(type_table_offset == 1);

    uintptr_t table_len = decode_uleb128(raw_lsda);
    uint8_t const *table = raw_lsda;
    uint8_t const *table_en = table + table_len;

    uintptr_t func_start = _Unwind_GetRegionStart(context);

    while (table < table_en) {
        uintptr_t start_ofs = decode_uleb128(table);
        uintptr_t len = decode_uleb128(table);
        uintptr_t lp = decode_uleb128(table);
        uintptr_t action = decode_uleb128(table);

        // If there's no landing pad, nothing to do
        if (!lp)
            continue;

        // See if the throw instruction pointer is handled by this landing pad

        uintptr_t start = func_start + start_ofs;
        uintptr_t end = start + len;

        if (throw_ip < start || throw_ip >= end)
            continue;

        if (action != 0) {
            // Ignore catch blocks
            //void **type_table_ent = (void**)action_table[-action];
            continue;
        }

        if (actions & _UA_SEARCH_PHASE)
            return _URC_HANDLER_FOUND;

        // Execute the landing pad

        int r0 = __builtin_eh_return_data_regno(0);
        int r1 = __builtin_eh_return_data_regno(1);

        _Unwind_SetGR(context, r0, uintptr_t(unwind_exception));
        _Unwind_SetGR(context, r1, 1);

        _Unwind_SetIP(context, func_start + lp);

        return _URC_INSTALL_CONTEXT;
    }

    return _URC_CONTINUE_UNWIND;
}

extern "C" int __exception_stop_fn(
        int, _Unwind_Action, _Unwind_Exception_Class,
        struct _Unwind_Exception *thrown_exception,
        struct _Unwind_Context *ctx, void *arg)
{
    __exception_jmp_buf_t *jmpbuf = (__exception_jmp_buf_t*)arg;
    _Unwind_Ptr cfa = _Unwind_GetCFA(ctx);

    if (cfa < (_Unwind_Ptr)jmpbuf->rsp)
        return _URC_NO_REASON;

    // Found it
    __cxa_free_exception(thrown_exception);

    __exception_longjmp(jmpbuf, 1);
}
