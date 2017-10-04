#pragma once
#include "types.h"
#include "assert.h"

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
			void *addr, uint8_t cns, uint8_t nsid);

	static nvme_cmd_t create_sub_queue(
			void *addr, uint32_t size,
			uint16_t sqid, uint16_t cqid, uint8_t prio);

	static nvme_cmd_t create_cmp_queue(
			void *addr, uint32_t size,
			uint16_t cqid, uint16_t intr);

	static nvme_cmd_t create_read(
			uint64_t lba, uint32_t count, uint8_t ns);

	static nvme_cmd_t create_write(
			uint64_t lba, uint32_t count, uint8_t ns, bool fua);

	static nvme_cmd_t create_trim(
			uint64_t lba, uint32_t count, uint8_t ns, bool fua);

	static nvme_cmd_t create_flush(uint8_t ns);
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

// 6.7 Dataset management - figure 165

struct nvme_dataset_range_t {
	// NVME_CMD_DSMGMT_CA_*
	uint32_t attr;

	uint32_t lba_count;
	uint64_t starting_lba;
};
