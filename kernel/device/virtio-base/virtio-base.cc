#include "virtio-base.h"
#include "likely.h"
#include "cpu/atomic.h"
#include "inttypes.h"
#include "time.h"
#include "numeric_limits.h"
#include "work_queue.h"

#define DEBUG_VIRTIO 1
#if DEBUG_VIRTIO
#define VIRTIO_TRACE(...) printdbg("virtio: " __VA_ARGS__)
#else
#define VIRTIO_TRACE(...) ((void)0)
#endif

ext::vector<virtio_base_t*> virtio_base_t::virtio_devs;

char const *virtio_base_t::cap_names[] = {
    "<invalid>",
    "VIRTIO_PCI_CAP_COMMON_CFG",
    "VIRTIO_PCI_CAP_NOTIFY_CFG",
    "VIRTIO_PCI_CAP_ISR_CFG",
    "VIRTIO_PCI_CAP_DEVICE_CFG",
    "VIRTIO_PCI_CAP_PCI_CFG"
};

virtio_base_t::~virtio_base_t()
{
}

bool virtio_base_t::virtio_init(pci_dev_iterator_t const& pci_iter,
                                char const *isr_name, bool per_cpu_queues)
{
    virtio_pci_cap_hdr_t cap_rec;

    // 4.1.2.3 " Transitional devices MUST have the
    // Transitional PCI Device ID in the range 0x1000 to 0x103f."
    is_transitional = pci_iter.config.device >= 0x1000 &&
            pci_iter.config.device < 0x1040;


    if (is_transitional) {
        printdbg("virtio: found transitional device\n");
        // "Transitional devices MUST have a PCI Revision ID of 0."
        if (pci_iter.config.revision != 0) {
            printdbg("virtio: unexpected transitional pci revision"
                     ", expected 0, got %x\n",
                     pci_iter.config.revision);
        }

        // "Transitional devices MUST have the PCI Subsystem
        // Device ID matching the Virtio Device ID"
        if (pci_iter.config.subsystem_id != pci_iter.config.device) {
            printdbg("virtio: unexpected transitional pci subsystem id"
                     ", expected %x, got %x\n",
                     pci_iter.config.device,
                     pci_iter.config.subsystem_id);
        }
    } else {
        printdbg("virtio: found modern device\n");
    }

    // Make sure bus master, memory space, and I/O space are enabled
    pci_adj_control_bits(pci_iter, PCI_CMD_BME | PCI_CMD_MSE | PCI_CMD_IOSE, 0);

    VIRTIO_TRACE("Initializing vendor=%#x, device=%#x\n",
                 pci_iter.config.vendor, pci_iter.config.device);

    for (int cap_start = 0, cap;
         0 != (cap = pci_find_capability(pci_iter, PCICAP_VENDOR, cap_start));
         cap_start = cap) {
        pci_config_copy(pci_iter, &cap_rec, cap, sizeof(cap_rec));

        VIRTIO_TRACE("Found %s capability"
                     ": type=%s (%#x), bar=%d"
                     ", ofs=%#x, len=%#x\n",
                     isr_name,
                     cap_names[cap_rec.type < countof(cap_names)
                                ? cap_rec.type : 0],
                     cap_rec.type, cap_rec.bar,
                     cap_rec.offset, cap_rec.length);

        bool is_mmio = pci_iter.config.is_bar_mmio(cap_rec.bar);

        uint64_t addr;
        addr = pci_iter.config.get_bar(cap_rec.bar) + cap_rec.offset;

        switch (cap_rec.type) {
        case VIRTIO_PCI_CAP_COMMON_CFG:
            if (common_cfg_size)
                break;

            common_cfg_size = cap_rec.length;

            // Putting this in I/O would be madness
            assert(is_mmio);

            common_cfg = (virtio_pci_common_cfg_t*)mmap(
                        (void*)addr, cap_rec.length,
                        PROT_READ | PROT_WRITE, MAP_PHYSICAL);

            // 4.1.4.3 Reset the device
            common_cfg->device_status = 0;

            // 4.1.4.3.2 Wait until reset completes
            uint64_t timeout;
            timeout = time_ns() + 2000000000;

            // Poll a million times between checking for timeout,
            // so we don't spend 99% of CPU checking time
            int timeout_divisor;
            timeout_divisor = 1000000;
            while (atomic_ld_acq(&common_cfg->device_status) != 0) {
                if (unlikely(!--timeout_divisor)) {
                    timeout_divisor = 1000000;
                    if (time_ns() > timeout) {
                        printk("Timeout waiting for %s device reset!\n",
                               isr_name);
                        return false;
                    }
                }

                pause();
            }

            // Notify the device that a virtio driver has found it
            mm_or(common_cfg->device_status, VIRTIO_STATUS_ACKNOWLEDGE);

            // Notify the device that a virtio driver is accessing it
            mm_or(common_cfg->device_status, VIRTIO_STATUS_DRIVER);

            // Negotiate features
            do {
                feature_set_t features;

                features.fetch_device_features(common_cfg);

                // Offer the feature bitmap to the subclass
                if (!offer_features(features)) {
                    // Tell the device we gave up
                    mm_or(common_cfg->device_status, VIRTIO_STATUS_FAILED);
                    return false;
                }

                // Write the driver supported features into feature select
                features.store_driver_features(common_cfg);

                // Then read back what got set
                features.fetch_driver_features(common_cfg);

                if (!verify_features(features)) {
                    // Tell the device we gave up
                    mm_or(common_cfg->device_status, VIRTIO_STATUS_FAILED);
                    return false;
                }
            } while (false);

            atomic_fence();
            mm_or(common_cfg->device_status, VIRTIO_STATUS_FEATURES_OK);

            atomic_fence();
            if (!(common_cfg->device_status & VIRTIO_STATUS_FEATURES_OK)) {
                // Tell the device we gave up
                mm_or(common_cfg->device_status, VIRTIO_STATUS_FAILED);
                return false;
            }

            queue_count = common_cfg->num_queues;

            // Allocate number of queues supported by device
            queues.reset(new (ext::nothrow) virtio_virtqueue_t[queue_count]);

            if (unlikely(!queues)) {
                // Tell the device we gave up
                mm_or(common_cfg->device_status, VIRTIO_STATUS_FAILED);
                return false;
            }

            if (per_cpu_queues) {
                // Use two interrupt vectors, first one is config IRQ,
                // second one is shared by all queues
                // Route first vector to CPU 0, route rest of vectors
                // round robin across all CPUs, starting at CPU 0

                ext::vector<int> target_cpus(queue_count + 1, 0);
                ext::vector<int> vector_offsets(queue_count + 1, 1);

                // Route config IRQs to CPU 0
                vector_offsets[0] = 0;

                // Distribute the rest of the IRQs across CPUs
                int cpu_count = thread_get_cpu_count();
                for (size_t i = 1; i <= queue_count; ++i)
                    target_cpus[i] = (i - 1) % cpu_count;

                use_msi = pci_try_msi_irq(pci_iter, &irq_range, 0, false,
                                          queue_count + 1,
                                          &virtio_base_t::irq_handler,
                                          isr_name, target_cpus.data(),
                                          vector_offsets.data());
            } else {
                use_msi = pci_try_msi_irq(pci_iter, &irq_range, 0, false,
                                          queue_count + 1,
                                          &virtio_base_t::irq_handler,
                                          isr_name);
            }

            pci_set_irq_unmask(pci_iter, true);

            // Initialize MSI-X IRQs
            common_cfg->config_msix_vector = 0;

            for (size_t i = queue_count; i > 0; --i) {
                virtio_virtqueue_t& vq = queues[i - 1];

                uint16_t queue_msix_vector = (use_msi && irq_range.msix)
                        ? i : 0;

                if (unlikely(!vq.init(i - 1, common_cfg,
                                      notify_cap_offset,
                                      notify_accessor,
                                      notify_off_multiplier,
                                      queue_msix_vector))) {
                    // Tell the device we gave up
                    mm_or(common_cfg->device_status, VIRTIO_STATUS_FAILED);
                    return false;
                }
            }

            atomic_fence();
            mm_or(common_cfg->device_status, VIRTIO_STATUS_FEATURES_OK);
            atomic_fence();
            mm_or(common_cfg->device_status, VIRTIO_STATUS_DRIVER_OK);

            VIRTIO_TRACE("Successfully negotiated features\n");

            break;

        case VIRTIO_PCI_CAP_NOTIFY_CFG:
            if (notify_cap_size)
                break;

            notify_cap_size = cap_rec.length;
            notify_is_mmio = is_mmio;
            notify_bar = cap_rec.bar;
            notify_cap_offset = cap_rec.offset;

            notify_accessor = pci_bar_accessor_t(
                        pci_iter, notify_bar,
                        cap_rec.offset + notify_cap_size);

            notify_off_multiplier = 0;
            pci_config_copy(pci_iter, &notify_off_multiplier,
                            cap + sizeof(cap_rec),
                            sizeof(notify_off_multiplier));

            break;

        case VIRTIO_PCI_CAP_ISR_CFG:
            assert(is_mmio);

            // I wish I could ignore this old crap and just use MSI-X, but
            // no, qemu broke virtio almost entirely, broken all the way
            // to always using pin interrupts
            isr_status = (uint32_t*)mmap((void*)addr, sizeof(uint32_t),
                                         PROT_READ, MAP_PHYSICAL);
            break;

        case VIRTIO_PCI_CAP_DEVICE_CFG:
            device_cfg_size = cap_rec.length;

            assert(is_mmio);
            device_cfg = (virtio_pci_common_cfg_t*)mmap(
                        (void*)addr, cap_rec.length, PROT_READ | PROT_WRITE,
                        MAP_PHYSICAL);

            break;

        case VIRTIO_PCI_CAP_PCI_CFG:
            VIRTIO_TRACE("Ignoring VIRTIO_PCI_CAP_PCI_CFG\n");
            break;

        default:
            break;
        }
    }

    VIRTIO_TRACE("Completed %s init successfully\n", isr_name);

    return true;
}

isr_context_t *virtio_base_t::irq_handler(int irq, isr_context_t *ctx)
{
    for (virtio_base_t *dev : virtio_devs) {
        if (irq >= dev->irq_range.base &&
                irq < dev->irq_range.base + dev->irq_range.count) {

//            if (irq_islevel(irq))
//                irq_setmask(irq, false);

//            workq::enqueue([=] {
                dev->irq_handler(irq - dev->irq_range.base);

//                if (irq_islevel(irq))
//                    irq_setmask(irq, true);
//            });
        }
    }
    return ctx;
}

bool virtio_virtqueue_t::init(
        int queue_idx,
        virtio_pci_common_cfg_t volatile *common_cfg,
        uint32_t cap_offset,
        pci_bar_accessor_t &notify_accessor,
        uint32_t notify_off_multiplier,
        uint16_t msix_vector)
{
    this->queue_idx = queue_idx;

    this->notify_accessor = notify_accessor;

    atomic_st_rel(&common_cfg->queue_select, queue_idx);

    uint8_t log2_queue_size = bit_log2(common_cfg->queue_size);

    // We don't enforce contiguous physical allocation,
    // so don't let it be more than the page size for now
    if ((sizeof(desc_t) << log2_queue_size) > PAGE_SIZE)
        log2_queue_size = bit_log2(PAGE_SIZE / sizeof(desc_t));

    this->log2_queue_size = log2_queue_size;

    int queue_count = 1 << log2_queue_size;

    size_t bytes = (sizeof(desc_t) << log2_queue_size) +
            sizeof(ring_hdr_t) +
            (sizeof(avail_t) << log2_queue_size) +
            sizeof(ring_ftr_t) +
            sizeof(ring_hdr_t) +
            (sizeof(used_t) << log2_queue_size) +
            sizeof(ring_ftr_t);

    single_page = bytes <= PAGE_SIZE;

    if (single_page) {
        char *buffer = (char*)mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                                   MAP_POPULATE);
        if (buffer == MAP_FAILED)
            return false;

        // Calculate pointers to descriptor table, available ring, used ring
        desc_tab = (desc_t*)buffer;
        avail_hdr = (ring_hdr_t*)(desc_tab + queue_count);
        avail_ring = (avail_t*)(avail_hdr + 1);
        avail_ftr = (ring_ftr_t*)(avail_ring + queue_count);
        used_hdr = (ring_hdr_t*)(avail_ftr + 1);
        used_ring = (used_t*)(used_hdr + 1);
        used_ftr = (ring_ftr_t*)(used_ring + queue_count);
    } else {
        desc_tab = (desc_t*)mmap(
                    nullptr, sizeof(desc_t) << log2_queue_size,
                    PROT_READ | PROT_WRITE, MAP_POPULATE);
        if (desc_tab == MAP_FAILED)
            return false;

        avail_hdr = (ring_hdr_t*)mmap(
                    nullptr, sizeof(ring_hdr_t) +
                    (sizeof(avail_t) << log2_queue_size) +
                    sizeof(ring_ftr_t),
                    PROT_READ | PROT_WRITE, MAP_POPULATE);
        if (avail_hdr == MAP_FAILED)
            return false;

        used_hdr = (ring_hdr_t*)mmap(
                    nullptr, sizeof(ring_hdr_t) +
                    (sizeof(used_t) << log2_queue_size) +
                    sizeof(ring_ftr_t),
                    PROT_READ | PROT_WRITE, MAP_POPULATE);
        if (used_hdr == MAP_FAILED)
            return false;

        avail_ring = (avail_t*)(avail_hdr + 1);
        avail_ftr = (ring_ftr_t*)(avail_ring + queue_count);

        used_ring = (used_t*)(used_hdr + 1);
        used_ftr = (ring_ftr_t*)(used_ring + queue_count);
    }

    completions.reset(new (ext::nothrow) virtio_iocp_t*[queue_count]());
    if (!completions)
        return false;

    if (!pending_completions.reserve(queue_count))
        return false;
    if (!finished_completions.reserve(queue_count))
        return false;

    // Link together all of the descriptors into the free list
    assert(desc_first_free == -1);
    desc_free_count = 1 << log2_queue_size;
    for (int i = desc_free_count; i > 0; --i) {
        desc_tab[i - 1].next = desc_first_free;
        desc_first_free = i - 1;
    }

    common_cfg->queue_size = 1 << log2_queue_size;

    uint64_t desc_paddr;
    desc_paddr = mphysaddr(desc_tab);
    assert((desc_paddr & -16) == desc_paddr);
    common_cfg->queue_desc = desc_paddr;

    uint64_t avail_paddr;
    avail_paddr = mphysaddr(avail_hdr);
    assert((avail_paddr & -2) == avail_paddr);
    common_cfg->queue_avail = avail_paddr;

    uint64_t used_paddr;
    used_paddr = mphysaddr(used_hdr);
    assert((used_paddr & -4) == used_paddr);

    common_cfg->queue_used = used_paddr;
    common_cfg->queue_msix_vector = msix_vector;

    // Can't reliably read back anymore until enabled
    common_cfg->queue_enable = 1;

    uint16_t queue_notify_off = common_cfg->queue_notify_off;

    notify_reg = pci_bar_register_t{
            cap_offset +
            queue_notify_off * notify_off_multiplier,
            sizeof(uint16_t)
    };

    if (unlikely(!(assert(common_cfg->queue_size == (1 << log2_queue_size)) ||
                   assert(common_cfg->queue_avail == avail_paddr) ||
                   assert(common_cfg->queue_desc == desc_paddr) ||
                   assert(common_cfg->queue_used == used_paddr) ||
                   assert(common_cfg->queue_msix_vector == msix_vector) ||
                   assert(common_cfg->queue_enable == 1))))
        return false;

    return true;
}

virtio_virtqueue_t::desc_t *
virtio_virtqueue_t::alloc_desc(bool dev_writable)
{
    scoped_lock lock(queue_lock);

    while (desc_first_free == avail_t(-1)) {
        VIRTIO_TRACE("Waiting for free descriptor\n");
        queue_not_full.wait(lock);
    }
    assert(desc_free_count > 0);

    --desc_free_count;
    desc_t *desc = desc_tab + desc_first_free;
    desc_first_free = desc->next;

    lock.unlock();

    desc->addr = 0;
    desc->len = 0;
    desc->flags.raw = 0;
    desc->next = -1;

    if (dev_writable)
        desc->flags.bits.write = true;

    VIRTIO_TRACE("Allocated descriptor id=%zu\n", desc - desc_tab);

    return desc;
}

void virtio_virtqueue_t::alloc_multiple(
        virtio_virtqueue_t::desc_t **descs, size_t count)
{
    scoped_lock lock(queue_lock);

    while (desc_free_count < count) {
        VIRTIO_TRACE("Waiting for %zu free descriptors"
                     ", desc_free_count=%u\n", count, desc_free_count);
        queue_not_full.wait(lock);
    }
    desc_free_count -= count;

    for (size_t i = 0; i < count; ++i) {
        desc_t *desc = desc_tab + desc_first_free;
        desc_first_free = desc->next;
        descs[i] = desc;
        desc->addr = 0;
        desc->len = 0;
        desc->flags.raw = 0;
        desc->next = -1;
    }
}

void virtio_virtqueue_t::enqueue_avail(desc_t **desc, size_t count,
                                       virtio_iocp_t *iocp)
{
    size_t mask = ~-(size_t(1) << log2_queue_size);

    scoped_lock lock(queue_lock);

    bool skip = false;
    size_t avail_head = atomic_ld_acq(&avail_hdr->idx);
    size_t chain_start = ~size_t(0);
    for (size_t i = 0; i < count; ++i) {
        if (!skip) {
            size_t id = index_of(desc[i]);

            iocp->set_expect(1);
            completions[id] = iocp;

            chain_start = id;
        }

        skip = desc[i]->flags.bits.next;

        if (!skip && chain_start != ~size_t(0))
            avail_ring[avail_head++ & mask] = chain_start;
    }

    // Update used idx
    atomic_st_rel(&avail_ftr->used_event, avail_head - 1);

    // Update idx
    atomic_st_rel(&avail_hdr->idx, avail_head);

    if (int16_t((uint16_t)avail_head - (uint16_t)avail_ftr->used_event) > 0)
        notify_accessor.wr_16(notify_reg, queue_idx);
        //atomic_st_rel(notify_ptr, queue_idx);
}

void virtio_virtqueue_t::sendrecv(void const *sent_data, size_t sent_size,
                                  void *rcvd_data, size_t rcvd_size,
                                  virtio_iocp_t *iocp)
{
    mmphysrange_t ranges[16];
    size_t range_count;

    desc_t *desc[32];
    size_t out = 0;

    if (sent_data && sent_size) {
        range_count = mphysranges(ranges, countof(ranges),
                                  sent_data, sent_size,
                                  ext::numeric_limits<uint32_t>::max());

        for (size_t i = 0; i < range_count; ++i, ++out) {
            desc[out] = alloc_desc(false);
            desc[out]->addr = ranges[i].physaddr;
            desc[out]->len = ranges[i].size;

            if (out > 0) {
                desc[out - 1]->flags.bits.next = true;
                desc[out - 1]->next = index_of(desc[out]);
            }
        }
    }

    if (rcvd_data && rcvd_size) {
        range_count = mphysranges(ranges, countof(ranges),
                                  rcvd_data, rcvd_size,
                                  ext::numeric_limits<uint32_t>::max());

        for (size_t i = 0; i < range_count; ++i, ++out) {
            desc[out] = alloc_desc(true);
            desc[out]->addr = ranges[i].physaddr;
            desc[out]->len = ranges[i].size;

            if (out > 0) {
                desc[out - 1]->flags.bits.next = true;
                desc[out - 1]->next = index_of(desc[out]);
            }
        }
    }

    enqueue_avail(desc, out, iocp);
}

void virtio_virtqueue_t::recycle_used()
{
    scoped_lock lock(queue_lock);

    VIRTIO_TRACE("Recycling used descriptors\n");

    size_t tail = used_tail;
    size_t const mask = ~-(1 << log2_queue_size);
    size_t const done_idx = atomic_ld_acq(&used_hdr->idx);
    VIRTIO_TRACE("done_idx = %zu\n", done_idx);
    if (unlikely(done_idx == tail)) {
        VIRTIO_TRACE("dropped spurious virtio IRQ\n");
        return;
    }

    do {
        used_t const& used = used_ring[tail & mask];
        avail_t const id = used.id;
        uint64_t const used_len = used.len;

        VIRTIO_TRACE("Recycling id=%u (head)\n", id);

        unsigned freed_count = 1;

        avail_t end = id;
        while (desc_tab[end].flags.bits.next) {
            end = desc_tab[end].next;
            ++freed_count;
            VIRTIO_TRACE("Recycling id=%u (chained)\n", end);
        }

        desc_tab[end].next = desc_first_free;
        desc_first_free = id;
        desc_free_count += freed_count;

        virtio_iocp_t* const completion = completions[id];
        completions[id] = nullptr;
        completion->set_result(used_len);
        if (unlikely(!pending_completions.push_back(completion)))
            panic_oom();
    } while ((++tail & 0xFFFF) != done_idx);

    used_tail = tail;

    // Notify device how far used ring has been processed
    atomic_st_rel(&used_ftr->used_event, tail);

    pending_completions.swap(finished_completions);

    queue_not_full.notify_all();
    lock.unlock();

    for (virtio_iocp_t *completion : finished_completions)
        completion->invoke();
    finished_completions.clear();

    VIRTIO_TRACE("Free descriptors: %u\n", desc_free_count);
}

int virtio_factory_base_t::detect_virtio(int dev_class, int device,
                                         char const *name)
{
    pci_dev_iterator_t pci_iter;

    VIRTIO_TRACE("Probing for %s device\n", name);

    // Spec 4.1.2 PCI Device Discovery
    if (unlikely(!pci_enumerate_begin(&pci_iter, dev_class,
                                      -1, VIRTIO_VENDOR, device)))
        return 0;

    do {
        // Include the entire range, including transitional
        if (unlikely(pci_iter.config.device < VIRTIO_DEV_MIN ||
                     pci_iter.config.device > VIRTIO_DEV_MAX))
            continue;

        VIRTIO_TRACE("Found %s device at %u:%u:%u\n", name,
                     pci_iter.bus, pci_iter.slot, pci_iter.func);

        ext::unique_ptr<virtio_base_t> self = create();

        if (unlikely(!virtio_base_t::virtio_devs.push_back(self)))
            panic_oom();

        if (likely(self->init(pci_iter))) {
            found_device(self);
            self.release();
        } else {
            virtio_base_t::virtio_devs.pop_back();
        }
    } while (pci_enumerate_next(&pci_iter));

    return 0;
}

virtio_factory_base_t::~virtio_factory_base_t()
{
}
