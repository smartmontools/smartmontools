/*
 * os_freebsd.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003-4 Eduard Martinescu <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#ifndef OS_FREEBSD_H_
#define OS_FREEBSD_H_

#define OS_XXXX_H_CVSID "$Id: os_freebsd.h,v 1.14 2004/09/04 22:11:55 arvoreen Exp $\n"

struct freebsd_dev_channel {
  int   channel;                // the ATA channel to work with
  int   device;                 // the device on the channel
  int   atacommand;             // the ATA Command file descriptor (/dev/ata)
  char* devname;                // the SCSI device name
  int   unitnum;                // the SCSI unit number
  int   scsicontrol;            // the SCSI control interface
};

#define FREEBSD_MAXDEV 64
#define FREEBSD_FDOFFSET 16;
#define MAX_NUM_DEV 26

#ifdef  HAVE_SYS_TWEREG_H
#include <sys/twereg.h>
#else
/**
 *  The following cut out of twereg.h
 *
 */
#if __FreeBSD_version < 500000
#define __packed
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
    u_int8_t	unit:4;
    u_int8_t	host_id:4;
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

/* 
   The following definitions/macros/prototypes are used for three
   different interfaces, referred to as "the three cases" below.
   CONTROLLER_3WARE_678K      -- 6000, 7000, and 8000 controllers via /dev/sd?
   CONTROLLER_3WARE_678K_CHAR -- 6000, 7000, and 8000 controllers via /dev/twe?
   CONTROLLER_3WARE_9000_CHAR -- 9000 controllers via /dev/twa?
*/


#endif /* OS_FREEBSD_H_ */
