
// dap
.struct 0
dap_start:
dap_sizeof_packet:
	.struct dap_sizeof_packet + 1
dap_reserved:
	.struct dap_reserved + 1
dap_block_count:
	.struct dap_block_count + 2
dap_address:
	.struct dap_address + 4
dap_lba:
	.struct dap_lba + 8
dap_end:
dap_length = dap_end - dap_start
