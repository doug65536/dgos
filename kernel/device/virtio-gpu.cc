#include "virtio-gpu.h"
#include "virtio-base.h"
#include "callout.h"
#include "dev_text.h"
#include "vector.h"
#include "irq.h"
#include "pci.h"
#include "numeric_limits.h"
#include "work_queue.h"

#define DEBUG_VIRTIO_GPU 1
#if DEBUG_VIRTIO_GPU
#define VIRTIO_GPU_TRACE(...) printdbg("virtio-gpu: " __VA_ARGS__)
#else
#define VIRTIO_GPU_TRACE(...) ((void)0)
#endif

#define VIRTIO_DEVICE_GPU   (0x1040+16)

#define VIRTIO_F_GPU3D_BIT  0

class virtio_gpu_dev_t;

class virtio_gpu_factory_t : public virtio_factory_base_t {
public:
    ~virtio_gpu_factory_t() {}
    int detect();

protected:
    // virtio_factory_base_t interface
    virtio_base_t *create() override final;
};

static virtio_gpu_factory_t virtio_gpu_factory;

class virtio_gpu_dev_t : public virtio_base_t {
public:
    virtio_gpu_dev_t()
        : backbuf(nullptr)
        , backbuf_sz(0)
        , gpu_config(nullptr)
    {
    }

    ~virtio_gpu_dev_t()
    {
        if (backbuf_sz)
            munmap(backbuf, backbuf_sz);
    }

    bool init(pci_dev_iterator_t const &pci_iter) override final;

private:
    friend class virtio_gpu_factory_t;

    struct virtio_gpu_config_t {
        uint32_t events_read;
        uint32_t events_clear;
        uint32_t num_scanouts;
        uint32_t reserved;
    };

    enum virtio_gpu_ctrl_type_t {
        /* 2d commands */
        VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
        VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
        VIRTIO_GPU_CMD_RESOURCE_UNREF,
        VIRTIO_GPU_CMD_SET_SCANOUT,
        VIRTIO_GPU_CMD_RESOURCE_FLUSH,
        VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
        VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
        VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,

        /* cursor commands */
        VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
        VIRTIO_GPU_CMD_MOVE_CURSOR,

        /* success responses */
        VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
        VIRTIO_GPU_RESP_OK_DISPLAY_INFO,

        /* error responses */
        VIRTIO_GPU_RESP_ERR_UNSPEC = 0x1200,
        VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY,
        VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID,
        VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID,
        VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID,
        VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER,
    };

    #define VIRTIO_GPU_FLAG_FENCE       (1 << 0)
    #define VIRTIO_GPU_MAX_SCANOUTS     16

    struct virtio_gpu_ctrl_hdr_t {
        virtio_gpu_ctrl_hdr_t(uint32_t type)
            : type(type)
            , flags(0)
            , fence_id(0)
            , ctx_id(0)
            , padding(0)
        {
        }

        uint32_t type;
        uint32_t flags;
        uint64_t fence_id;
        uint32_t ctx_id;
        uint32_t padding;
    };

    struct virtio_gpu_rect_t {
        uint32_t x;
        uint32_t y;
        uint32_t width;
        uint32_t height;
    };

    struct virtio_gpu_display_one_t {
        struct virtio_gpu_rect_t r;
        uint32_t enabled;
        uint32_t flags;
    };

    struct virtio_gpu_resp_display_info_t {
        virtio_gpu_resp_display_info_t()
            : hdr(0)
        {
            memset(pmodes, 0, sizeof(pmodes));
        }

        virtio_gpu_ctrl_hdr_t hdr;
        virtio_gpu_display_one_t pmodes[VIRTIO_GPU_MAX_SCANOUTS];
    };

    enum virtio_gpu_formats_t {
        VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM = 1,
        VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM = 2,
        VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM = 3,
        VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM = 4,
        VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM = 67,
        VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM = 68,
        VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM = 121,
        VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM = 134,
    };

    struct virtio_gpu_get_display_info_t {
        virtio_gpu_get_display_info_t()
            : hdr(VIRTIO_GPU_CMD_GET_DISPLAY_INFO)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
    };

    struct virtio_gpu_resource_create_2d_t {
        virtio_gpu_resource_create_2d_t()
            : hdr(VIRTIO_GPU_CMD_RESOURCE_CREATE_2D)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t resource_id;
        uint32_t format;
        uint32_t width;
        uint32_t height;
    };

    struct virtio_gpu_resource_unref_t {
        virtio_gpu_resource_unref_t()
            : hdr(VIRTIO_GPU_CMD_RESOURCE_UNREF)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t resource_id;
        uint32_t padding;
    };

    struct virtio_gpu_set_scanout_t {
        virtio_gpu_set_scanout_t()
            : hdr(VIRTIO_GPU_CMD_SET_SCANOUT)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        virtio_gpu_rect_t r;
        uint32_t scanout_id;
        uint32_t resource_id;
    };

    struct virtio_gpu_resource_flush_t {
        virtio_gpu_resource_flush_t()
            : hdr(VIRTIO_GPU_CMD_RESOURCE_FLUSH)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        virtio_gpu_rect_t r;
        uint32_t resource_id;
        uint32_t padding;
    };

    struct virtio_gpu_transfer_to_host_2d_t {
        virtio_gpu_transfer_to_host_2d_t()
            : hdr(VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D)
            , r{}
            , offset(0)
            , resource_id(0)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        virtio_gpu_rect_t r;
        uint64_t offset;
        uint32_t resource_id;
        uint32_t padding;
    };

    struct virtio_gpu_resource_attach_backing_t {
        virtio_gpu_resource_attach_backing_t()
            : hdr(VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING)
        {
        }

        void *operator new(size_t sz, size_t entries)
        {
            return malloc(sz + sizeof(virtio_gpu_mem_entry_t) * entries);
        }

        void operator delete(void *mem)
        {
            free(mem);
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t resource_id;
        uint32_t nr_entries;
    };

    struct virtio_gpu_mem_entry_t {
        uint64_t addr;
        uint32_t length;
        uint32_t padding;
    };

    struct virtio_gpu_resource_detach_backing_t {
        virtio_gpu_resource_detach_backing_t()
            : hdr(VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t resource_id;
        uint32_t padding;
    };

    struct virtio_gpu_cursor_pos_t {
        uint32_t scanout_id;
        uint32_t x;
        uint32_t y;
        uint32_t padding;
    };

    struct virtio_gpu_update_cursor_t {
        virtio_gpu_update_cursor_t(bool move_only)
            : hdr(move_only
                  ? VIRTIO_GPU_CMD_MOVE_CURSOR
                  : VIRTIO_GPU_CMD_UPDATE_CURSOR)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        virtio_gpu_cursor_pos_t pos;
        uint32_t resource_id;
        uint32_t hot_x;
        uint32_t hot_y;
        uint32_t padding;
    };

    // Worst case packet sized union, 64 bytes
    union alignas(16) virtio_gpu_any_t {
        virtio_gpu_ctrl_hdr_t hdr;
        // This one is huge, and doesn't need to be high performance, disable
        //virtio_gpu_resp_display_info_t resp_display_info;
        virtio_gpu_get_display_info_t get_display_info;
        virtio_gpu_resource_create_2d_t resource_create_2d;
        virtio_gpu_resource_unref_t resource_unref;
        virtio_gpu_set_scanout_t set_scanout;
        virtio_gpu_resource_flush_t resource_flush;
        virtio_gpu_transfer_to_host_2d_t transfer_to_host;
        virtio_gpu_resource_attach_backing_t attach_backing;
        virtio_gpu_resource_detach_backing_t detach_backing;
        virtio_gpu_update_cursor_t update_cursor;
    };

    void irq_handler(int offset) override final;

    void config_irq();
    void queue_irq_command();
    void queue_irq_cursor();

    bool offer_features(feature_set_t &features) override final;
    bool verify_features(feature_set_t &features) override final;

    bool handle_config_change();

    uint32_t *backbuf;
    size_t backbuf_sz;
    virtio_gpu_config_t volatile *gpu_config;
};

static void virtio_gpu_startup(void*)
{
    virtio_gpu_factory.detect();
}

REGISTER_CALLOUT(virtio_gpu_startup, nullptr,
                 callout_type_t::driver_base, "000");

int virtio_gpu_factory_t::detect()
{
    return detect_virtio(PCI_DEV_CLASS_DISPLAY, VIRTIO_DEVICE_GPU,
                         "virtio-gpu");
}

virtio_base_t *virtio_gpu_factory_t::create()
{
    return new virtio_gpu_dev_t;
}

bool virtio_gpu_dev_t::init(pci_dev_iterator_t const &pci_iter)
{
    if (!virtio_init(pci_iter, "virtio-gpu"))
        return false;

    gpu_config = (virtio_gpu_config_t*)device_cfg;

    if (!handle_config_change())
        return false;

    return true;
}

bool virtio_gpu_dev_t::handle_config_change()
{
    virtio_virtqueue_t& queue = queues[0];

    virtio_virtqueue_t::virtio_blocking_iocp_t iocp;

    // Request screen dimensions
    virtio_gpu_get_display_info_t get_display_info;
    virtio_gpu_resp_display_info_t display_info_resp;

    queue.sendrecv(&get_display_info, sizeof(get_display_info),
                   &display_info_resp, sizeof(display_info_resp), &iocp);
    iocp.wait();

    if (display_info_resp.hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO)
        return false;

    int scrn_w = display_info_resp.pmodes[0].r.width;
    int scrn_h = display_info_resp.pmodes[0].r.height;

    virtio_gpu_resource_create_2d_t create_2d;
    virtio_gpu_ctrl_hdr_t resp(0);

    create_2d.format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    create_2d.width = scrn_w;
    create_2d.height = scrn_h;
    create_2d.resource_id = 1;

    iocp.reset();
    queue.sendrecv(&create_2d, sizeof(create_2d), &resp, sizeof(resp), &iocp);
    iocp.wait();

    if (resp.type != VIRTIO_GPU_RESP_OK_NODATA)
        return false;

    if (backbuf_sz)
        munmap(backbuf, backbuf_sz);

    // Allocate backbuffer
    backbuf_sz = scrn_w * scrn_h * sizeof(uint32_t);
    backbuf = (uint32_t*)mmap(nullptr, backbuf_sz, PROT_READ | PROT_WRITE,
                              MAP_POPULATE, -1, 0);

    // Calculate worst case physical range list size
    size_t backbuf_ranges_cap = (backbuf_sz + PAGE_SIZE - 1) >> PAGE_SCALE;

    // Allocate worst case physical range list
    std::unique_ptr<mmphysrange_t[]> backbuf_ranges(
                new mmphysrange_t[backbuf_ranges_cap]);

    // Get physical ranges
    size_t backbuf_range_count = mphysranges(
                backbuf_ranges, backbuf_ranges_cap, backbuf, backbuf_sz,
                std::numeric_limits<uint32_t>::max());

    // Calculate size of attach backing command
    size_t backing_cmd_sz = sizeof(virtio_gpu_resource_attach_backing_t) +
            sizeof(virtio_gpu_mem_entry_t) * backbuf_range_count;

    // Allocate a buffer to hold virtio_gpu_resource_attach_backing_t command
    std::unique_ptr<virtio_gpu_resource_attach_backing_t> backing_cmd(
                new (backbuf_range_count) virtio_gpu_resource_attach_backing_t);

    backing_cmd->resource_id = 1;
    backing_cmd->nr_entries = backbuf_range_count;

    virtio_gpu_mem_entry_t *mem_entries =
            (virtio_gpu_mem_entry_t *)(backing_cmd.get() + 1);

    for (size_t i = 0; i < backbuf_range_count; ++i) {
        mem_entries[i].addr = backbuf_ranges[i].physaddr;
        mem_entries[i].length = backbuf_ranges[i].size;
    }

    iocp.reset();
    queue.sendrecv(backing_cmd.get(), backing_cmd_sz,
                   &resp, sizeof(resp), &iocp);
    iocp.wait();

    if (resp.type != VIRTIO_GPU_RESP_OK_NODATA)
        return false;

    // Fill the backbuffer
    memset32_nt(backbuf, 0x123456, backbuf_sz);

    virtio_gpu_set_scanout_t set_scanout;

    set_scanout.r.x = 0;
    set_scanout.r.y = 0;
    set_scanout.r.width = scrn_w;
    set_scanout.r.height = scrn_h;
    set_scanout.resource_id = 1;
    set_scanout.scanout_id = 0;

    iocp.reset();
    queue.sendrecv(&set_scanout, sizeof(set_scanout),
                   &resp, sizeof(resp), &iocp);
    iocp.wait();

    if (resp.type != VIRTIO_GPU_RESP_OK_NODATA)
        return false;

    virtio_gpu_transfer_to_host_2d_t xfer;

    xfer.r.x = 0;
    xfer.r.y = 0;
    xfer.r.width = scrn_w;
    xfer.r.height = scrn_h;
    xfer.resource_id = 1;

    iocp.reset();
    queue.sendrecv(&xfer, sizeof(xfer), &resp, sizeof(resp), &iocp);
    iocp.wait();

    if (resp.type != VIRTIO_GPU_RESP_OK_NODATA)
        return false;

    virtio_gpu_resource_flush_t flush;

    flush.r.x = 0;
    flush.r.y = 0;
    flush.r.width = scrn_w;
    flush.r.height = scrn_h;
    flush.resource_id = 1;

    iocp.reset();
    queue.sendrecv(&flush, sizeof(flush), &resp, sizeof(resp), &iocp);
    iocp.wait();

    if (resp.type != VIRTIO_GPU_RESP_OK_NODATA)
        return false;

    return true;
}

void virtio_gpu_dev_t::config_irq()
{
    int events = gpu_config->events_read;

    if (events & 1) {
        workq::enqueue([this] {
            handle_config_change();
        });
    }

    // Acknowledge
    if (events)
        gpu_config->events_clear = events;
}

void virtio_gpu_dev_t::queue_irq_command()
{
    virtio_virtqueue_t& queue = queues[0];
    queue.recycle_used();
}

void virtio_gpu_dev_t::queue_irq_cursor()
{

}

void virtio_gpu_dev_t::irq_handler(int offset)
{
    if (offset == 0 || irq_range.count == 1)
        config_irq();

    if (offset == 1 || irq_range.count == 1)
        queue_irq_command();

    if (offset == 2 || irq_range.count == 1)
        queue_irq_cursor();
}

bool virtio_gpu_dev_t::offer_features(feature_set_t &features)
{
    if (features[VIRTIO_F_GPU3D_BIT])
        printk("virtio: 3D supported\n");

    feature_set_t supported(std::initializer_list<int>{
                                VIRTIO_F_VERSION_1_BIT,
                                VIRTIO_F_INDIRECT_DESC_BIT,
                                VIRTIO_F_RING_EVENT_IDX_BIT,
                                VIRTIO_F_GPU3D_BIT
                            });

    features &= supported;

    return true;
}

bool virtio_gpu_dev_t::verify_features(virtio_base_t::feature_set_t &features)
{
    if (features[VIRTIO_F_GPU3D_BIT])
        printk("virtio: 3D enabled\n");

    if (features[VIRTIO_F_RING_EVENT_IDX_BIT])
        printk("virtio: ring event idx enabled\n");
    else
        return false;

    if (features[VIRTIO_F_INDIRECT_DESC_BIT])
        printk("virtio: indirect descriptors enabled\n");

    if (features[VIRTIO_F_VERSION_1_BIT])
        printk("virtio: version 1 enabled\n");
    else
        return false;

    return true;
}
