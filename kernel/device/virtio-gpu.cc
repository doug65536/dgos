#include "virtio-gpu.h"
#include "virtio-base.h"
#include "callout.h"
#include "dev_text.h"
#include "vector.h"
#include "irq.h"
#include "pci.h"
#include "numeric_limits.h"
#include "work_queue.h"
#include "heap.h"
#include "conio.h"
#include "framebuffer.h"
#include "vesainfo.h"
#include "threadsync.h"

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
        , cmd_queue(nullptr)
        , crsr_queue(nullptr)
        , scrn_w(0)
        , scrn_h(0)
        , configured(false)
    {
    }

    ~virtio_gpu_dev_t()
    {
        if (backbuf_sz)
            munmap(backbuf, backbuf_sz);
    }

    bool init(pci_dev_iterator_t const &pci_iter) override final;

    void reattach_framebuffer();

private:
    friend class virtio_gpu_factory_t;

    struct virtio_gpu_config_t {
        uint32_t events_read;
        uint32_t events_clear;
        uint32_t num_scanouts;
        uint32_t reserved;
    };

    //
    // 2D commands

    enum virtio_gpu_ctrl_type_t {
        VIRTIO_GPU_UNDEFINED = 0,

        // 2D commands
        VIRTIO_GPU_CMD_GET_DISPLAY_INFO = 0x0100,
        VIRTIO_GPU_CMD_RESOURCE_CREATE_2D,
        VIRTIO_GPU_CMD_RESOURCE_UNREF,
        VIRTIO_GPU_CMD_SET_SCANOUT,
        VIRTIO_GPU_CMD_RESOURCE_FLUSH,
        VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D,
        VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING,
        VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING,

        // 3D commands
        VIRTIO_GPU_CMD_CTX_CREATE = 0x0200,
        VIRTIO_GPU_CMD_CTX_DESTROY,
        VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE,
        VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE,
        VIRTIO_GPU_CMD_RESOURCE_CREATE_3D,
        VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D,
        VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D,
        VIRTIO_GPU_CMD_SUBMIT_3D,

        // Cursor commands
        VIRTIO_GPU_CMD_UPDATE_CURSOR = 0x0300,
        VIRTIO_GPU_CMD_MOVE_CURSOR,

        // Success responses
        VIRTIO_GPU_RESP_OK_NODATA = 0x1100,
        VIRTIO_GPU_RESP_OK_DISPLAY_INFO,

        // Error responses
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
        {
        }

        uint32_t type;
        uint32_t flags = 0;
        uint64_t fence_id = 0;
        uint32_t ctx_id = 0;
        uint32_t padding = 0;
    };

    struct virtio_gpu_rect_t {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct virtio_gpu_display_one_t {
        struct virtio_gpu_rect_t r;
        uint32_t enabled = 0;
        uint32_t flags = 0;
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
        uint32_t resource_id = 0;
        uint32_t format = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct virtio_gpu_resource_unref_t {
        virtio_gpu_resource_unref_t()
            : hdr(VIRTIO_GPU_CMD_RESOURCE_UNREF)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t resource_id = 0;
        uint32_t padding = 0;
    };

    struct virtio_gpu_set_scanout_t {
        virtio_gpu_set_scanout_t()
            : hdr(VIRTIO_GPU_CMD_SET_SCANOUT)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        virtio_gpu_rect_t r;
        uint32_t scanout_id = 0;
        uint32_t resource_id = 0;
    };

    struct virtio_gpu_resource_flush_t {
        virtio_gpu_resource_flush_t()
            : hdr(VIRTIO_GPU_CMD_RESOURCE_FLUSH)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        virtio_gpu_rect_t r;
        uint32_t resource_id = 0;
        uint32_t padding = 0;
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
        uint64_t offset = 0;
        uint32_t resource_id = 0;
        uint32_t padding = 0;
    };

    struct virtio_gpu_resource_attach_backing_t {
        virtio_gpu_resource_attach_backing_t()
            : hdr(VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING)
        {
        }

        void *operator new(size_t sz, size_t entries,
                           std::nothrow_t const&) noexcept
        {
            return malloc(sz + sizeof(virtio_gpu_mem_entry_t) * entries);
        }

        void operator delete(void *mem) noexcept
        {
            free(mem);
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t resource_id = 0;
        uint32_t nr_entries = 0;
    };

    struct virtio_gpu_mem_entry_t {
        uint64_t addr = 0;
        uint32_t length = 0;
        uint32_t padding = 0;
    };

    struct virtio_gpu_resource_detach_backing_t {
        virtio_gpu_resource_detach_backing_t()
            : hdr(VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t resource_id = 0;
        uint32_t padding = 0;
    };

    struct virtio_gpu_cursor_pos_t {
        uint32_t scanout_id = 0;
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t padding = 0;
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
        uint32_t resource_id = 0;
        uint32_t hot_x = 0;
        uint32_t hot_y = 0;
        uint32_t padding = 0;
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

    //
    // 3D commands

    struct virtio_gpu_ctx_create_t {
        virtio_gpu_ctx_create_t()
            : hdr(VIRTIO_GPU_CMD_CTX_CREATE)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t nlen = 0;
        uint32_t padding = 0;
        char debug_name[64] = {};
    };

    struct virtio_gpu_ctx_destroy_t {
        virtio_gpu_ctx_destroy_t()
            : hdr(VIRTIO_GPU_CMD_CTX_DESTROY)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
    };

    struct virtio_gpu_ctx_attach_resource_t {
        virtio_gpu_ctx_attach_resource_t()
            : hdr(VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t resource_id = 0;
        uint32_t padding = 0;
    };

    struct virtio_gpu_ctx_detach_resource_t {
        virtio_gpu_ctx_detach_resource_t()
            : hdr(VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t resource_id = 0;
        uint32_t padding = 0;
    };

    struct virtio_gpu_resource_create_3d_t {
        virtio_gpu_resource_create_3d_t()
            : hdr(VIRTIO_GPU_CMD_RESOURCE_CREATE_3D)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t resource_id = 0;
        uint32_t target = 0;
        uint32_t format = 0;
        uint32_t bind = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 0;
        uint32_t array_size = 0;
        uint32_t last_level = 0;
        uint32_t nr_samples = 0;
        uint32_t flags = 0;
        uint32_t padding = 0;
    };

    struct virtio_gpu_box_t {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t z = 0;
        uint32_t w = 0;
        uint32_t h = 0;
        uint32_t d = 0;
    };

    struct virtio_gpu_transfer_tofrom_host_3d_t {
        virtio_gpu_transfer_tofrom_host_3d_t(bool from_host)
            : hdr(from_host ? VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D
                            : VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        virtio_gpu_box_t box;
        uint64_t offset = 0;
        uint32_t resource_id = 0;
        uint32_t level = 0;
        uint32_t stride = 0;
        uint32_t layer_stride = 0;
    };

    struct virtio_gpu_cmd_submit_3d_t {
        virtio_gpu_cmd_submit_3d_t()
            : hdr(VIRTIO_GPU_CMD_SUBMIT_3D)
        {
        }

        virtio_gpu_ctrl_hdr_t hdr;
        uint32_t size = 0;
        uint32_t padding = 0;
    };

    union clear_type_t;

    struct cmd_list_t {
        std::vector<uint32_t> data;

        void add_clear(clear_type_t clear_type,
                       uint32_t r, uint32_t g, uint32_t b, uint32_t a,
                       uint64_t depth_f64, uint32_t stencil)
        {
            data.push_back(8 * sizeof(uint32_t));
            data.push_back(VIRGL_CCMD_CLEAR);
            data.push_back(clear_type.raw);
            data.push_back(r);
            data.push_back(g);
            data.push_back(b);
            data.push_back(a);
            data.push_back(uint32_t(depth_f64));
            data.push_back(uint32_t(depth_f64 >> 32));
            data.push_back(stencil);
        }

        void add_draw_vbo(uint32_t start, uint32_t count,
                          uint32_t mode, uint32_t indexed,
                          uint32_t instance_count, uint32_t index_bias,
                          uint32_t start_inst, uint32_t primitive_restart,
                          uint32_t restart_index, uint32_t min_index,
                          uint32_t max_index, uint32_t cso)
        {
            data.push_back(12 * sizeof(uint32_t));
            data.push_back(VIRGL_CCMD_DRAW_VBO);
            data.push_back(start);
            data.push_back(count);
            data.push_back(mode);
            data.push_back(indexed);
            data.push_back(instance_count);
            data.push_back(index_bias);
            data.push_back(start_inst);
            data.push_back(primitive_restart);
            data.push_back(restart_index);
            data.push_back(min_index);
            data.push_back(max_index);
            data.push_back(cso);
        }
    };

    union clear_type_t {
        struct bits_t {
            bool depth :1;
            bool stencil:1;
            bool color0:1;
            bool color1:1;
            bool color2:1;
            bool color3:1;
            bool color4:1;
            bool color5:1;
            bool color6:1;
            bool color7:1;
            unsigned unused1:6;
            unsigned unused2:16;
        } bits;
        uint32_t raw;
    };

    static_assert(sizeof(clear_type_t::bits_t) == sizeof(uint32_t),
                  "bitfield problem");

    enum : uint32_t {
       VIRGL_CCMD_NOP = 0,
       VIRGL_CCMD_CREATE_OBJECT = 1,
       VIRGL_CCMD_BIND_OBJECT,
       VIRGL_CCMD_DESTROY_OBJECT,
       VIRGL_CCMD_SET_VIEWPORT_STATE,
       VIRGL_CCMD_SET_FRAMEBUFFER_STATE,
       VIRGL_CCMD_SET_VERTEX_BUFFERS,
       VIRGL_CCMD_CLEAR,
       VIRGL_CCMD_DRAW_VBO,
       VIRGL_CCMD_RESOURCE_INLINE_WRITE,
       VIRGL_CCMD_SET_SAMPLER_VIEWS,
       VIRGL_CCMD_SET_INDEX_BUFFER,
       VIRGL_CCMD_SET_CONSTANT_BUFFER,
       VIRGL_CCMD_SET_STENCIL_REF,
       VIRGL_CCMD_SET_BLEND_COLOR,
       VIRGL_CCMD_SET_SCISSOR_STATE,
       VIRGL_CCMD_BLIT,
       VIRGL_CCMD_RESOURCE_COPY_REGION,
       VIRGL_CCMD_BIND_SAMPLER_STATES,
       VIRGL_CCMD_BEGIN_QUERY,
       VIRGL_CCMD_END_QUERY,
       VIRGL_CCMD_GET_QUERY_RESULT,
       VIRGL_CCMD_SET_POLYGON_STIPPLE,
       VIRGL_CCMD_SET_CLIP_STATE,
       VIRGL_CCMD_SET_SAMPLE_MASK,
       VIRGL_CCMD_SET_STREAMOUT_TARGETS,
       VIRGL_CCMD_SET_RENDER_CONDITION,
       VIRGL_CCMD_SET_UNIFORM_BUFFER,

       VIRGL_CCMD_SET_SUB_CTX,
       VIRGL_CCMD_CREATE_SUB_CTX,
       VIRGL_CCMD_DESTROY_SUB_CTX,
       VIRGL_CCMD_BIND_SHADER
    };

    enum : uint32_t {
        VIRGL_RES_BIND_DEPTH_STENCIL   = (1U << 0),
        VIRGL_RES_BIND_RENDER_TARGET   = (1U << 1),
        VIRGL_RES_BIND_SAMPLER_VIEW    = (1U << 3),
        VIRGL_RES_BIND_VERTEX_BUFFER   = (1U << 4),
        VIRGL_RES_BIND_INDEX_BUFFER    = (1U << 5),
        VIRGL_RES_BIND_CONSTANT_BUFFER = (1U << 6),
        VIRGL_RES_BIND_STREAM_OUTPUT   = (1U << 11),
        VIRGL_RES_BIND_CURSOR          = (1U << 16),
        VIRGL_RES_BIND_CUSTOM          = (1U << 17)
    };

    void irq_handler(int offset) override final;

    void config_irq();
    void queue_irq_command();
    void queue_irq_cursor();

    bool offer_features(feature_set_t &features) override final;
    bool verify_features(feature_set_t &features) override final;

    bool handle_config_change(int events);
    bool resize_backing(uint32_t new_w, uint32_t new_h,
                        uint32_t old_w, uint32_t old_h);

    bool issue_get_display_info();
    bool issue_create_2d(uint32_t resource_id, uint32_t width, uint32_t height);
    bool issue_attach_backing(uint32_t resource_id,
                              void *backbuf, uint32_t backbuf_sz);
    bool issue_set_scanout(uint32_t scanout_id, uint32_t resource_id);
    bool issue_transfer_to_host_2d(uint32_t x, uint32_t y,
                                   uint32_t width, uint32_t height);
    bool issue_flush(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    bool issue_detach_backing(uint32_t resource_id);
    bool issue_resource_unref(uint32_t resource_id);
    bool issue_create_render_target(
            uint32_t ctx_id, uint32_t resource_id, uint32_t format,
            uint32_t width, uint32_t height, uint32_t nr_samples);

    uint32_t *backbuf;
    size_t backbuf_sz;
    virtio_gpu_config_t volatile *gpu_config;
    virtio_virtqueue_t *cmd_queue;
    virtio_virtqueue_t *crsr_queue;
    uint32_t scrn_w;
    uint32_t scrn_h;
    bool configured;

    // Config change worker thread
    thread_t config_worker_tid = 0;
    unsigned config_current = 0;
    unsigned config_latest = 0;
    uint32_t config_events = 0;
    lock_type config_lock;
    std::condition_variable config_changed;

    _noreturn
    static int config_work_thread(void *arg);

    _noreturn
    int config_work_thread();
};

static void virtio_gpu_startup(void*)
{
    virtio_gpu_factory.detect();
}

//REGISTER_CALLOUT(virtio_gpu_startup, nullptr,
//                 callout_type_t::driver_base, "000");

int virtio_gpu_factory_t::detect()
{
    // hack, disabled for now
    //return 0;
    return detect_virtio(PCI_DEV_CLASS_DISPLAY, VIRTIO_DEVICE_GPU,
                         "virtio-gpu");
}

virtio_base_t *virtio_gpu_factory_t::create()
{
    return new (std::nothrow) virtio_gpu_dev_t;
}

void virtio_gpu_dev_t::reattach_framebuffer()
{
    vbe_selected_mode_t new_mode{};
    new_mode.mode_num = 0;
    new_mode.width = scrn_w;
    new_mode.height = scrn_h;
    new_mode.framebuffer_addr = uint64_t(backbuf);
    new_mode.framebuffer_bytes = sizeof(uint32_t) * scrn_w * scrn_h;
    new_mode.mask_pos_r = 0;
    new_mode.mask_pos_g = 8;
    new_mode.mask_pos_b = 16;
    new_mode.mask_pos_a = 24;
    new_mode.mask_size_r = 8;
    new_mode.mask_size_g = 8;
    new_mode.mask_size_b = 8;
    new_mode.mask_size_a = 8;
    new_mode.bpp = 32;
    new_mode.byte_pp = sizeof(uint32_t);
    new_mode.pitch = scrn_w * sizeof(uint32_t);

    fb_change_backing(new_mode);
}

bool virtio_gpu_dev_t::init(pci_dev_iterator_t const &pci_iter)
{
    if (!virtio_init(pci_iter, "virtio-gpu"))
        return false;

    cmd_queue = &queues[0];
    crsr_queue = &queues[1];

    gpu_config = (virtio_gpu_config_t*)device_cfg;

    config_worker_tid = thread_create(config_work_thread, this, 0, false);

    uint32_t events = atomic_ld_acq(&gpu_config->events_read);
    if (events & 1) {
        if (!handle_config_change(events))
            return false;

        atomic_st_rel(&gpu_config->events_clear, events);
    } else {
        issue_get_display_info();
        handle_config_change(0);
    }

    return true;
}

bool virtio_gpu_dev_t::issue_get_display_info()
{
    virtio_gpu_get_display_info_t get_display_info{};
    virtio_gpu_resp_display_info_t display_info_resp{};
    blocking_iocp_t iocp;

    VIRTIO_GPU_TRACE("Issuing get display info...\n");

    cmd_queue->sendrecv(&get_display_info, sizeof(get_display_info),
                        &display_info_resp, sizeof(display_info_resp), &iocp);

    if (!iocp.wait_until(std::chrono::steady_clock::now() +
                         std::chrono::seconds(5))) {
        VIRTIO_GPU_TRACE("...timed out!\n");
        return false;
    }

    VIRTIO_GPU_TRACE("...get display info completed\n");

    if (display_info_resp.hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO)
        return false;

    scrn_w = display_info_resp.pmodes[0].r.width;
    scrn_h = display_info_resp.pmodes[0].r.height;

    return true;
}

bool virtio_gpu_dev_t::issue_create_2d(
        uint32_t resource_id, uint32_t width, uint32_t height)
{
    virtio_gpu_resource_create_2d_t create_2d{};
    virtio_gpu_ctrl_hdr_t resp(0);
    blocking_iocp_t iocp;

    create_2d.resource_id = resource_id;
    create_2d.format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    create_2d.width = width;
    create_2d.height = height;

    VIRTIO_GPU_TRACE("Issuing create 2d...\n");

    cmd_queue->sendrecv(&create_2d, sizeof(create_2d),
                        &resp, sizeof(resp), &iocp);
    iocp.wait();

    VIRTIO_GPU_TRACE("...create 2d completed\n");

    if (unlikely(resp.type != VIRTIO_GPU_RESP_OK_NODATA))
        return false;

    return true;
}

bool virtio_gpu_dev_t::issue_detach_backing(uint32_t resource_id)
{
    blocking_iocp_t iocp;
    virtio_gpu_ctrl_hdr_t resp(0);

    virtio_gpu_resource_detach_backing_t detach_backing{};
    detach_backing.resource_id = resource_id;

    VIRTIO_GPU_TRACE("Issuing detach backing...\n");

    cmd_queue->sendrecv(&detach_backing, sizeof(detach_backing),
                        &resp, sizeof(resp), &iocp);
    iocp.wait();

    VIRTIO_GPU_TRACE("...detach backing completed\n");

    if (unlikely(resp.type != VIRTIO_GPU_RESP_OK_NODATA))
        return false;

    return true;
}

bool virtio_gpu_dev_t::issue_resource_unref(uint32_t resource_id)
{
    virtio_gpu_resource_unref_t unref{};
    virtio_gpu_ctrl_hdr_t resp(0);
    blocking_iocp_t iocp;

    unref.resource_id = resource_id;

    VIRTIO_GPU_TRACE("Issuing resource unref,,,\n");

    cmd_queue->sendrecv(&unref, sizeof(unref), &resp, sizeof(resp), &iocp);
    iocp.wait();

    VIRTIO_GPU_TRACE("...resource unred completed\n");

    if (unlikely(resp.type != VIRTIO_GPU_RESP_OK_NODATA))
        return false;

    return true;
}

bool virtio_gpu_dev_t::issue_create_render_target(
        uint32_t ctx_id, uint32_t resource_id, uint32_t format,
        uint32_t width, uint32_t height, uint32_t nr_samples)
{
    virtio_gpu_resource_create_2d_t create_3d{};
    virtio_gpu_ctrl_hdr_t resp(0);
    blocking_iocp_t iocp;

    create_3d.hdr.ctx_id = ctx_id;
    //create_3d.bind = VIRGL_RES_BIND_RENDER_TARGET;
    create_3d.format = format;
    create_3d.resource_id = resource_id;
    create_3d.width = width;
    create_3d.height = height;
    //create_3d.nr_samples = nr_samples;

    VIRTIO_GPU_TRACE("Issuing create 3d...\n");

    cmd_queue->sendrecv(&create_3d, sizeof(create_3d),
                        &resp, sizeof(resp), &iocp);

    VIRTIO_GPU_TRACE("...create 3d completed\n");

    if (unlikely(resp.type != VIRTIO_GPU_RESP_OK_NODATA))
        return false;

    return true;
}

bool virtio_gpu_dev_t::issue_attach_backing(
        uint32_t resource_id, void *backbuf, uint32_t backbuf_sz)
{
    blocking_iocp_t iocp;
    virtio_gpu_ctrl_hdr_t resp(0);

    // Calculate worst case physical range list size
    size_t backbuf_ranges_cap = (backbuf_sz + PAGE_SIZE - 1) >> PAGE_SCALE;

    // Allocate worst case physical range list
    std::unique_ptr<mmphysrange_t[]> backbuf_ranges(
                new (std::nothrow) mmphysrange_t[backbuf_ranges_cap]);

    // Get physical ranges
    size_t backbuf_range_count = mphysranges(
                backbuf_ranges, backbuf_ranges_cap, backbuf, backbuf_sz,
                std::numeric_limits<uint32_t>::max());

    // Calculate size of attach backing command
    size_t backing_cmd_sz = sizeof(virtio_gpu_resource_attach_backing_t) +
            sizeof(virtio_gpu_mem_entry_t) * backbuf_range_count;

    // Allocate a buffer to hold virtio_gpu_resource_attach_backing_t command
    std::unique_ptr<virtio_gpu_resource_attach_backing_t> backing_cmd(
                new (backbuf_range_count, std::nothrow)
                virtio_gpu_resource_attach_backing_t);

    backing_cmd->resource_id = resource_id;
    backing_cmd->nr_entries = backbuf_range_count;

    virtio_gpu_mem_entry_t *mem_entries =
            (virtio_gpu_mem_entry_t *)(backing_cmd.get() + 1);

    for (size_t i = 0; i < backbuf_range_count; ++i) {
        mem_entries[i].addr = backbuf_ranges[i].physaddr;
        mem_entries[i].length = backbuf_ranges[i].size;
    }

    VIRTIO_GPU_TRACE("Issuing attach backing...\n");

    cmd_queue->sendrecv(backing_cmd.get(), backing_cmd_sz,
                        &resp, sizeof(resp), &iocp);
    iocp.wait();

    VIRTIO_GPU_TRACE("...attach backing complete\n");

    if (unlikely(resp.type != VIRTIO_GPU_RESP_OK_NODATA))
        return false;

    return true;
}

bool virtio_gpu_dev_t::issue_set_scanout(
        uint32_t scanout_id, uint32_t resource_id)
{
    blocking_iocp_t iocp;
    virtio_gpu_ctrl_hdr_t resp(0);

    virtio_gpu_set_scanout_t set_scanout{};

    set_scanout.r.x = 0;
    set_scanout.r.y = 0;
    set_scanout.r.width = scrn_w;
    set_scanout.r.height = scrn_h;
    set_scanout.resource_id = 1;
    set_scanout.scanout_id = 0;

    VIRTIO_GPU_TRACE("Issuing set scanout...\n");

    cmd_queue->sendrecv(&set_scanout, sizeof(set_scanout),
                        &resp, sizeof(resp), &iocp);
    iocp.wait();

    VIRTIO_GPU_TRACE("...set scanout completed\n");

    if (unlikely(resp.type != VIRTIO_GPU_RESP_OK_NODATA))
        return false;

    return true;
}

bool virtio_gpu_dev_t::issue_transfer_to_host_2d(
        uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    virtio_gpu_transfer_to_host_2d_t xfer{};
    virtio_gpu_ctrl_hdr_t resp(0);
    blocking_iocp_t iocp;

    xfer.r.x = x;
    xfer.r.y = y;
    xfer.r.width = width;
    xfer.r.height = height;
    xfer.resource_id = 1;

    VIRTIO_GPU_TRACE("Issuing transfer to host 2d...\n");

    cmd_queue->sendrecv(&xfer, sizeof(xfer), &resp, sizeof(resp), &iocp);
    iocp.wait();

    VIRTIO_GPU_TRACE("...transfer to host 2d completed\n");

    if (unlikely(resp.type != VIRTIO_GPU_RESP_OK_NODATA))
        return false;

    return true;
}

bool virtio_gpu_dev_t::issue_flush(
        uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    virtio_gpu_resource_flush_t flush{};
    virtio_gpu_ctrl_hdr_t resp(0);
    blocking_iocp_t iocp;

    flush.r.x = x;
    flush.r.y = y;
    flush.r.width = width;
    flush.r.height = height;
    flush.resource_id = 1;

    VIRTIO_GPU_TRACE("Issuing flush...\n");

    cmd_queue->sendrecv(&flush, sizeof(flush), &resp, sizeof(resp), &iocp);
    iocp.wait();

    VIRTIO_GPU_TRACE("...flush completed\n");

    if (unlikely(resp.type != VIRTIO_GPU_RESP_OK_NODATA))
        return false;

    return true;
}

bool virtio_gpu_dev_t::handle_config_change(int events)
{
    if (configured) {
        if (!issue_detach_backing(1))
            return false;

        if (!issue_resource_unref(1))
            return false;

        configured = false;
    }

    uint32_t old_w = scrn_w;
    uint32_t old_h = scrn_h;

    VIRTIO_GPU_TRACE("Getting display info\n");
    // Request screen dimensions
    if (!issue_get_display_info())
        return false;
    VIRTIO_GPU_TRACE("...W=%u, H=%u\n", scrn_w, scrn_h);

    if (!issue_create_2d(1, scrn_w, scrn_h))
        return false;

    resize_backing(scrn_w, scrn_h, old_w, old_h);

    // Fill the backbuffer
    memset32_nt(backbuf, 0x123456, backbuf_sz);

    if (!issue_attach_backing(1, backbuf, backbuf_sz))
        return false;

    if (!issue_set_scanout(0, 1))
        return false;

    if (!issue_transfer_to_host_2d(0, 0, scrn_w, scrn_h))
        return false;

    if (!issue_flush(0, 0, scrn_w, scrn_h))
        return false;

    VIRTIO_GPU_TRACE("Reattaching framebuffer\n");
    reattach_framebuffer();

    VIRTIO_GPU_TRACE("Config change complete\n");

    configured = true;

    return true;
}

bool virtio_gpu_dev_t::resize_backing(uint32_t new_w, uint32_t new_h,
                                      uint32_t old_w, uint32_t old_h)
{
    uint32_t st;
    uint32_t en;
    intptr_t dir;
    uint32_t row_sz;
    uint32_t clear_sz;
    uint32_t new_sz = new_w * new_h * sizeof(uint32_t);

    if (new_w < old_w) {
        // The width was reduced, scan forward and pull the scanlines up
        st = 1;

        // Process lesser of new and old height
        if (old_h < new_h)
            en = old_h ? old_h - 1 : 0;
        else
            en = new_h ? new_h - 1 : 0;

        dir = 1;
        row_sz = new_w * sizeof(uint32_t);
        clear_sz = 0;
    } else if (new_w > old_w) {
        // Need to make backbuffer bigger before resizing
        uint32_t *new_backbuf;
        if (backbuf != nullptr) {
            new_backbuf = (uint32_t*)mremap(backbuf, backbuf_sz, new_sz,
                                            MREMAP_MAYMOVE);
        } else {
            new_backbuf = (uint32_t*)mmap(nullptr, new_sz,
                                          PROT_READ | PROT_WRITE,
                                          MAP_POPULATE, -1, 0);
        }
        if (unlikely(new_backbuf == MAP_FAILED))
            return false;

        backbuf = new_backbuf;

        // The width was increased, scan backward and pull the scanlines down
        en = 0;

        // Process lesser of new and old height
        if (old_h < new_h)
            st = old_h ? old_h - 1 : 0;
        else
            st = new_h ? new_h - 1 : 0;

        dir = -1;
        row_sz = old_w * sizeof(uint32_t);
        clear_sz = (new_w - old_w) * sizeof(uint32_t);
    } else {
        // Do nothing
        return true;
    }

    for (uint32_t row = st; row != en; row += dir) {
        uint32_t *src = backbuf + old_w * row;
        uint32_t *dst = backbuf + new_w * row;
        memmove(dst, src, row_sz);
        memset(dst + old_w, 0, clear_sz);
    }

    if (new_w < old_w || new_h < old_h) {
        // Need to make backbuffer smaller after resizing
        uint32_t *new_backbuf;
        new_backbuf = (uint32_t*)mremap(backbuf, backbuf_sz, new_sz,
                                        MREMAP_MAYMOVE);
        if (unlikely(new_backbuf == MAP_FAILED))
            return false;

        backbuf = new_backbuf;
    }

    row_sz = new_w * sizeof(uint32_t);

    for (uint32_t row = old_h; row < new_h; ++row) {
        uint32_t *dst = backbuf + row * new_w;
        memset(dst, 0, row_sz);
    }

    backbuf_sz = new_sz;
    scrn_w = new_w;
    scrn_h = new_h;

    return true;
}

void virtio_gpu_dev_t::config_irq()
{
    int events = gpu_config->events_read;

    if (events & 1) {
        gpu_config->events_clear = events;

        // Register
        scoped_lock lock(config_lock);

        // Remember config changes
        config_events = events;

        // Bump config version
        ++config_current;

        lock.unlock();

        // Kick config change thread
        config_changed.notify_all();
    }
}

void virtio_gpu_dev_t::irq_handler(int offset)
{
    if (use_msi && irq_range.count >= 3) {
        if (offset == 0 || irq_range.count == 1)
            config_irq();

        if (offset == 1 || irq_range.count == 1)
            cmd_queue->recycle_used();

        if (offset == 2 || irq_range.count == 1)
            crsr_queue->recycle_used();
    } else {
        uint32_t status = *isr_status;

        if (status & 1) {
            cmd_queue->recycle_used();
            crsr_queue->recycle_used();
        }
        if (status & 2) {
            config_irq();
        }
    }
}

int virtio_gpu_dev_t::config_work_thread(void *arg)
{
    reinterpret_cast<virtio_gpu_dev_t*>(arg)->config_work_thread();
}

int virtio_gpu_dev_t::config_work_thread()
{
    // Must disable interrupts here to prevent an IRQ occurring and that
    // ISR also acquiring the config lock. It would deadlock. In practice IRQs
    // will be enabled very soon because .wait will context switch to
    // something that probably has interrupts enabled
    cpu_scoped_irq_disable irq_dis;
    scoped_lock lock(config_lock);

    while (true)
    {
        while (config_latest == config_current)
            config_changed.wait(lock);

        uint32_t events = config_events;
        config_events = 0;

        lock.unlock();
        irq_dis.restore();

        // Handle config change with interrupts enabled
        // and config_lock unlocked
        handle_config_change(events);

        irq_dis.redisable();
        lock.lock();
    }
}

bool virtio_gpu_dev_t::offer_features(feature_set_t &features)
{
    if (features[VIRTIO_F_GPU3D_BIT])
        printk("virtio: 3D supported\n");

    feature_set_t supported({ VIRTIO_F_VERSION_1_BIT,
                              VIRTIO_F_INDIRECT_DESC_BIT,
                              VIRTIO_F_RING_EVENT_IDX_BIT,
                              VIRTIO_F_GPU3D_BIT });

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
