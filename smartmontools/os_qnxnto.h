/*
 * os_generic.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) Joerg Hering       <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2003-8 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */
#ifndef OS_QNXNTO_H_
#define OS_QNXNTO_H_
#define OS_QNXNTO_H_CVSID "$Id$\n"

// Additional material should start here.  Note: to keep the '-V' CVS
// reporting option working as intended, you should only #include
// system include files <something.h>.  Local #include files
// <"something.h"> should be #included in os_generic.c
#include <sys/cpt.h>

#ifndef __TYPES_H_INCLUDED
#include <sys/types.h>
#endif

#include <stdio.h>
#include <fcntl.h>
#include <gulliver.h>
#include <sys/cpt.h>
#include <sys/dcmd_cam.h>
#include <sys/cam_device.h>
#include "atacmds.h"

//----------------------------------------------------------------------------------------------------------
typedef struct _ata_pass_thru   ATA_PASS_THRU;
typedef struct _eide_identify   EIDE_IDENTIFY;
typedef struct _ata_sense       ATA_SENSE;

typedef void CCB;
struct _sim_hba;
struct _resmgr_context;


typedef struct _drive_attribute
 {
  int  id;
  int  threshold;
  char *name;
 }DRIVE_ATTRIBUTE;

//----------------------------------------------------------------------------------------------------------
/* UNIVOS OSD defines and data structures. */

#define INQLEN  36              /* Inquiry string length to store. */

#define CAM_SUCCESS     0       /* For signaling general success */
#define CAM_FAILURE     1       /* For signaling general failure */

#define CAM_FALSE       0       /* General purpose flag value */
#define CAM_TRUE        1       /* General purpose flag value */

//----------------------------------------------------------------------------------------------------------
// Group 3 and 4, command codes 60H-9FH are reserved
#define SC_ATA_PT16     0x85	// ATA Pass-through
//----------------------------------------------------------------------------------------------------------
#define ATA_SMART_LBA_MID_SIG	0x4f
#define ATA_SMART_LBA_HI_SIG	0xc2
#define ATA_SMART_SIG		0xc24f
//----------------------------------------------------------------------------------------------------------
	struct _ata_pass_thru {
		uchar_t		opcode;
#define ATA_PROTO_MSK				0x1e
#define ATA_PROTO_RESPONSE			(15 << 1)
#define ATA_PROTO_FPDMA				(12 << 1)
#define ATA_PROTO_UDMA_DATA_OUT		(11 << 1)
#define ATA_PROTO_UDMA_DATA_IN		(10 << 1)
#define ATA_PROTO_DEVICE_RESET		(9 << 1)
#define ATA_PROTO_DEVICE_DIAGNOSTIC	(8 << 1)
#define ATA_PROTO_DMA_QUEUED		(7 << 1)
#define ATA_PROTO_DMA				(6 << 1)
#define ATA_PROTO_PIO_DATA_OUT		(5 << 1)
#define ATA_PROTO_PIO_DATA_IN		(4 << 1)
#define ATA_PROTO_DATA_NONE			(3 << 1)
#define ATA_PROTO_SRST				(1 << 1)
#define ATA_PROTO_HRST				(0 << 1)
#define ATA_PROTO_EXTEND			0x01
		uchar_t		protocol;      // multiple count, protocol
#define ATA_MCOUNT_MSK				0xe0

#define ATA_FLG_CK_COND				0x20
#define ATA_FLG_T_DIR				0x08		// data from device
#define ATA_FLG_BYT_BLOK			0x04
#define ATA_FLG_TLEN_STPSIU			0x03
#define ATA_FLG_TLEN_SECTOR_COUNT	0x02
#define ATA_FLG_TLEN_FEATURE		0x01
		uchar_t		flags;

		uchar_t		efeatures;
		uchar_t		features;
		uchar_t		esector_count;
		uchar_t		sector_count;
		uchar_t		elba_low;
		uchar_t		lba_low;
		uchar_t		elba_mid;
		uchar_t		lba_mid;
		uchar_t		elba_high;
		uchar_t		lba_high;
		uchar_t		device;
		uchar_t		command;
		uchar_t		control;
	} ata_pass_thru_;
//----------------------------------------------------------------------------------------------------------
#define SENSE_DATA_FMT_DESCRIPTOR	0x02

// Fixed Format Sense Data Structure
// Note: The field "error" has the following format:
//        bit    7	- Address valid bit
//		  bits 6-4  - Error class
//		  bits 3-0  - Error code 
//
// Error classes 0-6 are vendor unique and also indicate that the
// sense data is in _nonextended_ format. (i.e. not usually used)
//		struct _scsi_nonextended_sense {
//			uchar_t	sd_err;
//			ulong_t	sd_block_address;
//		};
//
//	An error class of 7 and an error code of 0 (70H) indicate SCSI-1
//	extended sense data format (or SCSI-2 sense data format).
//
//	An error class of 7 and an error code of 1 (71H) indicate SCSI-2
//	deferred errors.
//
//	Error codes 74H to 7EH are reserved and error code 7FH indicates
//	a vendor-specific sense data format.
typedef struct _scsi_sense {
	uchar_t		error;				// Error Code
	uchar_t		segment;			// Segment number
	uchar_t		sense;				// Sense key/flags
	uchar_t		info[4];			// Information (32bit big-endian value)
	uchar_t		asl;				// Additional Sense Length
	uchar_t		csinfo[4];			// Command-Specific Information
	uchar_t		asc;				// Additional Sense Code
	uchar_t		ascq;				// Additional Sense Code Qualifier
	uchar_t		fruc;				// Field Replaceable Unit Code
	uchar_t		sks;				// Sense Key Specific
	ushort_t	sks_data;			// Sense Key Specific Data (16bit big-endian)
	ushort_t	asb;				// Additional Sense uchar_ts (Max 256-18)
} SCSI_SENSE;

// Descriptor Format Sense Data Structure
//	error code of 72 current, 73 deferred
//	extended sense data format (or SCSI-2 sense data format).
typedef struct _scsi_sense_descriptor {
	uchar_t		error;				// Error Code
	uchar_t		sense;				// Sense key/flags
	uchar_t		asc;				// Additional Sense Code
	uchar_t		ascq;				// Additional Sense Code Qualifier
	uchar_t		rsvd[3];
	uchar_t		asl;				// Additional Sense Length
} SCSI_SENSE_DESCRIPTOR;

typedef struct _scsi_sense_desriptor_header {
	uchar_t			descriptor_type;
	uchar_t			descriptor_len;
} SCSI_SENSE_DESCRIPTOR_HEADER;

#define SENSE_DTYPE_INFORMATION		0x00
#define SENSE_DTYPE_CSI				0x01	// Command Specific Information
#define SENSE_DTYPE_SKS				0x02	// Sense Key Specific
#define SENSE_DTYPE_FRU				0x03	// Field Replaceable Unit
#define SENSE_DTYPE_STREAM			0x04
#define SENSE_DTYPE_BLOCK			0x05
#define SENSE_DTYPE_OSD_OBJ_IDENT	0x06	// OSD Object Identification
#define SENSE_DTYPE_OSD_INTEGRITY	0x07	// OSD Response Integrity Check Value
#define SENSE_DTYPE_OSD_ATR_IDENT	0x08	// OSD Attribute Identification
#define SENSE_DTYPE_ATA				0x09

typedef struct _ata_status_descriptor {
	uchar_t			descriptor_type;
#define ATA_SD_DLEN					0x0c
	uchar_t			descriptor_len;			/* 0xc */
#define ATA_SD_FLG_EXTEND			0x01
	uchar_t			flags;
	uchar_t			error;
	uchar_t			esector_count;			/* (15:8) */
	uchar_t			sector_count;			/* (7:0) */
	uchar_t			elba_low;				/* (15:8) */
	uchar_t			lba_low;				/* (7:0) */
	uchar_t			elba_mid;				/* (15:8) */
	uchar_t			lba_mid;				/* (7:0) */
	uchar_t			elba_high;				/* (15:8) */
	uchar_t			lba_high;				/* (7:0) */
	uchar_t			device;
	uchar_t			status;
} ATA_STATUS_DESCRIPTOR;

//----------------------------------------------------------------------------------------------------------
// Sense Keys
#define SK_MSK			0x0F	// mask to sd_sense field for key

#define SK_NO_SENSE		0		// No sense data (no error)
		#define ASCQ_FILEMARK_DETECTED			0x01
		#define ASCQ_EOPM_DETECTED				0x02	// End of Partition/Medium Detected
		#define ASCQ_SETMARK_DETECTED			0x03
		#define ASCQ_BOPM_DETECTED				0x04	// Beginning of Partition/Medium Detected

#define SK_RECOVERED	1		// Recovered error
		#define ASC_ATA_PASS_THRU					0x00
			#define ASCQ_ATA_PASS_THRU_INFO_AVAIL	0x1d

#define SK_NOT_RDY		2		// Device not ready
	#define ASC_NO_SEEK_COMPLETE				0x02
	#define ASC_NOT_READY						0x04
		#define ASCQ_CAUSE_NOT_REPORTABLE			0x00
		#define ASCQ_BECOMING_READY					0x01
		#define ASCQ_INIT_COMMAND_REQUIRED			0x02
		#define ASCQ_MANUAL_INTERVENTION_REQUIRED	0x03
		#define ASCQ_FORMAT_IN_PROGRESS				0x04
		#define ASCQ_UNKNOWN_CHANGED				0xff	// NTO extension for fdc's
	#define ASC_MEDIA_FORMAT					0x30		// bad format
	#define ASC_MEDIA_NOT_PRESENT				0x3a
	#define ASC_NOT_CONFIGURED					0x3e

#define SK_MEDIUM		3		// Medium error
	#define ASC_UNRECOVERABLE_READ_ERROR	0x11
	#define ASC_RECORD_NOT_FOUND			0x14
		#define ASCQ_RECORD_NOT_FOUND		0x01
	#define ASC_UNABLE_TO_RECOVER_TOC		0x57
	#define ASC_INCOMPATIBLE_MEDIUM			0x64

#define SK_HARDWARE		4		// Hardware error
	#define ASC_INTERNAL_TARGET_FAILURE		0x44
	#define ASC_MEDIA_LOAD_EJECT_FAILURE	0x53
		#define ASCQ_UNRECOVERABLE_CIRC				0x06

#define SK_ILLEGAL		5		// Illegal Request (bad command)
	#define ASC_INVALID_COMMAND			0x20
	#define ASC_INVALID_FIELD			0x24
	#define ASC_INVALID_FIELD_PARAMETER	0x26
	#define ASC_COMMAND_SEQUENCE_ERROR	0x2c
		#define ASCQ_READ_SCRAMBLED		0x03
	#define ASC_ILLEGAL_MODE			0x64
	#define ASC_COPY_PROTECTION			0x6f

#define SK_UNIT_ATN		6		// Unit Attention
	#define ASC_MEDIUM_CHANGED					0x28
	#define ASC_BUS_RESET						0x29
	#define ASC_INSUFFICIENT_TIME_FOR_OPERATION	0x2e
	#define ASC_OPERATOR_REQUEST				0x5a
		#define ASCQ_OPERATOR_MEDIUM_REMOVAL	0x01

#define SK_DATA_PROT	7		// Data Protect
	#define ASC_WRITE_PROTECTED			0x27

#define SK_BLNK_CHK		8		// Blank Check
#define SK_VENDOR		9		// Vendor Specific
#define SK_CPY_ABORT	10		// Copy Aborted
#define SK_CMD_ABORT	11		// Aborted Command
#define SK_EQUAL		12		// Equal
#define SK_VOL_OFL		13		// Volume Overflow
#define SK_MISCMP		14		// Miscompare
#define SK_RESERVED		15		// Reserved
//----------------------------------------------------------------------------------------------------------
// Command Descriptor Block structure definitions

// CDB Flags
#define CF_LINK			0x01	// Linked-command indication
#define CF_FLAG			0x02	// Linked-command with flag bit
#define CF_VENDOR0		0x40	// Vendor unique bits
#define CF_VENDOR1		0x80

#define CF_FUA			0x08
#define CF_DPO			0x10

typedef union _cdb {
	// generic 6 byte command descriptor block
	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		lba_byte1;
		uchar_t		lba_byte0;				// LSB
		uchar_t		transfer_len;
		uchar_t		control;
	} gen6;

	// generic 10 byte command descriptor block
	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		lba_byte3;
		uchar_t		lba_byte4;
		uchar_t		lba_byte1;
		uchar_t		lba_byte0;
		uchar_t		rsvd;
		uchar_t		transfer_len[2];
		uchar_t		control;
	} gen10;

	// generic 12 byte command descriptor block
	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		lba_byte3;
		uchar_t		lba_byte4;
		uchar_t		lba_byte1;
		uchar_t		lba_byte0;
		uchar_t		transfer_len[4];
		uchar_t		rsvd10;
		uchar_t		control;
	} gen12;

	struct _format_unit {
        uchar_t         op_code;
#define FU_RSVD0        0xc0                    // reserved bits
#define FU_FMTDAT       0x10
#define FU_CMPLIST      0x08
        uchar_t         defect_list_fmt;
        uchar_t         track_num;
        ushort_t        interleave;
        uchar_t         rsvd1[7];
	} format_unit;

	struct _format_unit_old {
        uchar_t         op_code;
        uchar_t         rsvd0;
        uchar_t         medium_type_code;
        uchar_t         rsvd1;
        uchar_t         interleave;
        uchar_t         rsvd2;
#define FMT_RSVD3               0x80
#define FMT_SECT_SIZE_CD        0x70
#define FMT_IMMED               0x08
#define FMT_HEAD                0x04
#define FMT_ST                  0x02
#define FMT_CERT                0x01
        uchar_t         cert;
        uchar_t         track_addr;
        uchar_t         rsvd4[4];
	} format_unit_old;

#define RW_OPT_RELADR	0x01
#define RW_OPT_CORRCT	0x02					// Disable Corrections
#define RW_OPT_FUA		0x08					// Force Unit Access
#define RW_OPT_DPO		0x10					// Disable Page Out
	struct {
		uchar_t		opcode;
		uchar_t		lun_lba;
		uchar_t		lba[2];
		uchar_t		transfer_len;
		uchar_t		control;
	} read_write6;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		lba[4];
		uchar_t		rsvd2;
		uchar_t		transfer_len[2];
		uchar_t		control;
	} read_write10;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		lba[4];
		uchar_t		transfer_len[4];
		uchar_t		rsvd2;
		uchar_t		control;
	} read_write12;

#define MSEL_OPT_PF		0x10			// Page Format
#define MSEL_OPT_SP		0x01			// Save Page
	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		rsvd2;
		uchar_t		rsvd3;
		uchar_t		param_length;
		uchar_t		control;
	} mode_select;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		rsvd2;
		uchar_t		rsvd3;
		uchar_t		rsvd4;
		uchar_t		rsvd5;
		uchar_t		rsvd6;
		uchar_t		param_length[2];
		uchar_t		control;
	} mode_select10;

	struct {
		uchar_t		opcode;
#define LS_OPT_SP		0x01			// Save Parameters
#define LS_OPT_PCR		0x02			// Parameter Code Reset
		uchar_t		lun_opt;
#define LS_PC_CUR_THRESHOLD		0x00
#define LS_PC_CUR_CUMULATIVE	0x01
#define LS_PC_DFLT_THRESHOLD	0x02
#define LS_PC_DFLT_CUMULATIVE	0x03
		uchar_t		pc;					// Page Control
		uchar_t		rsvd3;
		uchar_t		rsvd4;
		uchar_t		rsvd5;
		uchar_t		rsvd6;
		uchar_t		param_length[2];
		uchar_t		control;
	} log_select;

	struct {
		uchar_t		opcode;
#define MSNS_OPT_DBD	0x08			// Disable Block Descriptors
		uchar_t		lun_opt;
#define PC_CURRENT		0x00
#define PC_CHANGEABLE	0x40
#define PC_DEFAULT		0x80
#define PC_SAVED		0xC0
#define PC_MSK			0xC0
		uchar_t		pc_page;
		uchar_t		subpage;
		uchar_t		allocation_length;
		uchar_t		control;
	} mode_sense;

	struct _mode_sense10 {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		pc_page;
		uchar_t		subpage;
		uchar_t		rsvd4;
		uchar_t		rsvd5;
		uchar_t		rsvd6;
		uchar_t		allocation_length[2];
		uchar_t		control;
	} mode_sense10;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		pc_page;
		uchar_t		rsvd3;
		uchar_t		rsvd4;
		uchar_t		parameter_pointer[2];
		uchar_t		allocation_length[2];
		uchar_t		control;
	} log_sense;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		rsvd2;
		uchar_t		rsvd3;
		uchar_t		prevent;
		uchar_t		control;
	} removal;

	struct {
		uchar_t		opcode;
#define LD_OPT_IMMED	0x01
		uchar_t		lun_opt;
		uchar_t		rsvd2;
		uchar_t		rsvd3;
#define LD_CMD_START	0x01
#define LD_CMD_LOEJ		0x02
#define LD_CMD_STOP		0x00
#define LD_CMD_EJECT	0x02
#define LD_CMD_LOAD		0x03

// Sequential-Access
#define LD_CMD_SA_HOLD		0x08
#define LD_CMD_SA_EOT		0x04
#define LD_CMD_SA_RT		0x02			// re-tension
#define LD_CMD_SA_LOEJ		0x01

// Block
#define LD_CMD_PC_MSK		0xf0
#define LD_CMD_PC_NC		0
#define LD_CMD_PC_ACTIVE	1
#define LD_CMD_PC_IDLE		2
#define LD_CMD_PC_STANDBY	3
#define LD_CMD_PC_SLEEP		5

		uchar_t		cmd;
		uchar_t		control;
	} load;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
#define SC_OPT_RELADR	0x01
#define SC_OPT_IMMED	0x02
		uchar_t		lba[4];
		uchar_t		num_blocks[2];
		uchar_t		control;
	} synchronize_cache;

// cdrom commands
	struct {
		uchar_t		opcode;
		uchar_t		rsvd1;
		uchar_t		rsvd2;
		uchar_t		rsvd3;
		uchar_t		rsvd4;
		uchar_t		rsvd5;
		uchar_t		rsvd6;
		uchar_t		allocation_length[2];
		uchar_t		control;
	} read_disc_information;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		rsvd2;
		uchar_t		rsvd3;
		uchar_t		rsvd4;
		uchar_t		rsvd5;
		uchar_t		rsvd6;
		uchar_t		rsvd7;
		uchar_t		resume;
		uchar_t		control;
	} pause_resume;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		rsvd2;
		uchar_t		start_minute;
		uchar_t		start_second;
		uchar_t		start_frame;
		uchar_t		end_minute;
		uchar_t		end_second;
		uchar_t		end_frame;
		uchar_t		control;
	} play_audio_msf;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		rsvd2;
		uchar_t		rsvd3;
		uchar_t		start_track;
		uchar_t		start_index;
		uchar_t		rsvd6;
		uchar_t		end_track;
		uchar_t		end_index;
		uchar_t		control;
	} play_audio_ti;

	struct {
		uchar_t		opcode;
#define CD_SCAN_DIR_FORWARD		0x00
#define CD_SCAN_DIR_REVERSE		0x10
		uchar_t		opt;
		uchar_t		start_address[4];
#define CD_SCAN_TYPE_LBA		0x00
#define CD_SCAN_TYPE_MSF		0x40
#define CD_SCAN_TYPE_TRK		0x80
#define CD_SCAN_TYPE_MSK		0xc0
		uchar_t		rsvd6;
		uchar_t		rsvd7;
		uchar_t		rsvd8;
		uchar_t		type;
		uchar_t		rsvd10;
		uchar_t		rsvd11;
	} cd_scan;

	struct {
		uchar_t		opcode;
#define RTOC_OPT_MSF	0x02
		uchar_t		lun_opt;
#define RTOC_FMT_TOC		0x0
#define RTOC_FMT_SESSION	0x1
#define RTOC_FMT_QSUBCODE	0x2
#define RTOC_FMT_QSUBCHNL	0x3
#define RTOC_FMT_ATIP		0x4
#define RTOC_FMT_CDTEXT		0x5
		uchar_t		format;
		uchar_t		rsvd3;
		uchar_t		rsvd4;
		uchar_t		rsvd5;
		uchar_t		start_track;
		uchar_t		allocation_length[2];
#define RTOC_CNTL_FMT_SESSION	0x40
		uchar_t		control_format;
	} read_toc;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		rsvd2[6];
		uchar_t		allocation_length[2];
		uchar_t		rsvd3[2];
	} mechanism_status;

	struct {
		uchar_t		opcode;
#define EXCHANGE_OPT_IMMED	0x01
		uchar_t		lun_opt;
		uchar_t		rsvd2;
		uchar_t		rsvd3;
#define EXCHANGE_CMD_START	0x01
#define EXCHANGE_CMD_LOEJ	0x02
		uchar_t		cmd;
		uchar_t		rsvd5;
		uchar_t		rsvd6;
		uchar_t		rsvd7;
		uchar_t		slot;
		uchar_t		rsvd9;
		uchar_t		rsvd10;
		uchar_t		rsvd11;
	} exchange;

	struct {
		uchar_t		opcode;
		uchar_t		rt;
		uchar_t		feature_number[2];
		uchar_t		rsvd4;
		uchar_t		rsvd5;
		uchar_t		rsvd6;
		uchar_t		allocation_length[2];
		uchar_t		control;
	} get_configuration;

	struct {
		uchar_t		opcode;
#define GE_OPT_POLLED			0x01
		uchar_t		opt;
		uchar_t		rsvd2;
		uchar_t		rsvd3;
#define NCR_OPERATIONAL_CHANGE	0x02
#define NCR_POWER_MANAGEMENT	0x04
#define NCR_EXTERNAL_REQUEST	0x08
#define NCR_MEDIA				0x10
#define NCR_MULTI_INITIATOR		0x20
#define NCR_DEVICE_BUSY			0x40
		uchar_t		ncr;         // notification class request
		uchar_t		rsvd5;
		uchar_t		rsvd6;
		uchar_t		allocation_length[2];
		uchar_t		control;
	} get_event;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		rsvd2;
		uchar_t		rsvd3;
		uchar_t		rsvd4;
		uchar_t		rsvd5;
		uchar_t		rsvd6;
		uchar_t		allocation_length[2];
		uchar_t		control;
	} read_formated_capacities;

	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		read_speed[2];
		uchar_t		write_speed[2];
		uchar_t		rsvd2[6];
	} cd_speed;		

	struct {
		uchar_t		opcode;
#define RSCHNL_OPT_MSF		0x02
		uchar_t		lun_opt;
#define RSCHNL_DATA_SUBQ	0x40
		uchar_t		data;
		uchar_t		data_format;
		uchar_t		rsvd4;
		uchar_t		rsvd5;
		uchar_t		track;
		uchar_t		allocation_length[2];
		uchar_t		control;
	} read_subchannel;

#define CD_FRAME_SYNC_SIZE         12
#define CD_FRAME_HDR_SIZE           4
#define CD_FRAME_SUB_HDR_SIZE       8
#define CD_FRAME_EDC_SIZE           4
#define CD_FRAME_ECC_SIZE         276
#define CD_FRAME_AUX_SIZE           8
#define CD_FRAME_ZERO_SIZE          8
#define CD_FRAME_SPARE_SIZE         4
#define CD_FRAME_C2_ERR_SIZE      294
#define CD_FRAME_BLOCK_ERR_SIZE     2

	struct {
		uchar_t		opcode;
		uchar_t		lun_stype;
// expected sector type
#define RDCD_EST_ANY_SECTOR				(0 << 2)
#define RDCD_EST_CDDA_SECTOR			(1 << 2)
#define RDCD_EST_YELLOW_MODE1_SECTOR	(2 << 2)
#define RDCD_EST_YELLOW_MODE2_SECTOR	(3 << 2)
#define RDCD_EST_XA_SECTOR				(4 << 2)
#define RDCD_EST_XA_FORM2_SECTOR		(5 << 2)
#define RDCD_EST_MSK					(7 << 2)
		uchar_t		lba[4];
		uchar_t		transfer_len[3];
		uchar_t		flags;
#define RDCD_FLG_SYNC			0x80
#define RDCD_FLG_UDATA			0x10
#define RDCD_FLG_ECC			0x08
#define RDCD_FLG_CD_ERR			0x02
#define RDCD_FLG_CD_BLOCK_ERR	0x04
#define RDCD_FLG_HC_NONE		( 0x00 << 5 )
#define RDCD_FLG_HC_HDR			( 0x01 << 5 )
#define RDCD_FLG_HC_SUBHEADER	( 0x02 << 5 )
#define RDCD_FLG_HC_ALL_HEADERS	( 0x03 << 5 )
		uchar_t		subch_selection;
		uchar_t		rsvd3;
	} read_cd;

	struct {
		uchar_t		opcode;
		uchar_t		lun_stype;
		uchar_t		rsvd2;
		uchar_t		start_minute;
		uchar_t		start_second;
		uchar_t		start_frame;
		uchar_t		end_minute;
		uchar_t		end_second;
		uchar_t		end_frame;
		uchar_t		flags;
		uchar_t		subch_selection;
		uchar_t		rsvd11;
	} read_cd_msf;

	struct _ata_pass_thru {
		uchar_t		opcode;
#define ATA_PROTO_MSK				0x1e
#define ATA_PROTO_RESPONSE			(15 << 1)
#define ATA_PROTO_FPDMA				(12 << 1)
#define ATA_PROTO_UDMA_DATA_OUT		(11 << 1)
#define ATA_PROTO_UDMA_DATA_IN		(10 << 1)
#define ATA_PROTO_DEVICE_RESET		(9 << 1)
#define ATA_PROTO_DEVICE_DIAGNOSTIC	(8 << 1)
#define ATA_PROTO_DMA_QUEUED		(7 << 1)
#define ATA_PROTO_DMA				(6 << 1)
#define ATA_PROTO_PIO_DATA_OUT		(5 << 1)
#define ATA_PROTO_PIO_DATA_IN		(4 << 1)
#define ATA_PROTO_DATA_NONE			(3 << 1)
#define ATA_PROTO_SRST				(1 << 1)
#define ATA_PROTO_HRST				(0 << 1)
#define ATA_PROTO_EXTEND			0x01
		uchar_t		protocol;      // multiple count, protocol
#define ATA_MCOUNT_MSK				0xe0

#define ATA_FLG_CK_COND				0x20
#define ATA_FLG_T_DIR				0x08		// data from device
#define ATA_FLG_BYT_BLOK			0x04
#define ATA_FLG_TLEN_STPSIU			0x03
#define ATA_FLG_TLEN_SECTOR_COUNT	0x02
#define ATA_FLG_TLEN_FEATURE		0x01
		uchar_t		flags;

		uchar_t		efeatures;
		uchar_t		features;
		uchar_t		esector_count;
		uchar_t		sector_count;
		uchar_t		elba_low;
		uchar_t		lba_low;
		uchar_t		elba_mid;
		uchar_t		lba_mid;
		uchar_t		elba_high;
		uchar_t		lba_high;
		uchar_t		device;
		uchar_t		command;
		uchar_t		control;
	} ata_pass_thru;

// sequential access commands
	struct {
		uchar_t		opcode;
#define ERASE_OPT_LONG	0x01
		uchar_t		opt;
		uchar_t		rsvd[3];
		uchar_t		control;
	} erase;

	struct {
		uchar_t		opcode;
#define LOCATE_OPT_CP	0x2
#define LOCATE_OPT_BT	0x4
		uchar_t		opt;
		uchar_t		rsvd2;
		uchar_t		ba[4];			// block address
		uchar_t		rsvd7;
		uchar_t		partition;
		uchar_t		control;
	} locate;

	struct {
		uchar_t		opcode;
		uchar_t		opt;
		uchar_t		rsvd2[3];
		uchar_t		control;
	} read_block_limits;

#define RP_OPT_BT	0x01			// block address type
#define RP_OPT_LNG	0x02			// long format
#define RP_OPT_TCLP	0x04			// total current logical position
	struct {
		uchar_t		opcode;
		uchar_t		lun_opt;
		uchar_t		rsvd2[7];
		uchar_t		control;
	} read_position;

#define SRW_OPT_FIXED	0x01
#define SRW_OPT_SILI	0x02
	struct {
		uchar_t		opcode;
		uchar_t		opt;
		uchar_t		transfer_len[3];
		uchar_t		control;
	} sa_read_write;

	struct {
		uchar_t		opcode;
		uchar_t		opt;
		uchar_t		rsvd[3];
		uchar_t		control;
	} rewind;

	struct {
		uchar_t		opcode;
#define SPACE_CODE_BLOCKS		0x00
#define SPACE_CODE_FMRKS		0x01
#define SPACE_CODE_SEQ_FMRKS	0x02
#define SPACE_CODE_EOD			0x03
#define SPACE_CODE_SMRKS		0x04
#define SPACE_CODE_SEQ_SMRKS	0x05
		uchar_t		lun_code;
		uchar_t		count[3];
		uchar_t		control;
	} space;

	struct {
		uchar_t		opcode;
#define WF_OPT_IMMED	0x01
#define WF_OPT_WSMK		0x02
		uchar_t		opt;
		uchar_t		transfer_length[3];
		uchar_t		control;
	} write_filemarks;

	struct {
		uchar_t		opcode;
#define RD_OPT_MEDIA	0x01
		uchar_t		opt;
		uchar_t		rsvd[5];
		uchar_t		allocation_length[2];
		uchar_t		control;
	} report_density;

	struct {
		uchar_t		opcode;
#define FM_OPT_IMMED	0x01
#define FM_OPT_VERIFY	0x02
		uchar_t		opt;
#define FM_FMT_DFLT				0x00
#define FM_FMT_PARTITION		0x01
#define FM_FMT_FORMAT_PARTITION	0x02
		uchar_t		format;
		uchar_t		transfer_length[2];
		uchar_t		control;
	} format_media;
} CDB;
//----------------------------------------------------------------------------------------------------------

struct _ata_sense
 {
  SCSI_SENSE_DESCRIPTOR	       sense;
  ATA_STATUS_DESCRIPTOR	       desc;
 };
//----------------------------------------------------------------------------------------------------------


#endif /* OS_QNXNTO_H_ */
