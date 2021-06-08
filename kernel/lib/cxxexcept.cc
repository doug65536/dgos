#include "cxxexcept.h"
#include "exception.h"
#include "printk.h"
#include "stdlib.h"
#include "likely.h"
#include <inttypes.h>
#include "assert.h"
#include "cpu/except_asm.h"
#include "hash.h"
#include "callout.h"
#include "export.h"
#include "thread.h"
#include "string.h"
#include "mutex.h"
#include "atomic.h"
#include "uleb.h"

#define DEBUG_CXXEXCEPT 1
#if DEBUG_CXXEXCEPT
#define TRACE_CXXEXCEPT(...) printdbg("cxxexcept: " __VA_ARGS__)
#else
#define TRACE_CXXEXCEPT(...) ((void)0)
#endif

// "GNUCC++\0" backwards
static constexpr uint64_t our_exception_class = 0x474e5543432b2b00;

extern "C" void register_eh_frame();

// Register EH frames as soon as we have a working heap
static void init_eh_frames(void*)
{
    register_eh_frame();
}

REGISTER_CALLOUT(init_eh_frames, nullptr,
                 callout_type_t::heap_ready, "000");

void abort()
{
    panic("abort called");
}

__cxa_eh_globals *__cxa_get_globals(void)
{
    return thread_cxa_get_globals();
}

// Can be faster, guaranteed not to be the first call on this thread
__cxa_eh_globals *__cxa_get_globals_fast(void)
{
    return thread_cxa_get_globals();
}

// Synchronize initialization of static variables at function scope
// Returns 1 if the initialization is not yet complete, otherwise 0.
extern "C"
int __cxa_guard_acquire(uint64_t volatile *guard_object)
{
    // Fastpath "already initialized" scenario
    if (likely(*guard_object == 1))
        return 0;

    // Try to win race
    for (uint64_t expect = *guard_object; expect != 1; pause()) {
        if (atomic_cmpxchg_upd(guard_object, &expect, 0x100)) {
            // Won the race, replaced it with 0x100 and return 1 to
            // tell caller to proceed with initialization. If another
            // thread comes
            return 1;
        }
    }

    return *guard_object == 0;
}

// Synchronize initialization of static variables at function scope
extern "C"
void __cxa_guard_release(uint64_t volatile *guard_object)
{
    *guard_object = 1;
}

_noreturn
void __cxa_guard_abort(uint64_t volatile */*guard_object*/)
{
    abort();
}

static void __cxa_cleanup(_Unwind_Reason_Code reason, _Unwind_Exception *ex)
{
    assert(ex->exception_class == our_exception_class);

    void *payload = ex + 1;
    __cxa_exception *cxa = (__cxa_exception*)payload - 1;

    if (cxa->exceptionDestructor)
        cxa->exceptionDestructor(payload);

    cxa->~__cxa_exception();
    free(cxa);
}

_noreturn
static void __throw_failed(_Unwind_Exception *__thrown_exception)
{
    __cxa_begin_catch(__thrown_exception);
    printdbg("Unhandled C++ exception!\n");
    std::terminate();
}

// used by 'throw' keyword
_noreturn
void __cxa_throw(void* __thrown_exception,
                 std::type_info * __tinfo,
                 void (*__destructor)(void*))
{
    TRACE_CXXEXCEPT("__cxa_throw payload: %#zx type=%s destructor=%#zx\n",
                    uintptr_t(__thrown_exception),
                    __tinfo->name(), uintptr_t(__destructor));

    __cxa_eh_globals *thread_info = thread_cxa_get_globals();
    ++thread_info->uncaughtExceptions;

    __cxa_exception *__header = (__cxa_exception*)__thrown_exception - 1;

    __header->exceptionType = __tinfo;
    __header->exceptionDestructor = __destructor;
    __header->terminateHandler = std::terminate;
    __header->unwindHeader.exception_class = our_exception_class;
    __header->adjustedPtr = __thrown_exception;

    __header->unwindHeader.exception_cleanup = __cxa_cleanup;

    _Unwind_RaiseException(&__header->unwindHeader);
    __throw_failed(&__header->unwindHeader);
}

void *__cxa_begin_catch(void *__thrown_exception)
{
    __cxa_exception *header = (__cxa_exception*)__thrown_exception - 1;
    __cxa_eh_globals *thread_info = thread_cxa_get_globals();

    ++header->handlerCount;

    header->nextException = thread_info->caughtExceptions;
    thread_info->caughtExceptions = header;

    --thread_info->uncaughtExceptions;

    return header + 1;
}

std::type_info *__cxa_current_exception_type()
{
    __cxa_eh_globals *thread_info = thread_cxa_get_globals();
    return thread_info->caughtExceptions
            ? thread_info->caughtExceptions->exceptionType
            : nullptr;
}

void __cxa_end_catch()
{
    TRACE_CXXEXCEPT("__cxa_end_catch\n");

    __cxa_eh_globals *thread_info = thread_cxa_get_globals();
    __cxa_exception *top_exception = thread_info->caughtExceptions;
    int new_count = --top_exception->handlerCount;
    // Mask off rethrow flag
    if ((new_count & 0x3FFFFFFF) == 0) {
        thread_info->caughtExceptions = top_exception->nextException;

        if (!(new_count & 0x40000000)) {
            // Not rethrown is the common case
            __cxa_free_exception(top_exception + 1);
        } else {
            // Rethrow
            // Clear rethrow flag, it has been rethrown now
            TRACE_CXXEXCEPT("...rethrowing %#zx type=%s\n",
                            uintptr_t(top_exception + 1),
                            top_exception->exceptionType->name());
            top_exception->handlerCount &= ~0x40000000;
            //top_exception->unwindHeader.private_1 = 0;
            //top_exception->unwindHeader.private_2 = 0;
            _Unwind_Resume_or_Rethrow(&top_exception->unwindHeader);
        }
    }

    TRACE_CXXEXCEPT("...continuing after catch\n");
}

void __cxa_rethrow()
{
    __cxa_eh_globals *thread_info = thread_cxa_get_globals();
    __cxa_exception *top_exception = thread_info->caughtExceptions;
    top_exception->handlerCount |= 0x40000000;
}

__cxa_exception *__cxa_get_exception_ptr()
{
    __cxa_eh_globals *thread_info = thread_cxa_get_globals();
    return thread_info->caughtExceptions
            ? thread_info->caughtExceptions
            : nullptr;
}

void __cxa_throw_bad_array_new_length()
{
    panic("bad array new length");
}

void *__cxa_allocate_exception(size_t thrown_size)
{
    TRACE_CXXEXCEPT("allocating thrown_size=%#zx\n", thrown_size);

    char *mem = (char*)calloc(1, sizeof(__cxa_exception) + thrown_size);

    if (unlikely(!mem))
        panic_oom();

    new (mem) __cxa_exception{};

    if (mem)
        mem += sizeof(__cxa_exception);

    TRACE_CXXEXCEPT("...payload at %#zx\n", uintptr_t(mem));

    return mem;
}

void __cxa_free_exception(void *thrown_exception)
{
    TRACE_CXXEXCEPT("freeing exception at %zu\n", uintptr_t(thrown_exception));

    __cxa_exception *cxa = (__cxa_exception*)thrown_exception - 1;

    if (cxa->exceptionDestructor)
        cxa->exceptionDestructor(thrown_exception);

    cxa->~__cxa_exception();

    free(cxa);
}

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


struct tblent_t {
    uintptr_t start = 0;
    uintptr_t end = 0;
    uintptr_t code = 0;
    uintptr_t action = 0;
    uintptr_t match = 0;
    char const *type_name = nullptr;
};

_Unwind_Reason_Code __gxx_personality_v0(
        int version, _Unwind_Action actions,
        _Unwind_Exception_Class exceptionClass,
        _Unwind_Exception *unwind_exception, _Unwind_Context *context)
{
    printdbg("gcc_personality_v0 called, version: %d"
             ", exceptionClass=%" PRIx64
             ", actions: %s%s%s%s%s"
             "\n",
             version, exceptionClass,
             actions & _UA_SEARCH_PHASE  ? " search" : "",
             actions & _UA_CLEANUP_PHASE ? " cleanup" : "",
             actions & _UA_HANDLER_FRAME ? " handler" : "",
             actions & _UA_FORCE_UNWIND  ? " force-unwind" : "",
             actions & _UA_END_OF_STACK  ? " end-of-stack" : "");

    int r0 = __builtin_eh_return_data_regno(0);
    int r1 = __builtin_eh_return_data_regno(1);

    if (unlikely(version != 1)) {
        TRACE_CXXEXCEPT("...unexpected version!"
                        " returning _URC_FATAL_PHASE1_ERROR\n");
        return _URC_FATAL_PHASE1_ERROR;
    }

    if (unlikely(!unwind_exception)) {
        TRACE_CXXEXCEPT("...no unwind_exception!"
                        " returning _URC_FATAL_PHASE1_ERROR\n");
        return _URC_FATAL_PHASE1_ERROR;
    }

    if (unlikely(!context)) {
        TRACE_CXXEXCEPT("...no context!"
                        " returning _URC_FATAL_PHASE1_ERROR\n");
        return _URC_FATAL_PHASE1_ERROR;
    }

    if (unlikely(unwind_exception->exception_class != our_exception_class)) {
        TRACE_CXXEXCEPT("...caught foreign exception\n");
        return _URC_FOREIGN_EXCEPTION_CAUGHT;
    }

    __cxa_exception *cxx_ex = (__cxa_exception*)(unwind_exception + 1) - 1;

    int ip_before_insn = 0;
    uintptr_t throw_ip = _Unwind_GetIPInfo(context, &ip_before_insn);

    throw_ip -= (ip_before_insn == 0);

    uintptr_t func_start = _Unwind_GetRegionStart(context);

    // Get a pointer to the the LSDA
    LSDA_ptr raw_lsda = (LSDA_ptr)_Unwind_GetLanguageSpecificData(context);

    if (unlikely(!raw_lsda)) {
        TRACE_CXXEXCEPT("...returning _URC_CONTINUE_UNWIND because"
                        " no language specific data area is present\n");
        return _URC_CONTINUE_UNWIND;
    }

    // Header
    uint8_t landingpad_start_enc = *raw_lsda++;

    uintptr_t landingpad_start = func_start;

    if (unlikely(landingpad_start_enc != DW_EH_PE_omit))
        landingpad_start = read_enc_val(raw_lsda, landingpad_start_enc);

    uint8_t type_enc = *raw_lsda++;

    uintptr_t type_info_table_offset = 0;

    void *type_info_table = nullptr;

    if (likely(type_enc != DW_EH_PE_omit)) {
        type_info_table_offset = read_enc_val(raw_lsda, DW_EH_PE_uleb128);
        type_info_table = (void*)(raw_lsda + type_info_table_offset);
    }

    uint8_t lsda_enc = *raw_lsda++;
    uintptr_t table_len =  decode_uleb128(raw_lsda);
    uint8_t const *table = raw_lsda;
    uint8_t const *table_en = table + table_len;

    // Type table begins after header
    LSDA_ptr action_table = table_en;

    TRACE_CXXEXCEPT("...throw_ip: %#zx"
                    " lpstart: %#zx"
                    " tbl: %#zx-%#zx"
                    "\n",
                    throw_ip, landingpad_start,
                    uintptr_t(table), uintptr_t(table_en));

    TRACE_CXXEXCEPT("...regions:\n");



    tblent_t match_ent;

    while (table < table_en) {
        uintptr_t start_ofs = read_enc_val(table, lsda_enc);
        uintptr_t len = read_enc_val(table, lsda_enc);
        uintptr_t lp = read_enc_val(table, lsda_enc);
        uintptr_t action =  read_enc_val(table, DW_EH_PE_uleb128);

        uintptr_t start = landingpad_start + start_ofs;
        uintptr_t end = start + len;
        uintptr_t landingpad = lp ? landingpad_start + lp : 0;

        bool is_match = (throw_ip >= start && throw_ip <= end);
        char const *match = is_match ? " (**match**)" : "";

        TRACE_CXXEXCEPT("...region: %#zx - %#zx landingpad: %#zx"
                        " action: %#zx %s\n",
                        start, end, landingpad, action, match);

        int type_index = -1;

        if (action) {
            type_index = action_table[action - 1];
            TRACE_CXXEXCEPT("...type index %#x\n", type_index);
        }

        char const *type_name = nullptr;

        if (is_match && type_index > 0) {
            uintptr_t tiaddr = 0;

            switch (type_enc & ~DW_EH_PE_indirect) {
            case DW_EH_PE_sdata4 | DW_EH_PE_pcrel:
                int32_t *rel32;
                rel32 = (int32_t*)type_info_table - type_index;
                tiaddr = uintptr_t(rel32) + *rel32;

                if (likely(type_enc & DW_EH_PE_indirect))
                    tiaddr = *(uintptr_t*)tiaddr;

                break;

            default:
                panic("Unhandled type table type: %#x", type_enc);
            }

            std::type_info const *catch_type = (std::type_info const *)tiaddr;

            type_name = catch_type->name();

            if (cxx_ex->exceptionType != catch_type) {
                TRACE_CXXEXCEPT("...ignored handler for wrong type \"%s\"\n",
                                catch_type->name());
                continue;
            }

            TRACE_CXXEXCEPT("...found handler that handles"
                            " exception with type %s\n", catch_type->name());
        }

        if (is_match) {
            match_ent = {start, end, landingpad, action, is_match, type_name};
            //break;
        }
    }

    if ((actions & _UA_HANDLER_FRAME) && cxx_ex->catchTemp) {
        TRACE_CXXEXCEPT("...handler phase running landing pad"
                        "IP: %#zx R0: %#zx, R1: %#zx\n",
                        uintptr_t(cxx_ex->catchTemp),
                        uintptr_t(cxx_ex->adjustedPtr), uintptr_t(1));
        _Unwind_SetGR(context, r0, uintptr_t(cxx_ex->adjustedPtr));
        _Unwind_SetGR(context, r1, 1);
        _Unwind_SetIP(context, uintptr_t(cxx_ex->catchTemp));
        cxx_ex->catchTemp = nullptr;
        return _URC_INSTALL_CONTEXT;
    }

    if ((actions & _UA_CLEANUP_PHASE) && match_ent.code) {
        TRACE_CXXEXCEPT("...handler phase installing context to"
                        " run landing pad at %#zx\n", match_ent.code);
        _Unwind_SetGR(context, r0, uintptr_t(unwind_exception));
        _Unwind_SetGR(context, r1, 0);
        _Unwind_SetIP(context, match_ent.code);
        return _URC_INSTALL_CONTEXT;
    }

    if ((actions & _UA_SEARCH_PHASE) && match_ent.action) {
        TRACE_CXXEXCEPT("...returning _URC_HANDLER_FOUND because"
                        " actions is search phase\n\n");

        uintptr_t r0_value, r1_value;

        if (match_ent.action) {
            r0_value = uintptr_t(unwind_exception + 1);
            r1_value = 1;
        } else {
            r0_value = uintptr_t(unwind_exception);
            r1_value = 0;
        }

        TRACE_CXXEXCEPT("...remembering context with"
                        " ip: %#zx r0: %#zx r1: %#zx\n\n",
                        match_ent.code, r0_value, r1_value);

        cxx_ex->adjustedPtr = (void*)r0_value;
        cxx_ex->handlerSwitchValue = r1_value;
        cxx_ex->catchTemp = (void*)match_ent.code;

        return _URC_HANDLER_FOUND;
    }

    if ((actions & _UA_CLEANUP_PHASE) && !match_ent.code) {
        TRACE_CXXEXCEPT("...cleanup phase continuing unwind"
                        " because match has 0 code address\n");
        return _URC_CONTINUE_UNWIND;
    }

    TRACE_CXXEXCEPT("...continuing unwind\n\n");

    return _URC_CONTINUE_UNWIND;
}

extern "C" int __exception_stop_fn(
        int, _Unwind_Action, _Unwind_Exception_Class,
        struct _Unwind_Exception *thrown_exception,
        struct _Unwind_Context *ctx, void *arg)
{
    __exception_jmp_buf_t *jmpbuf = (__exception_jmp_buf_t*)arg;
    _Unwind_Ptr cfa = _Unwind_GetCFA(ctx);

    if (cfa < (_Unwind_Ptr)jmpbuf->sp)
        return _URC_NO_REASON;

    // Found it
    _Unwind_DeleteException(thrown_exception);

    __longjmp(jmpbuf, 1);
}

std::type_info::~type_info()
{
}

bool std::type_info::operator==(const std::type_info &__rhs) const
{
    return __type_name == __rhs.__type_name;
}

bool std::type_info::operator!=(const std::type_info &__rhs) const
{
    return __type_name != __rhs.__type_name;
}

bool std::type_info::before(const std::type_info &__rhs) const
{
    return __type_name < __rhs.__type_name;
}

char const *std::type_info::name() const
{
    return __type_name;
}

__cxxabiv1::__class_type_info::~__class_type_info()
{
}

__cxxabiv1::__si_class_type_info::~__si_class_type_info()
{
}

__cxxabiv1::__vmi_class_type_info::~__vmi_class_type_info()
{
}

