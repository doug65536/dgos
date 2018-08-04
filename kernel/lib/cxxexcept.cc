#include "cxxexcept.h"
#include "printk.h"
#include "stdlib.h"
#include "likely.h"
#include <inttypes.h>

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
    return malloc(thrown_size);
}


void __cxa_free_exception(void *thrown_exception)
{
    free(thrown_exception);
}

// Begin language specific nonsense

typedef uint8_t const *LSDA_ptr;

struct LSDA_Header {
    /**
     * Read the LSDA table into a struct; advances the lsda pointer
     * as many bytes as read
     */
    LSDA_Header(LSDA_ptr *lsda) {
        LSDA_ptr read_ptr = *lsda;

        // Copy the LSDA fields
        start_encoding = read_ptr[0];
        type_encoding = read_ptr[1];
        ttype = read_ptr[2];

        // Advance the lsda pointer
        *lsda = read_ptr + sizeof(LSDA_Header);
    }

    uint8_t start_encoding;
    uint8_t type_encoding;
    uint8_t ttype;
};

struct LSDA_CS_Header {
    // Same as other LSDA constructors
    LSDA_CS_Header(LSDA_ptr *lsda) {
        LSDA_ptr read_ptr = *lsda;
        encoding = read_ptr[0];
        length = read_ptr[1];
        *lsda = read_ptr + sizeof(LSDA_CS_Header);
    }

    uint8_t encoding;
    uint8_t length;
};

struct LSDA_CS {
    // Same as other LSDA constructors
    LSDA_CS(LSDA_ptr *lsda) {
        LSDA_ptr read_ptr = *lsda;
        start = read_ptr[0];
        len = read_ptr[1];
        lp = read_ptr[2];
        action = read_ptr[3];
        *lsda = read_ptr + sizeof(LSDA_CS);
    }

    // Note start, len and lp would be void*'s, but they are actually relative
    // addresses: start and lp are relative to the start of the function, len
    // is relative to start

    // Offset into function from which we could handle a throw
    uint8_t start;
    // Length of the block that might throw
    uint8_t len;
    // Landing pad
    uint8_t lp;
    // Offset into action table + 1 (0 means no action)
    // Used to run destructors
    uint8_t action;
};

// End language specific nonsense

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

    if (unlikely(version != 1))
        return _URC_FATAL_PHASE1_ERROR;

    if (actions & (_UA_CLEANUP_PHASE)) {// | _UA_HANDLER_FRAME)) {
        // Calculate what the instruction pointer was just before the
        // exception was thrown for this stack frame
        uintptr_t throw_ip = _Unwind_GetIP(context) - 1;

        // Pointer to the beginning of the raw LSDA
        LSDA_ptr lsda = (uint8_t*)_Unwind_GetLanguageSpecificData(context);

        // Read LSDA headerfor the LSDA
        LSDA_Header header(&lsda);

        // Read the LSDA CS header
        LSDA_CS_Header cs_header(&lsda);

        // Calculate where the end of the LSDA CS table is
        const LSDA_ptr lsda_cs_table_end = lsda + cs_header.length;

        // Loop through each entry in the CS table
        while (lsda < lsda_cs_table_end)
        {
            LSDA_CS cs(&lsda);

            // If there's no LP we can't handle this exception; move on
            if (not cs.lp) continue;

            uintptr_t func_start = _Unwind_GetRegionStart(context);

            // Calculate the range of the instruction pointer valid for this
            // landing pad; if this LP can handle the current exception then
            // the IP for this stack frame must be in this range
            uintptr_t try_start = func_start + cs.start;
            uintptr_t try_end = func_start + cs.start + cs.len;

            // Check if this is the correct LP for the current try block
            if (throw_ip < try_start) continue;
            if (throw_ip > try_end) continue;

            // We found a landing pad for this exception; resume execution
            int r0 = __builtin_eh_return_data_regno(0);
            int r1 = __builtin_eh_return_data_regno(1);

            _Unwind_SetGR(context, r0, (uintptr_t)(unwind_exception));
            // Note the following code hardcodes the exception type;
            // we'll fix that later on
            _Unwind_SetGR(context, r1, (uintptr_t)(1));

            _Unwind_SetIP(context, func_start + cs.lp);
            break;
        }
        return _URC_INSTALL_CONTEXT;
//        _Unwind_SetGR(context, __builtin_eh_return_data_regno(0),
//                      (_Unwind_Ptr) &xh->unwindHeader);
//        _Unwind_SetGR(context, __builtin_eh_return_data_regno(1),
//                      handler_switch_value);
//        _Unwind_SetIP(context, landing_pad);
        return _URC_INSTALL_CONTEXT;
    }

    //if (actions & _UA_CLEANUP_PHASE)
    //    return _URC_INSTALL_CONTEXT;

//    if (actions & _UA_SEARCH_PHASE)
//    {
//        printdbg("Personality function, lookup phase\n");
//        return _URC_HANDLER_FOUND;
//    } else if (actions & _UA_CLEANUP_PHASE) {
//        printdbg("Personality function, cleanup\n");
//        return _URC_INSTALL_CONTEXT;
//    } else {
//        printdbg("Personality function, error\n");
//        return _URC_FATAL_PHASE1_ERROR;
//    }

    return _URC_CONTINUE_UNWIND;
}
