// ptbl_ent
.struct 0
ptbl_ent_start:
ptbl_ent_bootflag:
	.struct ptbl_ent_bootflag + 1
ptbl_ent_sthead:
	.struct ptbl_ent_sthead + 1
ptbl_ent_stseccyl:
	.struct ptbl_ent_stseccyl + 2
ptbl_ent_sysid:
	.struct ptbl_ent_sysid + 1
ptbl_ent_enhead:
	.struct ptbl_ent_enhead + 1
ptbl_ent_enseccyl:
	.struct ptbl_ent_enseccyl + 2
ptbl_ent_stsec:
	.struct ptbl_ent_stsec + 4
ptbl_ent_numsec:
	.struct ptbl_ent_numsec + 4
ptbl_ent_end:
ptbl_ent_length = ptbl_ent_end - ptbl_ent_start
