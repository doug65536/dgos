#include "dev_storage.h"
#include "device/pci.h"
#include "mm.h"
#include "printk.h"
#include "cpu/atomic.h"
#include "nvmebits.h"
#include "string.h"
#include "utility.h"
#include "thread.h"
#include "vector.h"
#include "threadsync.h"
#include "cpu/control_regs.h"

#define NVME_DEBUG	1
#if NVME_DEBUG
#define NVME_TRACE(...) printdbg("nvme: " __VA_ARGS__)
#else
#define NVME_TRACE(...) ((void)0)
#endif

// MMIO registers
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

// Scatter gather list
struct nvme_sgl_t {
	char data[16];
};

C_ASSERT(sizeof(nvme_sgl_t) == 16);

// Physical region pointer
struct nvme_prp_t {
	uintptr_t addr;
};

C_ASSERT(sizeof(nvme_prp_t) == 8);

// SGL or PRP
union nvme_data_ptr_t {
	// Scatter gather list
	nvme_sgl_t sgl1;

	// Physical region page
	nvme_prp_t prpp[2];
};

// Command submission queue header common to all commands
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

C_ASSERT(sizeof(nvme_cmd_hdr_t) == 40);

// Command submission queue admin opcodes
enum struct nvme_admin_cmd_opcode_t : uint8_t {
	delete_sq = 0x00,
	create_sq = 0x01,
	get_log_page = 0x02,
	delete_cq = 0x04,
	create_cq = 0x05,
	identify = 0x06,
	abort = 0x08,
	set_features = 0x09,
	get_features = 0x0A,
	async_evnt_req = 0x0C,
	firmware_commit = 0x10,
	firmware_download = 0x11,
	ns_attach = 0x15,
	keep_alive = 0x18
};

// Command submission queue NVM opcodes
enum struct nvme_cmd_opcode_t : uint8_t {
	flush = 0x00,
	write = 0x01,
	read = 0x02,
	write_uncorrectable = 0x4,
	compare = 0x05,
	write_zeros = 0x08,
	dataset_mgmt = 0x09,
	reservation_reg = 0x0D,
	reservation_rep = 0x0E,
	reservation_acq = 0x11,
	reservation_rel = 0x15
};

// NVM command structure with command factories
struct nvme_cmd_t {
	nvme_cmd_hdr_t hdr;

	// cmd dwords 10 thru 15
	uint32_t cmd_dword_10[6];

	// 5.11 Identify command
	static nvme_cmd_t create_identify(
			void *addr, uint8_t cns, uint8_t nsid)
	{
		nvme_cmd_t cmd{};
		cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
					uint8_t(nvme_admin_cmd_opcode_t::identify));
		cmd.hdr.nsid = nsid;
		cmd.hdr.dptr.prpp[0].addr = mphysaddr(addr);
		cmd.cmd_dword_10[0] =
				NVME_CMD_IDENT_CDW10_CNTID_n(0) |
				NVME_CMD_IDENT_CDW10_CNS_n(cns);
		return cmd;
	}

	static nvme_cmd_t create_sub_queue(
			void *addr, uint32_t size,
			uint16_t sqid, uint16_t cqid, uint8_t prio)
	{
		nvme_cmd_t cmd{};
		cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
					uint8_t(nvme_admin_cmd_opcode_t::create_sq));
		cmd.hdr.dptr.prpp[0].addr = mphysaddr(addr);
		cmd.cmd_dword_10[0] =
				NVME_CMD_CSQ_CDW10_QSIZE_n(size - 1) |
				NVME_CMD_CSQ_CDW10_QID_n(sqid);
		cmd.cmd_dword_10[1] =
				NVME_CMD_CSQ_CDW11_CQID_n(cqid) |
				NVME_CMD_CSQ_CDW11_QPRIO_n(prio) |
				NVME_CMD_CSQ_CDW11_PC_n(1);
		return cmd;
	}

	static nvme_cmd_t create_cmp_queue(
			void *addr, uint32_t size,
			uint16_t cqid, uint16_t intr)
	{
		nvme_cmd_t cmd{};
		cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
					uint8_t(nvme_admin_cmd_opcode_t::create_cq));
		cmd.hdr.dptr.prpp[0].addr = mphysaddr(addr);
		cmd.cmd_dword_10[0] =
				NVME_CMD_CCQ_CDW10_QSIZE_n(size - 1) |
				NVME_CMD_CCQ_CDW10_QID_n(cqid);
		cmd.cmd_dword_10[1] =
				NVME_CMD_CCQ_CDW11_IV_n(intr) |
				NVME_CMD_CCQ_CDW11_IEN_n(1) |
				NVME_CMD_CCQ_CDW11_PC_n(1);
		return cmd;
	}

	static nvme_cmd_t create_read(uint64_t lba, uint32_t count, uint8_t ns)
	{
		nvme_cmd_t cmd{};
		cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
					uint8_t(nvme_cmd_opcode_t::read));
		cmd.hdr.nsid = ns;
		cmd.cmd_dword_10[0] =
				NVME_CMD_READ_CDW10_SLBA_n(uint32_t(lba & 0xFFFFFFFF));
		cmd.cmd_dword_10[1] =
				NVME_CMD_READ_CDW11_SLBA_n(uint32_t(lba >> 32));
		cmd.cmd_dword_10[2] =
				NVME_CMD_READ_CDW12_NLB_n(count - 1);
		return cmd;
	}

	static nvme_cmd_t create_write(uint64_t lba, uint32_t count,
								   uint8_t ns, bool fua)
	{
		nvme_cmd_t cmd{};
		cmd.hdr.cdw0 = NVME_CMD_SDW0_OPC_n(
					uint8_t(nvme_cmd_opcode_t::read));
		cmd.hdr.nsid = ns;
		cmd.cmd_dword_10[0] =
				NVME_CMD_READ_CDW10_SLBA_n(uint32_t(lba & 0xFFFFFFFF));
		cmd.cmd_dword_10[1] =
				NVME_CMD_READ_CDW11_SLBA_n(uint32_t(lba >> 32));
		cmd.cmd_dword_10[2] =
				NVME_CMD_READ_CDW12_FUA_n((uint8_t)fua);
		return cmd;
	}

	static nvme_cmd_t create_flush(uint8_t ns)
	{
		nvme_cmd_t cmd{};
		assert(!"Unhandled!");
		// fixme
		return cmd;
	}

};

C_ASSERT(sizeof(nvme_cmd_t) == 64);
C_ASSERT(offsetof(nvme_cmd_t, cmd_dword_10) == 40);

// 4.6 Completion queue entry
struct nvme_cmp_t {
	// Command specific
	uint32_t cmp_dword[4];
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

// Identify controller data
struct nvme_identify_t {
	// PCI vendor ID
	uint16_t vid;

	// PCI subsystem vendor ID
	uint16_t ssvid;

	// Serial number
	char sn[20];

	// model number
	char mn[40];

	// Firmware revision
	uint64_t fr;

	// Recommended arbitration burst
	uint8_t rab;

	uint8_t ieee[3];

	// Controller multi-path I/O and namespace sharing capabilities
	uint8_t cmic;

	// Maximum data transfer size
	uint8_t mdts;

	// Controller ID
	uint16_t cntlid;

	// Version
	uint32_t ver;

	// RTD3 resume latency
	uint32_t rtd3r;

	// RTD3 entry latency
	uint32_t rtd3e;

	// Optional asynchronous events supported
	uint32_t oaes;

	// Controller attributes
	uint32_t ctratt;

	uint8_t reserved[256-100];

	// Optional admin command support
	uint16_t oacs;

	// Abort command limit
	uint8_t acl;

	// Asynchronous event request limit
	uint8_t aerl;

	// Firmware updates
	uint8_t frmw;

	// Log page attributes
	uint8_t lpa;

	// Error log page attributes
	uint8_t elpe;

	// Number of power states supported
	uint8_t npss;

	// Admin vendor specific command configuration
	uint8_t avscc;

	// Autonomous power state transition attributes
	uint8_t apsta;

	// Warning composite temperature threshold
	uint16_t wctemp;

	// Critical composite temperature threshold
	uint16_t cctemp;

	// maximum time for firmware activation
	uint16_t mtfa;

	// Host memory buffer preferred size
	uint32_t hmpre;

	// Host memory buffer minimum size
	uint32_t hmmin;

	// Total NVM capacity (bytes, 128-bit value)
	uint64_t tnvmcap_lo;
	uint64_t tnvmcap_hi;

	// Unallocated NVM capacity (bytes, 128-bit value)
	uint64_t unvmcap_lo;
	uint64_t unvmcap_hi;

	// Replay protected memory block support
	uint32_t rpmbs;

	//
	uint32_t reserved2;

	// Keepalive support
	uint16_t kas;

	char reserved3[190];

	// Submission queue entry size
	uint8_t sqes;

	// Completion queue entry size
	uint8_t cqes;

	// Maximum outstanding commands
	uint16_t maxcmd;

	// Number of namespaces
	uint32_t nn;

	// Optional nvm command support
	uint16_t oncs;

	// Fused operation support
	uint16_t fuses;

	// Format NVM attributes
	uint8_t vna;

	// Volatile write cache
	uint8_t vwc;

	// Atomic write unit normal
	uint16_t awun;

	// Atomic write unit power fail
	uint16_t awupf;

	// NVM vendor specific command confiuration
	uint8_t nvscc;

	uint8_t reserved4;

	// Acomit compare and write unit
	uint16_t acwu;

	uint16_t reserved5;

	// SGL support
	uint32_t sgls;

	uint8_t reserved6[768-540];

	// NVM subsystem qualified name
	uint8_t subnqn[256];

	uint8_t reserved7[2048-1024];

	// Power state descriptors
	uint8_t psd[32][32];

	// Vendor specific
	uint8_t vs[4096-3072];
};

C_ASSERT(offsetof(nvme_identify_t, fr) == 64);
C_ASSERT(offsetof(nvme_identify_t, ctratt) == 96);
C_ASSERT(offsetof(nvme_identify_t, oacs) == 256);
C_ASSERT(offsetof(nvme_identify_t, sqes) == 512);
C_ASSERT(sizeof(nvme_identify_t) == 4096);

struct nvme_ns_identify_t {
	// Namespace size
	uint64_t nsze;

	// Namespace capacity
	uint64_t ncap;

	// Namespace utilization
	uint64_t nuse;

	// Namespace features
	uint8_t nsfeat;

	// Number of LBA formats
	uint8_t nlbaf;

	// Formatted LBA size
	uint8_t flbas;

	// Metadata capabilities
	uint8_t mc;

	// End to end data protection capabilitities
	uint8_t dpc;

	// End to end protection type settings
	uint8_t dps;

	// Namespace multipath I/O and namespace sharing capabilities
	uint8_t nmic;

	// Reservation capabilities
	uint8_t rescap;

	// Format progress indicator
	uint8_t fpi;

	uint8_t reserved;

	// Namespace atomic write unit normal
	uint16_t nawun;

	// namespace atomic write unit power fail
	uint16_t nawupf;

	// namespace atomic compare and write unit
	uint16_t nacwu;

	// namespace atomic boundary size normal
	uint16_t nsabsn;

	// namespace atomic boundary offset
	uint16_t nsabo;

	// namespace atomic boundary size power fail
	uint16_t nabspf;

	uint16_t reserved2;

	// NVM capacity
	uint64_t nvmcap_lo;
	uint64_t nvmcap_hi;

	uint8_t reserved3[104-64];

	// namespace globally unique identifier
	uint8_t nguid[16];

	// ieee extended unique identifier
	uint64_t eui64;

	// LBA formats
	uint32_t lbaf[16];

	uint8_t reserved4[4096-192];
};

C_ASSERT(offsetof(nvme_ns_identify_t, nvmcap_lo) == 48);
C_ASSERT(offsetof(nvme_ns_identify_t, nguid) == 104);
C_ASSERT(offsetof(nvme_ns_identify_t, nlbaf) == 25);
C_ASSERT(offsetof(nvme_ns_identify_t, lbaf) == 128);
C_ASSERT(offsetof(nvme_ns_identify_t, reserved4) == 192);
C_ASSERT(sizeof(nvme_ns_identify_t) == 4096);

// ---------------------------------------------------------------------------
// VFS interface forward declarations
class nvme_if_t;
class nvme_dev_t;

// ---------------------------------------------------------------------------
// NVME queue helper - handles the circular queues for submission and
// completion queues, doorbell writes, and advancing submission queue head
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

	T* data()
	{
		return entries;
	}

	uint32_t get_tail() const
	{
		return tail;
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
	uint32_t enqueue(Args&& ...args)
	{
		int index = tail;
		T* item = new (entries + tail) T(forward<Args>(args)...);
		phase ^= set_tail(next(tail));
		return index;
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

	void take_until(uint16_t new_head)
	{
		while (head != new_head)
			take();
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

// Carries context information through device detection
// asynchronous commands
class nvme_detect_dev_ctx_t {
public:
	nvme_detect_dev_ctx_t(if_list_t& list)
		: list(list)
		, identify_data_physaddr(0)
		, identify_data(nullptr)
		, lock(0)
		, cur_ns(0)
		, done(false)
	{
		identify_data_physaddr = mm_alloc_contiguous(4096);

		NVME_TRACE("namespace identify data at physaddr=%lx\n",
				   identify_data_physaddr);

		if (!identify_data_physaddr)
			panic("Insufficient contiguous memory!\n");

		identify_data = mmap((void*)identify_data_physaddr, 4096,
							 PROT_READ, MAP_PHYSICAL, -1, 0);

		NVME_TRACE("namespace identify data at vaddr=%p\n",
				   (void*)identify_data);

		if (!identify_data_physaddr)
			panic("Failed to map identify data!\n");

		condvar_init(&done_cond);
	}

	~nvme_detect_dev_ctx_t()
	{
		condvar_destroy(&done_cond);
		munmap(identify_data, 4096);
		mm_free_contiguous(identify_data_physaddr, 4096);
	}

	void wait()
	{
		spinlock_lock_noirq(&lock);
		while (!done)
			condvar_wait_spinlock(&done_cond, &lock);
		spinlock_unlock_noirq(&lock);
	}

	void set_done()
	{
		spinlock_lock_noirq(&lock);
		done = true;
		spinlock_unlock_noirq(&lock);
		condvar_wake_all(&done_cond);
	}

	void *identify_data;
	if_list_t& list;
	uint8_t cur_ns;

private:
	uint64_t identify_data_physaddr;
	spinlock_t lock;
	condition_var_t done_cond;
	bool done;
};

// Completion callback
class nvme_callback_t {
public:
	typedef void (nvme_if_t::*member_t)(
			void *data, uint16_t cmd_id,
			int status_type, int status);

	nvme_callback_t()
		: member(nullptr)
		, data(nullptr)
	{
	}

	nvme_callback_t(member_t member, void *data)
		: member(member)
		, data(data)
	{
	}

	void operator()(nvme_if_t* owner, uint16_t cmd_id,
					int status_type, int status)
	{
		if (member)
			(owner->*member)(data, cmd_id, status_type, status);
	}

private:
	member_t member;
	void *data;
};

struct nvme_blocking_io_t {
	nvme_blocking_io_t()
		: expect_count(0)
		, done_count(0)
		, err(0)
	{
		mutex_init(&lock);
		condvar_init(&done_cond);
	}

	~nvme_blocking_io_t()
	{
		condvar_destroy(&done_cond);
		mutex_destroy(&lock);
	}

	mutex_t lock;
	condition_var_t done_cond;

	int expect_count;
	int done_count;
	int err;
};

enum struct nvme_op_t : uint8_t {
	read,
	write,
	trim,
	flush
};

typedef void (*nvme_async_callback_fn_t)(int error, uintptr_t arg);

struct nvme_request_callback_t {
	nvme_async_callback_fn_t callback;
	uintptr_t callback_arg;
	int error;
};

struct nvme_request_t {
	void *data;
	int64_t count;
	uint64_t lba;
	nvme_request_callback_t callback;
	nvme_op_t op;
	bool fua;
};

// ---------------------------------------------------------------------------
// VFS

class nvme_if_factory_t : public storage_if_factory_t {
public:
	nvme_if_factory_t() : storage_if_factory_t("nvme") {}
private:
	virtual if_list_t detect(void) final;
};

static nvme_if_factory_t nvme_if_factory;
STORAGE_REGISTER_FACTORY(nvme_if);

// NVMe interface instance
class nvme_if_t : public storage_if_base_t {
public:
	void init(pci_dev_iterator_t const& pci_iter);

private:
	STORAGE_IF_IMPL

	friend class nvme_dev_t;

	static isr_context_t *irq_handler(int irq, isr_context_t *ctx);
	void irq_handler(int irq_offset);

	uint32_t volatile* doorbell_ptr(bool completion, size_t queue);

	unsigned io(uint8_t ns, nvme_request_t &request, uint8_t log2_sectorsize);

	void io_handler(void *data, uint16_t cmd_id, int status_type, int status);

	nvme_mmio_t volatile *mmio_base;
	pci_config_hdr_t config;
	pci_irq_range_t irq_range;

	size_t doorbell_shift;

	// Queue count, not including admin queue
	size_t queue_count;

	uintptr_t queue_memory_physaddr;
	void* queue_memory;

	uint32_t host_buffer_size;
	uintptr_t host_buffer_physaddr;

	vector<uint32_t> namespaces;

	typedef nvme_queue_t<nvme_cmd_t> sub_queue_t;
	typedef nvme_queue_t<nvme_cmp_t> cmp_queue_t;

	class queue_state_t {
	public:
		queue_state_t()
			: lock(0)
		{
			condvar_init(&not_full);
			condvar_init(&not_empty);
		}

		~queue_state_t()
		{
			condvar_destroy(&not_full);
			condvar_destroy(&not_empty);
		}

		void init(size_t count)
		{
			cmp_handlers.resize(count);

			// Allocate enough memory for 4 PRP list entries per slot
			prp_lists = (uint64_t*)mmap(
						nullptr, count * sizeof(*prp_lists) * 16,
						PROT_READ | PROT_WRITE, MAP_POPULATE, -1, 0);
		}

		void submit_cmd(nvme_cmd_t&& cmd,
						nvme_callback_t::member_t callback = nullptr,
						void *data = nullptr,
						mmphysrange_t *ranges = nullptr,
						size_t range_count = 0)
		{
			spinlock_lock_noirq(&lock);
			while (sub_queue.is_full())
				condvar_wait_spinlock(&not_full, &lock);

			int index = sub_queue.get_tail();

			NVME_CMD_SDW0_CID_SET(cmd.hdr.cdw0, index);

			if (range_count > 2) {
				cmd.hdr.dptr.prpp[0].addr = ranges[0].physaddr;

				uint64_t *prp_list = prp_lists + index * 16;

				for (size_t i = 1; i < range_count; ++i)
					prp_list[i-1] = ranges[i].physaddr;

				cmd.hdr.dptr.prpp[1].addr = mphysaddr(prp_list);
			} else if (range_count > 1) {
				cmd.hdr.dptr.prpp[0].addr = ranges[0].physaddr;
				cmd.hdr.dptr.prpp[1].addr = ranges[1].physaddr;
			} else if (range_count > 0) {
				cmd.hdr.dptr.prpp[0].addr = ranges[0].physaddr;
			}

			cmp_handlers[index] = nvme_callback_t(callback, data);

			sub_queue.enqueue(cmd);

			spinlock_unlock_noirq(&lock);
		}

		void advance_head(uint16_t new_head)
		{
			spinlock_lock_noirq(&lock);
			sub_queue.take_until(new_head);
			spinlock_unlock(&lock);
			condvar_wake_all(&not_full);
		}

		void invoke_completion(nvme_if_t* owner, uint16_t cmd_id,
							   int status_type, int status)
		{
			cmp_handlers[cmd_id](owner, cmd_id, status_type, status);
		}

		sub_queue_t sub_queue;
		cmp_queue_t cmp_queue;

	private:
		vector<nvme_callback_t> cmp_handlers;
		uint64_t* prp_lists;

		spinlock_t lock;
		condition_var_t not_full;
		condition_var_t not_empty;
	};

	// Handle controller identify
	void identify_handler(
			void *data, uint16_t cmd_id, int status_type, int status);

	// Handle namespace list identify
	void identify_ns_id_handler(
			void *data, uint16_t cmd_id, int status_type, int status);

	// Handle namespace identify
	void identify_ns_handler(
			void *data, uint16_t cmd_id, int status_type, int status);

	unique_ptr<queue_state_t[]> queues;
};

class nvme_dev_t : public storage_dev_base_t {
public:
	void init(nvme_if_t *parent, uint8_t ns, uint8_t log2_sectorsize);

private:
	STORAGE_DEV_IMPL

	int64_t io(void *data, int64_t count,
			   uint64_t lba, bool fua, nvme_op_t op);

	static void async_complete(int error, uintptr_t arg);


	nvme_if_t *parent;
	uint8_t ns;
	uint8_t log2_sectorsize;
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

	NVME_TRACE("enumerating PCI busses for devices...\n");

	if (!pci_enumerate_begin(
				&pci_iter,
				PCI_DEV_CLASS_STORAGE,
				PCI_SUBCLASS_STORAGE_NVM))
		return list;

	do {
		assert(pci_iter.dev_class == PCI_DEV_CLASS_STORAGE);
		assert(pci_iter.subclass == PCI_SUBCLASS_STORAGE_NVM);

		// Make sure it is an NVMe device
		if (pci_iter.config.prog_if != PCI_PROGIF_STORAGE_NVM_NVME)
			continue;

		NVME_TRACE("found device!\n");


		if (nvme_count < countof(nvme_devices)) {
			nvme_if_t *self = nvme_devices + nvme_count++;

			self->init(pci_iter);
		}
	} while (pci_enumerate_next(&pci_iter));

	list.count = nvme_count - start_at;

	NVME_TRACE("found %u drives (namespaces)\n", list.count);

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
	size_t queue_slots = 1024;
	assert(queue_slots <= 4096);
	size_t max_queue_slots = NVME_CAP_MQES_GET(mmio_base->cap) + 1;

	if (queue_slots > max_queue_slots)
		queue_slots = max_queue_slots;

	// Size of one queue, in bytes
	size_t queue_bytes = queue_slots * sizeof(nvme_cmd_t) +
			queue_slots * sizeof(nvme_cmp_t);

	queue_count = thread_get_cpu_count() + 1;
	queue_bytes *= queue_count;

	queue_memory_physaddr = mm_alloc_contiguous(queue_bytes);

	queue_memory = mmap(
				(void*)queue_memory_physaddr, queue_bytes,
				PROT_READ | PROT_WRITE,
				MAP_PHYSICAL, -1, 0);

	memset(queue_memory, 0, queue_bytes);

	// 7.6.1 3) The admin queue should be configured
	mmio_base->aqa =
			NVME_AQA_ACQS_n(queue_slots) |
			NVME_AQA_ASQS_n(queue_slots);

	// Submission queue address
	mmio_base->asq = (uint64_t)queue_memory_physaddr;

	// Completion queue address
	mmio_base->acq = (uint64_t)queue_memory_physaddr +
			(sizeof(nvme_cmd_t) * queue_slots * queue_count);

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

	// Initialize queues
	queues.reset(new queue_state_t[queue_count + 1]);

	nvme_cmd_t *sub_queue_ptr = (nvme_cmd_t*)queue_memory;
	for (size_t i = 0; i < queue_count; ++i) {
		queues[i].sub_queue.init(sub_queue_ptr, queue_slots,
								 nullptr, doorbell_ptr(false, i), 1);
		sub_queue_ptr += queue_slots;
	}

	nvme_cmp_t *cmp_queue_ptr = (nvme_cmp_t*)sub_queue_ptr;
	for (size_t i = 0; i < queue_count; ++i) {
		queues[i].cmp_queue.init(cmp_queue_ptr, queue_slots,
								 doorbell_ptr(true, i), nullptr, 1);
		cmp_queue_ptr += queue_slots;
	}

	for (size_t i = 0; i < queue_count; ++i)
		queues[i].init(queue_slots);

	uintptr_t identify_physaddr = mm_alloc_contiguous(4096);

	void* identify = mmap((void*)identify_physaddr, 4096,
						  PROT_READ | PROT_WRITE,
						  MAP_PHYSICAL, -1, 0);

	// 5.11 Execute identify controller command
	queues[0].submit_cmd(nvme_cmd_t::create_identify(identify, 1, 0),
						 &nvme_if_t::identify_handler, identify);

	// Create completion queues
	for (size_t i = 1; i < queue_count; ++i) {
		queues[0].submit_cmd(nvme_cmd_t::create_cmp_queue(
								 queues[i].cmp_queue.data(),
								 queue_slots, i, i));
	}

	// Create submission queues
	for (size_t i = 1; i < queue_count; ++i) {
		queues[0].submit_cmd(nvme_cmd_t::create_sub_queue(
								 queues[i].sub_queue.data(),
								 queue_slots, i, i, 2));
	}

	NVME_TRACE("interface initialization success\n");
}

void nvme_if_t::identify_ns_id_handler(
		void *data, uint16_t cmd_id, int status_type, int status)
{
	NVME_TRACE("enumerating namespaces\n");

	auto ctx = (nvme_detect_dev_ctx_t*)data;
	auto ns_list = (uint32_t*)ctx->identify_data;

	int count;
	for (count = 0; count < 1024; ++count) {
		if (ns_list[count] == 0)
			break;
	}

	NVME_TRACE("Found %d namespaces\n", count);

	namespaces.reserve(count);

	for (int i = 0; i < count; ++i)
		namespaces.push_back(ns_list[i]);

	ctx->cur_ns = 0;
	uint8_t ns = namespaces[ctx->cur_ns];
	NVME_TRACE("issuing identify namespace %u\n", ns);
	queues[0].submit_cmd(nvme_cmd_t::create_identify(
							 ctx->identify_data, 0, ns),
						 &nvme_if_t::identify_ns_handler, ctx);
}

void nvme_if_t::identify_ns_handler(
		void *data, uint16_t cmd_id, int status_type, int status)
{
	auto ctx = (nvme_detect_dev_ctx_t*)data;
	auto ns_ident = (nvme_ns_identify_t*)ctx->identify_data;

	size_t cur_format_index = NVME_NS_IDENT_FLBAS_LBAIDX_GET(
				ns_ident->flbas);
	uint32_t lba_format = ns_ident->lbaf[cur_format_index];
	uint8_t log2_sectorsize = NVME_NS_IDENT_LBAF_LBADS_GET(lba_format);

	nvme_dev_t *drive = nvme_drives + nvme_drive_count++;

	drive->init(this, namespaces[ctx->cur_ns], log2_sectorsize);

	// Enumerate next namespace if there are more
	if (++ctx->cur_ns < namespaces.size()) {
		uint8_t ns = namespaces[ctx->cur_ns];
		NVME_TRACE("issuing identify namespace %u\n", ns);

		queues[0].submit_cmd(nvme_cmd_t::create_identify(
							 ctx->identify_data, 0, ns),
						 &nvme_if_t::identify_ns_handler, ctx);
	} else {
		ctx->set_done();
	}
}

void nvme_if_t::identify_handler(
		void *data, uint16_t cmd_id, int status_type, int status)
{
	nvme_identify_t* identify = (nvme_identify_t*)data;

	host_buffer_size = identify->hmpre;
	if (host_buffer_size > 0) {
		host_buffer_physaddr = (uintptr_t)
				mm_alloc_contiguous(host_buffer_size);
	}

	mm_free_contiguous(mphysaddr(data), 4096);
	munmap(data, 4096);
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

	NVME_TRACE("enumerating namespaces\n");

	nvme_detect_dev_ctx_t ctx(list);

	// Get namespace list
	queues[0].submit_cmd(nvme_cmd_t::create_identify(
							 ctx.identify_data, 2, 0),
						 &nvme_if_t::identify_ns_id_handler,
						 &ctx);

	ctx.wait();

	list.count = nvme_drive_count - start_at;

	NVME_TRACE("enumerating namespaces complete,"
			   " found %u namespaces\n", ctx.list.count);

	return list;
}

void nvme_dev_t::init(nvme_if_t *parent,
					  uint8_t ns, uint8_t log2_sectorsize)
{
	this->parent = parent;
	this->ns = ns;
	this->log2_sectorsize = log2_sectorsize;
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

		dev->irq_handler(irq_offset);
	}

	return ctx;
}

void nvme_if_t::irq_handler(int irq_offset)
{
	NVME_TRACE("received IRQ\n");

	for (int i = 0; i < queue_count; ++i)
	{
		queue_state_t& queue = queues[i];
		cmp_queue_t& cmp_queue = queue.cmp_queue;
		int phase = cmp_queue.get_phase();
		for (;;) {
			nvme_cmp_t packet = cmp_queue.peek();

			// Done when phase does not match expected phase
			if (NVME_CMP_DW3_P_GET(packet.cmp_dword[3]) != phase)
				break;

			cmp_queue.take();

			// Decode submission queue for which command has completed
			size_t sub_queue_id = NVME_CMP_DW2_SQID_GET(packet.cmp_dword[2]);
			queue_state_t& sub_queue_state = queues[sub_queue_id];

			// Get submission queue head
			size_t sub_queue_head = NVME_CMP_DW2_SQHD_GET(packet.cmp_dword[2]);
			sub_queue_state.advance_head(sub_queue_head);

			bool dnr = NVME_CMP_DW3_DNR_GET(packet.cmp_dword[3]);
			int status_type = NVME_CMP_DW3_SCT_GET(packet.cmp_dword[3]);
			int status = NVME_CMP_DW3_SC_GET(packet.cmp_dword[3]);
			uint16_t cmd_id = NVME_CMP_DW3_CID_GET(packet.cmp_dword[3]);

			queue.invoke_completion(this, cmd_id, status_type, status);
		}
	}
}

uint32_t volatile* nvme_if_t::doorbell_ptr(bool completion, size_t queue)
{
	uint32_t volatile *doorbells = (uint32_t volatile *)(mmio_base + 1);
	return doorbells + ((queue << doorbell_shift) + completion);
}

unsigned nvme_if_t::io(uint8_t ns, nvme_request_t &request,
					   uint8_t log2_sectorsize)
{
//	cpu_scoped_irq_disable intr_were_enabled;

	size_t cur_cpu = thread_cpu_number();

	int queue_index;

	if (cur_cpu < queue_count - 1)
		queue_index = cur_cpu + 1;
	else
		queue_index = cur_cpu % (queue_count - 1) + 1;

	size_t bytes = request.count << log2_sectorsize;

	uint32_t expect = 0;

	while (request.count > 0) {
		++expect;

		size_t chunk;
		size_t pages;
		size_t lba_count;
		size_t range_count;
		mmphysrange_t ranges[16];
		uintptr_t prps[2];
		nvme_cmd_t cmd;

		switch (request.op) {
		case nvme_op_t::read:
		case nvme_op_t::write:
			chunk = min(bytes, size_t(0x10000));

			pages = (chunk + PAGE_SIZE - 1) >> PAGE_SCALE;

			lba_count = chunk >> log2_sectorsize;
			request.count -= lba_count;

			range_count = mphysranges(ranges, countof(ranges),
											 request.data, chunk,
											 PAGE_SIZE);
			break;

		case nvme_op_t::flush:
			lba_count = 0;

		default:
			panic("Unhandled operation! op=%d", (int)request.op);
		}

		switch (request.op) {
		case nvme_op_t::read:
			cmd = nvme_cmd_t::create_read(
					request.lba, lba_count, ns);
			break;

		case nvme_op_t::write:
			cmd = nvme_cmd_t::create_write(
					request.lba, lba_count, ns, request.fua);
			break;

		case nvme_op_t::flush:
			cmd = nvme_cmd_t::create_flush(ns);
			break;

		}

		queues[queue_index].submit_cmd(
					move(cmd), &nvme_if_t::io_handler, &request,
					ranges, range_count);
	}

	return expect;
}

void nvme_if_t::io_handler(void *data, uint16_t cmd_id,
						   int status_type, int status)
{
	nvme_request_t* request = (nvme_request_t*)data;

	int err = status_type != 0 || status != 0;

	request->callback.callback(err, request->callback.callback_arg);
}

int64_t nvme_dev_t::io(void *data, int64_t count,
					   uint64_t lba, bool fua, nvme_op_t op)
{
   cpu_scoped_irq_disable intr_were_enabled;
   nvme_blocking_io_t block_state;

   mutex_lock(&block_state.lock);

   nvme_request_t request;
   request.data = data;
   request.count = count;
   request.lba = lba;
   request.op = op;
   request.fua = fua;
   request.callback.callback = async_complete;
   request.callback.callback_arg = uintptr_t(&block_state);

   block_state.expect_count = parent->io(ns, request, log2_sectorsize);

   while (block_state.done_count != block_state.expect_count)
	   condvar_wait(&block_state.done_cond, &block_state.lock);

   mutex_unlock(&block_state.lock);

   return block_state.err;
}

void nvme_dev_t::async_complete(int error, uintptr_t arg)
{
	nvme_blocking_io_t *state = (nvme_blocking_io_t*)arg;

	mutex_lock_noyield(&state->lock);
	if (error)
		state->err = error;

	++state->done_count;
	bool done = (state->done_count == state->expect_count);

	mutex_unlock(&state->lock);

	if (done)
		condvar_wake_one(&state->done_cond);
}

int64_t nvme_dev_t::read_blocks(
		void *data, int64_t count,
		uint64_t lba)
{
	return io(data, count, lba, false, nvme_op_t::read);
}

int64_t nvme_dev_t::write_blocks(
		void const *data, int64_t count,
		uint64_t lba, bool fua)
{
	return io((void*)data, count, lba, fua, nvme_op_t::write);
}

int nvme_dev_t::flush()
{
	return io(nullptr, 0, 0, false, nvme_op_t::flush);
}

int64_t nvme_dev_t::trim_blocks(int64_t count,
		uint64_t lba)
{
	return io(nullptr, count, lba, false, nvme_op_t::trim);
}

long nvme_dev_t::info(storage_dev_info_t key)
{
	switch (key) {
	case STORAGE_INFO_BLOCKSIZE:
		return 1L << log2_sectorsize;

	case STORAGE_INFO_HAVE_TRIM:
		// FIXME
		return 0;

	default:
		return 0;
	}
}
