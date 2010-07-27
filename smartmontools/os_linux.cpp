/*
 *  os_linux.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003-10 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2003-10 Doug Gilbert <dgilbert@interlog.com>
 * Copyright (C) 2008    Hank Wu <hank@areca.com.tw>
 * Copyright (C) 2008    Oliver Bock <brevilo@users.sourceforge.net>
 * Copyright (C) 2008-10 Christian Franke <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2008    Jordan Hargrave <jordan_hargrave@dell.com>
 *
 *  Parts of this file are derived from code that was
 *
 *  Written By: Adam Radford <linux@3ware.com>
 *  Modifications By: Joel Jacobson <linux@3ware.com>
 *                   Arnaldo Carvalho de Melo <acme@conectiva.com.br>
 *                    Brad Strand <linux@3ware.com>
 *
 *  Copyright (C) 1999-2003 3ware Inc.
 *
 *  Kernel compatablity By:     Andre Hedrick <andre@suse.com>
 *  Non-Copyright (C) 2000      Andre Hedrick <andre@suse.com>
 *
 * Other ars of this file are derived from code that was
 *
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2000 Andre Hedrick <andre@linux-ide.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

// This file contains the linux-specific IOCTL parts of
// smartmontools. It includes one interface routine for ATA devices,
// one for SCSI devices, and one for ATA devices behind escalade
// controllers.

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <glob.h>

#include <scsi/scsi.h>
#include <scsi/scsi_ioctl.h>
#include <scsi/sg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>
#include <sys/uio.h>
#include <sys/types.h>
#ifndef makedev // old versions of types.h do not include sysmacros.h
#include <sys/sysmacros.h>
#endif
#ifdef WITH_SELINUX
#include <selinux/selinux.h>
#endif

#include "int64.h"
#include "atacmds.h"
#include "extern.h"
#include "os_linux.h"
#include "scsicmds.h"
#include "utility.h"
#include "extern.h"
#include "cciss.h"
#include "megaraid.h"

#include "dev_interface.h"
#include "dev_ata_cmd_set.h"

#ifndef ENOTSUP
#define ENOTSUP ENOSYS
#endif

#define ARGUSED(x) ((void)(x))

const char *os_XXXX_c_cvsid="$Id$" \
ATACMDS_H_CVSID CONFIG_H_CVSID INT64_H_CVSID OS_LINUX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

/* for passing global control variables */
// (con->reportscsiioctl only)
extern smartmonctrl *con;


namespace os_linux { // No need to publish anything, name provided for Doxygen

/////////////////////////////////////////////////////////////////////////////
/// Shared open/close routines

class linux_smart_device
: virtual public /*implements*/ smart_device
{
public:
  explicit linux_smart_device(int flags, int retry_flags = -1)
    : smart_device(never_called),
      m_fd(-1),
      m_flags(flags), m_retry_flags(retry_flags)
      { }

  virtual ~linux_smart_device() throw();

  virtual bool is_open() const;

  virtual bool open();

  virtual bool close();

protected:
  /// Return filedesc for derived classes.
  int get_fd() const
    { return m_fd; }

private:
  int m_fd; ///< filedesc, -1 if not open.
  int m_flags; ///< Flags for ::open()
  int m_retry_flags; ///< Flags to retry ::open(), -1 if no retry
};


linux_smart_device::~linux_smart_device() throw()
{
  if (m_fd >= 0)
    ::close(m_fd);
}

bool linux_smart_device::is_open() const
{
  return (m_fd >= 0);
}

bool linux_smart_device::open()
{
  m_fd = ::open(get_dev_name(), m_flags);

  if (m_fd < 0 && errno == EROFS && m_retry_flags != -1)
    // Retry
    m_fd = ::open(get_dev_name(), m_retry_flags);

  if (m_fd < 0) {
    if (errno == EBUSY && (m_flags & O_EXCL))
      // device is locked
      return set_err(EBUSY,
        "The requested controller is used exclusively by another process!\n"
        "(e.g. smartctl or smartd)\n"
        "Please quit the impeding process or try again later...");
    return set_err((errno==ENOENT || errno==ENOTDIR) ? ENODEV : errno);
  }

  if (m_fd >= 0) {
    // sets FD_CLOEXEC on the opened device file descriptor.  The
    // descriptor is otherwise leaked to other applications (mail
    // sender) which may be considered a security risk and may result
    // in AVC messages on SELinux-enabled systems.
    if (-1 == fcntl(m_fd, F_SETFD, FD_CLOEXEC))
      // TODO: Provide an error printing routine in class smart_interface
      pout("fcntl(set  FD_CLOEXEC) failed, errno=%d [%s]\n", errno, strerror(errno));
  }

  return true;
}

// equivalent to close(file descriptor)
bool linux_smart_device::close()
{
  int fd = m_fd; m_fd = -1;
  if (::close(fd) < 0)
    return set_err(errno);
  return true;
}

// examples for smartctl
static const char  smartctl_examples[] =
		  "=================================================== SMARTCTL EXAMPLES =====\n\n"
		  "  smartctl --all /dev/hda                    (Prints all SMART information)\n\n"
		  "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/hda\n"
		  "                                              (Enables SMART on first disk)\n\n"
		  "  smartctl --test=long /dev/hda          (Executes extended disk self-test)\n\n"
		  "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/hda\n"
		  "                                      (Prints Self-Test & Attribute errors)\n"
		  "  smartctl --all --device=3ware,2 /dev/sda\n"
		  "  smartctl --all --device=3ware,2 /dev/twe0\n"
		  "  smartctl --all --device=3ware,2 /dev/twa0\n"
		  "  smartctl --all --device=3ware,2 /dev/twl0\n"
		  "          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
		  "  smartctl --all --device=hpt,1/1/3 /dev/sda\n"
		  "          (Prints all SMART info for the SATA disk attached to the 3rd PMPort\n"
		  "           of the 1st channel on the 1st HighPoint RAID controller)\n"
		  "  smartctl --all --device=areca,3 /dev/sg2\n"
		  "          (Prints all SMART info for 3rd ATA disk on Areca RAID controller)\n"
  ;


/////////////////////////////////////////////////////////////////////////////
/// Linux ATA support

class linux_ata_device
: public /*implements*/ ata_device_with_command_set,
  public /*extends*/ linux_smart_device
{
public:
  linux_ata_device(smart_interface * intf, const char * dev_name, const char * req_type);

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);
};

linux_ata_device::linux_ata_device(smart_interface * intf, const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "ata", req_type),
  linux_smart_device(O_RDONLY | O_NONBLOCK)
{
}

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


#define BUFFER_LENGTH (4+512)

int linux_ata_device::ata_command_interface(smart_command_set command, int select, char * data)
{
  unsigned char buff[BUFFER_LENGTH];
  // positive: bytes to write to caller.  negative: bytes to READ from
  // caller. zero: non-data command
  int copydata=0;

  const int HDIO_DRIVE_CMD_OFFSET = 4;

  // See struct hd_drive_cmd_hdr in hdreg.h.  Before calling ioctl()
  // buff[0]: ATA COMMAND CODE REGISTER
  // buff[1]: ATA SECTOR NUMBER REGISTER == LBA LOW REGISTER
  // buff[2]: ATA FEATURES REGISTER
  // buff[3]: ATA SECTOR COUNT REGISTER

  // Note that on return:
  // buff[2] contains the ATA SECTOR COUNT REGISTER

  // clear out buff.  Large enough for HDIO_DRIVE_CMD (4+512 bytes)
  memset(buff, 0, BUFFER_LENGTH);

  buff[0]=ATA_SMART_CMD;
  switch (command){
  case CHECK_POWER_MODE:
    buff[0]=ATA_CHECK_POWER_MODE;
    copydata=1;
    break;
  case READ_VALUES:
    buff[2]=ATA_SMART_READ_VALUES;
    buff[3]=1;
    copydata=512;
    break;
  case READ_THRESHOLDS:
    buff[2]=ATA_SMART_READ_THRESHOLDS;
    buff[1]=buff[3]=1;
    copydata=512;
    break;
  case READ_LOG:
    buff[2]=ATA_SMART_READ_LOG_SECTOR;
    buff[1]=select;
    buff[3]=1;
    copydata=512;
    break;
  case WRITE_LOG:
    break;
  case IDENTIFY:
    buff[0]=ATA_IDENTIFY_DEVICE;
    buff[3]=1;
    copydata=512;
    break;
  case PIDENTIFY:
    buff[0]=ATA_IDENTIFY_PACKET_DEVICE;
    buff[3]=1;
    copydata=512;
    break;
  case ENABLE:
    buff[2]=ATA_SMART_ENABLE;
    buff[1]=1;
    break;
  case DISABLE:
    buff[2]=ATA_SMART_DISABLE;
    buff[1]=1;
    break;
  case STATUS:
    // this command only says if SMART is working.  It could be
    // replaced with STATUS_CHECK below.
    buff[2]=ATA_SMART_STATUS;
    break;
  case AUTO_OFFLINE:
    // NOTE: According to ATAPI 4 and UP, this command is obsolete
    // select == 241 for enable but no data transfer.  Use TASK ioctl.
    buff[1]=ATA_SMART_AUTO_OFFLINE;
    buff[2]=select;
    break;
  case AUTOSAVE:
    // select == 248 for enable but no data transfer.  Use TASK ioctl.
    buff[1]=ATA_SMART_AUTOSAVE;
    buff[2]=select;
    break;
  case IMMEDIATE_OFFLINE:
    buff[2]=ATA_SMART_IMMEDIATE_OFFLINE;
    buff[1]=select;
    break;
  case STATUS_CHECK:
    // This command uses HDIO_DRIVE_TASK and has different syntax than
    // the other commands.
    buff[1]=ATA_SMART_STATUS;
    break;
  default:
    pout("Unrecognized command %d in linux_ata_command_interface()\n"
         "Please contact " PACKAGE_BUGREPORT "\n", command);
    errno=ENOSYS;
    return -1;
  }

  // This command uses the HDIO_DRIVE_TASKFILE ioctl(). This is the
  // only ioctl() that can be used to WRITE data to the disk.
  if (command==WRITE_LOG) {
    unsigned char task[sizeof(ide_task_request_t)+512];
    ide_task_request_t *reqtask=(ide_task_request_t *) task;
    task_struct_t      *taskfile=(task_struct_t *) reqtask->io_ports;
    int retval;

    memset(task,      0, sizeof(task));

    taskfile->data           = 0;
    taskfile->feature        = ATA_SMART_WRITE_LOG_SECTOR;
    taskfile->sector_count   = 1;
    taskfile->sector_number  = select;
    taskfile->low_cylinder   = 0x4f;
    taskfile->high_cylinder  = 0xc2;
    taskfile->device_head    = 0;
    taskfile->command        = ATA_SMART_CMD;

    reqtask->data_phase      = TASKFILE_OUT;
    reqtask->req_cmd         = IDE_DRIVE_TASK_OUT;
    reqtask->out_size        = 512;
    reqtask->in_size         = 0;

    // copy user data into the task request structure
    memcpy(task+sizeof(ide_task_request_t), data, 512);

    if ((retval=ioctl(get_fd(), HDIO_DRIVE_TASKFILE, task))) {
      if (retval==-EINVAL)
        pout("Kernel lacks HDIO_DRIVE_TASKFILE support; compile kernel with CONFIG_IDE_TASKFILE_IO set\n");
      return -1;
    }
    return 0;
  }

  // There are two different types of ioctls().  The HDIO_DRIVE_TASK
  // one is this:
  if (command==STATUS_CHECK || command==AUTOSAVE || command==AUTO_OFFLINE){
    int retval;

    // NOT DOCUMENTED in /usr/src/linux/include/linux/hdreg.h. You
    // have to read the IDE driver source code.  Sigh.
    // buff[0]: ATA COMMAND CODE REGISTER
    // buff[1]: ATA FEATURES REGISTER
    // buff[2]: ATA SECTOR_COUNT
    // buff[3]: ATA SECTOR NUMBER
    // buff[4]: ATA CYL LO REGISTER
    // buff[5]: ATA CYL HI REGISTER
    // buff[6]: ATA DEVICE HEAD

    unsigned const char normal_lo=0x4f, normal_hi=0xc2;
    unsigned const char failed_lo=0xf4, failed_hi=0x2c;
    buff[4]=normal_lo;
    buff[5]=normal_hi;

    if ((retval=ioctl(get_fd(), HDIO_DRIVE_TASK, buff))) {
      if (retval==-EINVAL) {
        pout("Error SMART Status command via HDIO_DRIVE_TASK failed");
        pout("Rebuild older linux 2.2 kernels with HDIO_DRIVE_TASK support added\n");
      }
      else
        syserror("Error SMART Status command failed");
      return -1;
    }

    // Cyl low and Cyl high unchanged means "Good SMART status"
    if (buff[4]==normal_lo && buff[5]==normal_hi)
      return 0;

    // These values mean "Bad SMART status"
    if (buff[4]==failed_lo && buff[5]==failed_hi)
      return 1;

    // We haven't gotten output that makes sense; print out some debugging info
    syserror("Error SMART Status command failed");
    pout("Please get assistance from " PACKAGE_HOMEPAGE "\n");
    pout("Register values returned from SMART Status command are:\n");
    pout("ST =0x%02x\n",(int)buff[0]);
    pout("ERR=0x%02x\n",(int)buff[1]);
    pout("NS =0x%02x\n",(int)buff[2]);
    pout("SC =0x%02x\n",(int)buff[3]);
    pout("CL =0x%02x\n",(int)buff[4]);
    pout("CH =0x%02x\n",(int)buff[5]);
    pout("SEL=0x%02x\n",(int)buff[6]);
    return -1;
  }

#if 1
  // Note to people doing ports to other OSes -- don't worry about
  // this block -- you can safely ignore it.  I have put it here
  // because under linux when you do IDENTIFY DEVICE to a packet
  // device, it generates an ugly kernel syslog error message.  This
  // is harmless but frightens users.  So this block detects packet
  // devices and make IDENTIFY DEVICE fail "nicely" without a syslog
  // error message.
  //
  // If you read only the ATA specs, it appears as if a packet device
  // *might* respond to the IDENTIFY DEVICE command.  This is
  // misleading - it's because around the time that SFF-8020 was
  // incorporated into the ATA-3/4 standard, the ATA authors were
  // sloppy. See SFF-8020 and you will see that ATAPI devices have
  // *always* had IDENTIFY PACKET DEVICE as a mandatory part of their
  // command set, and return 'Command Aborted' to IDENTIFY DEVICE.
  if (command==IDENTIFY || command==PIDENTIFY){
    unsigned short deviceid[256];
    // check the device identity, as seen when the system was booted
    // or the device was FIRST registered.  This will not be current
    // if the user has subsequently changed some of the parameters. If
    // device is a packet device, swap the command interpretations.
    if (!ioctl(get_fd(), HDIO_GET_IDENTITY, deviceid) && (deviceid[0] & 0x8000))
      buff[0]=(command==IDENTIFY)?ATA_IDENTIFY_PACKET_DEVICE:ATA_IDENTIFY_DEVICE;
  }
#endif

  // We are now doing the HDIO_DRIVE_CMD type ioctl.
  if ((ioctl(get_fd(), HDIO_DRIVE_CMD, buff)))
    return -1;

  // CHECK POWER MODE command returns information in the Sector Count
  // register (buff[3]).  Copy to return data buffer.
  if (command==CHECK_POWER_MODE)
    buff[HDIO_DRIVE_CMD_OFFSET]=buff[2];

  // if the command returns data then copy it back
  if (copydata)
    memcpy(data, buff+HDIO_DRIVE_CMD_OFFSET, copydata);

  return 0;
}

// >>>>>> Start of general SCSI specific linux code

/* Linux specific code.
 * Historically smartmontools (and smartsuite before it) used the
 * SCSI_IOCTL_SEND_COMMAND ioctl which is available to all linux device
 * nodes that use the SCSI subsystem. A better interface has been available
 * via the SCSI generic (sg) driver but this involves the extra step of
 * mapping disk devices (e.g. /dev/sda) to the corresponding sg device
 * (e.g. /dev/sg2). In the linux kernel 2.6 series most of the facilities of
 * the sg driver have become available via the SG_IO ioctl which is available
 * on all SCSI devices (on SCSI tape devices from lk 2.6.6).
 * So the strategy below is to find out if the SG_IO ioctl is available and
 * if so use it; failing that use the older SCSI_IOCTL_SEND_COMMAND ioctl.
 * Should work in 2.0, 2.2, 2.4 and 2.6 series linux kernels. */

#define MAX_DXFER_LEN 1024      /* can be increased if necessary */
#define SEND_IOCTL_RESP_SENSE_LEN 16    /* ioctl limitation */
#define SG_IO_RESP_SENSE_LEN 64 /* large enough see buffer */
#define LSCSI_DRIVER_MASK  0xf /* mask out "suggestions" */
#define LSCSI_DRIVER_SENSE  0x8 /* alternate CHECK CONDITION indication */
#define LSCSI_DID_ERROR 0x7 /* Need to work around aacraid driver quirk */
#define LSCSI_DRIVER_TIMEOUT  0x6
#define LSCSI_DID_TIME_OUT  0x3
#define LSCSI_DID_BUS_BUSY  0x2
#define LSCSI_DID_NO_CONNECT  0x1

#ifndef SCSI_IOCTL_SEND_COMMAND
#define SCSI_IOCTL_SEND_COMMAND 1
#endif

#define SG_IO_PRESENT_UNKNOWN 0
#define SG_IO_PRESENT_YES 1
#define SG_IO_PRESENT_NO 2

static int sg_io_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report,
                         int unknown);
static int sisc_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report);

static int sg_io_state = SG_IO_PRESENT_UNKNOWN;

/* Preferred implementation for issuing SCSI commands in linux. This
 * function uses the SG_IO ioctl. Return 0 if command issued successfully
 * (various status values should still be checked). If the SCSI command
 * cannot be issued then a negative errno value is returned. */
static int sg_io_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report,
                         int unknown)
{
#ifndef SG_IO
    ARGUSED(dev_fd); ARGUSED(iop); ARGUSED(report);
    return -ENOTTY;
#else
    struct sg_io_hdr io_hdr;

    if (report > 0) {
        int k, j;
        const unsigned char * ucp = iop->cmnd;
        const char * np;
        char buff[256];
        const int sz = (int)sizeof(buff);

        np = scsi_get_opcode_name(ucp[0]);
        j = snprintf(buff, sz, " [%s: ", np ? np : "<unknown opcode>");
        for (k = 0; k < (int)iop->cmnd_len; ++k)
            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "%02x ", ucp[k]);
        if ((report > 1) &&
            (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
            int trunc = (iop->dxfer_len > 256) ? 1 : 0;

            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n  Outgoing "
                          "data, len=%d%s:\n", (int)iop->dxfer_len,
                          (trunc ? " [only first 256 bytes shown]" : ""));
            dStrHex((const char *)iop->dxferp,
                    (trunc ? 256 : iop->dxfer_len) , 1);
        }
        else
            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n");
        pout("%s", buff);
    }
    memset(&io_hdr, 0, sizeof(struct sg_io_hdr));
    io_hdr.interface_id = 'S';
    io_hdr.cmd_len = iop->cmnd_len;
    io_hdr.mx_sb_len = iop->max_sense_len;
    io_hdr.dxfer_len = iop->dxfer_len;
    io_hdr.dxferp = iop->dxferp;
    io_hdr.cmdp = iop->cmnd;
    io_hdr.sbp = iop->sensep;
    /* sg_io_hdr interface timeout has millisecond units. Timeout of 0
       defaults to 60 seconds. */
    io_hdr.timeout = ((0 == iop->timeout) ? 60 : iop->timeout) * 1000;
    switch (iop->dxfer_dir) {
        case DXFER_NONE:
            io_hdr.dxfer_direction = SG_DXFER_NONE;
            break;
        case DXFER_FROM_DEVICE:
            io_hdr.dxfer_direction = SG_DXFER_FROM_DEV;
            break;
        case DXFER_TO_DEVICE:
            io_hdr.dxfer_direction = SG_DXFER_TO_DEV;
            break;
        default:
            pout("do_scsi_cmnd_io: bad dxfer_dir\n");
            return -EINVAL;
    }
    iop->resp_sense_len = 0;
    iop->scsi_status = 0;
    iop->resid = 0;
    if (ioctl(dev_fd, SG_IO, &io_hdr) < 0) {
        if (report && (! unknown))
            pout("  SG_IO ioctl failed, errno=%d [%s]\n", errno,
                 strerror(errno));
        return -errno;
    }
    iop->resid = io_hdr.resid;
    iop->scsi_status = io_hdr.status;
    if (report > 0) {
        pout("  scsi_status=0x%x, host_status=0x%x, driver_status=0x%x\n"
             "  info=0x%x  duration=%d milliseconds  resid=%d\n", io_hdr.status,
             io_hdr.host_status, io_hdr.driver_status, io_hdr.info,
             io_hdr.duration, io_hdr.resid);
        if (report > 1) {
            if (DXFER_FROM_DEVICE == iop->dxfer_dir) {
                int trunc, len;

		len = iop->dxfer_len - iop->resid;
		trunc = (len > 256) ? 1 : 0;
                if (len > 0) {
                    pout("  Incoming data, len=%d%s:\n", len,
                         (trunc ? " [only first 256 bytes shown]" : ""));
                    dStrHex((const char*)iop->dxferp, (trunc ? 256 : len),
                            1);
                } else
                    pout("  Incoming data trimmed to nothing by resid\n");
            }
        }
    }

    if (io_hdr.info | SG_INFO_CHECK) { /* error or warning */
        int masked_driver_status = (LSCSI_DRIVER_MASK & io_hdr.driver_status);

        if (0 != io_hdr.host_status) {
            if ((LSCSI_DID_NO_CONNECT == io_hdr.host_status) ||
                (LSCSI_DID_BUS_BUSY == io_hdr.host_status) ||
                (LSCSI_DID_TIME_OUT == io_hdr.host_status))
                return -ETIMEDOUT;
            else
               /* Check for DID_ERROR - workaround for aacraid driver quirk */
               if (LSCSI_DID_ERROR != io_hdr.host_status) {
                       return -EIO; /* catch all if not DID_ERR */
               }
        }
        if (0 != masked_driver_status) {
            if (LSCSI_DRIVER_TIMEOUT == masked_driver_status)
                return -ETIMEDOUT;
            else if (LSCSI_DRIVER_SENSE != masked_driver_status)
                return -EIO;
        }
        if (LSCSI_DRIVER_SENSE == masked_driver_status)
            iop->scsi_status = SCSI_STATUS_CHECK_CONDITION;
        iop->resp_sense_len = io_hdr.sb_len_wr;
        if ((SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) &&
            iop->sensep && (iop->resp_sense_len > 0)) {
            if (report > 1) {
                pout("  >>> Sense buffer, len=%d:\n",
                     (int)iop->resp_sense_len);
                dStrHex((const char *)iop->sensep, iop->resp_sense_len , 1);
            }
        }
        if (report) {
            if (SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) {
                if ((iop->sensep[0] & 0x7f) > 0x71)
                    pout("  status=%x: [desc] sense_key=%x asc=%x ascq=%x\n",
                         iop->scsi_status, iop->sensep[1] & 0xf,
                         iop->sensep[2], iop->sensep[3]);
                else
                    pout("  status=%x: sense_key=%x asc=%x ascq=%x\n",
                         iop->scsi_status, iop->sensep[2] & 0xf,
                         iop->sensep[12], iop->sensep[13]);
            }
            else
                pout("  status=0x%x\n", iop->scsi_status);
        }
    }
    return 0;
#endif
}

struct linux_ioctl_send_command
{
    int inbufsize;
    int outbufsize;
    UINT8 buff[MAX_DXFER_LEN + 16];
};

/* The Linux SCSI_IOCTL_SEND_COMMAND ioctl is primitive and it doesn't
 * support: CDB length (guesses it from opcode), resid and timeout.
 * Patches in Linux 2.4.21 and 2.5.70 to extend SEND DIAGNOSTIC timeout
 * to 2 hours in order to allow long foreground extended self tests. */
static int sisc_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report)
{
    struct linux_ioctl_send_command wrk;
    int status, buff_offset;
    size_t len;

    memcpy(wrk.buff, iop->cmnd, iop->cmnd_len);
    buff_offset = iop->cmnd_len;
    if (report > 0) {
        int k, j;
        const unsigned char * ucp = iop->cmnd;
        const char * np;
        char buff[256];
        const int sz = (int)sizeof(buff);

        np = scsi_get_opcode_name(ucp[0]);
        j = snprintf(buff, sz, " [%s: ", np ? np : "<unknown opcode>");
        for (k = 0; k < (int)iop->cmnd_len; ++k)
            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "%02x ", ucp[k]);
        if ((report > 1) &&
            (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
            int trunc = (iop->dxfer_len > 256) ? 1 : 0;

            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n  Outgoing "
                          "data, len=%d%s:\n", (int)iop->dxfer_len,
                          (trunc ? " [only first 256 bytes shown]" : ""));
            dStrHex((const char *)iop->dxferp,
                    (trunc ? 256 : iop->dxfer_len) , 1);
        }
        else
            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n");
        pout("%s", buff);
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
    status = ioctl(dev_fd, SCSI_IOCTL_SEND_COMMAND, &wrk);
    if (-1 == status) {
        if (report)
            pout("  SCSI_IOCTL_SEND_COMMAND ioctl failed, errno=%d [%s]\n",
                 errno, strerror(errno));
        return -errno;
    }
    if (0 == status) {
        if (report > 0)
            pout("  status=0\n");
        if (DXFER_FROM_DEVICE == iop->dxfer_dir) {
            memcpy(iop->dxferp, wrk.buff, iop->dxfer_len);
            if (report > 1) {
                int trunc = (iop->dxfer_len > 256) ? 1 : 0;

                pout("  Incoming data, len=%d%s:\n", (int)iop->dxfer_len,
                     (trunc ? " [only first 256 bytes shown]" : ""));
                dStrHex((const char*)iop->dxferp,
                        (trunc ? 256 : iop->dxfer_len) , 1);
            }
        }
        return 0;
    }
    iop->scsi_status = status & 0x7e; /* bits 0 and 7 used to be for vendors */
    if (LSCSI_DRIVER_SENSE == ((status >> 24) & 0xf))
        iop->scsi_status = SCSI_STATUS_CHECK_CONDITION;
    len = (SEND_IOCTL_RESP_SENSE_LEN < iop->max_sense_len) ?
                SEND_IOCTL_RESP_SENSE_LEN : iop->max_sense_len;
    if ((SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) &&
        iop->sensep && (len > 0)) {
        memcpy(iop->sensep, wrk.buff, len);
        iop->resp_sense_len = len;
        if (report > 1) {
            pout("  >>> Sense buffer, len=%d:\n", (int)len);
            dStrHex((const char *)wrk.buff, len , 1);
        }
    }
    if (report) {
        if (SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) {
            pout("  status=%x: sense_key=%x asc=%x ascq=%x\n", status & 0xff,
                 wrk.buff[2] & 0xf, wrk.buff[12], wrk.buff[13]);
        }
        else
            pout("  status=0x%x\n", status);
    }
    if (iop->scsi_status > 0)
        return 0;
    else {
        if (report > 0)
            pout("  ioctl status=0x%x but scsi status=0, fail with EIO\n",
                 status);
        return -EIO;      /* give up, assume no device there */
    }
}

/* SCSI command transmission interface function, linux version.
 * Returns 0 if SCSI command successfully launched and response
 * received. Even when 0 is returned the caller should check
 * scsi_cmnd_io::scsi_status for SCSI defined errors and warnings
 * (e.g. CHECK CONDITION). If the SCSI command could not be issued
 * (e.g. device not present or timeout) or some other problem
 * (e.g. timeout) then returns a negative errno value */
static int do_normal_scsi_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop,
                                  int report)
{
    int res;

    /* implementation relies on static sg_io_state variable. If not
     * previously set tries the SG_IO ioctl. If that succeeds assume
     * that SG_IO ioctl functional. If it fails with an errno value
     * other than ENODEV (no device) or permission then assume
     * SCSI_IOCTL_SEND_COMMAND is the only option. */
    switch (sg_io_state) {
    case SG_IO_PRESENT_UNKNOWN:
        /* ignore report argument */
        if (0 == (res = sg_io_cmnd_io(dev_fd, iop, report, 1))) {
            sg_io_state = SG_IO_PRESENT_YES;
            return 0;
        } else if ((-ENODEV == res) || (-EACCES == res) || (-EPERM == res))
            return res;         /* wait until we see a device */
        sg_io_state = SG_IO_PRESENT_NO;
        /* drop through by design */
    case SG_IO_PRESENT_NO:
        return sisc_cmnd_io(dev_fd, iop, report);
    case SG_IO_PRESENT_YES:
        return sg_io_cmnd_io(dev_fd, iop, report, 0);
    default:
        pout(">>>> do_scsi_cmnd_io: bad sg_io_state=%d\n", sg_io_state);
        sg_io_state = SG_IO_PRESENT_UNKNOWN;
        return -EIO;    /* report error and reset state */
    }
}

// >>>>>> End of general SCSI specific linux code

/////////////////////////////////////////////////////////////////////////////
/// Standard SCSI support

class linux_scsi_device
: public /*implements*/ scsi_device,
  public /*extends*/ linux_smart_device
{
public:
  linux_scsi_device(smart_interface * intf, const char * dev_name,
                    const char * req_type, bool scanning = false);

  virtual smart_device * autodetect_open();

  virtual bool scsi_pass_through(scsi_cmnd_io * iop);

private:
  bool m_scanning; ///< true if created within scan_smart_devices
};

linux_scsi_device::linux_scsi_device(smart_interface * intf,
  const char * dev_name, const char * req_type, bool scanning /*= false*/)
: smart_device(intf, dev_name, "scsi", req_type),
  // If opened with O_RDWR, a SATA disk in standby mode
  // may spin-up after device close().
  linux_smart_device(O_RDONLY | O_NONBLOCK),
  m_scanning(scanning)
{
}


bool linux_scsi_device::scsi_pass_through(scsi_cmnd_io * iop)
{
  int status = do_normal_scsi_cmnd_io(get_fd(), iop, con->reportscsiioctl);
  if (status < 0)
      return set_err(-status);
  return true;
}

/////////////////////////////////////////////////////////////////////////////
/// LSI MegaRAID support

class linux_megaraid_device
: public /* implements */ scsi_device,
  public /* extends */ linux_smart_device
{
public:
  linux_megaraid_device(smart_interface *intf, const char *name, 
    unsigned int bus, unsigned int tgt);

  virtual ~linux_megaraid_device() throw();

  virtual smart_device * autodetect_open();

  virtual bool open();
  virtual bool close();
 
  virtual bool scsi_pass_through(scsi_cmnd_io *iop);

private:
  unsigned int m_disknum;
  unsigned int m_busnum;
  unsigned int m_hba;
  int m_fd;

  bool (linux_megaraid_device::*pt_cmd)(int cdblen, void *cdb, int dataLen, void *data,
    int senseLen, void *sense, int report);
  bool megasas_cmd(int cdbLen, void *cdb, int dataLen, void *data,
    int senseLen, void *sense, int report);
  bool megadev_cmd(int cdbLen, void *cdb, int dataLen, void *data,
    int senseLen, void *sense, int report);
};

linux_megaraid_device::linux_megaraid_device(smart_interface *intf,
  const char *dev_name, unsigned int bus, unsigned int tgt)
 : smart_device(intf, dev_name, "megaraid", "megaraid"),
   linux_smart_device(O_RDWR | O_NONBLOCK),
   m_disknum(tgt), m_busnum(bus), m_hba(0),
   m_fd(-1), pt_cmd(0)
{
  set_info().info_name = strprintf("%s [megaraid_disk_%02d]", dev_name, m_disknum);
}

linux_megaraid_device::~linux_megaraid_device() throw()
{
  if (m_fd >= 0)
    ::close(m_fd);
}

smart_device * linux_megaraid_device::autodetect_open()
{
  int report = con->reportscsiioctl; 

  // Open device
  if (!open())
    return this;

  // The code below is based on smartd.cpp:SCSIFilterKnown()
  if (strcmp(get_req_type(), "megaraid"))
    return this;

  // Get INQUIRY
  unsigned char req_buff[64] = {0, };
  int req_len = 36;
  if (scsiStdInquiry(this, req_buff, req_len)) {
      close();
      set_err(EIO, "INQUIRY failed");
      return this;
  }

  int avail_len = req_buff[4] + 5;
  int len = (avail_len < req_len ? avail_len : req_len);
  if (len < 36)
      return this;

  if (report)
    printf("Got MegaRAID inquiry.. %s\n", req_buff+8);

  // Use INQUIRY to detect type
  {
    // SAT or USB ?
    ata_device * newdev = smi()->autodetect_sat_device(this, req_buff, len);
    if (newdev)
      // NOTE: 'this' is now owned by '*newdev'
      return newdev;
  }

  // Nothing special found
  return this;
}


bool linux_megaraid_device::open()
{
  char line[128];
  int   mjr, n1;
  FILE *fp;
  int report = con->reportscsiioctl; 

  if (!linux_smart_device::open())
    return false;

  /* Get device HBA */
  struct sg_scsi_id sgid;
  if (ioctl(get_fd(), SG_GET_SCSI_ID, &sgid) == 0) {
    m_hba = sgid.host_no;
  }
  else if (ioctl(get_fd(), SCSI_IOCTL_GET_BUS_NUMBER, &m_hba) != 0) {
    int err = errno;
    linux_smart_device::close();
    return set_err(err, "can't get bus number");
  }

  /* Perform mknod of device ioctl node */
  fp = fopen("/proc/devices", "r");
  while (fgets(line, sizeof(line), fp) != NULL) {
  	n1=0;
  	if (sscanf(line, "%d megaraid_sas_ioctl%n", &mjr, &n1) == 1 && n1 == 22) {
	   n1=mknod("/dev/megaraid_sas_ioctl_node", S_IFCHR, makedev(mjr, 0));
	   if(report > 0)
	     printf("Creating /dev/megaraid_sas_ioctl_node = %d\n", n1 >= 0 ? 0 : errno);
	   if (n1 >= 0 || errno == EEXIST)
	      break;
	}
	else if (sscanf(line, "%d megadev%n", &mjr, &n1) == 1 && n1 == 11) {
	   n1=mknod("/dev/megadev0", S_IFCHR, makedev(mjr, 0));
	   if(report > 0)
	     printf("Creating /dev/megadev0 = %d\n", n1 >= 0 ? 0 : errno);
	   if (n1 >= 0 || errno == EEXIST)
	      break;
	}
  }
  fclose(fp);

  /* Open Device IOCTL node */
  if ((m_fd = ::open("/dev/megaraid_sas_ioctl_node", O_RDWR)) >= 0) {
    pt_cmd = &linux_megaraid_device::megasas_cmd;
  }
  else if ((m_fd = ::open("/dev/megadev0", O_RDWR)) >= 0) {
    pt_cmd = &linux_megaraid_device::megadev_cmd;
  }
  else {
    int err = errno;
    linux_smart_device::close();
    return set_err(err, "cannot open /dev/megaraid_sas_ioctl_node or /dev/megadev0");
  }

  return true;
}

bool linux_megaraid_device::close()
{
  if (m_fd >= 0)
    ::close(m_fd);
  m_fd = -1; m_hba = 0; pt_cmd = 0;
  return linux_smart_device::close();
}

bool linux_megaraid_device::scsi_pass_through(scsi_cmnd_io *iop)
{
  int report = con->reportscsiioctl; 

  if (report > 0) {
        int k, j;
        const unsigned char * ucp = iop->cmnd;
        const char * np;
        char buff[256];
        const int sz = (int)sizeof(buff);

        np = scsi_get_opcode_name(ucp[0]);
        j = snprintf(buff, sz, " [%s: ", np ? np : "<unknown opcode>");
        for (k = 0; k < (int)iop->cmnd_len; ++k)
            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "%02x ", ucp[k]);
        if ((report > 1) &&
            (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
            int trunc = (iop->dxfer_len > 256) ? 1 : 0;

            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n  Outgoing "
                          "data, len=%d%s:\n", (int)iop->dxfer_len,
                          (trunc ? " [only first 256 bytes shown]" : ""));
            dStrHex((const char *)iop->dxferp,
                    (trunc ? 256 : iop->dxfer_len) , 1);
        }
        else
            j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n");
        pout("%s", buff);
  }

  /* Controller rejects Enable SMART and Test Unit Ready */
  if (iop->cmnd[0] == 0x00)
    return true;
  if (iop->cmnd[0] == 0x85 && iop->cmnd[1] == 0x06) {
    if(report > 0)
      pout("Rejecting SMART/ATA command to controller\n");
    // Emulate SMART STATUS CHECK drive reply
    // smartctl fail to work without this
    if(iop->cmnd[2]==0x2c) {
      iop->resp_sense_len=22; // copied from real response
      iop->sensep[0]=0x72; // descriptor format
      iop->sensep[7]=0x0e; // additional length
      iop->sensep[8]=0x09; // description pointer
      iop->sensep[17]=0x4f; // low cylinder GOOD smart status
      iop->sensep[19]=0xc2; // high cylinder GOOD smart status
    }
    return true;
  }

  if (pt_cmd == NULL)
    return false;
  return (this->*pt_cmd)(iop->cmnd_len, iop->cmnd, 
    iop->dxfer_len, iop->dxferp,
    iop->max_sense_len, iop->sensep, report);
}

/* Issue passthrough scsi command to PERC5/6 controllers */
bool linux_megaraid_device::megasas_cmd(int cdbLen, void *cdb, 
  int dataLen, void *data,
  int /*senseLen*/, void * /*sense*/, int /*report*/)
{
  struct megasas_pthru_frame	*pthru;
  struct megasas_iocpacket	uio;
  int rc;

  memset(&uio, 0, sizeof(uio));
  pthru = (struct megasas_pthru_frame *)uio.frame.raw;
  pthru->cmd = MFI_CMD_PD_SCSI_IO;
  pthru->cmd_status = 0xFF;
  pthru->scsi_status = 0x0;
  pthru->target_id = m_disknum;
  pthru->lun = 0;
  pthru->cdb_len = cdbLen;
  pthru->timeout = 0;
  pthru->flags = MFI_FRAME_DIR_READ;
  pthru->sge_count = 1;
  pthru->data_xfer_len = dataLen;
  pthru->sgl.sge32[0].phys_addr = (intptr_t)data;
  pthru->sgl.sge32[0].length = (uint32_t)dataLen;
  memcpy(pthru->cdb, cdb, cdbLen);

  uio.host_no = m_hba;
  uio.sge_count = 1;
  uio.sgl_off = offsetof(struct megasas_pthru_frame, sgl);
  uio.sgl[0].iov_base = data;
  uio.sgl[0].iov_len = dataLen;

  rc = 0;
  errno = 0;
  rc = ioctl(m_fd, MEGASAS_IOC_FIRMWARE, &uio);
  if (pthru->cmd_status || rc != 0) {
    if (pthru->cmd_status == 12) {
      return set_err(EIO, "megasas_cmd: Device %d does not exist\n", m_disknum);
    }
    return set_err((errno ? errno : EIO), "megasas_cmd result: %d.%d = %d/%d",
                   m_hba, m_disknum, errno,
                   pthru->cmd_status);
  }
  return true;
}

/* Issue passthrough scsi commands to PERC2/3/4 controllers */
bool linux_megaraid_device::megadev_cmd(int cdbLen, void *cdb, 
  int dataLen, void *data,
  int senseLen, void *sense, int /*report*/)
{
  struct uioctl_t uio;
  int rc;

  sense = NULL;
  senseLen = 0;

  /* Don't issue to the controller */
  if (m_disknum == 7)
    return false;

  memset(&uio, 0, sizeof(uio));
  uio.inlen  = dataLen;
  uio.outlen = dataLen;

  memset(data, 0, dataLen);
  uio.ui.fcs.opcode = 0x80;             // M_RD_IOCTL_CMD
  uio.ui.fcs.adapno = MKADAP(m_hba);

  uio.data.pointer = (uint8_t *)data;

  uio.mbox.cmd = MEGA_MBOXCMD_PASSTHRU;
  uio.mbox.xferaddr = (intptr_t)&uio.pthru;

  uio.pthru.ars     = 1;
  uio.pthru.timeout = 2;
  uio.pthru.channel = 0;
  uio.pthru.target  = m_disknum;
  uio.pthru.cdblen  = cdbLen;
  uio.pthru.reqsenselen  = MAX_REQ_SENSE_LEN;
  uio.pthru.dataxferaddr = (intptr_t)data;
  uio.pthru.dataxferlen  = dataLen;
  memcpy(uio.pthru.cdb, cdb, cdbLen);

  rc=ioctl(m_fd, MEGAIOCCMD, &uio);
  if (uio.pthru.scsistatus || rc != 0) {
    return set_err((errno ? errno : EIO), "megadev_cmd result: %d.%d =  %d/%d",
                   m_hba, m_disknum, errno,
                   uio.pthru.scsistatus);
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////
/// CCISS RAID support

#ifdef HAVE_LINUX_CCISS_IOCTL_H

class linux_cciss_device
: public /*implements*/ scsi_device,
  public /*extends*/ linux_smart_device
{
public:
  linux_cciss_device(smart_interface * intf, const char * name, unsigned char disknum);

  virtual bool scsi_pass_through(scsi_cmnd_io * iop);

private:
  unsigned char m_disknum; ///< Disk number.
};

linux_cciss_device::linux_cciss_device(smart_interface * intf,
  const char * dev_name, unsigned char disknum)
: smart_device(intf, dev_name, "cciss", "cciss"),
  linux_smart_device(O_RDWR | O_NONBLOCK),
  m_disknum(disknum)
{
  set_info().info_name = strprintf("%s [cciss_disk_%02d]", dev_name, disknum);
}

bool linux_cciss_device::scsi_pass_through(scsi_cmnd_io * iop)
{
  int status = cciss_io_interface(get_fd(), m_disknum, iop, con->reportscsiioctl);
  if (status < 0)
      return set_err(-status);
  return true;
}

#endif // HAVE_LINUX_CCISS_IOCTL_H

/////////////////////////////////////////////////////////////////////////////
/// AMCC/3ware RAID support

class linux_escalade_device
: public /*implements*/ ata_device,
  public /*extends*/ linux_smart_device
{
public:
  enum escalade_type_t {
    AMCC_3WARE_678K,
    AMCC_3WARE_678K_CHAR,
    AMCC_3WARE_9000_CHAR,
    AMCC_3WARE_9700_CHAR
  };

  linux_escalade_device(smart_interface * intf, const char * dev_name,
    escalade_type_t escalade_type, int disknum);

  virtual bool open();

  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

private:
  escalade_type_t m_escalade_type; ///< Controller type
  int m_disknum; ///< Disk number.
};

linux_escalade_device::linux_escalade_device(smart_interface * intf, const char * dev_name,
    escalade_type_t escalade_type, int disknum)
: smart_device(intf, dev_name, "3ware", "3ware"),
  linux_smart_device(O_RDONLY | O_NONBLOCK),
  m_escalade_type(escalade_type), m_disknum(disknum)
{
  set_info().info_name = strprintf("%s [3ware_disk_%02d]", dev_name, disknum);
}

/* This function will setup and fix device nodes for a 3ware controller. */
#define MAJOR_STRING_LENGTH 3
#define DEVICE_STRING_LENGTH 32
#define NODE_STRING_LENGTH 16
int setup_3ware_nodes(const char *nodename, const char *driver_name) {
  int              tw_major      = 0;
  int              index         = 0;
  char             majorstring[MAJOR_STRING_LENGTH+1];
  char             device_name[DEVICE_STRING_LENGTH+1];
  char             nodestring[NODE_STRING_LENGTH];
  struct stat      stat_buf;
  FILE             *file;
  int              retval = 0;
#ifdef WITH_SELINUX
  security_context_t orig_context = NULL;
  security_context_t node_context = NULL;
  int                selinux_enabled  = is_selinux_enabled();
  int                selinux_enforced = security_getenforce();
#endif


  /* First try to open up /proc/devices */
  if (!(file = fopen("/proc/devices", "r"))) {
    pout("Error opening /proc/devices to check/create 3ware device nodes\n");
    syserror("fopen");
    return 0;  // don't fail here: user might not have /proc !
  }

  /* Attempt to get device major number */
  while (EOF != fscanf(file, "%3s %32s", majorstring, device_name)) {
    majorstring[MAJOR_STRING_LENGTH]='\0';
    device_name[DEVICE_STRING_LENGTH]='\0';
    if (!strncmp(device_name, nodename, DEVICE_STRING_LENGTH)) {
      tw_major = atoi(majorstring);
      break;
    }
  }
  fclose(file);

  /* See if we found a major device number */
  if (!tw_major) {
    pout("No major number for /dev/%s listed in /proc/devices. Is the %s driver loaded?\n", nodename, driver_name);
    return 2;
  }
#ifdef WITH_SELINUX
  /* Prepare a database of contexts for files in /dev
   * and save the current context */
  if (selinux_enabled) {
    if (matchpathcon_init_prefix(NULL, "/dev") < 0)
      pout("Error initializing contexts database for /dev");
    if (getfscreatecon(&orig_context) < 0) {
      pout("Error retrieving original SELinux fscreate context");
      if (selinux_enforced)
        matchpathcon_fini();
        return 6;
      }
  }
#endif
  /* Now check if nodes are correct */
  for (index=0; index<16; index++) {
    sprintf(nodestring, "/dev/%s%d", nodename, index);
#ifdef WITH_SELINUX
    /* Get context of the node and set it as the default */
    if (selinux_enabled) {
      if (matchpathcon(nodestring, S_IRUSR | S_IWUSR, &node_context) < 0) {
        pout("Could not retrieve context for %s", nodestring);
        if (selinux_enforced) {
          retval = 6;
          break;
        }
      }
      if (setfscreatecon(node_context) < 0) {
        pout ("Error setting default fscreate context");
        if (selinux_enforced) {
          retval = 6;
          break;
        }
      }
    }
#endif
    /* Try to stat the node */
    if ((stat(nodestring, &stat_buf))) {
      pout("Node %s does not exist and must be created. Check the udev rules.\n", nodestring);
      /* Create a new node if it doesn't exist */
      if (mknod(nodestring, S_IFCHR|0600, makedev(tw_major, index))) {
        pout("problem creating 3ware device nodes %s", nodestring);
        syserror("mknod");
        retval = 3;
        break;
      } else {
#ifdef WITH_SELINUX
	if (selinux_enabled && node_context) {
	  freecon(node_context);
	  node_context = NULL;
	}
#endif
        continue;
      }
    }

    /* See if nodes major and minor numbers are correct */
    if ((tw_major != (int)(major(stat_buf.st_rdev))) ||
        (index    != (int)(minor(stat_buf.st_rdev))) ||
        (!S_ISCHR(stat_buf.st_mode))) {
      pout("Node %s has wrong major/minor number and must be created anew."
          " Check the udev rules.\n", nodestring);
      /* Delete the old node */
      if (unlink(nodestring)) {
        pout("problem unlinking stale 3ware device node %s", nodestring);
        syserror("unlink");
        retval = 4;
        break;
      }

      /* Make a new node */
      if (mknod(nodestring, S_IFCHR|0600, makedev(tw_major, index))) {
        pout("problem creating 3ware device nodes %s", nodestring);
        syserror("mknod");
        retval = 5;
        break;
      }
    }
#ifdef WITH_SELINUX
    if (selinux_enabled && node_context) {
      freecon(node_context);
      node_context = NULL;
    }
#endif
  }

#ifdef WITH_SELINUX
  if (selinux_enabled) {
    if(setfscreatecon(orig_context) < 0) {
      pout("Error re-setting original fscreate context");
      if (selinux_enforced)
        retval = 6;
    }
    if(orig_context)
      freecon(orig_context);
    if(node_context)
      freecon(node_context);
    matchpathcon_fini();
  }
#endif
  return retval;
}

bool linux_escalade_device::open()
{
  if (m_escalade_type == AMCC_3WARE_9700_CHAR || m_escalade_type == AMCC_3WARE_9000_CHAR ||
      m_escalade_type == AMCC_3WARE_678K_CHAR) {
    // the device nodes for these controllers are dynamically assigned,
    // so we need to check that they exist with the correct major
    // numbers and if not, create them
    const char * node   = (m_escalade_type == AMCC_3WARE_9700_CHAR ? "twl"     :
                           m_escalade_type == AMCC_3WARE_9000_CHAR ? "twa"     :
                                                                     "twe"      );
    const char * driver = (m_escalade_type == AMCC_3WARE_9700_CHAR ? "3w-sas"  :
                           m_escalade_type == AMCC_3WARE_9000_CHAR ? "3w-9xxx" :
                                                                     "3w-xxxx"  );
    if (setup_3ware_nodes(node, driver))
      return set_err((errno ? errno : ENXIO), "setup_3ware_nodes(\"%s\", \"%s\") failed", node, driver);
  }
  // Continue with default open
  return linux_smart_device::open();
}

// TODO: Function no longer useful
//void printwarning(smart_command_set command);

// PURPOSE
//   This is an interface routine meant to isolate the OS dependent
//   parts of the code, and to provide a debugging interface.  Each
//   different port and OS needs to provide it's own interface.  This
//   is the linux interface to the 3ware 3w-xxxx driver.  It allows ATA
//   commands to be passed through the SCSI driver.
// DETAILED DESCRIPTION OF ARGUMENTS
//   fd: is the file descriptor provided by open()
//   disknum is the disk number (0 to 15) in the RAID array
//   escalade_type indicates the type of controller type, and if scsi or char interface is used
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


/* 512 is the max payload size: increase if needed */
#define BUFFER_LEN_678K      ( sizeof(TW_Ioctl)                  ) // 1044 unpacked, 1041 packed
#define BUFFER_LEN_678K_CHAR ( sizeof(TW_New_Ioctl)+512-1        ) // 1539 unpacked, 1536 packed
#define BUFFER_LEN_9000      ( sizeof(TW_Ioctl_Buf_Apache)+512-1 ) // 2051 unpacked, 2048 packed
#define TW_IOCTL_BUFFER_SIZE ( MAX(MAX(BUFFER_LEN_678K, BUFFER_LEN_9000), BUFFER_LEN_678K_CHAR) )

bool linux_escalade_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  if (!ata_cmd_is_ok(in,
    true, // data_out_support
    false, // TODO: multi_sector_support
    true) // ata_48bit_support
  )
    return false;

  // Used by both the SCSI and char interfaces
  TW_Passthru *passthru=NULL;
  char ioctl_buffer[TW_IOCTL_BUFFER_SIZE];

  // only used for SCSI device interface
  TW_Ioctl   *tw_ioctl=NULL;
  TW_Output *tw_output=NULL;

  // only used for 6000/7000/8000 char device interface
  TW_New_Ioctl *tw_ioctl_char=NULL;

  // only used for 9000 character device interface
  TW_Ioctl_Buf_Apache *tw_ioctl_apache=NULL;

  memset(ioctl_buffer, 0, TW_IOCTL_BUFFER_SIZE);

  // TODO: Handle controller differences by different classes
  if (m_escalade_type == AMCC_3WARE_9700_CHAR || m_escalade_type == AMCC_3WARE_9000_CHAR) {
    tw_ioctl_apache                               = (TW_Ioctl_Buf_Apache *)ioctl_buffer;
    tw_ioctl_apache->driver_command.control_code  = TW_IOCTL_FIRMWARE_PASS_THROUGH;
    tw_ioctl_apache->driver_command.buffer_length = 512; /* payload size */
    passthru                                      = (TW_Passthru *)&(tw_ioctl_apache->firmware_command.command.oldcommand);
  }
  else if (m_escalade_type==AMCC_3WARE_678K_CHAR) {
    tw_ioctl_char                                 = (TW_New_Ioctl *)ioctl_buffer;
    tw_ioctl_char->data_buffer_length             = 512;
    passthru                                      = (TW_Passthru *)&(tw_ioctl_char->firmware_command);
  }
  else if (m_escalade_type==AMCC_3WARE_678K) {
    tw_ioctl                                      = (TW_Ioctl *)ioctl_buffer;
    tw_ioctl->cdb[0]                              = TW_IOCTL;
    tw_ioctl->opcode                              = TW_ATA_PASSTHRU;
    tw_ioctl->input_length                        = 512; // correct even for non-data commands
    tw_ioctl->output_length                       = 512; // correct even for non-data commands
    tw_output                                     = (TW_Output *)tw_ioctl;
    passthru                                      = (TW_Passthru *)&(tw_ioctl->input_data);
  }
  else {
    return set_err(ENOSYS,
      "Unrecognized escalade_type %d in linux_3ware_command_interface(disk %d)\n"
      "Please contact " PACKAGE_BUGREPORT "\n", (int)m_escalade_type, m_disknum);
  }

  // Same for (almost) all commands - but some reset below
  passthru->byte0.opcode  = TW_OP_ATA_PASSTHRU;
  passthru->request_id    = 0xFF;
  passthru->unit          = m_disknum;
  passthru->status        = 0;
  passthru->flags         = 0x1;

  // Set registers
  {
    const ata_in_regs_48bit & r = in.in_regs;
    passthru->features     = r.features_16;
    passthru->sector_count = r.sector_count_16;
    passthru->sector_num   = r.lba_low_16;
    passthru->cylinder_lo  = r.lba_mid_16;
    passthru->cylinder_hi  = r.lba_high_16;
    passthru->drive_head   = r.device;
    passthru->command      = r.command;
  }

  // Is this a command that reads or returns 512 bytes?
  // passthru->param values are:
  // 0x0 - non data command without TFR write check,
  // 0x8 - non data command with TFR write check,
  // 0xD - data command that returns data to host from device
  // 0xF - data command that writes data from host to device
  // passthru->size values are 0x5 for non-data and 0x07 for data
  bool readdata = false;
  if (in.direction == ata_cmd_in::data_in) {
    readdata=true;
    passthru->byte0.sgloff = 0x5;
    passthru->size         = 0x7; // TODO: Other value for multi-sector ?
    passthru->param        = 0xD;
    // For 64-bit to work correctly, up the size of the command packet
    // in dwords by 1 to account for the 64-bit single sgl 'address'
    // field. Note that this doesn't agree with the typedefs but it's
    // right (agree with kernel driver behavior/typedefs).
    if ((m_escalade_type == AMCC_3WARE_9700_CHAR || m_escalade_type == AMCC_3WARE_9000_CHAR)
        && sizeof(long) == 8)
      passthru->size++;
  }
  else if (in.direction == ata_cmd_in::no_data) {
    // Non data command -- but doesn't use large sector
    // count register values.
    passthru->byte0.sgloff = 0x0;
    passthru->size         = 0x5;
    passthru->param        = 0x8;
    passthru->sector_count = 0x0;
  }
  else if (in.direction == ata_cmd_in::data_out) {
    if (m_escalade_type == AMCC_3WARE_9700_CHAR || m_escalade_type == AMCC_3WARE_9000_CHAR)
      memcpy(tw_ioctl_apache->data_buffer, in.buffer, in.size);
    else if (m_escalade_type == AMCC_3WARE_678K_CHAR)
      memcpy(tw_ioctl_char->data_buffer,   in.buffer, in.size);
    else {
      // COMMAND NOT SUPPORTED VIA SCSI IOCTL INTERFACE
      // memcpy(tw_output->output_data, data, 512);
      // printwarning(command); // TODO: Parameter no longer valid
      return set_err(ENOTSUP, "DATA OUT not supported for this 3ware controller type");
    }
    passthru->byte0.sgloff = 0x5;
    passthru->size         = 0x7;  // TODO: Other value for multi-sector ?
    passthru->param        = 0xF;  // PIO data write
    if ((m_escalade_type == AMCC_3WARE_9700_CHAR || m_escalade_type == AMCC_3WARE_9000_CHAR)
        && sizeof(long) == 8)
      passthru->size++;
  }
  else
    return set_err(EINVAL);

  // Now send the command down through an ioctl()
  int ioctlreturn;
  if (m_escalade_type == AMCC_3WARE_9700_CHAR || m_escalade_type == AMCC_3WARE_9000_CHAR)
    ioctlreturn=ioctl(get_fd(), TW_IOCTL_FIRMWARE_PASS_THROUGH, tw_ioctl_apache);
  else if (m_escalade_type==AMCC_3WARE_678K_CHAR)
    ioctlreturn=ioctl(get_fd(), TW_CMD_PACKET_WITH_DATA, tw_ioctl_char);
  else
    ioctlreturn=ioctl(get_fd(), SCSI_IOCTL_SEND_COMMAND, tw_ioctl);

  // Deal with the different error cases
  if (ioctlreturn) {
    if (AMCC_3WARE_678K==m_escalade_type
        && in.in_regs.command==ATA_SMART_CMD
        && (   in.in_regs.features == ATA_SMART_AUTO_OFFLINE
            || in.in_regs.features == ATA_SMART_AUTOSAVE    )
        && in.in_regs.lba_low) {
      // error here is probably a kernel driver whose version is too old
      // printwarning(command); // TODO: Parameter no longer valid
      return set_err(ENOTSUP, "Probably kernel driver too old");
    }
    return set_err(EIO);
  }

  // The passthru structure is valid after return from an ioctl if:
  // - we are using the character interface OR
  // - we are using the SCSI interface and this is a NON-READ-DATA command
  // For SCSI interface, note that we set passthru to a different
  // value after ioctl().
  if (AMCC_3WARE_678K==m_escalade_type) {
    if (readdata)
      passthru=NULL;
    else
      passthru=(TW_Passthru *)&(tw_output->output_data);
  }

  // See if the ATA command failed.  Now that we have returned from
  // the ioctl() call, if passthru is valid, then:
  // - passthru->status contains the 3ware controller STATUS
  // - passthru->command contains the ATA STATUS register
  // - passthru->features contains the ATA ERROR register
  //
  // Check bits 0 (error bit) and 5 (device fault) of the ATA STATUS
  // If bit 0 (error bit) is set, then ATA ERROR register is valid.
  // While we *might* decode the ATA ERROR register, at the moment it
  // doesn't make much sense: we don't care in detail why the error
  // happened.

  if (passthru && (passthru->status || (passthru->command & 0x21))) {
    return set_err(EIO);
  }

  // If this is a read data command, copy data to output buffer
  if (readdata) {
    if (m_escalade_type == AMCC_3WARE_9700_CHAR || m_escalade_type == AMCC_3WARE_9000_CHAR)
      memcpy(in.buffer, tw_ioctl_apache->data_buffer, in.size);
    else if (m_escalade_type==AMCC_3WARE_678K_CHAR)
      memcpy(in.buffer, tw_ioctl_char->data_buffer, in.size);
    else
      memcpy(in.buffer, tw_output->output_data, in.size);
  }

  // Return register values
  if (passthru) {
    ata_out_regs_48bit & r = out.out_regs;
    r.error           = passthru->features;
    r.sector_count_16 = passthru->sector_count;
    r.lba_low_16      = passthru->sector_num;
    r.lba_mid_16      = passthru->cylinder_lo;
    r.lba_high_16     = passthru->cylinder_hi;
    r.device          = passthru->drive_head;
    r.status          = passthru->command;
  }

  // look for nonexistent devices/ports
  if (   in.in_regs.command == ATA_IDENTIFY_DEVICE
      && !nonempty(in.buffer, in.size)) {
    return set_err(ENODEV, "No drive on port %d", m_disknum);
  }

  return true;
}


/////////////////////////////////////////////////////////////////////////////
/// Areca RAID support

class linux_areca_device
: public /*implements*/ ata_device_with_command_set,
  public /*extends*/ linux_smart_device
{
public:
  linux_areca_device(smart_interface * intf, const char * dev_name, int disknum);

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);

private:
  int m_disknum; ///< Disk number.
};


// PURPOSE
//   This is an interface routine meant to isolate the OS dependent
//   parts of the code, and to provide a debugging interface.  Each
//   different port and OS needs to provide it's own interface.  This
//   is the linux interface to the Areca "arcmsr" driver.  It allows ATA
//   commands to be passed through the SCSI driver.
// DETAILED DESCRIPTION OF ARGUMENTS
//   fd: is the file descriptor provided by open()
//   disknum is the disk number (0 to 15) in the RAID array
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


/*DeviceType*/
#define ARECA_SATA_RAID                      	0x90000000
/*FunctionCode*/
#define FUNCTION_READ_RQBUFFER               	0x0801
#define FUNCTION_WRITE_WQBUFFER              	0x0802
#define FUNCTION_CLEAR_RQBUFFER              	0x0803
#define FUNCTION_CLEAR_WQBUFFER              	0x0804

/* ARECA IO CONTROL CODE*/
#define ARCMSR_IOCTL_READ_RQBUFFER           	(ARECA_SATA_RAID | FUNCTION_READ_RQBUFFER)
#define ARCMSR_IOCTL_WRITE_WQBUFFER          	(ARECA_SATA_RAID | FUNCTION_WRITE_WQBUFFER)
#define ARCMSR_IOCTL_CLEAR_RQBUFFER          	(ARECA_SATA_RAID | FUNCTION_CLEAR_RQBUFFER)
#define ARCMSR_IOCTL_CLEAR_WQBUFFER          	(ARECA_SATA_RAID | FUNCTION_CLEAR_WQBUFFER)
#define ARECA_SIG_STR							"ARCMSR"

// The SRB_IO_CONTROL & SRB_BUFFER structures are used to communicate(to/from) to areca driver
typedef struct _SRB_IO_CONTROL
{
	unsigned int HeaderLength;
	unsigned char Signature[8];
	unsigned int Timeout;
	unsigned int ControlCode;
	unsigned int ReturnCode;
	unsigned int Length;
} sSRB_IO_CONTROL;

typedef struct _SRB_BUFFER
{
	sSRB_IO_CONTROL srbioctl;
	unsigned char   ioctldatabuffer[1032]; // the buffer to put the command data to/from firmware
} sSRB_BUFFER;

// Looks in /proc/scsi to suggest correct areca devices
// If hint not NULL, return device path guess
int find_areca_in_proc(char *hint) {
 
    const char* proc_format_string="host\tchan\tid\tlun\ttype\topens\tqdepth\tbusy\tonline\n";

    // check data formwat
    FILE *fp=fopen("/proc/scsi/sg/device_hdr", "r");
    if (!fp) {
        pout("Unable to open /proc/scsi/sg/device_hdr for reading\n");
        return 1;
     }

     // get line, compare to format
     char linebuf[256];
     linebuf[255]='\0';
     char *out = fgets(linebuf, 256, fp);
     fclose(fp);
     if (!out) {
         pout("Unable to read contents of /proc/scsi/sg/device_hdr\n");
         return 2;
     }

     if (strcmp(linebuf, proc_format_string)) {
     	// wrong format!
	// Fix this by comparing only tokens not white space!!
	pout("Unexpected format %s in /proc/scsi/sg/device_hdr\n", proc_format_string);
	return 3;
     }

    // Format is understood, now search for correct device
    fp=fopen("/proc/scsi/sg/devices", "r");
    if (!fp) return 1;
    int host, chan, id, lun, type, opens, qdepth, busy, online;
    int dev=-1;
    int found=0;
    // search all lines of /proc/scsi/sg/devices
    while (9 == fscanf(fp, "%d %d %d %d %d %d %d %d %d", &host, &chan, &id, &lun, &type, &opens, &qdepth, &busy, &online)) {
        dev++;
	if (id == 16 && type == 3) {
	   // devices with id=16 and type=3 might be Areca controllers
	   if (!found && hint) {
	       sprintf(hint, "/dev/sg%d", dev);
	   }
	   pout("Device /dev/sg%d appears to be an Areca controller.\n", dev);
           found++;
        }
    }
    fclose(fp);
    return 0;
}



void dumpdata( unsigned char *block, int len)
{
	int ln = (len / 16) + 1;	 // total line#
	unsigned char c;
	int pos = 0;

	printf(" Address = %p, Length = (0x%x)%d\n", block, len, len);
	printf("      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F      ASCII      \n");
	printf("=====================================================================\n");

	for ( int l = 0; l < ln && len; l++ )
	{
		// printf the line# and the HEX data
		// if a line data length < 16 then append the space to the tail of line to reach 16 chars
		printf("%02X | ", l);
		for ( pos = 0; pos < 16 && len; pos++, len-- )
		{
			c = block[l*16+pos];    
			printf("%02X ", c);
		}

		if ( pos < 16 )
		{
			for ( int loop = pos; loop < 16; loop++ )
			{
				printf("   ");
			}
		}

		// print ASCII char
		for ( int loop = 0; loop < pos; loop++ )
		{
			c = block[l*16+loop];
			if ( c >= 0x20 && c <= 0x7F )
			{
				printf("%c", c);
			}
			else
			{
				printf(".");
			}
		}
		printf("\n");
	}   
	printf("=====================================================================\n");
}



int arcmsr_command_handler(int fd, unsigned long arcmsr_cmd, unsigned char *data, int data_len, void *ext_data /* reserved for further use */)
{
	ARGUSED(ext_data);

	int ioctlreturn = 0;
	sSRB_BUFFER sBuf;
	struct scsi_cmnd_io io_hdr;  
	int dir = DXFER_TO_DEVICE;

	UINT8 cdb[10];
	UINT8 sense[32];

	unsigned char *areca_return_packet;
	int total = 0;
	int expected = -1;
	unsigned char return_buff[2048];
	unsigned char *ptr = &return_buff[0];
	memset(return_buff, 0, sizeof(return_buff));

	memset((unsigned char *)&sBuf, 0, sizeof(sBuf));
	memset(&io_hdr, 0, sizeof(io_hdr));
	memset(cdb, 0, sizeof(cdb));
	memset(sense, 0, sizeof(sense));


	sBuf.srbioctl.HeaderLength = sizeof(sSRB_IO_CONTROL);   
	memcpy(sBuf.srbioctl.Signature, ARECA_SIG_STR, strlen(ARECA_SIG_STR));
	sBuf.srbioctl.Timeout = 10000;      
	sBuf.srbioctl.ControlCode = ARCMSR_IOCTL_READ_RQBUFFER;

	switch ( arcmsr_cmd )
	{
	// command for writing data to driver
	case ARCMSR_IOCTL_WRITE_WQBUFFER:   
		if ( data && data_len )
		{
			sBuf.srbioctl.Length = data_len;    
			memcpy((unsigned char *)sBuf.ioctldatabuffer, (unsigned char *)data, data_len);
		}
		// commands for clearing related buffer of driver
	case ARCMSR_IOCTL_CLEAR_RQBUFFER:
	case ARCMSR_IOCTL_CLEAR_WQBUFFER:
		cdb[0] = 0x3B; //SCSI_WRITE_BUF command;
		break;
		// command for reading data from driver
	case ARCMSR_IOCTL_READ_RQBUFFER:    
		cdb[0] = 0x3C; //SCSI_READ_BUF command;
		dir = DXFER_FROM_DEVICE;
		break;
	default:
		// unknown arcmsr commands
		return -1;
	}

	cdb[1] = 0x01;
	cdb[2] = 0xf0;    
	//
	// cdb[5][6][7][8] areca defined command code( to/from driver )
	//    
	cdb[5] = (char)( arcmsr_cmd >> 24);
	cdb[6] = (char)( arcmsr_cmd >> 16);
	cdb[7] = (char)( arcmsr_cmd >> 8);
	cdb[8] = (char)( arcmsr_cmd & 0x0F );

	io_hdr.dxfer_dir = dir;
	io_hdr.dxfer_len = sizeof(sBuf);
	io_hdr.dxferp = (unsigned char *)&sBuf;  
	io_hdr.cmnd = cdb;
	io_hdr.cmnd_len = sizeof(cdb);
	io_hdr.sensep = sense;  
	io_hdr.max_sense_len = sizeof(sense);
	io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

	while ( 1 )
	{
		ioctlreturn = do_normal_scsi_cmnd_io(fd, &io_hdr, 0);
		if ( ioctlreturn || io_hdr.scsi_status )
		{
			// errors found
			break;
		}

		if ( arcmsr_cmd != ARCMSR_IOCTL_READ_RQBUFFER )
		{
			// if succeeded, just returns the length of outgoing data
			return data_len;
		}

		if ( sBuf.srbioctl.Length )
		{
			//dumpdata(&sBuf.ioctldatabuffer[0], sBuf.srbioctl.Length);
			memcpy(ptr, &sBuf.ioctldatabuffer[0], sBuf.srbioctl.Length);
			ptr += sBuf.srbioctl.Length;
			total += sBuf.srbioctl.Length;
			// the returned bytes enough to compute payload length ?
			if ( expected < 0 && total >= 5 )
			{
				areca_return_packet = (unsigned char *)&return_buff[0];
				if ( areca_return_packet[0] == 0x5E && 
					 areca_return_packet[1] == 0x01 && 
					 areca_return_packet[2] == 0x61 )
				{
					// valid header, let's compute the returned payload length,
					// we expected the total length is 
					// payload + 3 bytes header + 2 bytes length + 1 byte checksum
					expected = areca_return_packet[4] * 256 + areca_return_packet[3] + 6;
				}
			}

			if ( total >= 7 && total >= expected )
			{
				//printf("total bytes received = %d, expected length = %d\n", total, expected);

				// ------ Okay! we received enough --------
				break;
			}
		}
	}

	// Deal with the different error cases
	if ( ioctlreturn )
	{
		printf("do_scsi_cmnd_io with write buffer failed code = %x\n", ioctlreturn);
		return -2;
	}


	if ( io_hdr.scsi_status )
	{
		printf("io_hdr.scsi_status with write buffer failed code = %x\n", io_hdr.scsi_status);
		return -3;
	}


	if ( data )
	{
		memcpy(data, return_buff, total);
	}

	return total;
}


linux_areca_device::linux_areca_device(smart_interface * intf, const char * dev_name, int disknum)
: smart_device(intf, dev_name, "areca", "areca"),
  linux_smart_device(O_RDWR | O_EXCL | O_NONBLOCK),
  m_disknum(disknum)
{
  set_info().info_name = strprintf("%s [areca_%02d]", dev_name, disknum);
}

// Areca RAID Controller
int linux_areca_device::ata_command_interface(smart_command_set command, int select, char * data)
{
	// ATA input registers
	typedef struct _ATA_INPUT_REGISTERS
	{
		unsigned char features;
		unsigned char sector_count;
		unsigned char sector_number;
		unsigned char cylinder_low; 
		unsigned char cylinder_high;    
		unsigned char device_head;  
		unsigned char command;      
		unsigned char reserved[8];
		unsigned char data[512]; // [in/out] buffer for outgoing/incoming data
	} sATA_INPUT_REGISTERS;

	// ATA output registers
	// Note: The output registers is re-sorted for areca internal use only
	typedef struct _ATA_OUTPUT_REGISTERS
	{
		unsigned char error;
		unsigned char status;
		unsigned char sector_count;
		unsigned char sector_number;
		unsigned char cylinder_low; 
		unsigned char cylinder_high;
	}sATA_OUTPUT_REGISTERS;

	// Areca packet format for outgoing:
	// B[0~2] : 3 bytes header, fixed value 0x5E, 0x01, 0x61
	// B[3~4] : 2 bytes command length + variant data length, little endian
	// B[5]   : 1 bytes areca defined command code, ATA passthrough command code is 0x1c
	// B[6~last-1] : variant bytes payload data
	// B[last] : 1 byte checksum, simply sum(B[3] ~ B[last -1])
	// 
	// 
	//   header 3 bytes  length 2 bytes   cmd 1 byte    payload data x bytes  cs 1 byte 
	// +--------------------------------------------------------------------------------+
	// + 0x5E 0x01 0x61 |   0x00 0x00   |     0x1c   | .................... |   0x00    |
	// +--------------------------------------------------------------------------------+
	// 

	//Areca packet format for incoming:
	// B[0~2] : 3 bytes header, fixed value 0x5E, 0x01, 0x61
	// B[3~4] : 2 bytes payload length, little endian
	// B[5~last-1] : variant bytes returned payload data
	// B[last] : 1 byte checksum, simply sum(B[3] ~ B[last -1])
	// 
	// 
	//   header 3 bytes  length 2 bytes   payload data x bytes  cs 1 byte 
	// +-------------------------------------------------------------------+
	// + 0x5E 0x01 0x61 |   0x00 0x00   | .................... |   0x00    |
	// +-------------------------------------------------------------------+
	unsigned char    areca_packet[640];
	int areca_packet_len = sizeof(areca_packet);
	unsigned char cs = 0;	

	sATA_INPUT_REGISTERS *ata_cmd;

	// For debugging
#if 0
	memset(sInq, 0, sizeof(sInq));
	scsiStdInquiry(fd, (unsigned char *)sInq, (int)sizeof(sInq));
	dumpdata((unsigned char *)sInq, sizeof(sInq));
#endif
	memset(areca_packet, 0, areca_packet_len);

	// ----- BEGIN TO SETUP HEADERS -------
	areca_packet[0] = 0x5E;
	areca_packet[1] = 0x01;
	areca_packet[2] = 0x61;
	areca_packet[3] = (unsigned char)((areca_packet_len - 6) & 0xff);
	areca_packet[4] = (unsigned char)(((areca_packet_len - 6) >> 8) & 0xff);
	areca_packet[5] = 0x1c;	// areca defined code for ATA passthrough command


	// ----- BEGIN TO SETUP PAYLOAD DATA -----

	memcpy(&areca_packet[7], "SmrT", 4);	// areca defined password

	ata_cmd = (sATA_INPUT_REGISTERS *)&areca_packet[12];
	ata_cmd->cylinder_low    = 0x4F;
	ata_cmd->cylinder_high   = 0xC2;


	if ( command == READ_VALUES     ||
		 command == READ_THRESHOLDS ||
		 command == READ_LOG ||
		 command == IDENTIFY ||
		 command == PIDENTIFY )
	{
		// the commands will return data
		areca_packet[6] = 0x13;
		ata_cmd->sector_count = 0x1;
	}
	else if ( command == WRITE_LOG )
	{
		// the commands will write data
		areca_packet[6] = 0x14;
	}
	else
	{
		// the commands will return no data
		areca_packet[6] = 0x15;
	}


	ata_cmd->command = ATA_SMART_CMD;
	// Now set ATA registers depending upon command
	switch ( command )
	{
	case CHECK_POWER_MODE:  
		//printf("command = CHECK_POWER_MODE\n");
		ata_cmd->command = ATA_CHECK_POWER_MODE;        
		break;
	case READ_VALUES:
		//printf("command = READ_VALUES\n");
		ata_cmd->features = ATA_SMART_READ_VALUES;
		break;
	case READ_THRESHOLDS:    
		//printf("command = READ_THRESHOLDS\n");
		ata_cmd->features = ATA_SMART_READ_THRESHOLDS;
		break;
	case READ_LOG: 
		//printf("command = READ_LOG\n");
		ata_cmd->features = ATA_SMART_READ_LOG_SECTOR;
		ata_cmd->sector_number = select;        
		break;
	case WRITE_LOG:        
		//printf("command = WRITE_LOG\n");    
		ata_cmd->features = ATA_SMART_WRITE_LOG_SECTOR;
		memcpy(ata_cmd->data, data, 512);
		ata_cmd->sector_count = 1;
		ata_cmd->sector_number = select;
		break;
	case IDENTIFY:
		//printf("command = IDENTIFY\n");   
		ata_cmd->command = ATA_IDENTIFY_DEVICE;         
		break;
	case PIDENTIFY:
		//printf("command = PIDENTIFY\n");
		errno=ENODEV;
		return -1;
	case ENABLE:
		//printf("command = ENABLE\n");
		ata_cmd->features = ATA_SMART_ENABLE;
		break;
	case DISABLE:
		//printf("command = DISABLE\n");
		ata_cmd->features = ATA_SMART_DISABLE;
		break;
	case AUTO_OFFLINE:
		//printf("command = AUTO_OFFLINE\n");
		ata_cmd->features = ATA_SMART_AUTO_OFFLINE;
		// Enable or disable?
		ata_cmd->sector_count = select;
		break;
	case AUTOSAVE:
		//printf("command = AUTOSAVE\n");
		ata_cmd->features = ATA_SMART_AUTOSAVE;
		// Enable or disable?
		ata_cmd->sector_count = select;
		break;
	case IMMEDIATE_OFFLINE:
		//printf("command = IMMEDIATE_OFFLINE\n");
		ata_cmd->features = ATA_SMART_IMMEDIATE_OFFLINE;
		// What test type to run?
		ata_cmd->sector_number = select;
		break;
	case STATUS_CHECK:
		//printf("command = STATUS_CHECK\n");
		ata_cmd->features = ATA_SMART_STATUS;           
		break;
	case STATUS:
		//printf("command = STATUS\n");
		ata_cmd->features = ATA_SMART_STATUS;       
		break;
	default:
		//printf("command = UNKNOWN\n");
		errno=ENOSYS;
		return -1;
	};

	areca_packet[11] = m_disknum - 1;		   // drive number

	// ----- BEGIN TO SETUP CHECKSUM -----
	for ( int loop = 3; loop < areca_packet_len - 1; loop++ )
	{
		cs += areca_packet[loop]; 
	}
	areca_packet[areca_packet_len-1] = cs;

	// ----- BEGIN TO SEND TO ARECA DRIVER ------
	int expected = 0;	
	unsigned char return_buff[2048];
	memset(return_buff, 0, sizeof(return_buff));

	expected = arcmsr_command_handler(get_fd(), ARCMSR_IOCTL_CLEAR_RQBUFFER, NULL, 0, NULL);
        if (expected==-3) {
	    find_areca_in_proc(NULL);
	    return -1;
	}

	expected = arcmsr_command_handler(get_fd(), ARCMSR_IOCTL_CLEAR_WQBUFFER, NULL, 0, NULL);
	expected = arcmsr_command_handler(get_fd(), ARCMSR_IOCTL_WRITE_WQBUFFER, areca_packet, areca_packet_len, NULL);
	if ( expected > 0 )
	{
		expected = arcmsr_command_handler(get_fd(), ARCMSR_IOCTL_READ_RQBUFFER, return_buff, sizeof(return_buff), NULL);
	}
	if ( expected < 0 )
	{
		return -1;
	}

	// ----- VERIFY THE CHECKSUM -----
	cs = 0;
	for ( int loop = 3; loop < expected - 1; loop++ )
	{
		cs += return_buff[loop]; 
	}

	if ( return_buff[expected - 1] != cs )
	{
		errno = EIO;
		return -1;
	}

	sATA_OUTPUT_REGISTERS *ata_out = (sATA_OUTPUT_REGISTERS *)&return_buff[5] ;
	if ( ata_out->status )
	{
		if ( command == IDENTIFY )
		{
			pout("The firmware of your Areca RAID controller appears to be outdated!\n" \
				 "Please update your controller to firmware version 1.46 or later.\n" \
				 "You may download it here: ftp://ftp.areca.com.tw/RaidCards/BIOS_Firmware\n\n");
		}
		errno = EIO;
		return -1;
	}

	// returns with data
	if ( command == READ_VALUES     ||
		 command == READ_THRESHOLDS ||
		 command == READ_LOG ||
		 command == IDENTIFY ||
		 command == PIDENTIFY )
	{
		memcpy(data, &return_buff[7], 512); 
	}

	if ( command == CHECK_POWER_MODE )
	{
		data[0] = ata_out->sector_count;
	}

	if ( command == STATUS_CHECK &&
		 ( ata_out->cylinder_low == 0xF4 && ata_out->cylinder_high == 0x2C ) )
	{
		return 1;
	}

	return 0;
}


/////////////////////////////////////////////////////////////////////////////
/// Marvell support

class linux_marvell_device
: public /*implements*/ ata_device_with_command_set,
  public /*extends*/ linux_smart_device
{
public:
  linux_marvell_device(smart_interface * intf, const char * dev_name, const char * req_type);

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);
};

linux_marvell_device::linux_marvell_device(smart_interface * intf,
  const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "marvell", req_type),
  linux_smart_device(O_RDONLY | O_NONBLOCK)
{
}

int linux_marvell_device::ata_command_interface(smart_command_set command, int select, char * data)
{
  typedef struct {
    int  inlen;
    int  outlen;
    char cmd[540];
  } mvsata_scsi_cmd;

  int copydata = 0;
  mvsata_scsi_cmd  smart_command;
  unsigned char *buff = (unsigned char *)&smart_command.cmd[6];
  // See struct hd_drive_cmd_hdr in hdreg.h
  // buff[0]: ATA COMMAND CODE REGISTER
  // buff[1]: ATA SECTOR NUMBER REGISTER
  // buff[2]: ATA FEATURES REGISTER
  // buff[3]: ATA SECTOR COUNT REGISTER

  // clear out buff.  Large enough for HDIO_DRIVE_CMD (4+512 bytes)
  memset(&smart_command, 0, sizeof(smart_command));
  smart_command.inlen = 540;
  smart_command.outlen = 540;
  smart_command.cmd[0] = 0xC;  //Vendor-specific code
  smart_command.cmd[4] = 6;     //command length

  buff[0] = ATA_SMART_CMD;
  switch (command){
  case CHECK_POWER_MODE:
    buff[0]=ATA_CHECK_POWER_MODE;
    break;
  case READ_VALUES:
    buff[2]=ATA_SMART_READ_VALUES;
    copydata=buff[3]=1;
    break;
  case READ_THRESHOLDS:
    buff[2]=ATA_SMART_READ_THRESHOLDS;
    copydata=buff[1]=buff[3]=1;
    break;
  case READ_LOG:
    buff[2]=ATA_SMART_READ_LOG_SECTOR;
    buff[1]=select;
    copydata=buff[3]=1;
    break;
  case IDENTIFY:
    buff[0]=ATA_IDENTIFY_DEVICE;
    copydata=buff[3]=1;
    break;
  case PIDENTIFY:
    buff[0]=ATA_IDENTIFY_PACKET_DEVICE;
    copydata=buff[3]=1;
    break;
  case ENABLE:
    buff[2]=ATA_SMART_ENABLE;
    buff[1]=1;
    break;
  case DISABLE:
    buff[2]=ATA_SMART_DISABLE;
    buff[1]=1;
    break;
  case STATUS:
  case STATUS_CHECK:
    // this command only says if SMART is working.  It could be
    // replaced with STATUS_CHECK below.
    buff[2] = ATA_SMART_STATUS;
    break;
  case AUTO_OFFLINE:
    buff[2]=ATA_SMART_AUTO_OFFLINE;
    buff[3]=select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
    break;
  case AUTOSAVE:
    buff[2]=ATA_SMART_AUTOSAVE;
    buff[3]=select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
    break;
  case IMMEDIATE_OFFLINE:
    buff[2]=ATA_SMART_IMMEDIATE_OFFLINE;
    buff[1]=select;
    break;
  default:
    pout("Unrecognized command %d in mvsata_os_specific_handler()\n", command);
    EXIT(1);
    break;
  }
  // There are two different types of ioctls().  The HDIO_DRIVE_TASK
  // one is this:
  // We are now doing the HDIO_DRIVE_CMD type ioctl.
  if (ioctl(get_fd(), SCSI_IOCTL_SEND_COMMAND, (void *)&smart_command))
      return -1;

  if (command==CHECK_POWER_MODE) {
    // LEON -- CHECK THIS PLEASE.  THIS SHOULD BE THE SECTOR COUNT
    // REGISTER, AND IT MIGHT BE buff[2] NOT buff[3].  Bruce
    data[0]=buff[3];
    return 0;
  }

  // Always succeed on a SMART status, as a disk that failed returned
  // buff[4]=0xF4, buff[5]=0x2C, i.e. "Bad SMART status" (see below).
  if (command == STATUS)
    return 0;
  //Data returned is starting from 0 offset
  if (command == STATUS_CHECK)
  {
    // Cyl low and Cyl high unchanged means "Good SMART status"
    if (buff[4] == 0x4F && buff[5] == 0xC2)
      return 0;
    // These values mean "Bad SMART status"
    if (buff[4] == 0xF4 && buff[5] == 0x2C)
      return 1;
    // We haven't gotten output that makes sense; print out some debugging info
    syserror("Error SMART Status command failed");
    pout("Please get assistance from %s\n",PACKAGE_BUGREPORT);
    pout("Register values returned from SMART Status command are:\n");
    pout("CMD =0x%02x\n",(int)buff[0]);
    pout("FR =0x%02x\n",(int)buff[1]);
    pout("NS =0x%02x\n",(int)buff[2]);
    pout("SC =0x%02x\n",(int)buff[3]);
    pout("CL =0x%02x\n",(int)buff[4]);
    pout("CH =0x%02x\n",(int)buff[5]);
    pout("SEL=0x%02x\n",(int)buff[6]);
    return -1;
  }

  if (copydata)
    memcpy(data, buff, 512);
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
/// Highpoint RAID support

class linux_highpoint_device
: public /*implements*/ ata_device_with_command_set,
  public /*extends*/ linux_smart_device
{
public:
  linux_highpoint_device(smart_interface * intf, const char * dev_name,
    unsigned char controller, unsigned char channel, unsigned char port);

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);

private:
  unsigned char m_hpt_data[3]; ///< controller/channel/port
};

linux_highpoint_device::linux_highpoint_device(smart_interface * intf, const char * dev_name,
  unsigned char controller, unsigned char channel, unsigned char port)
: smart_device(intf, dev_name, "hpt", "hpt"),
  linux_smart_device(O_RDONLY | O_NONBLOCK)
{
  m_hpt_data[0] = controller; m_hpt_data[1] = channel; m_hpt_data[2] = port;
  set_info().info_name = strprintf("%s [hpt_disk_%u/%u/%u]", dev_name, m_hpt_data[0], m_hpt_data[1], m_hpt_data[2]);
}

// this implementation is derived from ata_command_interface with a header
// packing for highpoint linux driver ioctl interface
//
// ioctl(fd,HPTIO_CTL,buff)
//          ^^^^^^^^^
//
// structure of hpt_buff
// +----+----+----+----+--------------------.....---------------------+
// | 1  | 2  | 3  | 4  | 5                                            |
// +----+----+----+----+--------------------.....---------------------+
//
// 1: The target controller                     [ int    ( 4 Bytes ) ]
// 2: The channel of the target controllee      [ int    ( 4 Bytes ) ]
// 3: HDIO_ ioctl call                          [ int    ( 4 Bytes ) ]
//    available from ${LINUX_KERNEL_SOURCE}/Documentation/ioctl/hdio
// 4: the pmport that disk attached,            [ int    ( 4 Bytes ) ]
//    if no pmport device, set to 1 or leave blank
// 5: data                                      [ void * ( var leangth ) ]
//
#define STRANGE_BUFFER_LENGTH (4+512*0xf8)

int linux_highpoint_device::ata_command_interface(smart_command_set command, int select, char * data)
{
  unsigned char hpt_buff[4*sizeof(int) + STRANGE_BUFFER_LENGTH];
  unsigned int *hpt = (unsigned int *)hpt_buff;
  unsigned char *buff = &hpt_buff[4*sizeof(int)];
  int copydata = 0;
  const int HDIO_DRIVE_CMD_OFFSET = 4;

  memset(hpt_buff, 0, 4*sizeof(int) + STRANGE_BUFFER_LENGTH);
  hpt[0] = m_hpt_data[0]; // controller id
  hpt[1] = m_hpt_data[1]; // channel number
  hpt[3] = m_hpt_data[2]; // pmport number

  buff[0]=ATA_SMART_CMD;
  switch (command){
  case CHECK_POWER_MODE:
    buff[0]=ATA_CHECK_POWER_MODE;
    copydata=1;
    break;
  case READ_VALUES:
    buff[2]=ATA_SMART_READ_VALUES;
    buff[3]=1;
    copydata=512;
    break;
  case READ_THRESHOLDS:
    buff[2]=ATA_SMART_READ_THRESHOLDS;
    buff[1]=buff[3]=1;
    copydata=512;
    break;
  case READ_LOG:
    buff[2]=ATA_SMART_READ_LOG_SECTOR;
    buff[1]=select;
    buff[3]=1;
    copydata=512;
    break;
  case WRITE_LOG:
    break;
  case IDENTIFY:
    buff[0]=ATA_IDENTIFY_DEVICE;
    buff[3]=1;
    copydata=512;
    break;
  case PIDENTIFY:
    buff[0]=ATA_IDENTIFY_PACKET_DEVICE;
    buff[3]=1;
    copydata=512;
    break;
  case ENABLE:
    buff[2]=ATA_SMART_ENABLE;
    buff[1]=1;
    break;
  case DISABLE:
    buff[2]=ATA_SMART_DISABLE;
    buff[1]=1;
    break;
  case STATUS:
    buff[2]=ATA_SMART_STATUS;
    break;
  case AUTO_OFFLINE:
    buff[2]=ATA_SMART_AUTO_OFFLINE;
    buff[3]=select;
    break;
  case AUTOSAVE:
    buff[2]=ATA_SMART_AUTOSAVE;
    buff[3]=select;
    break;
  case IMMEDIATE_OFFLINE:
    buff[2]=ATA_SMART_IMMEDIATE_OFFLINE;
    buff[1]=select;
    break;
  case STATUS_CHECK:
    buff[1]=ATA_SMART_STATUS;
    break;
  default:
    pout("Unrecognized command %d in linux_highpoint_command_interface()\n"
         "Please contact " PACKAGE_BUGREPORT "\n", command);
    errno=ENOSYS;
    return -1;
  }

  if (command==WRITE_LOG) {
    unsigned char task[4*sizeof(int)+sizeof(ide_task_request_t)+512];
    unsigned int *hpt = (unsigned int *)task;
    ide_task_request_t *reqtask = (ide_task_request_t *)(&task[4*sizeof(int)]);
    task_struct_t *taskfile = (task_struct_t *)reqtask->io_ports;
    int retval;

    memset(task, 0, sizeof(task));

    hpt[0] = m_hpt_data[0]; // controller id
    hpt[1] = m_hpt_data[1]; // channel number
    hpt[3] = m_hpt_data[2]; // pmport number
    hpt[2] = HDIO_DRIVE_TASKFILE; // real hd ioctl

    taskfile->data           = 0;
    taskfile->feature        = ATA_SMART_WRITE_LOG_SECTOR;
    taskfile->sector_count   = 1;
    taskfile->sector_number  = select;
    taskfile->low_cylinder   = 0x4f;
    taskfile->high_cylinder  = 0xc2;
    taskfile->device_head    = 0;
    taskfile->command        = ATA_SMART_CMD;

    reqtask->data_phase      = TASKFILE_OUT;
    reqtask->req_cmd         = IDE_DRIVE_TASK_OUT;
    reqtask->out_size        = 512;
    reqtask->in_size         = 0;

    memcpy(task+sizeof(ide_task_request_t)+4*sizeof(int), data, 512);

    if ((retval=ioctl(get_fd(), HPTIO_CTL, task))) {
      if (retval==-EINVAL)
        pout("Kernel lacks HDIO_DRIVE_TASKFILE support; compile kernel with CONFIG_IDE_TASKFILE_IO set\n");
      return -1;
    }
    return 0;
  }

  if (command==STATUS_CHECK){
    int retval;
    unsigned const char normal_lo=0x4f, normal_hi=0xc2;
    unsigned const char failed_lo=0xf4, failed_hi=0x2c;
    buff[4]=normal_lo;
    buff[5]=normal_hi;

    hpt[2] = HDIO_DRIVE_TASK;

    if ((retval=ioctl(get_fd(), HPTIO_CTL, hpt_buff))) {
      if (retval==-EINVAL) {
        pout("Error SMART Status command via HDIO_DRIVE_TASK failed");
        pout("Rebuild older linux 2.2 kernels with HDIO_DRIVE_TASK support added\n");
      }
      else
        syserror("Error SMART Status command failed");
      return -1;
    }

    if (buff[4]==normal_lo && buff[5]==normal_hi)
      return 0;

    if (buff[4]==failed_lo && buff[5]==failed_hi)
      return 1;

    syserror("Error SMART Status command failed");
    pout("Please get assistance from " PACKAGE_HOMEPAGE "\n");
    pout("Register values returned from SMART Status command are:\n");
    pout("CMD=0x%02x\n",(int)buff[0]);
    pout("FR =0x%02x\n",(int)buff[1]);
    pout("NS =0x%02x\n",(int)buff[2]);
    pout("SC =0x%02x\n",(int)buff[3]);
    pout("CL =0x%02x\n",(int)buff[4]);
    pout("CH =0x%02x\n",(int)buff[5]);
    pout("SEL=0x%02x\n",(int)buff[6]);
    return -1;
  }

#if 1
  if (command==IDENTIFY || command==PIDENTIFY) {
    unsigned char deviceid[4*sizeof(int)+512*sizeof(char)];
    unsigned int *hpt = (unsigned int *)deviceid;

    hpt[0] = m_hpt_data[0]; // controller id
    hpt[1] = m_hpt_data[1]; // channel number
    hpt[3] = m_hpt_data[2]; // pmport number

    hpt[2] = HDIO_GET_IDENTITY;
    if (!ioctl(get_fd(), HPTIO_CTL, deviceid) && (deviceid[4*sizeof(int)] & 0x8000))
      buff[0]=(command==IDENTIFY)?ATA_IDENTIFY_PACKET_DEVICE:ATA_IDENTIFY_DEVICE;
  }
#endif

  hpt[2] = HDIO_DRIVE_CMD;
  if ((ioctl(get_fd(), HPTIO_CTL, hpt_buff)))
    return -1;

  if (command==CHECK_POWER_MODE)
    buff[HDIO_DRIVE_CMD_OFFSET]=buff[2];

  if (copydata)
    memcpy(data, buff+HDIO_DRIVE_CMD_OFFSET, copydata);

  return 0;
}


#if 0 // TODO: Migrate from 'smart_command_set' to 'ata_in_regs' OR remove the function
// Utility function for printing warnings
void printwarning(smart_command_set command){
  static int printed[4]={0,0,0,0};
  const char* message=
    "can not be passed through the 3ware 3w-xxxx driver.  This can be fixed by\n"
    "applying a simple 3w-xxxx driver patch that can be found here:\n"
    PACKAGE_HOMEPAGE "\n"
    "Alternatively, upgrade your 3w-xxxx driver to version 1.02.00.037 or greater.\n\n";

  if (command==AUTO_OFFLINE && !printed[0]) {
    printed[0]=1;
    pout("The SMART AUTO-OFFLINE ENABLE command (smartmontools -o on option/Directive)\n%s", message);
  }
  else if (command==AUTOSAVE && !printed[1]) {
    printed[1]=1;
    pout("The SMART AUTOSAVE ENABLE command (smartmontools -S on option/Directive)\n%s", message);
  }
  else if (command==STATUS_CHECK && !printed[2]) {
    printed[2]=1;
    pout("The SMART RETURN STATUS return value (smartmontools -H option/Directive)\n%s", message);
  }
  else if (command==WRITE_LOG && !printed[3])  {
    printed[3]=1;
    pout("The SMART WRITE LOG command (smartmontools -t selective) only supported via char /dev/tw[ae] interface\n");
  }

  return;
}
#endif


/////////////////////////////////////////////////////////////////////////////
/// SCSI open with autodetection support

smart_device * linux_scsi_device::autodetect_open()
{
  // Open device
  if (!open())
    return this;

  // No Autodetection if device type was specified by user
  bool sat_only = false;
  if (*get_req_type()) {
    // Detect SAT if device object was created by scan_smart_devices().
    if (!(m_scanning && !strcmp(get_req_type(), "sat")))
      return this;
    sat_only = true;
  }

  // The code below is based on smartd.cpp:SCSIFilterKnown()

  // Get INQUIRY
  unsigned char req_buff[64] = {0, };
  int req_len = 36;
  if (scsiStdInquiry(this, req_buff, req_len)) {
    // Marvell controllers fail on a 36 bytes StdInquiry, but 64 suffices
    // watch this spot ... other devices could lock up here
    req_len = 64;
    if (scsiStdInquiry(this, req_buff, req_len)) {
      // device doesn't like INQUIRY commands
      close();
      set_err(EIO, "INQUIRY failed");
      return this;
    }
  }

  int avail_len = req_buff[4] + 5;
  int len = (avail_len < req_len ? avail_len : req_len);
  if (len < 36) {
    if (sat_only) {
      close();
      set_err(EIO, "INQUIRY too short for SAT");
    }
    return this;
  }

  // Use INQUIRY to detect type
  if (!sat_only) {

    // 3ware ?
    if (!memcmp(req_buff + 8, "3ware", 5) || !memcmp(req_buff + 8, "AMCC", 4)) {
      close();
      set_err(EINVAL, "AMCC/3ware controller, please try adding '-d 3ware,N',\n"
                      "you may need to replace %s with /dev/twlN, /dev/twaN or /dev/tweN", get_dev_name());
      return this;
    }

    // DELL?
    if (!memcmp(req_buff + 8, "DELL    PERC", 12) || !memcmp(req_buff + 8, "MegaRAID", 8)) {
      close();
      set_err(EINVAL, "DELL or MegaRaid controller, please try adding '-d megaraid,N'");
      return this;
    }

    // Marvell ?
    if (len >= 42 && !memcmp(req_buff + 36, "MVSATA", 6)) {
      //pout("Device %s: using '-d marvell' for ATA disk with Marvell driver\n", get_dev_name());
      close();
      smart_device_auto_ptr newdev(
        new linux_marvell_device(smi(), get_dev_name(), get_req_type())
      );
      newdev->open(); // TODO: Can possibly pass open fd
      delete this;
      return newdev.release();
    }
  }

  // SAT or USB ?
  {
    smart_device * newdev = smi()->autodetect_sat_device(this, req_buff, len);
    if (newdev)
      // NOTE: 'this' is now owned by '*newdev'
      return newdev;
  }

  // Nothing special found

  if (sat_only) {
    close();
    set_err(EIO, "Not a SAT device");
  }
  return this;
}


//////////////////////////////////////////////////////////////////////
// USB bridge ID detection

// Read USB ID from /sys file
static bool read_id(const std::string & path, unsigned short & id)
{
  FILE * f = fopen(path.c_str(), "r");
  if (!f)
    return false;
  int n = -1;
  bool ok = (fscanf(f, "%hx%n", &id, &n) == 1 && n == 4);
  fclose(f);
  return ok;
}

// Get USB bridge ID for "sdX"
static bool get_usb_id(const char * name, unsigned short & vendor_id,
                       unsigned short & product_id, unsigned short & version)
{
  // Only "sdX" supported
  if (!(!strncmp(name, "sd", 2) && !strchr(name, '/')))
    return false;

  // Start search at dir referenced by symlink "/sys/block/sdX/device"
  // -> "/sys/devices/.../usb*/.../host*/target*/..."
  std::string dir = strprintf("/sys/block/%s/device", name);

  // Stop search at "/sys/devices"
  struct stat st;
  if (stat("/sys/devices", &st))
    return false;
  ino_t stop_ino = st.st_ino;

  // Search in parent directories until "idVendor" is found,
  // fail if "/sys/devices" reached or too many iterations
  int cnt = 0;
  do {
    dir += "/..";
    if (!(++cnt < 10 && !stat(dir.c_str(), &st) && st.st_ino != stop_ino))
      return false;
  } while (access((dir + "/idVendor").c_str(), 0));

  // Read IDs
  if (!(   read_id(dir + "/idVendor", vendor_id)
        && read_id(dir + "/idProduct", product_id)
        && read_id(dir + "/bcdDevice", version)   ))
    return false;

  if (con->reportscsiioctl > 1)
    pout("USB ID = 0x%04x:0x%04x (0x%03x)\n", vendor_id, product_id, version);
  return true;
}


//////////////////////////////////////////////////////////////////////
/// Linux interface

class linux_smart_interface
: public /*implements*/ smart_interface
{
public:
  virtual std::string get_app_examples(const char * appname);

  virtual bool scan_smart_devices(smart_device_list & devlist, const char * type,
    const char * pattern = 0);

protected:
  virtual ata_device * get_ata_device(const char * name, const char * type);

  virtual scsi_device * get_scsi_device(const char * name, const char * type);

  virtual smart_device * autodetect_smart_device(const char * name);

  virtual smart_device * get_custom_smart_device(const char * name, const char * type);

  virtual std::string get_valid_custom_dev_types_str();

private:
  bool get_dev_list(smart_device_list & devlist, const char * pattern,
    bool scan_ata, bool scan_scsi, const char * req_type, bool autodetect);

  smart_device * missing_option(const char * opt);
};

std::string linux_smart_interface::get_app_examples(const char * appname)
{
  if (!strcmp(appname, "smartctl"))
    return smartctl_examples;
  return "";
}


// we are going to take advantage of the fact that Linux's devfs will only
// have device entries for devices that exist.  So if we get the equivalent of
// ls /dev/hd[a-t], we have all the ATA devices on the system
bool linux_smart_interface::get_dev_list(smart_device_list & devlist,
  const char * pattern, bool scan_ata, bool scan_scsi,
  const char * req_type, bool autodetect)
{
  // Use glob to look for any directory entries matching the pattern
  glob_t globbuf;
  memset(&globbuf, 0, sizeof(globbuf));
  int retglob = glob(pattern, GLOB_ERR, NULL, &globbuf);
  if (retglob) {
    //  glob failed: free memory and return
    globfree(&globbuf);

    if (retglob==GLOB_NOMATCH){
      pout("glob(3) found no matches for pattern %s\n", pattern);
      return true;
    }

    if (retglob==GLOB_NOSPACE)
      set_err(ENOMEM, "glob(3) ran out of memory matching pattern %s", pattern);
#ifdef GLOB_ABORTED // missing in old versions of glob.h
    else if (retglob==GLOB_ABORTED)
      set_err(EINVAL, "glob(3) aborted matching pattern %s", pattern);
#endif
    else
      set_err(EINVAL, "Unexplained error in glob(3) of pattern %s", pattern);

    return false;
  }

  // did we find too many paths?
  const int max_pathc = 32;
  int n = (int)globbuf.gl_pathc;
  if (n > max_pathc) {
    pout("glob(3) found %d > MAX=%d devices matching pattern %s: ignoring %d paths\n",
         n, max_pathc, pattern, n - max_pathc);
    n = max_pathc;
  }

  // now step through the list returned by glob.  If not a link, copy
  // to list.  If it is a link, evaluate it and see if the path ends
  // in "disc".
  for (int i = 0; i < n; i++){
    // see if path is a link
    char linkbuf[1024];
    int retlink = readlink(globbuf.gl_pathv[i], linkbuf, sizeof(linkbuf)-1);

    char tmpname[1024]={0};
    const char * name = 0;
    bool is_scsi = scan_scsi;
    // if not a link (or a strange link), keep it
    if (retlink<=0 || retlink>1023)
      name = globbuf.gl_pathv[i];
    else {
      // or if it's a link that points to a disc, follow it
      linkbuf[retlink] = 0;
      const char *p;
      if ((p=strrchr(linkbuf, '/')) && !strcmp(p+1, "disc"))
        // This is the branch of the code that gets followed if we are
        // using devfs WITH traditional compatibility links. In this
        // case, we add the traditional device name to the list that
        // is returned.
        name = globbuf.gl_pathv[i];
      else {
        // This is the branch of the code that gets followed if we are
        // using devfs WITHOUT traditional compatibility links.  In
        // this case, we check that the link to the directory is of
        // the correct type, and then append "disc" to it.
        bool match_ata  = strstr(linkbuf, "ide");
        bool match_scsi = strstr(linkbuf, "scsi");
        if (((match_ata && scan_ata) || (match_scsi && scan_scsi)) && !(match_ata && match_scsi)) {
          is_scsi = match_scsi;
          snprintf(tmpname, sizeof(tmpname), "%s/disc", globbuf.gl_pathv[i]);
          name = tmpname;
        }
      }
    }

    if (name) {
      // Found a name, add device to list.
      smart_device * dev;
      if (autodetect)
        dev = autodetect_smart_device(name);
      else if (is_scsi)
        dev = new linux_scsi_device(this, name, req_type, true /*scanning*/);
      else
        dev = new linux_ata_device(this, name, req_type);
      if (dev) // autodetect_smart_device() may return nullptr.
        devlist.push_back(dev);
    }
  }

  // free memory
  globfree(&globbuf);

  return true;
}

bool linux_smart_interface::scan_smart_devices(smart_device_list & devlist,
  const char * type, const char * pattern /*= 0*/)
{
  if (pattern) {
    set_err(EINVAL, "DEVICESCAN with pattern not implemented yet");
    return false;
  }

  if (!type)
    type = "";

  bool scan_ata  = (!*type || !strcmp(type, "ata" ));
  // "sat" detection will be later handled in linux_scsi_device::autodetect_open()
  bool scan_scsi = (!*type || !strcmp(type, "scsi") || !strcmp(type, "sat"));
  if (!(scan_ata || scan_scsi))
    return true;

  if (scan_ata)
    get_dev_list(devlist, "/dev/hd[a-t]", true, false, type, false);
  if (scan_scsi) {
    bool autodetect = !*type; // Try USB autodetection if no type specifed
    get_dev_list(devlist, "/dev/sd[a-z]", false, true, type, autodetect);
    // Support up to 104 devices
    get_dev_list(devlist, "/dev/sd[a-c][a-z]", false, true, type, autodetect);
  }

  // if we found traditional links, we are done
  if (devlist.size() > 0)
    return true;

  // else look for devfs entries without traditional links
  // TODO: Add udev support
  return get_dev_list(devlist, "/dev/discs/disc*", scan_ata, scan_scsi, type, false);
}

ata_device * linux_smart_interface::get_ata_device(const char * name, const char * type)
{
  return new linux_ata_device(this, name, type);
}

scsi_device * linux_smart_interface::get_scsi_device(const char * name, const char * type)
{
  return new linux_scsi_device(this, name, type);
}

smart_device * linux_smart_interface::missing_option(const char * opt)
{
  set_err(EINVAL, "requires option '%s'", opt);
  return 0;
}

// Return true if STR starts with PREFIX.
static bool str_starts_with(const char * str, const char * prefix)
{
  return !strncmp(str, prefix, strlen(prefix));
}

// Guess device type (ata or scsi) based on device name (Linux
// specific) SCSI device name in linux can be sd, sr, scd, st, nst,
// osst, nosst and sg.
static const char * lin_dev_prefix = "/dev/";
static const char * lin_dev_ata_disk_plus = "h";
static const char * lin_dev_ata_devfs_disk_plus = "ide/";
static const char * lin_dev_scsi_devfs_disk_plus = "scsi/";
static const char * lin_dev_scsi_disk_plus = "s";
static const char * lin_dev_scsi_tape1 = "ns";
static const char * lin_dev_scsi_tape2 = "os";
static const char * lin_dev_scsi_tape3 = "nos";
static const char * lin_dev_3ware_9700_char = "twl";
static const char * lin_dev_3ware_9000_char = "twa";
static const char * lin_dev_3ware_678k_char = "twe";
static const char * lin_dev_cciss_dir = "cciss/";
static const char * lin_dev_areca = "sg";

smart_device * linux_smart_interface::autodetect_smart_device(const char * name)
{
  const char * dev_name = name; // TODO: Remove this hack
  int dev_prefix_len = strlen(lin_dev_prefix);

  // if dev_name null, or string length zero
  int len;
  if (!dev_name || !(len = strlen(dev_name)))
    return 0;

  // Dereference if /dev/disk/by-*/* symlink
  char linkbuf[100];
  if (   str_starts_with(dev_name, "/dev/disk/by-")
      && readlink(dev_name, linkbuf, sizeof(linkbuf)) > 0
      && str_starts_with(linkbuf, "../../")) {
    dev_name = linkbuf + sizeof("../../")-1;
  }
  // Remove the leading /dev/... if it's there
  else if (!strncmp(lin_dev_prefix, dev_name, dev_prefix_len)) {
    if (len <= dev_prefix_len)
      // if nothing else in the string, unrecognized
      return 0;
    // else advance pointer to following characters
    dev_name += dev_prefix_len;
  }

  // form /dev/h* or h*
  if (!strncmp(lin_dev_ata_disk_plus, dev_name,
               strlen(lin_dev_ata_disk_plus)))
    return new linux_ata_device(this, name, "");

  // form /dev/ide/* or ide/*
  if (!strncmp(lin_dev_ata_devfs_disk_plus, dev_name,
               strlen(lin_dev_ata_devfs_disk_plus)))
    return new linux_ata_device(this, name, "");

  // form /dev/s* or s*
  if (!strncmp(lin_dev_scsi_disk_plus, dev_name,
               strlen(lin_dev_scsi_disk_plus))) {

    // Try to detect possible USB->(S)ATA bridge
    unsigned short vendor_id = 0, product_id = 0, version = 0;
    if (get_usb_id(dev_name, vendor_id, product_id, version)) {
      const char * usbtype = get_usb_dev_type_by_id(vendor_id, product_id, version);
      if (!usbtype)
        return 0;
      // Linux USB layer does not support 16 byte SAT pass through command
      if (!strcmp(usbtype, "sat"))
        usbtype = "sat,12";
      // Return SAT/USB device for this type
      // (Note: linux_scsi_device::autodetect_open() will not be called in this case)
      return get_sat_device(usbtype, new linux_scsi_device(this, name, ""));
    }

    // No USB bridge found, assume regular SCSI device
    return new linux_scsi_device(this, name, "");
  }

  // form /dev/scsi/* or scsi/*
  if (!strncmp(lin_dev_scsi_devfs_disk_plus, dev_name,
               strlen(lin_dev_scsi_devfs_disk_plus)))
    return new linux_scsi_device(this, name, "");

  // form /dev/ns* or ns*
  if (!strncmp(lin_dev_scsi_tape1, dev_name,
               strlen(lin_dev_scsi_tape1)))
    return new linux_scsi_device(this, name, "");

  // form /dev/os* or os*
  if (!strncmp(lin_dev_scsi_tape2, dev_name,
               strlen(lin_dev_scsi_tape2)))
    return new linux_scsi_device(this, name, "");

  // form /dev/nos* or nos*
  if (!strncmp(lin_dev_scsi_tape3, dev_name,
               strlen(lin_dev_scsi_tape3)))
    return new linux_scsi_device(this, name, "");

  // form /dev/twl*
  if (!strncmp(lin_dev_3ware_9700_char, dev_name,
               strlen(lin_dev_3ware_9700_char)))
    return missing_option("-d 3ware,N");

  // form /dev/twa*
  if (!strncmp(lin_dev_3ware_9000_char, dev_name,
               strlen(lin_dev_3ware_9000_char)))
    return missing_option("-d 3ware,N");

  // form /dev/twe*
  if (!strncmp(lin_dev_3ware_678k_char, dev_name,
               strlen(lin_dev_3ware_678k_char)))
    return missing_option("-d 3ware,N");

  // form /dev/cciss*
  if (!strncmp(lin_dev_cciss_dir, dev_name,
               strlen(lin_dev_cciss_dir)))
    return missing_option("-d cciss,N");

  // form /dev/sg*
  if ( !strncmp(lin_dev_areca, dev_name,
                strlen(lin_dev_areca)) )
    return missing_option("-d areca,N");

  // we failed to recognize any of the forms
  return 0;
}

smart_device * linux_smart_interface::get_custom_smart_device(const char * name, const char * type)
{
  // Marvell ?
  if (!strcmp(type, "marvell"))
    return new linux_marvell_device(this, name, type);

  // 3Ware ?
  int disknum = -1, n1 = -1, n2 = -1;
  if (sscanf(type, "3ware,%n%d%n", &n1, &disknum, &n2) == 1 || n1 == 6) {
    if (n2 != (int)strlen(type)) {
      set_err(EINVAL, "Option -d 3ware,N requires N to be a non-negative integer");
      return 0;
    }
    if (!(0 <= disknum && disknum <= 127)) {
      set_err(EINVAL, "Option -d 3ware,N (N=%d) must have 0 <= N <= 127", disknum);
      return 0;
    }

    if (!strncmp(name, "/dev/twl", 8))
      return new linux_escalade_device(this, name, linux_escalade_device::AMCC_3WARE_9700_CHAR, disknum);
    else if (!strncmp(name, "/dev/twa", 8))
      return new linux_escalade_device(this, name, linux_escalade_device::AMCC_3WARE_9000_CHAR, disknum);
    else if (!strncmp(name, "/dev/twe", 8))
      return new linux_escalade_device(this, name, linux_escalade_device::AMCC_3WARE_678K_CHAR, disknum);
    else
      return new linux_escalade_device(this, name, linux_escalade_device::AMCC_3WARE_678K, disknum);
  }

  // Areca?
  disknum = n1 = n2 = -1;
  if (sscanf(type, "areca,%n%d%n", &n1, &disknum, &n2) == 1 || n1 == 6) {
    if (n2 != (int)strlen(type)) {
      set_err(EINVAL, "Option -d areca,N requires N to be a non-negative integer");
      return 0;
    }
    if (!(1 <= disknum && disknum <= 24)) {
      set_err(EINVAL, "Option -d areca,N (N=%d) must have 1 <= N <= 24", disknum);
      return 0;
    }
    return new linux_areca_device(this, name, disknum);
  }

  // Highpoint ?
  int controller = -1, channel = -1; disknum = 1;
  n1 = n2 = -1; int n3 = -1;
  if (sscanf(type, "hpt,%n%d/%d%n/%d%n", &n1, &controller, &channel, &n2, &disknum, &n3) >= 2 || n1 == 4) {
    int len = strlen(type);
    if (!(n2 == len || n3 == len)) {
      set_err(EINVAL, "Option '-d hpt,L/M/N' supports 2-3 items");
      return 0;
    }
    if (!(1 <= controller && controller <= 8)) {
      set_err(EINVAL, "Option '-d hpt,L/M/N' invalid controller id L supplied");
      return 0;
    }
    if (!(1 <= channel && channel <= 8)) {
      set_err(EINVAL, "Option '-d hpt,L/M/N' invalid channel number M supplied");
      return 0;
    }
    if (!(1 <= disknum && disknum <= 15)) {
      set_err(EINVAL, "Option '-d hpt,L/M/N' invalid pmport number N supplied");
      return 0;
    }
    return new linux_highpoint_device(this, name, controller, channel, disknum);
  }

#ifdef HAVE_LINUX_CCISS_IOCTL_H
  // CCISS ?
  disknum = n1 = n2 = -1;
  if (sscanf(type, "cciss,%n%d%n", &n1, &disknum, &n2) == 1 || n1 == 6) {
    if (n2 != (int)strlen(type)) {
      set_err(EINVAL, "Option -d cciss,N requires N to be a non-negative integer");
      return 0;
    }
    if (!(0 <= disknum && disknum <= 127)) {
      set_err(EINVAL, "Option -d cciss,N (N=%d) must have 0 <= N <= 127", disknum);
      return 0;
    }
    return new linux_cciss_device(this, name, disknum);
  }
#endif // HAVE_LINUX_CCISS_IOCTL_H

  // MegaRAID ?
  if (sscanf(type, "megaraid,%d", &disknum) == 1) {
    return new linux_megaraid_device(this, name, 0, disknum);
  }
  return 0;
}

std::string linux_smart_interface::get_valid_custom_dev_types_str()
{
  return "marvell, areca,N, 3ware,N, hpt,L/M/N, megaraid,N"
#ifdef HAVE_LINUX_CCISS_IOCTL_H
                                              ", cciss,N"
#endif
    ;
}

} // namespace


/////////////////////////////////////////////////////////////////////////////
/// Initialize platform interface and register with smi()

void smart_interface::init()
{
  static os_linux::linux_smart_interface the_interface;
  smart_interface::set(&the_interface);
}
