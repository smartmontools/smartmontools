/*
 * dev_legacy.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2008-11 Christian Franke <smartmontools-support@lists.sourceforge.net>
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

#include "config.h"
#include "int64.h"
#include "utility.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "dev_interface.h"
#include "dev_ata_cmd_set.h"

#include <errno.h>

const char * dev_legacy_cpp_cvsid = "$Id$"
  DEV_INTERFACE_H_CVSID;

/////////////////////////////////////////////////////////////////////////////

// Legacy interface declarations (now commented out globally):

// from utility.h:
int guess_device_type(const char * dev_name);
int make_device_names (char ***devlist, const char* name);
int deviceopen(const char *pathname, char *type);
int deviceclose(int fd);

// from atacmds.h:
int ata_command_interface(int device, smart_command_set command, int select, char *data);

// from scsicmds.h:
int do_scsi_cmnd_io(int dev_fd, struct scsi_cmnd_io * iop, int report);

// from smartctl.h:
void print_smartctl_examples();

/////////////////////////////////////////////////////////////////////////////

namespace os { // No need to publish anything, name provided for Doxygen

/////////////////////////////////////////////////////////////////////////////
/// Implement shared open/close routines with old functions.

class legacy_smart_device
: virtual public /*implements*/ smart_device
{
public:
  explicit legacy_smart_device(const char * mode)
    : smart_device(never_called),
      m_fd(-1), m_mode(mode) { }

  virtual ~legacy_smart_device() throw();

  virtual bool is_open() const;

  virtual bool open();

  virtual bool close();

protected:
  /// Return filedesc for derived classes.
  int get_fd() const
    { return m_fd; }

private:
  int m_fd; ///< filedesc, -1 if not open.
  const char * m_mode; ///< Mode string for deviceopen().
};


legacy_smart_device::~legacy_smart_device() throw()
{
  if (m_fd >= 0)
    ::deviceclose(m_fd);
}

bool legacy_smart_device::is_open() const
{
  return (m_fd >= 0);
}

bool legacy_smart_device::open()
{
  m_fd = ::deviceopen(get_dev_name(), const_cast<char*>(m_mode));
  if (m_fd < 0) {
    set_err((errno==ENOENT || errno==ENOTDIR) ? ENODEV : errno);
    return false;
  }
  return true;
}

bool legacy_smart_device::close()
{
  int fd = m_fd; m_fd = -1;
  if (::deviceclose(fd) < 0) {
    set_err(errno);
    return false;
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Implement standard ATA support with old functions

class legacy_ata_device
: public /*implements*/ ata_device_with_command_set,
  public /*extends*/ legacy_smart_device
{
public:
  legacy_ata_device(smart_interface * intf, const char * dev_name, const char * req_type);

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);
};

legacy_ata_device::legacy_ata_device(smart_interface * intf, const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "ata", req_type),
  legacy_smart_device("ATA")
{
}

int legacy_ata_device::ata_command_interface(smart_command_set command, int select, char * data)
{
  return ::ata_command_interface(get_fd(), command, select, data);
}


/////////////////////////////////////////////////////////////////////////////
/// Implement standard SCSI support with old functions

class legacy_scsi_device
: public /*implements*/ scsi_device,
  public /*extends*/ legacy_smart_device
{
public:
  legacy_scsi_device(smart_interface * intf, const char * dev_name, const char * req_type);

  virtual smart_device * autodetect_open();

  virtual bool scsi_pass_through(scsi_cmnd_io * iop);
};

legacy_scsi_device::legacy_scsi_device(smart_interface * intf,
  const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "scsi", req_type),
  legacy_smart_device("SCSI")
{
}

bool legacy_scsi_device::scsi_pass_through(scsi_cmnd_io * iop)
{
  int status = ::do_scsi_cmnd_io(get_fd(), iop, scsi_debugmode);
  if (status < 0) {
      set_err(-status);
      return false;
  }
  return true;
}


/////////////////////////////////////////////////////////////////////////////
/// SCSI open with autodetection support

smart_device * legacy_scsi_device::autodetect_open()
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

  // SAT or USB ?
  {
    smart_device * newdev = smi()->autodetect_sat_device(this, req_buff, len);
    if (newdev)
      // NOTE: 'this' is now owned by '*newdev'
      return newdev;
  }

  // Nothing special found
  return this;
}


/////////////////////////////////////////////////////////////////////////////
/// Implement platform interface with old functions.

class legacy_smart_interface
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
};


//////////////////////////////////////////////////////////////////////

std::string legacy_smart_interface::get_app_examples(const char * appname)
{
  if (!strcmp(appname, "smartctl"))
    ::print_smartctl_examples(); // this prints to stdout ...
  return ""; // ... so don't print again.
}

ata_device * legacy_smart_interface::get_ata_device(const char * name, const char * type)
{
  return new legacy_ata_device(this, name, type);
}

scsi_device * legacy_smart_interface::get_scsi_device(const char * name, const char * type)
{
  return new legacy_scsi_device(this, name, type);
}


smart_device * legacy_smart_interface::autodetect_smart_device(const char * name)
{
  switch (::guess_device_type(name)) {
    case CONTROLLER_ATA : return new legacy_ata_device(this, name, "");
    case CONTROLLER_SCSI: return new legacy_scsi_device(this, name, "");
  }
  // TODO: Test autodetect device here
  return 0;
}


static void free_devnames(char * * devnames, int numdevs)
{
  static const char version[] = "$Id$";
  for (int i = 0; i < numdevs; i++)
    FreeNonZero(devnames[i], -1,__LINE__, version);
  FreeNonZero(devnames, (sizeof (char*) * numdevs),__LINE__, version);
}

bool legacy_smart_interface::scan_smart_devices(smart_device_list & devlist,
  const char * type, const char * pattern /*= 0*/)
{
  if (pattern) {
    set_err(EINVAL, "DEVICESCAN with pattern not implemented yet");
    return false;
  }

  // Make namelists
  char * * atanames = 0; int numata = 0;
  if (!type || !strcmp(type, "ata")) {
    numata = ::make_device_names(&atanames, "ATA");
    if (numata < 0) {
      set_err(ENOMEM);
      return false;
    }
  }

  char * * scsinames = 0; int numscsi = 0;
  if (!type || !strcmp(type, "scsi")) {
    numscsi = ::make_device_names(&scsinames, "SCSI");
    if (numscsi < 0) {
      free_devnames(atanames, numata);
      set_err(ENOMEM);
      return false;
    }
  }

  // Add to devlist
  int i;
  if (!type)
    type="";
  for (i = 0; i < numata; i++) {
    ata_device * atadev = get_ata_device(atanames[i], type);
    if (atadev)
      devlist.push_back(atadev);
  }
  free_devnames(atanames, numata);

  for (i = 0; i < numscsi; i++) {
    scsi_device * scsidev = get_scsi_device(scsinames[i], type);
    if (scsidev)
      devlist.push_back(scsidev);
  }
  free_devnames(scsinames, numscsi);
  return true;
}

} // namespace


/////////////////////////////////////////////////////////////////////////////
/// Initialize platform interface and register with smi()

void smart_interface::init()
{
  static os::legacy_smart_interface the_interface;
  smart_interface::set(&the_interface);
}
