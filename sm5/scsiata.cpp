/*
 * scsiata.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2006-8 Douglas Gilbert <dougg@torque.net>
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
 * Note that in the latter case, this code does not solve the
 * addressing issue (i.e. which SATA disk to address behind the logical
 * SCSI (RAID) interface).
 * 
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "int64.h"
#include "extern.h"
#include "scsicmds.h"
#include "scsiata.h"
#include "ataprint.h" // ataReadHDIdentity()
#include "utility.h"
#include "dev_interface.h"
#include "dev_ata_cmd_set.h" // ata_device_with_command_set
#include "dev_tunnelled.h" // tunnelled_device<>

const char *scsiata_c_cvsid="$Id: scsiata.cpp,v 1.16 2008/08/23 17:07:17 chrfranke Exp $"
CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID SCSICMDS_H_CVSID SCSIATA_H_CVSID UTILITY_H_CVSID;

/* for passing global control variables */
extern smartmonctrl *con;

#define DEF_SAT_ATA_PASSTHRU_SIZE 16
#define ATA_RETURN_DESCRIPTOR 9

namespace sat { // no need to publish anything, name provided for Doxygen

/// SAT support.
/// Implements ATA by tunnelling through SCSI.

class sat_device
: public tunnelled_device<
    /*implements*/ ata_device
    /*by tunnelling through a*/, scsi_device
  >
{
public:
  sat_device(smart_interface * intf, scsi_device * scsidev,
    const char * req_type, int passthrulen = 0);

  virtual ~sat_device() throw();

  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

private:
  int m_passthrulen;
};


sat_device::sat_device(smart_interface * intf, scsi_device * scsidev,
  const char * req_type, int passthrulen /*= 0*/)
: smart_device(intf, scsidev->get_dev_name(), "sat", req_type),
  tunnelled_device<ata_device, scsi_device>(scsidev),
  m_passthrulen(passthrulen)
{
  set_info().info_name = strprintf("%s [SAT]", scsidev->get_info_name());
}

sat_device::~sat_device() throw()
{
}


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
// ATA Return Descriptor (component of descriptor sense data)
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
//   This interface routine takes ATA SMART commands and packages
//   them in the SAT-defined ATA PASS THROUGH SCSI commands. There are
//   two available SCSI commands: a 12 byte and 16 byte variant; the
//   one used is chosen via this->m_passthrulen .
// DETAILED DESCRIPTION OF ARGUMENTS
//   device: is the file descriptor provided by (a SCSI dvice type) open()
//   command: defines the different ATA operations.
//   select: additional input data if needed (which log, which type of
//           self-test).
//   data:   location to write output data, if needed (512 bytes).
//     Note: not all commands use all arguments.
// RETURN VALUES
//  -1 if the command failed
//   0 if the command succeeded,
//   STATUS_CHECK routine: 
//  -1 if the command failed
//   0 if the command succeeded and disk SMART status is "OK"
//   1 if the command succeeded and disk SMART status is "FAILING"

bool sat_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  if (!ata_cmd_is_ok(in,
    true, // data_out_support
    true, // multi_sector_support
    true) // ata_48bit_support
  )
    return false;

    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    struct sg_scsi_sense_hdr ssh;
    unsigned char cdb[SAT_ATA_PASSTHROUGH_16LEN];
    unsigned char sense[32];
    const unsigned char * ardp;
    int status, ard_len, have_sense;
    int extend = 0;
    int ck_cond = 0;    /* set to 1 to read register(s) back */
    int protocol = 3;   /* non-data */
    int t_dir = 1;      /* 0 -> to device, 1 -> from device */
    int byte_block = 1; /* 0 -> bytes, 1 -> 512 byte blocks */
    int t_length = 0;   /* 0 -> no data transferred */
    int passthru_size = DEF_SAT_ATA_PASSTHRU_SIZE;

    memset(cdb, 0, sizeof(cdb));
    memset(sense, 0, sizeof(sense));

    // Set data direction
    // TODO: This works only for commands where sector_count holds count!
    switch (in.direction) {
      case ata_cmd_in::no_data:
        break;
      case ata_cmd_in::data_in:
        protocol = 4;  // PIO data-in
        t_length = 2;  // sector_count holds count
        break;
      case ata_cmd_in::data_out:
        protocol = 5;  // PIO data-out
        t_length = 2;  // sector_count holds count
        t_dir = 0;     // to device
        break;
      default:
        return set_err(EINVAL, "sat_device::ata_pass_through: invalid direction=%d",
            (int)in.direction);
    }

    // Check condition if any output register needed
    if (in.out_needed.is_set())
        ck_cond = 1;

    if ((SAT_ATA_PASSTHROUGH_12LEN == m_passthrulen) ||
        (SAT_ATA_PASSTHROUGH_16LEN == m_passthrulen))
        passthru_size = m_passthrulen;

    // Set extend bit on 48-bit ATA command
    if (in.in_regs.is_48bit_cmd()) {
      if (passthru_size != SAT_ATA_PASSTHROUGH_16LEN)
        return set_err(ENOSYS, "48-bit ATA commands require SAT ATA PASS-THROUGH (16)");
      extend = 1;
    }

    cdb[0] = (SAT_ATA_PASSTHROUGH_12LEN == passthru_size) ?
             SAT_ATA_PASSTHROUGH_12 : SAT_ATA_PASSTHROUGH_16;

    cdb[1] = (protocol << 1) | extend;
    cdb[2] = (ck_cond << 5) | (t_dir << 3) |
             (byte_block << 2) | t_length;

    if (passthru_size == SAT_ATA_PASSTHROUGH_12LEN) {
        // ATA PASS-THROUGH (12)
        const ata_in_regs & lo = in.in_regs;
        cdb[3] = lo.features;
        cdb[4] = lo.sector_count;
        cdb[5] = lo.lba_low;
        cdb[6] = lo.lba_mid;
        cdb[7] = lo.lba_high;
        cdb[8] = lo.device;
        cdb[9] = lo.command;
    }
    else {
        // ATA PASS-THROUGH (16)
        const ata_in_regs & lo = in.in_regs;
        const ata_in_regs & hi = in.in_regs.prev;
        // Note: all 'in.in_regs.prev.*' are always zero for 28-bit commands
        cdb[ 3] = hi.features;
        cdb[ 4] = lo.features;
        cdb[ 5] = hi.sector_count;
        cdb[ 6] = lo.sector_count;
        cdb[ 7] = hi.lba_low;
        cdb[ 8] = lo.lba_low;
        cdb[ 9] = hi.lba_mid;
        cdb[10] = lo.lba_mid;
        cdb[11] = hi.lba_high;
        cdb[12] = lo.lba_high;
        cdb[13] = lo.device;
        cdb[14] = lo.command;
    }

    memset(&io_hdr, 0, sizeof(io_hdr));
    if (0 == t_length) {
        io_hdr.dxfer_dir = DXFER_NONE;
        io_hdr.dxfer_len = 0;
    } else if (t_dir) {         /* from device */
        io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
        io_hdr.dxfer_len = in.size;
        io_hdr.dxferp = (unsigned char *)in.buffer;
        memset(in.buffer, 0, in.size); // prefill with zeroes
    } else {                    /* to device */
        io_hdr.dxfer_dir = DXFER_TO_DEVICE;
        io_hdr.dxfer_len = in.size;
        io_hdr.dxferp = (unsigned char *)in.buffer;
    }
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = passthru_size;
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    scsi_device * scsidev = get_tunnel_dev();
    if (!scsidev->scsi_pass_through(&io_hdr)) {
        if (con->reportscsiioctl > 0)
            pout("sat_device::ata_pass_through: scsi_pass_through() failed, "
                 "errno=%d [%s]\n", scsidev->get_errno(), scsidev->get_errmsg());
        return set_err(scsidev->get_err());
    }
    ardp = NULL;
    ard_len = 0;
    have_sense = sg_scsi_normalize_sense(io_hdr.sensep, io_hdr.resp_sense_len,
                                         &ssh);
    if (have_sense) {
        /* look for SAT ATA Return Descriptor */
        ardp = sg_scsi_sense_desc_find(io_hdr.sensep,
                                       io_hdr.resp_sense_len,
                                       ATA_RETURN_DESCRIPTOR);
        if (ardp) {
            ard_len = ardp[1] + 2;
            if (ard_len < 12)
                ard_len = 12;
            else if (ard_len > 14)
                ard_len = 14;
        }
        scsi_do_sense_disect(&io_hdr, &sinfo);
        status = scsiSimpleSenseFilter(&sinfo);
        if (0 != status) {
            if (con->reportscsiioctl > 0) {
                pout("sat_device::ata_pass_through: scsi error: %s\n",
                     scsiErrString(status));
                if (ardp && (con->reportscsiioctl > 1)) {
                    pout("Values from ATA Return Descriptor are:\n");
                    dStrHex((const char *)ardp, ard_len, 1);
                }
            }
            if (t_dir && (t_length > 0) && (in.direction == ata_cmd_in::data_in))
                memset(in.buffer, 0, in.size);
            return set_err(EIO, "scsi error %s", scsiErrString(status));
        }
    }
    if (ck_cond) {     /* expecting SAT specific sense data */
        if (have_sense) {
            if (ardp) {
                if (con->reportscsiioctl > 1) {
                    pout("Values from ATA Return Descriptor are:\n");
                    dStrHex((const char *)ardp, ard_len, 1);
                }
                // Set output registers
                ata_out_regs & lo = out.out_regs;
                lo.error        = ardp[ 3];
                lo.sector_count = ardp[ 5];
                lo.lba_low      = ardp[ 7];
                lo.lba_mid      = ardp[ 9];
                lo.lba_high     = ardp[11];
                lo.device       = ardp[12];
                lo.status       = ardp[13];
                if (in.in_regs.is_48bit_cmd()) {
                    ata_out_regs & hi = out.out_regs.prev;
                    hi.sector_count = ardp[ 4];
                    hi.lba_low      = ardp[ 6];
                    hi.lba_mid      = ardp[ 8];
                    hi.lba_high     = ardp[10];
                }
            }
        }
        if (ardp == NULL)
            ck_cond = 0;       /* not the type of sense data expected */
    }
    if (0 == ck_cond) {
        if (have_sense) {
            if ((ssh.response_code >= 0x72) &&
                ((SCSI_SK_NO_SENSE == ssh.sense_key) ||
                 (SCSI_SK_RECOVERED_ERR == ssh.sense_key)) &&
                (0 == ssh.asc) &&
                (SCSI_ASCQ_ATA_PASS_THROUGH == ssh.ascq)) {
                if (ardp) {
                    if (con->reportscsiioctl > 0) {
                        pout("Values from ATA Return Descriptor are:\n");
                        dStrHex((const char *)ardp, ard_len, 1);
                    }
                    return set_err(EIO, "SAT command failed");
                }
            }
        }
    }
    return true;
}

} // namespace

/////////////////////////////////////////////////////////////////////////////

/* Attempt an IDENTIFY DEVICE ATA command via SATL when packet_interface
   is false otherwise attempt IDENTIFY PACKET DEVICE. If successful
   return true, else false */

static bool has_sat_pass_through(ata_device * dev, bool packet_interface = false)
{
    ata_cmd_in in;
    in.in_regs.command = (packet_interface ? ATA_IDENTIFY_PACKET_DEVICE : ATA_IDENTIFY_DEVICE);
    char data[512];
    in.set_data_in(data, 1);
    return dev->ata_pass_through(in);
}

/////////////////////////////////////////////////////////////////////////////

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


/////////////////////////////////////////////////////////////////////////////

namespace sat {

/// Cypress USB Brigde support.

class usbcypress_device
: public tunnelled_device<
    /*implements*/ ata_device_with_command_set
    /*by tunnelling through a*/, scsi_device
  >
{
public:
  usbcypress_device(smart_interface * intf, scsi_device * scsidev,
    const char * req_type, unsigned char signature);

  virtual ~usbcypress_device() throw();

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);

  unsigned char m_signature;
};


usbcypress_device::usbcypress_device(smart_interface * intf, scsi_device * scsidev,
  const char * req_type, unsigned char signature)
: smart_device(intf, scsidev->get_dev_name(), "sat", req_type),
  tunnelled_device<ata_device_with_command_set, scsi_device>(scsidev),
  m_signature(signature)
{
  set_info().info_name = strprintf("%s [USB Cypress]", scsidev->get_info_name());
}

usbcypress_device::~usbcypress_device() throw()
{
}


/* see cy7c68300c_8.pdf for more information */
#define USBCYPRESS_PASSTHROUGH_LEN 16
int usbcypress_device::ata_command_interface(smart_command_set command, int select, char *data)
{
    struct scsi_cmnd_io io_hdr;
    unsigned char cdb[USBCYPRESS_PASSTHROUGH_LEN];
    unsigned char sense[32];
    int copydata = 0;
    int outlen = 0;
    int ck_cond = 0;    /* set to 1 to read register(s) back */
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
    int passthru_size = USBCYPRESS_PASSTHROUGH_LEN;

    memset(cdb, 0, sizeof(cdb));
    memset(sense, 0, sizeof(sense));

    ata_command = ATA_SMART_CMD;
    switch (command) {
    case CHECK_POWER_MODE:
        ata_command = ATA_CHECK_POWER_MODE;
        ck_cond = 1;
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
        ck_cond = 1;
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
        ck_cond = 1;
        break;
    default:
        pout("Unrecognized command %d in usbcypress_device::ata_command_interface()\n"
             "Please contact " PACKAGE_BUGREPORT "\n", command);
        errno=ENOSYS;
        return -1;
    }
    if (ATA_SMART_CMD == ata_command) {
        lba_mid = 0x4f;
        lba_high = 0xc2;
    }

    cdb[0] = m_signature; // bVSCBSignature : vendor-specific command
    cdb[1] = 0x24; // bVSCBSubCommand : 0x24 for ATACB
    cdb[2] = 0x0;
    if (ata_command == ATA_IDENTIFY_DEVICE || ata_command == ATA_IDENTIFY_PACKET_DEVICE)
        cdb[2] |= (1<<7); //set  IdentifyPacketDevice for these cmds
    cdb[3] = 0xff - (1<<0) - (1<<6); //features, sector count, lba low, lba med
                                     // lba high, command are valid
    cdb[4] = byte_block; //TransferBlockCount : 512


    cdb[6] = feature;
    cdb[7] = sector_count;
    cdb[8] = lba_low;
    cdb[9] = lba_mid;
    cdb[10] = lba_high;
    cdb[12] = ata_command;

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

    scsi_device * scsidev = get_tunnel_dev();
    if (!scsidev->scsi_pass_through(&io_hdr)) {
        if (con->reportscsiioctl > 0)
            pout("usbcypress_device::ata_command_interface: scsi_pass_through() failed, "
                 "errno=%d [%s]\n", scsidev->get_errno(), scsidev->get_errmsg());
        set_err(scsidev->get_err());
        return -1;
    }

    // if there is a sense the command failed or the
    // device doesn't support usbcypress
    if (io_hdr.scsi_status == SCSI_STATUS_CHECK_CONDITION && 
            sg_scsi_normalize_sense(io_hdr.sensep, io_hdr.resp_sense_len, NULL)) {
        return -1;
    }
    if (ck_cond) {
        unsigned char ardp[8];
        int ard_len = 8;
        /* XXX this is racy if there other scsi command between
         * the first usbcypress command and this one
         */
        //pout("If you got strange result, please retry without traffic on the disc\n");
        /* we use the same command as before, but we set
         * * the read taskfile bit, for not executing usbcypress command,
         * * but reading register selected in srb->cmnd[4]
         */
        cdb[2] = (1<<0); /* ask read taskfile */
        memset(sense, 0, sizeof(sense));

        /* transfert 8 bytes */
        memset(&io_hdr, 0, sizeof(io_hdr));
        io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
        io_hdr.dxfer_len = ard_len;
        io_hdr.dxferp = (unsigned char *)ardp;
        memset(ardp, 0, ard_len); /* prefill with zeroes */

        io_hdr.cmnd = cdb;
        io_hdr.cmnd_len = passthru_size;
        io_hdr.sensep = sense;
        io_hdr.max_sense_len = sizeof(sense);
        io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;


        if (!scsidev->scsi_pass_through(&io_hdr)) {
            if (con->reportscsiioctl > 0)
                pout("usbcypress_device::ata_command_interface: scsi_pass_through() failed, "
                     "errno=%d [%s]\n", scsidev->get_errno(), scsidev->get_errmsg());
            set_err(scsidev->get_err());
            return -1;
        }
        // if there is a sense the command failed or the
        // device doesn't support usbcypress
        if (io_hdr.scsi_status == SCSI_STATUS_CHECK_CONDITION && 
                sg_scsi_normalize_sense(io_hdr.sensep, io_hdr.resp_sense_len, NULL)) {
            return -1;
        }


        if (con->reportscsiioctl > 1) {
            pout("Values from ATA Return Descriptor are:\n");
            dStrHex((const char *)ardp, ard_len, 1);
        }

        if (ATA_CHECK_POWER_MODE == ata_command)
            data[0] = ardp[2];      /* sector count (0:7) */
        else if (STATUS_CHECK == command) {
            if ((ardp[4] == 0x4f) && (ardp[5] == 0xc2))
                return 0;    /* GOOD smart status */
            if ((ardp[4] == 0xf4) && (ardp[5] == 0x2c))
                return 1;    // smart predicting failure, "bad" status
            // We haven't gotten output that makes sense so
            // print out some debugging info
            syserror("Error SMART Status command failed");
            pout("This may be due to a race in usbcypress\n");
            pout("Retry without other disc access\n");
            pout("Please get assistance from " PACKAGE_HOMEPAGE "\n");
            pout("Values from ATA Return Descriptor are:\n");
            dStrHex((const char *)ardp, ard_len, 1);
            return -1;
        }
    }
    return 0;
}

static int isprint_string(const char *s)
{
    while (*s) {
        if (isprint(*s) == 0)
            return 0;
        s++;
    }
    return 1;
}

/* Attempt an IDENTIFY DEVICE ATA or IDENTIFY PACKET DEVICE command
   If successful return 1, else 0 */
// TODO: Combine with has_sat_pass_through above
static int has_usbcypress_pass_through(ata_device * atadev, const char *manufacturer, const char *product)
{
    struct ata_identify_device drive;
    char model[40], serial[20], firm[8];

    /* issue the command and do a checksum if possible */
    if (ataReadHDIdentity(atadev, &drive) < 0)
        return 0;

    /* check if model string match, revision doesn't work for me */
    format_ata_string(model, (char *)drive.model,40);
    if (*model == 0 || isprint_string(model) == 0)
        return 0;

    if (manufacturer && strncmp(manufacturer, model, 8))
        pout("manufacturer doesn't match in pass_through test\n");
    if (product &&
            strlen(model) > 8 && strncmp(product, model+8, strlen(model)-8))
        pout("product doesn't match in pass_through test\n");

    /* check serial */
    format_ata_string(serial, (char *)drive.serial_no,20);
    if (isprint_string(serial) == 0)
        return 0;
    format_ata_string(firm, (char *)drive.fw_rev,8);
    if (isprint_string(firm) == 0)
        return 0;
    return 1;
}

} // namespace

using namespace sat;


/////////////////////////////////////////////////////////////////////////////

// Return ATA->SCSI filter for SAT or USB.

ata_device * smart_interface::get_sat_device(const char * name, const char * type, scsi_device * scsidev /* = 0*/)
{
  if (!scsidev) {
    scsidev = get_scsi_device(name, "scsi");
    if (!scsidev)
      return 0;
  }
  if (!strncmp(type, "sat", 3)) {
    int ptlen = 0, n1 = -1, n2 = -1;
    if (!(((sscanf(type, "sat%n,%d%n", &n1, &ptlen, &n2) == 1 && n2 == (int)strlen(type)) || n1 == (int)strlen(type))
        && (ptlen == 0 || ptlen == 12 || ptlen == 16))) {
      set_err(EINVAL, "Option '-d sat,<n>' requires <n> to be 0, 12 or 16");
      return 0;
    }
    return new sat_device(this, scsidev, type, ptlen);
  }
  else if (!strncmp(type, "usbcypress", 10)) {
    unsigned signature = 0x24; int n1 = -1, n2 = -1;
    if (!(((sscanf(type, "usbcypress%n,0x%x%n", &n1, &signature, &n2) == 1 && n2 == (int)strlen(type)) || n1 == (int)strlen(type))
          && signature <= 0xff)) {
      set_err(EINVAL, "Option '-d usbcypress,<n>' requires <n> to be "
                      "an hexadecimal number between 0x0 and 0xff");
      return 0;
    }
    return new usbcypress_device(this, scsidev, type, signature);
  }
  else {
    set_err(EINVAL, "Unknown USB device type '%s'", type);
    return 0;
  }
}

// Try to detect a SAT device behind a SCSI interface.

ata_device * smart_interface::autodetect_sat_device(scsi_device * scsidev,
  const unsigned char * inqdata, unsigned inqsize)
{
  if (!scsidev->is_open())
    return 0;

  ata_device * atadev = 0;
  try {
    // SAT ?
    if (inqdata && inqsize >= 36 && !memcmp(inqdata + 8, "ATA     ", 8)) { // TODO: Linux-specific?
      atadev = new sat_device(this, scsidev, "");
      if (has_sat_pass_through(atadev))
        return atadev; // Detected SAT
      atadev->release(scsidev);
      delete atadev;
    }

    // USB ?
    {
      atadev = new usbcypress_device(this, scsidev, "", 0x24);
      if (has_usbcypress_pass_through(atadev,
            (inqdata && inqsize >= 36 ? (const char*)inqdata  + 8 : 0),
            (inqdata && inqsize >= 36 ? (const char*)inqdata + 16 : 0) ))
        return atadev; // Detected USB
      atadev->release(scsidev);
      delete atadev;
    }
  }
  catch (...) {
    if (atadev) {
      atadev->release(scsidev);
      delete atadev;
    }
    throw;
  }

  return 0;
}
