/*
 * dev_areca.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2012 Hank Wu <hank@areca.com.tw>
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

#ifndef DEV_ARECA_H
#define DEV_ARECA_H

#define DEV_ARECA_H_CVSID "$Id$"

/////////////////////////////////////////////////////////////////////////////
/// Areca RAID support

/* GENERIC ARECA IO CONTROL CODE*/
enum _GENERIC_ARCMSR_CMDS
{
ARCMSR_READ_RQBUFFER = 0,
ARCMSR_WRITE_WQBUFFER,
ARCMSR_CLEAR_RQBUFFER,
ARCMSR_CLEAR_WQBUFFER,
ARCMSR_RETURN_CODE_3F,
ARCMSR_CMD_TOTAL
};

#define ARECA_SIG_STR  "ARCMSR"

#if defined(_WIN32) || defined(__CYGWIN__)
#define ARCMSR_IOCTL_READ_RQBUFFER           0x90002004
#define ARCMSR_IOCTL_WRITE_WQBUFFER          0x90002008
#define ARCMSR_IOCTL_CLEAR_RQBUFFER          0x9000200C
#define ARCMSR_IOCTL_CLEAR_WQBUFFER          0x90002010
#define ARCMSR_IOCTL_RETURN_CODE_3F          0x90002018
#elif defined(__linux__)
/*DeviceType*/
#define ARECA_SATA_RAID                      0x90000000
/*FunctionCode*/
#define FUNCTION_READ_RQBUFFER               0x0801
#define FUNCTION_WRITE_WQBUFFER              0x0802
#define FUNCTION_CLEAR_RQBUFFER              0x0803
#define FUNCTION_CLEAR_WQBUFFER              0x0804
#define FUNCTION_RETURN_CODE_3F              0x0806

/* ARECA IO CONTROL CODE*/
#define ARCMSR_IOCTL_READ_RQBUFFER           (ARECA_SATA_RAID | FUNCTION_READ_RQBUFFER)
#define ARCMSR_IOCTL_WRITE_WQBUFFER          (ARECA_SATA_RAID | FUNCTION_WRITE_WQBUFFER)
#define ARCMSR_IOCTL_CLEAR_RQBUFFER          (ARECA_SATA_RAID | FUNCTION_CLEAR_RQBUFFER)
#define ARCMSR_IOCTL_CLEAR_WQBUFFER          (ARECA_SATA_RAID | FUNCTION_CLEAR_WQBUFFER)
#define ARCMSR_IOCTL_RETURN_CODE_3F          (ARECA_SATA_RAID | FUNCTION_RETURN_CODE_3F)
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/ioctl.h> // _IOWR

/*FunctionCode*/
#define FUNCTION_READ_RQBUFFER               0x0801
#define FUNCTION_WRITE_WQBUFFER              0x0802
#define FUNCTION_CLEAR_RQBUFFER              0x0803
#define FUNCTION_CLEAR_WQBUFFER              0x0804
#define FUNCTION_RETURN_CODE_3F              0x0806

/* ARECA IO CONTROL CODE*/
#define ARCMSR_IOCTL_READ_RQBUFFER           _IOWR('F', FUNCTION_READ_RQBUFFER, sSRB_BUFFER)
#define ARCMSR_IOCTL_WRITE_WQBUFFER          _IOWR('F', FUNCTION_WRITE_WQBUFFER, sSRB_BUFFER)
#define ARCMSR_IOCTL_CLEAR_RQBUFFER          _IOWR('F', FUNCTION_CLEAR_RQBUFFER, sSRB_BUFFER)
#define ARCMSR_IOCTL_CLEAR_WQBUFFER          _IOWR('F', FUNCTION_CLEAR_WQBUFFER, sSRB_BUFFER)
#define ARCMSR_IOCTL_RETURN_CODE_3F          _IOWR('F', FUNCTION_RETURN_CODE_3F, sSRB_BUFFER)
#endif


// The SRB_IO_CONTROL & SRB_BUFFER structures are used to communicate(to/from) to areca driver
typedef struct _ARCMSR_IO_HDR
{
  unsigned int HeaderLength;
  unsigned char Signature[8];
  unsigned int Timeout;
  unsigned int ControlCode;
  unsigned int ReturnCode;
  unsigned int Length;
} sARCMSR_IO_HDR;

typedef struct _SRB_BUFFER
{
  sARCMSR_IO_HDR  srbioctl;
  unsigned char   ioctldatabuffer[1032]; // the buffer to put the command data to/from firmware
} sSRB_BUFFER;

class generic_areca_device :
virtual public smart_device
{
public:
  generic_areca_device(smart_interface * intf, const char * dev_name, int disknum, int encnum = 1);
  ~generic_areca_device() throw();

  /////////////////////////////////////////////////////////////////////
  // OS-dependent functions
  virtual bool arcmsr_lock() = 0;
  virtual bool arcmsr_unlock() = 0;
  virtual int arcmsr_do_scsi_io(struct scsi_cmnd_io * iop) = 0;

  /////////////////////////////////////////////////////////////////////
  // OS-independent functions
  virtual int arcmsr_command_handler(unsigned long arcmsr_cmd, unsigned char *data, int data_len);
  virtual int arcmsr_ui_handler(unsigned char *areca_packet, int areca_packet_len, unsigned char *result);
  virtual bool arcmsr_probe();
  virtual int arcmsr_get_dev_type();
  virtual int arcmsr_get_controller_type();
  virtual bool arcmsr_scsi_pass_through(scsi_cmnd_io * iop);
  virtual bool arcmsr_ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

protected:
  generic_areca_device() : smart_device(never_called)
  {
  }

  void set_disknum(int disknum)
  {m_disknum = disknum;}

  void set_encnum(int encnum)
  {m_encnum = encnum;}

  int get_disknum()
  {return m_disknum;}

  int get_encnum()
  {return m_encnum;}

private:
  int m_disknum; ///< Disk number.
  int m_encnum;  ///< Enclosure number.
 };

// SATA(ATA) device behind Areca RAID Controller
class areca_ata_device
: public ata_device,
  public generic_areca_device
{
public:
  areca_ata_device(smart_interface * intf, const char * dev_name, int disknum, int encnum = 1);
  ~areca_ata_device() throw();
  bool arcmsr_lock() { return true; }
  bool arcmsr_unlock() { return true; }
  int arcmsr_do_scsi_io(struct scsi_cmnd_io * /* iop */)
  {
      return -1;
  }
protected:
  areca_ata_device(): smart_device(never_called)
  {
  }
  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);
};

// SAS(SCSI) device behind Areca RAID Controller
class areca_scsi_device
: public scsi_device,
  public generic_areca_device
{
public:
  areca_scsi_device(smart_interface * intf, const char * dev_name, int disknum, int encnum = 1);
  ~areca_scsi_device() throw();
  bool arcmsr_lock() { return true; }
  bool arcmsr_unlock() { return true; }
  int arcmsr_do_scsi_io(struct scsi_cmnd_io * /* iop */)
  {
      return -1;
  }
protected:
  areca_scsi_device(): smart_device(never_called)
  {
  }
  virtual bool scsi_pass_through(scsi_cmnd_io * iop);
};

#endif
