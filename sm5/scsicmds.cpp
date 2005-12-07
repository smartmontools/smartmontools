/*
 * scsicmds.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-5 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 *
 * Additional SCSI work:
 * Copyright (C) 2003-5 Douglas Gilbert <dougg@torque.net>
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
 *
 * In the SCSI world "SMART" is a dead or withdrawn standard. In recent
 * SCSI standards (since SCSI-3) it goes under the awkward name of
 * "Informational Exceptions" ["IE" or "IEC" (with the "C" for "control")].
 * The relevant information is spread around several SCSI draft
 * standards available at http://www.t10.org . Reference is made in the
 * code to the following acronyms:
 *      - SAM [SCSI Architectural model, versions 2 or 3]
 *      - SPC [SCSI Primary commands, versions 2 or 3]
 *      - SBC [SCSI Block commands, versions 2]
 *
 * Some SCSI disk vendors have snippets of "SMART" information in their
 * product manuals.
 */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "int64.h"
#include "extern.h"
#include "scsicmds.h"
#include "utility.h"

const char *scsicmds_c_cvsid="$Id: scsicmds.cpp,v 1.83 2005/12/07 02:50:20 dpgilbert Exp $"
CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

/* for passing global control variables */
extern smartmonctrl *con;

/* output binary in hex and optionally ascii */
void dStrHex(const char* str, int len, int no_ascii)
{
    const char* p = str;
    unsigned char c;
    char buff[82];
    int a = 0;
    const int bpstart = 5;
    const int cpstart = 60;
    int cpos = cpstart;
    int bpos = bpstart;
    int i, k;
    
    if (len <= 0) return;
    memset(buff,' ',80);
    buff[80]='\0';
    k = sprintf(buff + 1, "%.2x", a);
    buff[k + 1] = ' ';
    if (bpos >= ((bpstart + (9 * 3))))
        bpos++;

    for(i = 0; i < len; i++)
    {
        c = *p++;
        bpos += 3;
        if (bpos == (bpstart + (9 * 3)))
            bpos++;
        sprintf(&buff[bpos], "%.2x", (int)(unsigned char)c);
        buff[bpos + 2] = ' ';
        if (no_ascii)
            buff[cpos++] = ' ';
        else {
            if ((c < ' ') || (c >= 0x7f))
                c='.';
            buff[cpos++] = c;
        }
        if (cpos > (cpstart+15))
        {
            pout("%s\n", buff);
            bpos = bpstart;
            cpos = cpstart;
            a += 16;
            memset(buff,' ',80);
            k = sprintf(buff + 1, "%.2x", a);
            buff[k + 1] = ' ';
        }
    }
    if (cpos > cpstart)
    {
        pout("%s\n", buff);
    }
}

struct scsi_opcode_name {
    UINT8 opcode;
    const char * name;
};

static struct scsi_opcode_name opcode_name_arr[] = {
    /* in ascending opcode order */
    {TEST_UNIT_READY, "test unit ready"},       /* 0x00 */
    {REQUEST_SENSE, "request sense"},           /* 0x03 */
    {INQUIRY, "inquiry"},                       /* 0x12 */
    {MODE_SELECT, "mode select"},               /* 0x15 */
    {MODE_SENSE, "mode sense"},                 /* 0x1a */
    {RECEIVE_DIAGNOSTIC, "receive diagnostic"}, /* 0x1c */
    {SEND_DIAGNOSTIC, "send diagnostic"},       /* 0x1d */
    {READ_DEFECT_10, "read defect list(10)"},   /* 0x37 */
    {LOG_SENSE, "log sense"},                   /* 0x4d */
    {MODE_SELECT_10, "mode select(10)"},        /* 0x55 */
    {MODE_SENSE_10, "mode sense(10)"},          /* 0x5a */
};

const char * scsi_get_opcode_name(UINT8 opcode)
{
    int k;
    int len = sizeof(opcode_name_arr) / sizeof(opcode_name_arr[0]);
    struct scsi_opcode_name * onp;

    for (k = 0; k < len; ++k) {
        onp = &opcode_name_arr[k];
        if (opcode == onp->opcode)
            return onp->name;
        else if (opcode < onp->opcode)
            return NULL;
    }
    return NULL;
}


void scsi_do_sense_disect(const struct scsi_cmnd_io * io_buf,
                          struct scsi_sense_disect * out)
{
    memset(out, 0, sizeof(struct scsi_sense_disect));
    if ((SCSI_STATUS_CHECK_CONDITION == io_buf->scsi_status) && 
        (io_buf->resp_sense_len > 7)) {  
        out->error_code = (io_buf->sensep[0] & 0x7f);
        out->sense_key = (io_buf->sensep[2] & 0xf);
        if (io_buf->resp_sense_len > 13) {
            out->asc = io_buf->sensep[12];
            out->ascq = io_buf->sensep[13];
        }
    }
}

static int scsiSimpleSenseFilter(const struct scsi_sense_disect * sinfo)
{
    switch (sinfo->sense_key) {
    case SCSI_SK_NOT_READY:
        if (SCSI_ASC_NO_MEDIUM == sinfo->asc) 
            return SIMPLE_ERR_NO_MEDIUM;
        else if (SCSI_ASC_NOT_READY == sinfo->asc) {
            if (0x1 == sinfo->ascq)
                return SIMPLE_ERR_BECOMING_READY;
            else
                return SIMPLE_ERR_NOT_READY;
        } else
            return SIMPLE_ERR_NOT_READY;
    case SCSI_SK_MEDIUM_ERROR:
    case SCSI_SK_HARDWARE_ERROR:
        return SIMPLE_ERR_MEDIUM_HARDWARE;
    case SCSI_SK_ILLEGAL_REQUEST:
        if (SCSI_ASC_UNKNOWN_OPCODE == sinfo->asc)
            return SIMPLE_ERR_BAD_OPCODE;
        else if (SCSI_ASC_UNKNOWN_FIELD == sinfo->asc)
            return SIMPLE_ERR_BAD_FIELD;
        else if (SCSI_ASC_UNKNOWN_PARAM == sinfo->asc)
            return SIMPLE_ERR_BAD_PARAM;
        else
            return SIMPLE_ERR_BAD_PARAM;    /* all other illegal request */
    case SCSI_SK_UNIT_ATTENTION:
        return SIMPLE_ERR_TRY_AGAIN;
    default:
        return SIMPLE_NO_ERROR;
    }
}

const char * scsiErrString(int scsiErr)
{
    if (scsiErr < 0)
        return strerror(-scsiErr);
    switch (scsiErr) {
        case SIMPLE_NO_ERROR: 
            return "no error";
        case SIMPLE_ERR_NOT_READY: 
            return "device not ready";
        case SIMPLE_ERR_BAD_OPCODE: 
            return "unsupported scsi opcode";
        case SIMPLE_ERR_BAD_FIELD: 
            return "unsupported field in scsi command";
        case SIMPLE_ERR_BAD_PARAM: 
            return "badly formed scsi parameters";
        case SIMPLE_ERR_BAD_RESP: 
            return "scsi response fails sanity test";
        case SIMPLE_ERR_NO_MEDIUM: 
            return "no medium present";
        case SIMPLE_ERR_BECOMING_READY: 
            return "device will be ready soon";
        case SIMPLE_ERR_TRY_AGAIN: 
            return "unit attention reported, try again";
        case SIMPLE_ERR_MEDIUM_HARDWARE: 
            return "medium or hardware error (serious)";
        default:
            return "unknown error";
    }
}

/* Sends LOG SENSE command. Returns 0 if ok, 1 if device NOT READY, 2 if
   command not supported, 3 if field (within command) not supported or
   returns negated errno.  SPC-3 sections 6.6 and 7.2 (rec 22a).
   N.B. Sets PC==1 to fetch "current cumulative" log pages.
   If known_resp_len > 0 then a single fetch is done for this response
   length. If known_resp_len == 0 then twin fetches are performed, the
   first to deduce the response length, then send the same command again
   requesting the deduced response length. This protects certain fragile 
   HBAs. The twin fetch technique should not be used with the TapeAlert
   log page since it clears its state flags after each fetch. */
int scsiLogSense(int device, int pagenum, UINT8 *pBuf, int bufLen,
                 int known_resp_len)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    UINT8 cdb[10];
    UINT8 sense[32];
    int pageLen;
    int status, res;

    if (known_resp_len > bufLen)
        return -EIO;
    if (known_resp_len > 0)
        pageLen = known_resp_len;
    else {
        /* Starting twin fetch strategy: first fetch to find respone length */
        pageLen = 4;
        if (pageLen > bufLen)
            return -EIO;
        else
            memset(pBuf, 0, pageLen);

        memset(&io_hdr, 0, sizeof(io_hdr));
        memset(cdb, 0, sizeof(cdb));
        io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
        io_hdr.dxfer_len = pageLen;
        io_hdr.dxferp = pBuf;
        cdb[0] = LOG_SENSE;
        cdb[2] = 0x40 | (pagenum & 0x3f);  /* Page control (PC)==1 */
        cdb[7] = (pageLen >> 8) & 0xff;
        cdb[8] = pageLen & 0xff;
        io_hdr.cmnd = cdb;
        io_hdr.cmnd_len = sizeof(cdb);
        io_hdr.sensep = sense;
        io_hdr.max_sense_len = sizeof(sense);
        io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;
    
        status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
        if (0 != status)
            return status;
        scsi_do_sense_disect(&io_hdr, &sinfo);
        if ((res = scsiSimpleSenseFilter(&sinfo)))
            return res;
        /* sanity check on response */
        if ((SUPPORTED_LPAGES != pagenum) && (pBuf[0] != pagenum))
            return SIMPLE_ERR_BAD_RESP;
        if (0 == ((pBuf[2] << 8) + pBuf[3]))
            return SIMPLE_ERR_BAD_RESP;
        pageLen = (pBuf[2] << 8) + pBuf[3] + 4;
        /* some SCSI HBA don't like "odd" length transfers */
        if (pageLen % 2)
            pageLen += 1;   
        if (pageLen > bufLen)
            pageLen = bufLen;
    }
    memset(pBuf, 0, 4);
    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = pageLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = LOG_SENSE;
    cdb[2] = 0x40 | (pagenum & 0x3f);  /* Page control (PC)==1 */
    cdb[7] = (pageLen >> 8) & 0xff;
    cdb[8] = pageLen & 0xff;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status)
        return status;
    scsi_do_sense_disect(&io_hdr, &sinfo);
    status = scsiSimpleSenseFilter(&sinfo);
    if (0 != status)
        return status;
    /* sanity check on response */
    if ((SUPPORTED_LPAGES != pagenum) && (pBuf[0] != pagenum))
        return SIMPLE_ERR_BAD_RESP;
    if (0 == ((pBuf[2] << 8) + pBuf[3]))
        return SIMPLE_ERR_BAD_RESP;
    return 0;
}

/* Send MODE SENSE (6 byte) command. Returns 0 if ok, 1 if NOT READY,
 * 2 if command not supported (then MODE SENSE(10) should be supported),
 * 3 if field in command not supported or returns negated errno. 
 * SPC-3 sections 6.9 and 7.4 (rev 22a) [mode subpage==0] */
int scsiModeSense(int device, int pagenum, int pc, UINT8 *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    UINT8 cdb[6];
    UINT8 sense[32];
    int status;

    if ((bufLen < 0) || (bufLen > 255))
        return -EINVAL;
    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = MODE_SENSE;
    cdb[2] = (pc << 6) | (pagenum & 0x3f);
    cdb[4] = bufLen;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status)
        return status;
    scsi_do_sense_disect(&io_hdr, &sinfo);
    status = scsiSimpleSenseFilter(&sinfo);
    if (SIMPLE_ERR_TRY_AGAIN == status) {
        status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
        if (0 != status)
            return status;
        scsi_do_sense_disect(&io_hdr, &sinfo);
        status = scsiSimpleSenseFilter(&sinfo);
    }
    if ((0 == status) && (ALL_MODE_PAGES != pagenum)) {
        int offset;

        offset = scsiModePageOffset(pBuf, bufLen, 0);
        if (offset < 0)
            return SIMPLE_ERR_BAD_RESP;
        else if (pagenum != (pBuf[offset] & 0x3f))
            return SIMPLE_ERR_BAD_RESP;
    }
    return status;
}

/* Sends a 6 byte MODE SELECT command. Assumes given pBuf is the response
 * from a corresponding 6 byte MODE SENSE command. Such a response should
 * have a 4 byte header followed by 0 or more 8 byte block descriptors
 * (normally 1) and then 1 mode page. Returns 0 if ok, 1 if NOT READY,
 * 2 if command not supported (then MODE SELECT(10) may be supported), 
 * 3 if field in command not supported, 4 if bad parameter to command
 * or returns negated errno. SPC-3 sections 6.7 and 7.4 (rev 22a) */
int scsiModeSelect(int device, int sp, UINT8 *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    UINT8 cdb[6];
    UINT8 sense[32];
    int status, pg_offset, pg_len, hdr_plus_1_pg;

    pg_offset = 4 + pBuf[3];
    if (pg_offset + 2 >= bufLen)
        return -EINVAL;
    pg_len = pBuf[pg_offset + 1] + 2;
    hdr_plus_1_pg = pg_offset + pg_len;
    if (hdr_plus_1_pg > bufLen)
        return -EINVAL;
    pBuf[0] = 0;    /* Length of returned mode sense data reserved for SELECT */
    pBuf[pg_offset] &= 0x7f;    /* Mask out PS bit from byte 0 of page data */
    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_TO_DEVICE;
    io_hdr.dxfer_len = hdr_plus_1_pg;
    io_hdr.dxferp = pBuf;
    cdb[0] = MODE_SELECT;
    cdb[1] = 0x10 | (sp & 1);      /* set PF (page format) bit always */
    cdb[4] = hdr_plus_1_pg; /* make sure only one page sent */
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status)
        return status;
    scsi_do_sense_disect(&io_hdr, &sinfo);
    return scsiSimpleSenseFilter(&sinfo);
}

/* MODE SENSE (10 byte). Returns 0 if ok, 1 if NOT READY, 2 if command 
 * not supported (then MODE SENSE(6) might be supported), 3 if field in
 * command not supported or returns negated errno.  
 * SPC-3 sections 6.10 and 7.4 (rev 22a) [mode subpage==0] */
int scsiModeSense10(int device, int pagenum, int pc, UINT8 *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    UINT8 cdb[10];
    UINT8 sense[32];
    int status;

    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = MODE_SENSE_10;
    cdb[2] = (pc << 6) | (pagenum & 0x3f);
    cdb[7] = (bufLen >> 8) & 0xff;
    cdb[8] = bufLen & 0xff;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status)
        return status;
    scsi_do_sense_disect(&io_hdr, &sinfo);
    status = scsiSimpleSenseFilter(&sinfo);
    if (SIMPLE_ERR_TRY_AGAIN == status) {
        status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
        if (0 != status)
            return status;
        scsi_do_sense_disect(&io_hdr, &sinfo);
        status = scsiSimpleSenseFilter(&sinfo);
    }
    if ((0 == status) && (ALL_MODE_PAGES != pagenum)) {
        int offset;

        offset = scsiModePageOffset(pBuf, bufLen, 1);
        if (offset < 0)
            return SIMPLE_ERR_BAD_RESP;
        else if (pagenum != (pBuf[offset] & 0x3f))
            return SIMPLE_ERR_BAD_RESP;
    }
    return status;
}

/* Sends a 10 byte MODE SELECT command. Assumes given pBuf is the response
 * from a corresponding 10 byte MODE SENSE command. Such a response should
 * have a 8 byte header followed by 0 or more 8 byte block descriptors
 * (normally 1) and then 1 mode page. Returns 0 if ok, 1 NOT REAFY, 2 if 
 * command not supported (then MODE SELECT(6) may be supported), 3 if field
 * in command not supported, 4 if bad parameter to command or returns
 * negated errno. SPC-3 sections 6.8 and 7.4 (rev 22a) */
int scsiModeSelect10(int device, int sp, UINT8 *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    UINT8 cdb[10];
    UINT8 sense[32];
    int status, pg_offset, pg_len, hdr_plus_1_pg;

    pg_offset = 8 + (pBuf[6] << 8) + pBuf[7];
    if (pg_offset + 2 >= bufLen)
        return -EINVAL;
    pg_len = pBuf[pg_offset + 1] + 2;
    hdr_plus_1_pg = pg_offset + pg_len;
    if (hdr_plus_1_pg > bufLen)
        return -EINVAL;
    pBuf[0] = 0;    
    pBuf[1] = 0; /* Length of returned mode sense data reserved for SELECT */
    pBuf[pg_offset] &= 0x7f;    /* Mask out PS bit from byte 0 of page data */
    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_TO_DEVICE;
    io_hdr.dxfer_len = hdr_plus_1_pg;
    io_hdr.dxferp = pBuf;
    cdb[0] = MODE_SELECT_10;
    cdb[1] = 0x10 | (sp & 1);      /* set PF (page format) bit always */
    cdb[8] = hdr_plus_1_pg; /* make sure only one page sent */
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status)
        return status;
    scsi_do_sense_disect(&io_hdr, &sinfo);
    return scsiSimpleSenseFilter(&sinfo);
}

/* Standard INQUIRY returns 0 for ok, anything else is a major problem.
 * bufLen should be 36 for unsafe devices (like USB mass storage stuff)
 * otherwise they can lock up! SPC-3 sections 6.4 and 7.6 (rev 22a) */
int scsiStdInquiry(int device, UINT8 *pBuf, int bufLen)
{
    struct scsi_sense_disect sinfo;
    struct scsi_cmnd_io io_hdr;
    UINT8 cdb[6];
    UINT8 sense[32];
    int status;

    if ((bufLen < 0) || (bufLen > 255))
        return -EINVAL;
    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = INQUIRY;
    cdb[4] = bufLen;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status)
        return status;
    scsi_do_sense_disect(&io_hdr, &sinfo);
    return scsiSimpleSenseFilter(&sinfo);
}

/* INQUIRY to fetch Vital Page Data.  Returns 0 if ok, 1 if NOT READY
 * (unlikely), 2 if command not supported, 3 if field in command not 
 * supported, 5 if response indicates that EVPD bit ignored or returns
 * negated errno. SPC-3 section 6.4 and 7.6 (rev 22a) */
int scsiInquiryVpd(int device, int vpd_page, UINT8 *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    UINT8 cdb[6];
    UINT8 sense[32];
    int status, res;

    if ((bufLen < 0) || (bufLen > 255))
        return -EINVAL;
    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    if (bufLen > 1)
        pBuf[1] = 0x0;
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = INQUIRY;
    cdb[1] = 0x1;       /* set EVPD bit (enable Vital Product Data) */
    cdb[2] = vpd_page;
    cdb[4] = bufLen;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status)
        return status;
    scsi_do_sense_disect(&io_hdr, &sinfo);
    if ((res = scsiSimpleSenseFilter(&sinfo)))
        return res;
    /* Guard against devices that ignore EVPD bit and do standard INQUIRY */
    if (bufLen > 1) {
        if (vpd_page == pBuf[1]) {
            if ((0x80 == vpd_page) && (bufLen > 2) && (0x0 != pBuf[2]))
                return SIMPLE_ERR_BAD_RESP;
        } else
            return SIMPLE_ERR_BAD_RESP;
    }
    return 0;
}

/* REQUEST SENSE command. Returns 0 if ok, anything else major problem.
 * SPC-3 section 6.27 (rev 22a) */
int scsiRequestSense(int device, struct scsi_sense_disect * sense_info)
{
    struct scsi_cmnd_io io_hdr;
    UINT8 cdb[6];
    UINT8 sense[32];
    UINT8 buff[18];
    int status, len;
    UINT8 ecode;

    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = sizeof(buff);
    io_hdr.dxferp = buff;
    cdb[0] = REQUEST_SENSE;
    cdb[4] = sizeof(buff);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if ((0 == status) && (sense_info)) {
        ecode = buff[0] & 0x7f;
        sense_info->error_code = ecode;
        sense_info->sense_key = buff[2] & 0xf;
        sense_info->asc = 0;
        sense_info->ascq = 0;
        if ((0x70 == ecode) || (0x71 == ecode)) {
            len = buff[7] + 8;
            if (len > 13) {
                sense_info->asc = buff[12];
                sense_info->ascq = buff[13];
            }
        }
    }
    return status;
}

/* SEND DIAGNOSTIC command.  Returns 0 if ok, 1 if NOT READY, 2 if command
 * not supported, 3 if field in command not supported or returns negated
 * errno. SPC-3 section 6.28 (rev 22a) */
int scsiSendDiagnostic(int device, int functioncode, UINT8 *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    UINT8 cdb[6];
    UINT8 sense[32];
    int status;

    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = bufLen ? DXFER_TO_DEVICE: DXFER_NONE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = SEND_DIAGNOSTIC;
    if (SCSI_DIAG_DEF_SELF_TEST == functioncode)
        cdb[1] = 0x4;  /* SelfTest bit */
    else if (SCSI_DIAG_NO_SELF_TEST != functioncode)
        cdb[1] = (functioncode & 0x7) << 5; /* SelfTest _code_ */
    else   /* SCSI_DIAG_NO_SELF_TEST == functioncode */
        cdb[1] = 0x10;  /* PF bit */
    cdb[3] = (bufLen >> 8) & 0xff;
    cdb[4] = bufLen & 0xff;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    /* worst case is an extended foreground self test on a big disk */
    io_hdr.timeout = SCSI_TIMEOUT_SELF_TEST;
    
    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status)
        return status;
    scsi_do_sense_disect(&io_hdr, &sinfo);
    return scsiSimpleSenseFilter(&sinfo);
}

/* RECEIVE DIAGNOSTIC command. Returns 0 if ok, 1 if NOT READY, 2 if
 * command not supported, 3 if field in command not supported or returns
 * negated errno. SPC-3 section 6.18 (rev 22a) */
int scsiReceiveDiagnostic(int device, int pcv, int pagenum, UINT8 *pBuf, 
                      int bufLen)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    UINT8 cdb[6];
    UINT8 sense[32];
    int status;

    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = RECEIVE_DIAGNOSTIC;
    cdb[1] = pcv;
    cdb[2] = pagenum;
    cdb[3] = (bufLen >> 8) & 0xff;
    cdb[4] = bufLen & 0xff;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status)
        return status;
    scsi_do_sense_disect(&io_hdr, &sinfo);
    return scsiSimpleSenseFilter(&sinfo);
}

/* TEST UNIT READY command. SPC-3 section 6.33 (rev 22a) */
static int _testunitready(int device, struct scsi_sense_disect * sinfo)
{
    struct scsi_cmnd_io io_hdr;
    UINT8 cdb[6];
    UINT8 sense[32];
    int status;

    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_NONE;
    io_hdr.dxfer_len = 0;
    io_hdr.dxferp = NULL;
    cdb[0] = TEST_UNIT_READY;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status)
        return status;
    scsi_do_sense_disect(&io_hdr, sinfo);
    return 0;
}

/* Returns 0 for device responds and media ready, 1 for device responds and
   media not ready, or returns a negated errno value */
int scsiTestUnitReady(int device)
{
    struct scsi_sense_disect sinfo;
    int status;

    status = _testunitready(device, &sinfo);
    if (0 != status)
        return status;
    status = scsiSimpleSenseFilter(&sinfo);
    if (SIMPLE_ERR_TRY_AGAIN == status) {
        /* power on reset, media changed, ok ... try again */
        status = _testunitready(device, &sinfo);        
        if (0 != status)
            return status;
        status = scsiSimpleSenseFilter(&sinfo);
    }
    return status;
}

/* READ DEFECT (10) command. Returns 0 if ok, 1 if NOT READY, 2 if
 * command not supported, 3 if field in command not supported or returns
 * negated errno. SBC-2 section 5.12 (rev 16) */
int scsiReadDefect10(int device, int req_plist, int req_glist, int dl_format,
                     UINT8 *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    UINT8 cdb[10];
    UINT8 sense[32];
    int status;

    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = READ_DEFECT_10;
    cdb[2] = (unsigned char)(((req_plist << 4) & 0x10) |
               ((req_glist << 3) & 0x8) | (dl_format & 0x7));
    cdb[7] = (bufLen >> 8) & 0xff;
    cdb[8] = bufLen & 0xff;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status)
        return status;
    scsi_do_sense_disect(&io_hdr, &sinfo);
    return scsiSimpleSenseFilter(&sinfo);
}

/* Offset into mode sense (6 or 10 byte) response that actual mode page
 * starts at (relative to resp[0]). Returns -1 if problem */
int scsiModePageOffset(const UINT8 * resp, int len, int modese_len)
{
    int resp_len, bd_len;
    int offset = -1;

    if (resp) {
        if (10 == modese_len) {
            resp_len = (resp[0] << 8) + resp[1] + 2;
            bd_len = (resp[6] << 8) + resp[7];
            offset = bd_len + 8;
        } else {
            resp_len = resp[0] + 1;
            bd_len = resp[3];
            offset = bd_len + 4;
        }
        if ((offset + 2) > len) {
            pout("scsiModePageOffset: raw_curr too small, offset=%d "
                 "resp_len=%d bd_len=%d\n", offset, resp_len, bd_len);
            offset = -1;
        } else if ((offset + 2) > resp_len) {
            pout("scsiModePageOffset: response length too short, resp_len=%d"
                 " offset=%d bd_len=%d\n", resp_len, offset, bd_len);
            offset = -1;
        }
    }
    return offset;
}

/* IEC mode page byte 2 bit masks */
#define DEXCPT_ENABLE   0x08
#define EWASC_ENABLE    0x10
#define DEXCPT_DISABLE  0xf7
#define EWASC_DISABLE   0xef
#define TEST_DISABLE    0xfb

/* Fetches the Informational Exceptions Control mode page. First tries
 * the 6 byte MODE SENSE command and if that fails with an illegal opcode
 * tries a 10 byte MODE SENSE command. Returns 0 if successful, a positive
 * number if a known error (see  SIMPLE_ERR_ ...) or a negative errno
 * value. */
int scsiFetchIECmpage(int device, struct scsi_iec_mode_page *iecp, int modese_len)
{
    int err = 0;

    memset(iecp, 0, sizeof(*iecp));
    iecp->modese_len = modese_len;
    iecp->requestedCurrent = 1;
    if (iecp->modese_len <= 6) {
        if ((err = scsiModeSense(device, INFORMATIONAL_EXCEPTIONS_CONTROL_PAGE, 
                                 MPAGE_CONTROL_CURRENT, 
                                 iecp->raw_curr, sizeof(iecp->raw_curr)))) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                iecp->modese_len = 10;
            else {
                iecp->modese_len = 0;
                return err;
            }
        } else if (0 == iecp->modese_len)
            iecp->modese_len = 6;
    }
    if (10 == iecp->modese_len) {
        err = scsiModeSense10(device, INFORMATIONAL_EXCEPTIONS_CONTROL_PAGE,
                              MPAGE_CONTROL_CURRENT, 
                              iecp->raw_curr, sizeof(iecp->raw_curr));
        if (err) {
            iecp->modese_len = 0;
            return err;
        }
    } 
    iecp->gotCurrent = 1;
    iecp->requestedChangeable = 1;
    if (10 == iecp->modese_len)
        err = scsiModeSense10(device, INFORMATIONAL_EXCEPTIONS_CONTROL_PAGE,
                                 MPAGE_CONTROL_CHANGEABLE,
                                 iecp->raw_chg, sizeof(iecp->raw_chg));
    else if (6 == iecp->modese_len)
        err = scsiModeSense(device, INFORMATIONAL_EXCEPTIONS_CONTROL_PAGE, 
                            MPAGE_CONTROL_CHANGEABLE, 
                            iecp->raw_chg, sizeof(iecp->raw_chg));
    if (err)
        return err;
    iecp->gotChangeable = 1;
    return 0;
}

int scsi_IsExceptionControlEnabled(const struct scsi_iec_mode_page *iecp)
{
    int offset;

    if (iecp && iecp->gotCurrent) {
        offset = scsiModePageOffset(iecp->raw_curr, sizeof(iecp->raw_curr),
                                    iecp->modese_len);
        if (offset >= 0)
            return (iecp->raw_curr[offset + 2] & DEXCPT_ENABLE) ? 0 : 1;
        else
            return 0;
    } else
        return 0;
}

int scsi_IsWarningEnabled(const struct scsi_iec_mode_page *iecp)
{
    int offset;

    if (iecp && iecp->gotCurrent) {
        offset = scsiModePageOffset(iecp->raw_curr, sizeof(iecp->raw_curr),
                                    iecp->modese_len);
        if (offset >= 0)
            return (iecp->raw_curr[offset + 2] & EWASC_ENABLE) ? 1 : 0;
        else
            return 0;
    } else
        return 0;
}

/* set EWASC and clear PERF, EBF, DEXCPT TEST and LOGERR */
#define SCSI_IEC_MP_BYTE2_ENABLED 0x10 
#define SCSI_IEC_MP_BYTE2_TEST_MASK 0x4
/* exception/warning via an unrequested REQUEST SENSE command */
#define SCSI_IEC_MP_MRIE 6      
#define SCSI_IEC_MP_INTERVAL_T 0
#define SCSI_IEC_MP_REPORT_COUNT 1

/* Try to set (or clear) both Exception Control and Warning in the IE
 * mode page subject to the "changeable" mask. The object pointed to
 * by iecp is (possibly) inaccurate after this call, therefore
 * scsiFetchIECmpage() should be called again if the IEC mode page
 * is to be re-examined.
 * When -r ioctl is invoked 3 or more time on 'smartctl -s on ...'
 * then set the TEST bit (causes asc,ascq pair of 0x5d,0xff). */
int scsiSetExceptionControlAndWarning(int device, int enabled,
                                      const struct scsi_iec_mode_page *iecp)
{
    int k, offset, resp_len;
    int err = 0;
    UINT8 rout[SCSI_IECMP_RAW_LEN];
    int sp, eCEnabled, wEnabled;

    if ((! iecp) || (! iecp->gotCurrent))
        return -EINVAL;
    offset = scsiModePageOffset(iecp->raw_curr, sizeof(iecp->raw_curr),
                                iecp->modese_len);
    if (offset < 0)
        return -EINVAL;
    memcpy(rout, iecp->raw_curr, SCSI_IECMP_RAW_LEN);
    if (10 == iecp->modese_len) {
        resp_len = (rout[0] << 8) + rout[1] + 2;
        rout[3] &= 0xef;    /* for disks mask out DPOFUA bit */
    } else {
        resp_len = rout[0] + 1;
        rout[2] &= 0xef;    /* for disks mask out DPOFUA bit */
    }
    sp = (rout[offset] & 0x80) ? 1 : 0; /* PS bit becomes 'SELECT's SP bit */
    if (enabled) {
        rout[offset + 2] = SCSI_IEC_MP_BYTE2_ENABLED;
        if (con->reportscsiioctl > 2)
            rout[offset + 2] |= SCSI_IEC_MP_BYTE2_TEST_MASK;
        rout[offset + 3] = SCSI_IEC_MP_MRIE;
        rout[offset + 4] = (SCSI_IEC_MP_INTERVAL_T >> 24) & 0xff;
        rout[offset + 5] = (SCSI_IEC_MP_INTERVAL_T >> 16) & 0xff;
        rout[offset + 6] = (SCSI_IEC_MP_INTERVAL_T >> 8) & 0xff;
        rout[offset + 7] = SCSI_IEC_MP_INTERVAL_T & 0xff;
        rout[offset + 8] = (SCSI_IEC_MP_REPORT_COUNT >> 24) & 0xff;
        rout[offset + 9] = (SCSI_IEC_MP_REPORT_COUNT >> 16) & 0xff;
        rout[offset + 10] = (SCSI_IEC_MP_REPORT_COUNT >> 8) & 0xff;
        rout[offset + 11] = SCSI_IEC_MP_REPORT_COUNT & 0xff;
        if (iecp->gotChangeable) {
            UINT8 chg2 = iecp->raw_chg[offset + 2];

            rout[offset + 2] = chg2 ? (rout[offset + 2] & chg2) :
                                      iecp->raw_curr[offset + 2];
            for (k = 3; k < 12; ++k) {
                if (0 == iecp->raw_chg[offset + k])
                    rout[offset + k] = iecp->raw_curr[offset + k];
            }
        }
        if (0 == memcmp(&rout[offset + 2], &iecp->raw_chg[offset + 2], 10)) {
            if (con->reportscsiioctl > 0)
                pout("scsiSetExceptionControlAndWarning: already enabled\n");
            return 0;
        }
    } else { /* disabling Exception Control and (temperature) Warnings */
        eCEnabled = (rout[offset + 2] & DEXCPT_ENABLE) ? 0 : 1;
        wEnabled = (rout[offset + 2] & EWASC_ENABLE) ? 1 : 0;
        if ((! eCEnabled) && (! wEnabled)) {
            if (con->reportscsiioctl > 0)
                pout("scsiSetExceptionControlAndWarning: already disabled\n");
            return 0;   /* nothing to do, leave other setting alone */
        }
        if (wEnabled) 
            rout[offset + 2] &= EWASC_DISABLE;
        if (eCEnabled) {
            if (iecp->gotChangeable && 
                (iecp->raw_chg[offset + 2] & DEXCPT_ENABLE))
                rout[offset + 2] |= DEXCPT_ENABLE;
                rout[offset + 2] &= TEST_DISABLE;/* clear TEST bit for spec */
        }
    }
    if (10 == iecp->modese_len)
        err = scsiModeSelect10(device, sp, rout, resp_len);
    else if (6 == iecp->modese_len)
        err = scsiModeSelect(device, sp, rout, resp_len);
    return err;
}

int scsiGetTemp(int device, UINT8 *currenttemp, UINT8 *triptemp)
{
    UINT8 tBuf[252];
    int err;

    memset(tBuf, 0, sizeof(tBuf));
    if ((err = scsiLogSense(device, TEMPERATURE_LPAGE, tBuf, 
                            sizeof(tBuf), 0))) {
        *currenttemp = 0;
        *triptemp = 0;
        pout("Log Sense for temperature failed [%s]\n", scsiErrString(err));
        return err;
    }
    *currenttemp = tBuf[9];
    *triptemp = tBuf[15];
    return 0;
}

/* Read informational exception log page or Request Sense response.
 * Fetching asc/ascq code potentially flagging an exception or warning.
 * Returns 0 if ok, else error number. A current temperature of 255
 * (Celsius) implies that the temperature not available. */
int scsiCheckIE(int device, int hasIELogPage, int hasTempLogPage,
                UINT8 *asc, UINT8 *ascq, UINT8 *currenttemp,
                UINT8 *triptemp)
{
    UINT8 tBuf[252];
    struct scsi_sense_disect sense_info;
    int err;
    int temperatureSet = 0;
    unsigned short pagesize;
    UINT8 currTemp, trTemp;
 
    *asc = 0;
    *ascq = 0;
    *currenttemp = 0;
    *triptemp = 0;
    memset(tBuf,0,sizeof(tBuf)); // need to clear stack space of junk
    memset(&sense_info, 0, sizeof(sense_info));
    if (hasIELogPage) {
        if ((err = scsiLogSense(device, IE_LPAGE, tBuf, 
                                sizeof(tBuf), 0))) {
            pout("Log Sense failed, IE page [%s]\n", scsiErrString(err));
            return err;
        }
        // pull out page size from response, don't forget to add 4
        pagesize = (unsigned short) ((tBuf[2] << 8) | tBuf[3]) + 4; 
        if ((pagesize < 4) || tBuf[4] || tBuf[5]) {
            pout("Log Sense failed, IE page, bad parameter code or length\n");
            return SIMPLE_ERR_BAD_PARAM;
        }
        if (tBuf[7] > 1) {
            sense_info.asc = tBuf[8]; 
            sense_info.ascq = tBuf[9];
            if (! hasTempLogPage) {
                if (tBuf[7] > 2) 
                    *currenttemp = tBuf[10];
                if (tBuf[7] > 3)        /* IBM extension in SMART (IE) lpage */
                    *triptemp = tBuf[11];
            }
        } 
    }
    if (0 == sense_info.asc) {    
        /* ties in with MRIE field of 6 in IEC mode page (0x1c) */
        if ((err = scsiRequestSense(device, &sense_info))) {
            pout("Request Sense failed, [%s]\n", scsiErrString(err));
            return err;
        }
    }
    *asc = sense_info.asc;
    *ascq = sense_info.ascq;
    if ((! temperatureSet) && hasTempLogPage) {
        if (0 == scsiGetTemp(device, &currTemp, &trTemp)) {
            *currenttemp = currTemp;
            *triptemp = trTemp;
        }
    }
    return 0;
}

// The first character (W, C, I) tells the severity
static const char * TapeAlertsMessageTable[]= {  
    " ",
    /* 0x01 */
   "W: The tape drive is having problems reading data. No data has been lost,\n"
       "  but there has been a reduction in the performance of the tape.",
    /* 0x02 */
   "W: The tape drive is having problems writing data. No data has been lost,\n"
       "  but there has been a reduction in the capacity of the tape.",
    /* 0x03 */
   "W: The operation has stopped because an error has occurred while reading\n"
       "  or writing data that the drive cannot correct.",
    /* 0x04 */
   "C: Your data is at risk:\n"
       "  1. Copy any data you require from this tape. \n"
       "  2. Do not use this tape again.\n"
       "  3. Restart the operation with a different tape.",
    /* 0x05 */
   "C: The tape is damaged or the drive is faulty. Call the tape drive\n"
       "  supplier helpline.",
    /* 0x06 */
   "C: The tape is from a faulty batch or the tape drive is faulty:\n"
       "  1. Use a good tape to test the drive.\n"
       "  2. If problem persists, call the tape drive supplier helpline.",
    /* 0x07 */
   "W: The tape cartridge has reached the end of its calculated useful life:\n"
       "  1. Copy data you need to another tape.\n"
       "  2. Discard the old tape.",
    /* 0x08 */
   "W: The tape cartridge is not data-grade. Any data you back up to the tape\n"
       "  is at risk. Replace the cartridge with a data-grade tape.",
    /* 0x09 */
   "C: You are trying to write to a write-protected cartridge. Remove the\n"
       "  write-protection or use another tape.",
    /* 0x0a */
   "I: You cannot eject the cartridge because the tape drive is in use. Wait\n"
       "  until the operation is complete before ejecting the cartridge.",
    /* 0x0b */
   "I: The tape in the drive is a cleaning cartridge.",
    /* 0x0c */
   "I: You have tried to load a cartridge of a type which is not supported\n"
       "  by this drive.",
    /* 0x0d */
   "C: The operation has failed because the tape in the drive has experienced\n"
       "  a mechanical failure:\n"
       "  1. Discard the old tape.\n"
       "  2. Restart the operation with a different tape.",
    /* 0x0e */
   "C: The operation has failed because the tape in the drive has experienced\n"
       "  a mechanical failure:\n"
       "  1. Do not attempt to extract the tape cartridge\n"
       "  2. Call the tape drive supplier helpline.",
    /* 0x0f */
   "W: The memory in the tape cartridge has failed, which reduces\n"
       "  performance. Do not use the cartridge for further write operations.",
    /* 0x10 */
   "C: The operation has failed because the tape cartridge was manually\n"
       "  de-mounted while the tape drive was actively writing or reading.",
    /* 0x11 */
   "W: You have loaded a cartridge of a type that is read-only in this drive.\n"
       "  The cartridge will appear as write-protected.",
    /* 0x12 */
   "W: The tape directory on the tape cartridge has been corrupted. File\n"
       "  search performance will be degraded. The tape directory can be rebuilt\n"
       "  by reading all the data on the cartridge.",
    /* 0x13 */
   "I: The tape cartridge is nearing the end of its calculated life. It is\n"
       "  recommended that you:\n"
       "  1. Use another tape cartridge for your next backup.\n"
       "  2. Store this tape in a safe place in case you need to restore "
       "  data from it.",
    /* 0x14 */
   "C: The tape drive needs cleaning:\n"
       "  1. If the operation has stopped, eject the tape and clean the drive.\n"
       "  2. If the operation has not stopped, wait for it to finish and then\n"
       "  clean the drive.\n"
       "  Check the tape drive users manual for device specific cleaning instructions.",
    /* 0x15 */
   "W: The tape drive is due for routine cleaning:\n"
       "  1. Wait for the current operation to finish.\n"
       "  2. The use a cleaning cartridge.\n"
       "  Check the tape drive users manual for device specific cleaning instructions.",
    /* 0x16 */
   "C: The last cleaning cartridge used in the tape drive has worn out:\n"
       "  1. Discard the worn out cleaning cartridge.\n"
       "  2. Wait for the current operation to finish.\n"
       "  3. Then use a new cleaning cartridge.",
    /* 0x17 */
   "C: The last cleaning cartridge used in the tape drive was an invalid\n"
       "  type:\n"
       "  1. Do not use this cleaning cartridge in this drive.\n"
       "  2. Wait for the current operation to finish.\n"
       "  3. Then use a new cleaning cartridge.",
    /* 0x18 */
   "W: The tape drive has requested a retention operation",
    /* 0x19 */
   "W: A redundant interface port on the tape drive has failed",
    /* 0x1a */
   "W: A tape drive cooling fan has failed",
    /* 0x1b */
   "W: A redundant power supply has failed inside the tape drive enclosure.\n"
       "  Check the enclosure users manual for instructions on replacing the\n"
       "  failed power supply.",
    /* 0x1c */
   "W: The tape drive power consumption is outside the specified range.",
    /* 0x1d */
   "W: Preventive maintenance of the tape drive is required. Check the tape\n"
       "  drive users manual for device specific preventive maintenance\n"
       "  tasks or call the tape drive supplier helpline.",
    /* 0x1e */
   "C: The tape drive has a hardware fault:\n"
       "  1. Eject the tape or magazine.\n"
       "  2. Reset the drive.\n"
       "  3. Restart the operation.",
    /* 0x1f */
   "C: The tape drive has a hardware fault:\n"
       "  1. Turn the tape drive off and then on again.\n"
       "  2. Restart the operation.\n"
    "  3. If the problem persists, call the tape drive supplier helpline.",
    /* 0x20 */
   "W: The tape drive has a problem with the application client interface:\n"
       "  1. Check the cables and cable connections.\n"
       "  2. Restart the operation.",
    /* 0x21 */
   "C: The operation has failed:\n"
       "  1. Eject the tape or magazine.\n"
       "  2. Insert the tape or magazine again.\n"
       "  3. Restart the operation.",
    /* 0x22 */
   "W: The firmware download has failed because you have tried to use the\n"
       "  incorrect firmware for this tape drive. Obtain the correct\n"
       "  firmware and try again.",
    /* 0x23 */
   "W: Environmental conditions inside the tape drive are outside the\n"
       "  specified humidity range.",
    /* 0x24 */
   "W: Environmental conditions inside the tape drive are outside the\n"
       "  specified temperature range.",
    /* 0x25 */
   "W: The voltage supply to the tape drive is outside the specified range.",
    /* 0x26 */
   "C: A hardware failure of the tape drive is predicted. Call the tape\n"
       "  drive supplier helpline.",
    /* 0x27 */
   "W: The tape drive may have a hardware fault. Run extended diagnostics to\n"
       "  verify and diagnose the problem. Check the tape drive users manual for\n"
       "  device specific instructions on running extended diagnostic tests.",
    /* 0x28 */
   "C: The changer mechanism is having difficulty communicating with the tape\n"
       "  drive:\n"
       "  1. Turn the autoloader off then on.\n"
       "  2. Restart the operation.\n"
       "  3. If problem persists, call the tape drive supplier helpline.",
    /* 0x29 */
   "C: A tape has been left in the autoloader by a previous hardware fault:\n"
       "  1. Insert an empty magazine to clear the fault.\n"
       "  2. If the fault does not clear, turn the autoloader off and then\n"
       "  on again.\n"
       "  3. If the problem persists, call the tape drive supplier helpline.",
    /* 0x2a */
   "W: There is a problem with the autoloader mechanism.",
    /* 0x2b */
   "C: The operation has failed because the autoloader door is open:\n"
       "  1. Clear any obstructions from the autoloader door.\n"
       "  2. Eject the magazine and then insert it again.\n"
       "  3. If the fault does not clear, turn the autoloader off and then\n"
       "  on again.\n"
       "  4. If the problem persists, call the tape drive supplier helpline.",
    /* 0x2c */
   "C: The autoloader has a hardware fault:\n"
       "  1. Turn the autoloader off and then on again.\n"
       "  2. Restart the operation.\n"
       "  3. If the problem persists, call the tape drive supplier helpline.\n"
       "  Check the autoloader users manual for device specific instructions\n"
       "  on turning the device power on and off.",
    /* 0x2d */
   "C: The autoloader cannot operate without the magazine,\n"
       "  1. Insert the magazine into the autoloader.\n"
       "  2. Restart the operation.",
    /* 0x2e */
   "W: A hardware failure of the changer mechanism is predicted. Call the\n"
       "  tape drive supplier helpline.",
    /* 0x2f */
   "I: Reserved.",
    /* 0x30 */
   "I: Reserved.",
    /* 0x31 */
   "I: Reserved.",
    /* 0x32 */
   "W: Media statistics have been lost at some time in the past",
    /* 0x33 */
   "W: The tape directory on the tape cartridge just unloaded has been\n"
       "  corrupted. File search performance will be degraded. The tape\n"
       "  directory can be rebuilt by reading all the data.",
    /* 0x34 */
   "C: The tape just unloaded could not write its system area successfully:\n"
       "  1. Copy data to another tape cartridge.\n"
       "  2. Discard the old cartridge.",
    /* 0x35 */
   "C: The tape system are could not be read successfully at load time:\n"
    "  1. Copy data to another tape cartridge.\n",
    /* 0x36 */
   "C: The start or data could not be found on the tape:\n"
       "  1. Check you are using the correct format tape.\n"
       "  2. Discard the tape or return the tape to your supplier",
    /* 0x37 */
    "C: The operation has failed because the media cannot be loaded\n"
        "  and threaded.\n"
        "  1. Remove the cartridge, inspect it as specified in the product\n"
        "  manual, and retry the operation.\n"
        "  2. If the problem persists, call the tape drive supplier help line.",
    /* 0x38 */
    "C: The operation has failed because the medium cannot be unloaded:\n"
        "  1. Do not attempt to extract the tape cartridge.\n"
        "  2. Call the tape driver supplier help line.",
    /* 0x39 */
    "C: The tape drive has a problem with the automation interface:\n"
        "  1. Check the power to the automation system.\n"
        "  2. Check the cables and cable connections.\n"
        "  3. Call the supplier help line if problem persists.",
    /* 0x3a */
    "W: The tape drive has reset itself due to a detected firmware\n"
        "  fault. If problem persists, call the supplier help line.",
    };

const char * scsiTapeAlertsTapeDevice(unsigned short code)
{
    const int num = sizeof(TapeAlertsMessageTable) /
                        sizeof(TapeAlertsMessageTable[0]);

    return (code < num) ?  TapeAlertsMessageTable[code] : "Unknown Alert"; 
}

// The first character (W, C, I) tells the severity
static const char * ChangerTapeAlertsMessageTable[]= {  
    " ",
    /* 0x01 */
    "C: The library mechanism is having difficulty communicating with the\n"
        "  drive:\n"
        "  1. Turn the library off then on.\n"
        "  2. Restart the operation.\n"
        "  3. If the problem persists, call the library supplier help line.",
    /* 0x02 */
    "W: There is a problem with the library mechanism. If problem persists,\n"
        "  call the library supplier help line.",
    /* 0x03 */
    "C: The library has a hardware fault:\n"
        "  1. Reset the library.\n"
        "  2. Restart the operation.\n"
        "  Check the library users manual for device specific instructions on resetting\n"
        "  the device.",
    /* 0x04 */
    "C: The library has a hardware fault:\n"
        "  1. Turn the library off then on again.\n"
        "  2. Restart the operation.\n"
        "  3. If the problem persists, call the library supplier help line.\n"
        "  Check the library users manual for device specific instructions on turning the\n"
        "  device power on and off.",
    /* 0x05 */
    "W: The library mechanism may have a hardware fault.\n"
        "  Run extended diagnostics to verify and diagnose the problem. Check the library\n"
        "  users manual for device specific instructions on running extended diagnostic\n"
        "  tests.",
    /* 0x06 */
    "C: The library has a problem with the host interface:\n"
        "  1. Check the cables and connections.\n"
        "  2. Restart the operation.",
    /* 0x07 */
    "W: A hardware failure of the library is predicted. Call the library\n"
        "  supplier help line.",
    /* 0x08 */
    "W: Preventive maintenance of the library is required.\n"
        "  Check the library users manual for device specific preventative maintenance\n"
        "  tasks, or call your library supplier help line.",
    /* 0x09 */
    "C: General environmental conditions inside the library are outside the\n"
        "  specified humidity range.",
    /* 0x0a */
    "C: General environmental conditions inside the library are outside the\n"
        "  specified temperature range.",
    /* 0x0b */
    "C: The voltage supply to the library is outside the specified range.\n"
        "  There is a potential problem with the power supply or failure of\n"
        "  a redundant power supply.",
    /* 0x0c */
    "C: A cartridge has been left inside the library by a previous hardware\n"
        "  fault:\n"
        "  1. Insert an empty magazine to clear the fault.\n"
        "  2. If the fault does not clear, turn the library off and then on again.\n"
        "  3. If the problem persists, call the library supplier help line.",
    /* 0x0d */
    "W: There is a potential problem with the drive ejecting cartridges or\n"
        "  with the library mechanism picking a cartridge from a slot.\n"
        "  1. No action needs to be taken at this time.\n"
        "  2. If the problem persists, call the library supplier help line.",
    /* 0x0e */
    "W: There is a potential problem with the library mechanism placing a\n"
        "  cartridge into a slot.\n"
        "  1. No action needs to be taken at this time.\n"
        "  2. If the problem persists, call the library supplier help line.",
    /* 0x0f */
    "W: There is a potential problem with the drive or the library mechanism\n"
        "  loading cartridges, or an incompatible cartridge.",
    /* 0x10 */
    "C: The library has failed because the door is open:\n"
        "  1. Clear any obstructions from the library door.\n"
        "  2. Close the library door.\n"
        "  3. If the problem persists, call the library supplier help line.",
    /* 0x11 */
    "C: There is a mechanical problem with the library media import/export\n"
        "  mailslot.",
    /* 0x12 */
    "C: The library cannot operate without the magazine.\n"
        "  1. Insert the magazine into the library.\n"
        "  2. Restart the operation.",
    /* 0x13 */
    "W: Library security has been compromised.",
    /* 0x14 */
    "I: The library security mode has been changed.\n"
        "  The library has either been put into secure mode, or the library has exited\n"
        "  the secure mode.\n"
        "  This is for information purposes only. No action is required.",
    /* 0x15 */
    "I: The library has been manually turned offline and is unavailable for use.",
    /* 0x16 */
    "I: A drive inside the library has been taken offline.\n"
        "  This is for information purposes only. No action is required.",
    /* 0x17 */
    "W: There is a potential problem with the bar code label or the scanner\n"
        "  hardware in the library mechanism.\n"
        "  1. No action needs to be taken at this time.\n"
        "  2. If the problem persists, call the library supplier help line.",
    /* 0x18 */
    "C: The library has detected an inconsistency in its inventory.\n"
        "  1. Redo the library inventory to correct inconsistency.\n"
        "  2. Restart the operation.\n"
        "  Check the applications users manual or the hardware users manual for\n"
        "  specific instructions on redoing the library inventory.",
    /* 0x19 */
    "W: A library operation has been attempted that is invalid at this time.",
    /* 0x1a */
    "W: A redundant interface port on the library has failed.",
    /* 0x1b */
    "W: A library cooling fan has failed.",
    /* 0x1c */
    "W: A redundant power supply has failed inside the library. Check the\n"
        "  library users manual for instructions on replacing the failed power supply.",
    /* 0x1d */
    "W: The library power consumption is outside the specified range.",
    /* 0x1e */
    "C: A failure has occurred in the cartridge pass-through mechanism between\n"
        "  two library modules.",
    /* 0x1f */
    "C: A cartridge has been left in the pass-through mechanism from a\n"
        "  previous hardware fault. Check the library users guide for instructions on\n"
        "  clearing this fault.",
    /* 0x20 */
    "I: The library was unable to read the bar code on a cartridge.",
};

const char * scsiTapeAlertsChangerDevice(unsigned short code)
{
    const int num = sizeof(ChangerTapeAlertsMessageTable) /
                        sizeof(ChangerTapeAlertsMessageTable[0]);

    return (code < num) ?  ChangerTapeAlertsMessageTable[code] : "Unknown Alert"; 
}


/* this is a subset of the SCSI additional sense code strings indexed
 * by "ascq" for the case when asc==SCSI_ASC_IMPENDING_FAILURE (0x5d)
 */
static const char * strs_for_asc_5d[] = {
   /* 0x00 */   "FAILURE PREDICTION THRESHOLD EXCEEDED",
        "MEDIA FAILURE PREDICTION THRESHOLD EXCEEDED",
        "LOGICAL UNIT FAILURE PREDICTION THRESHOLD EXCEEDED",
        "SPARE AREA EXHAUSTION PREDICTION THRESHOLD EXCEEDED",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
        "",
   /* 0x10 */   "HARDWARE IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
        "HARDWARE IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
        "HARDWARE IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
        "HARDWARE IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
        "HARDWARE IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
        "HARDWARE IMPENDING FAILURE ACCESS TIMES TOO HIGH",
        "HARDWARE IMPENDING FAILURE START UNIT TIMES TOO HIGH",
        "HARDWARE IMPENDING FAILURE CHANNEL PARAMETRICS",
        "HARDWARE IMPENDING FAILURE CONTROLLER DETECTED",
        "HARDWARE IMPENDING FAILURE THROUGHPUT PERFORMANCE",
        "HARDWARE IMPENDING FAILURE SEEK TIME PERFORMANCE",
        "HARDWARE IMPENDING FAILURE SPIN-UP RETRY COUNT",
        "HARDWARE IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT",
        "",
        "",
        "",
   /* 0x20 */   "CONTROLLER IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
        "CONTROLLER IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
        "CONTROLLER IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
        "CONTROLLER IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
        "CONTROLLER IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
        "CONTROLLER IMPENDING FAILURE ACCESS TIMES TOO HIGH",
        "CONTROLLER IMPENDING FAILURE START UNIT TIMES TOO HIGH",
        "CONTROLLER IMPENDING FAILURE CHANNEL PARAMETRICS",
        "CONTROLLER IMPENDING FAILURE CONTROLLER DETECTED",
        "CONTROLLER IMPENDING FAILURE THROUGHPUT PERFORMANCE",
        "CONTROLLER IMPENDING FAILURE SEEK TIME PERFORMANCE",
        "CONTROLLER IMPENDING FAILURE SPIN-UP RETRY COUNT",
        "CONTROLLER IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT",
        "",
        "",
        "",
   /* 0x30 */   "DATA CHANNEL IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
        "DATA CHANNEL IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
        "DATA CHANNEL IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
        "DATA CHANNEL IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
        "DATA CHANNEL IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
        "DATA CHANNEL IMPENDING FAILURE ACCESS TIMES TOO HIGH",
        "DATA CHANNEL IMPENDING FAILURE START UNIT TIMES TOO HIGH",
        "DATA CHANNEL IMPENDING FAILURE CHANNEL PARAMETRICS",
        "DATA CHANNEL IMPENDING FAILURE CONTROLLER DETECTED",
        "DATA CHANNEL IMPENDING FAILURE THROUGHPUT PERFORMANCE",
        "DATA CHANNEL IMPENDING FAILURE SEEK TIME PERFORMANCE",
        "DATA CHANNEL IMPENDING FAILURE SPIN-UP RETRY COUNT",
        "DATA CHANNEL IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT",
        "",
        "",
        "",
   /* 0x40 */   "SERVO IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
        "SERVO IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
        "SERVO IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
        "SERVO IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
        "SERVO IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
        "SERVO IMPENDING FAILURE ACCESS TIMES TOO HIGH",
        "SERVO IMPENDING FAILURE START UNIT TIMES TOO HIGH",
        "SERVO IMPENDING FAILURE CHANNEL PARAMETRICS",
        "SERVO IMPENDING FAILURE CONTROLLER DETECTED",
        "SERVO IMPENDING FAILURE THROUGHPUT PERFORMANCE",
        "SERVO IMPENDING FAILURE SEEK TIME PERFORMANCE",
        "SERVO IMPENDING FAILURE SPIN-UP RETRY COUNT",
        "SERVO IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT",
        "",
        "",
        "",
   /* 0x50 */   "SPINDLE IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
        "SPINDLE IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
        "SPINDLE IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
        "SPINDLE IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
        "SPINDLE IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
        "SPINDLE IMPENDING FAILURE ACCESS TIMES TOO HIGH",
        "SPINDLE IMPENDING FAILURE START UNIT TIMES TOO HIGH",
        "SPINDLE IMPENDING FAILURE CHANNEL PARAMETRICS",
        "SPINDLE IMPENDING FAILURE CONTROLLER DETECTED",
        "SPINDLE IMPENDING FAILURE THROUGHPUT PERFORMANCE",
        "SPINDLE IMPENDING FAILURE SEEK TIME PERFORMANCE",
        "SPINDLE IMPENDING FAILURE SPIN-UP RETRY COUNT",
        "SPINDLE IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT",
        "",
        "",
        "",
   /* 0x60 */   "FIRMWARE IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
        "FIRMWARE IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
        "FIRMWARE IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
        "FIRMWARE IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
        "FIRMWARE IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
        "FIRMWARE IMPENDING FAILURE ACCESS TIMES TOO HIGH",
        "FIRMWARE IMPENDING FAILURE START UNIT TIMES TOO HIGH",
        "FIRMWARE IMPENDING FAILURE CHANNEL PARAMETRICS",
        "FIRMWARE IMPENDING FAILURE CONTROLLER DETECTED",
        "FIRMWARE IMPENDING FAILURE THROUGHPUT PERFORMANCE",
        "FIRMWARE IMPENDING FAILURE SEEK TIME PERFORMANCE",
        "FIRMWARE IMPENDING FAILURE SPIN-UP RETRY COUNT",
   /* 0x6c */   "FIRMWARE IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT"};


/* this is a subset of the SCSI additional sense code strings indexed
 *  * by "ascq" for the case when asc==SCSI_ASC_WARNING (0xb)
 *   */
static const char * strs_for_asc_b[] = {
       /* 0x00 */   "WARNING",
               "WARNING - SPECIFIED TEMPERATURE EXCEEDED",
               "WARNING - ENCLOSURE DEGRADED"};

static char spare_buff[128];

const char * scsiGetIEString(UINT8 asc, UINT8 ascq)
{
    const char * rp;

    if (SCSI_ASC_IMPENDING_FAILURE == asc) {
        if (ascq == 0xff)
            return "FAILURE PREDICTION THRESHOLD EXCEEDED (FALSE)";
        else if (ascq < 
                 (sizeof(strs_for_asc_5d) / sizeof(strs_for_asc_5d[0]))) {
            rp = strs_for_asc_5d[ascq];
            if (strlen(rp) > 0)
                return rp;
        }
        snprintf(spare_buff, sizeof(spare_buff),
                 "FAILURE PREDICTION THRESHOLD EXCEEDED: ascq=0x%x", ascq);
        return spare_buff;
    } else if (SCSI_ASC_WARNING == asc) {
        if (ascq < (sizeof(strs_for_asc_b) / sizeof(strs_for_asc_b[0]))) {
            rp = strs_for_asc_b[ascq];
            if (strlen(rp) > 0)
                return rp;
        }
        snprintf(spare_buff, sizeof(spare_buff), "WARNING: ascq=0x%x", ascq);
        return spare_buff;
    }
    return NULL;        /* not a IE additional sense code */
}


/* This is not documented in t10.org, page 0x80 is vendor specific */
/* Some IBM disks do an offline read-scan when they get this command. */
int scsiSmartIBMOfflineTest(int device)
{       
    UINT8 tBuf[256];
    int res;
        
    memset(tBuf, 0, sizeof(tBuf));
    /* Build SMART Off-line Immediate Diag Header */
    tBuf[0] = 0x80; /* Page Code */
    tBuf[1] = 0x00; /* Reserved */
    tBuf[2] = 0x00; /* Page Length MSB */
    tBuf[3] = 0x04; /* Page Length LSB */
    tBuf[4] = 0x03; /* SMART Revision */
    tBuf[5] = 0x00; /* Reserved */
    tBuf[6] = 0x00; /* Off-line Immediate Time MSB */
    tBuf[7] = 0x00; /* Off-line Immediate Time LSB */
    res = scsiSendDiagnostic(device, SCSI_DIAG_NO_SELF_TEST, tBuf, 8);
    if (res)
        pout("IBM offline test failed [%s]\n", scsiErrString(res));
    return res;
}

int scsiSmartDefaultSelfTest(int device)
{       
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_DEF_SELF_TEST, NULL, 0);
    if (res)
        pout("Default self test failed [%s]\n", scsiErrString(res));
    return res;
}

int scsiSmartShortSelfTest(int device)
{       
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_BG_SHORT_SELF_TEST, NULL, 0);
    if (res)
        pout("Short offline self test failed [%s]\n", scsiErrString(res));
    return res;
}

int scsiSmartExtendSelfTest(int device)
{       
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_BG_EXTENDED_SELF_TEST, NULL, 0);
    if (res)
        pout("Long (extended) offline self test failed [%s]\n",
             scsiErrString(res));
    return res;
}

int scsiSmartShortCapSelfTest(int device)
{       
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_FG_SHORT_SELF_TEST, NULL, 0);
    if (res)
        pout("Short foreground self test failed [%s]\n", scsiErrString(res));
    return res;
}

int scsiSmartExtendCapSelfTest(int device)
{
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_FG_EXTENDED_SELF_TEST, NULL, 0);
    if (res)
        pout("Long (extended) foreground self test failed [%s]\n",
             scsiErrString(res));
    return res;
}

int scsiSmartSelfTestAbort(int device)
{
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_ABORT_SELF_TEST, NULL, 0);
    if (res)
        pout("Abort self test failed [%s]\n", scsiErrString(res));
    return res;
}

/* Returns 0 and the expected duration of an extended self test (in seconds)
   if successful; any other return value indicates a failure. */
int scsiFetchExtendedSelfTestTime(int device, int * durationSec, int modese_len)
{
    int err, offset, res;
    UINT8 buff[64];

    memset(buff, 0, sizeof(buff));
    if (modese_len <= 6) {
        if ((err = scsiModeSense(device, CONTROL_MODE_PAGE, 
                                 MPAGE_CONTROL_CURRENT, 
                                 buff, sizeof(buff)))) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                modese_len = 10;
            else
                return err;
        } else if (0 == modese_len)
            modese_len = 6;
    }
    if (10 == modese_len) {
        err = scsiModeSense10(device, CONTROL_MODE_PAGE, 
                              MPAGE_CONTROL_CURRENT, 
                              buff, sizeof(buff));
        if (err)
            return err;
    } 
    offset = scsiModePageOffset(buff, sizeof(buff), modese_len);
    if (offset < 0)
        return -EINVAL;
    if (buff[offset + 1] >= 0xa) {
        res = (buff[offset + 10] << 8) | buff[offset + 11];
        *durationSec = res;
        return 0;
    }
    else
        return -EINVAL;
}

void scsiDecodeErrCounterPage(unsigned char * resp, 
                              struct scsiErrorCounter *ecp)
{
    int k, j, num, pl, pc;
    unsigned char * ucp;
    unsigned char * xp;
    uint64_t * ullp;

    memset(ecp, 0, sizeof(*ecp));
    num = (resp[2] << 8) | resp[3];
    ucp = &resp[0] + 4;
    while (num > 3) {
        pc = (ucp[0] << 8) | ucp[1];
        pl = ucp[3] + 4;
        switch (pc) {
            case 0: 
            case 1: 
            case 2: 
            case 3: 
            case 4: 
            case 5: 
            case 6: 
                ecp->gotPC[pc] = 1;
                ullp = &ecp->counter[pc];
                break;
        default: 
                ecp->gotExtraPC = 1;
                ullp = &ecp->counter[7];
                break;
        }
        k = pl - 4;
        xp = ucp + 4;
        if (k > (int)sizeof(*ullp)) {
            xp += (k - sizeof(*ullp));
            k = sizeof(*ullp);
        }
        *ullp = 0;
        for (j = 0; j < k; ++j) {
            if (j > 0)
                *ullp <<= 8;
            *ullp |= xp[j];
        }
        num -= pl;
        ucp += pl;
    }
}

void scsiDecodeNonMediumErrPage(unsigned char *resp, 
                                struct scsiNonMediumError *nmep)
{
    int k, j, num, pl, pc, szof;
    unsigned char * ucp;
    unsigned char * xp;

    memset(nmep, 0, sizeof(*nmep));
    num = (resp[2] << 8) | resp[3];
    ucp = &resp[0] + 4;
    szof = sizeof(nmep->counterPC0);
    while (num > 3) {
        pc = (ucp[0] << 8) | ucp[1];
        pl = ucp[3] + 4;
        switch (pc) {
            case 0: 
                nmep->gotPC0 = 1;
                k = pl - 4;
                xp = ucp + 4;
                if (k > szof) {
                    xp += (k - szof);
                    k = szof;
                }
                nmep->counterPC0 = 0;
                for (j = 0; j < k; ++j) {
                    if (j > 0)
                        nmep->counterPC0 <<= 8;
                    nmep->counterPC0 |= xp[j];
                }
                break;
            case 0x8009: 
                nmep->gotTFE_H = 1;
                k = pl - 4;
                xp = ucp + 4;
                if (k > szof) {
                    xp += (k - szof);
                    k = szof;
                }
                nmep->counterTFE_H = 0;
                for (j = 0; j < k; ++j) {
                    if (j > 0)
                        nmep->counterTFE_H <<= 8;
                    nmep->counterTFE_H |= xp[j];
                }
                break;
            case 0x8015: 
                nmep->gotPE_H = 1;
                k = pl - 4;
                xp = ucp + 4;
                if (k > szof) {
                    xp += (k - szof);
                    k = szof;
                }
                nmep->counterPE_H = 0;
                for (j = 0; j < k; ++j) {
                    if (j > 0)
                        nmep->counterPE_H <<= 8;
                    nmep->counterPE_H |= xp[j];
                }
                break;
        default: 
                nmep->gotExtraPC = 1;
                break;
        }
        num -= pl;
        ucp += pl;
    }
}

/* Counts number of failed self-tests. Also encodes the poweron_hour
   of the most recent failed self-test. Return value is negative if
   this function has a problem (typically -1), otherwise the bottom 8
   bits are the number of failed self tests and the 16 bits above that
   are the poweron hour of the most recent failure. Note: aborted self
   tests (typically by the user) and self tests in progress are not 
   considered failures. See Working Draft SCSI Primary Commands - 3 
   (SPC-3) section 7.2.10 T10/1416-D (rev 22a) */
int scsiCountFailedSelfTests(int fd, int noisy)
{
    int num, k, n, err, res, fails, fail_hour;
    UINT8 * ucp;
    unsigned char resp[LOG_RESP_SELF_TEST_LEN];

    if ((err = scsiLogSense(fd, SELFTEST_RESULTS_LPAGE, resp, 
                            LOG_RESP_SELF_TEST_LEN, 0))) {
        if (noisy)
            pout("scsiCountSelfTests Failed [%s]\n", scsiErrString(err));
        return -1;
    }
    if (resp[0] != SELFTEST_RESULTS_LPAGE) {
        if (noisy)
            pout("Self-test Log Sense Failed, page mismatch\n");
        return -1;
    }
    // compute page length
    num = (resp[2] << 8) + resp[3];
    // Log sense page length 0x190 bytes
    if (num != 0x190) {
        if (noisy)
            pout("Self-test Log Sense length is 0x%x not 0x190 bytes\n", num);
        return -1;
    }
    fails = 0;
    fail_hour = 0;
    // loop through the twenty possible entries
    for (k = 0, ucp = resp + 4; k < 20; ++k, ucp += 20 ) {

        // timestamp in power-on hours (or zero if test in progress)
        n = (ucp[6] << 8) | ucp[7];

        // The spec says "all 20 bytes will be zero if no test" but
        // DG has found otherwise.  So this is a heuristic.
        if ((0 == n) && (0 == ucp[4]))
            break;
        res = ucp[4] & 0xf;
        if ((res > 2) && (res < 8)) {
            fails++;
            if (1 == fails) 
                fail_hour = (ucp[6] << 8) + ucp[7];
        }
    }
    return (fail_hour << 8) + fails;
}

/* Returns 0 if able to read self test log page; then outputs 1 into
   *inProgress if self test still in progress, else outputs 0. */
int scsiSelfTestInProgress(int fd, int * inProgress)
{
    int num;
    UINT8 * ucp;
    unsigned char resp[LOG_RESP_SELF_TEST_LEN];

    if (scsiLogSense(fd, SELFTEST_RESULTS_LPAGE, resp, 
                     LOG_RESP_SELF_TEST_LEN, 0))
        return -1;
    if (resp[0] != SELFTEST_RESULTS_LPAGE)
        return -1;
    // compute page length
    num = (resp[2] << 8) + resp[3];
    // Log sense page length 0x190 bytes
    if (num != 0x190) {
        return -1;
    }
    ucp = resp + 4;
    if (inProgress)
        *inProgress = (0xf == (ucp[4] & 0xf)) ? 1 : 0;
    return 0;
}

/* Returns a negative value if failed to fetch Contol mode page or it was
   malformed. Returns 0 if GLTSD bit is zero and returns 1 if the GLTSD
   bit is set. Examines default mode page when current==0 else examines
   current mode page. */
int scsiFetchControlGLTSD(int device, int modese_len, int current)
{
    int err, offset;
    UINT8 buff[64];
    int pc = current ? MPAGE_CONTROL_CURRENT : MPAGE_CONTROL_DEFAULT;

    memset(buff, 0, sizeof(buff));
    if (modese_len <= 6) {
        if ((err = scsiModeSense(device, CONTROL_MODE_PAGE, pc, 
                                 buff, sizeof(buff)))) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                modese_len = 10;
            else
                return -EINVAL;
        } else if (0 == modese_len)
            modese_len = 6;
    }
    if (10 == modese_len) {
        err = scsiModeSense10(device, CONTROL_MODE_PAGE, pc,
                              buff, sizeof(buff));
        if (err)
            return -EINVAL;
    } 
    offset = scsiModePageOffset(buff, sizeof(buff), modese_len);
    if ((offset >= 0) && (buff[offset + 1] >= 0xa))
        return (buff[offset + 2] & 2) ? 1 : 0;
    return -EINVAL;
}

/* Attempts to set or clear GLTSD bit in Control mode page. If enabled is
   0 attempts to clear GLTSD otherwise it attempts to set it. Returns 0 if
   successful, negative if low level error, > 0 if higher level error (e.g.
   SIMPLE_ERR_BAD_PARAM if GLTSD bit is not changeable). */
int scsiSetControlGLTSD(int device, int enabled, int modese_len)
{
    int err, offset, resp_len, sp;
    UINT8 buff[64];
    UINT8 ch_buff[64];

    memset(buff, 0, sizeof(buff));
    if (modese_len <= 6) {
        if ((err = scsiModeSense(device, CONTROL_MODE_PAGE, 
                                 MPAGE_CONTROL_CURRENT, 
                                 buff, sizeof(buff)))) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                modese_len = 10;
            else
                return err;
        } else if (0 == modese_len)
            modese_len = 6;
    }
    if (10 == modese_len) {
        err = scsiModeSense10(device, CONTROL_MODE_PAGE, 
                              MPAGE_CONTROL_CURRENT, 
                              buff, sizeof(buff));
        if (err)
            return err;
    } 
    offset = scsiModePageOffset(buff, sizeof(buff), modese_len);
    if ((offset < 0) || (buff[offset + 1] < 0xa))
        return SIMPLE_ERR_BAD_RESP;

    if (enabled)
        enabled = 2;
    if (enabled == (buff[offset + 2] & 2))
        return 0;       /* GLTSD already in wanted state so nothing to do */

    if (modese_len == 6)
        err = scsiModeSense(device, CONTROL_MODE_PAGE, 
                            MPAGE_CONTROL_CHANGEABLE, 
                            ch_buff, sizeof(ch_buff));
    else
        err = scsiModeSense10(device, CONTROL_MODE_PAGE, 
                              MPAGE_CONTROL_CHANGEABLE, 
                              ch_buff, sizeof(ch_buff));
    if (err)
        return err;
    if (0 == (ch_buff[offset + 2] & 2))
        return SIMPLE_ERR_BAD_PARAM;  /* GLTSD bit not chageable */
    
    if (10 == modese_len) {
        resp_len = (buff[0] << 8) + buff[1] + 2;
        buff[3] &= 0xef;    /* for disks mask out DPOFUA bit */
    } else {
        resp_len = buff[0] + 1;
        buff[2] &= 0xef;    /* for disks mask out DPOFUA bit */
    }
    sp = (buff[offset] & 0x80) ? 1 : 0; /* PS bit becomes 'SELECT's SP bit */
    if (enabled)
        buff[offset + 2] |= 0x2;    /* set GLTSD bit */
    else
        buff[offset + 2] &= 0xfd;   /* clear GLTSD bit */
    if (10 == modese_len)
        err = scsiModeSelect10(device, sp, buff, resp_len);
    else if (6 == modese_len)
        err = scsiModeSelect(device, sp, buff, resp_len);
    return err;
}

/* Returns a negative value if failed to fetch Protocol specific port mode 
   page or it was malformed. Returns transport protocol identifier when
   value >= 0 . */
int scsiFetchTransportProtocol(int device, int modese_len)
{
    int err, offset;
    UINT8 buff[64];

    memset(buff, 0, sizeof(buff));
    if (modese_len <= 6) {
        if ((err = scsiModeSense(device, PROTOCOL_SPECIFIC_PORT_PAGE, 
                                 MPAGE_CONTROL_CURRENT, 
                                 buff, sizeof(buff)))) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                modese_len = 10;
            else
                return -EINVAL;
        } else if (0 == modese_len)
            modese_len = 6;
    }
    if (10 == modese_len) {
        err = scsiModeSense10(device, PROTOCOL_SPECIFIC_PORT_PAGE, 
                              MPAGE_CONTROL_CURRENT, 
                              buff, sizeof(buff));
        if (err)
            return -EINVAL;
    } 
    offset = scsiModePageOffset(buff, sizeof(buff), modese_len);
    if ((offset >= 0) && (buff[offset + 1] > 1)) {
        if ((0 == (buff[offset] & 0x40)) &&       /* SPF==0 */
            (PROTOCOL_SPECIFIC_PORT_PAGE == (buff[offset] & 0x3f))) 
                return (buff[offset + 2] & 0xf);
    }
    return -EINVAL;
}


/* This is Linux specific code to look for the libata ATA-SCSI
   simulator in the vendor device identification page.
   Returns 1 if found else 0. */
int isLinuxLibAta(unsigned char * vpd_di_buff, int len)
{
    int k, id_len, c_set, assoc, id_type, i_len;
    unsigned char * ucp;
    unsigned char * ip;

    if (len < 4) {
        /* Device identification VPD page length too short */
        return 0;
    }
    len -= 4;
    ucp = vpd_di_buff + 4;
    for (k = 0; k < len; k += id_len, ucp += id_len) {
        i_len = ucp[3];
        id_len = i_len + 4;
        if ((k + id_len) > len) {
            /* short descriptor, badly formed */
            return 0;
        }
        ip = ucp + 4;
        c_set = (ucp[0] & 0xf);
        assoc = ((ucp[1] >> 4) & 0x3);
        id_type = (ucp[1] & 0xf);
        if ((0 == id_type) && (2 == c_set) && (0 == assoc) &&
            (0 == strncmp((const char *)ip,
                          "Linux ATA-SCSI simulator", i_len))) {
            return 1;
        }
    }
    return 0;
}
