/*
 * dev_interface.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2008-13 Christian Franke <smartmontools-support@lists.sourceforge.net>
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
#include "dev_interface.h"
#include "dev_tunnelled.h"
#include "atacmds.h" // ATA_SMART_CMD/STATUS
#include "utility.h"

#include <errno.h>
#include <stdarg.h>
#include <stdexcept>

#if defined(HAVE_GETTIMEOFDAY)
#include <sys/time.h>
#elif defined(HAVE_FTIME)
#include <sys/timeb.h>
#endif

const char * dev_interface_cpp_cvsid = "$Id$"
  DEV_INTERFACE_H_CVSID;

/////////////////////////////////////////////////////////////////////////////
// smart_device

smart_device::smart_device(smart_interface * intf, const char * dev_name,
    const char * dev_type, const char * req_type)
: m_intf(intf), m_info(dev_name, dev_type, req_type),
  m_ata_ptr(0), m_scsi_ptr(0)
{
}

smart_device::smart_device(do_not_use_in_implementation_classes)
: m_intf(0), m_ata_ptr(0), m_scsi_ptr(0)
{
  throw std::logic_error("smart_device: wrong constructor called in implementation class");
}

smart_device::~smart_device() throw()
{
}

bool smart_device::is_syscall_unsup() const
{
  if (get_errno() == ENOSYS)
    return true;
#ifdef ENOTSUP
  if (get_errno() == ENOTSUP)
    return true;
#endif
  return false;
}

bool smart_device::set_err(int no, const char * msg, ...)
{
  if (!msg)
    return set_err(no);
  m_err.no = no;
  va_list ap; va_start(ap, msg);
  m_err.msg = vstrprintf(msg, ap);
  va_end(ap);
  return false;
}

bool smart_device::set_err(int no)
{
  return smi()->set_err_var(&m_err, no);
}

smart_device * smart_device::autodetect_open()
{
  open();
  return this;
}

bool smart_device::owns(const smart_device * /*dev*/) const
{
  return false;
}

void smart_device::release(const smart_device * /*dev*/)
{
}


/////////////////////////////////////////////////////////////////////////////
// ata_device

ata_in_regs_48bit::ata_in_regs_48bit()
: features_16(features, prev.features),
  sector_count_16(sector_count, prev.sector_count),
  lba_low_16(lba_low, prev.lba_low),
  lba_mid_16(lba_mid, prev.lba_mid),
  lba_high_16(lba_high, prev.lba_high),
  lba_48(     lba_low,      lba_mid,      lba_high,
         prev.lba_low, prev.lba_mid, prev.lba_high)
{
}

ata_out_regs_48bit::ata_out_regs_48bit()
: sector_count_16(sector_count, prev.sector_count),
  lba_low_16(lba_low, prev.lba_low),
  lba_mid_16(lba_mid, prev.lba_mid),
  lba_high_16(lba_high, prev.lba_high),
  lba_48(     lba_low,      lba_mid,      lba_high,
         prev.lba_low, prev.lba_mid, prev.lba_high)
{
}

ata_cmd_in::ata_cmd_in()
: direction(no_data),
  buffer(0),
  size(0)
{
}

ata_cmd_out::ata_cmd_out()
{
}

bool ata_device::ata_pass_through(const ata_cmd_in & in)
{
  ata_cmd_out dummy;
  return ata_pass_through(in, dummy);
}

bool ata_device::ata_cmd_is_supported(const ata_cmd_in & in,
  unsigned flags, const char * type /* = 0 */)
{
  // Check DATA IN/OUT
  switch (in.direction) {
    case ata_cmd_in::no_data:  break;
    case ata_cmd_in::data_in:  break;
    case ata_cmd_in::data_out: break;
    default:
      return set_err(EINVAL, "Invalid data direction %d", (int)in.direction);
  }

  // Check buffer size
  if (in.direction == ata_cmd_in::no_data) {
    if (in.size)
      return set_err(EINVAL, "Buffer size %u > 0 for NO DATA command", in.size);
  }
  else {
    if (!in.buffer)
      return set_err(EINVAL, "Buffer not set for DATA IN/OUT command");
    unsigned count = (in.in_regs.prev.sector_count<<16)|in.in_regs.sector_count;
    // TODO: Add check for sector count == 0
    if (count * 512 != in.size)
      return set_err(EINVAL, "Sector count %u does not match buffer size %u", count, in.size);
  }

  // Check features
  const char * errmsg = 0;
  if (in.direction == ata_cmd_in::data_out && !(flags & supports_data_out))
    errmsg = "DATA OUT ATA commands not implemented";
  else if (   in.out_needed.is_set() && !(flags & supports_output_regs)
           && !(   in.in_regs.command == ATA_SMART_CMD
                && in.in_regs.features == ATA_SMART_STATUS
                && (flags & supports_smart_status)))
    errmsg = "Read of ATA output registers not implemented";
  else if (!(in.size == 0 || in.size == 512) && !(flags & supports_multi_sector))
    errmsg = "Multi-sector ATA commands not implemented";
  else if (in.in_regs.is_48bit_cmd() && !(flags & (supports_48bit_hi_null|supports_48bit)))
    errmsg = "48-bit ATA commands not implemented";
  else if (in.in_regs.is_real_48bit_cmd() && !(flags & supports_48bit))
    errmsg = "48-bit ATA commands not fully implemented";

  if (errmsg)
    return set_err(ENOSYS, "%s%s%s%s", errmsg,
                   (type ? " [" : ""), (type ? type : ""), (type ? "]" : ""));

  return true;
}

bool ata_device::ata_identify_is_cached() const
{
  return false;
}


/////////////////////////////////////////////////////////////////////////////
// tunnelled_device_base

tunnelled_device_base::tunnelled_device_base(smart_device * tunnel_dev)
: smart_device(never_called),
  m_tunnel_base_dev(tunnel_dev)
{
}

tunnelled_device_base::~tunnelled_device_base() throw()
{
  delete m_tunnel_base_dev;
}

bool tunnelled_device_base::is_open() const
{
  return (m_tunnel_base_dev && m_tunnel_base_dev->is_open());
}

bool tunnelled_device_base::open()
{
  if (!m_tunnel_base_dev)
    return set_err(ENOSYS);
  if (!m_tunnel_base_dev->open())
    return set_err(m_tunnel_base_dev->get_err());
  return true;
}

bool tunnelled_device_base::close()
{
  if (!m_tunnel_base_dev)
    return true;
  if (!m_tunnel_base_dev->close())
    return set_err(m_tunnel_base_dev->get_err());
  return true;
}

bool tunnelled_device_base::owns(const smart_device * dev) const
{
  return (m_tunnel_base_dev && (m_tunnel_base_dev == dev));
}

void tunnelled_device_base::release(const smart_device * dev)
{
  if (m_tunnel_base_dev == dev)
    m_tunnel_base_dev = 0;
}


/////////////////////////////////////////////////////////////////////////////
// smart_interface

// Pointer to (usually singleton) interface object returned by ::smi()
smart_interface * smart_interface::s_instance;

std::string smart_interface::get_os_version_str()
{
  return SMARTMONTOOLS_BUILD_HOST;
}

std::string smart_interface::get_valid_dev_types_str()
{
  // default
  std::string s =
    "ata, scsi, sat[,auto][,N][+TYPE], usbcypress[,X], usbjmicron[,p][,x][,N], usbsunplus";
  // append custom
  std::string s2 = get_valid_custom_dev_types_str();
  if (!s2.empty()) {
    s += ", "; s += s2;
  }
  return s;
}

std::string smart_interface::get_app_examples(const char * /*appname*/)
{
  return "";
}

int64_t smart_interface::get_timer_usec()
{
#if defined(HAVE_GETTIMEOFDAY)
 #if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
  {
    static bool have_clock_monotonic = true;
    if (have_clock_monotonic) {
      struct timespec ts;
      if (!clock_gettime(CLOCK_MONOTONIC, &ts))
        return ts.tv_sec * 1000000LL + ts.tv_nsec/1000;
      have_clock_monotonic = false;
    }
  }
 #endif
  {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec * 1000000LL + tv.tv_usec;
  }
#elif defined(HAVE_FTIME)
  {
    struct timeb tb;
    ftime(&tb);
    return tb.time * 1000000LL + tb.millitm * 1000;
  }
#else
  return -1;
#endif
}

bool smart_interface::disable_system_auto_standby(bool /*disable*/)
{
  return set_err(ENOSYS);
}

bool smart_interface::set_err(int no, const char * msg, ...)
{
  if (!msg)
    return set_err(no);
  m_err.no = no;
  va_list ap; va_start(ap, msg);
  m_err.msg = vstrprintf(msg, ap);
  va_end(ap);
  return false;
}

bool smart_interface::set_err(int no)
{
  return set_err_var(&m_err, no);
}

bool smart_interface::set_err_var(smart_device::error_info * err, int no)
{
  err->no = no;
  err->msg = get_msg_for_errno(no);
  if (err->msg.empty() && no != 0)
    err->msg = strprintf("Unknown error %d", no);
  return false;
}

const char * smart_interface::get_msg_for_errno(int no)
{
  return strerror(no);
}


/////////////////////////////////////////////////////////////////////////////
// Default device factory

smart_device * smart_interface::get_smart_device(const char * name, const char * type)
{
  clear_err();

  // Call platform specific autodetection if no device type specified
  smart_device * dev;
  if (!type || !*type) {
    dev = autodetect_smart_device(name);
    if (!dev && !get_errno())
      set_err(EINVAL, "Unable to detect device type");
    return dev;
  }

  // First check for platform specific device types
  dev = get_custom_smart_device(name, type);
  if (dev || get_errno())
    return dev;

  if (!strcmp(type, "ata"))
    dev = get_ata_device(name, type);
  else if (!strcmp(type, "scsi"))
    dev = get_scsi_device(name, type);

  else if (  ((!strncmp(type, "sat", 3) && (!type[3] || strchr(",+", type[3])))
           || (!strncmp(type, "usb", 3)))) {
    // Split "sat...+base..." -> ("sat...", "base...")
    unsigned satlen = strcspn(type, "+");
    std::string sattype(type, satlen);
    const char * basetype = (type[satlen] ? type+satlen+1 : "");
    // Recurse to allocate base device, default is standard SCSI
    if (!*basetype)
      basetype = "scsi";
    smart_device_auto_ptr basedev( get_smart_device(name, basetype) );
    if (!basedev) {
      set_err(EINVAL, "Type '%s+...': %s", sattype.c_str(), get_errmsg());
      return 0;
    }
    // Result must be SCSI
    if (!basedev->is_scsi()) {
      set_err(EINVAL, "Type '%s+...': Device type '%s' is not SCSI", sattype.c_str(), basetype);
      return 0;
    }
    // Attach SAT tunnel
    ata_device * satdev = get_sat_device(sattype.c_str(), basedev->to_scsi());
    if (!satdev)
      return 0;
    basedev.release();
    return satdev;
  }

  else {
    set_err(EINVAL, "Unknown device type '%s'", type);
    return 0;
  }
  if (!dev && !get_errno())
    set_err(EINVAL, "Not a device of type '%s'", type);
  return dev;
}

smart_device * smart_interface::get_custom_smart_device(const char * /*name*/, const char * /*type*/)
{
  return 0;
}

std::string smart_interface::get_valid_custom_dev_types_str()
{
  return "";
}
