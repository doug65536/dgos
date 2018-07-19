#pragma once
#include "types.h"
#include "pci.h"
#include "vector.h"
#include "mutex.h"
#include "iocp.h"

class virtio_base_t;
struct virtio_pci_common_cfg_t;

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

    static_assert(sizeof(desc_t) == 16, "Unexpected size");

    struct ring_hdr_t {
        uint16_t flags;
        uint16_t idx;
    };

    struct ring_ftr_t {
        uint16_t used_event;
    };

    struct used_t {
        uint32_t id;
        uint32_t len;
    };

    virtio_virtqueue_t()
        : desc_tab(nullptr)
        , avail_ring(nullptr)
        , used_ring(nullptr)
        , avail_head(0)
        , used_head(0)
        , desc_first_free(-1)
        , log2_queue_size(0)
        , single_page(false)
    {
    }

    ~virtio_virtqueue_t()
    {
        scoped_lock hold(queue_lock);
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

    bool init(int queue_idx, virtio_pci_common_cfg_t volatile *common_cfg,
              char volatile *notify_base, uint32_t notify_off_multiplier,
              uint16_t msix_vector);

    desc_t *alloc_desc(bool dev_writable);

    uint16_t index_of(desc_t *desc) const;
    void enqueue_avail(desc_t **desc, size_t count);

private:
    using lock_type = std::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;

    lock_type queue_lock;
    std::condition_variable queue_not_full;

    desc_t *desc_tab;
    uint16_t *avail_ring;
    ring_hdr_t volatile *used_hdr;
    ring_ftr_t volatile *used_ftr;
    used_t volatile *used_ring;

    uint16_t avail_head;
    uint16_t used_head;

    int desc_first_free;
    uint16_t volatile *notify_ptr;

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

// Vendor ID for all virtio devices
#define VIRTIO_VENDOR   0x1AF4

// Inclusive device id range for all virtio devices
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

#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_NEED_RESET    64
#define VIRTIO_STATUS_FAILED        128

// Reserved feature bits

#define VIRTIO_F_VERSION_1_BIT      32
#define VIRTIO_F_RING_EVENT_IDX_BIT 29
#define VIRTIO_F_INDIRECT_DESC_BIT  28

// virtqueue

struct virtio_pci_common_cfg_t {
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
    uint16_t config_msix_vector;

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

struct virtio_pci_notify_cap_t {
    virtio_pci_cap_hdr_t cap;
    uint32_t notify_off_multiplier;
};

class virtio_base_t {
public:
    virtio_base_t()
        : use_msi(false)
        , common_cfg(nullptr)
        , common_cfg_size(0)
        , notify_off_multiplier(0)
    {
    }

    virtual ~virtio_base_t() {}
protected:
    friend class virtio_factory_base_t;

    // Represent up to 128 bit feature set
    struct feature_set_t {
        struct bit_proxy_t {
            // Proxy to represent a single bit, with set and get as bool
            constexpr bit_proxy_t(feature_set_t& owner, size_t bit)
                : owner(owner)
                , bit(bit)
            {
            }

            constexpr operator bool() const
            {
                return owner.features[bit >> 5] & (1U << (bit & 31));
            }

            bit_proxy_t& operator=(bool value)
            {
                uint32_t mask = (UINT32_C(1) << (bit & 31));
                owner.features[bit >> 5] =
                        (owner.features[bit >> 5] & ~mask) |
                        (-(value && mask) & mask);
                return *this;
            }

            feature_set_t& owner;
            size_t bit;
        };

        constexpr feature_set_t()
            : features{}
        {
        }

        feature_set_t(feature_set_t const& rhs)
        {
            for (size_t i = countof(features); i > 0; --i)
                features[i - 1] = rhs.features[i - 1];
        }

        constexpr feature_set_t(std::initializer_list<int> list)
            : feature_set_t()
        {
            for (uint32_t bit : list)
                (*this)[bit] = true;
        }

        bit_proxy_t operator[](size_t bit)
        {
            assert(bit < countof(features) * 32);
            return bit_proxy_t(*this, bit);
        }

        // Mask off feature bits in this set that are not set in rhs set
        feature_set_t& operator&=(feature_set_t const& rhs)
        {
            for (size_t i = countof(features); i > 0; --i)
                features[i - 1] &= rhs.features[i - 1];
            return *this;
        }

        bool operator==(feature_set_t const& rhs)
        {
            for (size_t i = countof(features); i > 0; --i) {
                if (features[i - 1] != rhs.features[i - 1])
                    return false;
            }
            return true;
        }

        void fetch_device_features(virtio_pci_common_cfg_t volatile *common_cfg)
        {
            for (size_t i = countof(features); i > 0; --i) {
                common_cfg->device_feature_select = i - 1;
                features[i - 1] = common_cfg->device_feature;
            }
        }

        void fetch_driver_features(virtio_pci_common_cfg_t volatile *common_cfg)
        {
            for (size_t i = countof(features); i > 0; --i) {
                common_cfg->driver_feature_select = i - 1;
                features[i - 1] = common_cfg->driver_feature;
            }
        }

        void store_driver_features(virtio_pci_common_cfg_t volatile *common_cfg)
        {
            for (size_t i = countof(features); i > 0; --i) {
                common_cfg->driver_feature_select = i - 1;
                common_cfg->driver_feature = features[i - 1];
            }
        }

        uint32_t features[4];
    };

    bool virtio_init(pci_dev_iterator_t const &pci_iter, char const *isr_name);

    virtual bool init(pci_dev_iterator_t const &pci_iter) = 0;

    // Tell the driver what is supported and expect it to clear features
    // it does not want to use. Subclass returns false to reject.
    virtual bool offer_features(feature_set_t& features) = 0;

    // Allow an implementation to verify which features actually got set
    virtual bool verify_features(feature_set_t& features) {
        return true;
    }

    static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
    virtual void irq_handler(int irq) = 0;

    static std::vector<virtio_base_t*> virtio_devs;

    bool use_msi;
    pci_irq_range_t irq_range;

    std::unique_ptr<virtio_virtqueue_t[]> queues;
    size_t queue_count;

    using lock_type = std::mcslock;
    using scoped_lock = std::unique_lock<lock_type>;
    lock_type cfg_lock;

    // MMIO
    virtio_pci_common_cfg_t volatile *common_cfg;
    size_t common_cfg_size;

    void volatile *device_cfg;
    size_t device_cfg_size;

    virtio_pci_notify_cap_t *notify_cap;
    size_t notify_cap_size;

    uint32_t notify_off_multiplier;
};
