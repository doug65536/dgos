#include "dev_storage.h"
#include "device/pci.h"
#include "mm.h"
#include "printk.h"
#include "cpu/atomic.h"
#include "nvmebits.h"
#include "string.h"
#include <utility.h>
#include "thread.h"

#define NVME_DEBUG	1
#if NVME_DEBUG
#define NVME_TRACE(...) printdbg("nvme: " __VA_ARGS__)
#else
#define NVME_TRACE(...) ((void)0)
#endif

struct nvme_mmio_t {
	// Capabilities
	uint64_t cap;

	// Version
	uint32_t vs;

	// Interrupt mask set
	uint32_t intms;

	// Interrupt mask clear
	uint32_t intmc;

	// Controller configuration
	uint32_t cc;

	uint32_t reserved1;

	// Controller status
	uint32_t csts;

	// NVM subsystem reset (optional)
	uint32_t nssr;

	// Admin queue attributes
	uint32_t aqa;

	// Admin submission queue base addres
	uint64_t asq;

	// Admin completion queue base address
	uint64_t acq;

	// Controller memory buffer location (optional)
	uint32_t cmbloc;

	// Controller memory buffer size (optional)
	uint32_t cmbsz;

	uint8_t reserved2[0xF00-0x40];

	// Command set specific
	uint8_t reserved3[0x1000-0x0F00];
};

C_ASSERT(sizeof(nvme_mmio_t) == 0x1000);

struct nvme_sgl_t {
	char data[16];
};

C_ASSERT(sizeof(nvme_sgl_t) == 16);

struct nvme_prp_t {
	uintptr_t addr;
};

C_ASSERT(sizeof(nvme_prp_t) == 8);

union nvme_data_ptr_t {
	// Scatter gather list
	nvme_sgl_t sgl1;

	// Physical region page
	nvme_prp_t prpp[2];
};

struct nvme_cmd_hdr_t {
	// Command dword 0
	uint32_t cdw0;

	// namespace ID
	uint32_t nsid;

	uint64_t reserved1;

	// Metadata pointer
	uint64_t mptr;

	// Data pointer
	nvme_data_ptr_t dptr;
};

struct nvme_cmd_t {
	nvme_cmd_hdr_t hdr;

	// cmd dwords 10 thru 15
	uint32_t cmd_dword_10[6];


};

//C_ASSERT(sizeof(nvme_cmd_t) == 64);

// 4.6 Completion queue entry
struct nvme_cmp_t {
	// Command specific
	uint32_t cmp_dword[2];

	// Submit queue head pointer at time of completion
	uint16_t sq_head;

	// Submit queue id
	uint16_t sq_id;

	// Command id
	uint16_t cmd_id;

	// status (including phase)
	uint16_t status;
};

C_ASSERT(sizeof(nvme_cmp_t) == 16);

enum struct nvme_cmd_status_t : uint8_t {
	success = 0x0,
	invalid_cmd = 0x1,
	invalid_field = 0x2,
	cmd_id_conflict = 0x3,
	data_xfer_error = 0x4,
	aborted_pwr_loss = 0x5,
	internal_error = 0x6,
	cmd_abort_requested = 0x7,
	cmd_abort_sq_deleted = 0x8,
	cmd_abort_fused_cmd = 0x9,
	cmd_abort_missing_fused_cmd = 0xA,
	invalid_ns_or_fmt = 0xB,
	cmd_seq_error = 0xC,
	invalid_sgl_seg_desc = 0xD,
	invalid_sgl_count = 0xE,
	invalid_data_sgl_len = 0xF,
	invalid_md_gl_len = 0x10,
	invalid_sgl_desc_type = 0x11,
	invalid_use_ctrl_mem = 0x12,
	invalid_prp_ofs = 0x13,
	atomic_write_unit_exceeded = 0x14,
	invalid_sgl_ofs = 0x16,
	invalid_sgl_subtype = 0x17,
	inconsistent_host_id = 0x18,
	keepalive_expired = 0x19,
	keepalive_invalid = 0x1A,

	lba_out_of_range = 0x80,
	capacity_exceeded = 0x81,
	ns_not_ready = 0x82,
	reservation_conflict = 0x83,
	format_in_progress = 0x84
};

// ---------------------------------------------------------------------------

template<typename T>
class nvme_queue_t {
public:
	nvme_queue_t()
		: entries(nullptr)
		, head(0)
		, tail(0)
		, mask(0)
		, phase(1)
	{
	}

	void init(T* entries, uint32_t count,
			  uint32_t volatile *head_doorbell,
			  uint32_t volatile *tail_doorbell,
			  int phase)
	{
		assert_ispo2(count);

		// Only makes sense to have one doorbell
		// Submission queues have tail doorbells
		// Completion queues have head doorbells
		assert((head_doorbell && !tail_doorbell) ||
			   (!head_doorbell && tail_doorbell));

		this->entries = entries;
		this->mask = count - 1;
		this->head_doorbell = head_doorbell;
		this->tail_doorbell = tail_doorbell;
		this->phase = phase;

	}

	uint32_t get_phase() const
	{
		return phase;
	}

	bool is_empty() const
	{
		return head == tail;
	}

	bool is_full() const
	{
		return next(tail) == head;
	}

	template<typename... Args>
	T& enqueue(Args&& ...args)
	{
		T* item = new (entries + tail) T(forward(args)...);
		phase ^= set_tail(next(tail));
		return *item;
	}

	T dequeue()
	{
		T item = entries[head];
		take();
		return item;
	}

	void take()
	{
		entries[head].~T();
		phase ^= set_head(next(head));
	}

	T& peek()
	{
		return entries[head];
	}

	void reset()
	{
		while (!is_empty())
			dequeue();
		head = 0;
		tail = 0;
	}

private:
	uint32_t next(uint32_t index) const
	{
		return (index + 1) & mask;
	}

	// Returns 1 if the queue wrapped
	int set_head(uint32_t new_head)
	{
		int wrapped = new_head < head;

		head = new_head;

		if (head_doorbell)
			*head_doorbell = head;

		return wrapped;
	}

	// Returns 1 if the queue wrapped
	int set_tail(uint32_t new_tail)
	{
		int wrapped = new_tail < tail;

		tail = new_tail;

		if (tail_doorbell)
			*tail_doorbell = tail;

		return wrapped;
	}

	T* entries;
	uint32_t mask;
	uint32_t head;
	uint32_t tail;
	uint32_t phase;
	uint32_t volatile *head_doorbell;
	uint32_t volatile *tail_doorbell;
};

class nvme_if_factory_t : public storage_if_factory_t {
public:
	nvme_if_factory_t() : storage_if_factory_t("nvme") {}
private:
	virtual if_list_t detect(void) final;
};

static nvme_if_factory_t nvme_if_factory;
STORAGE_REGISTER_FACTORY(nvme_if);

// AHCI interface instance
class nvme_if_t : public storage_if_base_t {
public:
	void init(pci_dev_iterator_t const& pci_iter);

private:
	STORAGE_IF_IMPL

	static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
	void irq_handler();

	uint32_t volatile* doorbell_ptr(bool completion, size_t queue);

	nvme_mmio_t volatile *mmio_base;
	pci_config_hdr_t config;
	pci_irq_range_t irq_range;

	size_t doorbell_shift;

	void* admin_queues_physaddr;
	void* admin_queues;


	// Admin submission and completion queues
	nvme_queue_t<nvme_cmd_t> admin_sub_q;
	nvme_queue_t<nvme_cmp_t> admin_com_q;
};

class nvme_dev_t : public storage_dev_base_t {
public:
	void init(nvme_if_t *parent /*, unsigned dev_port, bool dev_is_atapi*/);

private:
	STORAGE_DEV_IMPL
};

#define NVME_MAX_DEVICES    8
static nvme_if_t nvme_devices[NVME_MAX_DEVICES];
static unsigned nvme_count;

#define NVME_MAX_DRIVES    8
static nvme_dev_t nvme_drives[NVME_MAX_DRIVES];
static unsigned nvme_drive_count;

if_list_t nvme_if_factory_t::detect(void)
{
	unsigned start_at = nvme_count;

	if_list_t list = {
		nvme_devices + start_at,
		sizeof(*nvme_devices),
		0
	};

	pci_dev_iterator_t pci_iter;

	NVME_TRACE("Enumerating PCI busses for NVMe...\n");

	if (!pci_enumerate_begin(
				&pci_iter,
				PCI_DEV_CLASS_STORAGE,
				PCI_SUBCLASS_STORAGE_NVM))
		return list;

	do {
		assert(pci_iter.dev_class == PCI_DEV_CLASS_STORAGE);
		assert(pci_iter.subclass == PCI_SUBCLASS_STORAGE_NVM);

		// Make sure it is an AHCI device
		if (pci_iter.config.prog_if != PCI_PROGIF_STORAGE_NVM_NVME)
			continue;

		NVME_TRACE("Found NVME device!\n");


		if (nvme_count < countof(nvme_devices)) {
			nvme_if_t *self = nvme_devices + nvme_count++;

			self->init(pci_iter);
		}
	} while (pci_enumerate_next(&pci_iter));

	return list;
}

void nvme_if_t::init(pci_dev_iterator_t const &pci_iter)
{
	config = pci_iter.config;

	uint64_t addr = (uint64_t(pci_iter.config.base_addr[1]) << 32) |
			pci_iter.config.base_addr[0];

	mmio_base = (nvme_mmio_t*)mmap(
				(void*)(addr & -8),
				0x2000, PROT_READ | PROT_WRITE,
				MAP_PHYSICAL, -1, 0);

	// 7.6.1 Initialization

	// Enable bus master DMA, enable MMIO, disable port I/O
	pci_adj_control_bits(pci_iter.bus, pci_iter.slot,
						 pci_iter.func,
						 PCI_CMD_BUSMASTER | PCI_CMD_MEMEN,
						 PCI_CMD_IOEN);

	// Assume legacy IRQ pin usage until MSI succeeds
	irq_range.base = pci_iter.config.irq_line;
	irq_range.count = 1;

	bool use_msi = pci_set_msi_irq(
				pci_iter.bus, pci_iter.slot, pci_iter.func,
				&irq_range, 0, 0, 0, irq_handler);

	if (!use_msi) {
		// Fall back to pin based IRQ
		irq_hook(irq_range.base, &nvme_if_t::irq_handler);
		irq_setmask(irq_range.base, 1);
	}

	// Disable the controller
	if (mmio_base->cc & NVME_CC_EN)
		mmio_base->cc &= ~NVME_CC_EN;

	// 7.6.1 2) Wait for the controller to indicate that any previous
	// reset is complete
	while ((mmio_base->csts & NVME_CSTS_RDY) != 0)
		pause();

	// Attempt to use 64KB/16KB submission/completion queue sizes
	size_t queue_count = 1024;
	size_t max_queue_count = NVME_CAP_MQES_GET(mmio_base->cap);

	if (queue_count > max_queue_count)
		queue_count = max_queue_count;

	// Size of one queue, in bytes
	size_t queue_bytes = queue_count * sizeof(nvme_cmd_t) +
			queue_count * sizeof(nvme_cmp_t);

	// Allocate space for two queues (submission and completion)
	admin_queues_physaddr = mm_alloc_contiguous(queue_bytes);

	admin_queues = mmap(
				admin_queues_physaddr, queue_bytes,
				PROT_READ | PROT_WRITE,
				MAP_PHYSICAL, -1, 0);

	memset(admin_queues, 0, queue_bytes);

	nvme_cmd_t *submission_queue = (nvme_cmd_t*)
			((char*)admin_queues);

	nvme_cmp_t *completion_queue = (nvme_cmp_t *)
			((char*)admin_queues +
			queue_count * sizeof(nvme_cmd_t));

	// 7.6.1 3) The admin queue should be configured
	mmio_base->aqa =
			NVME_AQA_ACQS_n(queue_count) |
			NVME_AQA_ASQS_n(queue_count);

	// Submission queue address
	mmio_base->asq = (uint64_t)admin_queues_physaddr;

	// Completion queue address
	mmio_base->acq = (uint64_t)admin_queues_physaddr + queue_bytes;

	// 7.6.1 4) The controller settings should be configured

	uint32_t cc = NVME_CC_IOCQES_n(4) |
			NVME_CC_IOSQES_n(6) |
			NVME_CC_MPS_n(0) |
			NVME_CC_CCS_n(0);

	// Try to enable weighted round robin with urgent
	if (NVME_CAP_AMS_GET(mmio_base->cap) & 1)
		cc |= NVME_CC_AMS_n(1);

	mmio_base->cc = cc;

	cc |= NVME_CC_EN;

	mmio_base->cc = cc;

	// 7.6.1 4) Wait for ready
	while (!(mmio_base->csts & NVME_CSTS_RDY))
		pause();

	// Read the doorbell stride
	doorbell_shift = NVME_CAP_DSTRD_GET(mmio_base->cap) + 1;

	// Initialize the admin completion queue
	admin_com_q.init(completion_queue, queue_count,
					 doorbell_ptr(true, 0), nullptr, 1);

	// Initialize the admin submission queue
	admin_sub_q.init(submission_queue, queue_count,
					 nullptr, doorbell_ptr(false, 0), 1);
}

void nvme_if_t::cleanup()
{
}

if_list_t nvme_if_t::detect_devices()
{
	unsigned start_at = nvme_drive_count;

	if_list_t list = {
		nvme_drives + start_at,
		sizeof(*nvme_drives),
		0
	};

	return list;
}

void nvme_dev_t::init(nvme_if_t *parent)
{
}

void nvme_dev_t::cleanup()
{

}

isr_context_t *nvme_if_t::irq_handler(int irq, isr_context_t *ctx)
{
	for (unsigned i = 0; i < nvme_count; ++i) {
		nvme_if_t *dev = nvme_devices + i;

		int irq_offset = irq - dev->irq_range.base;

		if (unlikely(irq_offset < 0 || irq_offset > dev->irq_range.count))
			continue;

		dev->irq_handler();
	}

	return ctx;
}

void nvme_if_t::irq_handler()
{
}

uint32_t volatile* nvme_if_t::doorbell_ptr(bool completion, size_t queue)
{
	uint32_t volatile *doorbells = (uint32_t volatile *)(mmio_base + 1);
	return doorbells + ((queue << doorbell_shift) + completion);
}

int64_t nvme_dev_t::read_blocks(
		void *data, int64_t count,
		uint64_t lba)
{
	return -1;
}

int64_t nvme_dev_t::write_blocks(
		void const *data, int64_t count,
		uint64_t lba, bool fua)
{
	return -1;
}

int nvme_dev_t::flush()
{
	return -1;
}

int64_t nvme_dev_t::trim_blocks(int64_t count,
		uint64_t lba)
{
	return -1;
}

long nvme_dev_t::info(storage_dev_info_t key)
{
	return -1;
}
