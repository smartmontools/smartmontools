/*
 * scsiata.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2006 Douglas Gilbert <dougg@torque.net>
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
 * The code in this file is based on the SCSI to ATA Translation (SAT)
 * draft found at http://www.t10.org . The original draft used for this
 * code is sat-r08.pdf which is not too far away from becoming a
 * standard. The SAT commands of interest to smartmontools are the
 * ATA PASS THROUGH SCSI (16) and ATA PASS THROUGH SCSI (12) defined in
 * section 12 of that document.
 *
 * With more transports "hiding" SATA disks (and other S-ATAPI devices)
 * behind a SCSI command set, accessing special features like SMART
 * information becomes a challenge. The SAT standard offers ATA PASS
 * THROUGH commands for special usages. Note that the SAT layer may
 * be inside a generic OS layer (e.g. libata in linux), in a host
 * adapter (HA or HBA) firmware, or somewhere on the interconnect
 * between the host computer and the SATA devices (e.g. a RAID made
 * of SATA disks and the RAID talks "SCSI" to the host computer).
 * 
 */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "int64.h"
#include "extern.h"
#include "scsicmds.h"
#include "scsiata.h"
#include "utility.h"

const char *scsiata_c_cvsid="$Id: scsiata.c,v 1.4 2006/06/10 16:30:57 ballen4705 Exp $"
CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID SCSICMDS_H_CVSID SCSIATA_H_CVSID UTILITY_H_CVSID;

/* for passing global control variables */
extern smartmonctrl *con;

#define DEF_SAT_ATA_PASSTHRU_SIZE 16


// cdb[0]: ATA PASS THROUGH (16) SCSI command opcode byte (0x85)
// cdb[1]: multiple_count, protocol + extend
// cdb[2]: offline, ck_cond, t_dir, byte_block + t_length
// cdb[3]: features (15:8)
// cdb[4]: features (7:0)
// cdb[5]: sector_count (15:8)
// cdb[6]: sector_count (7:0)
// cdb[7]: lba_low (15:8)
// cdb[8]: lba_low (7:0)
// cdb[9]: lba_mid (15:8)
// cdb[10]: lba_mid (7:0)
// cdb[11]: lba_high (15:8)
// cdb[12]: lba_high (7:0)
// cdb[13]: device
// cdb[14]: (ata) command
// cdb[15]: control (SCSI, leave as zero)
//
// 24 bit lba (from MSB): cdb[12] cdb[10] cdb[8]
// 48 bit lba (from MSB): cdb[11] cdb[9] cdb[7] cdb[12] cdb[10] cdb[8]
//
//
// cdb[0]: ATA PASS THROUGH (12) SCSI command opcode byte (0xa1)
// cdb[1]: multiple_count, protocol + extend
// cdb[2]: offline, ck_cond, t_dir, byte_block + t_length
// cdb[3]: features (7:0)
// cdb[4]: sector_count (7:0)
// cdb[5]: lba_low (7:0)
// cdb[6]: lba_mid (7:0)
// cdb[7]: lba_high (7:0)
// cdb[8]: device
// cdb[9]: (ata) command
// cdb[10]: reserved
// cdb[11]: control (SCSI, leave as zero)
//
//
// ATA status return descriptor (component of descriptor sense data)
// des[0]: descriptor code (0x9)
// des[1]: additional descriptor length (0xc)
// des[2]: extend (bit 0)
// des[3]: error
// des[4]: sector_count (15:8)
// des[5]: sector_count (7:0)
// des[6]: lba_low (15:8)
// des[7]: lba_low (7:0)
// des[8]: lba_mid (15:8)
// des[9]: lba_mid (7:0)
// des[10]: lba_high (15:8)
// des[11]: lba_high (7:0)
// des[12]: device
// des[13]: status



// PURPOSE
//   This is an interface routine meant to isolate the OS dependent
//   parts of the code, and to provide a debugging interface.  Each
//   different port and OS needs to provide it's own interface.  This
//   is the linux one.
// DETAILED DESCRIPTION OF ARGUMENTS
//   device: is the file descriptor provided by open()
//   command: defines the different operations.
//   select: additional input data if needed (which log, which type of
//           self-test).
//   data:   location to write output data, if needed (512 bytes).
//   Note: not all commands use all arguments.
// RETURN VALUES
//  -1 if the command failed
//   0 if the command succeeded,
//   STATUS_CHECK routine: 
//  -1 if the command failed
//   0 if the command succeeded and disk SMART status is "OK"
//   1 if the command succeeded and disk SMART status is "FAILING"

int sat_command_interface(int device, smart_command_set command, int select,
                          char *data)
{
    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    struct sg_scsi_sense_hdr ssh;
    unsigned char cdb[SAT_ATA_PASSTHROUGH_16LEN];
    unsigned char sense[32];
    const unsigned char * ucp;
    int status, len;
    int copydata = 0;
    int outlen = 0;
    int extend = 0;
    int chk_cond = 0;   /* set to 1 to read register(s) back */
    int protocol = 3;   /* non-data */
    int t_dir = 1;      /* 0 -> to device, 1 -> from device */
    int byte_block = 1; /* 0 -> bytes, 1 -> 512 byte blocks */
    int t_length = 0;   /* 0 -> no data transferred */
    int feature = 0;
    int ata_command = 0;
    int sector_count = 0;
    int lba_low = 0;
    int lba_mid = 0;
    int lba_high = 0;
    int passthru_size = DEF_SAT_ATA_PASSTHRU_SIZE;

    memset(cdb, 0, sizeof(cdb));
    memset(sense, 0, sizeof(sense));

    ata_command = ATA_SMART_CMD;
    switch (command) {
    case CHECK_POWER_MODE:
        ata_command = ATA_CHECK_POWER_MODE;
        chk_cond = 1;
        copydata = 1;
        break;
    case READ_VALUES:           /* READ DATA */
        feature = ATA_SMART_READ_VALUES;
        sector_count = 1;     /* one (512 byte) block */
        protocol = 4;   /* PIO data-in */
        t_length = 2;   /* sector count holds count */
        copydata = 512;
        break;
    case READ_THRESHOLDS:       /* obsolete */
        feature = ATA_SMART_READ_THRESHOLDS;
        sector_count = 1;     /* one (512 byte) block */
        lba_low = 1;
        protocol = 4;   /* PIO data-in */
        t_length = 2;   /* sector count holds count */
        copydata=512;
        break;
    case READ_LOG:
        feature = ATA_SMART_READ_LOG_SECTOR;
        sector_count = 1;     /* one (512 byte) block */
        lba_low = select;
        protocol = 4;   /* PIO data-in */
        t_length = 2;   /* sector count holds count */
        copydata = 512;
        break;
    case WRITE_LOG:
        feature = ATA_SMART_WRITE_LOG_SECTOR;
        sector_count = 1;     /* one (512 byte) block */
        lba_low = select;
        protocol = 5;   /* PIO data-out */
        t_length = 2;   /* sector count holds count */
        t_dir = 0;      /* to device */
        outlen = 512;
        break;
    case IDENTIFY:
        ata_command = ATA_IDENTIFY_DEVICE;
        sector_count = 1;     /* one (512 byte) block */
        protocol = 4;   /* PIO data-in */
        t_length = 2;   /* sector count holds count */
        copydata = 512;
        break;
    case PIDENTIFY:
        ata_command = ATA_IDENTIFY_PACKET_DEVICE;
        sector_count = 1;     /* one (512 byte) block */
        protocol = 4;   /* PIO data-in */
        t_length = 2;   /* sector count (7:0) holds count */
        copydata = 512;
        break;
    case ENABLE:
        feature = ATA_SMART_ENABLE;
        lba_low = 1;
        break;
    case DISABLE:
        feature = ATA_SMART_DISABLE;
        lba_low = 1;
        break;
    case STATUS:
        // this command only says if SMART is working.  It could be
        // replaced with STATUS_CHECK below.
        feature = ATA_SMART_STATUS;
        chk_cond = 1;
        break;
    case AUTO_OFFLINE:
        feature = ATA_SMART_AUTO_OFFLINE;
        sector_count = select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
        break;
    case AUTOSAVE:
        feature = ATA_SMART_AUTOSAVE;
        sector_count = select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
        break;
    case IMMEDIATE_OFFLINE:
        feature = ATA_SMART_IMMEDIATE_OFFLINE;
        lba_low = select;
        break;
    case STATUS_CHECK:
        // This command uses HDIO_DRIVE_TASK and has different syntax than
        // the other commands.
        feature = ATA_SMART_STATUS;      /* SMART RETURN STATUS */
        chk_cond = 1;
        break;
    default:
        pout("Unrecognized command %d in sat_command_interface()\n"
             "Please contact " PACKAGE_BUGREPORT "\n", command);
        errno=ENOSYS;
        return -1;
    }
    if (ATA_SMART_CMD == ata_command) {
        lba_mid = 0x4f;
        lba_high = 0xc2;
    }

    if ((SAT_ATA_PASSTHROUGH_12LEN == con->satpassthrulen) ||
        (SAT_ATA_PASSTHROUGH_16LEN == con->satpassthrulen))
        passthru_size = con->satpassthrulen;
    cdb[0] = (SAT_ATA_PASSTHROUGH_12LEN == passthru_size) ?
             SAT_ATA_PASSTHROUGH_12 : SAT_ATA_PASSTHROUGH_16;

    cdb[1] = (protocol << 1) | extend;
    cdb[2] = (chk_cond << 5) | (t_dir << 3) |
             (byte_block << 2) | t_length;
    cdb[(SAT_ATA_PASSTHROUGH_12LEN == passthru_size) ? 3 : 4] = feature;
    cdb[(SAT_ATA_PASSTHROUGH_12LEN == passthru_size) ? 4 : 6] = sector_count;
    cdb[(SAT_ATA_PASSTHROUGH_12LEN == passthru_size) ? 5 : 8] = lba_low;
    cdb[(SAT_ATA_PASSTHROUGH_12LEN == passthru_size) ? 6 : 10] = lba_mid;
    cdb[(SAT_ATA_PASSTHROUGH_12LEN == passthru_size) ? 7 : 12] = lba_high;
    cdb[(SAT_ATA_PASSTHROUGH_12LEN == passthru_size) ? 9 : 14] = ata_command;

    memset(&io_hdr, 0, sizeof(io_hdr));
    if (0 == t_length) {
        io_hdr.dxfer_dir = DXFER_NONE;
        io_hdr.dxfer_len = 0;
    } else if (t_dir) {         /* from device */
        io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
        io_hdr.dxfer_len = copydata;
        io_hdr.dxferp = (unsigned char *)data;
        memset(data, 0, copydata); /* prefill with zeroes */
    } else {                    /* to device */
        io_hdr.dxfer_dir = DXFER_TO_DEVICE;
        io_hdr.dxfer_len = outlen;
        io_hdr.dxferp = (unsigned char *)data;
    }
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = passthru_size;
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    status = do_scsi_cmnd_io(device, &io_hdr, con->reportscsiioctl);
    if (0 != status) {
        if (con->reportscsiioctl > 0)
            pout("sat_command_interface: do_scsi_cmnd_io() failed, "
                 "status=%d\n", status);
        return -1;
    }
    if (chk_cond) {     /* expecting SAT specific sense data */
        ucp = NULL;
        if (sg_scsi_normalize_sense(io_hdr.sensep, io_hdr.resp_sense_len,
                                    &ssh)) {
            /* look for SAT extended ATA status return descriptor (9) */
            ucp = sg_scsi_sense_desc_find(io_hdr.sensep,
                                          io_hdr.resp_sense_len, 9);
            if (ucp) {
                len = ucp[1] + 2;
                if (len < 12)
                    len = 12;
                else if (len > 14)
                    len = 14;
                if (con->reportscsiioctl > 1) {
                    pout("Values from ATA status return descriptor are:\n");
                    dStrHex((const char *)ucp, len, 1);
                }
                if (ATA_CHECK_POWER_MODE == cdb[14])
                    data[0] = ucp[5];      /* sector count (0:7) */
                else if (STATUS_CHECK == command) {
                    if ((ucp[9] == 0x4f) && (ucp[11] == 0xc2))
                        return 0;    /* GOOD smart status */
                    if ((ucp[9] == 0xf4) && (ucp[11] == 0x2c))
                        return 1;    /* smart predicting failure, "bad" status */
                    // We haven't gotten output that makes sense; print out some debugging info
                    syserror("Error SMART Status command failed");
                    pout("Please get assistance from " PACKAGE_HOMEPAGE "\n");
                    pout("Values from ATA status return descriptor are:\n");
                    dStrHex((const char *)ucp, len, 1);
                    return -1;
                }
            }
        }
        if (ucp == NULL) {
            chk_cond = 0;       /* not the type of sense data expected */
            if (t_dir && (t_length > 0))
                data[0] = 0;
        }
    }
    if (0 == chk_cond) {
        ucp = NULL;
        if (sg_scsi_normalize_sense(io_hdr.sensep, io_hdr.resp_sense_len,
                                    &ssh)) {
            if ((ssh.response_code >= 0x72) &&
                ((SCSI_SK_NO_SENSE == ssh.sense_key) ||
                 (SCSI_SK_RECOVERED_ERR == ssh.sense_key)) &&
                (0 == ssh.asc) &&
                (SCSI_ASCQ_ATA_PASS_THROUGH == ssh.ascq)) {
                /* look for SAT extended ATA status return descriptor (9) */
                ucp = sg_scsi_sense_desc_find(io_hdr.sensep,
                                              io_hdr.resp_sense_len, 9);
                if (ucp) {
                    if (con->reportscsiioctl > 0) {
                        pout("Values from ATA status return descriptor are:\n");
                        len = ucp[1] + 2;
                        if (len < 12)
                            len = 12;
                        else if (len > 14)
                            len = 14;
                        dStrHex((const char *)ucp, len, 1);
                    }
                    return -1;
                }
            }
            scsi_do_sense_disect(&io_hdr, &sinfo);
            status = scsiSimpleSenseFilter(&sinfo);
            if (0 != status) {
                if (con->reportscsiioctl > 0)
                    pout("sat_command_interface: scsi error: %s\n",
                         scsiErrString(status));
                return -1;
            }
        }
    }
    return 0;
}

/* Next two functions are borrowed from sg_lib.c in the sg3_utils
   package. Same copyrght owner, same license as this file. */
int sg_scsi_normalize_sense(const unsigned char * sensep, int sb_len,
                            struct sg_scsi_sense_hdr * sshp)
{
    if (sshp)
        memset(sshp, 0, sizeof(struct sg_scsi_sense_hdr));
    if ((NULL == sensep) || (0 == sb_len) || (0x70 != (0x70 & sensep[0])))
        return 0;
    if (sshp) {
        sshp->response_code = (0x7f & sensep[0]);
        if (sshp->response_code >= 0x72) {  /* descriptor format */
            if (sb_len > 1)
                sshp->sense_key = (0xf & sensep[1]);
            if (sb_len > 2)
                sshp->asc = sensep[2];
            if (sb_len > 3)
                sshp->ascq = sensep[3];
            if (sb_len > 7)
                sshp->additional_length = sensep[7];
        } else {                              /* fixed format */
            if (sb_len > 2)
                sshp->sense_key = (0xf & sensep[2]);
            if (sb_len > 7) {
                sb_len = (sb_len < (sensep[7] + 8)) ? sb_len :
                                                      (sensep[7] + 8);
                if (sb_len > 12)
                    sshp->asc = sensep[12];
                if (sb_len > 13)
                    sshp->ascq = sensep[13];
            }
        }
    }
    return 1;
}


const unsigned char * sg_scsi_sense_desc_find(const unsigned char * sensep,
                                              int sense_len, int desc_type)
{
    int add_sen_len, add_len, desc_len, k;
    const unsigned char * descp;

    if ((sense_len < 8) || (0 == (add_sen_len = sensep[7])))
        return NULL;
    if ((sensep[0] < 0x72) || (sensep[0] > 0x73))
        return NULL;
    add_sen_len = (add_sen_len < (sense_len - 8)) ?
                         add_sen_len : (sense_len - 8);
    descp = &sensep[8];
    for (desc_len = 0, k = 0; k < add_sen_len; k += desc_len) {
        descp += desc_len;
        add_len = (k < (add_sen_len - 1)) ? descp[1]: -1;
        desc_len = add_len + 2;
        if (descp[0] == desc_type)
            return descp;
        if (add_len < 0) /* short descriptor ?? */
            break;
    }
    return NULL;
}
