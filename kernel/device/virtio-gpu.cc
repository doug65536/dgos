#include "virtio-gpu.h"
#include "virtio-base.h"
#include "callout.h"
#include "dev_text.h"
#include "vector.h"
#include "irq.h"
#include "pci.h"

// hack
#include "cpu/halt.h"

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
        : gpu_config(nullptr)
    {
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

    void irq_handler(int irq) override final;

    bool offer_features(feature_set_t &features) override final;

    virtio_gpu_config_t *gpu_config;
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

    printdbg("Worst case packet size: %zd bytes\n", sizeof(virtio_gpu_any_t));

    gpu_config = (virtio_gpu_config_t*)device_cfg;

    virtio_virtqueue_t::desc_t *desc[2];

    desc[0] = queues[0].alloc_desc(false);
    desc[1] = queues[0].alloc_desc(true);

    alignas(64) virtio_gpu_resource_create_2d_t create_2d;
    alignas(64) virtio_gpu_ctrl_hdr_t resp(0);

    create_2d.format = VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM;
    create_2d.width = 1440;
    create_2d.height = 900;
    create_2d.resource_id = 0;

    virtio_virtqueue_t& queue = queues[0];
    desc[0]->addr = mphysaddr(&create_2d);
    desc[0]->len = sizeof(create_2d);
    desc[0]->next = queue.index_of(desc[1]);
    desc[0]->flags.bits.next = true;
    desc[1]->addr = mphysaddr(&resp);
    desc[1]->len = sizeof(resp);
    queue.enqueue_avail(desc, countof(desc));

    return true;
}

void virtio_gpu_dev_t::irq_handler(int irq)
{

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
