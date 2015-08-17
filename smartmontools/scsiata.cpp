/*
 * scsiata.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2006-15 Douglas Gilbert <dgilbert@interlog.com>
 * Copyright (C) 2009-15 Christian Franke
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
 * The code in this file is based on the SCSI to ATA Translation (SAT)
 * draft found at http://www.t10.org . The original draft used for this
 * code is sat-r08.pdf which is not too far away from becoming a
 * standard. The SAT commands of interest to smartmontools are the
 * ATA PASS THROUGH SCSI (16) and ATA PASS THROUGH SCSI (12) defined in
 * section 12 of that document.
 *
 * sat-r09.pdf is the most recent, easily accessible draft prior to the
 * original SAT standard (ANSI INCITS 431-2007). By mid-2009 the second
 * version of the SAT standard (SAT-2) is nearing standardization. In
 * their wisdom an incompatible change has been introduced in draft
 * sat2r08a.pdf in the area of the ATA RETURN DESCRIPTOR. A new "fixed
 * format" ATA RETURN buffer has been defined (sat2r08b.pdf section
 * 12.2.7) for the case when DSENSE=0 in the Control mode page.
 * Unfortunately this is the normal case. If the change stands our
 * code will need to be extended for this case.
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
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "config.h"
#include "int64.h"
#include "scsicmds.h"
#include "atacmds.h" // ataReadHDIdentity()
#include "knowndrives.h" // lookup_usb_device()
#include "utility.h"
#include "dev_interface.h"
#include "dev_ata_cmd_set.h" // ata_device_with_command_set
#include "dev_tunnelled.h" // tunnelled_device<>

const char * scsiata_cpp_cvsid = "$Id$";

/* This is a slightly stretched SCSI sense "descriptor" format header.
   The addition is to allow the 0x70 and 0x71 response codes. The idea
   is to place the salient data of both "fixed" and "descriptor" sense
   format into one structure to ease application processing.
   The original sense buffer should be kept around for those cases
   in which more information is required (e.g. the LBA of a MEDIUM ERROR). */
/// Abridged SCSI sense data
struct sg_scsi_sense_hdr {
    unsigned char response_code; /* permit: 0x0, 0x70, 0x71, 0x72, 0x73 */
    unsigned char sense_key;
    unsigned char asc;
    unsigned char ascq;
    unsigned char byte4;
    unsigned char byte5;
    unsigned char byte6;
    unsigned char additional_length;
};

/* Maps the salient data from a sense buffer which is in either fixed or
   descriptor format into a structure mimicking a descriptor format
   header (i.e. the first 8 bytes of sense descriptor format).
   If zero response code returns 0. Otherwise returns 1 and if 'sshp' is
   non-NULL then zero all fields and then set the appropriate fields in
   that structure. sshp::additional_length is always 0 for response
   codes 0x70 and 0x71 (fixed format). */
static int sg_scsi_normalize_sense(const unsigned char * sensep, int sb_len,
                                   struct sg_scsi_sense_hdr * sshp);

#define SAT_ATA_PASSTHROUGH_12LEN 12
#define SAT_ATA_PASSTHROUGH_16LEN 16

#define DEF_SAT_ATA_PASSTHRU_SIZE 16
#define ATA_RETURN_DESCRIPTOR 9


namespace sat { // no need to publish anything, name provided for Doxygen

/// SAT support.
/// Implements ATA by tunnelling through SCSI.

class sat_device
: public tunnelled_device<
    /*implements*/ ata_device
    /*by tunnelling through a*/, scsi_device
  >,
  virtual public /*implements*/ scsi_device
{
public:
  sat_device(smart_interface * intf, scsi_device * scsidev,
    const char * req_type, int passthrulen = 0, bool enable_auto = false);

  virtual ~sat_device() throw();

  virtual smart_device * autodetect_open();

  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

  virtual bool scsi_pass_through(scsi_cmnd_io * iop);

private:
  int m_passthrulen;
  bool m_enable_auto;
};


sat_device::sat_device(smart_interface * intf, scsi_device * scsidev,
  const char * req_type, int passthrulen /* = 0 */, bool enable_auto /* = false */)
: smart_device(intf, scsidev->get_dev_name(),
    (enable_auto ? "sat,auto" : "sat"), req_type),
  tunnelled_device<ata_device, scsi_device>(scsidev),
  m_passthrulen(passthrulen),
  m_enable_auto(enable_auto)
{
  if (enable_auto)
    hide_ata(); // Start as SCSI, switch to ATA in autodetect_open()
  else
    hide_scsi(); // ATA always
  if (strcmp(scsidev->get_dev_type(), "scsi"))
    set_info().dev_type += strprintf("+%s", scsidev->get_dev_type());

  set_info().info_name = strprintf("%s [%sSAT]", scsidev->get_info_name(),
                                   (enable_auto ? "SCSI/" : ""));
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
//
//
// ATA registers returned via fixed format sense (allowed >= SAT-2)
// fxs[0]: info_valid (bit 7); response_code (6:0)
// fxs[1]: (obsolete)
// fxs[2]: sense_key (3:0) --> recovered error (formerly 'no sense')
// fxs[3]: information (31:24) --> ATA Error register
// fxs[4]: information (23:16) --> ATA Status register
// fxs[5]: information (15:8) --> ATA Device register
// fxs[6]: information (7:0) --> ATA Count (7:0)
// fxs[7]: additional sense length [should be >= 10]
// fxs[8]: command specific info (31:24) --> Extend (7), count_upper_nonzero
//         (6), lba_upper_nonzero(5), log_index (3:0)
// fxs[9]: command specific info (23:16) --> ATA LBA (7:0)
// fxs[10]: command specific info (15:8) --> ATA LBA (15:8)
// fxs[11]: command specific info (7:0) --> ATA LBA (23:16)
// fxs[12]: additional sense code (asc) --> 0x0
// fxs[13]: additional sense code qualifier (ascq) --> 0x1d
//          asc,ascq = 0x0,0x1d --> 'ATA pass through information available'



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
  if (!ata_cmd_is_supported(in,
    ata_device::supports_data_out |
    ata_device::supports_output_regs |
    ata_device::supports_multi_sector |
    ata_device::supports_48bit,
    "SAT")
  )
    return false;

    struct scsi_cmnd_io io_hdr;
    struct scsi_sense_disect sinfo;
    struct sg_scsi_sense_hdr ssh;
    unsigned char cdb[SAT_ATA_PASSTHROUGH_16LEN];
    unsigned char sense[32];
    const unsigned char * ardp;
    int ard_len, have_sense;
    int extend = 0;
    int ck_cond = 0;    /* set to 1 to read register(s) back */
    int protocol = 3;   /* non-data */
    int t_dir = 1;      /* 0 -> to device, 1 -> from device */
    int byte_block = 1; /* 0 -> bytes, 1 -> 512 byte blocks */
    int t_length = 0;   /* 0 -> no data transferred */
    int passthru_size = DEF_SAT_ATA_PASSTHRU_SIZE;
    bool sense_descriptor = true;

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
        if (scsi_debugmode > 0)
            pout("sat_device::ata_pass_through: scsi_pass_through() failed, "
                 "errno=%d [%s]\n", scsidev->get_errno(), scsidev->get_errmsg());
        return set_err(scsidev->get_err());
    }
    ardp = NULL;
    ard_len = 0;
    have_sense = sg_scsi_normalize_sense(io_hdr.sensep, io_hdr.resp_sense_len,
                                         &ssh);
    if (have_sense) {
        sense_descriptor = ssh.response_code >= 0x72;
        if (sense_descriptor) {
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
        }
        scsi_do_sense_disect(&io_hdr, &sinfo);
        int status = scsiSimpleSenseFilter(&sinfo);

        // Workaround for bogus sense_key in sense data with SAT ATA Return Descriptor
        if (   status && ck_cond && ardp && ard_len > 13
            && (ardp[13] & 0xc1) == 0x40 /* !BSY && DRDY && !ERR */) {
            if (scsi_debugmode > 0)
                pout("ATA status (0x%02x) indicates success, ignoring SCSI sense_key\n",
                     ardp[13]);
            status = 0;
        }

        if (0 != status) {  /* other than no_sense and recovered_error */
            if (scsi_debugmode > 0) {
                pout("sat_device::ata_pass_through: scsi error: %s\n",
                     scsiErrString(status));
                if (ardp && (scsi_debugmode > 1)) {
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
                if (scsi_debugmode > 1) {
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
            } else if ((! sense_descriptor) &&
                       (0 == ssh.asc) &&
                       (SCSI_ASCQ_ATA_PASS_THROUGH == ssh.ascq)) {
                /* in SAT-2 and later, ATA registers may be passed back via
                 * fixed format sense data [ref: sat3r07 section 12.2.2.7] */
                ata_out_regs & lo = out.out_regs;
                lo.error        = io_hdr.sensep[ 3];
                lo.status       = io_hdr.sensep[ 4];
                lo.device       = io_hdr.sensep[ 5];
                lo.sector_count = io_hdr.sensep[ 6];
                lo.lba_low      = io_hdr.sensep[ 9];
                lo.lba_mid      = io_hdr.sensep[10];
                lo.lba_high     = io_hdr.sensep[11];
                if (in.in_regs.is_48bit_cmd()) {
                    if (0 == (0x60 & io_hdr.sensep[8])) {
                        ata_out_regs & hi = out.out_regs.prev;
                        hi.sector_count = 0;
                        hi.lba_low      = 0;
                        hi.lba_mid      = 0;
                        hi.lba_high     = 0;
                    } else {
                        /* getting the "hi." values when either
                         * count_upper_nonzero or lba_upper_nonzero are set
                         * involves fetching the SCSI ATA PASS-THROUGH
                         * Results log page and decoding the descriptor with
                         * the matching log_index field. Painful. */ 
                    }
                }
            }
        }
    } else {    /* ck_cond == 0 */
        if (have_sense) {
            if (((SCSI_SK_NO_SENSE == ssh.sense_key) ||
                 (SCSI_SK_RECOVERED_ERR == ssh.sense_key)) &&
                (0 == ssh.asc) &&
                (SCSI_ASCQ_ATA_PASS_THROUGH == ssh.ascq)) {
                if (scsi_debugmode > 0) {
                    if (sense_descriptor && ardp) {
                        pout("Values from ATA Return Descriptor are:\n");
                        dStrHex((const char *)ardp, ard_len, 1);
                    } else if (! sense_descriptor) {
                        pout("Values from ATA fixed format sense are:\n");
                        pout("  Error: 0x%x\n", io_hdr.sensep[3]);
                        pout("  Status: 0x%x\n", io_hdr.sensep[4]);
                        pout("  Device: 0x%x\n", io_hdr.sensep[5]);
                        pout("  Count: 0x%x\n", io_hdr.sensep[6]);
                    }
                }
            }
            return set_err(EIO, "SAT command failed");
        }
    }
    return true;
}

bool sat_device::scsi_pass_through(scsi_cmnd_io * iop)
{
  scsi_device * scsidev = get_tunnel_dev();
  if (!scsidev->scsi_pass_through(iop)) {
    set_err(scsidev->get_err());
    return false;
  }
  return true;
}

smart_device * sat_device::autodetect_open()
{
  if (!open() || !m_enable_auto)
    return this;

  scsi_device * scsidev = get_tunnel_dev();

  unsigned char inqdata[36] = {0, };
  if (scsiStdInquiry(scsidev, inqdata, sizeof(inqdata))) {
      smart_device::error_info err = scsidev->get_err();
      close();
      set_err(err.no, "INQUIRY [SAT]: %s", err.msg.c_str());
      return this;
  }

  // Check for SAT "VENDOR"
  int inqsize = inqdata[4] + 5;
  bool sat = (inqsize >= 36 && !memcmp(inqdata + 8, "ATA     ", 8));

  // Change interface
  hide_ata(!sat);
  hide_scsi(sat);

  set_info().dev_type = (sat ? "sat" : scsidev->get_dev_type());
  set_info().info_name = strprintf("%s [%s]", scsidev->get_info_name(),
                                   (sat ? "SAT" : "SCSI"));
  return this;
}

} // namespace

/////////////////////////////////////////////////////////////////////////////

/* Attempt an IDENTIFY DEVICE ATA command via SATL when packet_interface
   is false otherwise attempt IDENTIFY PACKET DEVICE. If successful
   return true, else false */

static bool has_sat_pass_through(ata_device * dev, bool packet_interface = false)
{
    /* Note:  malloc() ensures the read buffer lands on a single
       page.  This avoids some bugs seen on LSI controlers under
       FreeBSD */
    char *data = (char *)malloc(512);
    ata_cmd_in in;
    in.in_regs.command = (packet_interface ? ATA_IDENTIFY_PACKET_DEVICE : ATA_IDENTIFY_DEVICE);
    in.set_data_in(data, 1);
    bool ret = dev->ata_pass_through(in);
    free(data);
    return ret;
}

/////////////////////////////////////////////////////////////////////////////

/* Next two functions are borrowed from sg_lib.c in the sg3_utils
   package. Same copyrght owner, same license as this file. */
static int sg_scsi_normalize_sense(const unsigned char * sensep, int sb_len,
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


// Call scsi_pass_through and check sense.
// TODO: Provide as member function of class scsi_device (?)
static bool scsi_pass_through_and_check(scsi_device * scsidev,  scsi_cmnd_io * iop,
                                        const char * msg = "")
{
  // Provide sense buffer
  unsigned char sense[32] = {0, };
  iop->sensep = sense;
  iop->max_sense_len = sizeof(sense);
  iop->timeout = SCSI_TIMEOUT_DEFAULT;

  // Run cmd
  if (!scsidev->scsi_pass_through(iop)) {
    if (scsi_debugmode > 0)
      pout("%sscsi_pass_through() failed, errno=%d [%s]\n",
           msg, scsidev->get_errno(), scsidev->get_errmsg());
    return false;
  }

  // Check sense
  scsi_sense_disect sinfo;
  scsi_do_sense_disect(iop, &sinfo);
  int err = scsiSimpleSenseFilter(&sinfo);
  if (err) {
    if (scsi_debugmode > 0)
      pout("%sscsi error: %s\n", msg, scsiErrString(err));
    return scsidev->set_err(EIO, "scsi error %s", scsiErrString(err));
  }

  return true;
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
        t_length = 2;   /* sector count holds count */
        copydata = 512;
        break;
    case READ_THRESHOLDS:       /* obsolete */
        feature = ATA_SMART_READ_THRESHOLDS;
        sector_count = 1;     /* one (512 byte) block */
        lba_low = 1;
        t_length = 2;   /* sector count holds count */
        copydata=512;
        break;
    case READ_LOG:
        feature = ATA_SMART_READ_LOG_SECTOR;
        sector_count = 1;     /* one (512 byte) block */
        lba_low = select;
        t_length = 2;   /* sector count holds count */
        copydata = 512;
        break;
    case WRITE_LOG:
        feature = ATA_SMART_WRITE_LOG_SECTOR;
        sector_count = 1;     /* one (512 byte) block */
        lba_low = select;
        t_length = 2;   /* sector count holds count */
        t_dir = 0;      /* to device */
        outlen = 512;
        break;
    case IDENTIFY:
        ata_command = ATA_IDENTIFY_DEVICE;
        sector_count = 1;     /* one (512 byte) block */
        t_length = 2;   /* sector count holds count */
        copydata = 512;
        break;
    case PIDENTIFY:
        ata_command = ATA_IDENTIFY_PACKET_DEVICE;
        sector_count = 1;     /* one (512 byte) block */
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
        if (scsi_debugmode > 0)
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
            if (scsi_debugmode > 0)
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


        if (scsi_debugmode > 1) {
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

#if 0 // Not used, see autodetect_sat_device() below.
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
    format_ata_string(model, drive.model, 40);
    if (*model == 0 || isprint_string(model) == 0)
        return 0;

    if (manufacturer && strncmp(manufacturer, model, 8))
        pout("manufacturer doesn't match in pass_through test\n");
    if (product &&
            strlen(model) > 8 && strncmp(product, model+8, strlen(model)-8))
        pout("product doesn't match in pass_through test\n");

    /* check serial */
    format_ata_string(serial, drive.serial_no, 20);
    if (isprint_string(serial) == 0)
        return 0;
    format_ata_string(firm, drive.fw_rev, 8);
    if (isprint_string(firm) == 0)
        return 0;
    return 1;
}
#endif

/////////////////////////////////////////////////////////////////////////////

/// JMicron USB Bridge support.

class usbjmicron_device
: public tunnelled_device<
    /*implements*/ ata_device,
    /*by tunnelling through a*/ scsi_device
  >
{
public:
  usbjmicron_device(smart_interface * intf, scsi_device * scsidev,
                    const char * req_type, bool prolific,
                    bool ata_48bit_support, int port);

  virtual ~usbjmicron_device() throw();

  virtual bool open();

  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

private:
  bool get_registers(unsigned short addr, unsigned char * buf, unsigned short size);

  bool m_prolific;
  bool m_ata_48bit_support;
  int m_port;
};


usbjmicron_device::usbjmicron_device(smart_interface * intf, scsi_device * scsidev,
                                     const char * req_type, bool prolific,
                                     bool ata_48bit_support, int port)
: smart_device(intf, scsidev->get_dev_name(), "usbjmicron", req_type),
  tunnelled_device<ata_device, scsi_device>(scsidev),
  m_prolific(prolific), m_ata_48bit_support(ata_48bit_support),
  m_port(port >= 0 || !prolific ? port : 0)
{
  set_info().info_name = strprintf("%s [USB JMicron]", scsidev->get_info_name());
}

usbjmicron_device::~usbjmicron_device() throw()
{
}


bool usbjmicron_device::open()
{
  // Open USB first
  if (!tunnelled_device<ata_device, scsi_device>::open())
    return false;

  // Detect port if not specified
  if (m_port < 0) {
    unsigned char regbuf[1] = {0};
    if (!get_registers(0x720f, regbuf, sizeof(regbuf))) {
      close();
      return false;
    }

    switch (regbuf[0] & 0x44) {
      case 0x04:
        m_port = 0; break;
      case 0x40:
        m_port = 1; break;
      case 0x44:
        close();
        return set_err(EINVAL, "Two devices connected, try '-d usbjmicron,[01]'");
      default:
        close();
        return set_err(ENODEV, "No device connected");
    }
  }

  return true;
}


bool usbjmicron_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  if (!ata_cmd_is_supported(in,
    ata_device::supports_data_out |
    ata_device::supports_smart_status |
    (m_ata_48bit_support ? ata_device::supports_48bit_hi_null : 0),
    "JMicron")
  )
    return false;

  if (m_port < 0)
    return set_err(EIO, "Unknown JMicron port");

  scsi_cmnd_io io_hdr;
  memset(&io_hdr, 0, sizeof(io_hdr));

  bool rwbit = true;
  unsigned char smart_status = 0xff;

  bool is_smart_status = (   in.in_regs.command  == ATA_SMART_CMD
                          && in.in_regs.features == ATA_SMART_STATUS);

  if (is_smart_status && in.out_needed.is_set()) {
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = 1;
    io_hdr.dxferp = &smart_status;
  }
  else switch (in.direction) {
    case ata_cmd_in::no_data:
      io_hdr.dxfer_dir = DXFER_NONE;
      break;
    case ata_cmd_in::data_in:
      io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
      io_hdr.dxfer_len = in.size;
      io_hdr.dxferp = (unsigned char *)in.buffer;
      memset(in.buffer, 0, in.size);
      break;
    case ata_cmd_in::data_out:
      io_hdr.dxfer_dir = DXFER_TO_DEVICE;
      io_hdr.dxfer_len = in.size;
      io_hdr.dxferp = (unsigned char *)in.buffer;
      rwbit = false;
      break;
    default:
      return set_err(EINVAL);
  }

  // Build pass through command
  unsigned char cdb[14];
  cdb[ 0] = 0xdf;
  cdb[ 1] = (rwbit ? 0x10 : 0x00);
  cdb[ 2] = 0x00;
  cdb[ 3] = (unsigned char)(io_hdr.dxfer_len >> 8);
  cdb[ 4] = (unsigned char)(io_hdr.dxfer_len     );
  cdb[ 5] = in.in_regs.features;
  cdb[ 6] = in.in_regs.sector_count;
  cdb[ 7] = in.in_regs.lba_low;
  cdb[ 8] = in.in_regs.lba_mid;
  cdb[ 9] = in.in_regs.lba_high;
  cdb[10] = in.in_regs.device | (m_port == 0 ? 0xa0 : 0xb0);
  cdb[11] = in.in_regs.command;
  // Prolific PL3507
  cdb[12] = 0x06;
  cdb[13] = 0x7b;

  io_hdr.cmnd = cdb;
  io_hdr.cmnd_len = (!m_prolific ? 12 : 14);

  scsi_device * scsidev = get_tunnel_dev();
  if (!scsi_pass_through_and_check(scsidev, &io_hdr,
         "usbjmicron_device::ata_pass_through: "))
    return set_err(scsidev->get_err());

  if (in.out_needed.is_set()) {
    if (is_smart_status) {
      if (io_hdr.resid == 1)
        // Some (Prolific) USB bridges do not transfer a status byte
        return set_err(ENOSYS, "Incomplete response, status byte missing [JMicron]");

      switch (smart_status) {
        case 0xc2:
          out.out_regs.lba_high = 0xc2;
          out.out_regs.lba_mid = 0x4f;
          break;
        case 0x2c:
          out.out_regs.lba_high = 0x2c;
          out.out_regs.lba_mid = 0xf4;
          break;
        default:
          // Some (JM20336) USB bridges always return 0x01, regardless of SMART Status
          return set_err(ENOSYS, "Invalid status byte (0x%02x) [JMicron]", smart_status);
      }
    }

#if 0 // Not needed for SMART STATUS, see also notes below
    else {
      // Read ATA output registers
      // NOTE: The register addresses are not valid for some older chip revisions
      // NOTE: There is a small race condition here!
      unsigned char regbuf[16] = {0, };
      if (!get_registers((m_port == 0 ? 0x8000 : 0x9000), regbuf, sizeof(regbuf)))
        return false;

      out.out_regs.sector_count = regbuf[ 0];
      out.out_regs.lba_mid      = regbuf[ 4];
      out.out_regs.lba_low      = regbuf[ 6];
      out.out_regs.device       = regbuf[ 9];
      out.out_regs.lba_high     = regbuf[10];
      out.out_regs.error        = regbuf[13];
      out.out_regs.status       = regbuf[14];
    }
#endif
  }

  return true;
}

bool usbjmicron_device::get_registers(unsigned short addr,
                                      unsigned char * buf, unsigned short size)
{
  unsigned char cdb[14];
  cdb[ 0] = 0xdf;
  cdb[ 1] = 0x10;
  cdb[ 2] = 0x00;
  cdb[ 3] = (unsigned char)(size >> 8);
  cdb[ 4] = (unsigned char)(size     );
  cdb[ 5] = 0x00;
  cdb[ 6] = (unsigned char)(addr >> 8);
  cdb[ 7] = (unsigned char)(addr     );
  cdb[ 8] = 0x00;
  cdb[ 9] = 0x00;
  cdb[10] = 0x00;
  cdb[11] = 0xfd;
  // Prolific PL3507
  cdb[12] = 0x06;
  cdb[13] = 0x7b;

  scsi_cmnd_io io_hdr;
  memset(&io_hdr, 0, sizeof(io_hdr));
  io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
  io_hdr.dxfer_len = size;
  io_hdr.dxferp = buf;
  io_hdr.cmnd = cdb;
  io_hdr.cmnd_len = sizeof(cdb);
  io_hdr.cmnd_len = (!m_prolific ? 12 : 14);

  scsi_device * scsidev = get_tunnel_dev();
  if (!scsi_pass_through_and_check(scsidev, &io_hdr,
         "usbjmicron_device::get_registers: "))
    return set_err(scsidev->get_err());

  return true;
}


/////////////////////////////////////////////////////////////////////////////

/// Prolific USB Bridge support. (PL2773) (Probably works on PL2771 also...)

class usbprolific_device
: public tunnelled_device<
    /*implements*/ ata_device,
    /*by tunnelling through a*/ scsi_device
  >
{
public:
  usbprolific_device(smart_interface * intf, scsi_device * scsidev,
                    const char * req_type);

  virtual ~usbprolific_device() throw();

  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);
};


usbprolific_device::usbprolific_device(smart_interface * intf, scsi_device * scsidev,
                                     const char * req_type)
: smart_device(intf, scsidev->get_dev_name(), "usbprolific", req_type),
  tunnelled_device<ata_device, scsi_device>(scsidev)
{
  set_info().info_name = strprintf("%s [USB Prolific]", scsidev->get_info_name());
}

usbprolific_device::~usbprolific_device() throw()
{
}

bool usbprolific_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  if (!ata_cmd_is_supported(in,
    ata_device::supports_data_out |
    ata_device::supports_48bit_hi_null |
    ata_device::supports_output_regs |
    ata_device::supports_smart_status,
    "Prolific" )
  )
    return false;

  scsi_cmnd_io io_hdr;
  memset(&io_hdr, 0, sizeof(io_hdr));
  unsigned char cmd_rw = 0x10;  // Read

  switch (in.direction) {
    case ata_cmd_in::no_data:
      io_hdr.dxfer_dir = DXFER_NONE;
      break;
    case ata_cmd_in::data_in:
      io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
      io_hdr.dxfer_len = in.size;
      io_hdr.dxferp = (unsigned char *)in.buffer;
      memset(in.buffer, 0, in.size);
      break;
    case ata_cmd_in::data_out:
      io_hdr.dxfer_dir = DXFER_TO_DEVICE;
      io_hdr.dxfer_len = in.size;
      io_hdr.dxferp = (unsigned char *)in.buffer;
      cmd_rw = 0x0; // Write
      break;
    default:
      return set_err(EINVAL);
  }

  // Based on reverse engineering of iSmart.exe with API Monitor.
  // Seen commands:
  // D0  0 0  0 06 7B 0 0 0 0 0 0                 // Read Firmware info?, reads 16 bytes
  // F4  0 0  0 06 7B                             // ??
  // D8 15 0 D8 06 7B 0 0 0 0 1 1 4F C2 A0 B0     // SMART Enable
  // D8 15 0 D0 06 7B 0 0 2 0 1 1 4F C2 A0 B0     // SMART Read values
  // D8 15 0 D1 06 7B 0 0 2 0 1 1 4F C2 A0 B0     // SMART Read thresholds
  // D8 15 0 D4 06 7B 0 0 0 0 0 1 4F C2 A0 B0     // SMART Execute self test
  // D7  0 0  0 06 7B 0 0 0 0 0 0 0 0 0 0         // Read status registers, Reads 16 bytes of data
  // Additional DATA OUT support based on document from Prolific

  // Build pass through command
  unsigned char cdb[16];
  cdb[ 0] = 0xD8;         // Operation Code (D8 = Prolific ATA pass through)
  cdb[ 1] = cmd_rw|0x5;   // Read(0x10)/Write(0x0) | NORMAL(0x5)/PREFIX(0x0)(?)
  cdb[ 2] = 0x0;          // Reserved
  cdb[ 3] = in.in_regs.features;        // Feature register (SMART command)
  cdb[ 4] = 0x06;         // Check Word (VendorID magic, Prolific: 0x067B)
  cdb[ 5] = 0x7B;         // Check Word (VendorID magic, Prolific: 0x067B)
  cdb[ 6] = (unsigned char)(io_hdr.dxfer_len >> 24);  // Length MSB
  cdb[ 7] = (unsigned char)(io_hdr.dxfer_len >> 16);  // Length ...
  cdb[ 8] = (unsigned char)(io_hdr.dxfer_len >>  8);  // Length ...
  cdb[ 9] = (unsigned char)(io_hdr.dxfer_len      );  // Length LSB
  cdb[10] = in.in_regs.sector_count;    // Sector Count
  cdb[11] = in.in_regs.lba_low;         // LBA Low (7:0)
  cdb[12] = in.in_regs.lba_mid;         // LBA Mid (15:8)
  cdb[13] = in.in_regs.lba_high;        // LBA High (23:16)
  cdb[14] = in.in_regs.device | 0xA0;   // Device/Head
  cdb[15] = in.in_regs.command;         // ATA Command Register (only PIO supported)
  // Use '-r scsiioctl,1' to print CDB for debug purposes

  io_hdr.cmnd = cdb;
  io_hdr.cmnd_len = 16;

  scsi_device * scsidev = get_tunnel_dev();
  if (!scsi_pass_through_and_check(scsidev, &io_hdr,
         "usbprolific_device::ata_pass_through: "))
    return set_err(scsidev->get_err());

  if (in.out_needed.is_set()) {
    // Read ATA output registers
    unsigned char regbuf[16] = {0, };
    memset(&io_hdr, 0, sizeof(io_hdr));
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = sizeof(regbuf);
    io_hdr.dxferp = regbuf;

    memset(cdb, 0, sizeof(cdb));
    cdb[ 0] = 0xD7;  // Prolific read registers
    cdb[ 4] = 0x06;  // Check Word (VendorID magic, Prolific: 0x067B)
    cdb[ 5] = 0x7B;  // Check Word (VendorID magic, Prolific: 0x067B)
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);

    if (!scsi_pass_through_and_check(scsidev, &io_hdr,
           "usbprolific_device::scsi_pass_through (get registers): "))
      return set_err(scsidev->get_err());

    // Use '-r scsiioctl,2' to print input registers for debug purposes
    // Example: 50 00 00 00 00 01 4f 00  c2 00 a0 da 00 b0 00 50
    out.out_regs.status       = regbuf[0];  // Status
    out.out_regs.error        = regbuf[1];  // Error
    out.out_regs.sector_count = regbuf[2];  // Sector Count (7:0)
    out.out_regs.lba_low      = regbuf[4];  // LBA Low (7:0)
    out.out_regs.lba_mid      = regbuf[6];  // LBA Mid (7:0)
    out.out_regs.lba_high     = regbuf[8];  // LBA High (7:0)
    out.out_regs.device       = regbuf[10]; // Device/Head
    //                          = regbuf[11]; // ATA Feature (7:0)
    //                          = regbuf[13]; // ATA Command
  }

  return true;
}


/////////////////////////////////////////////////////////////////////////////

/// SunplusIT USB Bridge support.

class usbsunplus_device
: public tunnelled_device<
    /*implements*/ ata_device,
    /*by tunnelling through a*/ scsi_device
  >
{
public:
  usbsunplus_device(smart_interface * intf, scsi_device * scsidev,
                    const char * req_type);

  virtual ~usbsunplus_device() throw();

  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);
};


usbsunplus_device::usbsunplus_device(smart_interface * intf, scsi_device * scsidev,
                                     const char * req_type)
: smart_device(intf, scsidev->get_dev_name(), "usbsunplus", req_type),
  tunnelled_device<ata_device, scsi_device>(scsidev)
{
  set_info().info_name = strprintf("%s [USB Sunplus]", scsidev->get_info_name());
}

usbsunplus_device::~usbsunplus_device() throw()
{
}

bool usbsunplus_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  if (!ata_cmd_is_supported(in,
    ata_device::supports_data_out |
    ata_device::supports_output_regs |
    ata_device::supports_48bit,
    "Sunplus")
  )
    return false;

  scsi_cmnd_io io_hdr;
  unsigned char cdb[12];

  if (in.in_regs.is_48bit_cmd()) {
    // Set "previous" registers
    memset(&io_hdr, 0, sizeof(io_hdr));
    io_hdr.dxfer_dir = DXFER_NONE;

    cdb[ 0] = 0xf8;
    cdb[ 1] = 0x00;
    cdb[ 2] = 0x23; // Subcommand: Pass through presetting
    cdb[ 3] = 0x00;
    cdb[ 4] = 0x00;
    cdb[ 5] = in.in_regs.prev.features;
    cdb[ 6] = in.in_regs.prev.sector_count;
    cdb[ 7] = in.in_regs.prev.lba_low;
    cdb[ 8] = in.in_regs.prev.lba_mid;
    cdb[ 9] = in.in_regs.prev.lba_high;
    cdb[10] = 0x00;
    cdb[11] = 0x00;

    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);

    scsi_device * scsidev = get_tunnel_dev();
    if (!scsi_pass_through_and_check(scsidev, &io_hdr,
           "usbsunplus_device::scsi_pass_through (presetting): "))
      return set_err(scsidev->get_err());
  }

  // Run Pass through command
  memset(&io_hdr, 0, sizeof(io_hdr));
  unsigned char protocol;
  switch (in.direction) {
    case ata_cmd_in::no_data:
      io_hdr.dxfer_dir = DXFER_NONE;
      protocol = 0x00;
      break;
    case ata_cmd_in::data_in:
      io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
      io_hdr.dxfer_len = in.size;
      io_hdr.dxferp = (unsigned char *)in.buffer;
      memset(in.buffer, 0, in.size);
      protocol = 0x10;
      break;
    case ata_cmd_in::data_out:
      io_hdr.dxfer_dir = DXFER_TO_DEVICE;
      io_hdr.dxfer_len = in.size;
      io_hdr.dxferp = (unsigned char *)in.buffer;
      protocol = 0x11;
      break;
    default:
      return set_err(EINVAL);
  }

  cdb[ 0] = 0xf8;
  cdb[ 1] = 0x00;
  cdb[ 2] = 0x22; // Subcommand: Pass through
  cdb[ 3] = protocol;
  cdb[ 4] = (unsigned char)(io_hdr.dxfer_len >> 9);
  cdb[ 5] = in.in_regs.features;
  cdb[ 6] = in.in_regs.sector_count;
  cdb[ 7] = in.in_regs.lba_low;
  cdb[ 8] = in.in_regs.lba_mid;
  cdb[ 9] = in.in_regs.lba_high;
  cdb[10] = in.in_regs.device | 0xa0;
  cdb[11] = in.in_regs.command;

  io_hdr.cmnd = cdb;
  io_hdr.cmnd_len = sizeof(cdb);

  scsi_device * scsidev = get_tunnel_dev();
  if (!scsi_pass_through_and_check(scsidev, &io_hdr,
         "usbsunplus_device::scsi_pass_through: "))
    // Returns sense key 0x03 (medium error) on ATA command error
    return set_err(scsidev->get_err());

  if (in.out_needed.is_set()) {
    // Read ATA output registers
    unsigned char regbuf[8] = {0, };
    memset(&io_hdr, 0, sizeof(io_hdr));
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = sizeof(regbuf);
    io_hdr.dxferp = regbuf;

    cdb[ 0] = 0xf8;
    cdb[ 1] = 0x00;
    cdb[ 2] = 0x21; // Subcommand: Get status
    memset(cdb+3, 0, sizeof(cdb)-3);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);

    if (!scsi_pass_through_and_check(scsidev, &io_hdr,
           "usbsunplus_device::scsi_pass_through (get registers): "))
      return set_err(scsidev->get_err());

    out.out_regs.error        = regbuf[1];
    out.out_regs.sector_count = regbuf[2];
    out.out_regs.lba_low      = regbuf[3];
    out.out_regs.lba_mid      = regbuf[4];
    out.out_regs.lba_high     = regbuf[5];
    out.out_regs.device       = regbuf[6];
    out.out_regs.status       = regbuf[7];
  }

  return true;
}


} // namespace

using namespace sat;


/////////////////////////////////////////////////////////////////////////////

// Return ATA->SCSI filter for SAT or USB.

ata_device * smart_interface::get_sat_device(const char * type, scsi_device * scsidev)
{
  if (!strncmp(type, "sat", 3)) {
    const char * t = type + 3;
    bool enable_auto = false;
    if (!strncmp(t, ",auto", 5)) {
      t += 5;
      enable_auto = true;
    }
    int ptlen = 0, n = -1;
    if (*t && !(sscanf(t, ",%d%n", &ptlen, &n) == 1 && n == (int)strlen(t)
                && (ptlen == 0 || ptlen == 12 || ptlen == 16))) {
      set_err(EINVAL, "Option '-d sat[,auto][,N]' requires N to be 0, 12 or 16");
      return 0;
    }
    return new sat_device(this, scsidev, type, ptlen, enable_auto);
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

  else if (!strncmp(type, "usbjmicron", 10)) {
    const char * t = type + 10;
    bool prolific = false;
    if (!strncmp(t, ",p", 2)) {
      t += 2;
      prolific = true;
    }
    bool ata_48bit_support = false;
    if (!strncmp(t, ",x", 2)) {
      t += 2;
      ata_48bit_support = true;
    }
    int port = -1, n = -1;
    if (*t && !(  (sscanf(t, ",%d%n", &port, &n) == 1
                && n == (int)strlen(t) && 0 <= port && port <= 1))) {
      set_err(EINVAL, "Option '-d usbjmicron[,p][,x],<n>' requires <n> to be 0 or 1");
      return 0;
    }
    return new usbjmicron_device(this, scsidev, type, prolific, ata_48bit_support, port);
  }

  else if (!strcmp(type, "usbprolific")) {
    return new usbprolific_device(this, scsidev, type);
  }

  else if (!strcmp(type, "usbsunplus")) {
    return new usbsunplus_device(this, scsidev, type);
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

  // SAT ?
  if (inqdata && inqsize >= 36 && !memcmp(inqdata + 8, "ATA     ", 8)) { // TODO: Linux-specific?
    ata_device_auto_ptr atadev( new sat_device(this, scsidev, "") , scsidev);
    if (has_sat_pass_through(atadev.get()))
      return atadev.release(); // Detected SAT
  }

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// USB device type detection

// Format USB ID for error messages
static std::string format_usb_id(int vendor_id, int product_id, int version)
{
  if (version >= 0)
    return strprintf("[0x%04x:0x%04x (0x%03x)]", vendor_id, product_id, version);
  else
    return strprintf("[0x%04x:0x%04x]", vendor_id, product_id);
}

// Get type name for USB device with known VENDOR:PRODUCT ID.
const char * smart_interface::get_usb_dev_type_by_id(int vendor_id, int product_id,
                                                     int version /*= -1*/)
{
  usb_dev_info info, info2;
  int n = lookup_usb_device(vendor_id, product_id, version, info, info2);

  if (n <= 0) {
    set_err(EINVAL, "Unknown USB bridge %s",
            format_usb_id(vendor_id, product_id, version).c_str());
    return 0;
  }

  if (n > 1) {
    set_err(EINVAL, "USB bridge %s type is ambiguous: '%s' or '%s'",
            format_usb_id(vendor_id, product_id, version).c_str(),
            (!info.usb_type.empty()  ? info.usb_type.c_str()  : "[unsupported]"),
            (!info2.usb_type.empty() ? info2.usb_type.c_str() : "[unsupported]"));
    return 0;
  }

  if (info.usb_type.empty()) {
    set_err(ENOSYS, "Unsupported USB bridge %s",
            format_usb_id(vendor_id, product_id, version).c_str());
    return 0;
  }

  // TODO: change return type to std::string
  static std::string type;
  type = info.usb_type;
  return type.c_str();
}
