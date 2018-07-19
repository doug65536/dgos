#include "virtio-base.h"
#include "likely.h"
#include "cpu/atomic.h"
#include "inttypes.h"

#define DEBUG_VIRTIO 1
#if DEBUG_VIRTIO
#define VIRTIO_TRACE(...) printdbg("virtio: " __VA_ARGS__)
#else
#define VIRTIO_TRACE(...) ((void)0)
#endif

std::vector<virtio_base_t*> virtio_base_t::virtio_devs;

bool virtio_base_t::virtio_init(pci_dev_iterator_t const& pci_iter,
                                char const *isr_name)
{
    virtio_pci_cap_hdr_t cap_rec;

    // Make sure bus master and memory space is enabled, disable I/O space
    pci_adj_control_bits(pci_iter, PCI_CMD_BME | PCI_CMD_MSE, PCI_CMD_IOSE);

    for (int cap_start = 0, cap;
         0 != (cap = pci_find_capability(pci_iter, PCICAP_VENDOR, cap_start));
         cap_start = cap) {
        pci_config_copy(pci_iter, &cap_rec, cap, sizeof(cap_rec));

        VIRTIO_TRACE("Found %s capability"
                     ": type=%#x, bar=%d"
                     ", ofs=%#x, len=%#x\n",
                     isr_name,
                     cap_rec.type, cap_rec.bar,
                     cap_rec.offset, cap_rec.length);

        bool is_mmio;
        is_mmio = pci_iter.config.is_bar_mmio(cap_rec.bar);

        // Not MMIO? Bail!
        if (unlikely(!is_mmio))
            return false;

        uint64_t bar;
        bar = pci_iter.config.get_bar(cap_rec.bar) + cap_rec.offset;

        switch (cap_rec.type) {
        case VIRTIO_PCI_CAP_COMMON_CFG:
            VIRTIO_TRACE("... VIRTIO_PCI_CAP_COMMON_CFG\n");

            common_cfg_size = cap_rec.length;

            common_cfg = (virtio_pci_common_cfg_t*)mmap(
                        (void*)bar, cap_rec.length, PROT_READ | PROT_WRITE,
                        MAP_PHYSICAL, -1, 0);

            // 4.1.4.3 Reset the device
            common_cfg->device_status = 0;

            // 4.1.4.3.2 Wait until reset completes
            while (common_cfg->device_status != 0)
                pause();

            // Notify the device that a virtio driver has found it
            common_cfg->device_status |= VIRTIO_STATUS_ACKNOWLEDGE;

            // Notify the device that a virtio driver is accessing it
            common_cfg->device_status |= VIRTIO_STATUS_DRIVER;

            // Negotiate features
            do {
                feature_set_t features;

                features.fetch_device_features(common_cfg);

                // Offer the feature bitmap to the subclass
                if (!offer_features(features)) {
                    // Tell the device we gave up
                    common_cfg->device_status |= VIRTIO_STATUS_FAILED;
                    return false;
                }

                // Write the driver supported features into feature select
                features.store_driver_features(common_cfg);

                // Then read back what got set
                features.fetch_driver_features(common_cfg);

                if (!verify_features(features)) {
                    // Tell the device we gave up
                    common_cfg->device_status |= VIRTIO_STATUS_FAILED;
                    return false;
                }
            } while (false);

            atomic_fence();
            common_cfg->device_status |= VIRTIO_STATUS_FEATURES_OK;
            atomic_fence();
            if (!(common_cfg->device_status & VIRTIO_STATUS_FEATURES_OK)) {
                // Tell the device we gave up
                common_cfg->device_status |= VIRTIO_STATUS_FAILED;
                return false;
            }

            queue_count = common_cfg->num_queues;

            use_msi = pci_try_msi_irq(pci_iter, &irq_range, 0, false,
                                      queue_count + 1,
                                      &virtio_base_t::irq_handler, isr_name);

            // Allocate number of queues supported by device
            queues.reset(new virtio_virtqueue_t[queue_count]);

            if (unlikely(!queues)) {
                // Tell the device we gave up
                common_cfg->device_status |= VIRTIO_STATUS_FAILED;
                return false;
            }

            pci_set_irq_unmask(pci_iter, true);

            // Initialize MSI-X IRQs
            common_cfg->config_msix_vector = 0;

            for (size_t i = queue_count; i > 0; --i) {
                virtio_virtqueue_t& vq = queues[i - 1];

                uint16_t queue_msix_vector = use_msi && irq_range.msix
                        ? i : 0;

                if (!vq.init(i - 1, common_cfg, (char*)notify_cap,
                             notify_off_multiplier, queue_msix_vector)) {
                    // Tell the device we gave up
                    common_cfg->device_status |= VIRTIO_STATUS_FAILED;
                    return false;
                }
            }

            common_cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;

            VIRTIO_TRACE("Successfully negotiated features\n");

            break;

        case VIRTIO_PCI_CAP_NOTIFY_CFG:
            VIRTIO_TRACE("... VIRTIO_PCI_CAP_NOTIFY_CFG\n");

            notify_cap_size = cap_rec.length;
            notify_cap = (virtio_pci_notify_cap_t *)mmap(
                        (void*)bar, cap_rec.length, PROT_READ | PROT_WRITE,
                        MAP_PHYSICAL | MAP_NOCACHE | MAP_WRITETHRU, -1, 0);
            if (unlikely(notify_cap == MAP_FAILED))
                return false;

            notify_off_multiplier = notify_cap->notify_off_multiplier;

            break;

        case VIRTIO_PCI_CAP_ISR_CFG:
            VIRTIO_TRACE("... VIRTIO_PCI_CAP_ISR_CFG\n");
            // This is ignored, because only MSI-X is used
            break;

        case VIRTIO_PCI_CAP_DEVICE_CFG:
            VIRTIO_TRACE("... VIRTIO_PCI_CAP_DEVICE_CFG\n");

            device_cfg_size = cap_rec.length;

            device_cfg = (virtio_pci_common_cfg_t*)mmap(
                        (void*)bar, cap_rec.length, PROT_READ | PROT_WRITE,
                        MAP_PHYSICAL, -1, 0);

            break;

        case VIRTIO_PCI_CAP_PCI_CFG:
            VIRTIO_TRACE("... VIRTIO_PCI_CAP_PCI_CFG\n");
            break;

        default:
            VIRTIO_TRACE("... unrecognized type!\n");
            break;
        }
    }

    return true;
}

isr_context_t *virtio_base_t::irq_handler(int irq, isr_context_t *ctx)
{
    for (virtio_base_t *dev : virtio_devs) {
        if (irq >= dev->irq_range.base &&
                irq < dev->irq_range.base + dev->irq_range.count) {
            dev->irq_handler(irq - dev->irq_range.base);
        }
    }
    return ctx;
}

bool virtio_virtqueue_t::init(
        int queue_idx, virtio_pci_common_cfg_t volatile *common_cfg,
        char volatile *notify_base, uint32_t notify_off_multiplier,
        uint16_t msix_vector)
{
    atomic_fence();
    common_cfg->queue_select = queue_idx;
    atomic_fence();

    notify_ptr = (uint16_t*)
            (notify_base + (common_cfg->queue_notify_off *
                            notify_off_multiplier));

    uint8_t log2_queue_size = bit_log2(common_cfg->queue_size);

    // We don't enforce contiguous physical allocation,
    // so don't let it be more than the page size for now
    if ((sizeof(desc_t) << log2_queue_size) > PAGE_SIZE)
        log2_queue_size = bit_log2(PAGE_SIZE / sizeof(desc_t));

    this->log2_queue_size = log2_queue_size;

    int queue_count = 1 << log2_queue_size;

    size_t bytes = (sizeof(desc_t) << log2_queue_size) +
            sizeof(ring_hdr_t) +
            (sizeof(uint16_t) << log2_queue_size) +
            sizeof(ring_ftr_t) +
            6 + (8 << log2_queue_size);

    single_page = bytes <= PAGE_SIZE;

    if (single_page) {
        char *buffer = (char*)mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                                   MAP_POPULATE, -1, 0);
        if (buffer == MAP_FAILED)
            return false;

        // Calculate pointers to descriptor table, available ring, used ring
        desc_tab = (desc_t*)buffer;
        avail_ring = (uint16_t*)(desc_tab + queue_count);
        used_hdr = (ring_hdr_t*)(avail_ring + queue_count);
    } else {
        desc_tab = (desc_t*)mmap(
                    nullptr, sizeof(desc_t) << log2_queue_size,
                    PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

        avail_ring = (uint16_t*)mmap(
                    nullptr, sizeof(uint16_t) << log2_queue_size,
                    PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);

        used_hdr = (ring_hdr_t*)mmap(
                    nullptr, sizeof(uint16_t) << log2_queue_size,
                    PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
    }

    used_ring = (used_t*)(used_hdr + 1);
    used_ftr = (ring_ftr_t*)(used_ring + queue_count);

    // Link together all of the descriptors into the free list
    for (int i = 1 << log2_queue_size; i > 0; --i) {
        desc_tab[i - 1].next = desc_first_free;
        desc_first_free = i - 1;
    }

    common_cfg->queue_size = 1 << log2_queue_size;
    common_cfg->queue_desc = mphysaddr(desc_tab);
    common_cfg->queue_avail = mphysaddr(avail_ring);
    common_cfg->queue_used = mphysaddr(used_ring);

    common_cfg->queue_msix_vector = msix_vector;
    assert(common_cfg->queue_msix_vector == msix_vector);

    common_cfg->queue_enable = 1;
    assert(common_cfg->queue_enable == 1);

    return true;
}

virtio_virtqueue_t::desc_t *virtio_virtqueue_t::alloc_desc(bool dev_writable)
{
    scoped_lock lock(queue_lock);

    while (desc_first_free == -1)
        queue_not_full.wait(lock);

    desc_t *desc = desc_tab + desc_first_free;
    desc_first_free = desc->next;

    memset(desc, 0, sizeof(*desc));

    desc->next = -1;

    if (dev_writable)
        desc->flags.bits.write = true;

    return desc;
}

uint16_t virtio_virtqueue_t::index_of(desc_t *desc) const
{
    return desc - desc_tab;
}

void virtio_virtqueue_t::enqueue_avail(desc_t **desc, size_t count)
{
    size_t mask = ~-(1U << log2_queue_size);

    scoped_lock lock(queue_lock);
    for (size_t i = 0; i < count; ++i) {
        uint16_t index = index_of(desc[i]);
        avail_ring[2 + (avail_head++ & mask)] = index;
    }
    atomic_fence();

    // Update used idx
    used_ftr->used_event = avail_head - 1;

    // Update idx
    avail_ring[1] = avail_head - 1;

    atomic_fence();

    *notify_ptr = avail_head - 1;
}

int virtio_factory_base_t::detect_virtio(int dev_class, int device,
                                         char const *name)
{
    pci_dev_iterator_t pci_iter;

    if (!pci_enumerate_begin(&pci_iter, dev_class, -1, VIRTIO_VENDOR, device))
        return 0;

    do {
        if (pci_iter.config.device < VIRTIO_DEV_MIN ||
                pci_iter.config.device > VIRTIO_DEV_MAX)
            continue;

        VIRTIO_TRACE("Found %s device\n", name);

        std::unique_ptr<virtio_base_t> self = create();

        if (self->init(pci_iter))
            virtio_base_t::virtio_devs.push_back(self.release());
    } while (pci_enumerate_next(&pci_iter));

    return 0;
}
