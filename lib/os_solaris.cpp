/*
 * os_solaris.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2003-08 SAWADA Keiji
 * Copyright (C) 2003-15 Casper Dik
 * Copyright (C) 2026 OmniOS Community Edition (OmniOSce) Association
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <glob.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/scsi/impl/uscsi.h>

#include "config.h"

#include <smartmon/scsicmds.h>
#include <smartmon/utility.h>
#include <smartmon/dev_interface.h>

#include <string>
#include <vector>

namespace smartmon {

namespace os_solaris { // No need to publish anything, name provided for Doxygen

// Return true if the device node that 'dev_name' resolves to is named
// after one of the drivers in 'table'.
static bool is_dev_type(const char * dev_name, const char * const * table,
  size_t size)
{
  char devpath[MAXPATHLEN];
  if (!realpath(dev_name, devpath))
    return false;

  const char * basename = strrchr(devpath, '/');
  if (!basename)
    return false;
  basename++;

  for (size_t i = 0; i < size; i++) {
    size_t l = strlen(table[i]);
    if (!strncmp(basename, table[i], l) && basename[l] == '@')
      return true;
  }
  return false;
}

template <size_t SIZE>
static inline bool is_dev_type(const char * dev_name,
  const char * const (& table)[SIZE])
{
  return is_dev_type(dev_name, table, SIZE);
}

static bool is_scsi_dev(const char * path)
{
  // 'disk' is the SATA framework disk driver
  return is_dev_type(path, {"sd", "ssd", "disk", "st"});
}

static bool is_ata_dev(const char * path)
{
  return is_dev_type(path, {"cmdk", "dad"});
}

/////////////////////////////////////////////////////////////////////////////
/// Shared open/close routines

class solaris_smart_device
: virtual public /*implements*/ smart_device
{
public:
  solaris_smart_device()
    : smart_device(never_called),
      m_fd(-1) { }

  virtual ~solaris_smart_device();

  virtual bool is_open() const override;

  virtual bool open() override;

  virtual bool close() override;

protected:
  /// Return filedesc for derived classes.
  int get_fd() const
    { return m_fd; }

private:
  int m_fd; ///< filedesc, -1 if not open.
};

solaris_smart_device::~solaris_smart_device()
{
  if (m_fd >= 0)
    ::close(m_fd);
}

bool solaris_smart_device::is_open() const
{
  return (m_fd >= 0);
}

bool solaris_smart_device::open()
{
  m_fd = ::open(get_dev_name(), O_RDWR | O_NONBLOCK);
  if (m_fd < 0) {
    set_err((errno == ENOENT || errno == ENOTDIR) ? ENODEV : errno);
    return false;
  }
  return true;
}

bool solaris_smart_device::close()
{
  int fd = m_fd; m_fd = -1;
  if (::close(fd) < 0) {
    set_err(errno);
    return false;
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Standard SCSI support via USCSICMD

class solaris_scsi_device
: public /*implements*/ scsi_device,
  public /*extends*/ solaris_smart_device
{
public:
  solaris_scsi_device(smart_interface * intf, const char * dev_name,
    const char * req_type);

  virtual bool scsi_pass_through(scsi_cmnd_io * iop) override;
};

solaris_scsi_device::solaris_scsi_device(smart_interface * intf,
  const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "scsi", req_type)
{
}

bool solaris_scsi_device::scsi_pass_through(scsi_cmnd_io * iop)
{
  int report = scsi_debugmode;

  if (report > 0) {
    const unsigned char * ucp = iop->cmnd;
    const char * np = scsi_get_opcode_name(ucp);

    lib_printf(" [%s: ", np ? np : "<unknown opcode>");
    for (unsigned k = 0; k < iop->cmnd_len; ++k)
      lib_printf("%02x ", ucp[k]);
    lib_printf("]\n");
    if (report > 1 && iop->dxfer_dir == DXFER_TO_DEVICE && iop->dxferp) {
      int trunc = (report < 4 && iop->dxfer_len > 256) ? 1 : 0;

      lib_printf("  Outgoing data, len=%d%s:\n", (int)iop->dxfer_len,
        (trunc ? " [only first 256 bytes shown]" : ""));
      dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len), 1);
    }
  }

  struct uscsi_cmd uscsi;
  memset(&uscsi, 0, sizeof(uscsi));

  uscsi.uscsi_cdb = reinterpret_cast<char *>(iop->cmnd);
  uscsi.uscsi_cdblen = iop->cmnd_len;
  uscsi.uscsi_timeout = (iop->timeout ? iop->timeout : SCSI_TIMEOUT_DEFAULT);
  uscsi.uscsi_bufaddr = reinterpret_cast<char *>(iop->dxferp);
  uscsi.uscsi_buflen = iop->dxfer_len;
  uscsi.uscsi_rqbuf = reinterpret_cast<char *>(iop->sensep);
  uscsi.uscsi_rqlen = iop->max_sense_len;

  switch (iop->dxfer_dir) {
    case DXFER_NONE:
    case DXFER_FROM_DEVICE:
      uscsi.uscsi_flags = USCSI_READ;
      break;
    case DXFER_TO_DEVICE:
      uscsi.uscsi_flags = USCSI_WRITE;
      break;
    default:
      return set_err(EINVAL);
  }
  uscsi.uscsi_flags |= (USCSI_ISOLATE | USCSI_RQENABLE | USCSI_SILENT);

  if (ioctl(get_fd(), USCSICMD, &uscsi)) {
    int err = errno;
    // errno is set to EIO when a non-zero SCSI completion status is given
    if (!(err == EIO && uscsi.uscsi_status))
      return set_err(err);
  }

  iop->scsi_status = uscsi.uscsi_status;
  iop->resid = uscsi.uscsi_resid;
  iop->resp_sense_len = iop->max_sense_len - uscsi.uscsi_rqresid;

  if (report > 0) {
    int len = iop->resp_sense_len;

    if (iop->scsi_status == SCSI_STATUS_CHECK_CONDITION && iop->sensep && len > 3) {
      if ((iop->sensep[0] & 0x7f) > 0x71)
        lib_printf("  status=%x: [desc] sense_key=%x asc=%x ascq=%x\n",
          iop->scsi_status, iop->sensep[1] & 0xf,
          iop->sensep[2], iop->sensep[3]);
      else
        lib_printf("  status=%x: sense_key=%x asc=%x ascq=%x\n",
          iop->scsi_status, iop->sensep[2] & 0xf,
          iop->sensep[12], iop->sensep[13]);
      if (report > 1) {
        lib_printf("  >>> Sense buffer, len=%d:\n", len);
        dStrHex(iop->sensep, ((len > 252) ? 252 : len), 1);
      }
    }
    else if (iop->scsi_status)
      lib_printf("  status=%x\n", iop->scsi_status);
    if (iop->resid)
      lib_printf("  dxfer_len=%zu, resid=%d\n", iop->dxfer_len, iop->resid);
    if (report > 1) {
      len = iop->dxfer_len - iop->resid;
      if (len > 0) {
        int trunc = (report < 4 && len > 256) ? 1 : 0;
        lib_printf("  Incoming data, len=%d%s:\n", len,
          (trunc ? " [only first 256 bytes shown]" : ""));
        dStrHex(iop->dxferp, (trunc ? 256 : len), 1);
      }
    }
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Solaris/illumos platform interface

class solaris_smart_interface
: public /*implements*/ smart_interface
{
public:
  virtual std::string get_os_version_str() override;

  virtual std::string get_app_examples(const char * appname) override;

  virtual bool scan_smart_devices(smart_device_list & devlist, const char * type,
    const char * pattern = 0) override;

protected:
  virtual ata_device * get_ata_device(const char * name, const char * type) override;

  virtual scsi_device * get_scsi_device(const char * name, const char * type) override;

  virtual smart_device * autodetect_smart_device(const char * name) override;
};

std::string solaris_smart_interface::get_os_version_str()
{
  struct utsname u;
  if (uname(&u) < 0)
    return smart_interface::get_os_version_str();
  return strprintf("%s %s %s %s", u.sysname, u.release, u.version, u.machine);
}

std::string solaris_smart_interface::get_app_examples(const char * appname)
{
  if (strcmp(appname, "smartctl"))
    return "";
  return
    "=================================================== SMARTCTL EXAMPLES =====\n\n"
    "  smartctl -a /dev/rdsk/c0t0d0s0             (Prints all SMART information)\n\n"
    "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/rdsk/c0t0d0s0\n"
    "                                              (Enables SMART on first disk)\n\n"
    "  smartctl -t long /dev/rdsk/c0t0d0s0 (Executes extended disk self-test)\n\n"
    "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/rdsk/c0t0d0s0\n"
    "                                      (Prints Self-Test & Attribute errors)\n";
}

ata_device * solaris_smart_interface::get_ata_device(const char * /*name*/,
  const char * /*type*/)
{
  return set_err_np(ENOSYS,
    "Device type 'ata' not implemented, try '-d sat' or '-d sat,12' instead");
}

scsi_device * solaris_smart_interface::get_scsi_device(const char * name,
  const char * type)
{
  return new solaris_scsi_device(this, name, type);
}

smart_device * solaris_smart_interface::autodetect_smart_device(const char * name)
{
  if (is_scsi_dev(name))
    return get_sat_device("sat,auto", get_scsi_device(name, ""));
  if (is_ata_dev(name))
    return get_ata_device(name, "");
  return nullptr;
}

// Add the paths matching 'dir_pattern' that pass 'testfun' and, if given,
// also match the user supplied shell 'pattern' to 'names'.
static void glob_dev_names(const char * dir_pattern, bool (* testfun)(const char *),
  const char * pattern, std::vector<std::string> & names)
{
  glob_t g;
  if (!glob(dir_pattern, 0, nullptr, &g)) {
    for (size_t i = 0; i < g.gl_pathc; i++) {
      const char * n = g.gl_pathv[i];
      if ((!pattern || !fnmatch(pattern, n, 0)) && testfun(n))
        names.push_back(n);
    }
  }
  globfree(&g);
}

// Tape drives are represented by the all-digit device.
static bool is_scsi_tape(const char * path)
{
  const char * n = strrchr(path, '/') + 1;
  return (!n[strspn(n, "0123456789")] && is_scsi_dev(path));
}

bool solaris_smart_interface::scan_smart_devices(smart_device_list & devlist,
  const char * type, const char * pattern /*= 0*/)
{
  if (!type)
    type = "";

  // Without a type, let SAT autodetection decide between SCSI and SAT
  const char * sat_type;
  if (!*type)
    sat_type = "sat,auto";
  else if (!strcmp(type, "sat"))
    sat_type = "sat";
  else if (!strcmp(type, "scsi"))
    sat_type = nullptr;
  else
    return true;

  std::vector<std::string> names;
  // Disks are represented by slice 0
  glob_dev_names("/dev/rdsk/*s0", is_scsi_dev, pattern, names);
  glob_dev_names("/dev/rmt/[0-9]*", is_scsi_tape, pattern, names);
  for (const auto & n : names) {
    scsi_device * scsidev = get_scsi_device(n.c_str(), (*type ? "scsi" : ""));
    smart_device * dev = scsidev;
    if (sat_type)
      dev = get_sat_device(sat_type, scsidev);
    if (dev)
      devlist.push_back(dev);
  }

  return true;
}

} // namespace os_solaris

/////////////////////////////////////////////////////////////////////////////
/// Initialize platform interface and register with smi()

void smart_interface::init()
{
  static os_solaris::solaris_smart_interface the_interface;
  smart_interface::set(&the_interface);
}

} // namespace smartmon
