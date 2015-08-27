/*
 * os_freebsd.c
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-10 Eduard Martinescu <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <camlib.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_pass.h>
#if defined(__DragonFly__)
#include <sys/nata.h>
#else
#include <sys/ata.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <glob.h>
#include <stddef.h>
#include <paths.h>
#include <sys/utsname.h>

#include "config.h"
#include "int64.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "cciss.h"
#include "utility.h"
#include "os_freebsd.h"

#include "dev_interface.h"
#include "dev_ata_cmd_set.h"
#include "dev_areca.h"

#define USBDEV "/dev/usb"
#if defined(__FreeBSD_version)

// This way we define one variable for the GNU/kFreeBSD and FreeBSD 
#define FREEBSDVER __FreeBSD_version
#else
#define FREEBSDVER __FreeBSD_kernel_version
#endif

#if (FREEBSDVER >= 800000)
#include <libusb20_desc.h>
#include <libusb20.h>
#elif defined(__DragonFly__)
#include <bus/usb/usb.h>
#include <bus/usb/usbhid.h>
#else
#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>
#endif

#define CONTROLLER_3WARE_9000_CHAR      0x01
#define CONTROLLER_3WARE_678K_CHAR      0x02

#ifndef PATHINQ_SETTINGS_SIZE
#define PATHINQ_SETTINGS_SIZE   128
#endif

const char *os_XXXX_c_cvsid="$Id$" \
ATACMDS_H_CVSID CCISS_H_CVSID CONFIG_H_CVSID INT64_H_CVSID OS_FREEBSD_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

#define NO_RETURN 0
#define BAD_SMART 1
#define NO_DISK_3WARE 2
#define BAD_KERNEL 3
#define MAX_MSG 3

// Utility function for printing warnings
void printwarning(int msgNo, const char* extra) {
  static int printed[] = {0,0,0,0};
  static const char* message[]={
    "The SMART RETURN STATUS return value (smartmontools -H option/Directive)\n can not be retrieved with this version of ATAng, please do not rely on this value\nYou should update to at least 5.2\n",

    "Error SMART Status command failed\nPlease get assistance from \n" PACKAGE_HOMEPAGE "\nRegister values returned from SMART Status command are:\n",

    "You must specify a DISK # for 3ware drives with -d 3ware,<n> where <n> begins with 1 for first disk drive\n",

    "ATA support is not provided for this kernel version. Please ugrade to a recent 5-CURRENT kernel (post 09/01/2003 or so)\n"
  };

  if (msgNo >= 0 && msgNo <= MAX_MSG) {
    if (!printed[msgNo]) {
      printed[msgNo] = 1;
      pout("%s", message[msgNo]);
      if (extra)
        pout("%s",extra);
    }
  }
  return;
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c

#define BUFFER_LEN_678K_CHAR ( sizeof(struct twe_usercommand) ) // 520
#define BUFFER_LEN_9000_CHAR ( sizeof(TW_OSLI_IOCTL_NO_DATA_BUF) + sizeof(TWE_Command) ) // 2048
#define TW_IOCTL_BUFFER_SIZE ( MAX(BUFFER_LEN_678K_CHAR, BUFFER_LEN_9000_CHAR) )

#ifndef ATA_DEVICE
#define ATA_DEVICE "/dev/ata"
#endif

#define ARGUSED(x) ((void)(x))

// global variable holding byte count of allocated memory
long long bytes;
extern unsigned char failuretest_permissive;

/////////////////////////////////////////////////////////////////////////////

namespace os_freebsd { // No need to publish anything, name provided for Doxygen

/////////////////////////////////////////////////////////////////////////////
/// Implement shared open/close routines with old functions.

class freebsd_smart_device
: virtual public /*implements*/ smart_device
{
public:
  explicit freebsd_smart_device()
    : smart_device(never_called),
      m_fd(-1) { }

  virtual ~freebsd_smart_device() throw();

  virtual bool is_open() const;

  virtual bool open();

  virtual bool close();

protected:
  /// Return filedesc for derived classes.
  int get_fd() const
    { return m_fd; }

  void set_fd(int fd)
    { m_fd = fd; }

private:
  int m_fd; ///< filedesc, -1 if not open.
};

#ifdef __GLIBC__
static inline void * reallocf(void *ptr, size_t size) {
   void *rv = realloc(ptr, size);
   if((rv == NULL) && (size != 0))
     free(ptr);
   return rv;
   }
#endif

freebsd_smart_device::~freebsd_smart_device() throw()
{
  if (m_fd >= 0)
    os_freebsd::freebsd_smart_device::close();
}

// migration from the old_style 
unsigned char m_controller_type;
unsigned char m_controller_port; 

// examples for smartctl
static const char  smartctl_examples[] =
   "=================================================== SMARTCTL EXAMPLES =====\n\n"
         "  smartctl -a /dev/ad0                       (Prints all SMART information)\n\n"
         "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/ad0\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long /dev/ad0              (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/ad0\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         "                                      (Prints Self-Test & Attribute errors)\n\n"
         "  smartctl -a --device=3ware,2 /dev/twa0\n"
         "  smartctl -a --device=3ware,2 /dev/twe0\n"
         "  smartctl -a --device=3ware,2 /dev/tws0\n"
         "                              (Prints all SMART information for ATA disk on\n"
         "                                 third port of first 3ware RAID controller)\n"
  "  smartctl -a --device=cciss,0 /dev/ciss0\n"
         "                              (Prints all SMART information for first disk \n"
         "                               on Common Interface for SCSI-3 Support driver)\n"
  "  smartctl -a --device=areca,3/1 /dev/arcmsr0\n"
         "                              (Prints all SMART information for 3rd disk in the 1st enclosure \n"
         "                               on first ARECA RAID controller)\n"

         ;

bool freebsd_smart_device::is_open() const
{
  return (m_fd >= 0);
}


bool freebsd_smart_device::open()
{
  const char *dev = get_dev_name();
  if ((m_fd = ::open(dev,O_RDONLY))<0) {
    set_err(errno);
    return false;
  }
  return true;
}

bool freebsd_smart_device::close()
{
  int failed = 0;
  // close device, if open
  if (is_open())
    failed=::close(get_fd());

  set_fd(-1);

  if(failed) return false;
    else return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Implement standard ATA support

class freebsd_ata_device
: public /*implements*/ ata_device,
  public /*extends*/ freebsd_smart_device
{
public:
  freebsd_ata_device(smart_interface * intf, const char * dev_name, const char * req_type);
  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

protected:
  virtual int do_cmd(struct ata_ioc_request* request, bool is_48bit_cmd);
};

freebsd_ata_device::freebsd_ata_device(smart_interface * intf, const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "ata", req_type),
  freebsd_smart_device()
{
}

int freebsd_ata_device::do_cmd( struct ata_ioc_request* request, bool is_48bit_cmd)
{
  int fd = get_fd(), ret;
  ARGUSED(is_48bit_cmd); // no support for 48 bit commands in the IOCATAREQUEST
  ret = ioctl(fd, IOCATAREQUEST, request);
  if (ret) set_err(errno);
  return ret;
}



bool freebsd_ata_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  bool ata_48bit = false; // no ata_48bit_support via IOCATAREQUEST
  if(!strcmp("atacam",get_dev_type())) // enable for atacam interface
    ata_48bit = true;

  if (!ata_cmd_is_ok(in,
    true,  // data_out_support
    true,  // multi_sector_support
    ata_48bit) 
    ) {
      set_err(ENOSYS, "48-bit ATA commands not implemented for legacy controllers");
      return false;
    }

  struct ata_ioc_request request;
  bzero(&request,sizeof(struct ata_ioc_request));

  request.timeout=SCSI_TIMEOUT_DEFAULT;
  request.u.ata.command=in.in_regs.command;
  request.u.ata.feature=in.in_regs.features;

  request.u.ata.count = in.in_regs.sector_count_16;
  request.u.ata.lba = in.in_regs.lba_48;

  switch (in.direction) {
    case ata_cmd_in::no_data:  
      request.flags=ATA_CMD_CONTROL;
      break;
    case ata_cmd_in::data_in:  
      request.flags=ATA_CMD_READ | ATA_CMD_CONTROL;
      request.data=(char *)in.buffer;
      request.count=in.size;
      break;
    case ata_cmd_in::data_out: 
      request.flags=ATA_CMD_WRITE | ATA_CMD_CONTROL;
      request.data=(char *)in.buffer;
      request.count=in.size;
      break;
    default:
      return set_err(ENOSYS);
  }

  clear_err();
  errno = 0;
  if (do_cmd(&request, in.in_regs.is_48bit_cmd()))
      return false;
  if (request.error)
      return set_err(EIO, "request failed, error code 0x%02x", request.error);

  out.out_regs.error = request.error;
  out.out_regs.sector_count_16 = request.u.ata.count;
  out.out_regs.lba_48 = request.u.ata.lba;


  // Command specific processing
  if (in.in_regs.command == ATA_SMART_CMD
       && in.in_regs.features == ATA_SMART_STATUS
       && in.out_needed.lba_high)
  {
    unsigned const char normal_lo=0x4f, normal_hi=0xc2;
    unsigned const char failed_lo=0xf4, failed_hi=0x2c;

    // Cyl low and Cyl high unchanged means "Good SMART status"
    if (!(out.out_regs.lba_mid==normal_lo && out.out_regs.lba_high==normal_hi)
    // These values mean "Bad SMART status"
        && !(out.out_regs.lba_mid==failed_lo && out.out_regs.lba_high==failed_hi))

    {
      // We haven't gotten output that makes sense; print out some debugging info
      char buf[512];
      snprintf(buf, sizeof(buf),
        "CMD=0x%02x\nFR =0x%02x\nNS =0x%02x\nSC =0x%02x\nCL =0x%02x\nCH =0x%02x\nRETURN =0x%04x\n",
        (int)request.u.ata.command,
        (int)request.u.ata.feature,
        (int)request.u.ata.count,
        (int)((request.u.ata.lba) & 0xff),
        (int)((request.u.ata.lba>>8) & 0xff),
        (int)((request.u.ata.lba>>16) & 0xff),
        (int)request.error);
      printwarning(BAD_SMART,buf);
      out.out_regs.lba_high = failed_hi; 
      out.out_regs.lba_mid = failed_lo;
    }
  }

  return true;
}

#if FREEBSDVER > 800100
class freebsd_atacam_device : public freebsd_ata_device
{
public:
  freebsd_atacam_device(smart_interface * intf, const char * dev_name, const char * req_type)
  : smart_device(intf, dev_name, "atacam", req_type), freebsd_ata_device(intf, dev_name, req_type)
  {}
  
  virtual bool open();
  virtual bool close();
  
protected:
  int m_fd;
  struct cam_device *m_camdev;

  virtual int do_cmd( struct ata_ioc_request* request , bool is_48bit_cmd);
};

bool freebsd_atacam_device::open(){
  const char *dev = get_dev_name();
  
  if ((m_camdev = cam_open_device(dev, O_RDWR)) == NULL) {
    set_err(errno);
    return false;
  }
  set_fd(m_camdev->fd);
  return true;
}

bool freebsd_atacam_device::close(){
  cam_close_device(m_camdev);
  set_fd(-1);
  return true;
}

int freebsd_atacam_device::do_cmd( struct ata_ioc_request* request, bool is_48bit_cmd)
{
  union ccb ccb;
  int camflags;

  // FIXME:
  // 48bit commands are broken in ATACAM before r242422/HEAD
  // and may cause system hang
  // Waiting for MFC to make sure that bug is fixed,
  // later version check needs to be added
  if(!strcmp("ata",m_camdev->sim_name) && is_48bit_cmd) {
    set_err(ENOSYS, "48-bit ATA commands not implemented for legacy controllers");
    return -1;
  }

  memset(&ccb, 0, sizeof(ccb));

  if (request->count == 0)
    camflags = CAM_DIR_NONE;
  else if (request->flags & ATA_CMD_READ)
    camflags = CAM_DIR_IN;
  else
    camflags = CAM_DIR_OUT;

  cam_fill_ataio(&ccb.ataio,
                 0,
                 NULL,
                 camflags,
                 MSG_SIMPLE_Q_TAG,
                 (u_int8_t*)request->data,
                 request->count,
                 request->timeout * 1000); // timeout in seconds

  ccb.ataio.cmd.flags = CAM_ATAIO_NEEDRESULT |
    (is_48bit_cmd ? CAM_ATAIO_48BIT : 0);
  // ata_28bit_cmd
  ccb.ataio.cmd.command = request->u.ata.command;
  ccb.ataio.cmd.features = request->u.ata.feature;
  ccb.ataio.cmd.lba_low = request->u.ata.lba;
  ccb.ataio.cmd.lba_mid = request->u.ata.lba >> 8;
  ccb.ataio.cmd.lba_high = request->u.ata.lba >> 16;
  // ata_48bit cmd
  ccb.ataio.cmd.lba_low_exp = request->u.ata.lba >> 24;
  ccb.ataio.cmd.lba_mid_exp = request->u.ata.lba >> 32;
  ccb.ataio.cmd.lba_high_exp = request->u.ata.lba >> 40;
  ccb.ataio.cmd.device = 0x40 | ((request->u.ata.lba >> 24) & 0x0f);
  ccb.ataio.cmd.sector_count = request->u.ata.count;
  ccb.ataio.cmd.sector_count_exp = request->u.ata.count  >> 8;;

  ccb.ccb_h.flags |= CAM_DEV_QFRZDIS;

  if (cam_send_ccb(m_camdev, &ccb) < 0) {
    set_err(EIO, "cam_send_ccb failed");
    return -1;
  }

  if ((ccb.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
    if(scsi_debugmode > 0)
      cam_error_print(m_camdev, &ccb, CAM_ESF_ALL, CAM_EPF_ALL, stderr);
    set_err(EIO);
    return -1;
  }

  request->u.ata.lba =
    ((u_int64_t)(ccb.ataio.res.lba_low)) |
    ((u_int64_t)(ccb.ataio.res.lba_mid) << 8) |
    ((u_int64_t)(ccb.ataio.res.lba_high) << 16) |
    ((u_int64_t)(ccb.ataio.res.lba_low_exp) << 24) |
    ((u_int64_t)(ccb.ataio.res.lba_mid_exp) << 32) |
    ((u_int64_t)(ccb.ataio.res.lba_high_exp) << 40);

  request->u.ata.count = ccb.ataio.res.sector_count | (ccb.ataio.res.sector_count_exp << 8);
  request->error = ccb.ataio.res.error;

  return 0;
}

#endif

/////////////////////////////////////////////////////////////////////////////
/// Implement AMCC/3ware RAID support

class freebsd_escalade_device
: public /*implements*/ ata_device,
  public /*extends*/ freebsd_smart_device
{
public:
  freebsd_escalade_device(smart_interface * intf, const char * dev_name,
    int escalade_type, int disknum);

protected:
  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);
  virtual bool open();

private:
  int m_escalade_type; ///< Type string for escalade_command_interface().
  int m_disknum; ///< Disk number.
};

freebsd_escalade_device::freebsd_escalade_device(smart_interface * intf, const char * dev_name,
    int escalade_type, int disknum)
: smart_device(intf, dev_name, "3ware", "3ware"),
  freebsd_smart_device(),
  m_escalade_type(escalade_type), m_disknum(disknum)
{
  set_info().info_name = strprintf("%s [3ware_disk_%02d]", dev_name, disknum);
}

bool freebsd_escalade_device::open()
{
  const char *dev = get_dev_name();
  int fd;
  
  if ((fd = ::open(dev,O_RDWR))<0) {
    set_err(errno);
    return false;
  }
  set_fd(fd);
  return true;
}

bool freebsd_escalade_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  // to hold true file descriptor
  int fd = get_fd();

  if (!ata_cmd_is_ok(in,
    true, // data_out_support
    false, // TODO: multi_sector_support
    true) // ata_48bit_support
  )
  return false;

  struct twe_usercommand* cmd_twe = NULL;
  TW_OSLI_IOCTL_NO_DATA_BUF* cmd_twa = NULL;
  TWE_Command_ATA* ata = NULL;

  // Used by both the SCSI and char interfaces
  char ioctl_buffer[TW_IOCTL_BUFFER_SIZE];

  if (m_disknum < 0) {
    printwarning(NO_DISK_3WARE,NULL);
    return -1;
  }

  memset(ioctl_buffer, 0, TW_IOCTL_BUFFER_SIZE);

  if (m_escalade_type==CONTROLLER_3WARE_9000_CHAR) {
    cmd_twa = (TW_OSLI_IOCTL_NO_DATA_BUF*)ioctl_buffer;
    cmd_twa->pdata = ((TW_OSLI_IOCTL_WITH_PAYLOAD*)cmd_twa)->payload.data_buf;
    cmd_twa->driver_pkt.buffer_length = in.size;
    // using "old" packet format to speak with SATA devices
    ata = (TWE_Command_ATA*)&cmd_twa->cmd_pkt.command.cmd_pkt_7k;
  } else if (m_escalade_type==CONTROLLER_3WARE_678K_CHAR) {
    cmd_twe = (struct twe_usercommand*)ioctl_buffer;
    ata = &cmd_twe->tu_command.ata;
  } else {
    return set_err(ENOSYS,
      "Unrecognized escalade_type %d in linux_3ware_command_interface(disk %d)\n"
      "Please contact " PACKAGE_BUGREPORT "\n", (int)m_escalade_type, m_disknum);
  }

  ata->opcode = TWE_OP_ATA_PASSTHROUGH;

  // Same for (almost) all commands - but some reset below
  ata->request_id    = 0xFF;
  ata->unit          = m_disknum;
  ata->status        = 0;
  ata->flags         = 0x1;
  ata->size         = 0x5; // TODO: multisector support
  // Set registers
  {
    const ata_in_regs_48bit & r = in.in_regs;
    ata->features     = r.features_16;
    ata->sector_count = r.sector_count_16;
    ata->sector_num   = r.lba_low_16;
    ata->cylinder_lo  = r.lba_mid_16;
    ata->cylinder_hi  = r.lba_high_16;
    ata->drive_head   = r.device;
    ata->command      = r.command;
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
    if (m_escalade_type==CONTROLLER_3WARE_678K_CHAR) {
      cmd_twe->tu_data = in.buffer;
      cmd_twe->tu_size = 512;
    }

    readdata=true;
    ata->sgl_offset   = 0x5;
    ata->param        = 0xD;
    // For 64-bit to work correctly, up the size of the command packet
    // in dwords by 1 to account for the 64-bit single sgl 'address'
    // field. Note that this doesn't agree with the typedefs but it's
    // right (agree with kernel driver behavior/typedefs).
    // if (sizeof(long)==8)
    //  ata->size++;
  }
  else if (in.direction == ata_cmd_in::no_data) {
    // Non data command -- but doesn't use large sector 
    // count register values.
    ata->sgl_offset   = 0x0;
    ata->param        = 0x8;
    ata->sector_count = 0x0;
  }
  else if (in.direction == ata_cmd_in::data_out) {
    ata->sgl_offset   = 0x5;
    ata->param        = 0xF; // PIO data write
    if (m_escalade_type==CONTROLLER_3WARE_678K_CHAR) {
      cmd_twe->tu_data = in.buffer;
      cmd_twe->tu_size = 512;
    }
    else if (m_escalade_type==CONTROLLER_3WARE_9000_CHAR) {
       memcpy(cmd_twa->pdata, in.buffer, in.size);
    }
  }
  else
    return set_err(EINVAL);

  // 3WARE controller can NOT have packet device internally
  if (in.in_regs.command == ATA_IDENTIFY_PACKET_DEVICE) {
    return set_err(ENODEV, "No drive on port %d", m_disknum);
  }

  // Now send the command down through an ioctl()
  int ioctlreturn;
  if (m_escalade_type==CONTROLLER_3WARE_9000_CHAR) {
    ioctlreturn=ioctl(fd,TW_OSL_IOCTL_FIRMWARE_PASS_THROUGH,cmd_twa);
  } else {
    ioctlreturn=ioctl(fd,TWEIO_COMMAND,cmd_twe);
  }

  // Deal with the different error cases
  if (ioctlreturn) {
    return set_err(EIO);
  }

  // See if the ATA command failed.  Now that we have returned from
  // the ioctl() call, if passthru is valid, then:
  // - ata->status contains the 3ware controller STATUS
  // - ata->command contains the ATA STATUS register
  // - ata->features contains the ATA ERROR register
  //
  // Check bits 0 (error bit) and 5 (device fault) of the ATA STATUS
  // If bit 0 (error bit) is set, then ATA ERROR register is valid.
  // While we *might* decode the ATA ERROR register, at the moment it
  // doesn't make much sense: we don't care in detail why the error
  // happened.

  if (ata->status || (ata->command & 0x21)) {
    if (scsi_debugmode)
      pout("Command failed, ata.status=(0x%2.2x), ata.command=(0x%2.2x), ata.flags=(0x%2.2x)\n",ata->status,ata->command,ata->flags);
    return set_err(EIO);
  }

  // If this is a read data command, copy data to output buffer
  if (readdata) {
    if (m_escalade_type==CONTROLLER_3WARE_9000_CHAR)
      memcpy(in.buffer, cmd_twa->pdata, in.size);
    else if(m_escalade_type==CONTROLLER_3WARE_678K_CHAR) {
      memcpy(in.buffer, cmd_twe->tu_data, in.size); // untested
    }
  }
  // Return register values
  if (ata) {
    ata_out_regs_48bit & r = out.out_regs;
    r.error           = ata->features;
    r.sector_count_16 = ata->sector_count;
    r.lba_low_16      = ata->sector_num;
    r.lba_mid_16      = ata->cylinder_lo;
    r.lba_high_16     = ata->cylinder_hi;
    r.device          = ata->drive_head;
    r.status          = ata->command;
  }
  // look for nonexistent devices/ports
  if (in.in_regs.command == ATA_IDENTIFY_DEVICE
  && !nonempty((unsigned char *)in.buffer, in.size)) {
    return set_err(ENODEV, "No drive on port %d", m_disknum);
  }
  return true;
}


/////////////////////////////////////////////////////////////////////////////
/// Implement Highpoint RAID support with old functions

class freebsd_highpoint_device
: public /*implements*/ ata_device_with_command_set,
  public /*extends*/ freebsd_smart_device
{
public:
  freebsd_highpoint_device(smart_interface * intf, const char * dev_name,
    unsigned char controller, unsigned char channel, unsigned char port);

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);
  virtual bool open();

private:
  unsigned char m_hpt_data[3]; ///< controller/channel/port
};


freebsd_highpoint_device::freebsd_highpoint_device(smart_interface * intf, const char * dev_name,
  unsigned char controller, unsigned char channel, unsigned char port)
: smart_device(intf, dev_name, "hpt", "hpt"),
  freebsd_smart_device()
{
  m_hpt_data[0] = controller; m_hpt_data[1] = channel; m_hpt_data[2] = port;
  set_info().info_name = strprintf("%s [hpt_disk_%u/%u/%u]", dev_name, m_hpt_data[0], m_hpt_data[1], m_hpt_data[2]);
}

bool freebsd_highpoint_device::open()
{
  const char *dev = get_dev_name();
  int fd;
  
  if ((fd = ::open(dev,O_RDWR))<0) {
    set_err(errno);
    return false;
  }
  set_fd(fd);
  return true;
}

int freebsd_highpoint_device::ata_command_interface(smart_command_set command, int select, char * data)
{
  int fd=get_fd(); 
  int ids[2];
  HPT_IOCTL_PARAM param;
  HPT_CHANNEL_INFO_V2 info;
  unsigned char* buff[512 + 2 * sizeof(HPT_PASS_THROUGH_HEADER)];
  PHPT_PASS_THROUGH_HEADER pide_pt_hdr, pide_pt_hdr_out;

  // get internal deviceid
  ids[0] = m_hpt_data[0] - 1;
  ids[1] = m_hpt_data[1] - 1;

  memset(&param, 0, sizeof(HPT_IOCTL_PARAM));

  param.magic = HPT_IOCTL_MAGIC;
  param.ctrl_code = HPT_IOCTL_GET_CHANNEL_INFO_V2;
  param.in = (unsigned char *)ids;
  param.in_size = sizeof(unsigned int) * 2;
  param.out = (unsigned char *)&info;
  param.out_size = sizeof(HPT_CHANNEL_INFO_V2);

  if (m_hpt_data[2]==1) {
    param.ctrl_code = HPT_IOCTL_GET_CHANNEL_INFO;
    param.out_size = sizeof(HPT_CHANNEL_INFO);
  }
  if (ioctl(fd, HPT_DO_IOCONTROL, &param)!=0 ||
      info.devices[m_hpt_data[2]-1]==0) {
    return -1;
  }

  // perform smart action
  memset(buff, 0, 512 + 2 * sizeof(HPT_PASS_THROUGH_HEADER));
  pide_pt_hdr = (PHPT_PASS_THROUGH_HEADER)buff;

  pide_pt_hdr->lbamid = 0x4f;
  pide_pt_hdr->lbahigh = 0xc2;
  pide_pt_hdr->command = ATA_SMART_CMD;
  pide_pt_hdr->id = info.devices[m_hpt_data[2] - 1];

  switch (command){
  case READ_VALUES:
    pide_pt_hdr->feature=ATA_SMART_READ_VALUES;
    pide_pt_hdr->protocol=HPT_READ;
    break;
  case READ_THRESHOLDS:
    pide_pt_hdr->feature=ATA_SMART_READ_THRESHOLDS;
    pide_pt_hdr->protocol=HPT_READ;
    break;
  case READ_LOG:
    pide_pt_hdr->feature=ATA_SMART_READ_LOG_SECTOR;
    pide_pt_hdr->lbalow=select;
    pide_pt_hdr->protocol=HPT_READ;
    break;
  case IDENTIFY:
    pide_pt_hdr->command=ATA_IDENTIFY_DEVICE;
    pide_pt_hdr->protocol=HPT_READ;
    break;
  case ENABLE:
    pide_pt_hdr->feature=ATA_SMART_ENABLE;
    break;
  case DISABLE:
    pide_pt_hdr->feature=ATA_SMART_DISABLE;
    break;
  case AUTO_OFFLINE:
    pide_pt_hdr->feature=ATA_SMART_AUTO_OFFLINE;
    pide_pt_hdr->sectorcount=select;
    break;
  case AUTOSAVE:
    pide_pt_hdr->feature=ATA_SMART_AUTOSAVE;
    pide_pt_hdr->sectorcount=select;
    break;
  case IMMEDIATE_OFFLINE:
    pide_pt_hdr->feature=ATA_SMART_IMMEDIATE_OFFLINE;
    pide_pt_hdr->lbalow=select;
    break;
  case STATUS_CHECK:
  case STATUS:
    pide_pt_hdr->feature=ATA_SMART_STATUS;
    break;
  case CHECK_POWER_MODE:
    pide_pt_hdr->command=ATA_CHECK_POWER_MODE;
    break;
  case WRITE_LOG:
    memcpy(buff+sizeof(HPT_PASS_THROUGH_HEADER), data, 512);
    pide_pt_hdr->feature=ATA_SMART_WRITE_LOG_SECTOR;
    pide_pt_hdr->lbalow=select;
    pide_pt_hdr->protocol=HPT_WRITE;
    break;
  default:
    pout("Unrecognized command %d in highpoint_command_interface()\n"
         "Please contact " PACKAGE_BUGREPORT "\n", command);
    errno=ENOSYS;
    return -1;
  }
  if (pide_pt_hdr->protocol!=0) {
    pide_pt_hdr->sectors = 1;
    pide_pt_hdr->sectorcount = 1;
  }

  memset(&param, 0, sizeof(HPT_IOCTL_PARAM));

  param.magic = HPT_IOCTL_MAGIC;
  param.ctrl_code = HPT_IOCTL_IDE_PASS_THROUGH;
  param.in = (unsigned char *)buff;
  param.in_size = sizeof(HPT_PASS_THROUGH_HEADER) + (pide_pt_hdr->protocol==HPT_READ ? 0 : pide_pt_hdr->sectors * 512);
  param.out = (unsigned char *)buff+param.in_size;
  param.out_size = sizeof(HPT_PASS_THROUGH_HEADER) + (pide_pt_hdr->protocol==HPT_READ ? pide_pt_hdr->sectors * 512 : 0);

  pide_pt_hdr_out = (PHPT_PASS_THROUGH_HEADER)param.out;

  if ((ioctl(fd, HPT_DO_IOCONTROL, &param)!=0) ||
      (pide_pt_hdr_out->command & 1)) {
    return -1;
  }

  if (command==STATUS_CHECK)
  {
    unsigned const char normal_lo=0x4f, normal_hi=0xc2;
    unsigned const char failed_lo=0xf4, failed_hi=0x2c;
    unsigned char low,high;

    high = pide_pt_hdr_out->lbahigh;
    low = pide_pt_hdr_out->lbamid;

    // Cyl low and Cyl high unchanged means "Good SMART status"
    if (low==normal_lo && high==normal_hi)
      return 0;

    // These values mean "Bad SMART status"
    if (low==failed_lo && high==failed_hi)
      return 1;

    // We haven't gotten output that makes sense; print out some debugging info
    char buf[512];
    snprintf(buf, sizeof(buf),
            "CMD=0x%02x\nFR =0x%02x\nNS =0x%02x\nSC =0x%02x\nCL =0x%02x\nCH =0x%02x\nRETURN =0x%04x\n",
            (int)pide_pt_hdr_out->command,
            (int)pide_pt_hdr_out->feature,
            (int)pide_pt_hdr_out->sectorcount,
            (int)pide_pt_hdr_out->lbalow,
            (int)pide_pt_hdr_out->lbamid,
            (int)pide_pt_hdr_out->lbahigh,
            (int)pide_pt_hdr_out->sectors);
    printwarning(BAD_SMART,buf);
  }
  else if (command==CHECK_POWER_MODE)
    data[0] = pide_pt_hdr_out->sectorcount & 0xff;
  else if (pide_pt_hdr->protocol==HPT_READ)
    memcpy(data, (unsigned char *)buff + 2 * sizeof(HPT_PASS_THROUGH_HEADER), 
      pide_pt_hdr->sectors * 512);
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
/// Standard SCSI support

class freebsd_scsi_device
: public /*implements*/ scsi_device,
  public /*extends*/ freebsd_smart_device
{
public:
  freebsd_scsi_device(smart_interface * intf, const char * dev_name, const char * req_type);

  virtual smart_device * autodetect_open();

  virtual bool scsi_pass_through(scsi_cmnd_io * iop);
  
  virtual bool open();

  virtual bool close();
  
private:
  struct cam_device *m_camdev;
};

bool freebsd_scsi_device::open(){
  const char *dev = get_dev_name();
  
  if ((m_camdev = cam_open_device(dev, O_RDWR)) == NULL) {
    set_err(errno);
    return false;
  }
  set_fd(m_camdev->fd);
  return true;
}

bool freebsd_scsi_device::close(){
  cam_close_device(m_camdev);
  set_fd(-1);
  return true;
}

freebsd_scsi_device::freebsd_scsi_device(smart_interface * intf,
  const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "scsi", req_type),
  freebsd_smart_device()
{
}


bool freebsd_scsi_device::scsi_pass_through(scsi_cmnd_io * iop)
{
  union ccb *ccb;

  if (scsi_debugmode) {
    unsigned int k;
    const unsigned char * ucp = iop->cmnd;
    const char * np;

    np = scsi_get_opcode_name(ucp[0]);
    pout(" [%s: ", np ? np : "<unknown opcode>");
    for (k = 0; k < iop->cmnd_len; ++k)
      pout("%02x ", ucp[k]);
    if ((scsi_debugmode > 1) && 
      (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
    int trunc = (iop->dxfer_len > 256) ? 1 : 0;

    pout("]\n  Outgoing data, len=%d%s:\n", (int)iop->dxfer_len,
      (trunc ? " [only first 256 bytes shown]" : ""));
    dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
      }
      else
        pout("]\n");
  }

  if(m_camdev==NULL) {
    if (scsi_debugmode)
      pout("  error: camdev=0!\n");
    return set_err(ENOTTY);
  }

  if (!(ccb = cam_getccb(m_camdev))) {
    if (scsi_debugmode)
      pout("  error allocating ccb\n");
    return set_err(ENOMEM);
  }

  // mfi SAT layer is known to be buggy
  if(!strcmp("mfi",m_camdev->sim_name)) {
    if (iop->cmnd[0] == SAT_ATA_PASSTHROUGH_12 || iop->cmnd[0] == SAT_ATA_PASSTHROUGH_16) { 
      // Controller does not return ATA output registers in SAT sense data
      if (iop->cmnd[2] & (1 << 5)) // chk_cond
        return set_err(ENOSYS, "ATA return descriptor not supported by controller firmware");
    }
    // SMART WRITE LOG SECTOR causing media errors
    if ((iop->cmnd[0] == SAT_ATA_PASSTHROUGH_16
        && iop->cmnd[14] == ATA_SMART_CMD  && iop->cmnd[3]==0 &&
        iop->cmnd[4] == ATA_SMART_WRITE_LOG_SECTOR) ||
        (iop->cmnd[0] == SAT_ATA_PASSTHROUGH_12
        && iop->cmnd[9] == ATA_SMART_CMD && iop->cmnd[3] == ATA_SMART_WRITE_LOG_SECTOR))
    {
      if(!failuretest_permissive)
        return set_err(ENOSYS, "SMART WRITE LOG SECTOR may cause problems, try with -T permissive to force"); 
    }
  }
  // clear out structure, except for header that was filled in for us
  bzero(&(&ccb->ccb_h)[1],
    sizeof(struct ccb_scsiio) - sizeof(struct ccb_hdr));

  cam_fill_csio(&ccb->csio,
    /* retries */ 1,
    /* cbfcnp */ NULL,
    /* flags */ (iop->dxfer_dir == DXFER_NONE ? CAM_DIR_NONE :(iop->dxfer_dir == DXFER_FROM_DEVICE ? CAM_DIR_IN : CAM_DIR_OUT)),
    /* tagaction */ MSG_SIMPLE_Q_TAG,
    /* dataptr */ iop->dxferp,
    /* datalen */ iop->dxfer_len,
    /* senselen */ iop->max_sense_len,
    /* cdblen */ iop->cmnd_len,
    /* timout (converted to seconds) */ iop->timeout*1000);
  memcpy(ccb->csio.cdb_io.cdb_bytes,iop->cmnd,iop->cmnd_len);

  if (cam_send_ccb(m_camdev,ccb) < 0) {
    if (scsi_debugmode) {
      pout("  error sending SCSI ccb\n");
      cam_error_print(m_camdev,ccb,CAM_ESF_ALL,CAM_EPF_ALL,stderr);
    }
    cam_freeccb(ccb);
    return set_err(EIO);
  }

  if (scsi_debugmode) {
    pout("  CAM status=0x%x, SCSI status=0x%x, resid=0x%x\n",
         ccb->ccb_h.status, ccb->csio.scsi_status, ccb->csio.resid);
    if ((scsi_debugmode > 1) && (DXFER_FROM_DEVICE == iop->dxfer_dir)) {
      int trunc, len;

      len = iop->dxfer_len - ccb->csio.resid;
      trunc = (len > 256) ? 1 : 0;
      if (len > 0) {
        pout("  Incoming data, len=%d%s:\n", len,
             (trunc ? " [only first 256 bytes shown]" : ""));
        dStrHex(iop->dxferp, (trunc ? 256 : len), 1);
      }
      else
        pout("  Incoming data trimmed to nothing by resid\n");
    }
  }

  if (((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) && ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_SCSI_STATUS_ERROR)) {
    if (scsi_debugmode)
      cam_error_print(m_camdev,ccb,CAM_ESF_ALL,CAM_EPF_ALL,stderr);
    cam_freeccb(ccb);
    return set_err(EIO);
  }

  iop->resid = ccb->csio.resid;
  iop->scsi_status = ccb->csio.scsi_status;
  if (iop->sensep && (ccb->ccb_h.status & CAM_AUTOSNS_VALID) != 0) {
    if (scsi_debugmode)
      pout("  sense_len=0x%x, sense_resid=0x%x\n",
           ccb->csio.sense_len, ccb->csio.sense_resid);
    iop->resp_sense_len = ccb->csio.sense_len - ccb->csio.sense_resid;
    /* Some SCSI controller device drivers miscalculate the sense_resid
       field so cap resp_sense_len on max_sense_len. */
    if (iop->resp_sense_len > iop->max_sense_len)
      iop->resp_sense_len = iop->max_sense_len;
    if (iop->resp_sense_len > 0) {
      memcpy(iop->sensep, &(ccb->csio.sense_data), iop->resp_sense_len);
      if (scsi_debugmode) {
        if (scsi_debugmode > 1) {
          pout("  >>> Sense buffer, len=%zu:\n", iop->resp_sense_len);
          dStrHex(iop->sensep, iop->resp_sense_len, 1);
        }
        if ((iop->sensep[0] & 0x7f) > 0x71)
          pout("  status=0x%x: [desc] sense_key=0x%x asc=0x%x ascq=0x%x\n",
               iop->scsi_status, iop->sensep[1] & 0xf,
               iop->sensep[2], iop->sensep[3]);
        else
          pout("  status=0x%x: sense_key=0x%x asc=0x%x ascq=0x%x\n",
               iop->scsi_status, iop->sensep[2] & 0xf,
               iop->sensep[12], iop->sensep[13]);
      }
    }
    else if (scsi_debugmode)
      pout("  status=0x%x\n", iop->scsi_status);
  }
  else if (scsi_debugmode)
    pout("  status=0x%x\n", iop->scsi_status);

  cam_freeccb(ccb);

  // mfip replacing PDT of the device so response does not make a sense
  // this sets PDT to 00h - direct-access block device
  if((!strcmp("mfi", m_camdev->sim_name) || !strcmp("mpt", m_camdev->sim_name))
   && iop->cmnd[0] == INQUIRY) {
     if (scsi_debugmode) {
        pout("  device on %s controller, patching PDT\n", m_camdev->sim_name);
     }
     iop->dxferp[0] = iop->dxferp[0] & 0xe0;
  }

  return true;
}


/////////////////////////////////////////////////////////////////////////////
/// Areca RAID support

///////////////////////////////////////////////////////////////////
// SATA(ATA) device behind Areca RAID Controller
class freebsd_areca_ata_device
: public /*implements*/ areca_ata_device,
  public /*extends*/ freebsd_smart_device
{
public:
  freebsd_areca_ata_device(smart_interface * intf, const char * dev_name, int disknum, int encnum = 1);
  virtual smart_device * autodetect_open();
  virtual bool arcmsr_lock();
  virtual bool arcmsr_unlock();
  virtual int arcmsr_do_scsi_io(struct scsi_cmnd_io * iop);
};

///////////////////////////////////////////////////////////////////
// SAS(SCSI) device behind Areca RAID Controller
class freebsd_areca_scsi_device
: public /*implements*/ areca_scsi_device,
  public /*extends*/ freebsd_smart_device
{
public:
  freebsd_areca_scsi_device(smart_interface * intf, const char * dev_name, int disknum, int encnum = 1);
  virtual smart_device * autodetect_open();
  virtual bool arcmsr_lock();
  virtual bool arcmsr_unlock();
  virtual int arcmsr_do_scsi_io(struct scsi_cmnd_io * iop);
};


// Areca RAID Controller(SATA Disk)
freebsd_areca_ata_device::freebsd_areca_ata_device(smart_interface * intf, const char * dev_name, int disknum, int encnum)
: smart_device(intf, dev_name, "areca", "areca"),
  freebsd_smart_device()
{
  set_disknum(disknum);
  set_encnum(encnum);
  set_info().info_name = strprintf("%s [areca_disk#%02d_enc#%02d]", dev_name, disknum, encnum);
}


smart_device * freebsd_areca_ata_device::autodetect_open()
{
  int is_ata = 1;

  // autodetect device type
  is_ata = arcmsr_get_dev_type();
  if(is_ata < 0)
  {
    set_err(EIO);
    return this;
  }

  if(is_ata == 1)
  {
    // SATA device
    return this;
  }

  // SAS device
  smart_device_auto_ptr newdev(new freebsd_areca_scsi_device(smi(), get_dev_name(), get_disknum(), get_encnum()));
  close();
  delete this;
  newdev->open();	// TODO: Can possibly pass open fd

  return newdev.release();
}

int freebsd_areca_ata_device::arcmsr_do_scsi_io(struct scsi_cmnd_io * iop)
{
  int ioctlreturn = 0;

  if(!is_open()) {
      if(!open()){
      }
  }

  ioctlreturn = ioctl(get_fd(), ((sSRB_BUFFER *)(iop->dxferp))->srbioctl.ControlCode, iop->dxferp);
  if (ioctlreturn)
  {
    // errors found
    return -1;
  }

  return ioctlreturn;
}

bool freebsd_areca_ata_device::arcmsr_lock()
{
  return true;
}


bool freebsd_areca_ata_device::arcmsr_unlock()
{
  return true;
}


// Areca RAID Controller(SAS Device)
freebsd_areca_scsi_device::freebsd_areca_scsi_device(smart_interface * intf, const char * dev_name, int disknum, int encnum)
: smart_device(intf, dev_name, "areca", "areca"),
  freebsd_smart_device()
{
  set_disknum(disknum);
  set_encnum(encnum);
  set_info().info_name = strprintf("%s [areca_disk#%02d_enc#%02d]", dev_name, disknum, encnum);
}

smart_device * freebsd_areca_scsi_device::autodetect_open()
{
  return this;
}

int freebsd_areca_scsi_device::arcmsr_do_scsi_io(struct scsi_cmnd_io * iop)
{
  int ioctlreturn = 0;

  if(!is_open()) {
      if(!open()){
      }
  }
  ioctlreturn = ioctl(get_fd(), ((sSRB_BUFFER *)(iop->dxferp))->srbioctl.ControlCode, iop->dxferp);
  if (ioctlreturn)
  {
    // errors found
    return -1;
  }

  return ioctlreturn;
}

bool freebsd_areca_scsi_device::arcmsr_lock()
{
  return true;
}


bool freebsd_areca_scsi_device::arcmsr_unlock()
{
  return true;
}


/////////////////////////////////////////////////////////////////////////////
/// Implement CCISS RAID support with old functions

class freebsd_cciss_device
: public /*implements*/ scsi_device,
  public /*extends*/ freebsd_smart_device
{
public:
  freebsd_cciss_device(smart_interface * intf, const char * name, unsigned char disknum);

  virtual bool scsi_pass_through(scsi_cmnd_io * iop);
  virtual bool open();

private:
  unsigned char m_disknum; ///< Disk number.
};

bool freebsd_cciss_device::open()
{
  const char *dev = get_dev_name();
  int fd;
  if ((fd = ::open(dev,O_RDWR))<0) {
    set_err(errno);
    return false;
  }
  set_fd(fd);
  return true;
}

freebsd_cciss_device::freebsd_cciss_device(smart_interface * intf,
  const char * dev_name, unsigned char disknum)
: smart_device(intf, dev_name, "cciss", "cciss"),
  freebsd_smart_device(),
  m_disknum(disknum)
{
  set_info().info_name = strprintf("%s [cciss_disk_%02d]", dev_name, disknum);
}

bool freebsd_cciss_device::scsi_pass_through(scsi_cmnd_io * iop)
{
  int status = cciss_io_interface(get_fd(), m_disknum, iop, scsi_debugmode);
  if (status < 0)
      return set_err(-status);
  return true;
  // not reached
  return true;
}


/////////////////////////////////////////////////////////////////////////////
/// SCSI open with autodetection support

smart_device * freebsd_scsi_device::autodetect_open()
{
  // Open device
  if (!open())
    return this;

  // No Autodetection if device type was specified by user
  if (*get_req_type())
    return this;

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
  if (len < 36)
    return this;

  // Use INQUIRY to detect type

  // 3ware ?
  if (!memcmp(req_buff + 8, "3ware", 5) || !memcmp(req_buff + 8, "AMCC", 4) ||
      !strcmp("tws",m_camdev->sim_name) || !strcmp("twa",m_camdev->sim_name)) {
    close();
    set_err(EINVAL, "3ware/LSI controller, please try adding '-d 3ware,N',\n"
                    "you may need to replace %s with /dev/twaN, /dev/tweN or /dev/twsN", get_dev_name());
    return this;
  }

  // SAT or USB, skip MFI controllers because of bugs
  {
    smart_device * newdev = smi()->autodetect_sat_device(this, req_buff, len);
    if (newdev) {
      // NOTE: 'this' is now owned by '*newdev'
      if(!strcmp("mfi",m_camdev->sim_name)) {
        newdev->close();
        newdev->set_err(ENOSYS, "SATA device detected,\n"
          "MegaRAID SAT layer is reportedly buggy, use '-d sat' to try anyhow");
      }
      return newdev;
    }
  }

  // Nothing special found
  return this;
}


/////////////////////////////////////////////////////////////////////////////
/// Implement platform interface with old functions.

class freebsd_smart_interface
: public /*implements*/ smart_interface
{
public:
  virtual std::string get_os_version_str();

  virtual std::string get_app_examples(const char * appname);

  virtual bool scan_smart_devices(smart_device_list & devlist, const char * type,
    const char * pattern = 0);

protected:
  virtual ata_device * get_ata_device(const char * name, const char * type);

#if FREEBSDVER > 800100
  virtual ata_device * get_atacam_device(const char * name, const char * type);
#endif

  virtual scsi_device * get_scsi_device(const char * name, const char * type);

  virtual smart_device * autodetect_smart_device(const char * name);

  virtual smart_device * get_custom_smart_device(const char * name, const char * type);

  virtual std::string get_valid_custom_dev_types_str();
};


//////////////////////////////////////////////////////////////////////

std::string freebsd_smart_interface::get_os_version_str()
{
  struct utsname osname;
  uname(&osname);
  return strprintf("%s %s %s", osname.sysname, osname.release, osname.machine);
}

std::string freebsd_smart_interface::get_app_examples(const char * appname)
{
  if (!strcmp(appname, "smartctl"))
    return smartctl_examples;
  return "";
}

ata_device * freebsd_smart_interface::get_ata_device(const char * name, const char * type)
{
  return new freebsd_ata_device(this, name, type);
}

#if FREEBSDVER > 800100
ata_device * freebsd_smart_interface::get_atacam_device(const char * name, const char * type)
{
  return new freebsd_atacam_device(this, name, type);
}
#endif

scsi_device * freebsd_smart_interface::get_scsi_device(const char * name, const char * type)
{
  return new freebsd_scsi_device(this, name, type);
}

// we are using CAM subsystem XPT enumerator to found all CAM (scsi/usb/ada/...)
// devices on system despite of it's names
//
// If any errors occur, leave errno set as it was returned by the
// system call, and return <0.
//
// arguments: 
// names: resulting array
// show_all - export duplicate device name or not
//
// Return values:
// -1:   error
// >=0: number of discovered devices

bool get_dev_names_cam(std::vector<std::string> & names, bool show_all)
{
  int fd;
  if ((fd = open(XPT_DEVICE, O_RDWR)) == -1) {
    if (errno == ENOENT) /* There are no CAM device on this computer */
      return 0;
    int serrno = errno;
    pout("%s control device couldn't opened: %s\n", XPT_DEVICE, strerror(errno));
    errno = serrno;
    return false;
  }

  union ccb ccb;
  bzero(&ccb, sizeof(union ccb));

  ccb.ccb_h.path_id = CAM_XPT_PATH_ID;
  ccb.ccb_h.target_id = CAM_TARGET_WILDCARD;
  ccb.ccb_h.target_lun = CAM_LUN_WILDCARD;

  ccb.ccb_h.func_code = XPT_DEV_MATCH;
  int bufsize = sizeof(struct dev_match_result) * MAX_NUM_DEV;
  ccb.cdm.match_buf_len = bufsize;
  // TODO: Use local buffer instead of malloc() if possible
  ccb.cdm.matches = (struct dev_match_result *)malloc(bufsize);
  bzero(ccb.cdm.matches,bufsize); // clear ccb.cdm.matches structure
  
  if (ccb.cdm.matches == NULL) {
    close(fd);
    throw std::bad_alloc();
  }
  ccb.cdm.num_matches = 0;
  ccb.cdm.num_patterns = 0;
  ccb.cdm.pattern_buf_len = 0;

  /*
   * We do the ioctl multiple times if necessary, in case there are
   * more than MAX_NUM_DEV nodes in the EDT.
   */
  int skip_device = 0, skip_bus = 0, changed = 0; // TODO: bool
  std::string devname;
  do {
    if (ioctl(fd, CAMIOCOMMAND, &ccb) == -1) {
      int serrno = errno;
      pout("error sending CAMIOCOMMAND ioctl: %s\n", strerror(errno));
      free(ccb.cdm.matches);
      close(fd);
      errno = serrno;
      return false;
    }

    if ((ccb.ccb_h.status != CAM_REQ_CMP)
      || ((ccb.cdm.status != CAM_DEV_MATCH_LAST)
      && (ccb.cdm.status != CAM_DEV_MATCH_MORE))) {
      pout("got CAM error %#x, CDM error %d\n", ccb.ccb_h.status, ccb.cdm.status);
      free(ccb.cdm.matches);
      close(fd);
      errno = ENXIO;
      return false;
    }

    for (unsigned i = 0; i < ccb.cdm.num_matches; i++) {
      struct bus_match_result *bus_result;
      struct device_match_result *dev_result;
      struct periph_match_result *periph_result;

      if (ccb.cdm.matches[i].type == DEV_MATCH_BUS) {
        bus_result = &ccb.cdm.matches[i].result.bus_result;

        if (strcmp(bus_result->dev_name,"xpt") == 0) /* skip XPT bus at all */
        skip_bus = 1;
        else
          skip_bus = 0;
        changed = 1;
      } else if (ccb.cdm.matches[i].type == DEV_MATCH_DEVICE) {
        dev_result = &ccb.cdm.matches[i].result.device_result;

        if (dev_result->flags & DEV_RESULT_UNCONFIGURED || skip_bus == 1)
          skip_device = 1;
        else
          skip_device = 0;
        
        //        /* Shall we skip non T_DIRECT devices ? */
        //        if (dev_result->inq_data.device != T_DIRECT)
        //          skip_device = 1;
        changed = 1;
      } else if (ccb.cdm.matches[i].type == DEV_MATCH_PERIPH && 
          (skip_device == 0 || show_all)) { 
        /* One device may be populated as many peripherals (pass0 & da0 for example). 
        * We are searching for best name
        */
        periph_result =  &ccb.cdm.matches[i].result.periph_result;
        /* Prefer non-"pass" names */
        if (devname.empty() || strncmp(periph_result->periph_name, "pass", 4) != 0) {
          devname = strprintf("%s%s%d", _PATH_DEV, periph_result->periph_name, periph_result->unit_number);
	    }
        changed = 0;
      };
      if ((changed == 1 || show_all) && !devname.empty()) {
        names.push_back(devname);
        devname.erase();
        changed = 0;
      };
    }

  } while ((ccb.ccb_h.status == CAM_REQ_CMP) && (ccb.cdm.status == CAM_DEV_MATCH_MORE));

  if (!devname.empty())
    names.push_back(devname);

  free(ccb.cdm.matches);
  close(fd);
  return true;
}

// we are using ATA subsystem enumerator to found all ATA devices on system
// despite of it's names
//
// If any errors occur, leave errno set as it was returned by the
// system call, and return <0.

// Return values:
// -1:   error
// >=0: number of discovered devices
int get_dev_names_ata(char*** names) {
  struct ata_ioc_devices devices;
  int fd=-1,maxchannel,serrno=-1,n=0;
  char **mp = NULL;

  *names=NULL;

  if ((fd = open(ATA_DEVICE, O_RDWR)) < 0) {
    if (errno == ENOENT) /* There are no ATA device on this computer */
      return 0;
    serrno = errno;
    pout("%s control device can't be opened: %s\n", ATA_DEVICE, strerror(errno));
    n = -1;
    goto end;
  };

  if (ioctl(fd, IOCATAGMAXCHANNEL, &maxchannel) < 0) {
    serrno = errno;
    pout("ioctl(IOCATAGMAXCHANNEL) on /dev/ata failed: %s\n", strerror(errno));
    n = -1;
    goto end;
  };

  // allocate space for up to MAX_NUM_DEV number of ATA devices
  mp =  (char **)calloc(MAX_NUM_DEV, sizeof(char*));
  if (mp == NULL) {
    serrno=errno;
    pout("Out of memory constructing scan device list (on line %d)\n", __LINE__);
    n = -1;
    goto end;
  };

  for (devices.channel = 0; devices.channel < maxchannel && n < MAX_NUM_DEV; devices.channel++) {
    int j;

    if (ioctl(fd, IOCATADEVICES, &devices) < 0) {
      if (errno == ENXIO)
        continue; /* such channel not exist */
      pout("ioctl(IOCATADEVICES) on %s channel %d failed: %s\n", ATA_DEVICE, devices.channel, strerror(errno));
      n = -1;
      goto end;
    };
    for (j=0;j<=1 && n<MAX_NUM_DEV;j++) {
      if (devices.name[j][0] != '\0') {
        asprintf(mp+n, "%s%s", _PATH_DEV, devices.name[j]);
        if (mp[n] == NULL) {
          pout("Out of memory constructing scan ATA device list (on line %d)\n", __LINE__);
          n = -1;
          goto end;
        };
        bytes+=1+strlen(mp[n]);
        n++;
      };
    };
  };  
  mp = (char **)reallocf(mp,n*(sizeof (char*))); // shrink to correct size
  if (mp == NULL && n > 0 ) { // reallocf never fail for size=0, but may return NULL
    serrno=errno;
    pout("Out of memory constructing scan device list (on line %d)\n", __LINE__);
    n = -1;
    goto end;
  };
  bytes += (n)*(sizeof(char*)); // and set allocated byte count

end:
  if (fd>=0)
    close(fd);
  if (n <= 0) {
    free(mp);
    mp = NULL;
  }

  *names=mp;

  if (serrno>-1)
    errno=serrno;
  return n;
}



bool freebsd_smart_interface::scan_smart_devices(smart_device_list & devlist,
  const char * type, const char * pattern /*= 0*/)
{
  if (pattern) {
    set_err(EINVAL, "DEVICESCAN with pattern not implemented yet");
    return false;
  }

  // Make namelists
  char * * atanames = 0; int numata = 0;
  if (!type || !strcmp(type, "ata")) {
    numata = get_dev_names_ata(&atanames);
    if (numata < 0) {
      set_err(ENOMEM);
      return false;
    }
  }

  std::vector<std::string> scsinames;
  if (!type || !strcmp(type, "scsi")) { // do not export duplicated names
    if (!get_dev_names_cam(scsinames, false)) {
      set_err(errno);
      return false;
    }
  }

  // Add to devlist
  int i;
  if (type==NULL)
    type="";
  for (i = 0; i < numata; i++) {
    ata_device * atadev = get_ata_device(atanames[i], type);
    if (atadev)
      devlist.push_back(atadev);
    free(atanames[i]);
  }
  if(numata) free(atanames);

  for (i = 0; i < (int)scsinames.size(); i++) {
    if(!*type) { // try USB autodetection if no type specified
      smart_device * smartdev = autodetect_smart_device(scsinames[i].c_str());
      if(smartdev)
        devlist.push_back(smartdev);
    }
    else {
      scsi_device * scsidev = get_scsi_device(scsinames[i].c_str(), type);
      if (scsidev)
        devlist.push_back(scsidev);
    }
  }
  return true;
}


#if (FREEBSDVER < 800000) // without this build fail on FreeBSD 8
static char done[USB_MAX_DEVICES];

static int usbdevinfo(int f, int a, int rec, int busno, unsigned short & vendor_id,
  unsigned short & product_id, unsigned short & version)
{ 

  struct usb_device_info di;
  int e, p, i;
  char devname[256];

  snprintf(devname, sizeof(devname),"umass%d",busno);

  di.udi_addr = a;
  e = ioctl(f, USB_DEVICEINFO, &di);
  if (e) {
    if (errno != ENXIO)
      printf("addr %d: I/O error\n", a);
    return 0;
  }
  done[a] = 1;

  // list devices
  for (i = 0; i < USB_MAX_DEVNAMES; i++) {
    if (di.udi_devnames[i][0]) {
      if(strcmp(di.udi_devnames[i],devname)==0) {
        // device found!
        vendor_id = di.udi_vendorNo;
        product_id = di.udi_productNo;
        version = di.udi_releaseNo;
        return 1;
        // FIXME
      }
    }
  }
  if (!rec)
    return 0;
  for (p = 0; p < di.udi_nports; p++) {
    int s = di.udi_ports[p];
    if (s >= USB_MAX_DEVICES) {
      continue;
    }
    if (s == 0)
      printf("addr 0 should never happen!\n");
    else {
      if(usbdevinfo(f, s, 1, busno, vendor_id, product_id, version)) return 1;
    }
  }
  return 0;
}
#endif


static int usbdevlist(int busno,unsigned short & vendor_id,
  unsigned short & product_id, unsigned short & version)
{
#if (FREEBSDVER >= 800000) // libusb2 interface
  struct libusb20_device *pdev = NULL;
  struct libusb20_backend *pbe;
  uint32_t matches = 0;
  char buf[128]; // do not change!
  char devname[128];
  uint8_t n;
  struct LIBUSB20_DEVICE_DESC_DECODED *pdesc;

  pbe = libusb20_be_alloc_default();

  while ((pdev = libusb20_be_device_foreach(pbe, pdev))) {
    matches++;

    if (libusb20_dev_open(pdev, 0)) {
      warnx("libusb20_dev_open: could not open device");
      return 0;
    }

    pdesc=libusb20_dev_get_device_desc(pdev);

    snprintf(devname, sizeof(devname),"umass%d:",busno);
    for (n = 0; n != 255; n++) {
      if (libusb20_dev_get_iface_desc(pdev, n, buf, sizeof(buf)))
        break;
      if (buf[0] == 0)
        continue;
      if(strncmp(buf,devname,strlen(devname))==0){
        // found!
        vendor_id = pdesc->idVendor;
        product_id = pdesc->idProduct;
        version = pdesc->bcdDevice;
        libusb20_dev_close(pdev);
        libusb20_be_free(pbe);
        return 1;
      }
    }

    libusb20_dev_close(pdev);
  }

  if (matches == 0) {
    printf("No device match or lack of permissions.\n");
  }

  libusb20_be_free(pbe);

  return false;
#else // freebsd < 8.0 USB stack, ioctl interface

  int  i, f, a, rc;
  char buf[50];
  int ncont;

  for (ncont = 0, i = 0; i < 10; i++) {
    snprintf(buf, sizeof(buf), "%s%d", USBDEV, i);
    f = open(buf, O_RDONLY);
    if (f >= 0) {
      memset(done, 0, sizeof done);
      for (a = 1; a < USB_MAX_DEVICES; a++) {
        if (!done[a]) {
          rc = usbdevinfo(f, a, 1, busno,vendor_id, product_id, version);
          if(rc) return 1;
        }

      }
      close(f);
    } else {
      if (errno == ENOENT || errno == ENXIO)
        continue;
      warn("%s", buf);
    }
    ncont++;
  }
  return 0;
#endif
}

smart_device * freebsd_smart_interface::autodetect_smart_device(const char * name)
{
  unsigned short vendor_id = 0, product_id = 0, version = 0;
  struct cam_device *cam_dev;
  union ccb ccb;
  int bus=-1;
  int i,c;
  int len;
  const char * test_name = name;

  // if dev_name null, or string length zero
  if (!name || !(len = strlen(name)))
    return 0;

  // Dereference symlinks
  struct stat st;
  std::string pathbuf;
  if (!lstat(name, &st) && S_ISLNK(st.st_mode)) {
    char * p = realpath(name, (char *)0);
    if (p) {
      pathbuf = p;
      free(p);
      test_name = pathbuf.c_str();
    }
  }

  // check ATA bus
  char * * atanames = 0; int numata = 0;
  numata = get_dev_names_ata(&atanames);
  if (numata > 0) {
    // check ATA/ATAPI devices
    for (i = 0; i < numata; i++) {
      if(!strcmp(atanames[i],test_name)) {
        for (c = i; c < numata; c++) free(atanames[c]);
        free(atanames);
        return new freebsd_ata_device(this, test_name, "");
      }
      else free(atanames[i]);
    }
    if(numata) free(atanames);
  }
  else {
    if (numata < 0)
      pout("Unable to get ATA device list\n");
  }

  // check CAM
  std::vector<std::string> scsinames;
  if (!get_dev_names_cam(scsinames, true))
    pout("Unable to get CAM device list\n");
  else if (!scsinames.empty()) {
    // check all devices on CAM bus
    for (i = 0; i < (int)scsinames.size(); i++) {
      if(strcmp(scsinames[i].c_str(), test_name)==0)
      { // our disk device is CAM
        if(strncmp(scsinames[i].c_str(), "/dev/pmp", strlen("/dev/pmp")) == 0) {
          pout("Skipping port multiplier [%s]\n", scsinames[i].c_str());
          set_err(EINVAL);
          return 0;
        }
        if ((cam_dev = cam_open_device(test_name, O_RDWR)) == NULL) {
          // open failure
          set_err(errno);
          return 0;
        }
        // zero the payload
        bzero(&(&ccb.ccb_h)[1], PATHINQ_SETTINGS_SIZE);
        ccb.ccb_h.func_code = XPT_PATH_INQ; // send PATH_INQ to the device
        if (ioctl(cam_dev->fd, CAMIOCOMMAND, &ccb) == -1) {
          warn("Get Transfer Settings CCB failed\n"
            "%s", strerror(errno));
          cam_close_device(cam_dev);
          return 0;
        }
        // now check if we are working with USB device, see umass.c
        if(strcmp(ccb.cpi.dev_name,"umass-sim") == 0) { // USB device found
          usbdevlist(bus,vendor_id, product_id, version);
          int bus=ccb.cpi.unit_number; // unit_number will match umass number
          cam_close_device(cam_dev);
          if(usbdevlist(bus,vendor_id, product_id, version)){
            const char * usbtype = get_usb_dev_type_by_id(vendor_id, product_id, version);
            if (usbtype)
              return get_sat_device(usbtype, new freebsd_scsi_device(this, test_name, ""));
          }
          return 0;
        }
#if FREEBSDVER > 800100
        // check if we have ATA device connected to CAM (ada)
        if(ccb.cpi.protocol == PROTO_ATA){
          cam_close_device(cam_dev);
          return new freebsd_atacam_device(this, test_name, "");
        }
#endif
        // close cam device, we don`t need it anymore
        cam_close_device(cam_dev);
        // handle as usual scsi
        return new freebsd_scsi_device(this, test_name, "");      
      }
    }
  }
  // device is LSI raid supported by mfi driver
  if(!strncmp("/dev/mfid", test_name, strlen("/dev/mfid")))
    set_err(EINVAL, "To monitor disks on LSI RAID load mfip.ko module and run 'smartctl -a /dev/passX' to show SMART information");
  // device type unknown
  return 0;
}


smart_device * freebsd_smart_interface::get_custom_smart_device(const char * name, const char * type)
{
  // 3Ware ?
  static const char * fbsd_dev_twe_ctrl = "/dev/twe";
  static const char * fbsd_dev_twa_ctrl = "/dev/twa";
  static const char * fbsd_dev_tws_ctrl = "/dev/tws";
  int disknum = -1, n1 = -1, n2 = -1, contr = -1;

  if (sscanf(type, "3ware,%n%d%n", &n1, &disknum, &n2) == 1 || n1 == 6) {
    if (n2 != (int)strlen(type)) {
      set_err(EINVAL, "Option -d 3ware,N requires N to be a non-negative integer");
      return 0;
    }
    if (!(0 <= disknum && disknum <= 127)) {
      set_err(EINVAL, "Option -d 3ware,N (N=%d) must have 0 <= N <= 127", disknum);
      return 0;
    }

    // guess 3ware device type based on device name
    if (str_starts_with(name, fbsd_dev_twa_ctrl) ||
        str_starts_with(name, fbsd_dev_tws_ctrl)   ) {
      contr=CONTROLLER_3WARE_9000_CHAR;
    }
    if (!strncmp(fbsd_dev_twe_ctrl, name, strlen(fbsd_dev_twe_ctrl))){
      contr=CONTROLLER_3WARE_678K_CHAR;
    }

    if(contr == -1){
      set_err(EINVAL, "3ware controller type unknown, use %sX, %sX or %sX devices", 
        fbsd_dev_twe_ctrl, fbsd_dev_twa_ctrl, fbsd_dev_tws_ctrl);
      return 0;
    }
    return new freebsd_escalade_device(this, name, contr, disknum);
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
    if (!(1 <= channel && channel <= 128)) {
      set_err(EINVAL, "Option '-d hpt,L/M/N' invalid channel number M supplied");
      return 0;
    }
    if (!(1 <= disknum && disknum <= 15)) {
      set_err(EINVAL, "Option '-d hpt,L/M/N' invalid pmport number N supplied");
      return 0;
    }
    return new freebsd_highpoint_device(this, name, controller, channel, disknum);
  }

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
    return get_sat_device("sat,auto", new freebsd_cciss_device(this, name, disknum));
  }
#if FREEBSDVER > 800100
  // adaX devices ?
  if(!strcmp(type,"atacam"))
    return new freebsd_atacam_device(this, name, "");
#endif
  // Areca?
  disknum = n1 = n2 = -1;
  int encnum = 1;
  if (sscanf(type, "areca,%n%d/%d%n", &n1, &disknum, &encnum, &n2) >= 1 || n1 == 6) {
    if (!(1 <= disknum && disknum <= 128)) {
      set_err(EINVAL, "Option -d areca,N/E (N=%d) must have 1 <= N <= 128", disknum);
      return 0;
    }
    if (!(1 <= encnum && encnum <= 8)) {
      set_err(EINVAL, "Option -d areca,N/E (E=%d) must have 1 <= E <= 8", encnum);
      return 0;
    }
    return new freebsd_areca_ata_device(this, name, disknum, encnum);
  }

  return 0;
}

std::string freebsd_smart_interface::get_valid_custom_dev_types_str()
{
  return "3ware,N, hpt,L/M/N, cciss,N, areca,N/E"
#if FREEBSDVER > 800100
  ", atacam"
#endif
  ;
}

} // namespace

/////////////////////////////////////////////////////////////////////////////
/// Initialize platform interface and register with smi()

void smart_interface::init()
{
  static os_freebsd::freebsd_smart_interface the_interface;
  smart_interface::set(&the_interface);
}
