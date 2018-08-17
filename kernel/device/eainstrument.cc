#include "eainstrument.h"
#include "arch/x86_64/cpu/asm_constants.h"
#include "arch/x86_64/cpu/control_regs.h"

// Using hacky hardcoded stubs to avoid _no_instrument proliferation

int volatile eainst_lock;

_no_instrument
static uintptr_t eainst_lock_acquire()
{
    uintptr_t flags;
    __asm__ __volatile__ (
        "pushfq\n\t"
        "popq %[flags]\n\t"
        "cli\n\t"
        : [flags] "=r" (flags)
    );
    while (__atomic_load_n(&eainst_lock, __ATOMIC_ACQUIRE) != 0 ||
           __sync_val_compare_and_swap(&eainst_lock, 0, 1) != 0)
        __builtin_ia32_pause();
    return flags;
}

_no_instrument
static void eainst_lock_release(uintptr_t flags)
{
    __atomic_store_n(&eainst_lock, 0, __ATOMIC_RELEASE);
    __asm__ __volatile__ (
        "pushq %[flags]\n\t"
        "popfq\n\t"
        :
        : [flags] "r" (flags)
    );
}

// Return thread id in low 32 bits, apicid in high 32 bits
_no_instrument
static uintptr_t eainst_get_current_thread()
{
    intptr_t ids;
    __asm__ __volatile__ (
        // RDTSCP puts IA32_TSC_AUX MSR (address C0000103H) into ECX
        "rdtscp\n\t"
        "shlq $32,%%rcx\n\t"
        // Get GSBASE into RAX, to handle null GS (and give 0 TID if too early)
        "rdgsbase %q[ids]\n\t"
        "testq %q[ids],%q[ids]\n\t"
        "jz 0f\n\t"
        "movq %c[cur_thread_ofs](%[ids]),%q[ids]\n\t"
        "testq %q[ids],%q[ids]\n\t"
        "jz 0f\n\t"
        "movl %c[thread_id_ofs](%[ids]),%k[ids]\n\t"
        "0:\n\t"
        "orq %%rcx,%[ids]\n\t"
        : [ids] "=a" (ids)
        : [cur_thread_ofs] "n" (CPU_INFO_CURTHREAD_OFS)
        , [thread_id_ofs] "n" (THREAD_THREAD_ID_OFS)
        : "memory", "rdx", "rcx"
    );
    return ids;
}

_no_instrument
static void eainst_write_record(trace_item const& item)
{
    void const *p = &item;
    size_t byte_count = sizeof(item);
    __asm__ __volatile__ (
        "rep outsb\n\t"
        : "+S" (p)
        , "+c" (byte_count)
        : "d" (0xEA)
        : "memory"
    );
}

void __cyg_profile_func_enter(void *this_fn, void * /*call_site*/)
{
    uintptr_t tid = eainst_get_current_thread();
    uintptr_t flags = eainst_lock_acquire();
    trace_item item;
    item.set_ip(this_fn);
    item.set_tid(tid & 0xFFFFFFFF);
    item.set_cid(tid >> 32);
    item.irq_en = flags & CPU_EFLAGS_IF;
    item.call = true;
    eainst_write_record(item);
    eainst_lock_release(flags);
}

void __cyg_profile_func_exit(void *this_fn, void * /*call_site*/)
{
    uintptr_t tid = eainst_get_current_thread();
    uintptr_t flags = eainst_lock_acquire();
    trace_item item;
    item.set_ip(this_fn);
    item.set_tid(tid & 0xFFFFFFFF);
    item.set_cid(tid >> 32);
    item.irq_en = flags & CPU_EFLAGS_IF;
    item.call = false;
    eainst_write_record(item);
    eainst_lock_release(flags);
}
