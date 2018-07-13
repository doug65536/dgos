#include "virtio-gpu.h"
#include "virtio-base.h"
#include "callout.h"
#include "dev_text.h"
#include "vector.h"
#include "irq.h"
#include "pci.h"

#define DEBUG_VIRTIO_GPU 1
#if DEBUG_VIRTIO_GPU
#define VIRTIO_GPU_TRACE(...) printdbg("virtio-gpu: " __VA_ARGS__)
#else
#define VIRTIO_GPU_TRACE(...) ((void)0)
#endif

#define VIRTIO_DEVICE_GPU  (0x1040+16)

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
    bool init(pci_dev_iterator_t const &pci_iter) override final;

private:
    friend class virtio_gpu_factory_t;

    void irq_handler(int irq) override final;

    pci_irq_range_t irq_range;
    bool use_msi;
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

    return true;
}

void virtio_gpu_dev_t::irq_handler(int irq)
{

}
