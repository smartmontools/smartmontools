/*
 * dev_interface.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2008-16 Christian Franke
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

#ifndef DEV_INTERFACE_H
#define DEV_INTERFACE_H

#define DEV_INTERFACE_H_CVSID "$Id$\n"

#include "utility.h"

#include <stdexcept>
#include <string>
#include <vector>

/////////////////////////////////////////////////////////////////////////////
// Common functionality for all device types

// Forward declarations
class smart_interface;
class ata_device;
class scsi_device;
class nvme_device;

/// Base class for all devices
class smart_device
{
// Types
public:
  /// Device info strings
  struct device_info {
    device_info()
      { }
    device_info(const char * d_name, const char * d_type, const char * r_type)
      : dev_name(d_name), info_name(d_name),
        dev_type(d_type), req_type(r_type)
      { }

    std::string dev_name;  ///< Device (path)name
    std::string info_name; ///< Informal name
    std::string dev_type;  ///< Actual device type
    std::string req_type;  ///< Device type requested by user, empty if none
  };

  /// Error (number,message) pair
  struct error_info {
    explicit error_info(int n = 0)
      : no(n) { }
    error_info(int n, const char * m)
      : no(n), msg(m) { }
    void clear()
      { no = 0; msg.erase(); }

    int no;          ///< Error number
    std::string msg; ///< Error message
  };

// Construction
protected:
  /// Constructor to init interface and device info.
  /// Must be called in implementation classes.
  smart_device(smart_interface * intf, const char * dev_name,
    const char * dev_type, const char * req_type);

  /// Dummy enum for dummy constructor.
  enum do_not_use_in_implementation_classes { never_called };
  /// Dummy constructor for abstract classes.
  /// Must never be called in implementation classes.
  explicit smart_device(do_not_use_in_implementation_classes);

public:
  virtual ~smart_device() throw();

// Attributes
public:
  ///////////////////////////////////////////////
  // Dynamic downcasts to actual device flavor

  /// Return true if ATA device
  bool is_ata() const
    { return !!m_ata_ptr; }
  /// Return true if SCSI device
  bool is_scsi() const
    { return !!m_scsi_ptr; }
  /// Return true if NVMe device
  bool is_nvme() const
    { return !!m_nvme_ptr; }

  /// Downcast to ATA device.
  ata_device * to_ata()
    { return m_ata_ptr; }
  /// Downcast to ATA device (const).
  const ata_device * to_ata() const
    { return m_ata_ptr; }
  /// Downcast to SCSI device.
  scsi_device * to_scsi()
    { return m_scsi_ptr; }
  /// Downcast to SCSI device (const).
  const scsi_device * to_scsi() const
    { return m_scsi_ptr; }
  /// Downcast to NVMe device.
  nvme_device * to_nvme()
    { return m_nvme_ptr; }
  /// Downcast to NVMe device (const).
  const nvme_device * to_nvme() const
    { return m_nvme_ptr; }

  ///////////////////////////////////////////////
  // Device information

  /// Get device info struct.
  const device_info & get_info() const
    { return m_info; }

  /// Get device (path)name.
  const char * get_dev_name() const
    { return m_info.dev_name.c_str(); }
  /// Get informal name.
  const char * get_info_name() const
    { return m_info.info_name.c_str(); }
  /// Get device type.
  const char * get_dev_type() const
    { return m_info.dev_type.c_str(); }
  /// Get type requested by user, empty if none.
  const char * get_req_type() const
    { return m_info.req_type.c_str(); }

protected:
  /// R/W access to device info struct.
  device_info & set_info()
    { return m_info; }

public:
  ///////////////////////////////////////////////
  // Last error information

  /// Get last error info struct.
  const error_info & get_err() const
    { return m_err; }
  /// Get last error number.
  int get_errno() const
    { return m_err.no; }
  /// Get last error message.
  const char * get_errmsg() const
    { return m_err.msg.c_str(); }

  /// Return true if last error indicates an unsupported system call.
  /// Default implementation returns true on ENOSYS and ENOTSUP.
  virtual bool is_syscall_unsup() const;

  /// Set last error number and message.
  /// Printf()-like formatting is supported.
  /// Returns false always to allow use as a return expression.
  bool set_err(int no, const char * msg, ...)
    __attribute_format_printf(3, 4);

  /// Set last error info struct.
  bool set_err(const error_info & err)
    { m_err = err; return false; }

  /// Clear last error info.
  void clear_err()
    { m_err.clear(); }

  /// Set last error number and default message.
  /// Message is retrieved from interface's get_msg_for_errno(no).
  bool set_err(int no);

  /// Get current number of allocated 'smart_device' objects.
  static int get_num_objects()
    { return s_num_objects; }

// Operations
public:
  ///////////////////////////////////////////////
  // Device open/close
  // Must be implemented in derived class

  /// Return true if device is open.
  virtual bool is_open() const = 0;

  /// Open device, return false on error.
  virtual bool open() = 0;

  /// Close device, return false on error.
  virtual bool close() = 0;

  /// Open device with autodetection support.
  /// May return another device for further access.
  /// In this case, the original pointer is no longer valid.
  /// Default implementation calls 'open()' and returns 'this'.
  virtual smart_device * autodetect_open();

  ///////////////////////////////////////////////
  // Support for checking power mode reported by operating system

  /// Early test if device is powered up or down.
  /// Can be used without calling 'open()' first!
  /// Return true when device is powered down, false when
  /// powered up. If this function is not implemented or
  /// the mode cannot be determined, return false.
  /// Default implementation returns false.
  virtual bool is_powered_down();

  ///////////////////////////////////////////////
  // Support for tunnelled devices

  /// Return true if other device is owned by this device.
  /// Default implementation returns false.
  virtual bool owns(const smart_device * dev) const;

  /// Release ownership of other device.
  /// Default implementation does nothing.
  virtual void release(const smart_device * dev);

protected:
  /// Get interface which produced this object.
  smart_interface * smi()
    { return m_intf; }
  /// Get interface which produced this object (const).
  const smart_interface * smi() const
    { return m_intf; }

// Implementation
private:
  smart_interface * m_intf;
  device_info m_info;
  error_info m_err;

  // Pointers for to_ata(), to_scsi(), to_nvme()
  // set by ATA/SCSI/NVMe interface classes.
  friend class ata_device;
  ata_device * m_ata_ptr;
  friend class scsi_device;
  scsi_device * m_scsi_ptr;
  friend class nvme_device;
  nvme_device * m_nvme_ptr;

  // Number of objects.
  static int s_num_objects;

  // Prevent copy/assigment
  smart_device(const smart_device &);
  void operator=(const smart_device &);
};


/////////////////////////////////////////////////////////////////////////////
// ATA specific interface

/// ATA register value and info whether it has ever been set
// (Automatically set by first assignment)
class ata_register
{
public:
  ata_register()
    : m_val(0x00), m_is_set(false) { }

  ata_register & operator=(unsigned char x)
    { m_val = x; m_is_set = true; return * this; }

  unsigned char val() const
    { return m_val; }
  operator unsigned char() const
    { return m_val; }

  bool is_set() const
    { return m_is_set; }

private:
  unsigned char m_val; ///< Register value
  bool m_is_set; ///< true if set
};

/// ATA Input registers (for 28-bit commands)
struct ata_in_regs
{
  // ATA-6/7 register names  // ATA-3/4/5        // ATA-8
  ata_register features;     // features         // features
  ata_register sector_count; // sector count     // count
  ata_register lba_low;      // sector number    // ]
  ata_register lba_mid;      // cylinder low     // ] lba
  ata_register lba_high;     // cylinder high    // ]
  ata_register device;       // device/head      // device
  ata_register command;      // command          // command

  /// Return true if any register is set
  bool is_set() const
    { return (features.is_set() || sector_count.is_set()
      || lba_low.is_set() || lba_mid.is_set() || lba_high.is_set()
      || device.is_set() || command.is_set());                    }
};

/// ATA Output registers (for 28-bit commands)
struct ata_out_regs
{
  ata_register error;
  ata_register sector_count;
  ata_register lba_low;
  ata_register lba_mid;
  ata_register lba_high;
  ata_register device;
  ata_register status;

  /// Return true if any register is set
  bool is_set() const
    { return (error.is_set() || sector_count.is_set()
      || lba_low.is_set() || lba_mid.is_set() || lba_high.is_set()
      || device.is_set() || status.is_set());                      }
};


/// 16-bit alias to a 8-bit ATA register pair.
class ata_reg_alias_16
{
public:
  ata_reg_alias_16(ata_register & lo, ata_register & hi)
    : m_lo(lo), m_hi(hi) { }

  ata_reg_alias_16 & operator=(unsigned short x)
    { m_lo = (unsigned char) x;
      m_hi = (unsigned char)(x >> 8);
      return * this;                   }

  unsigned short val() const
    { return m_lo | (m_hi << 8); }
  operator unsigned short() const
    { return m_lo | (m_hi << 8); }

private:
  ata_register & m_lo, & m_hi;

  // References must not be copied.
  ata_reg_alias_16(const ata_reg_alias_16 &);
  void operator=(const ata_reg_alias_16 &);
};


/// 48-bit alias to six 8-bit ATA registers (for LBA).
class ata_reg_alias_48
{
public:
  ata_reg_alias_48(ata_register & ll, ata_register & lm, ata_register & lh,
                   ata_register & hl, ata_register & hm, ata_register & hh)
    : m_ll(ll), m_lm(lm), m_lh(lh),
      m_hl(hl), m_hm(hm), m_hh(hh)
    { }

  ata_reg_alias_48 & operator=(uint64_t x)
    {
      m_ll = (unsigned char) x;
      m_lm = (unsigned char)(x >>  8);
      m_lh = (unsigned char)(x >> 16);
      m_hl = (unsigned char)(x >> 24);
      m_hm = (unsigned char)(x >> 32);
      m_hh = (unsigned char)(x >> 40);
      return * this;
    }

  uint64_t val() const
    {
      return (   (unsigned)m_ll
              | ((unsigned)m_lm <<  8)
              | ((unsigned)m_lh << 16)
              | ((unsigned)m_hl << 24)
              | ((uint64_t)m_hm << 32)
              | ((uint64_t)m_hh << 40));
    }

  operator uint64_t() const
    { return val(); }

private:
  ata_register & m_ll, & m_lm, & m_lh,
               & m_hl, & m_hm, & m_hh;

  // References must not be copied.
  ata_reg_alias_48(const ata_reg_alias_48 &);
  void operator=(const ata_reg_alias_48 &);
};


/// ATA Input registers for 48-bit commands
// See section 4.14 of T13/1532D Volume 1 Revision 4b
//
// Uses ATA-6/7 method to specify 16-bit registers as
// recent (low byte) and previous (high byte) content of
// 8-bit registers.
//
// (ATA-8 ACS does not longer follow this scheme, it uses
// abstract registers with sufficient size and leaves the
// actual mapping to the transport layer.)
//
struct ata_in_regs_48bit
: public ata_in_regs   // "most recently written" registers
{
  ata_in_regs prev;  ///< "previous content"

  // 16-bit aliases for above pair.
  ata_reg_alias_16 features_16;
  ata_reg_alias_16 sector_count_16;
  ata_reg_alias_16 lba_low_16;
  ata_reg_alias_16 lba_mid_16;
  ata_reg_alias_16 lba_high_16;

  // 48-bit alias to all 8-bit LBA registers.
  ata_reg_alias_48 lba_48;

  /// Return true if 48-bit command
  bool is_48bit_cmd() const
    { return prev.is_set(); }

  /// Return true if 48-bit command with any nonzero high byte
  bool is_real_48bit_cmd() const
    { return (   prev.features || prev.sector_count
              || prev.lba_low || prev.lba_mid || prev.lba_high); }

  ata_in_regs_48bit();
};


/// ATA Output registers for 48-bit commands
struct ata_out_regs_48bit
: public ata_out_regs   // read with HOB=0
{
  ata_out_regs prev;  ///< read with HOB=1

  // 16-bit aliases for above pair.
  ata_reg_alias_16 sector_count_16;
  ata_reg_alias_16 lba_low_16;
  ata_reg_alias_16 lba_mid_16;
  ata_reg_alias_16 lba_high_16;

  // 48-bit alias to all 8-bit LBA registers.
  ata_reg_alias_48 lba_48;

  ata_out_regs_48bit();
};


/// Flags for each ATA output register
struct ata_out_regs_flags
{
  bool error, sector_count, lba_low, lba_mid, lba_high, device, status;

  /// Return true if any flag is set.
  bool is_set() const
    { return (   error || sector_count || lba_low
              || lba_mid || lba_high || device || status); }

  /// Default constructor clears all flags.
  ata_out_regs_flags()
    : error(false), sector_count(false), lba_low(false), lba_mid(false),
      lba_high(false), device(false), status(false) { }
};


/// ATA pass through input parameters
struct ata_cmd_in
{
  ata_in_regs_48bit in_regs;  ///< Input registers
  ata_out_regs_flags out_needed; ///< True if output register value needed
  enum { no_data = 0, data_in, data_out } direction; ///< I/O direction
  void * buffer; ///< Pointer to data buffer
  unsigned size; ///< Size of buffer

  /// Prepare for 28-bit DATA IN command
  void set_data_in(void * buf, unsigned nsectors)
    {
      buffer = buf;
      in_regs.sector_count = nsectors;
      direction = data_in;
      size = nsectors * 512;
    }

  /// Prepare for 28-bit DATA OUT command
  void set_data_out(const void * buf, unsigned nsectors)
    {
      buffer = const_cast<void *>(buf);
      in_regs.sector_count = nsectors;
      direction = data_out;
      size = nsectors * 512;
    }

  /// Prepare for 48-bit DATA IN command
  void set_data_in_48bit(void * buf, unsigned nsectors)
    {
      buffer = buf;
      // Note: This also sets 'in_regs.is_48bit_cmd()'
      in_regs.sector_count_16 = nsectors;
      direction = data_in;
      size = nsectors * 512;
    }

  ata_cmd_in();
};

/// ATA pass through output parameters
struct ata_cmd_out
{
  ata_out_regs_48bit out_regs; ///< Output registers

  ata_cmd_out();
};

/// ATA device access
class ata_device
: virtual public /*extends*/ smart_device
{
public:
  /// ATA pass through.
  /// Return false on error.
  /// Must be implemented in derived class.
  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out) = 0;

  /// ATA pass through without output registers.
  /// Return false on error.
  /// Calls ata_pass_through(in, dummy), cannot be reimplemented.
  bool ata_pass_through(const ata_cmd_in & in);

  /// Return true if OS caches ATA identify sector.
  /// Default implementation returns false.
  virtual bool ata_identify_is_cached() const;

protected:
  /// Flags for ata_cmd_is_supported().
  enum {
    supports_data_out = 0x01, // PIO DATA OUT
    supports_smart_status = 0x02, // read output registers for SMART STATUS only
    supports_output_regs = 0x04, // read output registers for all commands
    supports_multi_sector = 0x08, // more than one sector (1 DRQ/sector variant)
    supports_48bit_hi_null = 0x10, // 48-bit commands with null high bytes only
    supports_48bit = 0x20, // all 48-bit commands
  };

  /// Check command input parameters.
  /// Return false if required features are not implemented.
  /// Calls set_err(...) accordingly.
  bool ata_cmd_is_supported(const ata_cmd_in & in, unsigned flags,
    const char * type = 0);

  /// Check command input parameters (old version).
  // TODO: Remove if no longer used.
  bool ata_cmd_is_ok(const ata_cmd_in & in,
    bool data_out_support = false,
    bool multi_sector_support = false,
    bool ata_48bit_support = false)
    {
      return ata_cmd_is_supported(in,
        (data_out_support ? supports_data_out : 0) |
        supports_output_regs |
        (multi_sector_support ? supports_multi_sector : 0) |
        (ata_48bit_support ? supports_48bit : 0));
    }

  /// Hide/unhide ATA interface.
  void hide_ata(bool hide = true)
    { m_ata_ptr = (!hide ? this : 0); }

  /// Default constructor, registers device as ATA.
  ata_device()
    : smart_device(never_called)
    { hide_ata(false); }
};


/////////////////////////////////////////////////////////////////////////////
// SCSI specific interface

struct scsi_cmnd_io;

/// SCSI device access
class scsi_device
: virtual public /*extends*/ smart_device
{
public:
  /// SCSI pass through.
  /// Returns false on error.
  virtual bool scsi_pass_through(scsi_cmnd_io * iop) = 0;

protected:
  /// Hide/unhide SCSI interface.
  void hide_scsi(bool hide = true)
    { m_scsi_ptr = (!hide ? this : 0); }

  /// Default constructor, registers device as SCSI.
  scsi_device()
    : smart_device(never_called)
    { hide_scsi(false); }
};


/////////////////////////////////////////////////////////////////////////////
// NVMe specific interface

/// NVMe pass through input parameters
struct nvme_cmd_in
{
  unsigned char opcode; ///< Opcode (CDW0 07:00)
  unsigned nsid; ///< Namespace ID
  unsigned cdw10, cdw11, cdw12, cdw13, cdw14, cdw15; ///< Cmd specific

  void * buffer; ///< Pointer to data buffer
  unsigned size; ///< Size of buffer

  enum {
    no_data = 0x0, data_out = 0x1, data_in = 0x2, data_io = 0x3
  };

  /// Get I/O direction from opcode
  unsigned char direction() const
    { return (opcode & 0x3); }

  // Prepare for DATA IN command
  void set_data_in(unsigned char op, void * buf, unsigned sz)
    {
      opcode = op;
      if (direction() != data_in)
        throw std::logic_error("invalid opcode for DATA IN");
      buffer = buf;
      size = sz;
    }

  nvme_cmd_in()
    : opcode(0), nsid(0),
      cdw10(0), cdw11(0), cdw12(0), cdw13(0), cdw14(0), cdw15(0),
      buffer(0), size(0)
    { }
};

/// NVMe pass through output parameters
struct nvme_cmd_out
{
   unsigned result; ///< Command specific result (DW0)
   unsigned short status; ///< Status Field (DW3 31:17)
   bool status_valid; ///< true if status is valid

   nvme_cmd_out()
     : result(0), status(0), status_valid(false)
     { }
};

/// NVMe device access
class nvme_device
: virtual public /*extends*/ smart_device
{
public:
  /// NVMe pass through.
  /// Return false on error.
  virtual bool nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out) = 0;

  /// Get namespace id.
  unsigned get_nsid() const
    { return m_nsid; }

protected:
  /// Hide/unhide NVMe interface.
  void hide_nvme(bool hide = true)
    { m_nvme_ptr = (!hide ? this : 0); }

  /// Constructor requires namespace ID, registers device as NVMe.
  explicit nvme_device(unsigned nsid)
    : smart_device(never_called),
      m_nsid(nsid)
    { hide_nvme(false); }

  /// Set namespace id.
  /// Should be called in open() function if get_nsid() returns 0.
  void set_nsid(unsigned nsid)
    { m_nsid = nsid; }

  /// Set last error number and message if pass-through returns NVMe error status.
  /// Returns false always to allow use as a return expression.
  bool set_nvme_err(nvme_cmd_out & out, unsigned status, const char * msg = 0);

private:
  unsigned m_nsid;
};


/////////////////////////////////////////////////////////////////////////////
/// Smart pointer class for device pointers

template <class Dev>
class any_device_auto_ptr
{
public:
  typedef Dev device_type;

  /// Construct from optional pointer to device
  /// and optional pointer to base device.
  explicit any_device_auto_ptr(device_type * dev = 0,
                               smart_device * base_dev = 0)
    : m_dev(dev), m_base_dev(base_dev) { }

  /// Destructor deletes device object.
  ~any_device_auto_ptr() throw()
    { reset(); }

  /// Assign a new pointer.
  /// Throws if a pointer is already assigned.
  void operator=(device_type * dev)
    {
      if (m_dev)
        fail();
      m_dev = dev;
    }

  /// Delete device object and clear the pointer.
  void reset()
    {
      if (m_dev) {
        if (m_base_dev && m_dev->owns(m_base_dev))
          m_dev->release(m_base_dev);
        delete m_dev;
        m_dev = 0;
      }
    }

  /// Return the pointer and release ownership.
  device_type * release()
    {
      device_type * dev = m_dev;
      m_dev = 0;
      return dev;
    }

  /// Replace the pointer.
  /// Used to call dev->autodetect_open().
  void replace(device_type * dev)
    { m_dev = dev; }

  /// Return the pointer.
  device_type * get() const
    { return m_dev; }

  /// Pointer dereferencing.
  device_type & operator*() const
    { return *m_dev; }

  /// Pointer dereferencing.
  device_type * operator->() const
    { return m_dev; }

  /// For (ptr != 0) check.
  operator bool() const
    { return !!m_dev; }

  /// For (ptr == 0) check.
  bool operator !() const
    { return !m_dev; }

private:
  device_type * m_dev;
  smart_device * m_base_dev;

  void fail() const
    { throw std::logic_error("any_device_auto_ptr: wrong usage"); }

  // Prevent copy/assignment
  any_device_auto_ptr(const any_device_auto_ptr<Dev> &);
  void operator=(const any_device_auto_ptr<Dev> &);
};

typedef any_device_auto_ptr<smart_device> smart_device_auto_ptr;
typedef any_device_auto_ptr<ata_device>   ata_device_auto_ptr;
typedef any_device_auto_ptr<scsi_device>  scsi_device_auto_ptr;
typedef any_device_auto_ptr<nvme_device>  nvme_device_auto_ptr;


/////////////////////////////////////////////////////////////////////////////
// smart_device_list

/// List of devices for DEVICESCAN
class smart_device_list
{
// Construction
public:
  smart_device_list()
    { }

  ~smart_device_list() throw()
    {
      for (unsigned i = 0; i < m_list.size(); i++)
        delete m_list[i];
    }

// Attributes
  unsigned size() const
    { return m_list.size(); }

// Operations
  void clear()
    {
      for (unsigned i = 0; i < m_list.size(); i++)
        delete m_list[i];
      m_list.clear();
    }


  void push_back(smart_device * dev)
    { m_list.push_back(dev); }

  void push_back(smart_device_auto_ptr & dev)
    {
      m_list.push_back(dev.get());
      dev.release();
    }

  smart_device * at(unsigned i)
    { return m_list.at(i); }

  const smart_device * at(unsigned i) const
    { return m_list.at(i); }

  smart_device * release(unsigned i)
    {
      smart_device * dev = m_list.at(i);
      m_list[i] = 0;
      return dev;
    }

  void append(smart_device_list & devlist)
    {
      for (unsigned i = 0; i < devlist.size(); i++) {
        smart_device * dev = devlist.at(i);
        if (!dev)
          continue;
        push_back(dev);
        devlist.m_list.at(i) = 0;
      }
    }

// Implementation
private:
  std::vector<smart_device *> m_list;

  // Prevent copy/assigment
  smart_device_list(const smart_device_list &);
  void operator=(const smart_device_list &);
};


/// List of types for DEVICESCAN
typedef std::vector<std::string> smart_devtype_list;


/////////////////////////////////////////////////////////////////////////////
// smart_interface

/// The platform interface abstraction
class smart_interface
{
public:
  /// Initialize platform interface and register with smi().
  /// Must be implemented by platform module and register interface with set()
  static void init();

  smart_interface()
    { }

  virtual ~smart_interface() throw()
    { }

  /// Return info string about build host and/or OS version.
  /// Default implementation returns SMARTMONTOOLS_BUILD_HOST.
  virtual std::string get_os_version_str();

  /// Return valid args for device type option/directive.
  /// Default implementation returns "ata, scsi, sat, usb*..."
  /// concatenated with result from get_valid_custom_dev_types_str().
  virtual std::string get_valid_dev_types_str();

  /// Return example string for program 'appname'.
  /// Default implementation returns empty string.
  /// For the migration of print_smartctl_examples(),
  /// function is allowed to print examples to stdout.
  /// TODO: Remove this hack.
  virtual std::string get_app_examples(const char * appname);

  /// Get microseconds since some unspecified starting point.
  /// Used only for command duration measurements in debug outputs.
  /// Returns -1 if unsupported.
  /// Default implementation uses clock_gettime(), gettimeofday() or ftime().
  virtual int64_t get_timer_usec();

  /// Disable/Enable system auto standby/sleep mode.
  /// Return false if unsupported or if system is running
  /// on battery.
  /// Default implementation returns false.
  virtual bool disable_system_auto_standby(bool disable);


  ///////////////////////////////////////////////
  // Last error information

  /// Get last error info struct.
  const smart_device::error_info & get_err() const
    { return m_err; }
  /// Get last error number.
  int get_errno() const
    { return m_err.no; }
  /// Get last error message.
  const char * get_errmsg() const
    { return m_err.msg.c_str(); }

  /// Set last error number and message.
  /// Printf()-like formatting is supported.
  /// Returns false always to allow use as a return expression.
  bool set_err(int no, const char * msg, ...)
    __attribute_format_printf(3, 4);

  /// Set last error info struct.
  bool set_err(const smart_device::error_info & err)
    { m_err = err; return false; }

  /// Clear last error info.
  void clear_err()
    { m_err.clear(); }

  /// Set last error number and default message.
  /// Message is retrieved from get_msg_for_errno(no).
  bool set_err(int no);

  /// Set last error number and default message to any error_info.
  /// Used by set_err(no).
  bool set_err_var(smart_device::error_info * err, int no);

  /// Convert error number into message, used by set_err(no).
  /// Default implementation returns strerror(no).
  virtual const char * get_msg_for_errno(int no);

  ///////////////////////////////////////////////////////////////////////////
  // Device factory:

  /// Return device object for device 'name' with some 'type'.
  /// 'type' is 0 if not specified by user.
  /// Return 0 on error.
  /// Default implementation selects between ata, scsi and custom device.
  virtual smart_device * get_smart_device(const char * name, const char * type);

  /// Fill 'devlist' with devices of some 'type' with device names
  /// specified by some optional 'pattern'.
  /// Use platform specific default if 'type' is empty or 0.
  /// Return false on error.
  virtual bool scan_smart_devices(smart_device_list & devlist, const char * type,
    const char * pattern = 0) = 0;

  /// Fill 'devlist' with devices of all 'types' with device names
  /// specified by some optional 'pattern'.
  /// Use platform specific default if 'types' is empty.
  /// Return false on error.
  /// Default implementation calls above function for all types
  /// and concatenates the results.
  virtual bool scan_smart_devices(smart_device_list & devlist,
    const smart_devtype_list & types, const char * pattern = 0);

protected:
  /// Return standard ATA device.
  virtual ata_device * get_ata_device(const char * name, const char * type) = 0;

  /// Return standard SCSI device.
  virtual scsi_device * get_scsi_device(const char * name, const char * type) = 0;

  /// Return standard NVMe device.
  /// Default implementation returns 0.
  virtual nvme_device * get_nvme_device(const char * name, const char * type,
    unsigned nsid);

  /// Autodetect device if no device type specified.
  virtual smart_device * autodetect_smart_device(const char * name) = 0;

  /// Return device for platform specific 'type'.
  /// Default implementation returns 0.
  virtual smart_device * get_custom_smart_device(const char * name, const char * type);

  /// Return valid 'type' args accepted by above.
  /// This is called in get_valid_dev_types_str().
  /// Default implementation returns empty string.
  virtual std::string get_valid_custom_dev_types_str();

  /// Return ATA->SCSI filter for a SAT or USB 'type'.
  /// Device 'scsidev' is used for SCSI access.
  /// Return 0 and delete 'scsidev' on error.
  /// Override only if platform needs special handling.
  virtual ata_device * get_sat_device(const char * type, scsi_device * scsidev);
  //{ implemented in scsiata.cpp }

public:
  /// Try to detect a SAT device behind a SCSI interface.
  /// Inquiry data can be passed if available.
  /// Return appropriate device if yes, otherwise 0.
  /// Override only if platform needs special handling.
  virtual ata_device * autodetect_sat_device(scsi_device * scsidev,
    const unsigned char * inqdata, unsigned inqsize);
  //{ implemented in scsiata.cpp }

  /// Get type name for USB device with known VENDOR:PRODUCT ID.
  /// Return name if device known and supported, otherwise 0.
  virtual const char * get_usb_dev_type_by_id(int vendor_id, int product_id,
                                              int version = -1);
  //{ implemented in scsiata.cpp }

protected:
  /// Set interface to use, must be called from init().
  static void set(smart_interface * intf)
    { s_instance = intf; }

// Implementation
private:
  smart_device::error_info m_err;

  friend smart_interface * smi(); // below
  static smart_interface * s_instance; ///< Pointer to the interface object.

  // Prevent copy/assigment
  smart_interface(const smart_interface &);
  void operator=(const smart_interface &);
};


/////////////////////////////////////////////////////////////////////////////
// smi()

/// Global access to the (usually singleton) smart_interface
inline smart_interface * smi()
  { return smart_interface::s_instance; }

/////////////////////////////////////////////////////////////////////////////

#endif // DEV_INTERFACE_H
