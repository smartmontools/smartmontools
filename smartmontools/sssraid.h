/*
 * sssraid.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2022 3SNIC Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _SSSRAID_H_
#define _SSSRAID_H_
#include <stdint.h>
#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t
#define ADM_MAX_DATA_SIZE 0x1000U		// 4096
#define ADM_SCSI_CDB_MAX_LEN 32
#define ADM_SCSI_CDB_SENSE_MAX_LEN 96
#define ADM_SCSI_CDB_SENSE_LEN32 32
#define ADM_CMD_SHOW_PDLIST 0x502		// 1282
#define ADM_CMD_SCSI_PASSTHROUGH 0x51e	// 1310
#define BSG_APPEND_TIMEOUT_MS 600000
#define DEFAULT_CONMMAND_TIMEOUT_MS 180000
#define ADM_BSG_MSGCODE_SCSI_PTHRU  1

#define ADM_RAID_SET 0xc0
#define ADM_RAID_WRITE 0xc1
#define ADM_RAID_READ 0xc2
#define ADM_RAID_PARAM_WRITE 0xc3
#define ADM_RAID_READ_FROM_CQE 0xc4

// pdlist device type interface
enum adm_pdlist_intf {
    ADM_DEVICE_TYPE_SAS = 1,
    ADM_DEVICE_TYPE_EXP = 2,
    ADM_DEVICE_TYPE_SATA = 8,
    ADM_DEVICE_TYPE_PLANE = 9,
    ADM_DEVICE_TYPE_NVME = 10,
    ADM_DEVICE_TYPE_UNKNOW,
    ADM_DEVICE_TYPE_BUTT
};

struct cmd_pdlist_idx {
    u16 start_idx;
    u16 count;
    u32 rsvd;
};

struct cmd_pdlist_entry {
    u16 enc_id;
    u16 slot_id;
    u8 interface;
    u8 media_type;
    u8 logi_type;
    u8 logi_status;
	u32 reserve[26];
};
#define CMD_PDS_MAX_NUM 256U
#define CMD_PDLIST_ONCE_NUM ((ADM_MAX_DATA_SIZE - 64) / sizeof(struct cmd_pdlist_entry))

struct cmd_show_pdlist {
    u16 num;
    u16 rsvd0;
    u32 rsvd1[15];
    struct cmd_pdlist_entry disks[CMD_PDLIST_ONCE_NUM];
};

struct multi_disk_location {
    u16 enc_id;
    u16 slot_id;
    u16 did;
    u8 flag;
    u8 rsvd;
};

struct cmd_scsi_passthrough {
    struct multi_disk_location loc;
    u8 lun;
    u8 cdb_len;
    u8 sense_buffer_len;
    u8 rsvd0;
    u8 cdb[32];
    u32 rsvd1;
    u8 *sense_buffer;
};

struct sssraid_passthru_common_cmd {
	u8	opcode;
	u8	flags;
	u16	rsvd0;
	u32	nsid;
	union {
		struct {
			u16 subopcode;
			u16 rsvd1;
		} info_0;
		u32 cdw2;
	};
	union {
		struct {
			u16 data_len;
			u16 param_len;
		} info_1;
		u32 cdw3;
	};
	u64 metadata;

	u64 addr;
    u32 metadata_len;
    u32 data_len;

	u32 cdw10;
	u32 cdw11;
	u32 cdw12;
	u32 cdw13;
	u32 cdw14;
	u32 cdw15;
	u32 timeout_ms;
	u32 result0;
	u32 result1;
};

struct sssraid_ioq_passthru_cmd {
	u8  opcode;
	u8  flags;
	u16 rsvd0;
	u32 nsid;
	union {
		struct {
			u16 res_sense_len;
			u8  cdb_len;
			u8  rsvd0;
		} info_0;
		u32 cdw2;
	};
	union {
		struct {
			u16 subopcode;
			u16 rsvd1;
		} info_1;
		u32 cdw3;
	};
	union {
		struct {
			u16 rsvd;
			u16 param_len;
		} info_2;
		u32 cdw4;
	};
	u32 cdw5;
	u64 addr;
	u64 prp2;
	union {
		struct {
			u16 eid;
			u16 sid;
		} info_3;
		u32 cdw10;
	};
	union {
		struct {
			u16 did;
			u8  did_flag;
			u8  rsvd2;
		} info_4;
		u32 cdw11;
	};
	u32 cdw12;
	u32 cdw13;
	u32 cdw14;
	u32 data_len;
	u32 cdw16;
	u32 cdw17;
	u32 cdw18;
	u32 cdw19;
	u32 cdw20;
	u32 cdw21;
	u32 cdw22;
	u32 cdw23;
	u64 sense_addr;
	u32 cdw26[4];
	u32 timeout_ms;
	u32 result0;
	u32 result1;
};

struct bsg_ioctl_cmd {
    u32 msgcode;
    u32 control;
    union {
        struct sssraid_passthru_common_cmd ioctl_r64;
        struct sssraid_ioq_passthru_cmd ioctl_pthru;
    };
};

#endif // _SSSRAID_H
