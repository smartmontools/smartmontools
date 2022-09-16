/*
 * os_freebsd.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-8 Eduard Martinescu
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2003 Paul Saab
 * Copyright (c) 2003 Vinod Kashyap
 * Copyright (c) 2000 BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Copyright (c) 2004-05 Applied Micro Circuits Corporation.
 * Copyright (c) 2004-05 Vinod Kashyap
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef OS_FREEBSD_H_
#define OS_FREEBSD_H_

#define OS_FREEBSD_H_CVSID "$Id$"

#define MAX_NUM_DEV 26

#ifdef  HAVE_SYS_TWEREG_H
#include <sys/twereg.h>
#else
/**
 *  The following cut out of twereg.h
 *
 */
#if __FreeBSD_version < 500040
#define __packed __attribute__((__packed__))
#endif

#define TWE_MAX_SGL_LENGTH		62
#define TWE_MAX_ATA_SGL_LENGTH		60
#define TWE_OP_ATA_PASSTHROUGH		0x11

/* scatter/gather list entry */
typedef struct
{
    u_int32_t	address;
    u_int32_t	length;
} __packed TWE_SG_Entry;

typedef struct {
    u_int8_t	opcode:5;		/* TWE_OP_INITCONNECTION */
    u_int8_t	res1:3;		
    u_int8_t	size;
    u_int8_t	request_id;
    u_int8_t	res2:4;
    u_int8_t	host_id:4;
    u_int8_t	status;
    u_int8_t	flags;
    u_int16_t	message_credits;
    u_int32_t	response_queue_pointer;
} __packed TWE_Command_INITCONNECTION;

typedef struct
{
    u_int8_t	opcode:5;		/* TWE_OP_READ/TWE_OP_WRITE */
    u_int8_t	res1:3;
    u_int8_t	size;
    u_int8_t	request_id;
    u_int8_t	unit:4;
    u_int8_t	host_id:4;
    u_int8_t	status;
    u_int8_t	flags;
    u_int16_t	block_count;
    u_int32_t	lba;
    TWE_SG_Entry sgl[TWE_MAX_SGL_LENGTH];
} __packed TWE_Command_IO;

typedef struct
{
    u_int8_t	opcode:5;		/* TWE_OP_HOTSWAP */
    u_int8_t	res1:3;
    u_int8_t	size;
    u_int8_t	request_id;
    u_int8_t	unit:4;
    u_int8_t	host_id:4;
    u_int8_t	status;
    u_int8_t	flags;
    u_int8_t	action;
#define TWE_OP_HOTSWAP_REMOVE		0x00	/* remove assumed-degraded unit */
#define TWE_OP_HOTSWAP_ADD_CBOD		0x01	/* add CBOD to empty port */
#define TWE_OP_HOTSWAP_ADD_SPARE	0x02	/* add spare to empty port */
    u_int8_t	aport;
} __packed TWE_Command_HOTSWAP;

typedef struct
{
    u_int8_t	opcode:5;		/* TWE_OP_SETATAFEATURE */
    u_int8_t	res1:3;
    u_int8_t	size;
    u_int8_t	request_id;
    u_int8_t	unit:4;
    u_int8_t	host_id:4;
    u_int8_t	status;
    u_int8_t	flags;
    u_int8_t	feature;
#define TWE_OP_SETATAFEATURE_WCE	0x02
#define TWE_OP_SETATAFEATURE_DIS_WCE	0x82
    u_int8_t	feature_mode;
    u_int16_t	all_units;
    u_int16_t	persistence;
} __packed TWE_Command_SETATAFEATURE;

typedef struct
{
    u_int8_t	opcode:5;		/* TWE_OP_CHECKSTATUS */
    u_int8_t	res1:3;
    u_int8_t	size;
    u_int8_t	request_id;
    u_int8_t	unit:4;
    u_int8_t	res2:4;
    u_int8_t	status;
    u_int8_t	flags;
    u_int16_t	target_status;		/* set low byte to target request's ID */
} __packed TWE_Command_CHECKSTATUS;

typedef struct
{
    u_int8_t	opcode:5;		/* TWE_OP_GETPARAM, TWE_OP_SETPARAM */
    u_int8_t	res1:3;
    u_int8_t	size;
    u_int8_t	request_id;
    u_int8_t	unit:4;
    u_int8_t	host_id:4;
    u_int8_t	status;
    u_int8_t	flags;
    u_int16_t	param_count;
    TWE_SG_Entry sgl[TWE_MAX_SGL_LENGTH];
} __packed TWE_Command_PARAM;

typedef struct
{
    u_int8_t	opcode:5;		/* TWE_OP_REBUILDUNIT */
    u_int8_t	res1:3;
    u_int8_t	size;
    u_int8_t	request_id;
    u_int8_t	src_unit:4;
    u_int8_t	host_id:4;
    u_int8_t	status;
    u_int8_t	flags;
    u_int8_t	action:7;
#define TWE_OP_REBUILDUNIT_NOP		0
#define TWE_OP_REBUILDUNIT_STOP		2	/* stop all rebuilds */
#define TWE_OP_REBUILDUNIT_START	4	/* start rebuild with lowest unit */
#define TWE_OP_REBUILDUNIT_STARTUNIT	5	/* rebuild src_unit (not supported) */
    u_int8_t	cs:1;				/* request state change on src_unit */
    u_int8_t	logical_subunit;		/* for RAID10 rebuild of logical subunit */
} __packed TWE_Command_REBUILDUNIT;

typedef struct
{
    u_int8_t	opcode:5;
    u_int8_t	sgl_offset:3;
    u_int8_t	size;
    u_int8_t	request_id;
    u_int8_t	unit;
    u_int8_t	status;
    u_int8_t	flags;
    u_int16_t	param;
    u_int16_t	features;
    u_int16_t	sector_count;
    u_int16_t	sector_num;
    u_int16_t	cylinder_lo;
    u_int16_t	cylinder_hi;
    u_int8_t	drive_head;
    u_int8_t	command;
    TWE_SG_Entry sgl[TWE_MAX_ATA_SGL_LENGTH];
} __packed TWE_Command_ATA;

typedef struct
{
    u_int8_t	opcode:5;
    u_int8_t	sgl_offset:3;
    u_int8_t	size;
    u_int8_t	request_id;
    u_int8_t	unit:4;
    u_int8_t	host_id:4;
    u_int8_t	status;
    u_int8_t	flags;
#define TWE_FLAGS_SUCCESS	0x00
#define TWE_FLAGS_INFORMATIONAL	0x01
#define TWE_FLAGS_WARNING	0x02
#define TWE_FLAGS_FATAL		0x03
#define TWE_FLAGS_PERCENTAGE	(1<<8)	/* bits 0-6 indicate completion percentage */
    u_int16_t	count;			/* block count, parameter count, message credits */
} __packed TWE_Command_Generic;

/* command packet - must be TWE_ALIGNMENT aligned */
typedef union
{
    TWE_Command_INITCONNECTION	initconnection;
    TWE_Command_IO		io;
    TWE_Command_PARAM		param;
    TWE_Command_CHECKSTATUS	checkstatus;
    TWE_Command_REBUILDUNIT	rebuildunit;
    TWE_Command_SETATAFEATURE	setatafeature;
    TWE_Command_ATA		ata;
    TWE_Command_Generic		generic;
    u_int8_t			pad[512];
} TWE_Command;

/* response queue entry */
typedef union
{
    struct 
    {
	u_int32_t	undefined_1:4;
	u_int32_t	response_id:8;
	u_int32_t	undefined_2:20;
    } u;
    u_int32_t	value;
} TWE_Response_Queue;

#endif

#ifdef  HAVE_SYS_TWEIO_H
#include <sys/tweio.h>
#else
/*
 * Following cut out of tweio.h
 *
 */
/*
 * User-space command
 *
 * Note that the command's scatter/gather list will be computed by the
 * driver, and cannot be filled in by the consumer.
 */
struct twe_usercommand {
    TWE_Command	tu_command;	/* command ready for the controller */
    void	*tu_data;	/* pointer to data in userspace */
    size_t	tu_size;	/* userspace data length */
};

#define TWEIO_COMMAND		_IOWR('T', 100, struct twe_usercommand)

#endif

#ifdef  HAVE_SYS_TW_OSL_IOCTL_H
#include <sys/tw_osl_ioctl.h>
#else
/*
 * Following cut out of tw_osl_types.h
 *
 */

typedef void			TW_VOID;
typedef char			TW_INT8;
typedef unsigned char		TW_UINT8;
typedef short			TW_INT16;
typedef unsigned short		TW_UINT16;
typedef int			TW_INT32;
typedef unsigned int		TW_UINT32;
typedef long long		TW_INT64;
typedef unsigned long long	TW_UINT64;

/*
 * Following cut out of tw_cl_share.h
 *
 */

#pragma pack(1)

struct tw_cl_event_packet {
	TW_UINT32	sequence_id;
	TW_UINT32	time_stamp_sec;
	TW_UINT16	aen_code;
	TW_UINT8	severity;
	TW_UINT8	retrieved;
	TW_UINT8	repeat_count;
	TW_UINT8	parameter_len;
	TW_UINT8	parameter_data[98];
	TW_UINT32	event_src;
	TW_UINT8	severity_str[20];
};

#pragma pack()

/*
 * Following cut out of tw_cl_fwif.h
 *
 */

#define TWA_FW_CMD_ATA_PASSTHROUGH		0x11

#define TWA_SENSE_DATA_LENGTH		18

#pragma pack(1)
/* 7000 structures. */
struct tw_cl_command_init_connect {
	TW_UINT8	res1__opcode;	/* 3:5 */
	TW_UINT8	size;
	TW_UINT8	request_id;
	TW_UINT8	res2;
	TW_UINT8	status;
	TW_UINT8	flags;
	TW_UINT16	message_credits;
	TW_UINT32	features;
	TW_UINT16	fw_srl;
	TW_UINT16	fw_arch_id;
	TW_UINT16	fw_branch;
	TW_UINT16	fw_build;
	TW_UINT32	result;
};


/* Structure for downloading firmware onto the controller. */
struct tw_cl_command_download_firmware {
	TW_UINT8	sgl_off__opcode;/* 3:5 */
	TW_UINT8	size;
	TW_UINT8	request_id;
	TW_UINT8	unit;
	TW_UINT8	status;
	TW_UINT8	flags;
	TW_UINT16	param;
	TW_UINT8	sgl[1];
};


/* Structure for hard resetting the controller. */
struct tw_cl_command_reset_firmware {
	TW_UINT8	res1__opcode;	/* 3:5 */
	TW_UINT8	size;
	TW_UINT8	request_id;
	TW_UINT8	unit;
	TW_UINT8	status;
	TW_UINT8	flags;
	TW_UINT8	res2;
	TW_UINT8	param;
};


/* Structure for sending get/set param commands. */
struct tw_cl_command_param {
	TW_UINT8	sgl_off__opcode;/* 3:5 */
	TW_UINT8	size;
	TW_UINT8	request_id;
	TW_UINT8	host_id__unit;	/* 4:4 */
	TW_UINT8	status;
	TW_UINT8	flags;
	TW_UINT16	param_count;
	TW_UINT8	sgl[1];
};


/* Generic command packet. */
struct tw_cl_command_generic {
	TW_UINT8	sgl_off__opcode;/* 3:5 */
	TW_UINT8	size;
	TW_UINT8	request_id;
	TW_UINT8	host_id__unit;	/* 4:4 */
	TW_UINT8	status;
	TW_UINT8	flags;
	TW_UINT16	count;	/* block cnt, parameter cnt, message credits */
};


/* Command packet header. */
struct tw_cl_command_header {
	TW_UINT8	sense_data[TWA_SENSE_DATA_LENGTH];
	struct {
		TW_INT8		reserved[4];
		TW_UINT16	error;
		TW_UINT8	padding;
		TW_UINT8	res__severity;	/* 5:3 */
	} status_block;
	TW_UINT8	err_specific_desc[98];
	struct {
		TW_UINT8	size_header;
		TW_UINT16	reserved;
		TW_UINT8	size_sense;
	} header_desc;
};


/* 7000 Command packet. */
union tw_cl_command_7k {
	struct tw_cl_command_init_connect	init_connect;
	struct tw_cl_command_download_firmware	download_fw;
	struct tw_cl_command_reset_firmware	reset_fw;
	struct tw_cl_command_param		param;
	struct tw_cl_command_generic		generic;
	TW_UINT8	padding[1024 - sizeof(struct tw_cl_command_header)];
};


/* 9000 Command Packet. */
struct tw_cl_command_9k {
	TW_UINT8	res__opcode;	/* 3:5 */
	TW_UINT8	unit;
	TW_UINT16	lun_l4__req_id;	/* 4:12 */
	TW_UINT8	status;
	TW_UINT8	sgl_offset; /* offset (in bytes) to sg_list, from the
					end of sgl_entries */
	TW_UINT16	lun_h4__sgl_entries;
	TW_UINT8	cdb[16];
	TW_UINT8	sg_list[872];/* total struct size =
					1024-sizeof(cmd_hdr) */
};


/* Full command packet. */
struct tw_cl_command_packet {
	struct tw_cl_command_header	cmd_hdr;
	union {
		union tw_cl_command_7k	cmd_pkt_7k;
		struct tw_cl_command_9k cmd_pkt_9k;
	} command;
};

#pragma pack()

/*
 * Following cut out of tw_cl_ioctl.h
 *
 */

#pragma pack(1)

/* Structure used to handle GET/RELEASE LOCK ioctls. */
struct tw_cl_lock_packet {
	TW_UINT32	timeout_msec;
	TW_UINT32	time_remaining_msec;
	TW_UINT32	force_flag;
};


/* Structure used to handle GET COMPATIBILITY INFO ioctl. */
struct tw_cl_compatibility_packet {
	TW_UINT8	driver_version[32];/* driver version */
	TW_UINT16	working_srl;	/* driver & firmware negotiated srl */
	TW_UINT16	working_branch;	/* branch # of the firmware that the
					driver is compatible with */
	TW_UINT16	working_build;	/* build # of the firmware that the
					driver is compatible with */
};


/* Driver understandable part of the ioctl packet built by the API. */
struct tw_cl_driver_packet {
	TW_UINT32	control_code;
	TW_UINT32	status;
	TW_UINT32	unique_id;
	TW_UINT32	sequence_id;
	TW_UINT32	os_status;
	TW_UINT32	buffer_length;
};

#pragma pack()

/*
 * Following cut out of tw_osl_ioctl.h
 *
 */

#pragma pack(1)
/*
 * We need the structure below to ensure that the first byte of
 * data_buf is not overwritten by the kernel, after we return
 * from the ioctl call.  Note that cmd_pkt has been reduced
 * to an array of 1024 bytes even though it's actually 2048 bytes
 * in size.  This is because, we don't expect requests from user
 * land requiring 2048 (273 sg elements) byte cmd pkts.
 */
typedef struct tw_osli_ioctl_no_data_buf {
	struct tw_cl_driver_packet	driver_pkt;
	TW_VOID				*pdata; /* points to data_buf */
	TW_INT8				padding[488 - sizeof(TW_VOID *)];
	struct tw_cl_command_packet	cmd_pkt;
} TW_OSLI_IOCTL_NO_DATA_BUF;

#pragma pack()

#define TW_OSL_IOCTL_FIRMWARE_PASS_THROUGH		\
	_IOWR('T', 202, TW_OSLI_IOCTL_NO_DATA_BUF)

#pragma pack(1)

typedef struct tw_osli_ioctl_with_payload {
	struct tw_cl_driver_packet	driver_pkt;
	TW_INT8				padding[488];
	struct tw_cl_command_packet	cmd_pkt;
	union {
		struct tw_cl_event_packet		event_pkt;
		struct tw_cl_lock_packet		lock_pkt;
		struct tw_cl_compatibility_packet	compat_pkt;
		TW_INT8					data_buf[1];
	} payload;
} TW_OSLI_IOCTL_WITH_PAYLOAD;

#pragma pack()

#endif

#define HPT_CTL_CODE(x) (x+0xFF00)
#define HPT_IOCTL_GET_CHANNEL_INFO          HPT_CTL_CODE(3)
#define HPT_IOCTL_GET_CHANNEL_INFO_V2       HPT_CTL_CODE(53)
#define HPT_IOCTL_IDE_PASS_THROUGH          HPT_CTL_CODE(24)

#define HPT_READ 1
#define HPT_WRITE 2

#define HPT_IOCTL_MAGIC   0xA1B2C3D4

#define MAXDEV_PER_CHANNEL 2
#define PMPORT_PER_CHANNEL 15 /* max devices connected to this channel via pmport */

#pragma pack(1)
typedef struct _HPT_CHANNEL_INFO {
  unsigned int reserve1;
  unsigned int reserve2;
  unsigned int devices[MAXDEV_PER_CHANNEL];
} HPT_CHANNEL_INFO, *PHPT_CHANNEL_INFO;

typedef struct _HPT_CHANNEL_INFO_V2 {
  unsigned int reserve1;
  unsigned int reserve2;
  unsigned int devices[PMPORT_PER_CHANNEL];
} HPT_CHANNEL_INFO_V2, *PHPT_CHANNEL_INFO_V2;

typedef struct _HPT_IOCTL_PARAM {
  unsigned int magic;     /* used to check if it's a valid ioctl packet */
  unsigned int ctrl_code; /* operation control code */
  void* in;               /* input data buffer */
  unsigned int in_size;   /* size of input data buffer */
  void* out;              /* output data buffer */
  unsigned int out_size;  /* size of output data buffer */
  void* returned_size;    /* count of chars returned */
} HPT_IOCTL_PARAM, *PHPT_IOCTL_PARAM;
#define HPT_DO_IOCONTROL	_IOW('H', 0, HPT_IOCTL_PARAM)

typedef struct _HPT_PASS_THROUGH_HEADER {
  unsigned int id;          /* disk ID */
  unsigned char feature;
  unsigned char sectorcount;
  unsigned char lbalow;
  unsigned char lbamid;
  unsigned char lbahigh;
  unsigned char driverhead;
  unsigned char command;
  unsigned char sectors;    /* data size in sectors, if the command has data transfer */
  unsigned char protocol;   /* HPT_(READ,WRITE) or zero for non-DATA */
  unsigned char reserve[3];
}
HPT_PASS_THROUGH_HEADER, *PHPT_PASS_THROUGH_HEADER;
#pragma pack()

#ifndef __unused
#define __unused __attribute__ ((__unused__))
#endif

// MFI definition from the kernel sources, see sys/dev/mfi

#define MFI_STAT_OK          0x00
#define MFI_DCMD_PD_GET_LIST 0x02010000

#define MFI_CTRLR_PREFIX	"/dev/mfi"
#define MRSAS_CTRLR_PREFIX	"/dev/mrsas"

/*
 * MFI Frame flags
 */
#define MFI_FRAME_POST_IN_REPLY_QUEUE           0x0000
#define MFI_FRAME_DONT_POST_IN_REPLY_QUEUE      0x0001
#define MFI_FRAME_SGL32                         0x0000
#define MFI_FRAME_SGL64                         0x0002
#define MFI_FRAME_SENSE32                       0x0000
#define MFI_FRAME_SENSE64                       0x0004
#define MFI_FRAME_DIR_NONE                      0x0000
#define MFI_FRAME_DIR_WRITE                     0x0008
#define MFI_FRAME_DIR_READ                      0x0010
#define MFI_FRAME_DIR_BOTH                      0x0018
#define MFI_FRAME_IEEE_SGL                      0x0020
#define MFI_FRAME_FMT "\20" \
    "\1NOPOST" \
    "\2SGL64" \
    "\3SENSE64" \
    "\4WRITE" \
    "\5READ" \
    "\6IEEESGL"

/* MFI Commands */
typedef enum {
    MFI_CMD_INIT =              0x00,
    MFI_CMD_LD_READ,
    MFI_CMD_LD_WRITE,
    MFI_CMD_LD_SCSI_IO,
    MFI_CMD_PD_SCSI_IO,
    MFI_CMD_DCMD,
    MFI_CMD_ABORT,
    MFI_CMD_SMP,
    MFI_CMD_STP
} mfi_cmd_t;

/* Scatter Gather elements */
struct mfi_sg32 {
    uint32_t    addr;
    uint32_t    len;
} __packed;

struct mfi_sg64 {
    uint64_t    addr;
    uint32_t    len;
} __packed;

struct mfi_sg_skinny {
    uint64_t    addr;
    uint32_t    len;
    uint32_t    flag;
} __packed;

union mfi_sgl {
    struct mfi_sg32             sg32[1];
    struct mfi_sg64             sg64[1];
    struct mfi_sg_skinny        sg_skinny[1];
} __packed;

/* Message frames.  All messages have a common header */
struct mfi_frame_header {
    uint8_t             cmd;
    uint8_t             sense_len;
    uint8_t             cmd_status;
    uint8_t             scsi_status;
    uint8_t             target_id;
    uint8_t             lun_id;
    uint8_t             cdb_len;
    uint8_t             sg_count;
    uint32_t    context;
    /*
     * pad0 is MSI Specific. Not used by Driver. Zero the value before
     * sending the command to f/w.
     */
    uint32_t    pad0;
    uint16_t    flags;
#define MFI_FRAME_DATAOUT       0x08
#define MFI_FRAME_DATAIN        0x10
    uint16_t    timeout;
    uint32_t    data_len;
} __packed;

#define MFI_PASS_FRAME_SIZE 48
struct mfi_pass_frame {
    struct mfi_frame_header header;
    uint32_t    sense_addr_lo;
    uint32_t    sense_addr_hi;
    uint8_t             cdb[16];
    union mfi_sgl       sgl;
} __packed;

#define MFI_DCMD_FRAME_SIZE     40
#define MFI_MBOX_SIZE           12

struct mfi_dcmd_frame {
    struct mfi_frame_header header;
    uint32_t	opcode;
    uint8_t		mbox[MFI_MBOX_SIZE];
    union mfi_sgl	sgl;
} __packed;

#define MAX_IOCTL_SGE   16
struct mfi_ioc_packet {
    uint16_t    mfi_adapter_no;
    uint16_t    mfi_pad1;
    uint32_t    mfi_sgl_off;
    uint32_t    mfi_sge_count;
    uint32_t    mfi_sense_off;
    uint32_t    mfi_sense_len;
    union {
        uint8_t raw[128];
        struct mfi_frame_header hdr;
    } mfi_frame;

    struct iovec mfi_sgl[MAX_IOCTL_SGE];
} __packed;

#ifdef COMPAT_FREEBSD32
struct mfi_ioc_packet32 {
    uint16_t    mfi_adapter_no;
    uint16_t    mfi_pad1;
    uint32_t    mfi_sgl_off;
    uint32_t    mfi_sge_count;
    uint32_t    mfi_sense_off;
    uint32_t    mfi_sense_len;
    union {
        uint8_t raw[128];
        struct mfi_frame_header hdr;
    } mfi_frame;

    struct iovec32 mfi_sgl[MAX_IOCTL_SGE];
} __packed;
#endif

struct mfi_pd_address {
    uint16_t		device_id;
    uint16_t		encl_device_id;
    uint8_t			encl_index;
    uint8_t			slot_number;
    uint8_t			scsi_dev_type;	/* 0 = disk */
    uint8_t			connect_port_bitmap;
    uint64_t		sas_addr[2];
} __packed;

#define MAX_SYS_PDS 240
struct mfi_pd_list {
    uint32_t		size;
    uint32_t		count;
    struct mfi_pd_address	addr[MAX_SYS_PDS];
} __packed;

#define MFI_CMD		_IOWR('M', 1, struct mfi_ioc_packet)

#endif /* OS_FREEBSD_H_ */
