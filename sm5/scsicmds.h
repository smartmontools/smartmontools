/*
 * scsicmds.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
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


#ifndef SCSICMDS_H_
#define SCSICMDS_H_

#ifndef CVSID4
#define CVSID4 "$Id: scsicmds.h,v 1.9 2003/01/04 01:37:48 dpgilbert Exp $\n"
#endif

/* #define SCSI_DEBUG 1 */ /* Comment out to disable command debugging */

/* Following conditional defines bypass inclusion of scsi/scsi.h and
 * scsi/scsi_ioctl.h . Issue will be resolved later ... */
#ifndef LOG_SENSE
#define LOG_SENSE 0x4d
#endif

#ifndef MODE_SENSE
#define MODE_SENSE 0x1a
#endif
#ifndef MODE_SENSE_10
#define MODE_SENSE_10 0x5a
#endif

#ifndef MODE_SELECT
#define MODE_SELECT 0x15
#endif

#ifndef MODE_SELECT_10
#define MODE_SELECT_10 0x55
#endif

#ifndef INQUIRY
#define INQUIRY 0x12
#endif

#ifndef REQUEST_SENSE
#define REQUEST_SENSE  0x03
#endif

#ifndef RECEIVE_DIAGNOSTIC
#define RECEIVE_DIAGNOSTIC  0x1c
#endif

#ifndef SEND_DIAGNOSTIC
#define SEND_DIAGNOSTIC  0x1d
#endif

#ifndef SCSI_IOCTL_SEND_COMMAND
#define SCSI_IOCTL_SEND_COMMAND 1
#endif
#ifndef SCSI_IOCTL_TEST_UNIT_READY
#define SCSI_IOCTL_TEST_UNIT_READY 2
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>

typedef unsigned char UINT8;
typedef char INT8;
typedef unsigned int UINT32;
typedef int INT32;




/* Command Descriptor Block for SCSI Commands 
 * There are three standard length 6, 10, and 12 bytes 
 * List below are standard forms of CDBs the are not listed
 * in structure because format can change within specific commands
 * these header include data buffer sizes. This is for used with ioctl
 * 
*/
/* 6-type CDB format */
/*
	cdb[0] is Opcode
	cdb[1] bits 5-7 LUN
	       bits 0-4 Logical Block (MSB)
	cdb[2] is Logical Block
	cdb[3] is Logical Block (LSB)
	cdb[4] is Transfer Length
	cdb[5] is Control Byte 
*/
#define CDB_6_HDR_SIZE			14
#define CDB_6_MAX_DATA_SIZE		0xff

struct cdb6hdr{
	UINT32 inbufsize;
	UINT32 outbufsize;
	UINT8 cdb [6];
} __attribute__ ((packed));



/* 10-type CDB format */
/*
	cdb[0] is Opcode
	cdb[1] bits 5-7 LUN
	       bits 0-4 Reserved
	cdb[2] is Logical Block (MSB)
	cdb[3] is Logical Block
	cdb[4] is Logical Block
	cdb[5] is Logical Block (LSB)
	cdb[6] is Reserved
	cdb[7] is Transfer Length (MSB)
	cdb[8] is Transfer Length (LSB)
	cdb[9] is Control Byte 
*/
#define CDB_10_HDR_SIZE 14
#define CDB_10_MAX_DATA_SIZE		0xffff

struct cdb10hdr {
	UINT32 inbufsize;
	UINT32 outbufsize;
	UINT8 cdb [10];
} __attribute__ ((packed));



/* 12-type CDB format */
/*
	cdb[0] is Opcode
	cdb[1] bits 5-7 LUN
	       bits 0-4 Reserved
	cdb[2] is Logical Block (MSB)
	cdb[3] is Logical Block
	cdb[4] is Logical Block
	cdb[5] is Logical Block (LSB)
	cdb[6] is Transfer Length (MSB)
	cdb[7] is Transfer Length
	cdb[8] is Transfer Length
	cdb[9] is Transfer Length (LSB)
	cdb[10] is Reserved
	cdb[11] is Control Byte 
*/

#define CDB_12_HDR_SIZE 14
#define CDB_12_MAX_DATA_SIZE		0xffffffff

struct cdb12hdr {
	UINT32 inbufsize;
	UINT32 outbufsize;
	UINT8 cdb [12];
} __attribute__ ((packed));



/* Mode Select Data Header */

#define MODE_DATA_HDR_SIZE	12

struct modedatahdr {
	UINT8 modedatalength;
	UINT8 mediumtype;
	UINT8 reserved2;
	UINT8 blockdescriptor;
	UINT8 numberofblocks[3];
	UINT8 densitycode;
	UINT8 blocklength[3];
} ;



/* ANSI SCSI-3 Log Sense Return Log Pages from device. */
#define SUPPORT_LOG_PAGES				0x00
#define BUFFER_OVERRUN_PAGE				0x01
#define WRITE_ERROR_COUNTER_PAGE		0x02
#define READ_ERROR_COUNTER_PAGE			0x03
#define READ_REVERSE_ERROR_COUNTER_PAGE 0x04
#define VERIFY_ERROR_COUNTER_PAGE		0x05
#define NON_MEDIUM_ERROR_PAGE			0x06
#define LAST_N_ERROR_PAGE				0x07
#define FORMAT_STATUS_PAGE				0x08
#define TEMPERATURE_PAGE				0x0d
#define STARTSTOP_CYCLE_COUNTER_PAGE	0x0e
#define APPLICATION_CLIENT_PAGE			0x0f
#define SELFTEST_RESULTS_PAGE			0x10

/* From IBM Documentation */
/* See more information at http://www.storage.ibm.com/techsup/hddtech/prodspecs.htm */
#define TAPE_ALERTS_PAGE				0x2e
#define SMART_PAGE						0x2f

/* ANSI SCSI-3 Mode Pages */
#define VENDOR_UNIQUE_PARAMETERS		     0x00
#define READ_WRITE_ERROR_RECOVERY_PARAMETERS 0x01
#define DISCONNECT_RECONNECT_PARAMETERS		 0x02
#define FORMAT_DEVICE_PARAMETERS             0x03
#define RIGID_DISK_DRIVE_GEOMETRY_PARAMETERS 0x04
#define FLEXIBLE_DISK_PARAMETERS			 0x05
#define VERIFY_ERROR_RECOVERY_PARAMETERS	 0x07
#define CACHING_PARAMETERS					 0x08
#define PERIPHERAL_DEVICE					 0x09
#define XOR_CONTROL_MODE_PARAMETERS			 0x10
#define CONTROL_MODE_PAGE_PARAMETERS		 0x0a
#define MEDIUM_TYPES_SUPPORTED				 0x0b
#define NOTCH_PARAMETERS					 0x0c
#define POWER_CONDITION_PARAMETERS			 0x0d
#define CD_DEVICE_PARAMETERS				 0x0d
#define CD_AUDIO_CONTROL_PAGE				 0x0e
#define DATA_COMPRESSION_PARAMETERS			 0x0f
#define MEDIUM_PARTITION_MODE_PARAMTERES_1	 0x11
#define MEDIUM_PARTITION_MODE_PARAMTERES_2	 0x12
#define MEDIUM_PARTITION_MODE_PARAMTERES_3	 0x13
#define MEDIUM_PARTITION_MODE_PARAMTERES_4	 0x14
#define ENCLOSURE_SERVICES_MANAGEMENT		 0x14
#define LUN_CONTROL							 0x18
#define PORT_CONTROL						 0x19
#define POWER_CONTROL						 0x1a
#define LUN_MAPPING_PAGE					 0x1b
#define INFORMATIONAL_EXCEPTIONS_CONTROL	 0x1c
#define FAULT_FAILURE_REPORTING_PAGE		 0x1c
#define ELEMENT_ADDRESS_ASSIGNMENT			 0x1d
#define TIMEOUT_AND_PROTECT_PARAMETERS		 0x1d
#define TRANSPORT_GEOMETRY_PARAMETERS		 0x1e
#define DEVICE_CAPABILITIES					 0x1f
#define CD_CAPABILITIES_AND_MECHANISM_STATUS 0x2a

#define ALL_PARAMETERS						 0x3f


/* defines for functioncode parameter in SENDDIAGNOSTIC function */

#define SCSI_DIAG_NO_SELF_TEST			0x00
#define SCSI_DIAG_SELF_TEST				0xff
#define SCSI_DIAG_BG_SHORT_SELF_TEST	0x01
#define SCSI_DIAG_BG_EXTENDED_SELF_TEST	0x02
#define SCSI_DIAG_FG_SHORT_SELF_TEST	0x05
#define SCSI_DIAG_FG_EXTENDED_SELF_TEST	0x06
#define SCSI_DIAG_ABORT_SELF_TEST		0x04



#define LOGPAGEHDRSIZE	4

/* STANDARD SCSI Commands  */
UINT8 testunitnotready (int device);

UINT8 stdinquiry ( int device, UINT8 *pBuffer);

UINT8 inquiry ( int device, UINT8 pagenum, UINT8 *pBuf);

UINT8 logsense (int device, UINT8 pagenum, UINT8 *pBuffer);

UINT8 modesense ( int device, UINT8 pagenum, UINT8 *pBuffer);

UINT8 modeselect ( int device, UINT8 pagenum, UINT8 *pBuffer);

UINT8 modesense10 ( int device, UINT8 pagenum, UINT8 *pBuffer);

UINT8 modeselect10 ( int device, UINT8 pagenum, UINT8 *pBuffer);

UINT8 requestsense (int device,   UINT8 *pBuffer);

UINT8 senddiagnostic (int device, UINT8 functioncode, UINT8 *pBuffer);

UINT8 receivediagnostic (int device, UINT8 pagenum,  UINT8 *pBuf);
/* S.M.A.R.T. specific commands */

/*scsSmartSupport return value  can be masked with the following */
/* Parsing response of ModePage 1c */
#define DEXCPT_DISABLE  0xf7
#define DEXCPT_ENABLE	0x08
#define EWASC_ENABLE	0x10
#define EWASC_DISABLE	0xef

#define CHECK_SMART_BY_LGPG_2F  0x01
#define CHECK_SMART_BY_REQSENSE 0x00

#define SMART_SENSE_MAX_ENTRY	0x6c

UINT8 scsiCheckSmart(int device, UINT8 method, UINT8 *retval, UINT8 *currenttemp, UINT8 *triptemp);

UINT8 scsiSmartSupport (int device, UINT8 *retval);


UINT8 scsiSmartEWASCEnable (int device);
UINT8 scsiSmartEWASCDisable (int device);

UINT8 scsiSmartDEXCPTEnable (int device);
UINT8 scsiSmartDEXCPTDisable (int device);


/* T10 Standard SMART Sense Code assignment taken from t10.org */

char* scsiSmartGetSenseCode ( UINT8 ascq);
UINT8 scsiGetTemp ( int device, UINT8 *currenttemp, UINT8 *triptemp);


UINT8 scsiSmartOfflineTest (int device);
UINT8 scsiSmartShortSelfTest (int device);
UINT8 scsiSmartExtendSelfTest (int device);
UINT8 scsiSmartShortCapSelfTest (int device);
UINT8 scsiSmartExtendCapSelfTest (int device);
UINT8 scsiSmartSelfTestAbort (int device);

char* scsiTapeAlertsTapeDevice ( unsigned short code);
#endif

