#include "pxemain.h"
#include "boottable.h"
#include "screen.h"
#include "assert.h"
#include "string.h"
#include "cpu.h"
#include "gdt_sel_pxe.h"
#include "pxestruct.h"
#include "fs.h"
#include "malloc.h"
#include "physmem.h"
#include "elf64.h"
#include "../kernel/lib/bswap.h"
#include "halt.h"

// iPXE is loading a real mode segment and crashing when doing PM call
#define PXE_USE_PROTECTED_MODE  0

#define DEBUG_TFTP 0
#if DEBUG_TFTP
#define PXE_TFTP_TRACE(...) PRINT("tftp: " __VA_ARGS__)
#else
#define PXE_TFTP_TRACE(...) ((void)0)
#endif

uint8_t pxe_server_ip[4];

uint16_t pxe_entry_vec[2];
uint16_t bang_pxe_ptr[2];

uint16_t (*pxe_call)(uint16_t op, pxenv_base_t *arg_struct);

class pxe_fragment_t {
    size_t offset;
    size_t length;
    void *data;
};

static bool pxe_api_op_invoke(uint16_t opcode, pxenv_base_t *op)
{
    uint16_t status = pxe_call(opcode, op);
    return status == PXENV_EXIT_SUCCESS && op->status == PXENV_STATUS_SUCCESS;
}

template<typename T>
static _always_inline bool pxe_api_op_run(T *op)
{
    return pxe_api_op_invoke(pxenv_opcode_t<T>::opcode, op);
}

// Returns negotiated block size on success, -1 on error
static int pxe_api_tftp_open(char const *filename, uint16_t packet_size)
{
    PXE_TFTP_TRACE("opening %s with packet size %u", filename, packet_size);

    pxenv_tftp_open_t op{};

    strncpy((char*)op.filename, filename, sizeof(op.filename));
    op.filename[countof(op.filename)-1] = 0;

    C_ASSERT(sizeof(op.server_ip) == sizeof(pxe_server_ip));
    memcpy(op.server_ip, pxe_server_ip, sizeof(op.server_ip));

    // FIXME
    op.tftp_port = htons(69);

    op.packet_size = packet_size;

    if (likely(pxe_api_op_run(&op))) {
        PXE_TFTP_TRACE("open succeeded");
        return op.packet_size;
    }

    PXE_TFTP_TRACE("open failed, status=0x%x", op.status);

    return -1;
}

static bool pxe_api_tftp_close()
{
    PXE_TFTP_TRACE("closing transfer");

    pxenv_tftp_close_t op{};

    bool result = pxe_api_op_run(&op);

    PXE_TFTP_TRACE("closed, status=0x%x", op.status);

    return result;
}

// Returns amount of data received, or -1 on error
static int pxe_api_tftp_read(void *buffer, uint16_t sequence, uint16_t size)
{
    PXE_TFTP_TRACE("reading sequence=%u, size=%u, buffer=0x%p",
                   sequence, size, buffer);

    pxenv_tftp_read_t op{};

    op.buffer_ofs = uintptr_t(buffer) & 0xF;
    op.buffer_seg = uintptr_t(buffer) >> 4;
    op.sequence = sequence;
    op.buffer_size = size;

    if (likely(pxe_api_op_run(&op))) {
        PXE_TFTP_TRACE("read succeeded, got %u", op.buffer_size);
        return op.buffer_size;
    }

    PXE_TFTP_TRACE("read failed, status=0x%x", op.status);
    return -1;
}

#if __SIZEOF_POINTER__ == 8
static constexpr size_t log2_radix_slots = 9;
#elif __SIZEOF_POINTER__ == 4
static constexpr size_t log2_radix_slots = 10;
#else
#error Unexpected pointer size
#endif
static constexpr size_t radix_slots = size_t(1) << log2_radix_slots;

class pxe_openfile_t;
#define MAX_OPEN_FILES  48
static pxe_openfile_t *pxe_open_files;
static int pxe_current_file = -1;

static void *bounce_buf;
static int bounce_buf_sz;

void pxe_end_current()
{
    if (pxe_current_file >= 0) {
        pxe_api_tftp_close();
        free(bounce_buf);
        bounce_buf = nullptr;
        bounce_buf_sz = 0;
    }
    pxe_current_file = -1;
}

class pxe_openfile_t {
public:
    char *filename;

    // Points to a single page. 3-level radix tree.
    // Works the same as 3-level PAE x86 paging structure without the flag bits
    // At each level of indirection, the pointer points to a page of 512 or
    // 1024 entries for progressively less significant groups of 9 or 10 bits
    // of the key (64 bit or 32 bit pointers, respectively)
    // Handles key range from 0 to 512GB-1
    void ****radix_tree;

    // Size field is not updated until end of file is reached
    int64_t size;
    size_t received;
    int block_size;

    bool set_filename(char const *new_filename)
    {
        char *copy = strdup(new_filename);
        if (!copy)
            return false;
        if (filename)
            free(filename);
        filename = copy;
        return true;
    }

    template<typename T>
    void *radix_commit(T &p) {
        p = (T)alloc_phys(PAGE_SIZE).base;
        if (!p)
            return nullptr;
        return memset(p, 0, sizeof(void*) * radix_slots);
    }

    void *radix_lookup(uint64_t addr, bool commit)
    {
        unsigned misalignment = addr & PAGE_MASK;
        addr -= misalignment;

        size_t ai = addr >> (PAGE_SIZE_BIT + (log2_radix_slots * 0));
        size_t bi = addr >> (PAGE_SIZE_BIT + (log2_radix_slots * 1));
        size_t ci = addr >> (PAGE_SIZE_BIT + (log2_radix_slots * 2));
        ai &= (radix_slots - 1);
        bi &= (radix_slots - 1);
        ci &= (radix_slots - 1);

        phys_alloc_t alloc;

        if (!commit) {
            if (unlikely(!radix_tree ||
                         !radix_tree[ci] ||
                         !radix_tree[ci][bi] ||
                         !radix_tree[ci][bi][ai]))
                return nullptr;
            return (char*)radix_tree[ci][bi][ai] + misalignment;
        }

        if (!radix_tree) {
            alloc = alloc_phys(PAGE_SIZE);
            if (unlikely(!alloc.size))
                return nullptr;
            radix_tree = (void****)alloc.base;
            if (unlikely(!radix_tree))
                return nullptr;
            memset(radix_tree, 0, sizeof(*radix_tree) * radix_slots);
        }

        void ***&level1 = radix_tree[ci];

        if (unlikely(!level1)) {
            if (unlikely(!radix_commit(level1)))
                return nullptr;
        }

        void **&level2 = level1[bi];

        if (unlikely(!level2)) {
            if (unlikely(!radix_commit(level2)))
                return nullptr;
        }

        void *&level3 = level2[ai];

        if (!level3) {
            if (unlikely(!radix_commit(level3)))
                return nullptr;
        }

        return (char*)level3 + misalignment;
    }

    bool open(char const *filename)
    {
        if (!set_filename(filename))
            return false;

        pxe_end_current();

        block_size = pxe_api_tftp_open(filename, PAGE_SIZE);
        if (unlikely(block_size < 0))
            return false;

        // Don't know size until we get a packet smaller than the block size
        size = -1;

        return block_size > 0;
    }

    int close()
    {
        free(filename);
        filename = nullptr;
        return 0;
    }

    int pread(void *buf, size_t bytes, off_t ofs)
    {
        int64_t end = ofs + bytes;

        if (!read_until(end))
            return -1;

        int total = 0;

        while (bytes > 0) {
            void *cache = radix_lookup(ofs, false);

            // Should never happen
            if (unlikely(!cache))
                return -1;

            uintptr_t page_end = (uintptr_t(cache) + PAGE_SIZE) & -PAGE_SIZE;
            size_t xfer = page_end - uintptr_t(cache);

            if (unlikely(xfer > bytes))
                xfer = bytes;

            memcpy(buf, cache, xfer);

            buf = (char*)buf + xfer;
            bytes -= xfer;
            ofs += xfer;
            total += xfer;
        }

        return total;
    }

    bool read_until(int64_t offset)
    {
        if (unlikely(size >= 0 && offset > size))
            offset = size;

        while (received < offset) {
            if (unlikely(!bounce_buf)) {
                bounce_buf = malloc(block_size);
                if (!bounce_buf)
                    return false;
                bounce_buf_sz = block_size;
            }

            int got = pxe_api_tftp_read(bounce_buf, received / block_size + 1,
                                        block_size);
            if (got < 0)
                return false;

            // Transfer bounce buffer into cache pages
            // without crossing page boundaries
            int64_t cached = 0;
            do {
                void *buffer = radix_lookup(received + cached, true);
                if (unlikely(!buffer))
                    return false;
                uintptr_t page_end = (uintptr_t(buffer) +
                                      PAGE_SIZE) & -PAGE_SIZE;
                size_t xfer = page_end - uintptr_t(buffer);
                if (cached + xfer > got)
                    xfer = got - cached;

                memcpy(buffer, (char*)bounce_buf + cached, xfer);

                cached += xfer;
            } while (cached < got);

            received += got;

            if (got < block_size) {
                size = received;
                break;
            }
        }

        return true;
    }

    static pxe_openfile_t *lookup_file(int file)
    {
        if (likely(pxe_open_files &&
                     file >= 0 &&
                     file < MAX_OPEN_FILES &&
                     pxe_open_files[file].filename))
            return pxe_open_files + file;

        return nullptr;
    }

    static int alloc_file()
    {
        if (unlikely(!pxe_open_files))
            pxe_open_files = (pxe_openfile_t*)
                    calloc(MAX_OPEN_FILES, sizeof(*pxe_open_files));

        if (unlikely(!pxe_open_files))
            return -1;

        int file;
        for (file = 0; file < MAX_OPEN_FILES; ++file) {
            if (!pxe_open_files[file].filename)
                break;
        }

        return file < MAX_OPEN_FILES ? file : -1;
    }
};

static int pxe_boot_open(tchar const *filename)
{
    int file = pxe_openfile_t::alloc_file();

    pxe_openfile_t &desc = pxe_open_files[file];

    if (file < 0)
        return file;

    if (!desc.open(filename)) {
        if (pxe_current_file == file)
            pxe_end_current();

        desc.close();
        return -1;
    }

    return file;
}

static int pxe_boot_close(int file)
{
    if (unlikely(!pxe_open_files || file < 0 || file >= MAX_OPEN_FILES))
        return -1;

    pxe_openfile_t &desc = pxe_open_files[file];

    if (file == pxe_current_file)
        pxe_end_current();

    return desc.close();
}

static int pxe_boot_pread(int file, void *buf, size_t bytes, off_t ofs)
{
    pxe_openfile_t *desc = pxe_openfile_t::lookup_file(file);

    if (desc)
        return desc->pread(buf, bytes, ofs);

    return -1;
}

static uint64_t pxe_boot_drv_serial()
{
    return 0;
}

static void pxe_init_fs()
{
    pxenv_get_cached_info_t gci{};
    char buf[1500];

    gci.buffer_ofs = uintptr_t(buf) & 0xF;
    gci.buffer_seg = uintptr_t(buf) >> 4;
    gci.buffer_size = sizeof(buf);

    uint16_t result;

    memset(buf, 0, sizeof(buf));
    gci.buffer_size = sizeof(buf);
    gci.packet_type = PXENV_PACKET_TYPE_DHCP_CACHED_REPLY;
    result = pxe_call(PXENV_GET_CACHED_INFO, &gci);

    if (result != PXENV_EXIT_SUCCESS)
        PANIC("Failed to get cached network info!");

    memcpy(pxe_server_ip, buf + 20, sizeof(pxe_server_ip));

    fs_api.boot_open = pxe_boot_open;
    fs_api.boot_pread = pxe_boot_pread;
    fs_api.boot_close = pxe_boot_close;
    fs_api.boot_drv_serial = pxe_boot_drv_serial;
}

static bool pxe_set_api(bangpxe_t *bp)
{
    if (memcmp(bp->sig, "!PXE", 4) ||
            boottbl_checksum((char const *)bp, bp->StructLength))
        return false;

#if PXE_USE_PROTECTED_MODE
    // Initialize 16-bit protected mode GDT entries
    gdt[GDT_SEL_PXE_STACK >> 3].set_base(bp->Stack.ofs)
            .set_limit(bp->Stack.size-1)
            .set_access(true, 0, false, false, true)
            .set_flags(false, false);
    gdt[GDT_SEL_PXE_UD >> 3].set_base(bp->UNDIData.ofs)
            .set_limit(bp->UNDIData.size-1)
            .set_access(true, 0, false, false, true)
            .set_flags(false, false);
    gdt[GDT_SEL_PXE_UC >> 3].set_base(bp->UNDICode.ofs)
            .set_limit(bp->UNDICode.size-1)
            .set_access(true, 0, true, false, false)
            .set_flags(false, false);
    gdt[GDT_SEL_PXE_UCW >> 3].set_base(bp->UNDICodeWrite.ofs)
            .set_limit(bp->UNDICodeWrite.size-1)
            .set_access(true, 0, false, false, true)
            .set_flags(false, false);
    gdt[GDT_SEL_PXE_BD >> 3].set_base(bp->BC_Data.ofs)
            .set_limit(bp->BC_Data.size-1)
            .set_access(true, 0, false, false, true)
            .set_flags(false, false);
    gdt[GDT_SEL_PXE_BC >> 3].set_base(bp->BC_Code.ofs)
            .set_limit(bp->BC_Code.size-1)
            .set_access(true, 0, true, false, false)
            .set_flags(false, false);
    gdt[GDT_SEL_PXE_BCW >> 3].set_base(bp->BC_CodeWrite.ofs)
            .set_limit(bp->BC_CodeWrite.size-1)
            .set_access(true, 0, false, false, true)
            .set_flags(false, false);

    gdt[GDT_SEL_PXE_ENTRY >> 3].set_base(bp->EntryPointESP_seg << 4)
            .set_limit(0xFFFF)
            .set_access(true, 0, true, false, true)
            .set_flags(false, false);

    gdt[GDT_SEL_PXE_TEMP >> 3].set_base(bp->EntryPointSP_seg << 4)
            .set_limit(0xFFFF)
            .set_access(true, 0, true, false, true)
            .set_flags(false, false);

//    pep->bc_code_seg = GDT_SEL_PXE_BC;
//    pep->bc_data_seg = GDT_SEL_PXE_BD;
//    pep->stack_seg = GDT_SEL_PXE_STACK;
//    pep->undi_code_seg = GDT_SEL_PXE_UC;
//    pep->undi_data_seg = GDT_SEL_PXE_UD;

    bp->EntryPointSP_seg = GDT_SEL_PXE_ENTRY;
    bp->EntryPointESP_seg = GDT_SEL_PXE_ENTRY;

    bp->Stack.seg = GDT_SEL_PXE_STACK;
    bp->UNDIData.seg = GDT_SEL_PXE_UD;
    bp->UNDICode.seg = GDT_SEL_PXE_UC;
    bp->UNDICodeWrite.seg = GDT_SEL_PXE_UCW;
    bp->BC_Data.seg = GDT_SEL_PXE_BD;
    bp->BC_Code.seg = GDT_SEL_PXE_BC;
    bp->BC_CodeWrite.seg = GDT_SEL_PXE_BCW;

    bp->FirstSelector = GDT_SEL_PXE_1ST;

    bp->StatusCallout_ofs = -1;
    bp->StatusCallout_seg = -1;

    bang_pxe_ptr[0] = uintptr_t(bp) - bp->UNDICode.ofs;
    bang_pxe_ptr[1] = GDT_SEL_PXE_UC;

    pxe_call = pxe_call_bangpxe_pm;

    pxe_entry_vec[0] = bp->EntryPointESP_ofs;
    pxe_entry_vec[1] = bp->EntryPointESP_seg;
#else
    pxe_call = pxe_call_bangpxe_rm;

    pxe_entry_vec[0] = bp->EntryPointSP_ofs;
    pxe_entry_vec[1] = bp->EntryPointSP_seg;
#endif

    PRINT("Using %s API", "!PXE");

    return true;
}

static bool pxe_set_api(pxenv_plus_t *pep)
{
    if (memcmp(pep->sig, "PXENV+", 6) ||
            boottbl_checksum((char const *)pep, pep->length))
        return false;

    if (pep->version >= 0x201) {
        bang_pxe_ptr[0] = pep->pxe_ptr_ofs;
        bang_pxe_ptr[1] = pep->pxe_ptr_seg;
        return pxe_set_api((bangpxe_t*)((uintptr_t(pep->pxe_ptr_seg) << 4) +
                                        pep->pxe_ptr_ofs));
    }

    pxe_call = pxe_call_pxenv;

    pxe_entry_vec[0] = pep->rm_entry_ofs;
    pxe_entry_vec[1] = pep->rm_entry_seg;

    PRINT("Using %s API", "PXENV+");

    return true;
}

extern "C" void pxe_main(pxenv_plus_t *pep, bangpxe_t *)
{
    PRINT("PXE bootloader started...");

    if (!pxe_set_api(pep))
        return;

    pxe_init_fs();

    elf64_run(cpu_choose_kernel());
}
