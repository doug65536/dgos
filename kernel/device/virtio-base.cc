#include "virtio-base.h"
#include "likely.h"

#define DEBUG_VIRTIO 1
#if DEBUG_VIRTIO
#define VIRTIO_TRACE(...) printdbg("virtio: " __VA_ARGS__)
#else
#define VIRTIO_TRACE(...) ((void)0)
#endif

vector<virtio_base_t*> virtio_base_t::virtio_devs;

bool virtio_base_t::virtio_init(const pci_dev_iterator_t &pci_iter,
                                char const *isr_name)
{
    use_msi = pci_try_msi_irq(pci_iter, &irq_range, 0, false, 4,
                              &virtio_base_t::irq_handler, isr_name);

    virtio_pci_cap_hdr_t cap_rec;

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

        switch (cap_rec.type) {
        case VIRTIO_PCI_CAP_COMMON_CFG:
            VIRTIO_TRACE("... VIRTIO_PCI_CAP_COMMON_CFG\n");

            uint64_t bar;
            bar = pci_iter.config.base_addr[cap_rec.bar];

            // Get upper half of bar
            if ((bar & 6) == 4) {
                bar |= uint64_t(pci_iter.config.base_addr[
                                cap_rec.bar + 1]) << 32;
            }

            break;

        case VIRTIO_PCI_CAP_NOTIFY_CFG:
            VIRTIO_TRACE("... VIRTIO_PCI_CAP_NOTIFY_CFG\n");
            break;

        case VIRTIO_PCI_CAP_ISR_CFG:
            VIRTIO_TRACE("... VIRTIO_PCI_CAP_ISR_CFG\n");
            break;

        case VIRTIO_PCI_CAP_DEVICE_CFG:
            VIRTIO_TRACE("... VIRTIO_PCI_CAP_DEVICE_CFG\n");
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
                irq < dev->irq_range.base + dev->irq_range.count)
            dev->irq_handler(irq - dev->irq_range.base);
    }
    return ctx;
}

bool virtio_virtqueue_t::set_size(uint8_t log2_queue_size)
{
    if (log2_queue_size > 15)
        return false;

    this->log2_queue_size = log2_queue_size;

    size_t bytes = (sizeof(desc_t) << log2_queue_size) +
            sizeof(avail_hdr_t) +
            (sizeof(uint16_t) << log2_queue_size) +
            sizeof(avail_ftr_t) +
            6 + (8 << log2_queue_size);

    buffer = (char*)mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                         MAP_POPULATE, -1, 0);
    if (buffer == MAP_FAILED)
        return false;

    return true;
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

        unique_ptr<virtio_base_t> self = create();

        if (self->init(pci_iter))
            virtio_base_t::virtio_devs.push_back(self.release());
    } while (pci_enumerate_next(&pci_iter));

    return 0;
}
