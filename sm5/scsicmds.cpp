/*
 * scsicmds.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-3 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 *
 * Additional SCSI work:
 * Copyright (C) 2003 Douglas Gilbert <dougg@torque.net>
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
 * In the SCSI world "SMART" is a dead or withdrawn standard. In recent
 * SCSI standards (since SCSI-3) it goes under the awkward name of
 * "Informational Exceptions" ["IE" or "IEC" (with the "C" for "control")].
 * The relevant information is spread around several SCSI draft
 * standards available at http://www.t10.org . Reference is made in the
 * code to the following acronyms:
 *      - SAM [SCSI Architectural model, versions 2 or 3]
 *      - SPC [SCSI Primary commands, versions 2 or 3]
 *      - SBC [SCSI Block commands, versions 2]
 */

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "scsicmds.h"
#include "utility.h"
#include "extern.h"

const char *scsicmds_c_cvsid="$Id: scsicmds.cpp,v 1.33 2003/04/07 10:58:31 dpgilbert Exp $" SCSICMDS_H_CVSID EXTERN_H_CVSID;

/* for passing global control variables */
extern smartmonctrl *con;

/* output binary in hex and optionally ascii */
static void dStrHex(const char* str, int len, int no_ascii)
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

#if 1   
/* Linux specific code, FreeBSD could conditionally compile in CAM stuff 
 * instead of this. */

/* #include <scsi/scsi.h>       bypass for now */
/* #include <scsi/scsi_ioctl.h> bypass for now */

#define MAX_DXFER_LEN 1024      /* can be increased if necessary */
#define SEND_IOCTL_RESP_SENSE_LEN 16    /* ioctl limitation */
#define DRIVER_SENSE  0x8       /* alternate CHECK CONDITION indication */

#ifndef SCSI_IOCTL_SEND_COMMAND
#define SCSI_IOCTL_SEND_COMMAND 1
#endif
#ifndef SCSI_IOCTL_TEST_UNIT_READY
#define SCSI_IOCTL_TEST_UNIT_READY 2
#endif

struct ioctl_send_command_hack
{
    int inbufsize;
    int outbufsize;
    UINT8 buff[MAX_DXFER_LEN + 16];
};

static int do_scsi_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop)
{
    struct ioctl_send_command_hack wrk;
    int status, buff_offset;
    size_t len;

    memcpy(wrk.buff, iop->cmnd, iop->cmnd_len);
    buff_offset = iop->cmnd_len;
    if (con->reportscsiioctl > 0) {
        int k;
        const unsigned char * ucp = iop->cmnd;
        const char * np;

        np = scsi_get_opcode_name(ucp[0]);
        pout(" [%s: ", np ? np : "<unknown opcode>");
        for (k = 0; k < iop->cmnd_len; ++k)
            pout("%02x ", ucp[k]);
        if ((con->reportscsiioctl > 1) && 
            (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
            int trunc = (iop->dxfer_len > 256) ? 1 : 0;

            pout("]\n  Outgoing data, len=%d%s:\n", iop->dxfer_len,
                 (trunc ? " [only first 256 bytes shown]" : ""));
            dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
        }
        else
            pout("]");
    }
    switch (iop->dxfer_dir) {
        case DXFER_NONE:
            wrk.inbufsize = 0;
            wrk.outbufsize = 0;
            break;
        case DXFER_FROM_DEVICE:
            wrk.inbufsize = 0;
            if (iop->dxfer_len > MAX_DXFER_LEN)
                return -EINVAL;
            wrk.outbufsize = iop->dxfer_len;
            break;
        case DXFER_TO_DEVICE:
            if (iop->dxfer_len > MAX_DXFER_LEN)
                return -EINVAL;
            memcpy(wrk.buff + buff_offset, iop->dxferp, iop->dxfer_len);
            wrk.inbufsize = iop->dxfer_len;
            wrk.outbufsize = 0;
            break;
        default:
            pout("do_scsi_cmnd_io: bad dxfer_dir\n");
            return -EINVAL;
    }
    iop->resp_sense_len = 0;
    iop->scsi_status = 0;
    iop->resid = 0;
    /* The SCSI_IOCTL_SEND_COMMAND ioctl is primitive and it doesn't 
     * support: CDB length (guesses it from opcode), resid and timeout */
    status = ioctl(dev_fd, SCSI_IOCTL_SEND_COMMAND , &wrk);
    if (-1 == status) {
        if (con->reportscsiioctl)
            pout("  status=-1, errno=%d\n", errno);
        return -errno;
    }
    if (0 == status) {
        if (con->reportscsiioctl > 0)
            pout("  status=0\n");
        if (DXFER_FROM_DEVICE == iop->dxfer_dir) {
            memcpy(iop->dxferp, wrk.buff, iop->dxfer_len);
            if (con->reportscsiioctl > 1) {
                int trunc = (iop->dxfer_len > 256) ? 1 : 0;

                pout("  Incoming data, len=%d%s:\n", iop->dxfer_len,
                     (trunc ? " [only first 256 bytes shown]" : ""));
                dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
            }
        }
        return 0;
    }
    iop->scsi_status = status & 0x7e; /* bits 0 and 7 used to be for vendors */
    if (DRIVER_SENSE == ((status >> 24) & 0xff))
        iop->scsi_status = 2;
    len = (SEND_IOCTL_RESP_SENSE_LEN < iop->max_sense_len) ?
                SEND_IOCTL_RESP_SENSE_LEN : iop->max_sense_len;
    if ((SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) && 
        iop->sensep && (len > 0)) {
        memcpy(iop->sensep, wrk.buff, len);
        iop->resp_sense_len = len;
        if (con->reportscsiioctl > 1) {
            pout("  >>> Sense buffer, len=%d:\n", len);
            dStrHex(wrk.buff, len , 1);
        }
    }
    if (con->reportscsiioctl) {
        if (SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) {
            pout("  status=%x: sense_key=%x asc=%x ascq=%x\n", status & 0xff,
                 wrk.buff[2] & 0xf, wrk.buff[12], wrk.buff[13]);
        }
        else
            pout("  status=0x%x\n", status);
    }
    if (iop->scsi_status > 0)
        return 0;
    else
        return -ENODEV;      /* give up, assume no device there */
}
#endif

void scsi_do_sense_disect(const struct scsi_cmnd_io * io_buf,
                          struct scsi_sense_disect * out)
{
    memset(out, 0, sizeof(out));
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

/* Sends LOG SENSE command. Returns 0 if ok, 1 if device NOT READY, 2 if
   command not supported, 3 if field (within command) not supported or
   returns negated errno.  SPC sections 7.6 and 8.2 */
int logsense(int device, int pagenum, UINT8 *pBuf, int bufLen)
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
    cdb[0] = LOG_SENSE;
    cdb[2] = 0x40 | pagenum;  /* Page control (PC)==1 [current cumulative] */
    cdb[7] = (bufLen >> 8) & 0xff;
    cdb[8] = bufLen & 0xff;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);

    status = do_scsi_cmnd_io(device, &io_hdr);
    scsi_do_sense_disect(&io_hdr, &sinfo);
    if (SCSI_SK_NOT_READY == sinfo.sense_key)
        return 1;
    else if (SCSI_SK_ILLEGAL_REQUEST == sinfo.sense_key) {
        if (SCSI_ASC_UNKNOWN_OPCODE == sinfo.asc)
            return 2;
        else if (SCSI_ASC_UNKNOWN_FIELD == sinfo.asc)
            return 3;
    }
    if (status > 0) {
        pout("log sense: status=%x sense_key=%x asc=%x ascq=%x\n",
             status, sinfo.sense_key, sinfo.asc, sinfo.ascq);
        status = -EIO;
    }
    return status;
}

/* Send MODE SENSE (6 byte) command. Returns 0 if ok, 1 if NOT READY,
 * 2 if command not supported (then MODE SENSE(10) should be supported),
 * 3 if field in command not supported or returns negated errno. 
 * SPC sections 7.9 and 8.4 */
int modesense(int device, int pagenum, UINT8 *pBuf, int bufLen)
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
    cdb[2] = pagenum;
    cdb[4] = bufLen;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);

    status = do_scsi_cmnd_io(device, &io_hdr);
    scsi_do_sense_disect(&io_hdr, &sinfo);
    if (SCSI_SK_NOT_READY == sinfo.sense_key)
        return 1;
    else if (SCSI_SK_ILLEGAL_REQUEST == sinfo.sense_key) {
        if (SCSI_ASC_UNKNOWN_OPCODE == sinfo.asc)
            return 2;
        else if (SCSI_ASC_UNKNOWN_FIELD == sinfo.asc)
            return 3;
    }
    if (status > 0) {
        pout("modesense: status=%x sense_key=%x asc=%x ascq=%x\n",
             status, sinfo.sense_key, sinfo.asc, sinfo.ascq);
        status = -EIO;
    }
    return status;
}

/* Sends a 6 byte MODE SELECT command. Assumes given pBuf is the response
 * from a corresponding 6 byte MODE SENSE command. Such a response should
 * have a 4 byte header followed by 0 or more 8 byte block descriptors
 * (normally 1) and then 1 mode page. Returns 0 if ok, 1 if NOT READY,
 * 2 if command not supported (then MODE SELECT(10) may be supported), 
 * 3 if field in command not supported, 4 if bad parameter to command
 * or returns negated errno. SPC sections 7.7 and 8.4 */
int modeselect(int device, int pagenum, UINT8 *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    UINT8 cdb[6];
    UINT8 sense[32];
    int status, pg_offset, pg_len, hdr_plus_1_pg;
    int sense_len = pBuf[0] + 1;

    if (sense_len < 4)
        return -EINVAL;
    pg_offset = 4 + pBuf[3];
    if (pg_offset + 2 >= bufLen)
        return -EINVAL;
    pg_len = pBuf[pg_offset + 1] + 2;
    hdr_plus_1_pg = pg_offset + pg_len;
    if ((hdr_plus_1_pg > bufLen) || (hdr_plus_1_pg > sense_len))
        return -EINVAL;
    pBuf[0] = 0;    /* Length of returned mode sense data reserved for SELECT */
    pBuf[pg_offset] &= 0x3f;    /* Mask of PS bit from byte 0 of page data */
    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_TO_DEVICE;
    io_hdr.dxfer_len = hdr_plus_1_pg;
    io_hdr.dxferp = pBuf;
    cdb[0] = MODE_SELECT;
    cdb[1] = 0x11;      /* set PF and SP bits */
    cdb[4] = hdr_plus_1_pg; /* make sure only one page sent */
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);

    status = do_scsi_cmnd_io(device, &io_hdr);
    scsi_do_sense_disect(&io_hdr, &sinfo);
    if (SCSI_SK_NOT_READY == sinfo.sense_key)
        return 1;
    else if (SCSI_SK_ILLEGAL_REQUEST == sinfo.sense_key) {
        if (SCSI_ASC_UNKNOWN_OPCODE == sinfo.asc)
            return 2;
        else if (SCSI_ASC_UNKNOWN_FIELD == sinfo.asc)
            return 3;
        else if (SCSI_ASC_UNKNOWN_PARAM == sinfo.asc)
            return 4;
    }
    if (status > 0) {
        pout("modeselect: status=%x sense_key=%x asc=%x ascq=%x\n",
             status, sinfo.sense_key, sinfo.asc, sinfo.ascq);
        status = -EIO;
    }
    return status;
}

/* MODE SENSE (10 byte). Returns 0 if ok, 1 if NOT READY, 2 if command 
 * not supported (then MODE SENSE(6) might be supported), 3 if field in
 * command not supported or returns negated errno.  
 * SPC sections 7.10 and 8.4 */
int modesense10(int device, int pagenum, UINT8 *pBuf, int bufLen)
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
    cdb[0] = MODE_SENSE;
    cdb[2] = pagenum;
    cdb[7] = (bufLen >> 8) & 0xff;
    cdb[8] = bufLen & 0xff;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);

    status = do_scsi_cmnd_io(device, &io_hdr);
    scsi_do_sense_disect(&io_hdr, &sinfo);
    if (SCSI_SK_NOT_READY == sinfo.sense_key)
        return 1;
    else if (SCSI_SK_ILLEGAL_REQUEST == sinfo.sense_key) {
        if (SCSI_ASC_UNKNOWN_OPCODE == sinfo.asc)
            return 2;
        else if (SCSI_ASC_UNKNOWN_FIELD == sinfo.asc)
            return 3;
    }
    if (status > 0) {
        pout("modesense10: status=%x sense_key=%x asc=%x ascq=%x\n",
             status, sinfo.sense_key, sinfo.asc, sinfo.ascq);
        status = -EIO;
    }
    return status;
}

/* Sends a 10 byte MODE SELECT command. Assumes given pBuf is the response
 * from a corresponding 10 byte MODE SENSE command. Such a response should
 * have a 8 byte header followed by 0 or more 8 byte block descriptors
 * (normally 1) and then 1 mode page. Returns 0 if ok, 1 NOT REAFY, 2 if 
 * command not supported (then MODE SELECT(6) may be supported), 3 if field
 * in command not supported, 4 if bad parameter to command or returns
 * negated errno. SAM sections 7.8 and 8.4 */
int modeselect10(int device, int pagenum, UINT8 *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    UINT8 cdb[10];
    UINT8 sense[32];
    int status, pg_offset, pg_len, hdr_plus_1_pg;
    int sense_len = (pBuf[0] << 8) + pBuf[1] + 2;

    if (sense_len < 4)
        return -EINVAL;
    pg_offset = 8 + (pBuf[6] << 8) + pBuf[7];
    if (pg_offset + 2 >= bufLen)
        return -EINVAL;
    pg_len = pBuf[pg_offset + 1] + 2;
    hdr_plus_1_pg = pg_offset + pg_len;
    if ((hdr_plus_1_pg > bufLen) || (hdr_plus_1_pg > sense_len))
        return -EINVAL;
    pBuf[0] = 0;    
    pBuf[1] = 0; /* Length of returned mode sense data reserved for SELECT */
    pBuf[pg_offset] &= 0x3f;    /* Mask of PS bit from byte 0 of page data */
    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_TO_DEVICE;
    io_hdr.dxfer_len = hdr_plus_1_pg;
    io_hdr.dxferp = pBuf;
    cdb[0] = MODE_SELECT_10;
    cdb[2] = 0x11;      /* set PF and SP bits */
    cdb[8] = hdr_plus_1_pg; /* make sure only one page sent */
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);

    status = do_scsi_cmnd_io(device, &io_hdr);
    scsi_do_sense_disect(&io_hdr, &sinfo);
    if (SCSI_SK_NOT_READY == sinfo.sense_key)
        return 1;
    else if (SCSI_SK_ILLEGAL_REQUEST == sinfo.sense_key) {
        if (SCSI_ASC_UNKNOWN_OPCODE == sinfo.asc)
            return 2;
        else if (SCSI_ASC_UNKNOWN_FIELD == sinfo.asc)
            return 3;
        else if (SCSI_ASC_UNKNOWN_PARAM == sinfo.asc)
            return 4;
    }
    if (status > 0) {
        pout("modeselect10: status=%x sense_key=%x asc=%x ascq=%x\n",
             status, sinfo.sense_key, sinfo.asc, sinfo.ascq);
        status = -EIO;
    }
    return status;
}

/* Standard INQUIRY returns 0 for ok, anything else is a major problem.
 * bufLen should be 36 for unsafe devices (like USB mass storage stuff)
 * otherwise they can lock up! SPC sections 7.4 and 8.6 */
int stdinquiry(int device, UINT8 *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr;
    UINT8 cdb[6];
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
    status = do_scsi_cmnd_io(device, &io_hdr);
    return status;
}

/* INQUIRY to fetch Vital Page Data.  Returns 0 if ok, 1 if NOT READY
 * (unlikely), 2 if command not supported, 3 if field in command not 
 * supported or returns negated errno. SPC section 7.4 and 8.6 */
int inquiry_vpd(int device, int vpd_page, UINT8 *pBuf, int bufLen)
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
    cdb[0] = INQUIRY;
    cdb[1] = 0x1;       /* set EVPD bit (enable Vital Product Data) */
    cdb[2] = vpd_page;
    cdb[4] = bufLen;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);

    status = do_scsi_cmnd_io(device, &io_hdr);
    scsi_do_sense_disect(&io_hdr, &sinfo);
    if (SCSI_SK_NOT_READY == sinfo.sense_key)
        return 1;
    else if (SCSI_SK_ILLEGAL_REQUEST == sinfo.sense_key) {
        if (SCSI_ASC_UNKNOWN_OPCODE == sinfo.asc)
            return 2;
        else if (SCSI_ASC_UNKNOWN_FIELD == sinfo.asc)
            return 3;
    }
    if (status > 0) {
        pout("inquiry_vpd: status=%x sense_key=%x asc=%x ascq=%x\n",
             status, sinfo.sense_key, sinfo.asc, sinfo.ascq);
        status = -EIO;
    }
    return status;
}

/* REQUEST SENSE command. Returns 0 if ok, anything else major problem.
 * SPC section 7.24 */
int requestsense(int device, struct scsi_sense_disect * sense_info)
{
    struct scsi_cmnd_io io_hdr;
    UINT8 cdb[6];
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
    status = do_scsi_cmnd_io(device, &io_hdr);
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
 * errno. SPC section 7.25 */
int senddiagnostic(int device, int functioncode, UINT8 *pBuf, int bufLen)
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
        cdb[1] = 0x14;  /* PF and SelfTest bit */
    else if (SCSI_DIAG_NO_SELF_TEST != functioncode)
        cdb[1] = (functioncode << 5 ) | 0x10; /* SelfTest _code_ + PF bit */
    else   /* SCSI_DIAG_NO_SELF_TEST == functioncode */
        cdb[1] = 0x10;  /* PF bit */
    cdb[3] = (bufLen >> 8) & 0xff;
    cdb[4] = bufLen & 0xff;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = 5 * 60 * 60;   /* five hours because a foreground 
                    extended self tests can take 1 hour plus */
    
    status = do_scsi_cmnd_io(device, &io_hdr);
    scsi_do_sense_disect(&io_hdr, &sinfo);
    if (SCSI_SK_NOT_READY == sinfo.sense_key)
        return 1;
    else if (SCSI_SK_ILLEGAL_REQUEST == sinfo.sense_key) {
        if (SCSI_ASC_UNKNOWN_OPCODE == sinfo.asc)
            return 2;
        else if (SCSI_ASC_UNKNOWN_FIELD == sinfo.asc)
            return 3;
    }
    if (status > 0) {
        pout("senddiagnostic: status=%x sense_key=%x asc=%x ascq=%x\n",
             status, sinfo.sense_key, sinfo.asc, sinfo.ascq);
        status = -EIO;
    }
    return status;
}

/* RECEIVE DIAGNOSTIC command. Returns 0 if ok, 1 if NOT READY, 2 if
 * command not supported, 3 if field in command not supported or returns
 * negated errno. SPC section 7.17 */
int receivediagnostic(int device, int pcv, int pagenum, UINT8 *pBuf, 
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

    status = do_scsi_cmnd_io(device, &io_hdr);
    scsi_do_sense_disect(&io_hdr, &sinfo);
    if (SCSI_SK_NOT_READY == sinfo.sense_key)
        return 1;
    else if (SCSI_SK_ILLEGAL_REQUEST == sinfo.sense_key) {
        if (SCSI_ASC_UNKNOWN_OPCODE == sinfo.asc)
            return 2;
        else if (SCSI_ASC_UNKNOWN_FIELD == sinfo.asc)
            return 3;
    }
    if (status > 0) {
        pout("receivediagnostic: status=%x sense_key=%x asc=%x ascq=%x\n",
             status, sinfo.sense_key, sinfo.asc, sinfo.ascq);
        status = -EIO;
    }
    return status;
}

/* TEST UNIT READY command. SPC section 7.28 (probably in SBC as well) */
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

    status = do_scsi_cmnd_io(device, &io_hdr);
    scsi_do_sense_disect(&io_hdr, sinfo);
    return status;
}

/* Returns 0 for device responds and media ready, 1 for device responds and
   media not ready, or returns a negated errno value */
int scsiTestUnitReady(int device)
{
    struct scsi_sense_disect sinfo;
    int status;

    status = _testunitready(device, &sinfo);
    if (SCSI_SK_NOT_READY == sinfo.sense_key)
        return 1;
    else if (SCSI_SK_UNIT_ATTENTION == sinfo.sense_key) {
        /* power on reset, media changed, ok ... try again */
        status = _testunitready(device, &sinfo);        
        if (SCSI_SK_NOT_READY == sinfo.sense_key)
            return 1;
    }
    return status;
}

/* ModePage1C Handler */
#define FETCH_IEC_2BYTE   0x00    
#define DEXCPT_DISABLE  0xf7
#define DEXCPT_ENABLE   0x08
#define EWASC_ENABLE    0x10
#define EWASC_DISABLE   0xef

/* Mode page 0x1c is the "Imformation Exception Control" (IEC) page */
static int scsiModePage1CHandler(int device, UINT8 setting, 
                                 struct scsi_iec_mode_page *iecp)
{
    UINT8 tBuf[64];
    int len, bd_len, err;
    UINT8 *p; 
        
    if ((err = modesense(device, INFORMATIONAL_EXCEPTIONS_CONTROL, 
                         tBuf, sizeof(tBuf))))
        return err;
    len = tBuf[0] + 1;
    bd_len = tBuf[3];
    p = tBuf + 4 + bd_len;
    len = len - bd_len - 4;
    if ((INFORMATIONAL_EXCEPTIONS_CONTROL != (p[0] & 0x3f)) ||
        ((4 + bd_len + p[1]) > sizeof(tBuf)))  {
        if (con->reportscsiioctl) {
            pout("1CHandler: malformed IEC page\n");
            return 10;
       }
    }
    if (con->reportscsiioctl)
        pout("1CHandler: IEC mode page [2]=0x%x [3]=0x%x", p[2], p[3]);

    switch (setting) {
        case DEXCPT_DISABLE:    /* double negative :-) */
            p[2] &= DEXCPT_DISABLE;
            p[3] = 0x04;     /* mrie: unconditional generate recovered err */
#if 0
            /* >>>> try setting TEST bit */ p[2] |= 0x4;
            p[3] = 0x06;    /* mrie=='on request' */
#endif
            break;
        case DEXCPT_ENABLE:
            p[2] |= DEXCPT_ENABLE;
            /* >>>> turn off TEST p[2] &= (~0x04) & 0xff; */
            break;
        case EWASC_ENABLE:
            p[2] |= EWASC_ENABLE;
            break;
        case EWASC_DISABLE:
            p[2] &= EWASC_DISABLE;
            break;
        case FETCH_IEC_2BYTE:
            if (iecp) {
                iecp->byte_2 = p[2];
                iecp->mrie = p[3] & 0xf;
                iecp->interval_timer = ((p[4] << 24) & 0xff) +
                                       ((p[5] << 16) & 0xff) +
                                       ((p[6] << 8) & 0xff) +
                                       (p[7] & 0xff);
                iecp->report_count = ((p[8] << 24) & 0xff) +
                                     ((p[9] << 16) & 0xff) +
                                     ((p[10] << 8) & 0xff) +
                                     (p[11] & 0xff);
            }
            if (con->reportscsiioctl)
                pout("\n");
            return 0;
        default:
            if (con->reportscsiioctl)
                pout("\n");
            return 1;
    }
                        
    if (con->reportscsiioctl)
        pout(" -> [2']=0x%x [3']=0x%x\n", p[2], p[3]);
    if (modeselect(device, 0x1c, tBuf, sizeof(tBuf)))
        return 1;
#if 0
    // trial to see if TEST bit in mode pg 0x1c + mrie==6 work
    // They do on my disks
    {
        struct scsi_sense_disect sinfo;

        if (0 == requestsense(device, &sinfo))
            pout("on request: ecode=%x, key=%x, asc=%x, ascq=%x\n",
                 sinfo.error_code, sinfo.sense_key, sinfo.asc, sinfo.ascq);
        if (0 == requestsense(device, &sinfo))
            pout("on request: ecode=%x, key=%x, asc=%x, ascq=%x\n",
                 sinfo.error_code, sinfo.sense_key, sinfo.asc, sinfo.ascq);
    }
#endif
    return 0;
}

int scsiFetchIECmpage(int device, struct scsi_iec_mode_page *iecp)
{
    return scsiModePage1CHandler(device, FETCH_IEC_2BYTE, iecp);
}

int scsiSetExceptionControl(int device, int enabled,
                            const struct scsi_iec_mode_page *iecp)
{
    if (iecp) {
        if (enabled == scsi_IsExceptionControlEnabled(iecp))
            return 0;   /* already done */
    }
    return scsiModePage1CHandler(device, 
                        (enabled ? DEXCPT_DISABLE : DEXCPT_ENABLE), NULL);
}

int scsiSetWarning(int device, int enabled,
                   const struct scsi_iec_mode_page *iecp)
{
    if (iecp) {
        if (enabled == scsi_IsWarningEnabled(iecp))
            return 0;   /* already done */
    }
    return scsiModePage1CHandler(device, 
                        (enabled ? EWASC_ENABLE : EWASC_DISABLE), NULL);
}

int scsi_IsExceptionControlEnabled(const struct scsi_iec_mode_page *iecp)
{
    if (iecp)
        return (iecp->byte_2 & DEXCPT_ENABLE) ? 0 : 1;
    else
        return 0;
}

int scsi_IsWarningEnabled(const struct scsi_iec_mode_page *iecp)
{
    if (iecp)
        return (iecp->byte_2 & EWASC_ENABLE) ? 1 : 0;
    else
        return 0;
}

int scsiGetTemp(int device, UINT8 *currenttemp, UINT8 *triptemp)
{
    UINT8 tBuf[1024];
    int err;

    if ((err = logsense(device, TEMPERATURE_PAGE, tBuf, sizeof(tBuf)))) {
        *currenttemp = 0;
        *triptemp = 0;
        pout("Log Sense failed, err=%d\n", err);
        return 1;
    }
    *currenttemp = tBuf[9];
    *triptemp = tBuf[15];
    return 0;
}

/* Read informational exception log page or Request Sense response */
int scsiCheckIE(int device, UINT8 method, UINT8 *asc, UINT8 *ascq,
                UINT8 *currenttemp)
{
    UINT8 tBuf[1024];
    struct scsi_sense_disect sense_info;
    int err;
    unsigned short pagesize;
 
    *asc = 0;
    *ascq = 0;
    *currenttemp = 0;
    memset(&sense_info, 0, sizeof(sense_info));
    if (method == CHECK_SMART_BY_LGPG_2F) {
        if ((err = logsense(device, IE_LOG_PAGE, tBuf, sizeof(tBuf)))) {
            pout("Log Sense failed, IE page, err=%d\n", err);
            return 1;
        }
        pagesize = (unsigned short) (tBuf[2] << 8) | tBuf[3];
        if ((pagesize < 4) || tBuf[4] || tBuf[5]) {
            pout("Log Sense failed, IE page, bad parameter code or length\n");
            return 2;
        }
        if (tBuf[7] > 1) {
            sense_info.asc = tBuf[8]; 
            sense_info.ascq = tBuf[9];
            if (tBuf[7] > 2) 
                *currenttemp = tBuf[10];
        } 
    } else {
        if ((err = requestsense(device, &sense_info))) {
            pout("Request Sense failed, err=%d\n", err);
            return 1;
        }
    }
    *asc = sense_info.asc;
    *ascq = sense_info.ascq;
    return 0;
}

static const char * TapeAlertsMessageTable[]= {  
    " ",
   "The tape drive is having problems reading data. No data has been lost, "
       "but there has been a reduction in the performance of the tape.",
   "The tape drive is having problems writing data. No data has been lost, "
       "but there has been a reduction in the performance of the tape.",
   "The operation has stopped because an error has occurred while reading "
       "or writing data which the drive cannot correct.",
   "Your data is at risk:\n1. Copy any data you require from this tape. \n"
       "2. Do not use this tape again.\n"
       "3. Restart the operation with a different tape.",
   "The tape is damaged or the drive is faulty. Call the tape drive "
       "supplier helpline.",
   "The tape is from a faulty batch or the tape drive is faulty:\n"
       "1. Use a good tape to test the drive.\n"
       "2. If problem persists, call the tape drive supplier helpline.",
   "The tape cartridge has reached the end of its calculated useful life: \n"
       "1. Copy data you need to another tape.\n"
       "2. Discard the old tape.",
   "The tape cartridge is not data-grade. Any data you back up to the tape "
       "is at risk. Replace the cartridge with a data-grade tape.",
   "You are trying to write to a write-protected cartridge. Remove the "
       "write-protection or use another tape.",
   "You cannot eject the cartridge because the tape drive is in use. Wait "
       "until the operation is complete before ejecting the cartridge.",
   "The tape in the drive is a cleaning cartridge.",
   "You have tried to load a cartridge of a type which is not supported "
       "by this drive.",
   "The operation has failed because the tape in the drive has snapped:\n"
       "1. Discard the old tape.\n"
       "2. Restart the operation with a different tape.",
   "The operation has failed because the tape in the drive has snapped:\n"
       "1. Do not attempt to extract the tape cartridge\n"
       "2. Call the tape drive supplier helpline.",
   "The memory in the tape cartridge has failed, which reduces performance. "
       "Do not use the cartridge for further backup operations.",
   "The operation has failed because the tape cartridge was manually "
       "ejected while the tape drive was actively writing or reading.",
   "You have loaded of a type that is read-only in this drive. The "
       "cartridge will appear as write-protected.",
   "The directory on the tape cartridge has been corrupted. File search "
       "performance will be degraded. The tape directory can be rebuilt "
       "by reading all the data on the cartridge.",
   "The tape cartridge is nearing the end of its calculated life. It is "
       "recommended that you:\n"
       "1. Use another tape cartridge for your next backup.\n"
       "2. Store this tape in a safe place in case you need to restore "
       "data from it.",
   "The tape drive needs cleaning:\n"
       "1. If the operation has stopped, eject the tape and clean the drive.\n"
       "2. If the operation has not stopped, wait for it to finish and then "
       "clean the drive. Check the tape drive users manual for device "
       "specific cleaning instructions.",
   "The tape drive is due for routine cleaning:\n"
       "1. Wait for the current operation to finish.\n"
       "2. The use a cleaning cartridge. Check the tape drive users manual "
       "for device specific cleaning instructions.",
   "The last cleaning cartridge used in the tape drive has worn out:\n"
       "1. Discard the worn out cleaning cartridge.\n"
       "2. Wait for the current operation to finish.\n"
       "3. Then use a new cleaning cartridge.",
   "The last cleaning cartridge used in the tape drive was an invalid type:\n"
       "1. Do not use this cleaning cartridge in this drive.\n"
       "2. Wait for the current operation to finish.\n"
       "3. Then use a new cleaning cartridge.",
   "The tape drive has requested a retention operation",
   "A redundant interface port on the tape drive has failed",
   "A tape drive cooling fan has failed",
   "A redundant power supply has failed inside the tape drive enclosure. "
       "Check the enclosure users manual for instructions on replacing the "
       "failed power supply.",
   "The tape drive power consumption is outside the specified range.",
   "Preventive maintenance of the tape drive is required. Check the tape "
       "drive users manual for device specific preventive maintenance "
       "tasks or call the tape drive supplier helpline.",
   "The tape drive has a hardware fault:\n"
       "1. Eject the tape or magazine.\n"
       "2. Reset the drive.\n"
       "3. Restart the operation.",
   "The tape drive has a hardware fault:\n"
       "1. Turn the tape drive off and then on again.\n"
       "2. Restart the operation.\n"
       "3. If the problem persists, call the tape drive supplier helpline.\n"
       " Check the tape drive users manual for device specific instructions "
       "on turning the device power in and off.",
   "The tape drive has a problem with the host interface:\n"
       "1. Check the cables and cable connections.\n"
       "2. Restart the operation.",
   "The operation has failed:\n"
       "1. Eject the tape or magazine.\n"
       "2. Insert the tape or magazine again.\n"
       "3. Restart the operation.",
   "The firmware download has failed because you have tried to use the "
       "incorrect firmware for this tape drive. Obtain the correct "
       "firmware and try again.",
   "Environmental conditions inside the tape drive are outside the "
       "specified humidity range.",
   "Environmental conditions inside the tape drive are outside the "
       "specified temperature range.",
   "The voltage supply to the tape drive is outside the specified range.",
   "A hardware failure of the tape drive is predicted. Call the tape "
       "drive supplier helpline.",
   "The tape drive may have a fault. Check for availability of diagnostic "
       "information and run extended diagnostics if applicable. Check the "
       "tape drive users manual for instruction on running extended "
       "diagnostic tests and retrieving diagnostic data",
   "The changer mechanism is having difficulty communicating with the tape "
       "drive:\n"
       "1. Turn the autoloader off then on.\n"
       "2. Restart the operation.\n"
       "3. If problem persists, call the tape drive supplier helpline.",
   "A tape has been left in the autoloader by a previous hardware fault:\n"
       "1. Insert an empty magazine to clear the fault.\n"
       "2. If the fault does not clear, turn the autoloader off and then "
       "on again.\n"
       "3. If the problem persists, call the tape drive supplier helpline.",
   "There is a problem with the autoloader mechanism.",
   "The operation has failed because the autoloader door is open:\n"
       "1. Clear any obstructions from the autoloader door.\n"
       "2. Eject the magazine and then insert it again.\n"
       "3. If the fault does not clear, turn the autoloader off and then "
       "on again.\n"
       "4. If the problem persists, call the tape drive supplier helpline.",
   "The autoloader has a hardware fault:\n"
       "1. Turn the autoloader off and then on again.\n"
       "2. Restart the operation.\n"
       "3. If the problem persists, call the tape drive supplier helpline.\n"
       " Check the autoloader users manual for device specific instructions "
       "on turning the device power on and off.",
   "The autoloader cannot operate without the magazine,\n"
       "1. Insert the magazine into the autoloader.\n"
       "2. Restart the operation.",
   "A hardware failure of the changer mechanism is predicted. Call the "
       "tape drive supplier helpline.",
   " ",
   " ",
   " ",
   "Media statistics have been lost at some time in the past",
   "The tape directory on the tape cartridge just unloaded has been "
       "corrupted. File search performance will be degraded. The tape "
       "directory can be rebuilt by reading all the data.",
   "The tape just unloaded could not write its system area successfully:\n"
       "1. Copy data to another tape cartridge.\n"
       "2. Discard the old cartridge.",
   "The tape system are could not be read successfully at load time:\n"
       "1. Copy data to another tape cartridge.\n"
       "2. Discard the old cartridge.",
   "The start or data could not be found on the tape:\n"
       "1. Check you are using the correct format tape.\n"
       "2. Discard the tape or return the tape to you supplier",
    };

const char * scsiTapeAlertsTapeDevice(unsigned short code)
{
#define NUMENTRIESINTAPEALERTSTABLE 54
    return (code > NUMENTRIESINTAPEALERTSTABLE) ? "Unknown Alert" : 
                                        TapeAlertsMessageTable[code];
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
    return senddiagnostic(device, SCSI_DIAG_NO_SELF_TEST, tBuf, 8);
}

int scsiSmartDefaultSelfTest(int device)
{       
    return senddiagnostic(device, SCSI_DIAG_DEF_SELF_TEST, NULL, 0);
}

int scsiSmartShortSelfTest(int device)
{       
    return senddiagnostic(device, SCSI_DIAG_BG_SHORT_SELF_TEST, NULL, 0);
}

int scsiSmartExtendSelfTest(int device)
{       
    return senddiagnostic(device, SCSI_DIAG_BG_EXTENDED_SELF_TEST, NULL, 0);
}

int scsiSmartShortCapSelfTest(int device)
{       
    return senddiagnostic(device, SCSI_DIAG_FG_SHORT_SELF_TEST, NULL, 0);
}

int scsiSmartExtendCapSelfTest(int device)
{
    return senddiagnostic(device, SCSI_DIAG_FG_EXTENDED_SELF_TEST, NULL, 0);
}

int scsiSmartSelfTestAbort(int device)
{
    return senddiagnostic(device, SCSI_DIAG_ABORT_SELF_TEST, NULL, 0);
}
