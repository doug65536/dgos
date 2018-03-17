#include "dev_storage.h"
#include "nvmedecl.h"
#include "device/pci.h"
#include "mm.h"
#include "printk.h"
#include "cpu/atomic.h"
#include "nvmebits.h"
#include "string.h"
#include "utility.h"
#include "thread.h"
#include "vector.h"
#include "cpu/control_regs.h"
#include "mutex.h"

#define NVME_DEBUG	1
#if NVME_DEBUG
#define NVME_TRACE(...) printdbg("nvme: " __VA_ARGS__)
#else
#define NVME_TRACE(...) ((void)0)
#endif

// 5.11 Identify command
nvme_cmd_t nvme_cmd_t::create_identify(
        void *addr, uint8_t cns, uint8_t nsid)
{
    nvme_cmd_t cmd{};
    cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
                uint8_t(nvme_admin_cmd_opcode_t::identify));
    cmd.hdr.nsid = nsid;
    cmd.hdr.dptr.prpp[0].addr = mphysaddr(addr);
    cmd.cmd_dword_10[0] =
            NVME_CMD_IDENT_CDW10_CNTID_n(0) |
            NVME_CMD_IDENT_CDW10_CNS_n(cns);
    return cmd;
}

nvme_cmd_t nvme_cmd_t::create_sub_queue(
        void *addr, uint32_t size,
        uint16_t sqid, uint16_t cqid, uint8_t prio)
{
    nvme_cmd_t cmd{};
    cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
                uint8_t(nvme_admin_cmd_opcode_t::create_sq));
    cmd.hdr.dptr.prpp[0].addr = mphysaddr(addr);
    cmd.cmd_dword_10[0] =
            NVME_CMD_CSQ_CDW10_QSIZE_n(size - 1) |
            NVME_CMD_CSQ_CDW10_QID_n(sqid);
    cmd.cmd_dword_10[1] =
            NVME_CMD_CSQ_CDW11_CQID_n(cqid) |
            NVME_CMD_CSQ_CDW11_QPRIO_n(prio) |
            NVME_CMD_CSQ_CDW11_PC_n(1);
    return cmd;
}

nvme_cmd_t nvme_cmd_t::create_cmp_queue(
        void *addr, uint32_t size,
        uint16_t cqid, uint16_t intr)
{
    nvme_cmd_t cmd{};
    cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
                uint8_t(nvme_admin_cmd_opcode_t::create_cq));
    cmd.hdr.dptr.prpp[0].addr = mphysaddr(addr);
    cmd.cmd_dword_10[0] =
            NVME_CMD_CCQ_CDW10_QSIZE_n(size - 1) |
            NVME_CMD_CCQ_CDW10_QID_n(cqid);
    cmd.cmd_dword_10[1] =
            NVME_CMD_CCQ_CDW11_IV_n(intr) |
            NVME_CMD_CCQ_CDW11_IEN_n(1) |
            NVME_CMD_CCQ_CDW11_PC_n(1);
    return cmd;
}

nvme_cmd_t nvme_cmd_t::create_read(uint64_t lba, uint32_t count, uint8_t ns)
{
    nvme_cmd_t cmd{};
    cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
                uint8_t(nvme_cmd_opcode_t::read));
    cmd.hdr.nsid = ns;
    cmd.cmd_dword_10[0] =
            NVME_CMD_READ_CDW10_SLBA_n(uint32_t(lba & 0xFFFFFFFF));
    cmd.cmd_dword_10[1] =
            NVME_CMD_READ_CDW11_SLBA_n(uint32_t(lba >> 32));
    cmd.cmd_dword_10[2] =
            NVME_CMD_READ_CDW12_NLB_n(count - 1);
    return cmd;
}

nvme_cmd_t nvme_cmd_t::create_write(uint64_t lba, uint32_t count,
                               uint8_t ns, bool fua)
{
    nvme_cmd_t cmd{};
    cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
                uint8_t(nvme_cmd_opcode_t::write));
    cmd.hdr.nsid = ns;
    cmd.cmd_dword_10[0] =
            NVME_CMD_WRITE_CDW10_SLBA_n(uint32_t(lba & 0xFFFFFFFF));
    cmd.cmd_dword_10[1] =
            NVME_CMD_WRITE_CDW11_SLBA_n(uint32_t(lba >> 32));
    cmd.cmd_dword_10[2] =
            NVME_CMD_WRITE_CDW12_FUA_n((uint8_t)fua) |
            NVME_CMD_WRITE_CDW12_NLB_n(count - 1);
    return cmd;
}

nvme_cmd_t nvme_cmd_t::create_trim(uint64_t lba, uint32_t count,
                                   uint8_t ns)
{
    nvme_cmd_t cmd{};
    cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
                uint8_t(nvme_cmd_opcode_t::dataset_mgmt));
    cmd.hdr.nsid = ns;

    nvme_dataset_range_t range;
    range.attr = 0;
    range.lba_count = count;
    range.starting_lba = lba;

    cmd.hdr.dptr.prpp[0].addr = mphysaddr(&range);

    cmd.cmd_dword_10[0] = NVME_CMD_DSMGMT_CDW10_NR_n(1);
    cmd.cmd_dword_10[1] = NVME_CMD_DSMGMT_CDW11_AD_n(1);

    return cmd;
}

nvme_cmd_t nvme_cmd_t::create_flush(uint8_t ns)
{
    nvme_cmd_t cmd{};
    cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
                uint8_t(nvme_cmd_opcode_t::flush));
    cmd.hdr.nsid = ns;
    return cmd;
}

nvme_cmd_t nvme_cmd_t::create_setfeatures(uint16_t ncqr, uint16_t nsqr)
{
    nvme_cmd_t cmd{};
    cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
                uint8_t(nvme_admin_cmd_opcode_t::set_features));
    cmd.cmd_dword_10[0] =
            NVME_CMD_SETFEAT_CDW10_FID_n(
                uint8_t(nvme_feat_id_t::num_queues));
    cmd.cmd_dword_10[1] =
            NVME_CMD_SETFEAT_NQ_CDW11_NCQR_n(ncqr-1) |
            NVME_CMD_SETFEAT_NQ_CDW11_NSQR_n(nsqr-1);
    return cmd;
}

// ---------------------------------------------------------------------------
// VFS interface forward declarations
class nvme_if_t;
class nvme_dev_t;

// ---------------------------------------------------------------------------
// NVME queue helper - handles the circular queues for submission and
// completion queues, doorbell writes, and advancing submission queue head
template<typename T>
class nvme_queue_t {
public:
    nvme_queue_t()
        : entries(nullptr)
        , head_doorbell(nullptr)
        , tail_doorbell(nullptr)
        , mask(0)
        , head(0)
        , tail(0)
        , phase(true)
    {
    }

    void init(T* entries, uint32_t count,
              uint32_t volatile *head_doorbell,
              uint32_t volatile *tail_doorbell,
              bool phase)
    {
        assert(count > 0);
        assert_ispo2(count);

        // Only makes sense to have one doorbell
        // Submission queues have tail doorbells
        // Completion queues have head doorbells
        assert((head_doorbell && !tail_doorbell) ||
               (!head_doorbell && tail_doorbell));

        this->entries = entries;
        this->mask = count - 1;
        this->head_doorbell = head_doorbell;
        this->tail_doorbell = tail_doorbell;
        this->phase = phase;
    }

    T* data()
    {
        return entries;
    }

    uint32_t get_tail() const
    {
        return tail;
    }

    bool get_phase() const
    {
        return phase;
    }

    bool is_empty() const
    {
        return head == tail;
    }

    bool is_full() const
    {
        return next(tail) == head;
    }

    template<typename... Args>
    uint32_t enqueue(T&& item)
    {
        size_t index = tail;
        entries[tail] = move(item);
        phase ^= set_tail(next(tail));
        return index;
    }

    T& at_tail(size_t tail_offset, bool& ret_phase)
    {
        uint32_t index = (tail + tail_offset) & mask;
        ret_phase = phase ^ (index < tail);
        return entries[index];
    }

    // Returns the number of items ready to be dequeued
    uint32_t count() const
    {
        return (tail - head) & mask;
    }

    // Returns the number of items that may be enqueued
    uint32_t space() const
    {
        return (head - tail) & mask;
    }

    uint32_t enqueued(uint32_t count)
    {
        uint32_t index = (tail + count) & mask;
        phase ^= set_tail(index);
        return index;
    }

    T dequeue()
    {
        T item = entries[head];
        take(1);
        return item;
    }

    // Used when completions have been consumed
    void take(uint32_t count)
    {
        uint32_t index = (head + count) & mask;
        phase ^= set_head(index);
    }

    // Used when processing completions to free up submission queue entries
    void take_until(uint32_t new_head)
    {
        phase ^= set_head(new_head);
    }

    T const& at_head(uint32_t head_offset, bool &expect_phase) const
    {
        uint32_t index = (head + head_offset) & mask;
        expect_phase = phase ^ (index < head);
        return entries[(head + head_offset) & mask];
    }

    void reset()
    {
        while (!is_empty())
            dequeue();
        head = 0;
        tail = 0;
    }

private:
    uint32_t next(uint32_t index) const
    {
        return (index + 1) & mask;
    }

    // Returns 1 if the queue wrapped
    bool set_head(uint32_t new_head)
    {
        bool wrapped = new_head < head;

        head = new_head;

        if (head_doorbell)
            *head_doorbell = head;

        return wrapped;
    }

    // Returns 1 if the queue wrapped
    bool set_tail(uint32_t new_tail)
    {
        bool wrapped = new_tail < tail;

        tail = new_tail;

        if (tail_doorbell)
            *tail_doorbell = tail;

        return wrapped;
    }

    T* entries;
    uint32_t volatile *head_doorbell;
    uint32_t volatile *tail_doorbell;
    uint32_t mask;
    uint32_t head;
    uint32_t tail;
    bool phase;
};

// Carries context information through device detection
// asynchronous commands
class nvme_detect_dev_ctx_t {
public:
    nvme_detect_dev_ctx_t(if_list_t& list)
        : identify_data(nullptr)
        , list(list)
        , cur_ns(0)
        , identify_data_physaddr(0)
        , done(false)
    {
        identify_data_physaddr = mm_alloc_contiguous(4096);

        NVME_TRACE("namespace identify data at physaddr=%lx\n",
                   identify_data_physaddr);

        if (!identify_data_physaddr)
            panic("Insufficient contiguous memory!\n");

        identify_data = mmap((void*)identify_data_physaddr, 4096,
                             PROT_READ, MAP_PHYSICAL, -1, 0);

        NVME_TRACE("namespace identify data at vaddr=%p\n",
                   (void*)identify_data);

        if (!identify_data_physaddr)
            panic("Failed to map identify data!\n");
    }

    ~nvme_detect_dev_ctx_t()
    {
        munmap(identify_data, 4096);
        mm_free_contiguous(identify_data_physaddr, 4096);
    }

    void wait()
    {
        scoped_lock hold(lock);
        while (!done)
            done_cond.wait(hold);
    }

    void set_done()
    {
        scoped_lock hold(lock);
        done = true;
        done_cond.notify_all();
    }

    void *identify_data;
    if_list_t& list;
    uint8_t cur_ns;

private:
    using lock_t = ticketlock;
    using scoped_lock = unique_lock<lock_t>;
    uint64_t identify_data_physaddr;
    lock_t lock;
    condition_variable done_cond;
    bool done;
};

// Completion callback
class nvme_callback_t {
public:
    typedef void (nvme_if_t::*member_t)(
            void *data, nvme_cmp_t& packet,
            uint16_t cmd_id, int status_type, int status);

    nvme_callback_t()
        : member(nullptr)
        , data(nullptr)
    {
    }

    nvme_callback_t(member_t member, void *data)
        : member(member)
        , data(data)
    {
    }

    void operator()(nvme_if_t* owner, nvme_cmp_t& packet,
                    uint16_t cmd_id, int status_type, int status)
    {
        if (member)
            (owner->*member)(data, packet, cmd_id, status_type, status);
    }

private:
    member_t member;
    void *data;
};

enum struct nvme_op_t : uint8_t {
    read,
    write,
    trim,
    flush
};

struct nvme_request_t {
    void *data;
    int64_t count;
    uint64_t lba;
    iocp_t *iocp;
    nvme_op_t op;
    bool fua;
};

typedef nvme_queue_t<nvme_cmd_t> sub_queue_t;
typedef nvme_queue_t<nvme_cmp_t> cmp_queue_t;

class nvme_queue_state_t {
public:
    nvme_queue_state_t()
        : prp_lists(nullptr)
        , ready(false)
    {
    }

    ~nvme_queue_state_t()
    {
    }

    operator bool() const
    {
        return ready;
    }

    void init(size_t count,
              nvme_cmd_t *sub_queue_ptr, uint32_t volatile *sub_doorbell,
              nvme_cmp_t *cmp_queue_ptr, uint32_t volatile *cmp_doorbell);

    template<typename T>
    void submit_multiple();

    void submit_cmd(nvme_cmd_t&& cmd,
                    nvme_callback_t::member_t callback = nullptr,
                    void *data = nullptr,
                    mmphysrange_t *ranges = nullptr,
                    size_t range_count = 0);

    void advance_head(uint16_t new_head, bool need_lock);

    void invoke_completion(nvme_if_t* owner, nvme_cmp_t& packet,
                           uint16_t cmd_id, int status_type, int status);

    void process_completions(nvme_if_t *nvme_if, nvme_queue_state_t *queues);

    nvme_cmd_t *sub_queue_ptr();

    nvme_cmp_t *cmp_queue_ptr();

private:
    sub_queue_t sub_queue;
    cmp_queue_t cmp_queue;

    vector<nvme_cmp_t> cmp_buf;

    vector<nvme_callback_t> cmp_handlers;
    uint64_t *prp_lists;

    using lock_t = ticketlock;
    using scoped_lock = unique_lock<lock_t>;
    lock_t lock;
    condition_variable not_full;
    condition_variable not_empty;
    bool ready;
};

// ---------------------------------------------------------------------------
// VFS

class nvme_if_factory_t : public storage_if_factory_t {
public:
    nvme_if_factory_t() : storage_if_factory_t("nvme") {}
private:
    virtual if_list_t detect(void) final;
};

static nvme_if_factory_t nvme_if_factory;
STORAGE_REGISTER_FACTORY(nvme_if);

// NVMe interface instance
class nvme_if_t : public storage_if_base_t {
public:
    bool init(const pci_dev_iterator_t &pci_dev);
    size_t get_queue_count() const;

private:
    STORAGE_IF_IMPL

    friend class nvme_dev_t;

    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    void irq_handler(int irq_offset);

    uint32_t volatile* doorbell_ptr(bool completion, size_t queue);

    unsigned io(uint8_t ns, nvme_request_t &request, uint8_t log2_sectorsize);

    // Handle setting the queue count
    void setfeat_queues_handler(void *data, nvme_cmp_t &packet,
                                uint16_t cmd_id, int status_type, int status);

    // Handle controller identify
    void identify_handler(void *data, nvme_cmp_t &packet,
                          uint16_t cmd_id, int status_type, int status);

    // Handle namespace list identify
    void identify_ns_id_handler(void *data, nvme_cmp_t &packet,
                                uint16_t cmd_id, int status_type, int status);

    // Handle namespace identify
    void identify_ns_handler(void *data, nvme_cmp_t &packet,
                             uint16_t cmd_id, int status_type, int status);

    // I/O request
    void io_handler(void *data, nvme_cmp_t &packet,
                    uint16_t cmd_id, int status_type, int status);

    static errno_t status_to_errno(int status_type, int status);

    nvme_mmio_t volatile *mmio_base;
    pci_config_hdr_t config;
    pci_irq_range_t irq_range;

    size_t doorbell_shift;

    // Queue count, including admin queue
    size_t queue_count;
    size_t max_queues;

    uintptr_t queue_memory_physaddr;
    void* queue_memory;

    uint32_t host_buffer_size;
    uintptr_t host_buffer_physaddr;

    vector<uint32_t> namespaces;

    unique_ptr<nvme_queue_state_t[]> queues;
    bool use_msi;
};

class nvme_dev_t : public storage_dev_base_t {
public:
    void init(nvme_if_t *parent, uint8_t ns, uint8_t log2_sectorsize);

private:
    STORAGE_DEV_IMPL

    errno_t io(void *data, int64_t count,
               uint64_t lba, bool fua, nvme_op_t op, iocp_t *iocp);

    nvme_if_t *parent;
    uint8_t ns;
    uint8_t log2_sectorsize;
};

#define NVME_MAX_DEVICES    8
static nvme_if_t nvme_devices[NVME_MAX_DEVICES];
static unsigned nvme_count;

#define NVME_MAX_DRIVES    8
static nvme_dev_t nvme_drives[NVME_MAX_DRIVES];
static unsigned nvme_drive_count;

if_list_t nvme_if_factory_t::detect(void)
{
    unsigned start_at = nvme_count;

    if_list_t list = {
        nvme_devices + start_at,
        sizeof(*nvme_devices),
        0
    };

    //return list;

    pci_dev_iterator_t pci_iter;

    NVME_TRACE("enumerating PCI busses for devices...\n");

    if (!pci_enumerate_begin(
                &pci_iter,
                PCI_DEV_CLASS_STORAGE,
                PCI_SUBCLASS_STORAGE_NVM))
        return list;

    do {
        assert(pci_iter.dev_class == PCI_DEV_CLASS_STORAGE);
        assert(pci_iter.subclass == PCI_SUBCLASS_STORAGE_NVM);

        // Make sure it is an NVMe device
        if (pci_iter.config.prog_if != PCI_PROGIF_STORAGE_NVM_NVME)
            continue;

        NVME_TRACE("found device!\n");

        if (nvme_count < countof(nvme_devices)) {
            nvme_if_t *self = nvme_devices + nvme_count++;

            if (!self->init(pci_iter)) {
                // Destruct and restore zero initialization
                self->~nvme_if_t();
                memset(self, 0, sizeof(*self));
                new (self) nvme_if_t;
                --nvme_count;
            }
        }
    } while (pci_enumerate_next(&pci_iter));

    list.count = nvme_count - start_at;

    NVME_TRACE("found %u drives (namespaces)\n", list.count);

    return list;
}

bool nvme_if_t::init(pci_dev_iterator_t const &pci_dev)
{
    config = pci_dev.config;

    uint64_t addr = (uint64_t(pci_dev.config.base_addr[1]) << 32) |
            pci_dev.config.base_addr[0];

    mmio_base = (nvme_mmio_t*)mmap(
                (void*)(addr & -8),
                0x2000, PROT_READ | PROT_WRITE,
                MAP_PHYSICAL, -1, 0);

    // 7.6.1 Initialization

    // Enable bus master DMA, enable MMIO, disable port I/O
    pci_adj_control_bits(pci_dev, PCI_CMD_BME | PCI_CMD_MSE, PCI_CMD_IOSE);

    size_t requested_queue_count = thread_get_cpu_count() + 1;

    // The admin queue is queue[0],
    // and the I/O command queue for CPU 0 is queue[1],
    // and the I/O command queue for CPU 1 is queue[2], etc,
    // so provide a target CPU list that targets the MSI-X interrupts
    // appropriately, in case MSI-X is supported
    unique_ptr<int[]> target_cpus(new int[requested_queue_count]);
    target_cpus[0] = 0;
    for (size_t i = 1; i < requested_queue_count; ++i)
        target_cpus[i] = i - 1;

    // Try to use MSI IRQ
    use_msi = pci_try_msi_irq(pci_dev, &irq_range, 0, true,
                              min(requested_queue_count - 1, size_t(32)),
                              irq_handler, target_cpus.get());

    target_cpus.reset();

    NVME_TRACE("Using IRQs msi=%d, base=%u, count=%u\n",
               use_msi, irq_range.base, irq_range.count);

    // Disable the controller
    if (mmio_base->cc & NVME_CC_EN)
        mmio_base->cc &= ~NVME_CC_EN;

    // 7.6.1 2) Wait for the controller to indicate that any previous
    // reset is complete
    while ((mmio_base->csts & NVME_CSTS_RDY) != 0)
        pause();

    // Attempt to use 64KB/16KB submission/completion queue sizes
    size_t queue_slots = 1024;
    assert(queue_slots <= 4096);
    size_t max_queue_slots = NVME_CAP_MQES_GET(mmio_base->cap) + 1;

    if (queue_slots > max_queue_slots)
        queue_slots = max_queue_slots;

    // Size of one queue, in bytes
    size_t queue_bytes = queue_slots * sizeof(nvme_cmd_t) +
            queue_slots * sizeof(nvme_cmp_t);

    queue_count = 1;
    queue_bytes *= requested_queue_count;

    queue_memory_physaddr = mm_alloc_contiguous(queue_bytes);

    queue_memory = mmap(
                (void*)queue_memory_physaddr, queue_bytes,
                PROT_READ | PROT_WRITE,
                MAP_PHYSICAL, -1, 0);

    memset(queue_memory, 0, queue_bytes);

    // 7.6.1 3) The admin queue should be configured
    mmio_base->aqa =
            NVME_AQA_ACQS_n(queue_slots) |
            NVME_AQA_ASQS_n(queue_slots);

    // Submission queue address
    mmio_base->asq = (uint64_t)queue_memory_physaddr;

    // 3.1.10 The vector for the admin queues is always 0
    // Completion queue address
    mmio_base->acq = (uint64_t)queue_memory_physaddr +
            (sizeof(nvme_cmd_t) * queue_slots * requested_queue_count);

    // 7.6.1 4) The controller settings should be configured

    uint32_t cc = NVME_CC_IOCQES_n(bit_log2(sizeof(nvme_cmp_t))) |
            NVME_CC_IOSQES_n(bit_log2(sizeof(nvme_cmd_t))) |
            NVME_CC_MPS_n(0) |
            NVME_CC_CCS_n(0);

    // Try to enable weighted round robin with urgent
    if (NVME_CAP_AMS_GET(mmio_base->cap) & 1)
        cc |= NVME_CC_AMS_n(1);

    mmio_base->cc = cc;

    // Set enable with a separate write
    cc |= NVME_CC_EN;
    mmio_base->cc = cc;

    // 7.6.1 4) Wait for ready
    uint32_t ctrl_status;
    while (!((ctrl_status = mmio_base->csts) &
             (NVME_CSTS_RDY | NVME_CSTS_CFS)))
        pause();

    if (ctrl_status & NVME_CSTS_CFS) {
        // Controller fatal status
        NVME_TRACE("Controller fatal status!\n");
        return false;
    }

    // Read the doorbell stride
    doorbell_shift = NVME_CAP_DSTRD_GET(mmio_base->cap) + 1;

    // 7.4 Initialize queues

    // Initialize just the admin queue until we are sure about the
    // number of supported queues

    nvme_cmd_t *sub_queue_ptr = (nvme_cmd_t*)queue_memory;
    nvme_cmp_t *cmp_queue_ptr = (nvme_cmp_t*)
            (sub_queue_ptr + requested_queue_count * queue_slots);

    queues.reset(new nvme_queue_state_t[requested_queue_count]);

    nvme_queue_state_t& admin_queue = queues[0];

    admin_queue.init(queue_slots, sub_queue_ptr, doorbell_ptr(false, 0),
                   cmp_queue_ptr, doorbell_ptr(true, 0));
    sub_queue_ptr += queue_slots;
    cmp_queue_ptr += queue_slots;

    NVME_TRACE("Requesting queue count %zd\n", requested_queue_count - 1);

    blocking_iocp_t blocking_setfeatures;

    // Do a Set Features command to request the desired number of queues
    admin_queue.submit_cmd(nvme_cmd_t::create_setfeatures(
                               requested_queue_count - 1,
                               requested_queue_count - 1),
                           &nvme_if_t::setfeat_queues_handler,
                           (iocp_t*)&blocking_setfeatures);

    blocking_setfeatures.set_expect(1);
    errno_t status = blocking_setfeatures.wait();

    if (status != errno_t::OK)
        return false;

    queue_count = min(requested_queue_count, max_queues);

    NVME_TRACE("Allocated queue count %zu\n", queue_count - 1);

    for (size_t i = 1; i < queue_count; ++i) {
        nvme_queue_state_t& queue = queues[i];
        queue.init(queue_slots, sub_queue_ptr, doorbell_ptr(false, i),
                   cmp_queue_ptr, doorbell_ptr(true, i));
        sub_queue_ptr += queue_slots;
        cmp_queue_ptr += queue_slots;
    }

    uintptr_t identify_physaddr = mm_alloc_contiguous(4096);

    void* identify = mmap((void*)identify_physaddr, 4096,
                          PROT_READ | PROT_WRITE,
                          MAP_PHYSICAL, -1, 0);

    // 5.11 Execute identify controller command
    admin_queue.submit_cmd(nvme_cmd_t::create_identify(identify, 1, 0),
                           &nvme_if_t::identify_handler, identify);

    // Create completion queues
    for (size_t i = 1; i < queue_count; ++i) {
        admin_queue.submit_cmd(nvme_cmd_t::create_cmp_queue(
                                   queues[i].cmp_queue_ptr(),
                                   queue_slots, i, i & (irq_range.count - 1)));
    }

    // Create submission queues
    for (size_t i = 1; i < queue_count; ++i) {
        admin_queue.submit_cmd(nvme_cmd_t::create_sub_queue(
                                   queues[i].sub_queue_ptr(),
                                   queue_slots, i, i, 2));
    }

    NVME_TRACE("interface initialization success\n");

    return true;
}

size_t nvme_if_t::get_queue_count() const
{
    return queue_count;
}

void nvme_if_t::identify_ns_id_handler(
        void *data, nvme_cmp_t&, uint16_t, int, int)
{
    NVME_TRACE("enumerating namespaces\n");

    auto ctx = (nvme_detect_dev_ctx_t*)data;
    auto ns_list = (uint32_t*)ctx->identify_data;

    int count;
    for (count = 0; count < 1024; ++count) {
        if (ns_list[count] == 0)
            break;
    }

    NVME_TRACE("Found %d namespaces\n", count);

    namespaces.reserve(count);

    for (int i = 0; i < count; ++i)
        namespaces.push_back(ns_list[i]);

    ctx->cur_ns = 0;
    uint8_t ns = namespaces[ctx->cur_ns];
    NVME_TRACE("issuing identify namespace %u\n", ns);
    queues[0].submit_cmd(nvme_cmd_t::create_identify(
                             ctx->identify_data, 0, ns),
                         &nvme_if_t::identify_ns_handler, ctx);
}

void nvme_if_t::identify_ns_handler(
        void *data, nvme_cmp_t&, uint16_t, int, int)
{
    auto ctx = (nvme_detect_dev_ctx_t*)data;
    auto ns_ident = (nvme_ns_identify_t*)ctx->identify_data;

    size_t cur_format_index = NVME_NS_IDENT_FLBAS_LBAIDX_GET(ns_ident->flbas);
    uint32_t lba_format = ns_ident->lbaf[cur_format_index];
    uint8_t log2_sectorsize = NVME_NS_IDENT_LBAF_LBADS_GET(lba_format);

    nvme_dev_t *drive = nvme_drives + nvme_drive_count++;

    drive->init(this, namespaces[ctx->cur_ns], log2_sectorsize);

    // Enumerate next namespace if there are more
    if (++ctx->cur_ns < namespaces.size()) {
        uint8_t ns = namespaces[ctx->cur_ns];
        NVME_TRACE("issuing identify namespace %u\n", ns);

        queues[0].submit_cmd(nvme_cmd_t::create_identify(
                             ctx->identify_data, 0, ns),
                         &nvme_if_t::identify_ns_handler, ctx);
    } else {
        ctx->set_done();
    }
}

void nvme_if_t::identify_handler(
        void *data, nvme_cmp_t&, uint16_t, int, int)
{
    nvme_identify_t* identify = (nvme_identify_t*)data;

    host_buffer_size = identify->hmpre;
    if (host_buffer_size > 0)
        host_buffer_physaddr = mm_alloc_contiguous(host_buffer_size);

    mm_free_contiguous(mphysaddr(data), 4096);
    munmap(data, 4096);
}

void nvme_if_t::cleanup()
{
}

if_list_t nvme_if_t::detect_devices()
{
    unsigned start_at = nvme_drive_count;

    if_list_t list = {
        nvme_drives + start_at,
        sizeof(*nvme_drives),
        0
    };

    NVME_TRACE("enumerating namespaces\n");

    nvme_detect_dev_ctx_t ctx(list);

    // Get namespace list
    queues[0].submit_cmd(nvme_cmd_t::create_identify(
                             ctx.identify_data, 2, 0),
                         &nvme_if_t::identify_ns_id_handler,
                         &ctx);

    ctx.wait();

    list.count = nvme_drive_count - start_at;

    NVME_TRACE("enumerating namespaces complete,"
               " found %u namespaces\n", ctx.list.count);

    return list;
}

void nvme_dev_t::init(nvme_if_t *parent,
                      uint8_t ns, uint8_t log2_sectorsize)
{
    this->parent = parent;
    this->ns = ns;
    this->log2_sectorsize = log2_sectorsize;
}

void nvme_dev_t::cleanup()
{

}

isr_context_t *nvme_if_t::irq_handler(int irq, isr_context_t *ctx)
{
    for (unsigned i = 0; i < nvme_count; ++i) {
        nvme_if_t *dev = nvme_devices + i;

        int irq_offset = irq - dev->irq_range.base;

        if (unlikely(irq_offset < 0 || irq_offset > dev->irq_range.count))
            continue;

        dev->irq_handler(irq_offset);
    }

    return ctx;
}

void nvme_if_t::irq_handler(int irq_offset)
{
    //NVME_TRACE("received IRQ\n");

    int queue_stride = irq_range.count;

    for (size_t i = irq_offset; i < queue_count; i += queue_stride)
    {
        nvme_queue_state_t& queue = queues[i];
        queue.process_completions(this, queues);
    }
}

uint32_t volatile* nvme_if_t::doorbell_ptr(bool completion, size_t queue)
{
    uint32_t volatile *doorbells = (uint32_t volatile *)(mmio_base + 1);
    return doorbells + ((queue << doorbell_shift) + completion);
}

unsigned nvme_if_t::io(uint8_t ns, nvme_request_t &request,
                       uint8_t log2_sectorsize)
{
    size_t cur_cpu = thread_cpu_number();

    int queue_index;

    if (cur_cpu < queue_count - 1)
        queue_index = cur_cpu + 1;
    else
        queue_index = cur_cpu % (queue_count - 1) + 1;

    size_t bytes = request.count << log2_sectorsize;

    uint32_t expect = 0;

    while (request.count > 0) {
        ++expect;

        size_t chunk;
        size_t lba_count;
        size_t range_count;
        mmphysrange_t ranges[16];
        nvme_cmd_t cmd;

        switch (request.op) {
        case nvme_op_t::read:
        case nvme_op_t::write:
            chunk = min(bytes, size_t(0x10000));
            range_count = mphysranges(ranges, countof(ranges),
                                      request.data, chunk, PAGE_SIZE);

            lba_count = chunk >> log2_sectorsize;
            request.count -= lba_count;
            request.data = (char*)request.data + (lba_count << log2_sectorsize);
            break;

        case nvme_op_t::flush:
            range_count = 0;
            request.count = 0;
            break;

        default:
            panic("Unhandled operation! op=%d", (int)request.op);
        }

        switch (request.op) {
        case nvme_op_t::read:
            cmd = nvme_cmd_t::create_read(
                    request.lba, lba_count, ns);
            break;

        case nvme_op_t::write:
            cmd = nvme_cmd_t::create_write(
                    request.lba, lba_count, ns, request.fua);
            break;

        case nvme_op_t::trim:
            cmd = nvme_cmd_t::create_trim(
                    request.lba, lba_count, ns);
            break;

        case nvme_op_t::flush:
            cmd = nvme_cmd_t::create_flush(ns);
            break;

        }

        nvme_queue_state_t& queue = queues[queue_index];
        queue.submit_cmd(move(cmd), &nvme_if_t::io_handler, request.iocp,
                         ranges, range_count);
    }

    return expect;
}

void nvme_if_t::io_handler(void *data, nvme_cmp_t&,
                           uint16_t, int status_type, int status)
{
    iocp_t* iocp = (iocp_t*)data;

    errno_t err = status_to_errno(status_type, status);

    iocp->set_result(err);
    iocp->invoke();
}

errno_t nvme_if_t::status_to_errno(int status_type, int status)
{
    return status_type == 0 && status == 0
            ? errno_t::OK
            : errno_t::EIO;
}

void nvme_if_t::setfeat_queues_handler(
        void *data, nvme_cmp_t& packet,
        uint16_t, int status_type, int status)
{
    iocp_t* iocp = (iocp_t*)data;
    uint16_t max_sq = NVME_CMP_SETFEAT_NQ_DW0_NCQA_GET(packet.cmp_dword[0]);
    uint16_t max_cq = NVME_CMP_SETFEAT_NQ_DW0_NSQA_GET(packet.cmp_dword[0]);
    max_queues = min(max_sq, max_cq) + 1;

    errno_t err = status_to_errno(status_type, status);

    iocp->set_result(err);
    iocp->invoke();
}

errno_t nvme_dev_t::io(
        void *data, int64_t count, uint64_t lba,
        bool fua, nvme_op_t op, iocp_t *iocp)
{
   nvme_request_t request;
   request.data = data;
   request.count = count;
   request.lba = lba;
   request.op = op;
   request.fua = fua;
   request.iocp = iocp;

   int expect = parent->io(ns, request, log2_sectorsize);
   iocp->set_expect(expect);

   return errno_t::OK;
}

errno_t nvme_dev_t::read_async(
        void *data, int64_t count,
        uint64_t lba, iocp_t *iocp)
{
    //NVME_TRACE("Reading %ld blocks at LBA %lx", count, lba);

    return io(data, count, lba, false, nvme_op_t::read, iocp);
}

errno_t nvme_dev_t::write_async(
        void const *data, int64_t count,
        uint64_t lba, bool fua, iocp_t *iocp)
{
    return io((void*)data, count, lba, fua, nvme_op_t::write, iocp);
}

errno_t nvme_dev_t::flush_async(iocp_t *iocp)
{
    return io(nullptr, 0, 0, false, nvme_op_t::flush, iocp);
}

errno_t nvme_dev_t::trim_async(int64_t count,
        uint64_t lba, iocp_t *iocp)
{
    return io(nullptr, count, lba, false, nvme_op_t::trim, iocp);
}

long nvme_dev_t::info(storage_dev_info_t key)
{
    switch (key) {
    case STORAGE_INFO_BLOCKSIZE:
        return 1L << log2_sectorsize;

    case STORAGE_INFO_HAVE_TRIM:
        return 1;

    case STORAGE_INFO_NAME:
        return long("NVME");

    default:
        return 0;
    }
}

void nvme_queue_state_t::init(
        size_t count, nvme_cmd_t *sub_queue_ptr,
        uint32_t volatile *sub_doorbell, nvme_cmp_t *cmp_queue_ptr,
        volatile uint32_t *cmp_doorbell)
{
    sub_queue.init(sub_queue_ptr, count, nullptr, sub_doorbell, 1);
    cmp_queue.init(cmp_queue_ptr, count, cmp_doorbell, nullptr, 1);

    cmp_handlers.resize(count);
    cmp_buf.reserve(count);

    // Allocate enough memory for 4 PRP list entries per slot
    prp_lists = (uint64_t*)mmap(
                nullptr, count * sizeof(*prp_lists) * 16,
                PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
}

void nvme_queue_state_t::submit_cmd(
        nvme_cmd_t &&cmd, nvme_callback_t::member_t callback,
        void *data, mmphysrange_t *ranges, size_t range_count)
{
    scoped_lock hold(lock);

    while (sub_queue.is_full())
        not_full.wait(hold);

    uint32_t index = sub_queue.get_tail();

    NVME_CMD_SDW0_CID_SET(cmd.hdr.cdw0, index);

    if (range_count > 2) {
        cmd.hdr.dptr.prpp[0].addr = ranges[0].physaddr;

        uint64_t volatile *prp_list = prp_lists + index * 16;

        for (size_t i = 1; i < range_count; ++i)
            prp_list[i-1] = ranges[i].physaddr;

        cmd.hdr.dptr.prpp[1].addr = mphysaddr(prp_list);
    } else if (range_count > 1) {
        cmd.hdr.dptr.prpp[0].addr = ranges[0].physaddr;
        cmd.hdr.dptr.prpp[1].addr = ranges[1].physaddr;
    } else if (range_count > 0) {
        cmd.hdr.dptr.prpp[0].addr = ranges[0].physaddr;
    }

    assert(index < cmp_handlers.size());
    cmp_handlers[index] = nvme_callback_t(callback, data);

    sub_queue.enqueue(move(cmd));
}

void nvme_queue_state_t::advance_head(uint16_t new_head, bool need_lock)
{
    scoped_lock hold(lock, defer_lock_t());

    if (need_lock)
        hold.lock();

    sub_queue.take_until(new_head);
    not_full.notify_all();
}

void nvme_queue_state_t::invoke_completion(
        nvme_if_t *owner, nvme_cmp_t &packet,
        uint16_t cmd_id, int status_type, int status)
{
    assert(cmd_id < cmp_handlers.size());
    cmp_handlers[cmd_id](owner, packet, cmd_id, status_type, status);
}

void nvme_queue_state_t::process_completions(
        nvme_if_t *nvme_if, nvme_queue_state_t *queues)
{
    scoped_lock hold(lock);

    uint32_t i = 0;
    for (;;) {
        bool phase;
        nvme_cmp_t const& packet = cmp_queue.at_head(i, phase);

        // Done when phase does not match expected phase
        if (NVME_CMP_DW3_P_GET(packet.cmp_dword[3]) != phase)
            break;

        ++i;

        // Decode submission queue for which command has completed
        unsigned sub_queue_id = NVME_CMP_DW2_SQID_GET(
                    packet.cmp_dword[2]);
        assert(sub_queue_id < nvme_if->get_queue_count());
        nvme_queue_state_t& sub_queue_state = queues[sub_queue_id];

        // Get submission queue head
        unsigned sub_queue_head = NVME_CMP_DW2_SQHD_GET(
                    packet.cmp_dword[2]);
        sub_queue_state.advance_head(sub_queue_head,
                                     &sub_queue_state != this);

        cmp_buf.push_back(packet);
    }

    if (i > 0)
        cmp_queue.take(i);

    hold.unlock();

    for (nvme_cmp_t& packet : cmp_buf) {
        //bool dnr = NVME_CMP_DW3_DNR_GET(packet.cmp_dword[3]);
        int status_type = NVME_CMP_DW3_SCT_GET(packet.cmp_dword[3]);
        int status = NVME_CMP_DW3_SC_GET(packet.cmp_dword[3]);
        uint16_t cmd_id = NVME_CMP_DW3_CID_GET(packet.cmp_dword[3]);

        invoke_completion(nvme_if, packet, cmd_id, status_type, status);
    }

    cmp_buf.clear();
}

nvme_cmd_t *nvme_queue_state_t::sub_queue_ptr()
{
    return sub_queue.data();
}

nvme_cmp_t *nvme_queue_state_t::cmp_queue_ptr()
{
    return cmp_queue.data();
}
