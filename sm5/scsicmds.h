/*
 * scsicmds.h
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-8 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * Additional SCSI work:
 * Copyright (C) 2003-8 Douglas Gilbert <dougg@torque.net>
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
 * N.B. What was formerly known as "SMART" are now called "informational
 * exceptions" in recent t10.org drafts (i.e. recent SCSI).
 *
 */


#ifndef SCSICMDS_H_
#define SCSICMDS_H_

#define SCSICMDS_H_CVSID "$Id: scsicmds.h,v 1.69 2009/06/24 04:10:10 dpgilbert Exp $\n"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* #define SCSI_DEBUG 1 */ /* Comment out to disable command debugging */

/* Following conditional defines just in case OS already has them defined.
 * If they are defined we hope they are defined correctly (for SCSI). */
#ifndef TEST_UNIT_READY
#define TEST_UNIT_READY 0x0
#endif
#ifndef LOG_SELECT
#define LOG_SELECT 0x4c
#endif
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
#ifndef READ_DEFECT_10
#define READ_DEFECT_10  0x37
#endif

#ifndef SAT_ATA_PASSTHROUGH_12
#define SAT_ATA_PASSTHROUGH_12 0xa1
#endif
#ifndef SAT_ATA_PASSTHROUGH_16
#define SAT_ATA_PASSTHROUGH_16 0x85
#endif

typedef unsigned char UINT8;
typedef signed char INT8;
typedef unsigned int UINT32;
typedef int INT32;

#define DXFER_NONE        0
#define DXFER_FROM_DEVICE 1
#define DXFER_TO_DEVICE   2

struct scsi_cmnd_io
{
    UINT8 * cmnd;       /* [in]: ptr to SCSI command block (cdb) */
    size_t  cmnd_len;   /* [in]: number of bytes in SCSI command */
    int dxfer_dir;      /* [in]: DXFER_NONE, DXFER_FROM_DEVICE, or 
                                 DXFER_TO_DEVICE */
    UINT8 * dxferp;     /* [in]: ptr to outgoing or incoming data buffer */
    size_t dxfer_len;   /* [in]: bytes to be transferred to/from dxferp */
    UINT8 * sensep;     /* [in]: ptr to sense buffer, filled when 
                                 CHECK CONDITION status occurs */
    size_t max_sense_len; /* [in]: max number of bytes to write to sensep */
    unsigned timeout;   /* [in]: seconds, 0-> default timeout (60 seconds?) */
    size_t resp_sense_len;  /* [out]: sense buffer length written */
    UINT8 scsi_status;  /* [out]: 0->ok, 2->CHECK CONDITION, etc ... */
    int resid;          /* [out]: Number of bytes requested to be transferred
                                  less actual number transferred (0 if not
                                   supported) */
};

struct scsi_sense_disect {
    UINT8 error_code;
    UINT8 sense_key;
    UINT8 asc; 
    UINT8 ascq;
};

/* Useful data from Informational Exception Control mode page (0x1c) */
#define SCSI_IECMP_RAW_LEN 64
struct scsi_iec_mode_page {
    UINT8 requestedCurrent;
    UINT8 gotCurrent;
    UINT8 requestedChangeable;
    UINT8 gotChangeable;
    UINT8 modese_len;   /* 0 (don't know), 6 or 10 */
    UINT8 raw_curr[SCSI_IECMP_RAW_LEN];
    UINT8 raw_chg[SCSI_IECMP_RAW_LEN];
};

/* Carrier for Error counter log pages (e.g. read, write, verify ...) */
struct scsiErrorCounter {
    UINT8 gotPC[7];
    UINT8 gotExtraPC;
    uint64_t counter[8];
};

/* Carrier for Non-medium error log page */
struct scsiNonMediumError {
    UINT8 gotPC0;
    UINT8 gotExtraPC;
    uint64_t counterPC0;
    UINT8 gotTFE_H;
    uint64_t counterTFE_H; /* Track following errors [Hitachi] */
    UINT8 gotPE_H;
    uint64_t counterPE_H;  /* Positioning errors [Hitachi] */
};

/* SCSI Peripheral types (of interest) */
#define SCSI_PT_DIRECT_ACCESS           0x0
#define SCSI_PT_SEQUENTIAL_ACCESS       0x1
#define SCSI_PT_CDROM                   0x5
#define SCSI_PT_MEDIUM_CHANGER          0x8
#define SCSI_PT_ENCLOSURE               0xd

/* ANSI SCSI-3 Log Pages retrieved by LOG SENSE. */
#define SUPPORTED_LPAGES                        0x00
#define BUFFER_OVERRUN_LPAGE                    0x01
#define WRITE_ERROR_COUNTER_LPAGE               0x02
#define READ_ERROR_COUNTER_LPAGE                0x03
#define READ_REVERSE_ERROR_COUNTER_LPAGE        0x04
#define VERIFY_ERROR_COUNTER_LPAGE              0x05
#define NON_MEDIUM_ERROR_LPAGE                  0x06
#define LAST_N_ERROR_LPAGE                      0x07
#define FORMAT_STATUS_LPAGE                     0x08
#define TEMPERATURE_LPAGE                       0x0d
#define STARTSTOP_CYCLE_COUNTER_LPAGE           0x0e
#define APPLICATION_CLIENT_LPAGE                0x0f
#define SELFTEST_RESULTS_LPAGE                  0x10
#define BACKGROUND_RESULTS_LPAGE                0x15   /* SBC-3 */
#define PROTOCOL_SPECIFIC_LPAGE                 0x18
#define IE_LPAGE                                0x2f

/* Seagate vendor specific log pages. */
#define SEAGATE_CACHE_LPAGE                     0x37
#define SEAGATE_FACTORY_LPAGE                   0x3e

/* Log page response lengths */
#define LOG_RESP_SELF_TEST_LEN 0x194

/* See the SSC-2 document at www.t10.org . Earler note: From IBM 
Documentation, see http://www.storage.ibm.com/techsup/hddtech/prodspecs.htm */
#define TAPE_ALERTS_LPAGE                        0x2e

/* ANSI SCSI-3 Mode Pages */
#define VENDOR_UNIQUE_PAGE                       0x00
#define READ_WRITE_ERROR_RECOVERY_PAGE           0x01
#define DISCONNECT_RECONNECT_PAGE                0x02
#define FORMAT_DEVICE_PAGE                       0x03
#define RIGID_DISK_DRIVE_GEOMETRY_PAGE           0x04
#define FLEXIBLE_DISK_PAGE                       0x05
#define VERIFY_ERROR_RECOVERY_PAGE               0x07
#define CACHING_PAGE                             0x08
#define PERIPHERAL_DEVICE_PAGE                   0x09
#define XOR_CONTROL_MODE_PAGE                    0x10
#define CONTROL_MODE_PAGE                        0x0a
#define MEDIUM_TYPES_SUPPORTED_PAGE              0x0b
#define NOTCH_PAGE                               0x0c
#define CD_DEVICE_PAGE                           0x0d
#define CD_AUDIO_CONTROL_PAGE                    0x0e
#define DATA_COMPRESSION_PAGE                    0x0f
#define ENCLOSURE_SERVICES_MANAGEMENT_PAGE       0x14
#define PROTOCOL_SPECIFIC_LUN_PAGE               0x18
#define PROTOCOL_SPECIFIC_PORT_PAGE              0x19
#define POWER_CONDITION_PAGE                     0x1a
#define INFORMATIONAL_EXCEPTIONS_CONTROL_PAGE    0x1c
#define FAULT_FAILURE_REPORTING_PAGE             0x1c

/* Background control mode subpage is [0x1c,0x1] */
#define BACKGROUND_CONTROL_M_SUBPAGE             0x1   /* SBC-2 */

#define ALL_MODE_PAGES                           0x3f

/* Mode page control field */
#define MPAGE_CONTROL_CURRENT               0
#define MPAGE_CONTROL_CHANGEABLE            1
#define MPAGE_CONTROL_DEFAULT               2
#define MPAGE_CONTROL_SAVED                 3

/* defines for useful SCSI Status codes */
#define SCSI_STATUS_CHECK_CONDITION     0x2

/* defines for useful Sense Key codes */
#define SCSI_SK_NO_SENSE                0x0
#define SCSI_SK_RECOVERED_ERR           0x1
#define SCSI_SK_NOT_READY               0x2
#define SCSI_SK_MEDIUM_ERROR            0x3
#define SCSI_SK_HARDWARE_ERROR          0x4
#define SCSI_SK_ILLEGAL_REQUEST         0x5
#define SCSI_SK_UNIT_ATTENTION          0x6
#define SCSI_SK_ABORTED_COMMAND         0xb

/* defines for useful Additional Sense Codes (ASCs) */
#define SCSI_ASC_NOT_READY              0x4     /* more info in ASCQ code */
#define SCSI_ASC_NO_MEDIUM              0x3a    /* more info in ASCQ code */
#define SCSI_ASC_UNKNOWN_OPCODE         0x20
#define SCSI_ASC_UNKNOWN_FIELD          0x24
#define SCSI_ASC_UNKNOWN_PARAM          0x26
#define SCSI_ASC_WARNING                0xb
#define SCSI_ASC_IMPENDING_FAILURE      0x5d

#define SCSI_ASCQ_ATA_PASS_THROUGH      0x1d

/* Simplified error code (negative values as per errno) */
#define SIMPLE_NO_ERROR                 0
#define SIMPLE_ERR_NOT_READY            1
#define SIMPLE_ERR_BAD_OPCODE           2
#define SIMPLE_ERR_BAD_FIELD            3       /* in cbd */
#define SIMPLE_ERR_BAD_PARAM            4       /* in data */
#define SIMPLE_ERR_BAD_RESP             5       /* response fails sanity */
#define SIMPLE_ERR_NO_MEDIUM            6       /* no medium present */
#define SIMPLE_ERR_BECOMING_READY       7       /* device will be ready soon */
#define SIMPLE_ERR_TRY_AGAIN            8       /* some warning, try again */
#define SIMPLE_ERR_MEDIUM_HARDWARE      9       /* medium or hardware error */
#define SIMPLE_ERR_UNKNOWN              10      /* unknown sense value */
#define SIMPLE_ERR_ABORTED_COMMAND      11      /* most likely transport error */


/* defines for functioncode parameter in SENDDIAGNOSTIC function */
#define SCSI_DIAG_NO_SELF_TEST          0x00
#define SCSI_DIAG_DEF_SELF_TEST         0xff
#define SCSI_DIAG_BG_SHORT_SELF_TEST    0x01
#define SCSI_DIAG_BG_EXTENDED_SELF_TEST 0x02
#define SCSI_DIAG_FG_SHORT_SELF_TEST    0x05
#define SCSI_DIAG_FG_EXTENDED_SELF_TEST 0x06
#define SCSI_DIAG_ABORT_SELF_TEST       0x04


/* SCSI command timeout values (units are seconds) */
#define SCSI_TIMEOUT_DEFAULT    6               /* 6 seconds should be ample */
#define SCSI_TIMEOUT_SELF_TEST  (5 * 60 * 60)   /* allow max 5 hours for */
                                            /* extended foreground self test */



#define LOGPAGEHDRSIZE  4

class scsi_device;

void scsi_do_sense_disect(const struct scsi_cmnd_io * in,
                          struct scsi_sense_disect * out);

int scsiSimpleSenseFilter(const struct scsi_sense_disect * sinfo);

const char * scsiErrString(int scsiErr);

/* STANDARD SCSI Commands  */
int scsiTestUnitReady(scsi_device * device);

int scsiStdInquiry(scsi_device * device, UINT8 *pBuf, int bufLen);

int scsiInquiryVpd(scsi_device * device, int vpd_page, UINT8 *pBuf, int bufLen);

int scsiLogSense(scsi_device * device, int pagenum, int subpagenum, UINT8 *pBuf,
                 int bufLen, int known_resp_len);

int scsiLogSelect(scsi_device * device, int pcr, int sp, int pc, int pagenum,
                  int subpagenum, UINT8 *pBuf, int bufLen);

int scsiModeSense(scsi_device * device, int pagenum, int subpagenum, int pc,
                  UINT8 *pBuf, int bufLen);

int scsiModeSelect(scsi_device * device, int sp, UINT8 *pBuf, int bufLen);

int scsiModeSense10(scsi_device * device, int pagenum, int subpagenum, int pc,
                    UINT8 *pBuf, int bufLen);

int scsiModeSelect10(scsi_device * device, int sp, UINT8 *pBuf, int bufLen);

int scsiModePageOffset(const UINT8 * resp, int len, int modese_len);

int scsiRequestSense(scsi_device * device, struct scsi_sense_disect * sense_info);

int scsiSendDiagnostic(scsi_device * device, int functioncode, UINT8 *pBuf, int bufLen);

int scsiReceiveDiagnostic(scsi_device * device, int pcv, int pagenum, UINT8 *pBuf,
                      int bufLen);

int scsiReadDefect10(scsi_device * device, int req_plist, int req_glist, int dl_format,
                     UINT8 *pBuf, int bufLen);

/* SMART specific commands */
int scsiCheckIE(scsi_device * device, int hasIELogPage, int hasTempLogPage, UINT8 *asc,
                UINT8 *ascq, UINT8 *currenttemp, UINT8 *triptemp);

int scsiFetchIECmpage(scsi_device * device, struct scsi_iec_mode_page *iecp,
                      int modese_len);
int scsi_IsExceptionControlEnabled(const struct scsi_iec_mode_page *iecp);
int scsi_IsWarningEnabled(const struct scsi_iec_mode_page *iecp);
int scsiSetExceptionControlAndWarning(scsi_device * device, int enabled,
                            const struct scsi_iec_mode_page *iecp);
void scsiDecodeErrCounterPage(unsigned char * resp,
                              struct scsiErrorCounter *ecp);
void scsiDecodeNonMediumErrPage(unsigned char * resp,
                                struct scsiNonMediumError *nmep);
int scsiFetchExtendedSelfTestTime(scsi_device * device, int * durationSec,
                                  int modese_len);
int scsiCountFailedSelfTests(scsi_device * device, int noisy);
int scsiSelfTestInProgress(scsi_device * device, int * inProgress);
int scsiFetchControlGLTSD(scsi_device * device, int modese_len, int current);
int scsiSetControlGLTSD(scsi_device * device, int enabled, int modese_len);
int scsiFetchTransportProtocol(scsi_device * device, int modese_len);

/* T10 Standard IE Additional Sense Code strings taken from t10.org */

const char* scsiGetIEString(UINT8 asc, UINT8 ascq);
int scsiGetTemp(scsi_device * device, UINT8 *currenttemp, UINT8 *triptemp);


int scsiSmartIBMOfflineTest(scsi_device * device);

int scsiSmartDefaultSelfTest(scsi_device * device);
int scsiSmartShortSelfTest(scsi_device * device);
int scsiSmartExtendSelfTest(scsi_device * device);
int scsiSmartShortCapSelfTest(scsi_device * device);
int scsiSmartExtendCapSelfTest(scsi_device * device);
int scsiSmartSelfTestAbort(scsi_device * device);

const char * scsiTapeAlertsTapeDevice(unsigned short code);
const char * scsiTapeAlertsChangerDevice(unsigned short code);

const char * scsi_get_opcode_name(UINT8 opcode);
void dStrHex(const char* str, int len, int no_ascii);
inline void dStrHex(const unsigned char* str, int len, int no_ascii)
  { dStrHex((const char *)str, len, no_ascii); }


/* SCSI command transmission interface function declaration. Its
 * definition is target OS specific (see os_<OS>.c file).
 * Returns 0 if SCSI command successfully launched and response
 * received. Even when 0 is returned the caller should check
 * scsi_cmnd_io::scsi_status for SCSI defined errors and warnings
 * (e.g. CHECK CONDITION). If the SCSI command could not be issued
 * (e.g. device not present or not a SCSI device) or some other problem
 * arises (e.g. timeout) then returns a negative errno value. */
// Moved to C++ interface
//int do_scsi_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report);



#endif

