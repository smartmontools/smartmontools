/*
 * dev_legacy.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2008-10 Christian Franke <smartmontools-support@lists.sourceforge.net>
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
#include "extern.h"
#include "utility.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "dev_interface.h"
#include "dev_ata_cmd_set.h"

const char * dev_legacy_cpp_cvsid = "$Id$"
  DEV_INTERFACE_H_CVSID;

extern smartmonctrl * con; // con->reportscsiioctl

/////////////////////////////////////////////////////////////////////////////

// Legacy interface declarations (now commented out globally):

// from utility.h:
int guess_device_type(const char * dev_name);
int make_device_names (char ***devlist, const char* name);
int deviceopen(const char *pathname, char *type);
int deviceclose(int fd);
#ifdef HAVE_GET_OS_VERSION_STR
const char * get_os_version_str(void);
#endif

// from atacmds.h:
int ata_command_interface(int device, smart_command_set command, int select, char *data);
int escalade_command_interface(int fd, int escalade_port, int escalade_type, smart_command_set command, int select, char *data);
int marvell_command_interface(int device, smart_command_set command, int select, char *data);
int highpoint_command_interface(int device, smart_command_set command, int select, char *data);
int areca_command_interface(int fd, int disknum, smart_command_set command, int select, char *data);
#ifdef HAVE_ATA_IDENTIFY_IS_CACHED
int ata_identify_is_cached(int fd);
#endif

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
  m_fd = ::deviceopen(get_dev_name(), (char*)m_mode);
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

#ifdef HAVE_ATA_IDENTIFY_IS_CACHED
  virtual bool ata_identify_is_cached() const;
#endif

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

#ifdef HAVE_ATA_IDENTIFY_IS_CACHED
bool legacy_ata_device::ata_identify_is_cached() const
{
  return !!::ata_identify_is_cached(get_fd());
}
#endif


/////////////////////////////////////////////////////////////////////////////
/// Implement AMCC/3ware RAID support with old functions

class legacy_escalade_device
: public /*implements*/ ata_device_with_command_set,
  public /*extends*/ legacy_smart_device
{
public:
  legacy_escalade_device(smart_interface * intf, const char * dev_name,
    int escalade_type, int disknum);

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);

private:
  int m_escalade_type; ///< Type string for escalade_command_interface().
  int m_disknum; ///< Disk number.
};

legacy_escalade_device::legacy_escalade_device(smart_interface * intf, const char * dev_name,
    int escalade_type, int disknum)
: smart_device(intf, dev_name, "3ware", "3ware"),
  legacy_smart_device(
    escalade_type==CONTROLLER_3WARE_9000_CHAR ? "ATA_3WARE_9000" :
    escalade_type==CONTROLLER_3WARE_678K_CHAR ? "ATA_3WARE_678K" :
    /*             CONTROLLER_3WARE_678K     */ "ATA"             ),
  m_escalade_type(escalade_type), m_disknum(disknum)
{
  set_info().info_name = strprintf("%s [3ware_disk_%02d]", dev_name, disknum);
}

int legacy_escalade_device::ata_command_interface(smart_command_set command, int select, char * data)
{
  return ::escalade_command_interface(get_fd(), m_disknum, m_escalade_type, command, select, data);
}


/////////////////////////////////////////////////////////////////////////////
/// Implement Areca RAID support with old functions

class legacy_areca_device
: public /*implements*/ ata_device_with_command_set,
  public /*extends*/ legacy_smart_device
{
public:
  legacy_areca_device(smart_interface * intf, const char * dev_name, int disknum);

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);

private:
  int m_disknum; ///< Disk number.
};

legacy_areca_device::legacy_areca_device(smart_interface * intf, const char * dev_name, int disknum)
: smart_device(intf, dev_name, "areca", "areca"),
  legacy_smart_device("ATA_ARECA"),
  m_disknum(disknum)
{
  set_info().info_name = strprintf("%s [areca_%02d]", dev_name, disknum);
}

int legacy_areca_device::ata_command_interface(smart_command_set command, int select, char * data)
{
  return ::areca_command_interface(get_fd(), m_disknum, command, select, data);
}


/////////////////////////////////////////////////////////////////////////////
/// Implement Marvell support with old functions

class legacy_marvell_device
: public /*implements*/ ata_device_with_command_set,
  public /*extends*/ legacy_smart_device
{
public:
  legacy_marvell_device(smart_interface * intf, const char * dev_name, const char * req_type);

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);
};


legacy_marvell_device::legacy_marvell_device(smart_interface * intf,
  const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "marvell", req_type),
  legacy_smart_device("ATA")
{
}

int legacy_marvell_device::ata_command_interface(smart_command_set command, int select, char * data)
{
  return ::marvell_command_interface(get_fd(), command, select, data);
}


/////////////////////////////////////////////////////////////////////////////
/// Implement Highpoint RAID support with old functions

class legacy_highpoint_device
: public /*implements*/ ata_device_with_command_set,
  public /*extends*/ legacy_smart_device
{
public:
  legacy_highpoint_device(smart_interface * intf, const char * dev_name,
    unsigned char controller, unsigned char channel, unsigned char port);

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);

private:
  unsigned char m_hpt_data[3]; ///< controller/channel/port
};


legacy_highpoint_device::legacy_highpoint_device(smart_interface * intf, const char * dev_name,
  unsigned char controller, unsigned char channel, unsigned char port)
: smart_device(intf, dev_name, "hpt", "hpt"),
  legacy_smart_device("ATA")
{
  m_hpt_data[0] = controller; m_hpt_data[1] = channel; m_hpt_data[2] = port;
  set_info().info_name = strprintf("%s [hpt_disk_%u/%u/%u]", dev_name, m_hpt_data[0], m_hpt_data[1], m_hpt_data[2]);
}

int legacy_highpoint_device::ata_command_interface(smart_command_set command, int select, char * data)
{
  unsigned char old_hpt_data[3];
  memcpy(old_hpt_data, con->hpt_data, 3);
  memcpy(con->hpt_data, m_hpt_data, 3);
  int status = ::highpoint_command_interface(get_fd(), command, select, data);
  memcpy(con->hpt_data, old_hpt_data, 3);
  return status;
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
  unsigned char oldtype = con->controller_type, oldport = con->controller_port;
  con->controller_type = CONTROLLER_SCSI; con->controller_port = 0;
  int status = ::do_scsi_cmnd_io(get_fd(), iop, con->reportscsiioctl);
  con->controller_type = oldtype; con->controller_port = oldport;
  if (status < 0) {
      set_err(-status);
      return false;
  }
  return true;
}


/////////////////////////////////////////////////////////////////////////////
/// Implement CCISS RAID support with old functions

class legacy_cciss_device
: public /*implements*/ scsi_device,
  public /*extends*/ legacy_smart_device
{
public:
  legacy_cciss_device(smart_interface * intf, const char * name, unsigned char disknum);

  virtual bool scsi_pass_through(scsi_cmnd_io * iop);

private:
  unsigned char m_disknum; ///< Disk number.
};


legacy_cciss_device::legacy_cciss_device(smart_interface * intf,
  const char * dev_name, unsigned char disknum)
: smart_device(intf, dev_name, "cciss", "cciss"),
  legacy_smart_device("SCSI"),
  m_disknum(disknum)
{
  set_info().info_name = strprintf("%s [cciss_disk_%02d]", dev_name, disknum);
}

bool legacy_cciss_device::scsi_pass_through(scsi_cmnd_io * iop)
{
  // See os_linux.cpp
  unsigned char oldtype = con->controller_type, oldport = con->controller_port;
  con->controller_type = CONTROLLER_CCISS; con->controller_port = m_disknum+1;
  int status = ::do_scsi_cmnd_io(get_fd(), iop, con->reportscsiioctl);
  con->controller_type = oldtype; con->controller_port = oldport;
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

  // 3ware ?
  if (!memcmp(req_buff + 8, "3ware", 5) || !memcmp(req_buff + 8, "AMCC", 4)) {
    close();
#if defined(_WIN32) || defined(__CYGWIN__)
    set_err(EINVAL, "AMCC/3ware controller, please try changing device to %s,N", get_dev_name());
#else
    set_err(EINVAL, "AMCC/3ware controller, please try adding '-d 3ware,N',\n"
                    "you may need to replace %s with /dev/twaN or /dev/tweN", get_dev_name());
#endif
    return this;
  }

  // Marvell ?
  if (len >= 42 && !memcmp(req_buff + 36, "MVSATA", 6)) { // TODO: Linux-specific?
    //pout("Device %s: using '-d marvell' for ATA disk with Marvell driver\n", get_dev_name());
    close();
    smart_device_auto_ptr newdev(
      new legacy_marvell_device(smi(), get_dev_name(), get_req_type()),
      this
    );
    newdev->open(); // TODO: Can possibly pass open fd
    delete this;
    return newdev.release();
  }

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
#ifdef HAVE_GET_OS_VERSION_STR
  virtual std::string get_os_version_str();
#endif

  virtual std::string get_app_examples(const char * appname);

  virtual bool scan_smart_devices(smart_device_list & devlist, const char * type,
    const char * pattern = 0);

protected:
  virtual ata_device * get_ata_device(const char * name, const char * type);

  virtual scsi_device * get_scsi_device(const char * name, const char * type);

  virtual smart_device * autodetect_smart_device(const char * name);

  virtual smart_device * get_custom_smart_device(const char * name, const char * type);

  virtual std::string get_valid_custom_dev_types_str();
};


//////////////////////////////////////////////////////////////////////

#ifdef HAVE_GET_OS_VERSION_STR
std::string legacy_smart_interface::get_os_version_str()
{
  return ::get_os_version_str();
}
#endif

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
  if (type==NULL)
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


smart_device * legacy_smart_interface::get_custom_smart_device(const char * name, const char * type)
{
  // Marvell ?
  if (!strcmp(type, "marvell"))
    return new legacy_marvell_device(this, name, type);

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
    int contr = ::guess_device_type(name);
    if (contr != CONTROLLER_3WARE_9000_CHAR && contr != CONTROLLER_3WARE_678K_CHAR)
      contr = CONTROLLER_3WARE_678K;
    return new legacy_escalade_device(this, name, contr, disknum);
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
    return new legacy_areca_device(this, name, disknum);
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
    return new legacy_highpoint_device(this, name, controller, channel, disknum);
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
    return new legacy_cciss_device(this, name, disknum);
  }

  return 0;
}

std::string legacy_smart_interface::get_valid_custom_dev_types_str()
{
  return "marvell, areca,N, 3ware,N, hpt,L/M/N, cciss,N";
}

} // namespace


/////////////////////////////////////////////////////////////////////////////
/// Initialize platform interface and register with smi()

void smart_interface::init()
{
  static os::legacy_smart_interface the_interface;
  smart_interface::set(&the_interface);
}
