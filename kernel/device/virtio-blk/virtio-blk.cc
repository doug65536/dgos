// pci driver: C=STORAGE, V=VIRTIO (0x1AF4)

#include "kmodule.h"
#include "../pci.h"

PCI_DRIVER(
        virtio_blk,
        0x1AF4, -1, PCI_DEV_CLASS_STORAGE, -1, -1);

#include "virtio-blk.h"
#include "../virtio-base/virtio-base.h"
#include "dev_storage.h"
#include "callout.h"
#include "numeric_limits.h"

#define DEBUG_VIRTIO_BLK 0
#if DEBUG_VIRTIO_BLK
#define VIRTIO_BLK_TRACE(...) printdbg("virtio-blk: " __VA_ARGS__)
#else
#define VIRTIO_BLK_TRACE(...) ((void)0)
#endif

#define VIRTIO_DEVICE_BLK (0x1042)

// Maximum size of any single segment is in size_max.
#define VIRTIO_BLK_F_SIZE_MAX_BIT (1)

// Maximum number of segments in a request is in seg_max.
#define VIRTIO_BLK_F_SEG_MAX_BIT (2)

// Disk-style geometry specified in geometry.
#define VIRTIO_BLK_F_GEOMETRY_BIT (4)

// Device is read-only.
#define VIRTIO_BLK_F_RO_BIT (5)

// Block size of disk is in blk_size.
#define VIRTIO_BLK_F_BLK_SIZE_BIT (6)

// Device exports information on optimal I/O alignment.
#define VIRTIO_BLK_F_TOPOLOGY_BIT (10)

#define VIRTIO_BLK_T_IN     0
#define VIRTIO_BLK_T_OUT    1
#define VIRTIO_BLK_T_FLUSH  4

#define VIRTIO_BLK_S_OK     0
#define VIRTIO_BLK_S_IOERR  1
#define VIRTIO_BLK_S_UNSUPP 2

class virtio_blk_if_t;

class virtio_blk_factory_t : public virtio_factory_base_t {
public:
    ~virtio_blk_factory_t() {}
    int detect();

protected:
    // virtio_factory_base_t interface
    virtio_base_t *create() override final;
    void found_device(virtio_base_t *device) override final;
};

ext::vector<virtio_blk_if_t *> virtio_blk_ifs;

class virtio_blk_if_factory_t : public storage_if_factory_t {
public:
    virtio_blk_if_factory_t();
private:
    virtual ext::vector<storage_if_base_t *> detect(void) override final;
};

enum struct virtio_blk_op_t : uint8_t {
    read,
    write,
    trim,
    flush
};

class virtio_blk_if_t final
        : public storage_if_base_t
        , public storage_dev_base_t
        , public virtio_base_t
{
    struct per_queue_t;

public:
    using virtio_iocp_t = virtio_virtqueue_t::virtio_iocp_t;

    struct request_t {
        struct header_t {
            uint32_t type;
            uint32_t reserved;
            uint64_t lba;
        } header;

        per_queue_t *owner;
        void *data;
        int64_t count;
        virtio_iocp_t io_iocp;
        iocp_t *caller_iocp;
        virtio_blk_op_t op;
        bool fua;

        uint8_t status;
    };

    virtio_blk_if_t()
        : per_queue(nullptr)
        , blk_config(nullptr)
        , log2_sectorsize(0)
    {
    }

    ~virtio_blk_if_t()
    {
    }

    int io(request_t *request);

private:
    STORAGE_IF_IMPL
    STORAGE_DEV_IMPL

    errno_t io(void *data, int64_t count, uint64_t lba, bool fua,
               virtio_blk_op_t op, iocp_t *iocp);

private:
    using lock_type = ext::spinlock;
    using scoped_lock = ext::unique_lock<lock_type>;

    struct blk_config_t {
        uint64_t capacity;
        uint32_t size_max;
        uint32_t seg_max;

        struct virtio_blk_geometry {
            uint16_t cylinders;
            uint8_t heads;
            uint8_t sectors;
        } geometry;

        uint32_t blk_size;

        struct virtio_blk_topology {
            // # of logical blocks per physical block (log2)
            uint8_t physical_block_exp;

            // offset of first aligned logical block
            uint8_t alignment_offset;

            // suggested minimum I/O size in blocks
            uint16_t min_io_size;

            // optimal (suggested maximum) I/O size in blocks
            uint32_t opt_io_size;
        } topology;

        uint8_t reserved;
    };

    struct per_queue_t {
        bool init(virtio_blk_if_t *owner, virtio_virtqueue_t *queue);

        int io(request_t *request);

        virtio_blk_if_t *owner;
        lock_type per_queue_lock;
        ext::vector<virtio_virtqueue_t::desc_t*> desc_chain;
        ext::vector<mmphysrange_t> phys_ranges;
        virtio_virtqueue_t *req_queue;
    };

    // virtio_base_t interface
    bool init(pci_dev_iterator_t const &pci_iter) override final;
    bool offer_features(feature_set_t &features) override final;
    bool verify_features(feature_set_t &features) override final;
    void irq_handler(int offset) override final;

    void config_irq();

    static void io_completion(const uint64_t &total_len, uintptr_t arg);

    per_queue_t *per_queue;
    blk_config_t *blk_config;
    uint8_t log2_sectorsize;
};

int virtio_blk_factory_t::detect()
{
    return detect_virtio(PCI_DEV_CLASS_STORAGE, VIRTIO_DEVICE_BLK,
                         "virtio-blk");
}

virtio_blk_if_factory_t::virtio_blk_if_factory_t()
    : storage_if_factory_t("virtio-blk")
{
    storage_if_register_factory(this);
}

ext::vector<storage_dev_base_t *> virtio_blk_if_t::detect_devices()
{
    return ext::vector<storage_dev_base_t *>(
                virtio_blk_ifs.begin(), virtio_blk_ifs.end());
}

virtio_base_t *virtio_blk_factory_t::create()
{
    return new (ext::nothrow) virtio_blk_if_t;
}

void virtio_blk_factory_t::found_device(virtio_base_t *device)
{
    if (unlikely(!virtio_blk_ifs.push_back(
                     static_cast<virtio_blk_if_t*>(device))))
        panic_oom();
}

int virtio_blk_if_t::io(request_t *request)
{
    uint32_t cpu_nr = thread_cpu_number();
    unsigned queue_nr = cpu_nr % queue_count;

    return per_queue[queue_nr].io(request);
}

bool virtio_blk_if_t::per_queue_t::init(
        virtio_blk_if_t *owner, virtio_virtqueue_t *queue)
{
    this->owner = owner;
    req_queue = queue;
    if (!desc_chain.reserve(size_t(1) << queue->get_log2_queue_size()))
        return false;
    if (!phys_ranges.resize(16))
        return false;

    return true;
}

int virtio_blk_if_t::per_queue_t::io(request_t *request)
{
    request->owner = this;

    switch (request->op) {
    case virtio_blk_op_t::read:
        request->header.type = VIRTIO_BLK_T_IN;
        break;

    case virtio_blk_op_t::write:
        request->header.type = VIRTIO_BLK_T_OUT;
        break;

    case virtio_blk_op_t::flush:
        request->header.type = VIRTIO_BLK_T_FLUSH;
        break;

    case virtio_blk_op_t::trim:
        return -1;
    }

    size_t range_count;
    char *data = (char*)request->data;
    size_t remain = request->count << owner->log2_sectorsize;

    scoped_lock lock(per_queue_lock);

    for (;;) {
        range_count = mphysranges(phys_ranges.data(), phys_ranges.size(),
                                  data, remain, owner->blk_config->size_max);

        if (likely(range_count < phys_ranges.size()))
            break;

        // Double the size and try again
        phys_ranges.resize(phys_ranges.size() * 2);
    }

    if (unlikely(desc_chain.size() < range_count + 2))
        desc_chain.resize(range_count + 2);

    req_queue->alloc_multiple(desc_chain.data(), range_count + 2);

    desc_chain[0]->addr = mphysaddr(&request->header);
    desc_chain[0]->len = sizeof(request->header);

    size_t i;
    for (i = 0; i < range_count; ++i) {
        desc_chain[i]->next = req_queue->index_of(desc_chain[i + 1]);
        desc_chain[i]->flags.bits.next = true;

        desc_chain[i + 1]->addr = phys_ranges[i].physaddr;
        desc_chain[i + 1]->len = phys_ranges[i].size;
        desc_chain[i + 1]->flags.bits.write =   // writes RAM
                (request->op == virtio_blk_op_t::read);
    }

    desc_chain[i]->next = req_queue->index_of(desc_chain[i + 1]);
    desc_chain[i]->flags.bits.next = true;

    desc_chain[i + 1]->addr = mphysaddr(&request->status);
    desc_chain[i + 1]->len = sizeof(request->status);
    desc_chain[i + 1]->next = -1;
    desc_chain[i + 1]->flags.bits.next = false;
    desc_chain[i + 1]->flags.bits.write = true;

    request->io_iocp.reset(&virtio_blk_if_t::io_completion,
                           uintptr_t(request));
    req_queue->enqueue_avail(desc_chain.data(), range_count + 2,
                             &request->io_iocp);

    return 1;
}

void virtio_blk_if_t::io_completion(
        uint64_t const& total_len, uintptr_t arg)
{
    request_t *request = reinterpret_cast<request_t*>(arg);
    request->caller_iocp->invoke();
    delete request;
}

bool virtio_blk_if_t::init(pci_dev_iterator_t const &pci_iter)
{
    if (!virtio_init(pci_iter, "virtio-blk", true))
        return false;

    blk_config = (blk_config_t*)device_cfg;
    log2_sectorsize = bit_log2(blk_config->blk_size);

    per_queue = new (ext::nothrow) per_queue_t[queue_count]();

    for (size_t i = 0; i < queue_count; ++i) {
        if (!per_queue[i].init(this, &queues[i]))
            return false;
    }

    return true;
}

bool virtio_blk_if_t::offer_features(virtio_base_t::feature_set_t &features)
{
    virtio_base_t::feature_set_t support{
        VIRTIO_BLK_F_SIZE_MAX_BIT,
        VIRTIO_BLK_F_SEG_MAX_BIT,
        VIRTIO_BLK_F_GEOMETRY_BIT,
        VIRTIO_BLK_F_RO_BIT,
        VIRTIO_BLK_F_BLK_SIZE_BIT,
        VIRTIO_BLK_F_TOPOLOGY_BIT
    };

    features &= support;

    return true;
}

bool virtio_blk_if_t::verify_features(virtio_base_t::feature_set_t &features)
{
    if (features[VIRTIO_BLK_F_SIZE_MAX_BIT])
        printk("virtio-blk: supports %s\n", "SIZE_MAX");
    if (features[VIRTIO_BLK_F_SEG_MAX_BIT])
        printk("virtio-blk: supports %s\n", "SEG_MAX");
    if (features[VIRTIO_BLK_F_GEOMETRY_BIT])
        printk("virtio-blk: supports %s\n", "GEOMETRY");
    if (features[VIRTIO_BLK_F_RO_BIT])
        printk("virtio-blk: supports %s\n", "RO");
    if (features[VIRTIO_BLK_F_BLK_SIZE_BIT])
        printk("virtio-blk: supports %s\n", "BLK_SIZE");
    if (features[VIRTIO_BLK_F_TOPOLOGY_BIT])
        printk("virtio-blk: supports %s\n", "TOPOLOGY");

    return true;
}

void virtio_blk_if_t::config_irq()
{
}

void virtio_blk_if_t::irq_handler(int offset)
{
    assert(offset >= 0 && offset < 2);

    if (offset == 0 || irq_range.count == 1)
        config_irq();

    if (offset == 1 || irq_range.count == 1) {
        uint32_t cpu_nr = thread_cpu_number();
        uint32_t queue_nr = cpu_nr % queue_count;

        VIRTIO_BLK_TRACE("IRQ offset=%d, cpu=%u, queue=%u\n",
                         offset, cpu_nr, queue_nr);

        per_queue[queue_nr].req_queue->recycle_used();
    }
}

errno_t virtio_blk_if_t::io(
        void *data, int64_t count, uint64_t lba,
        bool fua, virtio_blk_op_t op, iocp_t *iocp)
{
   virtio_blk_if_t::request_t *request =
           new (ext::nothrow) virtio_blk_if_t::request_t;
   request->data = data;
   request->count = count;
   request->header.lba = lba;
   request->op = op;
   request->fua = fua;
   request->caller_iocp = iocp;

   int expect = io(request);
   iocp->set_expect(expect);

   return errno_t::OK;
}

void virtio_blk_if_t::cleanup_if()
{
}

void virtio_blk_if_t::cleanup_dev()
{
}

errno_t virtio_blk_if_t::cancel_io(iocp_t *iocp)
{
    return errno_t::ENOSYS;
}

errno_t virtio_blk_if_t::read_async(void *data, int64_t count,
                                    uint64_t lba, iocp_t *iocp)
{
    return io(data, count, lba, false, virtio_blk_op_t::read, iocp);
}

errno_t virtio_blk_if_t::write_async(void const *data, int64_t count,
                                     uint64_t lba, bool fua, iocp_t *iocp)
{
    return io(const_cast<void*>(data), count, lba, fua,
              virtio_blk_op_t::write, iocp);
}

errno_t virtio_blk_if_t::flush_async(iocp_t *iocp)
{
    return io(nullptr, 0, 0, false, virtio_blk_op_t::flush, iocp);
}

errno_t virtio_blk_if_t::trim_async(int64_t count, uint64_t lba, iocp_t *iocp)
{
    return errno_t::OK;
}

long virtio_blk_if_t::info(storage_dev_info_t key)
{
    switch (key) {
    case STORAGE_INFO_BLOCKSIZE:
        return blk_config->blk_size;

    case STORAGE_INFO_BLOCKSIZE_LOG2:
        return bit_log2(blk_config->blk_size);

    case STORAGE_INFO_HAVE_TRIM:
        return 0;

    case STORAGE_INFO_NAME:
        return long("virtio-blk");

    default:
        return 0;
    }
}

static virtio_blk_factory_t virtio_blk_factory;
static virtio_blk_if_factory_t virtio_blk_if_factory;

ext::vector<storage_if_base_t *> virtio_blk_if_factory_t::detect()
{
    virtio_blk_factory.detect();

    return ext::vector<storage_if_base_t *>(
                virtio_blk_ifs.begin(), virtio_blk_ifs.end());
}
