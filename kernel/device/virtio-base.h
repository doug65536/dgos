#pragma once
#include "types.h"
#include "pci.h"
#include "vector.h"
#include "mutex.h"

class virtio_base_t;

class virtio_factory_base_t {
protected:
    int detect_virtio(int dev_class, int device, char const *name);

    virtual ~virtio_factory_base_t() {}
    virtual virtio_base_t *create() = 0;
};

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring;
    // Followed by 'queue_size' uint16_t ring
};

class virtio_virtqueue_t {
public:
    struct desc_t {
        uint64_t addr;

        uint32_t len;

        union flag_field {
            struct {
                bool next:1;
                bool write:1;
                bool indirect:1;
            } bits;

            uint16_t raw;
        } flags;

        uint16_t next;
    };

    struct avail_hdr_t {
        uint16_t flags;
        uint16_t idx;
    };

    struct avail_ftr_t {
        uint16_t used_event;
    };

    static_assert(sizeof(desc_t) == 16, "Unexpected size");

    virtio_virtqueue_t()
        : desc_tab(nullptr)
        , avail_ring(nullptr)
        , used_ring(nullptr)
        , log2_queue_size(0)
        , single_page(false)
    {
    }

    ~virtio_virtqueue_t()
    {
        scoped_lock hold(lock);
        if (single_page && desc_tab) {
            munmap(desc_tab, (sizeof(*desc_tab) << log2_queue_size) +
                   (sizeof(uint16_t) << log2_queue_size) +
                   (sizeof(uint16_t) << log2_queue_size));
            desc_tab = nullptr;
            avail_ring = nullptr;
            used_ring = nullptr;
            log2_queue_size = 0;
            single_page = false;
        }

    }

    bool set_size(uint8_t log2_queue_size);

private:
    using lock_type = mcslock;
    using scoped_lock = unique_lock<lock_type>;

    lock_type lock;
    condition_variable not_full;

    desc_t *desc_tab;
    uint16_t *avail_ring;
    uint16_t volatile *used_ring;

    uint8_t log2_queue_size;
    bool single_page;
};

struct virtio_pci_cap_hdr_t {
    uint8_t cap_vendor;
    uint8_t cap_next;
    uint8_t cap_len;
    uint8_t type;
    uint8_t bar;
    uint8_t padding[3];
    uint32_t offset;
    uint32_t length;
};

struct pci_cfg_cap_t {

};

#define VIRTIO_VENDOR   0x1AF4
#define VIRTIO_DEV_MIN  0x1000
#define VIRTIO_DEV_MAX  0x107F

// Common configuration
#define VIRTIO_PCI_CAP_COMMON_CFG   1

// Notifications
#define VIRTIO_PCI_CAP_NOTIFY_CFG   2

// ISR Status
#define VIRTIO_PCI_CAP_ISR_CFG      3

// Device specific configuration
#define VIRTIO_PCI_CAP_DEVICE_CFG   4

// PCI configuration access
#define VIRTIO_PCI_CAP_PCI_CFG      5

struct virtio_pci_common_cfg {
    // About the whole device

    // read-write
    uint32_t device_feature_select;

    // read-only for driver
    uint32_t device_feature;

    // read-write
    uint32_t driver_feature_select;

    // read-write
    uint32_t driver_feature;

    // read-write
    uint16_t msix_config;

    // read-only for driver
    uint16_t num_queues;

    // read-write
    uint8_t device_status;

    // read-only for driver
    uint8_t config_generation;

    // About a specific virtqueue

    // read-write
    uint16_t queue_select;

    // read-write, power of 2, or 0
    uint16_t queue_size;

    // read-write
    uint16_t queue_msix_vector;

    // read-write
    uint16_t queue_enable;

    // read-only for driver
    uint16_t queue_notify_off;

    // read-write
    uint64_t queue_desc;

    // read-write
    uint64_t queue_avail;

    // read-write
    uint64_t queue_used;
};

class virtio_base_t {
public:
    virtual ~virtio_base_t() {}
protected:
    friend class virtio_factory_base_t;

    bool virtio_init(pci_dev_iterator_t const &pci_iter, char const *isr_name);
    bool configure_queues_n(virtio_virtqueue_t *queues, size_t count);

    template<size_t N>
    bool configure_queues(virtio_virtqueue_t (&queues)[N])
    {
        return configure_queues_n(queues, N);
    }

    virtual bool init(pci_dev_iterator_t const &pci_iter) = 0;

    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    virtual void irq_handler(int irq) = 0;

    static vector<virtio_base_t*> virtio_devs;

    bool use_msi;
    pci_irq_range_t irq_range;

    virtio_virtqueue_t vq;

    // MMIO
    using lock_type = mcslock;
    using scoped_lock = unique_lock<lock_type>;
    lock_type cfg_lock;
    virtio_pci_common_cfg volatile *common_cfg;
    size_t common_cfg_size;
};
