#include "virtio-blk.h"
#include "virtio-base.h"
#include "dev_storage.h"
#include "callout.h"
#include "numeric_limits.h"

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

static virtio_blk_factory_t virtio_blk_factory;

std::vector<virtio_blk_if_t *> virtio_blk_ifs;

class virtio_blk_if_factory_t : public storage_if_factory_t {
public:
    virtio_blk_if_factory_t() : storage_if_factory_t("virtio-blk") {}
private:
    virtual std::vector<storage_if_base_t *> detect(void) override final;
};

static virtio_blk_if_factory_t virtio_blk_if_factory;
STORAGE_REGISTER_FACTORY(virtio_blk_if);

enum struct virtio_blk_op_t : uint8_t {
    read,
    write,
    trim,
    flush
};

class virtio_blk_if_t
        : public virtio_base_t
        , public storage_if_base_t
        , public storage_dev_base_t
{
public:
    using virtio_iocp_t = virtio_virtqueue_t::virtio_iocp_t;

    struct request_t {
        struct header_t {
            uint32_t type;
            uint32_t reserved;
            uint64_t lba;
        } header;

        virtio_blk_if_t *owner;
        uintptr_t tag;
        void *data;
        int64_t count;
        uint64_t lba;
        virtio_iocp_t io_iocp;
        iocp_t *caller_iocp;
        virtio_blk_op_t op;
        bool fua;

        uint8_t status;
    };

    int io(request_t *request);

private:
    STORAGE_IF_IMPL
    STORAGE_DEV_IMPL

    errno_t io(void *data, int64_t count, uint64_t lba, bool fua,
               virtio_blk_op_t op, iocp_t *iocp);

private:
    using lock_type = std::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;

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

    // virtio_base_t interface
    bool init(const pci_dev_iterator_t &pci_iter) override final;
    bool offer_features(feature_set_t &features) override final;
    bool verify_features(feature_set_t &features) override final;
    void irq_handler(int offset) override final;

    void config_irq();

    static void io_completion(const uint64_t &total_len, uintptr_t arg);

    using req_tag_t = uintptr_t;

    lock_type drive_lock;

    blk_config_t *blk_config;
    virtio_virtqueue_t *req_queue;
    uint8_t log2_sectorsize;
};

int virtio_blk_factory_t::detect()
{
    return detect_virtio(PCI_DEV_CLASS_STORAGE, VIRTIO_DEVICE_BLK,
                         "virtio-blk");
}

std::vector<storage_if_base_t *> virtio_blk_if_factory_t::detect()
{
    virtio_blk_factory.detect();

    return std::vector<storage_if_base_t *>(
                virtio_blk_ifs.begin(), virtio_blk_ifs.end());
}

std::vector<storage_dev_base_t *> virtio_blk_if_t::detect_devices()
{
    return std::vector<storage_dev_base_t *>(
                virtio_blk_ifs.begin(), virtio_blk_ifs.end());
}

virtio_base_t *virtio_blk_factory_t::create()
{
    return new virtio_blk_if_t;
}

void virtio_blk_factory_t::found_device(virtio_base_t *device)
{
    virtio_blk_ifs.push_back(static_cast<virtio_blk_if_t*>(device));
}

int virtio_blk_if_t::io(request_t *request)
{
    request->header.lba = request->lba;

    switch (request->op) {
    case virtio_blk_op_t::read:
        request->header.type = 0;
        break;

    case virtio_blk_op_t::write:
        request->header.type = 1;
        break;

    case virtio_blk_op_t::flush:
        request->header.type = 4;
        break;

    case virtio_blk_op_t::trim:
        return -1;
    }

    mmphysrange_t ranges[16];
    size_t range_count;
    char *data = (char*)request->data;
    size_t remain = request->count << log2_sectorsize;

    std::vector<virtio_virtqueue_t::desc_t*> desc_chain;

    virtio_virtqueue_t::desc_t *desc;
    virtio_virtqueue_t::desc_t *prev;

    do {
        range_count = mphysranges(ranges, countof(ranges), data, remain,
                                  blk_config->size_max);


        desc = req_queue->alloc_desc(false);

        desc->addr = mphysaddr(&request->header);
        desc->len = sizeof(request->header);
        desc_chain.push_back(desc);

        size_t batch_bytes = 0;
        for (size_t i = 0; i < range_count; ++i) {
            prev = desc;
            desc = req_queue->alloc_desc(request->op == virtio_blk_op_t::read);
            desc->addr = ranges[i].physaddr;
            desc->len = ranges[i].size;
            batch_bytes += ranges[i].size;

            prev->flags.bits.next = true;
            prev->next = req_queue->index_of(desc);
            desc_chain.push_back(desc);
        }

        data += batch_bytes;
        remain -= batch_bytes;
    } while (remain);

    prev = desc;
    desc = req_queue->alloc_desc(true);
    desc->addr = mphysaddr(&request->status);
    desc->len = sizeof(request->status);
    prev->flags.bits.next = true;
    prev->next = req_queue->index_of(desc);
    desc_chain.push_back(desc);

    request->owner = this;

    request->io_iocp.reset(&virtio_blk_if_t::io_completion,
                           uintptr_t(request));
    req_queue->enqueue_avail(desc_chain.data(), desc_chain.size(),
                             &request->io_iocp);

    return 1;
}

void virtio_blk_if_t::io_completion(
        uint64_t const& total_len, uintptr_t arg)
{
    request_t *request = reinterpret_cast<request_t*>(arg);
    request->caller_iocp->invoke();
    free(request);
}

bool virtio_blk_if_t::init(pci_dev_iterator_t const &pci_iter)
{
    if (!virtio_init(pci_iter, "virtio-blk"))
        return false;

    req_queue = &queues[0];

    blk_config = (blk_config_t*)device_cfg;
    log2_sectorsize = bit_log2(blk_config->blk_size);

    return true;
}

bool virtio_blk_if_t::offer_features(virtio_base_t::feature_set_t &features)
{
    return true;
}

bool virtio_blk_if_t::verify_features(virtio_base_t::feature_set_t &features)
{
    return true;
}

void virtio_blk_if_t::config_irq()
{
}

void virtio_blk_if_t::irq_handler(int offset)
{
    if (offset == 0 || irq_range.count == 1)
        config_irq();

    if (offset == 1 || irq_range.count == 1)
        req_queue->recycle_used();
}

errno_t virtio_blk_if_t::io(
        void *data, int64_t count, uint64_t lba,
        bool fua, virtio_blk_op_t op, iocp_t *iocp)
{
   virtio_blk_if_t::request_t *request = new virtio_blk_if_t::request_t;
   request->data = data;
   request->count = count;
   request->lba = lba;
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

    case STORAGE_INFO_HAVE_TRIM:
        return 0;

    case STORAGE_INFO_NAME:
        return long("virtio-blk");

    default:
        return 0;
    }
}

