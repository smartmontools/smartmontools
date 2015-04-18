/*
 * os_win32.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004-15 Christian Franke <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2012    Hank Wu <hank@areca.com.tw>
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
#define WINVER 0x0502
#define _WIN32_WINNT WINVER

#include "int64.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"
#include "smartctl.h" // TODO: Do not use smartctl only variables here

#include "dev_interface.h"
#include "dev_ata_cmd_set.h"
#include "dev_areca.h"

#include "os_win32/wmiquery.h"

#include <errno.h>

#ifdef _DEBUG
#include <assert.h>
#else
#undef assert
#define assert(x) /* */
#endif

#include <stddef.h> // offsetof()
#include <io.h> // access()

// WIN32_LEAN_AND_MEAN may be required to prevent inclusion of <winioctl.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#if HAVE_NTDDDISK_H
// i686-pc-cygwin, i686-w64-mingw32, x86_64-w64-mingw32
// (Missing: FILE_DEVICE_SCSI)
#include <devioctl.h>
#include <ntdddisk.h>
#include <ntddscsi.h>
#include <ntddstor.h>
#elif HAVE_DDK_NTDDDISK_H
// older i686-pc-cygwin, i686-pc-mingw32, i586-mingw32msvc
// (Missing: IOCTL_IDE_PASS_THROUGH, IOCTL_ATA_PASS_THROUGH, FILE_DEVICE_SCSI)
#include <ddk/ntdddisk.h>
#include <ddk/ntddscsi.h>
#include <ddk/ntddstor.h>
#else
// MSVC10, older MinGW
// (Missing: IOCTL_SCSI_MINIPORT_*)
#include <ntddscsi.h>
#include <winioctl.h>
#endif

#ifndef _WIN32
// csmisas.h requires _WIN32 but w32api-headers no longer define it on Cygwin
#define _WIN32
#endif

// CSMI support
#include "csmisas.h"

// Silence -Wunused-local-typedefs warning from g++ >= 4.8
#if __GNUC__ >= 4
#define ATTR_UNUSED __attribute__((unused))
#else
#define ATTR_UNUSED /**/
#endif

// Macro to check constants at compile time using a dummy typedef
#define ASSERT_CONST(c, n) \
  typedef char assert_const_##c[((c) == (n)) ? 1 : -1] ATTR_UNUSED
#define ASSERT_SIZEOF(t, n) \
  typedef char assert_sizeof_##t[(sizeof(t) == (n)) ? 1 : -1] ATTR_UNUSED

#ifndef _WIN64
#define SELECT_WIN_32_64(x32, x64) (x32)
#else
#define SELECT_WIN_32_64(x32, x64) (x64)
#endif

// Cygwin does no longer provide strnicmp() compatibility macro
// MSVCRT does not provide strncasecmp()
#if defined(__CYGWIN__) && !defined(strnicmp)
#define strnicmp strncasecmp
#endif

const char * os_win32_cpp_cvsid = "$Id$";

/////////////////////////////////////////////////////////////////////////////
// Windows I/O-controls, some declarations are missing in the include files

extern "C" {

// SMART_* IOCTLs, also known as DFP_* (Disk Fault Protection)

ASSERT_CONST(SMART_GET_VERSION, 0x074080);
ASSERT_CONST(SMART_SEND_DRIVE_COMMAND, 0x07c084);
ASSERT_CONST(SMART_RCV_DRIVE_DATA, 0x07c088);
ASSERT_SIZEOF(GETVERSIONINPARAMS, 24);
ASSERT_SIZEOF(SENDCMDINPARAMS, 32+1);
ASSERT_SIZEOF(SENDCMDOUTPARAMS, 16+1);


// IDE PASS THROUGH (2000, XP, undocumented)

#ifndef IOCTL_IDE_PASS_THROUGH

#define IOCTL_IDE_PASS_THROUGH \
  CTL_CODE(IOCTL_SCSI_BASE, 0x040A, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#endif // IOCTL_IDE_PASS_THROUGH

#pragma pack(1)

typedef struct {
  IDEREGS IdeReg;
  ULONG DataBufferSize;
  UCHAR DataBuffer[1];
} ATA_PASS_THROUGH;

#pragma pack()

ASSERT_CONST(IOCTL_IDE_PASS_THROUGH, 0x04d028);
ASSERT_SIZEOF(ATA_PASS_THROUGH, 12+1);


// ATA PASS THROUGH (Win2003, XP SP2)

#ifndef IOCTL_ATA_PASS_THROUGH

#define IOCTL_ATA_PASS_THROUGH \
  CTL_CODE(IOCTL_SCSI_BASE, 0x040B, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

typedef struct _ATA_PASS_THROUGH_EX {
  USHORT Length;
  USHORT AtaFlags;
  UCHAR PathId;
  UCHAR TargetId;
  UCHAR Lun;
  UCHAR ReservedAsUchar;
  ULONG DataTransferLength;
  ULONG TimeOutValue;
  ULONG ReservedAsUlong;
  ULONG_PTR DataBufferOffset;
  UCHAR PreviousTaskFile[8];
  UCHAR CurrentTaskFile[8];
} ATA_PASS_THROUGH_EX;

#define ATA_FLAGS_DRDY_REQUIRED 0x01
#define ATA_FLAGS_DATA_IN       0x02
#define ATA_FLAGS_DATA_OUT      0x04
#define ATA_FLAGS_48BIT_COMMAND 0x08
#define ATA_FLAGS_USE_DMA       0x10
#define ATA_FLAGS_NO_MULTIPLE   0x20 // Vista

#endif // IOCTL_ATA_PASS_THROUGH

ASSERT_CONST(IOCTL_ATA_PASS_THROUGH, 0x04d02c);
ASSERT_SIZEOF(ATA_PASS_THROUGH_EX, SELECT_WIN_32_64(40, 48));


// IOCTL_SCSI_PASS_THROUGH[_DIRECT]

ASSERT_CONST(IOCTL_SCSI_PASS_THROUGH, 0x04d004);
ASSERT_CONST(IOCTL_SCSI_PASS_THROUGH_DIRECT, 0x04d014);
ASSERT_SIZEOF(SCSI_PASS_THROUGH, SELECT_WIN_32_64(44, 56));
ASSERT_SIZEOF(SCSI_PASS_THROUGH_DIRECT, SELECT_WIN_32_64(44, 56));


// SMART IOCTL via SCSI MINIPORT ioctl

#ifndef FILE_DEVICE_SCSI
#define FILE_DEVICE_SCSI 0x001b
#endif

#ifndef IOCTL_SCSI_MINIPORT_SMART_VERSION

#define IOCTL_SCSI_MINIPORT_SMART_VERSION               ((FILE_DEVICE_SCSI << 16) + 0x0500)
#define IOCTL_SCSI_MINIPORT_IDENTIFY                    ((FILE_DEVICE_SCSI << 16) + 0x0501)
#define IOCTL_SCSI_MINIPORT_READ_SMART_ATTRIBS          ((FILE_DEVICE_SCSI << 16) + 0x0502)
#define IOCTL_SCSI_MINIPORT_READ_SMART_THRESHOLDS       ((FILE_DEVICE_SCSI << 16) + 0x0503)
#define IOCTL_SCSI_MINIPORT_ENABLE_SMART                ((FILE_DEVICE_SCSI << 16) + 0x0504)
#define IOCTL_SCSI_MINIPORT_DISABLE_SMART               ((FILE_DEVICE_SCSI << 16) + 0x0505)
#define IOCTL_SCSI_MINIPORT_RETURN_STATUS               ((FILE_DEVICE_SCSI << 16) + 0x0506)
#define IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTOSAVE     ((FILE_DEVICE_SCSI << 16) + 0x0507)
#define IOCTL_SCSI_MINIPORT_SAVE_ATTRIBUTE_VALUES       ((FILE_DEVICE_SCSI << 16) + 0x0508)
#define IOCTL_SCSI_MINIPORT_EXECUTE_OFFLINE_DIAGS       ((FILE_DEVICE_SCSI << 16) + 0x0509)
#define IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTO_OFFLINE ((FILE_DEVICE_SCSI << 16) + 0x050a)
#define IOCTL_SCSI_MINIPORT_READ_SMART_LOG              ((FILE_DEVICE_SCSI << 16) + 0x050b)
#define IOCTL_SCSI_MINIPORT_WRITE_SMART_LOG             ((FILE_DEVICE_SCSI << 16) + 0x050c)

#endif // IOCTL_SCSI_MINIPORT_SMART_VERSION

ASSERT_CONST(IOCTL_SCSI_MINIPORT, 0x04d008);
ASSERT_SIZEOF(SRB_IO_CONTROL, 28);


// IOCTL_STORAGE_QUERY_PROPERTY

#ifndef IOCTL_STORAGE_QUERY_PROPERTY

#define IOCTL_STORAGE_QUERY_PROPERTY \
  CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _STORAGE_DEVICE_DESCRIPTOR {
  ULONG Version;
  ULONG Size;
  UCHAR DeviceType;
  UCHAR DeviceTypeModifier;
  BOOLEAN RemovableMedia;
  BOOLEAN CommandQueueing;
  ULONG VendorIdOffset;
  ULONG ProductIdOffset;
  ULONG ProductRevisionOffset;
  ULONG SerialNumberOffset;
  STORAGE_BUS_TYPE BusType;
  ULONG RawPropertiesLength;
  UCHAR RawDeviceProperties[1];
} STORAGE_DEVICE_DESCRIPTOR;

typedef enum _STORAGE_QUERY_TYPE {
  PropertyStandardQuery = 0,
  PropertyExistsQuery,
  PropertyMaskQuery,
  PropertyQueryMaxDefined
} STORAGE_QUERY_TYPE;

typedef enum _STORAGE_PROPERTY_ID {
  StorageDeviceProperty = 0,
  StorageAdapterProperty,
  StorageDeviceIdProperty,
  StorageDeviceUniqueIdProperty,
  StorageDeviceWriteCacheProperty,
  StorageMiniportProperty,
  StorageAccessAlignmentProperty
} STORAGE_PROPERTY_ID;

typedef struct _STORAGE_PROPERTY_QUERY {
  STORAGE_PROPERTY_ID PropertyId;
  STORAGE_QUERY_TYPE QueryType;
  UCHAR AdditionalParameters[1];
} STORAGE_PROPERTY_QUERY;

#endif // IOCTL_STORAGE_QUERY_PROPERTY

ASSERT_CONST(IOCTL_STORAGE_QUERY_PROPERTY, 0x002d1400);
ASSERT_SIZEOF(STORAGE_DEVICE_DESCRIPTOR, 36+1+3);
ASSERT_SIZEOF(STORAGE_PROPERTY_QUERY, 8+1+3);


// IOCTL_STORAGE_PREDICT_FAILURE

ASSERT_CONST(IOCTL_STORAGE_PREDICT_FAILURE, 0x002d1100);
ASSERT_SIZEOF(STORAGE_PREDICT_FAILURE, 4+512);


// 3ware specific versions of SMART ioctl structs

#define SMART_VENDOR_3WARE      0x13C1  // identifies 3ware specific parameters

#pragma pack(1)

typedef struct _GETVERSIONINPARAMS_EX {
  BYTE bVersion;
  BYTE bRevision;
  BYTE bReserved;
  BYTE bIDEDeviceMap;
  DWORD fCapabilities;
  DWORD dwDeviceMapEx;  // 3ware specific: RAID drive bit map
  WORD wIdentifier;     // Vendor specific identifier
  WORD wControllerId;   // 3ware specific: Controller ID (0,1,...)
  ULONG dwReserved[2];
} GETVERSIONINPARAMS_EX;

typedef struct _SENDCMDINPARAMS_EX {
  DWORD cBufferSize;
  IDEREGS irDriveRegs;
  BYTE bDriveNumber;
  BYTE bPortNumber;     // 3ware specific: port number
  WORD wIdentifier;     // Vendor specific identifier
  DWORD dwReserved[4];
  BYTE bBuffer[1];
} SENDCMDINPARAMS_EX;

#pragma pack()

ASSERT_SIZEOF(GETVERSIONINPARAMS_EX, sizeof(GETVERSIONINPARAMS));
ASSERT_SIZEOF(SENDCMDINPARAMS_EX, sizeof(SENDCMDINPARAMS));


// CSMI structs

ASSERT_SIZEOF(IOCTL_HEADER, sizeof(SRB_IO_CONTROL));
ASSERT_SIZEOF(CSMI_SAS_DRIVER_INFO_BUFFER, 204);
ASSERT_SIZEOF(CSMI_SAS_PHY_INFO_BUFFER, 2080);
ASSERT_SIZEOF(CSMI_SAS_STP_PASSTHRU_BUFFER, 168);

} // extern "C"

/////////////////////////////////////////////////////////////////////////////

namespace os_win32 { // no need to publish anything, name provided for Doxygen

#ifdef _MSC_VER
#pragma warning(disable:4250)
#endif

class win_smart_device
: virtual public /*implements*/ smart_device
{
public:
  win_smart_device()
    : smart_device(never_called),
      m_fh(INVALID_HANDLE_VALUE)
    { }

  virtual ~win_smart_device() throw();

  virtual bool is_open() const;

  virtual bool close();

protected:
  /// Set handle for open() in derived classes.
  void set_fh(HANDLE fh)
    { m_fh = fh; }

  /// Return handle for derived classes.
  HANDLE get_fh() const
    { return m_fh; }

private:
  HANDLE m_fh; ///< File handle
};


/////////////////////////////////////////////////////////////////////////////

class win_ata_device
: public /*implements*/ ata_device,
  public /*extends*/ win_smart_device
{
public:
  win_ata_device(smart_interface * intf, const char * dev_name, const char * req_type);

  virtual ~win_ata_device() throw();

  virtual bool open();

  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

  virtual bool ata_identify_is_cached() const;

private:
  bool open(int phydrive, int logdrive, const char * options, int port);

  std::string m_options;
  bool m_usr_options; // options set by user?
  bool m_admin; // open with admin access?
  int m_phydrive; // PhysicalDriveN or -1
  bool m_id_is_cached; // ata_identify_is_cached() return value.
  bool m_is_3ware; // LSI/3ware controller detected?
  int m_port; // LSI/3ware port
  int m_smartver_state;
};


/////////////////////////////////////////////////////////////////////////////

class win_scsi_device
: public /*implements*/ scsi_device,
  virtual public /*extends*/ win_smart_device
{
public:
  win_scsi_device(smart_interface * intf, const char * dev_name, const char * req_type);

  virtual bool open();

  virtual bool scsi_pass_through(scsi_cmnd_io * iop);

private:
  bool open(int pd_num, int ld_num, int tape_num, int sub_addr);
};


/////////////////////////////////////////////////////////////////////////////

class csmi_device
: virtual public /*extends*/ smart_device
{
public:
  /// Get bitmask of used ports
  unsigned get_ports_used();

protected:
  csmi_device()
    : smart_device(never_called)
    { memset(&m_phy_ent, 0, sizeof(m_phy_ent)); }

  /// Get phy info
  bool get_phy_info(CSMI_SAS_PHY_INFO & phy_info);

  /// Select physical drive
  bool select_port(int port);

  /// Get info for selected physical drive
  const CSMI_SAS_PHY_ENTITY & get_phy_ent() const
    { return m_phy_ent; }

  /// Call platform-specific CSMI ioctl
  virtual bool csmi_ioctl(unsigned code, IOCTL_HEADER * csmi_buffer,
    unsigned csmi_bufsiz) = 0;

private:
  CSMI_SAS_PHY_ENTITY m_phy_ent; ///< CSMI info for this phy
};


class csmi_ata_device
: virtual public /*extends*/ csmi_device,
  virtual public /*implements*/ ata_device
{
public:
  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

protected:
  csmi_ata_device()
    : smart_device(never_called) { }
};


//////////////////////////////////////////////////////////////////////

class win_csmi_device
: public /*implements*/ csmi_ata_device
{
public:
  win_csmi_device(smart_interface * intf, const char * dev_name,
    const char * req_type);

  virtual ~win_csmi_device() throw();

  virtual bool open();

  virtual bool close();

  virtual bool is_open() const;

  bool open_scsi();

protected:
  virtual bool csmi_ioctl(unsigned code, IOCTL_HEADER * csmi_buffer,
    unsigned csmi_bufsiz);

private:
  HANDLE m_fh; ///< Controller device handle
  int m_port; ///< Port number
};


//////////////////////////////////////////////////////////////////////

class win_tw_cli_device
: public /*implements*/ ata_device_with_command_set
{
public:
  win_tw_cli_device(smart_interface * intf, const char * dev_name, const char * req_type);

  virtual bool is_open() const;

  virtual bool open();

  virtual bool close();

protected:
  virtual int ata_command_interface(smart_command_set command, int select, char * data);

private:
  bool m_ident_valid, m_smart_valid;
  ata_identify_device m_ident_buf;
  ata_smart_values m_smart_buf;
};


/////////////////////////////////////////////////////////////////////////////
/// Areca RAID support

///////////////////////////////////////////////////////////////////
// SATA(ATA) device behind Areca RAID Controller
class win_areca_ata_device
: public /*implements*/ areca_ata_device,
  public /*extends*/ win_smart_device
{
public:
  win_areca_ata_device(smart_interface * intf, const char * dev_name, int disknum, int encnum = 1);
  virtual bool open();
  virtual smart_device * autodetect_open();
  virtual bool arcmsr_lock();
  virtual bool arcmsr_unlock();
  virtual int arcmsr_do_scsi_io(struct scsi_cmnd_io * iop);

private:
  HANDLE m_mutex;
};

///////////////////////////////////////////////////////////////////
// SAS(SCSI) device behind Areca RAID Controller
class win_areca_scsi_device
: public /*implements*/ areca_scsi_device,
  public /*extends*/ win_smart_device
{
public:
  win_areca_scsi_device(smart_interface * intf, const char * dev_name, int disknum, int encnum = 1);
  virtual bool open();
  virtual smart_device * autodetect_open();
  virtual bool arcmsr_lock();
  virtual bool arcmsr_unlock();
  virtual int arcmsr_do_scsi_io(struct scsi_cmnd_io * iop);

private:
  HANDLE m_mutex;
};


//////////////////////////////////////////////////////////////////////
// Platform specific interface

class win_smart_interface
: public /*implements part of*/ smart_interface
{
public:
  virtual std::string get_os_version_str();

  virtual std::string get_app_examples(const char * appname);

#ifndef __CYGWIN__
  virtual int64_t get_timer_usec();
#endif

  virtual bool disable_system_auto_standby(bool disable);

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

#ifndef _WIN64
// Running on 64-bit Windows as 32-bit app ?
static bool is_wow64()
{
  BOOL (WINAPI * IsWow64Process_p)(HANDLE, PBOOL) =
    (BOOL (WINAPI *)(HANDLE, PBOOL))
    GetProcAddress(GetModuleHandleA("kernel32.dll"), "IsWow64Process");
  if (!IsWow64Process_p)
    return false;
  BOOL w64 = FALSE;
  if (!IsWow64Process_p(GetCurrentProcess(), &w64))
    return false;
  return !!w64;
}
#endif // _WIN64

// Return info string about build host and OS version
std::string win_smart_interface::get_os_version_str()
{
  char vstr[sizeof(SMARTMONTOOLS_BUILD_HOST)-1+sizeof("-2003r2(64)-sp2.1")+13]
    = SMARTMONTOOLS_BUILD_HOST;
  if (vstr[1] < '6')
    vstr[1] = '6';
  char * const vptr = vstr+sizeof(SMARTMONTOOLS_BUILD_HOST)-1;
  const int vlen = sizeof(vstr)-sizeof(SMARTMONTOOLS_BUILD_HOST);
  assert(vptr == vstr+strlen(vstr) && vptr+vlen+1 == vstr+sizeof(vstr));

  OSVERSIONINFOEXA vi; memset(&vi, 0, sizeof(vi));
  vi.dwOSVersionInfoSize = sizeof(vi);
  if (!GetVersionExA((OSVERSIONINFOA *)&vi)) {
    memset(&vi, 0, sizeof(vi));
    vi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
    if (!GetVersionExA((OSVERSIONINFOA *)&vi))
      return vstr;
  }

  const char * w = 0;
  if (vi.dwPlatformId == VER_PLATFORM_WIN32_NT) {

    if (vi.dwMajorVersion > 6 || (vi.dwMajorVersion == 6 && vi.dwMinorVersion >= 2)) {
      // Starting with Windows 8.1 Preview, GetVersionEx() does no longer report the
      // actual OS version, see:
      // http://msdn.microsoft.com/en-us/library/windows/desktop/dn302074.aspx

      ULONGLONG major_equal = VerSetConditionMask(0, VER_MAJORVERSION, VER_EQUAL);
      for (unsigned major = vi.dwMajorVersion; major <= 9; major++) {
        OSVERSIONINFOEXA vi2; memset(&vi2, 0, sizeof(vi2));
        vi2.dwOSVersionInfoSize = sizeof(vi2); vi2.dwMajorVersion = major;
        if (!VerifyVersionInfo(&vi2, VER_MAJORVERSION, major_equal))
          continue;
        if (vi.dwMajorVersion < major) {
          vi.dwMajorVersion = major; vi.dwMinorVersion = 0;
        }

        ULONGLONG minor_equal = VerSetConditionMask(0, VER_MINORVERSION, VER_EQUAL);
        for (unsigned minor = vi.dwMinorVersion; minor <= 9; minor++) {
          memset(&vi2, 0, sizeof(vi2)); vi2.dwOSVersionInfoSize = sizeof(vi2);
          vi2.dwMinorVersion = minor;
          if (!VerifyVersionInfo(&vi2, VER_MINORVERSION, minor_equal))
            continue;
          vi.dwMinorVersion = minor;
          break;
        }

        break;
      }
    }

    if (vi.dwMajorVersion <= 0xf && vi.dwMinorVersion <= 0xf) {
      bool ws = (vi.wProductType <= VER_NT_WORKSTATION);
      switch (vi.dwMajorVersion << 4 | vi.dwMinorVersion) {
        case 0x50: w =       "2000";              break;
        case 0x51: w =       "xp";                break;
        case 0x52: w = (!GetSystemMetrics(89/*SM_SERVERR2*/)
                           ? "2003"  : "2003r2"); break;
        case 0x60: w = (ws ? "vista" : "2008"  ); break;
        case 0x61: w = (ws ? "win7"  : "2008r2"); break;
        case 0x62: w = (ws ? "win8"  : "2012"  ); break;
        case 0x63: w = (ws ? "win8.1": "2012r2"); break;
        case 0x64: w = (ws ? "win10" : "w10srv"); break;
      }
    }
  }

  const char * w64 = "";
#ifndef _WIN64
  if (is_wow64())
    w64 = "(64)";
#endif

  if (!w)
    snprintf(vptr, vlen, "-%s%u.%u%s",
      (vi.dwPlatformId==VER_PLATFORM_WIN32_NT ? "nt" : "??"),
      (unsigned)vi.dwMajorVersion, (unsigned)vi.dwMinorVersion, w64);
  else if (vi.wServicePackMinor)
    snprintf(vptr, vlen, "-%s%s-sp%u.%u", w, w64, vi.wServicePackMajor, vi.wServicePackMinor);
  else if (vi.wServicePackMajor)
    snprintf(vptr, vlen, "-%s%s-sp%u", w, w64, vi.wServicePackMajor);
  else
    snprintf(vptr, vlen, "-%s%s", w, w64);
  return vstr;
}

#ifndef __CYGWIN__
// MSVCRT only provides ftime() which uses GetSystemTime()
// This provides only ~15ms resolution by default.
// Use QueryPerformanceCounter instead (~300ns).
// (Cygwin provides CLOCK_MONOTONIC which has the same effect)
int64_t win_smart_interface::get_timer_usec()
{
  static int64_t freq = 0;

  LARGE_INTEGER t;
  if (freq == 0)
    freq = (QueryPerformanceFrequency(&t) ? t.QuadPart : -1);
  if (freq <= 0)
    return smart_interface::get_timer_usec();

  if (!QueryPerformanceCounter(&t))
    return -1;
  if (!(0 <= t.QuadPart && t.QuadPart <= (int64_t)(~(uint64_t)0 >> 1)/1000000))
    return -1;

  return (t.QuadPart * 1000000LL) / freq;
}
#endif // __CYGWIN__


// Return value for device detection functions
enum win_dev_type { DEV_UNKNOWN = 0, DEV_ATA, DEV_SCSI, DEV_SAT, DEV_USB };

static win_dev_type get_phy_drive_type(int drive);
static win_dev_type get_phy_drive_type(int drive, GETVERSIONINPARAMS_EX * ata_version_ex);
static win_dev_type get_log_drive_type(int drive);
static bool get_usb_id(int drive, unsigned short & vendor_id,
                       unsigned short & product_id);

static const char * ata_get_def_options(void);


static int is_permissive()
{
  if (!failuretest_permissive) {
    pout("To continue, add one or more '-T permissive' options.\n");
    return 0;
  }
  failuretest_permissive--;
  return 1;
}

// return number for drive letter, -1 on error
// "[A-Za-z]:([/\\][.]?)?" => 0-25
// Accepts trailing '"' to fix broken "X:\" parameter passing from .bat files
static int drive_letter(const char * s)
{
  return (   (('A' <= s[0] && s[0] <= 'Z') || ('a' <= s[0] && s[0] <= 'z'))
          && s[1] == ':'
          && (!s[2] || (   strchr("/\\\"", s[2])
                        && (!s[3] || (s[3] == '.' && !s[4])))              ) ?
          (s[0] & 0x1f) - 1 : -1);
}

// Skip trailing "/dev/", do not allow "/dev/X:"
static const char * skipdev(const char * s)
{
  return (!strncmp(s, "/dev/", 5) && drive_letter(s+5) < 0 ? s+5 : s);
}

ata_device * win_smart_interface::get_ata_device(const char * name, const char * type)
{
  const char * testname = skipdev(name);
  if (!strncmp(testname, "csmi", 4))
    return new win_csmi_device(this, name, type);
  if (!strncmp(testname, "tw_cli", 6))
    return new win_tw_cli_device(this, name, type);
  return new win_ata_device(this, name, type);
}

scsi_device * win_smart_interface::get_scsi_device(const char * name, const char * type)
{
  return new win_scsi_device(this, name, type);
}

static int sdxy_to_phydrive(const char (& xy)[2+1])
{
  int phydrive = xy[0] - 'a';
  if (xy[1])
    phydrive = (phydrive + 1) * ('z' - 'a' + 1) + (xy[1] - 'a');
  return phydrive;
}

static win_dev_type get_dev_type(const char * name, int & phydrive)
{
  phydrive = -1;
  name = skipdev(name);
  if (!strncmp(name, "st", 2))
    return DEV_SCSI;
  if (!strncmp(name, "nst", 3))
    return DEV_SCSI;
  if (!strncmp(name, "tape", 4))
    return DEV_SCSI;

  int logdrive = drive_letter(name);
  if (logdrive >= 0) {
    win_dev_type type = get_log_drive_type(logdrive);
    return (type != DEV_UNKNOWN ? type : DEV_SCSI);
  }

  char drive[2+1] = "";
  if (sscanf(name, "sd%2[a-z]", drive) == 1) {
    phydrive = sdxy_to_phydrive(drive);
    return get_phy_drive_type(phydrive);
  }

  phydrive = -1;
  if (sscanf(name, "pd%d", &phydrive) == 1 && phydrive >= 0)
    return get_phy_drive_type(phydrive);
  return DEV_UNKNOWN;
}

smart_device * win_smart_interface::get_custom_smart_device(const char * name, const char * type)
{
  // Areca?
  int disknum = -1, n1 = -1, n2 = -1;
  int encnum = 1;
  char devpath[32];

  if (sscanf(type, "areca,%n%d/%d%n", &n1, &disknum, &encnum, &n2) >= 1 || n1 == 6) {
    if (!(1 <= disknum && disknum <= 128)) {
      set_err(EINVAL, "Option -d areca,N/E (N=%d) must have 1 <= N <= 128", disknum);
      return 0;
    }
    if (!(1 <= encnum && encnum <= 8)) {
      set_err(EINVAL, "Option -d areca,N/E (E=%d) must have 1 <= E <= 8", encnum);
      return 0;
    }

    name = skipdev(name);
#define ARECA_MAX_CTLR_NUM  16
    n1 = -1;
    int ctlrindex = 0;
    if (sscanf(name, "arcmsr%d%n", &ctlrindex, &n1) >= 1 && n1 == (int)strlen(name)) {
      /*
       1. scan from "\\\\.\\scsi[0]:" up to "\\\\.\\scsi[ARECA_MAX_CTLR_NUM]:" and
       2. map arcmsrX into "\\\\.\\scsiX"
      */
     for (int idx = 0; idx < ARECA_MAX_CTLR_NUM; idx++) {
        memset(devpath, 0, sizeof(devpath));
        snprintf(devpath, sizeof(devpath), "\\\\.\\scsi%d:", idx);
        win_areca_ata_device *arcdev = new win_areca_ata_device(this, devpath, disknum, encnum);
        if(arcdev->arcmsr_probe()) {
          if(ctlrindex-- == 0) {
            return arcdev;
          }
        }
        delete arcdev;
      }
      set_err(ENOENT, "No Areca controller found");
    }
    else
      set_err(EINVAL, "Option -d areca,N/E requires device name /dev/arcmsrX");
  }

  return 0;
}

std::string win_smart_interface::get_valid_custom_dev_types_str()
{
  return "areca,N[/E]";
}


smart_device * win_smart_interface::autodetect_smart_device(const char * name)
{
  const char * testname = skipdev(name);
  if (str_starts_with(testname, "hd"))
    return new win_ata_device(this, name, "");

  if (str_starts_with(testname, "tw_cli"))
    return new win_tw_cli_device(this, name, "");

  if (str_starts_with(testname, "csmi"))
    return new win_csmi_device(this, name, "");

  int phydrive = -1;
  win_dev_type type = get_dev_type(name, phydrive);

  if (type == DEV_ATA)
    return new win_ata_device(this, name, "");

  if (type == DEV_SCSI)
    return new win_scsi_device(this, name, "");

  if (type == DEV_SAT)
    return get_sat_device("sat", new win_scsi_device(this, name, ""));

  if (type == DEV_USB) {
    // Get USB bridge ID
    unsigned short vendor_id = 0, product_id = 0;
    if (!(phydrive >= 0 && get_usb_id(phydrive, vendor_id, product_id))) {
      set_err(EINVAL, "Unable to read USB device ID");
      return 0;
    }
    // Get type name for this ID
    const char * usbtype = get_usb_dev_type_by_id(vendor_id, product_id);
    if (!usbtype)
      return 0;
    // Return SAT/USB device for this type
    return get_sat_device(usbtype, new win_scsi_device(this, name, ""));
  }

  return 0;
}


// Scan for devices

bool win_smart_interface::scan_smart_devices(smart_device_list & devlist,
  const char * type, const char * pattern /* = 0*/)
{
  if (pattern) {
    set_err(EINVAL, "DEVICESCAN with pattern not implemented yet");
    return false;
  }

  // Check for "[*,]pd" type
  bool pd = false;
  char type2[16+1] = "";
  if (type) {
    int nc = -1;
    if (!strcmp(type, "pd")) {
      pd = true;
      type = 0;
    }
    else if (sscanf(type, "%16[^,],pd%n", type2, &nc) == 1 &&
             nc == (int)strlen(type)) {
      pd = true;
      type = type2;
    }
  }

  // Set valid types
  bool ata, scsi, sat, usb, csmi;
  if (!type) {
    ata = scsi = usb = sat = csmi = true;
  }
  else {
    ata = scsi = usb = sat = csmi = false;
    if (!strcmp(type, "ata"))
      ata = true;
    else if (!strcmp(type, "scsi"))
      scsi = true;
    else if (!strcmp(type, "sat"))
      sat = true;
    else if (!strcmp(type, "usb"))
      usb = true;
    else if (!strcmp(type, "csmi"))
      csmi = true;
    else {
      set_err(EINVAL,
              "Invalid type '%s', valid arguments are: ata[,pd], scsi[,pd], sat[,pd], usb[,pd], csmi, pd",
              type);
      return false;
    }
  }

  char name[20];

  if (ata || scsi || sat || usb) {
    // Scan up to 128 drives and 2 3ware controllers
    const int max_raid = 2;
    bool raid_seen[max_raid] = {false, false};

    for (int i = 0; i < 128; i++) {
      if (pd)
        snprintf(name, sizeof(name), "/dev/pd%d", i);
      else if (i + 'a' <= 'z')
        snprintf(name, sizeof(name), "/dev/sd%c", i + 'a');
      else
        snprintf(name, sizeof(name), "/dev/sd%c%c",
                 i / ('z'-'a'+1) - 1 + 'a',
                 i % ('z'-'a'+1)     + 'a');

      GETVERSIONINPARAMS_EX vers_ex;

      switch (get_phy_drive_type(i, (ata ? &vers_ex : 0))) {
        case DEV_ATA:
          // Driver supports SMART_GET_VERSION or STORAGE_QUERY_PROPERTY returned ATA/SATA
          if (!ata)
            continue;

          // Interpret RAID drive map if present
          if (vers_ex.wIdentifier == SMART_VENDOR_3WARE) {
            // Skip if too many controllers or logical drive from this controller already seen
            if (!(vers_ex.wControllerId < max_raid && !raid_seen[vers_ex.wControllerId]))
              continue;
            raid_seen[vers_ex.wControllerId] = true;
            // Add physical drives
            int len = strlen(name);
            for (int pi = 0; pi < 32; pi++) {
              if (vers_ex.dwDeviceMapEx & (1L << pi)) {
                snprintf(name+len, sizeof(name)-1-len, ",%u", pi);
                devlist.push_back( new win_ata_device(this, name, "ata") );
              }
            }
          }
          else {
            devlist.push_back( new win_ata_device(this, name, "ata") );
          }
          break;

        case DEV_SCSI:
          // STORAGE_QUERY_PROPERTY returned SCSI/SAS/...
          if (!scsi)
            continue;
          devlist.push_back( new win_scsi_device(this, name, "scsi") );
          break;

        case DEV_SAT:
          // STORAGE_QUERY_PROPERTY returned VendorId "ATA     "
          if (!sat)
            continue;
          devlist.push_back( get_sat_device("sat", new win_scsi_device(this, name, "")) );
          break;

        case DEV_USB:
          // STORAGE_QUERY_PROPERTY returned USB
          if (!usb)
            continue;
          {
            // TODO: Use common function for this and autodetect_smart_device()
            // Get USB bridge ID
            unsigned short vendor_id = 0, product_id = 0;
            if (!get_usb_id(i, vendor_id, product_id))
              continue;
            // Get type name for this ID
            const char * usbtype = get_usb_dev_type_by_id(vendor_id, product_id);
            if (!usbtype)
              continue;
            // Return SAT/USB device for this type
            ata_device * dev = get_sat_device(usbtype, new win_scsi_device(this, name, ""));
            if (!dev)
              continue;
            devlist.push_back(dev);
          }
          break;

        default:
          // Unknown type
          break;
      }
    }
  }

  if (csmi) {
    // Scan CSMI devices
    for (int i = 0; i <= 9; i++) {
      snprintf(name, sizeof(name)-1, "/dev/csmi%d,0", i);
      win_csmi_device test_dev(this, name, "");
      if (!test_dev.open_scsi())
        continue;

      unsigned ports_used = test_dev.get_ports_used();
      if (!ports_used)
        continue;

      for (int pi = 0; pi < 32; pi++) {
        if (!(ports_used & (1 << pi)))
          continue;
        snprintf(name, sizeof(name)-1, "/dev/csmi%d,%d", i, pi);
        devlist.push_back( new win_csmi_device(this, name, "ata") );
      }
    }
  }
  return true;
}


// get examples for smartctl
std::string win_smart_interface::get_app_examples(const char * appname)
{
  if (strcmp(appname, "smartctl"))
    return "";
  return "=================================================== SMARTCTL EXAMPLES =====\n\n"
         "  smartctl -a /dev/sda                       (Prints all SMART information)\n\n"
         "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/sda\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long /dev/sda              (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/sda\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         "  smartctl -a /dev/sda\n"
         "             (Prints all information for disk on PhysicalDrive 0)\n"
         "  smartctl -a /dev/pd3\n"
         "             (Prints all information for disk on PhysicalDrive 3)\n"
         "  smartctl -a /dev/tape1\n"
         "             (Prints all information for SCSI tape on Tape 1)\n"
         "  smartctl -A /dev/hdb,3\n"
         "                (Prints Attributes for physical drive 3 on 3ware 9000 RAID)\n"
         "  smartctl -A /dev/tw_cli/c0/p1\n"
         "            (Prints Attributes for 3ware controller 0, port 1 using tw_cli)\n"
         "  smartctl --all --device=areca,3/1 /dev/arcmsr0\n"
         "           (Prints all SMART info for 3rd ATA disk of the 1st enclosure\n"
         "            on 1st Areca RAID controller)\n"
         "\n"
         "  ATA SMART access methods and ordering may be specified by modifiers\n"
         "  following the device name: /dev/hdX:[saicm], where\n"
         "  's': SMART_* IOCTLs,         'a': IOCTL_ATA_PASS_THROUGH,\n"
         "  'i': IOCTL_IDE_PASS_THROUGH, 'f': IOCTL_STORAGE_*,\n"
         "  'm': IOCTL_SCSI_MINIPORT_*.\n"
      + strprintf(
         "  The default on this system is /dev/sdX:%s\n", ata_get_def_options()
        );
}


bool win_smart_interface::disable_system_auto_standby(bool disable)
{
  if (disable) {
    SYSTEM_POWER_STATUS ps;
    if (!GetSystemPowerStatus(&ps))
      return set_err(ENOSYS, "Unknown power status");
    if (ps.ACLineStatus != 1) {
      SetThreadExecutionState(ES_CONTINUOUS);
      if (ps.ACLineStatus == 0)
        set_err(EIO, "AC offline");
      else
        set_err(EIO, "Unknown AC line status");
      return false;
    }
  }

  if (!SetThreadExecutionState(ES_CONTINUOUS | (disable ? ES_SYSTEM_REQUIRED : 0)))
    return set_err(ENOSYS);
  return true;
}


/////////////////////////////////////////////////////////////////////////////
// ATA Interface
/////////////////////////////////////////////////////////////////////////////

#define SMART_CYL_LOW  0x4F
#define SMART_CYL_HI   0xC2

static void print_ide_regs(const IDEREGS * r, int out)
{
  pout("%s=0x%02x,%s=0x%02x, SC=0x%02x, SN=0x%02x, CL=0x%02x, CH=0x%02x, SEL=0x%02x\n",
    (out?"STS":"CMD"), r->bCommandReg, (out?"ERR":" FR"), r->bFeaturesReg,
    r->bSectorCountReg, r->bSectorNumberReg, r->bCylLowReg, r->bCylHighReg, r->bDriveHeadReg);
}

static void print_ide_regs_io(const IDEREGS * ri, const IDEREGS * ro)
{
  pout("    Input : "); print_ide_regs(ri, 0);
  if (ro) {
    pout("    Output: "); print_ide_regs(ro, 1);
  }
}

/////////////////////////////////////////////////////////////////////////////

// call SMART_GET_VERSION, return device map or -1 on error

static int smart_get_version(HANDLE hdevice, GETVERSIONINPARAMS_EX * ata_version_ex = 0)
{
  GETVERSIONINPARAMS vers; memset(&vers, 0, sizeof(vers));
  const GETVERSIONINPARAMS_EX & vers_ex = (const GETVERSIONINPARAMS_EX &)vers;
  DWORD num_out;

  if (!DeviceIoControl(hdevice, SMART_GET_VERSION,
    NULL, 0, &vers, sizeof(vers), &num_out, NULL)) {
    if (ata_debugmode)
      pout("  SMART_GET_VERSION failed, Error=%u\n", (unsigned)GetLastError());
    errno = ENOSYS;
    return -1;
  }
  assert(num_out == sizeof(GETVERSIONINPARAMS));

  if (ata_debugmode > 1) {
    pout("  SMART_GET_VERSION suceeded, bytes returned: %u\n"
         "    Vers = %d.%d, Caps = 0x%x, DeviceMap = 0x%02x\n",
      (unsigned)num_out, vers.bVersion, vers.bRevision,
      (unsigned)vers.fCapabilities, vers.bIDEDeviceMap);
    if (vers_ex.wIdentifier == SMART_VENDOR_3WARE)
      pout("    Identifier = %04x(3WARE), ControllerId=%u, DeviceMapEx = 0x%08x\n",
      vers_ex.wIdentifier, vers_ex.wControllerId, (unsigned)vers_ex.dwDeviceMapEx);
  }

  if (ata_version_ex)
    *ata_version_ex = vers_ex;

  // TODO: Check vers.fCapabilities here?
  return vers.bIDEDeviceMap;
}


// call SMART_* ioctl

static int smart_ioctl(HANDLE hdevice, IDEREGS * regs, char * data, unsigned datasize, int port)
{
  SENDCMDINPARAMS inpar;
  SENDCMDINPARAMS_EX & inpar_ex = (SENDCMDINPARAMS_EX &)inpar;

  unsigned char outbuf[sizeof(SENDCMDOUTPARAMS)-1 + 512];
  const SENDCMDOUTPARAMS * outpar;
  DWORD code, num_out;
  unsigned int size_out;
  const char * name;

  memset(&inpar, 0, sizeof(inpar));
  inpar.irDriveRegs = *regs;

  // Older drivers may require bits 5 and 7 set
  // ATA-3: bits shall be set, ATA-4 and later: bits are obsolete
  inpar.irDriveRegs.bDriveHeadReg |= 0xa0;

  // Drive number 0-3 was required on Win9x/ME only
  //inpar.irDriveRegs.bDriveHeadReg |= (drive & 1) << 4;
  //inpar.bDriveNumber = drive;

  if (port >= 0) {
    // Set RAID port
    inpar_ex.wIdentifier = SMART_VENDOR_3WARE;
    inpar_ex.bPortNumber = port;
  }

  if (datasize == 512) {
    code = SMART_RCV_DRIVE_DATA; name = "SMART_RCV_DRIVE_DATA";
    inpar.cBufferSize = size_out = 512;
  }
  else if (datasize == 0) {
    code = SMART_SEND_DRIVE_COMMAND; name = "SMART_SEND_DRIVE_COMMAND";
    if (regs->bFeaturesReg == ATA_SMART_STATUS)
      size_out = sizeof(IDEREGS); // ioctl returns new IDEREGS as data
      // Note: cBufferSize must be 0 on Win9x
    else
      size_out = 0;
  }
  else {
    errno = EINVAL;
    return -1;
  }

  memset(&outbuf, 0, sizeof(outbuf));

  if (!DeviceIoControl(hdevice, code, &inpar, sizeof(SENDCMDINPARAMS)-1,
    outbuf, sizeof(SENDCMDOUTPARAMS)-1 + size_out, &num_out, NULL)) {
    // CAUTION: DO NOT change "regs" Parameter in this case, see win_ata_device::ata_pass_through()
    long err = GetLastError();
    if (ata_debugmode && (err != ERROR_INVALID_PARAMETER || ata_debugmode > 1)) {
      pout("  %s failed, Error=%ld\n", name, err);
      print_ide_regs_io(regs, NULL);
    }
    errno = (   err == ERROR_INVALID_FUNCTION/*9x*/
             || err == ERROR_INVALID_PARAMETER/*NT/2K/XP*/
             || err == ERROR_NOT_SUPPORTED ? ENOSYS : EIO);
    return -1;
  }
  // NOTE: On Win9x, inpar.irDriveRegs now contains the returned regs

  outpar = (const SENDCMDOUTPARAMS *)outbuf;

  if (outpar->DriverStatus.bDriverError) {
    if (ata_debugmode) {
      pout("  %s failed, DriverError=0x%02x, IDEError=0x%02x\n", name,
        outpar->DriverStatus.bDriverError, outpar->DriverStatus.bIDEError);
      print_ide_regs_io(regs, NULL);
    }
    errno = (!outpar->DriverStatus.bIDEError ? ENOSYS : EIO);
    return -1;
  }

  if (ata_debugmode > 1) {
    pout("  %s suceeded, bytes returned: %u (buffer %u)\n", name,
      (unsigned)num_out, (unsigned)outpar->cBufferSize);
    print_ide_regs_io(regs, (regs->bFeaturesReg == ATA_SMART_STATUS ?
      (const IDEREGS *)(outpar->bBuffer) : NULL));
  }

  if (datasize)
    memcpy(data, outpar->bBuffer, 512);
  else if (regs->bFeaturesReg == ATA_SMART_STATUS) {
    if (nonempty(outpar->bBuffer, sizeof(IDEREGS)))
      memcpy(regs, outpar->bBuffer, sizeof(IDEREGS));
    else {  // Workaround for driver not returning regs
      if (ata_debugmode)
        pout("  WARNING: driver does not return ATA registers in output buffer!\n");
      *regs = inpar.irDriveRegs;
    }
  }

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// IDE PASS THROUGH (2000, XP, undocumented)
//
// Based on WinATA.cpp, 2002 c't/Matthias Withopf
// ftp://ftp.heise.de/pub/ct/listings/0207-218.zip

static int ide_pass_through_ioctl(HANDLE hdevice, IDEREGS * regs, char * data, unsigned datasize)
{
  if (datasize > 512) {
    errno = EINVAL;
    return -1;
  }
  unsigned int size = sizeof(ATA_PASS_THROUGH)-1 + datasize;
  ATA_PASS_THROUGH * buf = (ATA_PASS_THROUGH *)VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
  DWORD num_out;
  const unsigned char magic = 0xcf;

  if (!buf) {
    errno = ENOMEM;
    return -1;
  }

  buf->IdeReg = *regs;
  buf->DataBufferSize = datasize;
  if (datasize)
    buf->DataBuffer[0] = magic;

  if (!DeviceIoControl(hdevice, IOCTL_IDE_PASS_THROUGH,
    buf, size, buf, size, &num_out, NULL)) {
    long err = GetLastError();
    if (ata_debugmode) {
      pout("  IOCTL_IDE_PASS_THROUGH failed, Error=%ld\n", err);
      print_ide_regs_io(regs, NULL);
    }
    VirtualFree(buf, 0, MEM_RELEASE);
    errno = (err == ERROR_INVALID_FUNCTION || err == ERROR_NOT_SUPPORTED ? ENOSYS : EIO);
    return -1;
  }

  // Check ATA status
  if (buf->IdeReg.bCommandReg/*Status*/ & 0x01) {
    if (ata_debugmode) {
      pout("  IOCTL_IDE_PASS_THROUGH command failed:\n");
      print_ide_regs_io(regs, &buf->IdeReg);
    }
    VirtualFree(buf, 0, MEM_RELEASE);
    errno = EIO;
    return -1;
  }

  // Check and copy data
  if (datasize) {
    if (   num_out != size
        || (buf->DataBuffer[0] == magic && !nonempty(buf->DataBuffer+1, datasize-1))) {
      if (ata_debugmode) {
        pout("  IOCTL_IDE_PASS_THROUGH output data missing (%u, %u)\n",
          (unsigned)num_out, (unsigned)buf->DataBufferSize);
        print_ide_regs_io(regs, &buf->IdeReg);
      }
      VirtualFree(buf, 0, MEM_RELEASE);
      errno = EIO;
      return -1;
    }
    memcpy(data, buf->DataBuffer, datasize);
  }

  if (ata_debugmode > 1) {
    pout("  IOCTL_IDE_PASS_THROUGH suceeded, bytes returned: %u (buffer %u)\n",
      (unsigned)num_out, (unsigned)buf->DataBufferSize);
    print_ide_regs_io(regs, &buf->IdeReg);
  }
  *regs = buf->IdeReg;

  // Caution: VirtualFree() fails if parameter "dwSize" is nonzero
  VirtualFree(buf, 0, MEM_RELEASE);
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// ATA PASS THROUGH (Win2003, XP SP2)

// Warning:
// IOCTL_ATA_PASS_THROUGH[_DIRECT] can only handle one interrupt/DRQ data
// transfer per command. Therefore, multi-sector transfers are only supported
// for the READ/WRITE MULTIPLE [EXT] commands. Other commands like READ/WRITE SECTORS
// or READ/WRITE LOG EXT work only with single sector transfers.
// The latter are supported on Vista (only) through new ATA_FLAGS_NO_MULTIPLE.
// See:
// http://social.msdn.microsoft.com/Forums/en-US/storageplatformata/thread/eb408507-f221-455b-9bbb-d1069b29c4da

static int ata_pass_through_ioctl(HANDLE hdevice, IDEREGS * regs, IDEREGS * prev_regs, char * data, int datasize)
{
  const int max_sectors = 32; // TODO: Allocate dynamic buffer

  typedef struct {
    ATA_PASS_THROUGH_EX apt;
    ULONG Filler;
    UCHAR ucDataBuf[max_sectors * 512];
  } ATA_PASS_THROUGH_EX_WITH_BUFFERS;

  const unsigned char magic = 0xcf;

  ATA_PASS_THROUGH_EX_WITH_BUFFERS ab; memset(&ab, 0, sizeof(ab));
  ab.apt.Length = sizeof(ATA_PASS_THROUGH_EX);
  //ab.apt.PathId = 0;
  //ab.apt.TargetId = 0;
  //ab.apt.Lun = 0;
  ab.apt.TimeOutValue = 10;
  unsigned size = offsetof(ATA_PASS_THROUGH_EX_WITH_BUFFERS, ucDataBuf);
  ab.apt.DataBufferOffset = size;

  if (datasize > 0) {
    if (datasize > (int)sizeof(ab.ucDataBuf)) {
      errno = EINVAL;
      return -1;
    }
    ab.apt.AtaFlags = ATA_FLAGS_DATA_IN;
    ab.apt.DataTransferLength = datasize;
    size += datasize;
    ab.ucDataBuf[0] = magic;
  }
  else if (datasize < 0) {
    if (-datasize > (int)sizeof(ab.ucDataBuf)) {
      errno = EINVAL;
      return -1;
    }
    ab.apt.AtaFlags = ATA_FLAGS_DATA_OUT;
    ab.apt.DataTransferLength = -datasize;
    size += -datasize;
    memcpy(ab.ucDataBuf, data, -datasize);
  }
  else {
    assert(ab.apt.AtaFlags == 0);
    assert(ab.apt.DataTransferLength == 0);
  }

  assert(sizeof(ab.apt.CurrentTaskFile) == sizeof(IDEREGS));
  IDEREGS * ctfregs = (IDEREGS *)ab.apt.CurrentTaskFile;
  IDEREGS * ptfregs = (IDEREGS *)ab.apt.PreviousTaskFile;
  *ctfregs = *regs;

  if (prev_regs) {
    *ptfregs = *prev_regs;
    ab.apt.AtaFlags |= ATA_FLAGS_48BIT_COMMAND;
  }

  DWORD num_out;
  if (!DeviceIoControl(hdevice, IOCTL_ATA_PASS_THROUGH,
    &ab, size, &ab, size, &num_out, NULL)) {
    long err = GetLastError();
    if (ata_debugmode) {
      pout("  IOCTL_ATA_PASS_THROUGH failed, Error=%ld\n", err);
      print_ide_regs_io(regs, NULL);
    }
    errno = (err == ERROR_INVALID_FUNCTION || err == ERROR_NOT_SUPPORTED ? ENOSYS : EIO);
    return -1;
  }

  // Check ATA status
  if (ctfregs->bCommandReg/*Status*/ & (0x01/*Err*/|0x08/*DRQ*/)) {
    if (ata_debugmode) {
      pout("  IOCTL_ATA_PASS_THROUGH command failed:\n");
      print_ide_regs_io(regs, ctfregs);
    }
    errno = EIO;
    return -1;
  }

  // Check and copy data
  if (datasize > 0) {
    if (   num_out != size
        || (ab.ucDataBuf[0] == magic && !nonempty(ab.ucDataBuf+1, datasize-1))) {
      if (ata_debugmode) {
        pout("  IOCTL_ATA_PASS_THROUGH output data missing (%u)\n", (unsigned)num_out);
        print_ide_regs_io(regs, ctfregs);
      }
      errno = EIO;
      return -1;
    }
    memcpy(data, ab.ucDataBuf, datasize);
  }

  if (ata_debugmode > 1) {
    pout("  IOCTL_ATA_PASS_THROUGH suceeded, bytes returned: %u\n", (unsigned)num_out);
    print_ide_regs_io(regs, ctfregs);
  }
  *regs = *ctfregs;
  if (prev_regs)
    *prev_regs = *ptfregs;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// SMART IOCTL via SCSI MINIPORT ioctl

// This function is handled by ATAPI port driver (atapi.sys) or by SCSI
// miniport driver (via SCSI port driver scsiport.sys).
// It can be used to skip the missing or broken handling of some SMART
// command codes (e.g. READ_LOG) in the disk class driver (disk.sys)

static int ata_via_scsi_miniport_smart_ioctl(HANDLE hdevice, IDEREGS * regs, char * data, int datasize)
{
  // Select code
  DWORD code = 0; const char * name = 0;
  if (regs->bCommandReg == ATA_IDENTIFY_DEVICE) {
    code = IOCTL_SCSI_MINIPORT_IDENTIFY; name = "IDENTIFY";
  }
  else if (regs->bCommandReg == ATA_SMART_CMD) switch (regs->bFeaturesReg) {
    case ATA_SMART_READ_VALUES:
      code = IOCTL_SCSI_MINIPORT_READ_SMART_ATTRIBS; name = "READ_SMART_ATTRIBS"; break;
    case ATA_SMART_READ_THRESHOLDS:
      code = IOCTL_SCSI_MINIPORT_READ_SMART_THRESHOLDS; name = "READ_SMART_THRESHOLDS"; break;
    case ATA_SMART_ENABLE:
      code = IOCTL_SCSI_MINIPORT_ENABLE_SMART; name = "ENABLE_SMART"; break;
    case ATA_SMART_DISABLE:
      code = IOCTL_SCSI_MINIPORT_DISABLE_SMART; name = "DISABLE_SMART"; break;
    case ATA_SMART_STATUS:
      code = IOCTL_SCSI_MINIPORT_RETURN_STATUS; name = "RETURN_STATUS"; break;
    case ATA_SMART_AUTOSAVE:
      code = IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTOSAVE; name = "ENABLE_DISABLE_AUTOSAVE"; break;
  //case ATA_SMART_SAVE: // obsolete since ATA-6, not used by smartmontools
  //  code = IOCTL_SCSI_MINIPORT_SAVE_ATTRIBUTE_VALUES; name = "SAVE_ATTRIBUTE_VALUES"; break;
    case ATA_SMART_IMMEDIATE_OFFLINE:
      code = IOCTL_SCSI_MINIPORT_EXECUTE_OFFLINE_DIAGS; name = "EXECUTE_OFFLINE_DIAGS"; break;
    case ATA_SMART_AUTO_OFFLINE:
      code = IOCTL_SCSI_MINIPORT_ENABLE_DISABLE_AUTO_OFFLINE; name = "ENABLE_DISABLE_AUTO_OFFLINE"; break;
    case ATA_SMART_READ_LOG_SECTOR:
      code = IOCTL_SCSI_MINIPORT_READ_SMART_LOG; name = "READ_SMART_LOG"; break;
    case ATA_SMART_WRITE_LOG_SECTOR:
      code = IOCTL_SCSI_MINIPORT_WRITE_SMART_LOG; name = "WRITE_SMART_LOG"; break;
  }
  if (!code) {
    errno = ENOSYS;
    return -1;
  }

  // Set SRB
  struct {
    SRB_IO_CONTROL srbc;
    union {
      SENDCMDINPARAMS in;
      SENDCMDOUTPARAMS out;
    } params;
    char space[512-1];
  } sb;
  ASSERT_SIZEOF(sb, sizeof(SRB_IO_CONTROL)+sizeof(SENDCMDINPARAMS)-1+512);
  memset(&sb, 0, sizeof(sb));

  unsigned size;
  if (datasize > 0) {
    if (datasize > (int)sizeof(sb.space)+1) {
      errno = EINVAL;
      return -1;
    }
    size = datasize;
  }
  else if (datasize < 0) {
    if (-datasize > (int)sizeof(sb.space)+1) {
      errno = EINVAL;
      return -1;
    }
    size = -datasize;
    memcpy(sb.params.in.bBuffer, data, size);
  }
  else if (code == IOCTL_SCSI_MINIPORT_RETURN_STATUS)
    size = sizeof(IDEREGS);
  else
    size = 0;
  sb.srbc.HeaderLength = sizeof(SRB_IO_CONTROL);
  memcpy(sb.srbc.Signature, "SCSIDISK", 8); // atapi.sys
  sb.srbc.Timeout = 60; // seconds
  sb.srbc.ControlCode = code;
  //sb.srbc.ReturnCode = 0;
  sb.srbc.Length = sizeof(SENDCMDINPARAMS)-1 + size;
  sb.params.in.irDriveRegs = *regs;
  sb.params.in.cBufferSize = size;

  // Call miniport ioctl
  size += sizeof(SRB_IO_CONTROL) + sizeof(SENDCMDINPARAMS)-1;
  DWORD num_out;
  if (!DeviceIoControl(hdevice, IOCTL_SCSI_MINIPORT,
    &sb, size, &sb, size, &num_out, NULL)) {
    long err = GetLastError();
    if (ata_debugmode) {
      pout("  IOCTL_SCSI_MINIPORT_%s failed, Error=%ld\n", name, err);
      print_ide_regs_io(regs, NULL);
    }
    errno = (err == ERROR_INVALID_FUNCTION || err == ERROR_NOT_SUPPORTED ? ENOSYS : EIO);
    return -1;
  }

  // Check result
  if (sb.srbc.ReturnCode) {
    if (ata_debugmode) {
      pout("  IOCTL_SCSI_MINIPORT_%s failed, ReturnCode=0x%08x\n", name, (unsigned)sb.srbc.ReturnCode);
      print_ide_regs_io(regs, NULL);
    }
    errno = EIO;
    return -1;
  }

  if (sb.params.out.DriverStatus.bDriverError) {
    if (ata_debugmode) {
      pout("  IOCTL_SCSI_MINIPORT_%s failed, DriverError=0x%02x, IDEError=0x%02x\n", name,
        sb.params.out.DriverStatus.bDriverError, sb.params.out.DriverStatus.bIDEError);
      print_ide_regs_io(regs, NULL);
    }
    errno = (!sb.params.out.DriverStatus.bIDEError ? ENOSYS : EIO);
    return -1;
  }

  if (ata_debugmode > 1) {
    pout("  IOCTL_SCSI_MINIPORT_%s suceeded, bytes returned: %u (buffer %u)\n", name,
      (unsigned)num_out, (unsigned)sb.params.out.cBufferSize);
    print_ide_regs_io(regs, (code == IOCTL_SCSI_MINIPORT_RETURN_STATUS ?
                             (const IDEREGS *)(sb.params.out.bBuffer) : 0));
  }

  if (datasize > 0)
    memcpy(data, sb.params.out.bBuffer, datasize);
  else if (datasize == 0 && code == IOCTL_SCSI_MINIPORT_RETURN_STATUS)
    memcpy(regs, sb.params.out.bBuffer, sizeof(IDEREGS));

  return 0;
}


/////////////////////////////////////////////////////////////////////////////

// ATA PASS THROUGH via 3ware specific SCSI MINIPORT ioctl

static int ata_via_3ware_miniport_ioctl(HANDLE hdevice, IDEREGS * regs, char * data, int datasize, int port)
{
  struct {
    SRB_IO_CONTROL srbc;
    IDEREGS regs;
    UCHAR buffer[512];
  } sb;
  ASSERT_SIZEOF(sb, sizeof(SRB_IO_CONTROL)+sizeof(IDEREGS)+512);

  if (!(0 <= datasize && datasize <= (int)sizeof(sb.buffer) && port >= 0)) {
    errno = EINVAL;
    return -1;
  }
  memset(&sb, 0, sizeof(sb));
  strncpy((char *)sb.srbc.Signature, "<3ware>", sizeof(sb.srbc.Signature));
  sb.srbc.HeaderLength = sizeof(SRB_IO_CONTROL);
  sb.srbc.Timeout = 60; // seconds
  sb.srbc.ControlCode = 0xA0000000;
  sb.srbc.ReturnCode = 0;
  sb.srbc.Length = sizeof(IDEREGS) + (datasize > 0 ? datasize : 1);
  sb.regs = *regs;
  sb.regs.bReserved = port;

  DWORD num_out;
  if (!DeviceIoControl(hdevice, IOCTL_SCSI_MINIPORT,
    &sb, sizeof(sb), &sb, sizeof(sb), &num_out, NULL)) {
    long err = GetLastError();
    if (ata_debugmode) {
      pout("  ATA via IOCTL_SCSI_MINIPORT failed, Error=%ld\n", err);
      print_ide_regs_io(regs, NULL);
    }
    errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
    return -1;
  }

  if (sb.srbc.ReturnCode) {
    if (ata_debugmode) {
      pout("  ATA via IOCTL_SCSI_MINIPORT failed, ReturnCode=0x%08x\n", (unsigned)sb.srbc.ReturnCode);
      print_ide_regs_io(regs, NULL);
    }
    errno = EIO;
    return -1;
  }

  // Copy data
  if (datasize > 0)
    memcpy(data, sb.buffer, datasize);

  if (ata_debugmode > 1) {
    pout("  ATA via IOCTL_SCSI_MINIPORT suceeded, bytes returned: %u\n", (unsigned)num_out);
    print_ide_regs_io(regs, &sb.regs);
  }
  *regs = sb.regs;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////

// 3ware specific call to update the devicemap returned by SMART_GET_VERSION.
// 3DM/CLI "Rescan Controller" function does not to always update it.

static int update_3ware_devicemap_ioctl(HANDLE hdevice)
{
  SRB_IO_CONTROL srbc;
  memset(&srbc, 0, sizeof(srbc));
  strncpy((char *)srbc.Signature, "<3ware>", sizeof(srbc.Signature));
  srbc.HeaderLength = sizeof(SRB_IO_CONTROL);
  srbc.Timeout = 60; // seconds
  srbc.ControlCode = 0xCC010014;
  srbc.ReturnCode = 0;
  srbc.Length = 0;

  DWORD num_out;
  if (!DeviceIoControl(hdevice, IOCTL_SCSI_MINIPORT,
    &srbc, sizeof(srbc), &srbc, sizeof(srbc), &num_out, NULL)) {
    long err = GetLastError();
    if (ata_debugmode)
      pout("  UPDATE DEVICEMAP via IOCTL_SCSI_MINIPORT failed, Error=%ld\n", err);
    errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
    return -1;
  }
  if (srbc.ReturnCode) {
    if (ata_debugmode)
      pout("  UPDATE DEVICEMAP via IOCTL_SCSI_MINIPORT failed, ReturnCode=0x%08x\n", (unsigned)srbc.ReturnCode);
    errno = EIO;
    return -1;
  }
  if (ata_debugmode > 1)
    pout("  UPDATE DEVICEMAP via IOCTL_SCSI_MINIPORT suceeded\n");
  return 0;
}



/////////////////////////////////////////////////////////////////////////////

// Routines for pseudo device /dev/tw_cli/*
// Parses output of 3ware "tw_cli /cx/py show all" or 3DM SMART data window


// Get clipboard data

static int get_clipboard(char * data, int datasize)
{
  if (!OpenClipboard(NULL))
    return -1;
  HANDLE h = GetClipboardData(CF_TEXT);
  if (!h) {
    CloseClipboard();
    return 0;
  }
  const void * p = GlobalLock(h);
  int n = GlobalSize(h);
  if (n > datasize)
    n = datasize;
  memcpy(data, p, n);
  GlobalFree(h);
  CloseClipboard();
  return n;
}


// Run a command, write stdout to dataout
// TODO: Combine with daemon_win32.cpp:daemon_spawn()

static int run_cmd(const char * cmd, char * dataout, int outsize)
{
  // Create stdout pipe
  SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, TRUE};
  HANDLE pipe_out_w, h;
  if (!CreatePipe(&h, &pipe_out_w, &sa/*inherit*/, outsize))
    return -1;
  HANDLE self = GetCurrentProcess();
  HANDLE pipe_out_r;
  if (!DuplicateHandle(self, h, self, &pipe_out_r,
    GENERIC_READ, FALSE/*!inherit*/, DUPLICATE_CLOSE_SOURCE)) {
    CloseHandle(pipe_out_w);
    return -1;
  }
  HANDLE pipe_err_w;
  if (!DuplicateHandle(self, pipe_out_w, self, &pipe_err_w,
    0, TRUE/*inherit*/, DUPLICATE_SAME_ACCESS)) {
    CloseHandle(pipe_out_r); CloseHandle(pipe_out_w);
    return -1;
  }

  // Create process
  STARTUPINFO si; memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
  si.hStdInput  = INVALID_HANDLE_VALUE;
  si.hStdOutput = pipe_out_w; si.hStdError  = pipe_err_w;
  si.dwFlags = STARTF_USESTDHANDLES;
  PROCESS_INFORMATION pi;
  if (!CreateProcess(
    NULL, const_cast<char *>(cmd),
    NULL, NULL, TRUE/*inherit*/,
    CREATE_NO_WINDOW/*do not create a new console window*/,
    NULL, NULL, &si, &pi)) {
    CloseHandle(pipe_err_w); CloseHandle(pipe_out_r); CloseHandle(pipe_out_w);
    return -1;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pipe_err_w); CloseHandle(pipe_out_w);

  // Copy stdout to output buffer
  int i = 0;
  while (i < outsize) {
    DWORD num_read;
    if (!ReadFile(pipe_out_r, dataout+i, outsize-i, &num_read, NULL) || num_read == 0)
      break;
    i += num_read;
  }
  CloseHandle(pipe_out_r);
  // Wait for process
  WaitForSingleObject(pi.hProcess, INFINITE);
  CloseHandle(pi.hProcess);
  return i;
}


static const char * findstr(const char * str, const char * sub)
{
  const char * s = strstr(str, sub);
  return (s ? s+strlen(sub) : "");
}


static void copy_swapped(unsigned char * dest, const char * src, int destsize)
{
  int srclen = strcspn(src, "\r\n");
  int i;
  for (i = 0; i < destsize-1 && i < srclen-1; i+=2) {
    dest[i] = src[i+1]; dest[i+1] = src[i];
  }
  if (i < destsize-1 && i < srclen)
    dest[i+1] = src[i];
}


// TODO: This is OS independent

win_tw_cli_device::win_tw_cli_device(smart_interface * intf, const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "tw_cli", req_type),
  m_ident_valid(false), m_smart_valid(false)
{
  memset(&m_ident_buf, 0, sizeof(m_ident_buf));
  memset(&m_smart_buf, 0, sizeof(m_smart_buf));
}


bool win_tw_cli_device::is_open() const
{
  return (m_ident_valid || m_smart_valid);
}


bool win_tw_cli_device::open()
{
  m_ident_valid = m_smart_valid = false;
  const char * name = skipdev(get_dev_name());
  // Read tw_cli or 3DM browser output into buffer
  char buffer[4096];
  int size = -1, n1 = -1, n2 = -1;
  if (!strcmp(name, "tw_cli/clip")) { // read clipboard
    size = get_clipboard(buffer, sizeof(buffer));
  }
  else if (!strcmp(name, "tw_cli/stdin")) {  // read stdin
    size = fread(buffer, 1, sizeof(buffer), stdin);
  }
  else if (sscanf(name, "tw_cli/%nc%*u/p%*u%n", &n1, &n2) >= 0 && n2 == (int)strlen(name)) {
    // tw_cli/cx/py => read output from "tw_cli /cx/py show all"
    char cmd[100];
    snprintf(cmd, sizeof(cmd), "tw_cli /%s show all", name+n1);
    if (ata_debugmode > 1)
      pout("%s: Run: \"%s\"\n", name, cmd);
    size = run_cmd(cmd, buffer, sizeof(buffer));
  }
  else {
    return set_err(EINVAL);
  }

  if (ata_debugmode > 1)
    pout("%s: Read %d bytes\n", name, size);
  if (size <= 0)
    return set_err(ENOENT);
  if (size >= (int)sizeof(buffer))
    return set_err(EIO);

  buffer[size] = 0;
  if (ata_debugmode > 1)
    pout("[\n%.100s%s\n]\n", buffer, (size>100?"...":""));

  // Fake identify sector
  ASSERT_SIZEOF(ata_identify_device, 512);
  ata_identify_device * id = &m_ident_buf;
  memset(id, 0, sizeof(*id));
  copy_swapped(id->model    , findstr(buffer, " Model = "   ), sizeof(id->model));
  copy_swapped(id->fw_rev   , findstr(buffer, " Firmware Version = "), sizeof(id->fw_rev));
  copy_swapped(id->serial_no, findstr(buffer, " Serial = "  ), sizeof(id->serial_no));
  unsigned long nblocks = 0; // "Capacity = N.N GB (N Blocks)"
  sscanf(findstr(buffer, "Capacity = "), "%*[^(\r\n](%lu", &nblocks);
  if (nblocks) {
    id->words047_079[49-47] = 0x0200; // size valid
    id->words047_079[60-47] = (unsigned short)(nblocks    ); // secs_16
    id->words047_079[61-47] = (unsigned short)(nblocks>>16); // secs_32
  }
  id->command_set_1 = 0x0001; id->command_set_2 = 0x4000; // SMART supported, words 82,83 valid
  id->cfs_enable_1  = 0x0001; id->csf_default   = 0x4000; // SMART enabled, words 85,87 valid

  // Parse smart data hex dump
  const char * s = findstr(buffer, "Drive Smart Data:");
  if (!*s)
    s = findstr(buffer, "Drive SMART Data:"); // tw_cli from 9.5.x
  if (!*s) {
    s = findstr(buffer, "S.M.A.R.T. (Controller"); // from 3DM browser window
    if (*s) {
      const char * s1 = findstr(s, "<td class"); // html version
      if (*s1)
        s = s1;
      s += strcspn(s, "\r\n");
    }
    else
      s = buffer; // try raw hex dump without header
  }
  unsigned char * sd = (unsigned char *)&m_smart_buf;
  int i = 0;
  for (;;) {
    unsigned x = ~0; int n = -1;
    if (!(sscanf(s, "%x %n", &x, &n) == 1 && !(x & ~0xff)))
      break;
    sd[i] = (unsigned char)x;
    if (!(++i < 512 && n > 0))
      break;
    s += n;
    if (*s == '<') // "<br>"
      s += strcspn(s, "\r\n");
  }
  if (i < 512) {
    if (!id->model[1]) {
      // No useful data found
      char * err = strstr(buffer, "Error:");
      if (!err)
        err = strstr(buffer, "error :");
      if (err && (err = strchr(err, ':'))) {
        // Show tw_cli error message
        err++;
        err[strcspn(err, "\r\n")] = 0;
        return set_err(EIO, "%s", err);
      }
      return set_err(EIO);
    }
    sd = 0;
  }

  m_ident_valid = true;
  m_smart_valid = !!sd;
  return true;
}


bool win_tw_cli_device::close()
{
  m_ident_valid = m_smart_valid = false;
  return true;
}


int win_tw_cli_device::ata_command_interface(smart_command_set command, int /*select*/, char * data)
{
  switch (command) {
    case IDENTIFY:
      if (!m_ident_valid)
        break;
      memcpy(data, &m_ident_buf, 512);
      return 0;
    case READ_VALUES:
      if (!m_smart_valid)
        break;
      memcpy(data, &m_smart_buf, 512);
      return 0;
    case ENABLE:
    case STATUS:
    case STATUS_CHECK: // Fake "good" SMART status
      return 0;
    default:
      break;
  }
  // Arrive here for all unsupported commands
  set_err(ENOSYS);
  return -1;
}


/////////////////////////////////////////////////////////////////////////////
// IOCTL_STORAGE_QUERY_PROPERTY

union STORAGE_DEVICE_DESCRIPTOR_DATA {
  STORAGE_DEVICE_DESCRIPTOR desc;
  char raw[256];
};

// Get STORAGE_DEVICE_DESCRIPTOR_DATA for device.
// (This works without admin rights)

static int storage_query_property_ioctl(HANDLE hdevice, STORAGE_DEVICE_DESCRIPTOR_DATA * data)
{
  STORAGE_PROPERTY_QUERY query = {StorageDeviceProperty, PropertyStandardQuery, {0} };
  memset(data, 0, sizeof(*data));

  DWORD num_out;
  if (!DeviceIoControl(hdevice, IOCTL_STORAGE_QUERY_PROPERTY,
    &query, sizeof(query), data, sizeof(*data), &num_out, NULL)) {
    if (ata_debugmode > 1 || scsi_debugmode > 1)
      pout("  IOCTL_STORAGE_QUERY_PROPERTY failed, Error=%u\n", (unsigned)GetLastError());
    errno = ENOSYS;
    return -1;
  }

  if (ata_debugmode > 1 || scsi_debugmode > 1) {
    pout("  IOCTL_STORAGE_QUERY_PROPERTY returns:\n"
         "    Vendor:   \"%s\"\n"
         "    Product:  \"%s\"\n"
         "    Revision: \"%s\"\n"
         "    Removable: %s\n"
         "    BusType:   0x%02x\n",
         (data->desc.VendorIdOffset        ? data->raw+data->desc.VendorIdOffset : "(null)"),
         (data->desc.ProductIdOffset       ? data->raw+data->desc.ProductIdOffset : "(null)"),
         (data->desc.ProductRevisionOffset ? data->raw+data->desc.ProductRevisionOffset : "(null)"),
         (data->desc.RemovableMedia? "Yes":"No"), data->desc.BusType
    );
  }
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// IOCTL_STORAGE_PREDICT_FAILURE

// Call IOCTL_STORAGE_PREDICT_FAILURE, return PredictFailure value
// or -1 on error, opionally return VendorSpecific data.
// (This works without admin rights)

static int storage_predict_failure_ioctl(HANDLE hdevice, char * data = 0)
{
  STORAGE_PREDICT_FAILURE pred;
  memset(&pred, 0, sizeof(pred));

  DWORD num_out;
  if (!DeviceIoControl(hdevice, IOCTL_STORAGE_PREDICT_FAILURE,
    0, 0, &pred, sizeof(pred), &num_out, NULL)) {
    if (ata_debugmode > 1)
      pout("  IOCTL_STORAGE_PREDICT_FAILURE failed, Error=%u\n", (unsigned)GetLastError());
    errno = ENOSYS;
    return -1;
  }

  if (ata_debugmode > 1) {
    pout("  IOCTL_STORAGE_PREDICT_FAILURE returns:\n"
         "    PredictFailure: 0x%08x\n"
         "    VendorSpecific: 0x%02x,0x%02x,0x%02x,...,0x%02x\n",
         (unsigned)pred.PredictFailure,
         pred.VendorSpecific[0], pred.VendorSpecific[1], pred.VendorSpecific[2],
         pred.VendorSpecific[sizeof(pred.VendorSpecific)-1]
    );
  }
  if (data)
    memcpy(data, pred.VendorSpecific, sizeof(pred.VendorSpecific));
  return (!pred.PredictFailure ? 0 : 1);
}


/////////////////////////////////////////////////////////////////////////////

// Return true if ATA drive behind a SAT layer
static bool is_sat(const STORAGE_DEVICE_DESCRIPTOR_DATA * data)
{
  if (!data->desc.VendorIdOffset)
    return false;
  if (strcmp(data->raw + data->desc.VendorIdOffset, "ATA     "))
    return false;
  return true;
}

// Return true if Intel ICHxR RAID volume
static bool is_intel_raid_volume(const STORAGE_DEVICE_DESCRIPTOR_DATA * data)
{
  if (!(data->desc.VendorIdOffset && data->desc.ProductIdOffset))
    return false;
  const char * vendor = data->raw + data->desc.VendorIdOffset;
  if (!(!strnicmp(vendor, "Intel", 5) && strspn(vendor+5, " ") == strlen(vendor+5)))
    return false;
  if (strnicmp(data->raw + data->desc.ProductIdOffset, "Raid ", 5))
    return false;
  return true;
}

// get DEV_* for open handle
static win_dev_type get_controller_type(HANDLE hdevice, bool admin, GETVERSIONINPARAMS_EX * ata_version_ex)
{
  // Get BusType from device descriptor
  STORAGE_DEVICE_DESCRIPTOR_DATA data;
  if (storage_query_property_ioctl(hdevice, &data))
    return DEV_UNKNOWN;

  // Newer BusType* values are missing in older includes
  switch ((int)data.desc.BusType) {
    case BusTypeAta:
    case 0x0b: // BusTypeSata
      // Certain Intel AHCI drivers (C600+/C220+) have broken
      // IOCTL_ATA_PASS_THROUGH support and a working SAT layer
      if (is_sat(&data))
        return DEV_SAT;

      if (ata_version_ex)
        memset(ata_version_ex, 0, sizeof(*ata_version_ex));
      return DEV_ATA;

    case BusTypeScsi:
    case BusTypeRAID:
      if (is_sat(&data))
        return DEV_SAT;

      // Intel ICHxR RAID volume: reports SMART_GET_VERSION but does not support SMART_*
      if (is_intel_raid_volume(&data))
        return DEV_SCSI;
      // LSI/3ware RAID volume: supports SMART_*
      if (admin && smart_get_version(hdevice, ata_version_ex) >= 0)
        return DEV_ATA;

      return DEV_SCSI;

    case 0x09: // BusTypeiScsi
    case 0x0a: // BusTypeSas
      if (is_sat(&data))
        return DEV_SAT;

      return DEV_SCSI;

    case BusTypeUsb:
      return DEV_USB;

    default:
      return DEV_UNKNOWN;
  }
  /*NOTREACHED*/
}

// get DEV_* for device path
static win_dev_type get_controller_type(const char * path, GETVERSIONINPARAMS_EX * ata_version_ex = 0)
{
  bool admin = true;
  HANDLE h = CreateFileA(path, GENERIC_READ|GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
  if (h == INVALID_HANDLE_VALUE) {
    admin = false;
    h = CreateFileA(path, 0,
      FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE)
      return DEV_UNKNOWN;
  }
  if (ata_debugmode > 1 || scsi_debugmode > 1)
    pout(" %s: successfully opened%s\n", path, (!admin ? " (without admin rights)" :""));
  win_dev_type type = get_controller_type(h, admin, ata_version_ex);
  CloseHandle(h);
  return type;
}

// get DEV_* for physical drive number
static win_dev_type get_phy_drive_type(int drive, GETVERSIONINPARAMS_EX * ata_version_ex)
{
  char path[30];
  snprintf(path, sizeof(path)-1, "\\\\.\\PhysicalDrive%d", drive);
  return get_controller_type(path, ata_version_ex);
}

static win_dev_type get_phy_drive_type(int drive)
{
  return get_phy_drive_type(drive, 0);
}

// get DEV_* for logical drive number
static win_dev_type get_log_drive_type(int drive)
{
  char path[30];
  snprintf(path, sizeof(path)-1, "\\\\.\\%c:", 'A'+drive);
  return get_controller_type(path);
}

// Build IDENTIFY information from STORAGE_DEVICE_DESCRIPTOR
static int get_identify_from_device_property(HANDLE hdevice, ata_identify_device * id)
{
  STORAGE_DEVICE_DESCRIPTOR_DATA data;
  if (storage_query_property_ioctl(hdevice, &data))
    return -1;

  memset(id, 0, sizeof(*id));

  // Some drivers split ATA model string into VendorId and ProductId,
  // others return it as ProductId only.
  char model[sizeof(id->model) + 1] = "";

  unsigned i = 0;
  if (data.desc.VendorIdOffset) {
    for ( ;i < sizeof(model)-1 && data.raw[data.desc.VendorIdOffset+i]; i++)
      model[i] = data.raw[data.desc.VendorIdOffset+i];
  }

  if (data.desc.ProductIdOffset) {
    while (i > 1 && model[i-2] == ' ') // Keep last blank from VendorId
      i--;
    // Ignore VendorId "ATA"
    if (i <= 4 && !strncmp(model, "ATA", 3) && (i == 3 || model[3] == ' '))
      i = 0;
    for (unsigned j = 0; i < sizeof(model)-1 && data.raw[data.desc.ProductIdOffset+j]; i++, j++)
      model[i] = data.raw[data.desc.ProductIdOffset+j];
  }

  while (i > 0 && model[i-1] == ' ')
    i--;
  model[i] = 0;
  copy_swapped(id->model, model, sizeof(id->model));

  if (data.desc.ProductRevisionOffset)
    copy_swapped(id->fw_rev, data.raw+data.desc.ProductRevisionOffset, sizeof(id->fw_rev));

  id->command_set_1 = 0x0001; id->command_set_2 = 0x4000; // SMART supported, words 82,83 valid
  id->cfs_enable_1  = 0x0001; id->csf_default   = 0x4000; // SMART enabled, words 85,87 valid
  return 0;
}

// Get Serial Number in IDENTIFY from WMI
static bool get_serial_from_wmi(int drive, ata_identify_device * id)
{
  bool debug = (ata_debugmode > 1);

  wbem_services ws;
  if (!ws.connect()) {
    if (debug)
      pout("WMI connect failed\n");
    return false;
  }

  wbem_object wo;
  if (!ws.query1(wo, "SELECT Model,SerialNumber FROM Win32_DiskDrive WHERE "
                     "DeviceID=\"\\\\\\\\.\\\\PHYSICALDRIVE%d\"", drive))
    return false;

  std::string serial = wo.get_str("SerialNumber");
  if (debug)
    pout("  WMI:PhysicalDrive%d: \"%s\", S/N:\"%s\"\n", drive, wo.get_str("Model").c_str(), serial.c_str());

  copy_swapped(id->serial_no, serial.c_str(), sizeof(id->serial_no));
  return true;
}


/////////////////////////////////////////////////////////////////////////////
// USB ID detection using WMI

// Get USB ID for a physical drive number
static bool get_usb_id(int drive, unsigned short & vendor_id, unsigned short & product_id)
{
  bool debug = (scsi_debugmode > 1);

  wbem_services ws;
  if (!ws.connect()) {
    if (debug)
      pout("WMI connect failed\n");
    return false;
  }

  // Get device name
  wbem_object wo;
  if (!ws.query1(wo, "SELECT Model FROM Win32_DiskDrive WHERE DeviceID=\"\\\\\\\\.\\\\PHYSICALDRIVE%d\"", drive))
    return false;

  std::string name = wo.get_str("Model");
  if (debug)
    pout("PhysicalDrive%d, \"%s\":\n", drive, name.c_str());

  // Get USB_CONTROLLER -> DEVICE associations
  wbem_enumerator we;
  if (!ws.query(we, "SELECT Antecedent,Dependent FROM Win32_USBControllerDevice"))
    return false;

  unsigned short usb_venid = 0, prev_usb_venid = 0;
  unsigned short usb_proid = 0, prev_usb_proid = 0;
  std::string prev_usb_ant;
  std::string prev_ant, ant, dep;

  const regular_expression regex("^.*PnPEntity\\.DeviceID=\"([^\"]*)\"", REG_EXTENDED);

  while (we.next(wo)) {
    prev_ant = ant;
    // Find next 'USB_CONTROLLER, DEVICE' pair
    ant = wo.get_str("Antecedent");
    dep = wo.get_str("Dependent");

    if (debug && ant != prev_ant)
      pout(" %s:\n", ant.c_str());

    // Extract DeviceID
    regmatch_t match[2];
    if (!(regex.execute(dep.c_str(), 2, match) && match[1].rm_so >= 0)) {
      if (debug)
        pout("  | (\"%s\")\n", dep.c_str());
      continue;
    }

    std::string devid(dep.c_str()+match[1].rm_so, match[1].rm_eo-match[1].rm_so);

    if (str_starts_with(devid, "USB\\\\VID_")) {
      // USB bridge entry, save CONTROLLER, ID
      int nc = -1;
      if (!(sscanf(devid.c_str(), "USB\\\\VID_%4hx&PID_%4hx%n",
            &prev_usb_venid, &prev_usb_proid, &nc) == 2 && nc == 9+4+5+4)) {
        prev_usb_venid = prev_usb_proid = 0;
      }
      prev_usb_ant = ant;
      if (debug)
        pout("  +-> \"%s\" [0x%04x:0x%04x]\n", devid.c_str(), prev_usb_venid, prev_usb_proid);
      continue;
    }
    else if (str_starts_with(devid, "USBSTOR\\\\")) {
      // USBSTOR device found
      if (debug)
        pout("  +--> \"%s\"\n", devid.c_str());

      // Retrieve name
      wbem_object wo2;
      if (!ws.query1(wo2, "SELECT Name FROM Win32_PnPEntity WHERE DeviceID=\"%s\"", devid.c_str()))
        continue;
      std::string name2 = wo2.get_str("Name");

      // Continue if not name of physical disk drive
      if (name2 != name) {
        if (debug)
          pout("  +---> (\"%s\")\n", name2.c_str());
        continue;
      }

      // Fail if previous USB bridge is associated to other controller or ID is unknown
      if (!(ant == prev_usb_ant && prev_usb_venid)) {
        if (debug)
          pout("  +---> \"%s\" (Error: No USB bridge found)\n", name2.c_str());
        return false;
      }

      // Handle multiple devices with same name
      if (usb_venid) {
        // Fail if multiple devices with same name have different USB bridge types
        if (!(usb_venid == prev_usb_venid && usb_proid == prev_usb_proid)) {
          if (debug)
            pout("  +---> \"%s\" (Error: More than one USB ID found)\n", name2.c_str());
          return false;
        }
      }

      // Found
      usb_venid = prev_usb_venid;
      usb_proid = prev_usb_proid;
      if (debug)
        pout("  +===> \"%s\" [0x%04x:0x%04x]\n", name2.c_str(), usb_venid, usb_proid);

      // Continue to check for duplicate names ...
    }
    else {
      if (debug)
        pout("  |   \"%s\"\n", devid.c_str());
    }
  }

  if (!usb_venid)
    return false;

  vendor_id = usb_venid;
  product_id = usb_proid;

  return true;
}


/////////////////////////////////////////////////////////////////////////////

// Call GetDevicePowerState()
// returns: 1=active, 0=standby, -1=error
// (This would also work for SCSI drives)

static int get_device_power_state(HANDLE hdevice)
{
  BOOL state = TRUE;
  if (!GetDevicePowerState(hdevice, &state)) {
    long err = GetLastError();
    if (ata_debugmode)
      pout("  GetDevicePowerState() failed, Error=%ld\n", err);
    errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
    // TODO: This may not work as expected on transient errors,
    // because smartd interprets -1 as SLEEP mode regardless of errno.
    return -1;
  }

  if (ata_debugmode > 1)
    pout("  GetDevicePowerState() succeeded, state=%d\n", state);
  return state;
}


/////////////////////////////////////////////////////////////////////////////

// Get default ATA device options

static const char * ata_get_def_options()
{
  return "pasifm"; // GetDevicePowerState(), ATA_, SMART_*, IDE_PASS_THROUGH,
                   // STORAGE_*, SCSI_MINIPORT_*
}


// Common routines for devices with HANDLEs

win_smart_device::~win_smart_device() throw()
{
  if (m_fh != INVALID_HANDLE_VALUE)
    ::CloseHandle(m_fh);
}

bool win_smart_device::is_open() const
{
  return (m_fh != INVALID_HANDLE_VALUE);
}

bool win_smart_device::close()
{
  if (m_fh == INVALID_HANDLE_VALUE)
    return true;
  BOOL rc = ::CloseHandle(m_fh);
  m_fh = INVALID_HANDLE_VALUE;
  return !!rc;
}

// ATA

win_ata_device::win_ata_device(smart_interface * intf, const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "ata", req_type),
  m_usr_options(false),
  m_admin(false),
  m_phydrive(-1),
  m_id_is_cached(false),
  m_is_3ware(false),
  m_port(-1),
  m_smartver_state(0)
{
}

win_ata_device::~win_ata_device() throw()
{
}


// Open ATA device

bool win_ata_device::open()
{
  const char * name = skipdev(get_dev_name()); int len = strlen(name);
  // [sh]d[a-z]([a-z])?(:[saicmfp]+)? => Physical drive 0-701, with options
  char drive[2+1] = "", options[8+1] = ""; int n1 = -1, n2 = -1;
  if (   sscanf(name, "%*[sh]d%2[a-z]%n:%6[saimfp]%n", drive, &n1, options, &n2) >= 1
      && ((n1 == len && !options[0]) || n2 == len)                                   ) {
    return open(sdxy_to_phydrive(drive), -1, options, -1);
  }
  // [sh]d[a-z],N(:[saicmfp3]+)? => Physical drive 0-701, RAID port N, with options
  drive[0] = 0; options[0] = 0; n1 = -1; n2 = -1;
  unsigned port = ~0;
  if (   sscanf(name, "%*[sh]d%2[a-z],%u%n:%7[saimfp3]%n", drive, &port, &n1, options, &n2) >= 2
      && port < 32 && ((n1 == len && !options[0]) || n2 == len)                                  ) {
    return open(sdxy_to_phydrive(drive), -1, options, port);
  }
  // pd<m>,N => Physical drive <m>, RAID port N
  int phydrive = -1; port = ~0; n1 = -1; n2 = -1;
  if (   sscanf(name, "pd%d%n,%u%n", &phydrive, &n1, &port, &n2) >= 1
      && phydrive >= 0 && ((n1 == len && (int)port < 0) || (n2 == len && port < 32))) {
    return open(phydrive, -1, "", (int)port);
  }
  // [a-zA-Z]: => Physical drive behind logical drive 0-25
  int logdrive = drive_letter(name);
  if (logdrive >= 0) {
    return open(-1, logdrive, "", -1);
  }

  return set_err(EINVAL);
}


bool win_ata_device::open(int phydrive, int logdrive, const char * options, int port)
{
  m_phydrive = -1;
  char devpath[30];
  if (0 <= phydrive && phydrive <= 255)
    snprintf(devpath, sizeof(devpath)-1, "\\\\.\\PhysicalDrive%d", (m_phydrive = phydrive));
  else if (0 <= logdrive && logdrive <= 'Z'-'A')
    snprintf(devpath, sizeof(devpath)-1, "\\\\.\\%c:", 'A'+logdrive);
  else
    return set_err(ENOENT);

  // Open device
  HANDLE h = INVALID_HANDLE_VALUE;
  if (!(*options && !options[strspn(options, "fp")])) {
    // Open with admin rights
    m_admin = true;
    h = CreateFileA(devpath, GENERIC_READ|GENERIC_WRITE,
      FILE_SHARE_READ|FILE_SHARE_WRITE,
      NULL, OPEN_EXISTING, 0, 0);
  }
  if (h == INVALID_HANDLE_VALUE) {
    // Open without admin rights
    m_admin = false;
    h = CreateFileA(devpath, 0,
      FILE_SHARE_READ|FILE_SHARE_WRITE,
      NULL, OPEN_EXISTING, 0, 0);
  }
  if (h == INVALID_HANDLE_VALUE) {
    long err = GetLastError();
    if (err == ERROR_FILE_NOT_FOUND)
      set_err(ENOENT, "%s: not found", devpath);
    else if (err == ERROR_ACCESS_DENIED)
      set_err(EACCES, "%s: access denied", devpath);
    else
      set_err(EIO, "%s: Error=%ld", devpath, err);
    return false;
  }
  set_fh(h);

  // Warn once if admin rights are missing
  if (!m_admin) {
    static bool noadmin_warning = false;
    if (!noadmin_warning) {
      pout("Warning: Limited functionality due to missing admin rights\n");
      noadmin_warning = true;
    }
  }

  if (ata_debugmode > 1)
    pout("%s: successfully opened%s\n", devpath, (!m_admin ? " (without admin rights)" :""));

  m_usr_options = false;
  if (*options) {
    // Save user options
    m_options = options; m_usr_options = true;
  }
  else if (port >= 0)
    // RAID: SMART_* and SCSI_MINIPORT
    m_options = "s3";
  else {
    // Set default options according to Windows version
    static const char * def_options = ata_get_def_options();
    m_options = def_options;
  }

  // SMART_GET_VERSION may spin up disk, so delay until first real SMART_* call
  m_port = port;
  if (port < 0)
    return true;

  // 3ware RAID: Get port map
  GETVERSIONINPARAMS_EX vers_ex;
  int devmap = smart_get_version(h, &vers_ex);

  // 3ware RAID if vendor id present
  m_is_3ware = (vers_ex.wIdentifier == SMART_VENDOR_3WARE);

  unsigned long portmap = 0;
  if (port >= 0 && devmap >= 0) {
    // 3ware RAID: check vendor id
    if (!m_is_3ware) {
      pout("SMART_GET_VERSION returns unknown Identifier = 0x%04x\n"
           "This is no 3ware 9000 controller or driver has no SMART support.\n",
           vers_ex.wIdentifier);
      devmap = -1;
    }
    else
      portmap = vers_ex.dwDeviceMapEx;
  }
  if (devmap < 0) {
    pout("%s: ATA driver has no SMART support\n", devpath);
    if (!is_permissive()) {
      close();
      return set_err(ENOSYS);
    }
    devmap = 0x0f;
  }
  m_smartver_state = 1;

  {
    // 3ware RAID: update devicemap first

    if (!update_3ware_devicemap_ioctl(h)) {
      if (   smart_get_version(h, &vers_ex) >= 0
          && vers_ex.wIdentifier == SMART_VENDOR_3WARE    )
        portmap = vers_ex.dwDeviceMapEx;
    }
    // Check port existence
    if (!(portmap & (1L << port))) {
      if (!is_permissive()) {
        close();
        return set_err(ENOENT, "%s: Port %d is empty or does not exist", devpath, port);
      }
    }
  }

  return true;
}


/////////////////////////////////////////////////////////////////////////////

// Interface to ATA devices
bool win_ata_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  // No multi-sector support for now, see above
  // warning about IOCTL_ATA_PASS_THROUGH
  if (!ata_cmd_is_supported(in,
    ata_device::supports_data_out |
    ata_device::supports_output_regs |
    ata_device::supports_48bit)
  )
    return false;

  // 3ware RAID: SMART DISABLE without port number disables SMART functions
  if (   m_is_3ware && m_port < 0
      && in.in_regs.command == ATA_SMART_CMD
      && in.in_regs.features == ATA_SMART_DISABLE)
    return set_err(ENOSYS, "SMART DISABLE requires 3ware port number");

  // Determine ioctl functions valid for this ATA cmd
  const char * valid_options = 0;

  switch (in.in_regs.command) {
    case ATA_IDENTIFY_DEVICE:
    case ATA_IDENTIFY_PACKET_DEVICE:
      // SMART_*, ATA_, IDE_, SCSI_PASS_THROUGH, STORAGE_PREDICT_FAILURE
      // and SCSI_MINIPORT_* if requested by user
      valid_options = (m_usr_options ? "saimf" : "saif");
      break;

    case ATA_CHECK_POWER_MODE:
      // Try GetDevicePowerState() first, ATA/IDE_PASS_THROUGH may spin up disk
      valid_options = "pai3";
      break;

    case ATA_SMART_CMD:
      switch (in.in_regs.features) {
        case ATA_SMART_READ_VALUES:
        case ATA_SMART_READ_THRESHOLDS:
        case ATA_SMART_AUTOSAVE:
        case ATA_SMART_ENABLE:
        case ATA_SMART_DISABLE:
        case ATA_SMART_AUTO_OFFLINE:
          // SMART_*, ATA_, IDE_, SCSI_PASS_THROUGH, STORAGE_PREDICT_FAILURE
          // and SCSI_MINIPORT_* if requested by user
          valid_options = (m_usr_options ? "saimf" : "saif");
          break;

        case ATA_SMART_IMMEDIATE_OFFLINE:
          // SMART_SEND_DRIVE_COMMAND does not support ABORT_SELF_TEST
          valid_options = (m_usr_options || in.in_regs.lba_low != 127/*ABORT*/ ?
                           "saim3" : "aim3");
          break;

        case ATA_SMART_READ_LOG_SECTOR:
          // SMART_RCV_DRIVE_DATA does not support READ_LOG
          // Try SCSI_MINIPORT also to skip buggy class driver
          // SMART functions do not support multi sector I/O.
          if (in.size == 512)
            valid_options = (m_usr_options ? "saim3" : "aim3");
          else
            valid_options = "a";
          break;

        case ATA_SMART_WRITE_LOG_SECTOR:
          // ATA_PASS_THROUGH, SCSI_MINIPORT, others don't support DATA_OUT
          // but SCSI_MINIPORT_* only if requested by user and single sector.
          valid_options = (in.size == 512 && m_usr_options ? "am" : "a");
          break;

        case ATA_SMART_STATUS:
          valid_options = (m_usr_options ? "saimf" : "saif");
          break;

        default:
          // Unknown SMART command, handle below
          break;
      }
      break;

    default:
      // Other ATA command, handle below
      break;
  }

  if (!valid_options) {
    // No special ATA command found above, select a generic pass through ioctl.
    if (!(   in.direction == ata_cmd_in::no_data
          || (in.direction == ata_cmd_in::data_in && in.size == 512))
         ||  in.in_regs.is_48bit_cmd()                               )
      // DATA_OUT, more than one sector, 48-bit command: ATA_PASS_THROUGH only
      valid_options = "a";
    else
      // ATA/IDE_PASS_THROUGH
      valid_options = "ai";
  }

  if (!m_admin) {
    // Restrict to IOCTL_STORAGE_*
    if (strchr(valid_options, 'f'))
      valid_options = "f";
    else if (strchr(valid_options, 'p'))
      valid_options = "p";
    else
      return set_err(ENOSYS, "Function requires admin rights");
  }

  // Set IDEREGS
  IDEREGS regs, prev_regs;
  {
    const ata_in_regs & lo = in.in_regs;
    regs.bFeaturesReg     = lo.features;
    regs.bSectorCountReg  = lo.sector_count;
    regs.bSectorNumberReg = lo.lba_low;
    regs.bCylLowReg       = lo.lba_mid;
    regs.bCylHighReg      = lo.lba_high;
    regs.bDriveHeadReg    = lo.device;
    regs.bCommandReg      = lo.command;
    regs.bReserved        = 0;
  }
  if (in.in_regs.is_48bit_cmd()) {
    const ata_in_regs & hi = in.in_regs.prev;
    prev_regs.bFeaturesReg     = hi.features;
    prev_regs.bSectorCountReg  = hi.sector_count;
    prev_regs.bSectorNumberReg = hi.lba_low;
    prev_regs.bCylLowReg       = hi.lba_mid;
    prev_regs.bCylHighReg      = hi.lba_high;
    prev_regs.bDriveHeadReg    = hi.device;
    prev_regs.bCommandReg      = hi.command;
    prev_regs.bReserved        = 0;
  }

  // Set data direction
  int datasize = 0;
  char * data = 0;
  switch (in.direction) {
    case ata_cmd_in::no_data:
      break;
    case ata_cmd_in::data_in:
      datasize = (int)in.size;
      data = (char *)in.buffer;
      break;
    case ata_cmd_in::data_out:
      datasize = -(int)in.size;
      data = (char *)in.buffer;
      break;
    default:
      return set_err(EINVAL, "win_ata_device::ata_pass_through: invalid direction=%d",
          (int)in.direction);
  }


  // Try all valid ioctls in the order specified in m_options
  bool powered_up = false;
  bool out_regs_set = false;
  bool id_is_cached = false;
  const char * options = m_options.c_str();

  for (int i = 0; ; i++) {
    char opt = options[i];

    if (!opt) {
      if (in.in_regs.command == ATA_CHECK_POWER_MODE && powered_up) {
        // Power up reported by GetDevicePowerState() and no ioctl available
        // to detect the actual mode of the drive => simulate ATA result ACTIVE/IDLE.
        regs.bSectorCountReg = 0xff;
        out_regs_set = true;
        break;
      }
      // No IOCTL found
      return set_err(ENOSYS);
    }
    if (!strchr(valid_options, opt))
      // Invalid for this command
      continue;

    errno = 0;
    assert(   datasize == 0 || datasize == 512
           || (datasize == -512 && strchr("am", opt))
           || (datasize > 512 && opt == 'a'));
    int rc;
    switch (opt) {
      default: assert(0);
      case 's':
        // call SMART_GET_VERSION once for each drive
        if (m_smartver_state > 1) {
          rc = -1; errno = ENOSYS;
          break;
        }
        if (!m_smartver_state) {
          assert(m_port == -1);
          GETVERSIONINPARAMS_EX vers_ex;
          if (smart_get_version(get_fh(), &vers_ex) < 0) {
            if (!failuretest_permissive) {
              m_smartver_state = 2;
              rc = -1; errno = ENOSYS;
              break;
            }
            failuretest_permissive--;
          }
          else  {
            // 3ware RAID if vendor id present
            m_is_3ware = (vers_ex.wIdentifier == SMART_VENDOR_3WARE);
          }

          m_smartver_state = 1;
        }
        rc = smart_ioctl(get_fh(), &regs, data, datasize, m_port);
        out_regs_set = (in.in_regs.features == ATA_SMART_STATUS);
        id_is_cached = (m_port < 0); // Not cached by 3ware driver
        break;
      case 'm':
        rc = ata_via_scsi_miniport_smart_ioctl(get_fh(), &regs, data, datasize);
        id_is_cached = (m_port < 0);
        break;
      case 'a':
        rc = ata_pass_through_ioctl(get_fh(), &regs,
          (in.in_regs.is_48bit_cmd() ? &prev_regs : 0),
          data, datasize);
        out_regs_set = true;
        break;
      case 'i':
        rc = ide_pass_through_ioctl(get_fh(), &regs, data, datasize);
        out_regs_set = true;
        break;
      case 'f':
        if (in.in_regs.command == ATA_IDENTIFY_DEVICE) {
            rc = get_identify_from_device_property(get_fh(), (ata_identify_device *)data);
            if (rc == 0 && m_phydrive >= 0)
              get_serial_from_wmi(m_phydrive, (ata_identify_device *)data);
            id_is_cached = true;
        }
        else if (in.in_regs.command == ATA_SMART_CMD) switch (in.in_regs.features) {
          case ATA_SMART_READ_VALUES:
            rc = storage_predict_failure_ioctl(get_fh(), data);
            if (rc > 0)
              rc = 0;
            break;
          case ATA_SMART_ENABLE:
            rc = 0;
            break;
          case ATA_SMART_STATUS:
            rc = storage_predict_failure_ioctl(get_fh());
            if (rc == 0) {
              // Good SMART status
              out.out_regs.lba_high = 0xc2; out.out_regs.lba_mid = 0x4f;
            }
            else if (rc > 0) {
              // Bad SMART status
              out.out_regs.lba_high = 0x2c; out.out_regs.lba_mid = 0xf4;
              rc = 0;
            }
            break;
          default:
            errno = ENOSYS; rc = -1;
        }
        else {
            errno = ENOSYS; rc = -1;
        }
        break;
      case '3':
        rc = ata_via_3ware_miniport_ioctl(get_fh(), &regs, data, datasize, m_port);
        out_regs_set = true;
        break;
      case 'p':
        assert(in.in_regs.command == ATA_CHECK_POWER_MODE && in.size == 0);
        rc = get_device_power_state(get_fh());
        if (rc == 0) {
          // Power down reported by GetDevicePowerState(), using a passthrough ioctl would
          // spin up the drive => simulate ATA result STANDBY.
          regs.bSectorCountReg = 0x00;
          out_regs_set = true;
        }
        else if (rc > 0) {
          // Power up reported by GetDevicePowerState(), but this reflects the actual mode
          // only if it is selected by the device driver => try a passthrough ioctl to get the
          // actual mode, if none available simulate ACTIVE/IDLE.
          powered_up = true;
          rc = -1; errno = ENOSYS;
        }
        break;
    }

    if (!rc)
      // Working ioctl found
      break;

    if (errno != ENOSYS)
      // Abort on I/O error
      return set_err(errno);

    out_regs_set = false;
    // CAUTION: *_ioctl() MUST NOT change "regs" Parameter in the ENOSYS case
  }

  // Return IDEREGS if set
  if (out_regs_set) {
    ata_out_regs & lo = out.out_regs;
    lo.error        = regs.bFeaturesReg;
    lo.sector_count = regs.bSectorCountReg;
    lo.lba_low      = regs.bSectorNumberReg;
    lo.lba_mid      = regs.bCylLowReg;
    lo.lba_high     = regs.bCylHighReg;
    lo.device       = regs.bDriveHeadReg;
    lo.status       = regs.bCommandReg;
    if (in.in_regs.is_48bit_cmd()) {
      ata_out_regs & hi = out.out_regs.prev;
      hi.sector_count = prev_regs.bSectorCountReg;
      hi.lba_low      = prev_regs.bSectorNumberReg;
      hi.lba_mid      = prev_regs.bCylLowReg;
      hi.lba_high     = prev_regs.bCylHighReg;
    }
  }

  if (   in.in_regs.command == ATA_IDENTIFY_DEVICE
      || in.in_regs.command == ATA_IDENTIFY_PACKET_DEVICE)
    // Update ata_identify_is_cached() result according to ioctl used.
    m_id_is_cached = id_is_cached;

  return true;
}

// Return true if OS caches the ATA identify sector
bool win_ata_device::ata_identify_is_cached() const
{
  return m_id_is_cached;
}


//////////////////////////////////////////////////////////////////////
// csmi_ata_device

bool csmi_device::get_phy_info(CSMI_SAS_PHY_INFO & phy_info)
{
  // Get driver info to check CSMI support
  CSMI_SAS_DRIVER_INFO_BUFFER driver_info_buf;
  memset(&driver_info_buf, 0, sizeof(driver_info_buf));
  if (!csmi_ioctl(CC_CSMI_SAS_GET_DRIVER_INFO, &driver_info_buf.IoctlHeader, sizeof(driver_info_buf)))
    return false;

  if (scsi_debugmode > 1) {
    const CSMI_SAS_DRIVER_INFO & driver_info = driver_info_buf.Information;
    pout("CSMI_SAS_DRIVER_INFO:\n");
    pout("  Name:        \"%.81s\"\n", driver_info.szName);
    pout("  Description: \"%.81s\"\n", driver_info.szDescription);
    pout("  Revision:    %d.%d\n", driver_info.usMajorRevision, driver_info.usMinorRevision);
  }

  // Get Phy info
  CSMI_SAS_PHY_INFO_BUFFER phy_info_buf;
  memset(&phy_info_buf, 0, sizeof(phy_info_buf));
  if (!csmi_ioctl(CC_CSMI_SAS_GET_PHY_INFO, &phy_info_buf.IoctlHeader, sizeof(phy_info_buf)))
    return false;

  phy_info = phy_info_buf.Information;

  const int max_number_of_phys = sizeof(phy_info.Phy) / sizeof(phy_info.Phy[0]);
  if (phy_info.bNumberOfPhys > max_number_of_phys)
    return set_err(EIO, "CSMI_SAS_PHY_INFO: Bogus NumberOfPhys=%d", phy_info.bNumberOfPhys);

  if (scsi_debugmode > 1) {
    pout("CSMI_SAS_PHY_INFO: NumberOfPhys=%d\n", phy_info.bNumberOfPhys);
    for (int i = 0; i < max_number_of_phys; i++) {
      const CSMI_SAS_PHY_ENTITY & pe = phy_info.Phy[i];
      const CSMI_SAS_IDENTIFY & id = pe.Identify, & at = pe.Attached;
      if (id.bDeviceType == CSMI_SAS_NO_DEVICE_ATTACHED)
        continue;

      pout("Phy[%d] Port:   0x%02x\n", i, pe.bPortIdentifier);
      pout("  Type:        0x%02x, 0x%02x\n", id.bDeviceType, at.bDeviceType);
      pout("  InitProto:   0x%02x, 0x%02x\n", id.bInitiatorPortProtocol, at.bInitiatorPortProtocol);
      pout("  TargetProto: 0x%02x, 0x%02x\n", id.bTargetPortProtocol, at.bTargetPortProtocol);
      pout("  PhyIdent:    0x%02x, 0x%02x\n", id.bPhyIdentifier, at.bPhyIdentifier);
      const unsigned char * b = id.bSASAddress;
      pout("  SASAddress:  %02x %02x %02x %02x %02x %02x %02x %02x, ",
        b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
      b = at.bSASAddress;
      pout(               "%02x %02x %02x %02x %02x %02x %02x %02x\n",
        b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7]);
    }
  }

  return true;
}

unsigned csmi_device::get_ports_used()
{
  CSMI_SAS_PHY_INFO phy_info;
  if (!get_phy_info(phy_info))
    return 0;

  unsigned ports_used = 0;
  for (unsigned i = 0; i < sizeof(phy_info.Phy) / sizeof(phy_info.Phy[0]); i++) {
    const CSMI_SAS_PHY_ENTITY & pe = phy_info.Phy[i];
    if (pe.Identify.bDeviceType == CSMI_SAS_NO_DEVICE_ATTACHED)
      continue;
    if (pe.Attached.bDeviceType == CSMI_SAS_NO_DEVICE_ATTACHED)
      continue;
    switch (pe.Attached.bTargetPortProtocol) {
      case CSMI_SAS_PROTOCOL_SATA:
      case CSMI_SAS_PROTOCOL_STP:
        break;
      default:
        continue;
    }

    if (pe.bPortIdentifier == 0xff)
      // Older (<= 9.*) Intel RST driver
      ports_used |= (1 << i);
    else
      ports_used |= (1 << pe.bPortIdentifier);
  }

  return ports_used;
}


bool csmi_device::select_port(int port)
{
  CSMI_SAS_PHY_INFO phy_info;
  if (!get_phy_info(phy_info))
    return false;

  // Find port
  int max_port = -1, port_index = -1;
  for (unsigned i = 0; i < sizeof(phy_info.Phy) / sizeof(phy_info.Phy[0]); i++) {
    const CSMI_SAS_PHY_ENTITY & pe = phy_info.Phy[i];
    if (pe.Identify.bDeviceType == CSMI_SAS_NO_DEVICE_ATTACHED)
      continue;

    if (pe.bPortIdentifier == 0xff) {
      // Older (<= 9.*) Intel RST driver
      max_port = phy_info.bNumberOfPhys - 1;
      if (i >= phy_info.bNumberOfPhys)
        break;
      if ((int)i != port)
        continue;
    }
    else {
      if (pe.bPortIdentifier > max_port)
        max_port = pe.bPortIdentifier;
      if (pe.bPortIdentifier != port)
        continue;
    }

    port_index = i;
    break;
  }

  if (port_index < 0) {
    if (port <= max_port)
      return set_err(ENOENT, "Port %d is disabled", port);
    else
      return set_err(ENOENT, "Port %d does not exist (#ports: %d)", port,
        max_port + 1);
  }

  const CSMI_SAS_PHY_ENTITY & phy_ent = phy_info.Phy[port_index];
  if (phy_ent.Attached.bDeviceType == CSMI_SAS_NO_DEVICE_ATTACHED)
    return set_err(ENOENT, "No device on port %d", port);

  switch (phy_ent.Attached.bTargetPortProtocol) {
    case CSMI_SAS_PROTOCOL_SATA:
    case CSMI_SAS_PROTOCOL_STP:
      break;
    default:
      return set_err(ENOENT, "No SATA device on port %d (protocol: %d)",
        port, phy_ent.Attached.bTargetPortProtocol);
  }

  m_phy_ent = phy_ent;
  return true;
}


bool csmi_ata_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  if (!ata_cmd_is_supported(in,
    ata_device::supports_data_out |
    ata_device::supports_output_regs |
    ata_device::supports_multi_sector |
    ata_device::supports_48bit,
    "CMSI")
  )
    return false;

  // Create buffer with appropriate size
  raw_buffer pthru_raw_buf(sizeof(CSMI_SAS_STP_PASSTHRU_BUFFER) + in.size);
  CSMI_SAS_STP_PASSTHRU_BUFFER * pthru_buf = (CSMI_SAS_STP_PASSTHRU_BUFFER *)pthru_raw_buf.data();

  // Set addresses from Phy info
  CSMI_SAS_STP_PASSTHRU & pthru = pthru_buf->Parameters;
  const CSMI_SAS_PHY_ENTITY & phy_ent = get_phy_ent();
  pthru.bPhyIdentifier = phy_ent.Identify.bPhyIdentifier;
  pthru.bPortIdentifier = phy_ent.bPortIdentifier;
  memcpy(pthru.bDestinationSASAddress, phy_ent.Attached.bSASAddress,
    sizeof(pthru.bDestinationSASAddress));
  pthru.bConnectionRate = CSMI_SAS_LINK_RATE_NEGOTIATED;

  // Set transfer mode
  switch (in.direction) {
    case ata_cmd_in::no_data:
      pthru.uFlags = CSMI_SAS_STP_PIO | CSMI_SAS_STP_UNSPECIFIED;
      break;
    case ata_cmd_in::data_in:
      pthru.uFlags = CSMI_SAS_STP_PIO | CSMI_SAS_STP_READ;
      pthru.uDataLength = in.size;
      break;
    case ata_cmd_in::data_out:
      pthru.uFlags = CSMI_SAS_STP_PIO | CSMI_SAS_STP_WRITE;
      pthru.uDataLength = in.size;
      memcpy(pthru_buf->bDataBuffer, in.buffer, in.size);
      break;
    default:
      return set_err(EINVAL, "csmi_ata_device::ata_pass_through: invalid direction=%d",
        (int)in.direction);
  }

  // Set host-to-device FIS
  {
    unsigned char * fis = pthru.bCommandFIS;
    const ata_in_regs & lo = in.in_regs;
    const ata_in_regs & hi = in.in_regs.prev;
    fis[ 0] = 0x27; // Type: host-to-device FIS
    fis[ 1] = 0x80; // Bit7: Update command register
    fis[ 2] = lo.command;
    fis[ 3] = lo.features;
    fis[ 4] = lo.lba_low;
    fis[ 5] = lo.lba_mid;
    fis[ 6] = lo.lba_high;
    fis[ 7] = lo.device;
    fis[ 8] = hi.lba_low;
    fis[ 9] = hi.lba_mid;
    fis[10] = hi.lba_high;
    fis[11] = hi.features;
    fis[12] = lo.sector_count;
    fis[13] = hi.sector_count;
  }

  // Call ioctl
  if (!csmi_ioctl(CC_CSMI_SAS_STP_PASSTHRU, &pthru_buf->IoctlHeader, pthru_raw_buf.size())) {
    return false;
  }

  // Get device-to-host FIS
  {
    const unsigned char * fis = pthru_buf->Status.bStatusFIS;
    ata_out_regs & lo = out.out_regs;
    lo.status       = fis[ 2];
    lo.error        = fis[ 3];
    lo.lba_low      = fis[ 4];
    lo.lba_mid      = fis[ 5];
    lo.lba_high     = fis[ 6];
    lo.device       = fis[ 7];
    lo.sector_count = fis[12];
    if (in.in_regs.is_48bit_cmd()) {
      ata_out_regs & hi = out.out_regs.prev;
      hi.lba_low      = fis[ 8];
      hi.lba_mid      = fis[ 9];
      hi.lba_high     = fis[10];
      hi.sector_count = fis[13];
    }
  }

  // Get data
  if (in.direction == ata_cmd_in::data_in)
    // TODO: Check ptru_buf->Status.uDataBytes
    memcpy(in.buffer, pthru_buf->bDataBuffer, in.size);

  return true;
}


//////////////////////////////////////////////////////////////////////
// win_csmi_device

win_csmi_device::win_csmi_device(smart_interface * intf, const char * dev_name,
  const char * req_type)
: smart_device(intf, dev_name, "ata", req_type),
  m_fh(INVALID_HANDLE_VALUE), m_port(-1)
{
}

win_csmi_device::~win_csmi_device() throw()
{
  if (m_fh != INVALID_HANDLE_VALUE)
    CloseHandle(m_fh);
}

bool win_csmi_device::is_open() const
{
  return (m_fh != INVALID_HANDLE_VALUE);
}

bool win_csmi_device::close()
{
  if (m_fh == INVALID_HANDLE_VALUE)
    return true;
  BOOL rc = CloseHandle(m_fh);
  m_fh = INVALID_HANDLE_VALUE;
  return !!rc;
}


bool win_csmi_device::open_scsi()
{
  // Parse name
  unsigned contr_no = ~0, port = ~0; int nc = -1;
  const char * name = skipdev(get_dev_name());
  if (!(   sscanf(name, "csmi%u,%u%n", &contr_no, &port, &nc) >= 0
        && nc == (int)strlen(name) && contr_no <= 9 && port < 32)  )
    return set_err(EINVAL);

  // Open controller handle
  char devpath[30];
  snprintf(devpath, sizeof(devpath)-1, "\\\\.\\Scsi%u:", contr_no);

  HANDLE h = CreateFileA(devpath, GENERIC_READ|GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE,
    (SECURITY_ATTRIBUTES *)0, OPEN_EXISTING, 0, 0);

  if (h == INVALID_HANDLE_VALUE) {
    long err = GetLastError();
    if (err == ERROR_FILE_NOT_FOUND)
      set_err(ENOENT, "%s: not found", devpath);
    else if (err == ERROR_ACCESS_DENIED)
      set_err(EACCES, "%s: access denied", devpath);
    else
      set_err(EIO, "%s: Error=%ld", devpath, err);
    return false;
  }

  if (scsi_debugmode > 1)
    pout(" %s: successfully opened\n", devpath);

  m_fh = h;
  m_port = port;
  return true;
}


bool win_csmi_device::open()
{
  if (!open_scsi())
    return false;

  // Get Phy info for this drive
  if (!select_port(m_port)) {
    close();
    return false;
  }

  return true;
}


bool win_csmi_device::csmi_ioctl(unsigned code, IOCTL_HEADER * csmi_buffer,
  unsigned csmi_bufsiz)
{
  // Determine signature
  const char * sig;
  switch (code) {
    case CC_CSMI_SAS_GET_DRIVER_INFO:
      sig = CSMI_ALL_SIGNATURE; break;
    case CC_CSMI_SAS_GET_PHY_INFO:
    case CC_CSMI_SAS_STP_PASSTHRU:
      sig = CSMI_SAS_SIGNATURE; break;
    default:
      return set_err(ENOSYS, "Unknown CSMI code=%u", code);
  }

  // Set header
  csmi_buffer->HeaderLength = sizeof(IOCTL_HEADER);
  strncpy((char *)csmi_buffer->Signature, sig, sizeof(csmi_buffer->Signature));
  csmi_buffer->Timeout = CSMI_SAS_TIMEOUT;
  csmi_buffer->ControlCode = code;
  csmi_buffer->ReturnCode = 0;
  csmi_buffer->Length = csmi_bufsiz - sizeof(IOCTL_HEADER);

  // Call function
  DWORD num_out = 0;
  if (!DeviceIoControl(m_fh, IOCTL_SCSI_MINIPORT,
    csmi_buffer, csmi_bufsiz, csmi_buffer, csmi_bufsiz, &num_out, (OVERLAPPED*)0)) {
    long err = GetLastError();
    if (scsi_debugmode)
      pout("  IOCTL_SCSI_MINIPORT(CC_CSMI_%u) failed, Error=%ld\n", code, err);
    if (   err == ERROR_INVALID_FUNCTION
        || err == ERROR_NOT_SUPPORTED
        || err == ERROR_DEV_NOT_EXIST)
      return set_err(ENOSYS, "CSMI is not supported (Error=%ld)", err);
    else
      return set_err(EIO, "CSMI(%u) failed with Error=%ld", code, err);
  }

  // Check result
  if (csmi_buffer->ReturnCode) {
    if (scsi_debugmode) {
      pout("  IOCTL_SCSI_MINIPORT(CC_CSMI_%u) failed, ReturnCode=%u\n",
        code, (unsigned)csmi_buffer->ReturnCode);
    }
    return set_err(EIO, "CSMI(%u) failed with ReturnCode=%u", code, (unsigned)csmi_buffer->ReturnCode);
  }

  if (scsi_debugmode > 1)
    pout("  IOCTL_SCSI_MINIPORT(CC_CSMI_%u) succeeded, bytes returned: %u\n", code, (unsigned)num_out);

  return true;
}


/////////////////////////////////////////////////////////////////////////////
// SPT Interface (for SCSI devices and ATA devices behind SATLs)
// Only supported in NT and later
/////////////////////////////////////////////////////////////////////////////

win_scsi_device::win_scsi_device(smart_interface * intf,
  const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "scsi", req_type)
{
}

bool win_scsi_device::open()
{
  const char * name = skipdev(get_dev_name()); int len = strlen(name);
  // sd[a-z]([a-z])?,N => Physical drive 0-701, RAID port N
  char drive[2+1] = ""; int sub_addr = -1; int n1 = -1; int n2 = -1;
  if (   sscanf(name, "sd%2[a-z]%n,%d%n", drive, &n1, &sub_addr, &n2) >= 1
      && ((n1 == len && sub_addr == -1) || (n2 == len && sub_addr >= 0))  ) {
    return open(sdxy_to_phydrive(drive), -1, -1, sub_addr);
  }
  // pd<m>,N => Physical drive <m>, RAID port N
  int pd_num = -1; sub_addr = -1; n1 = -1; n2 = -1;
  if (   sscanf(name, "pd%d%n,%d%n", &pd_num, &n1, &sub_addr, &n2) >= 1
      && pd_num >= 0 && ((n1 == len && sub_addr == -1) || (n2 == len && sub_addr >= 0))) {
    return open(pd_num, -1, -1, sub_addr);
  }
  // [a-zA-Z]: => Physical drive behind logical drive 0-25
  int logdrive = drive_letter(name);
  if (logdrive >= 0) {
    return open(-1, logdrive, -1, -1);
  }
  // n?st<m> => tape drive <m> (same names used in Cygwin's /dev emulation)
  int tape_num = -1; n1 = -1;
  if (sscanf(name, "st%d%n", &tape_num, &n1) == 1 && tape_num >= 0 && n1 == len) {
    return open(-1, -1, tape_num, -1);
  }
  tape_num = -1; n1 = -1;
  if (sscanf(name, "nst%d%n", &tape_num, &n1) == 1 && tape_num >= 0 && n1 == len) {
    return open(-1, -1, tape_num, -1);
  }
  // tape<m> => tape drive <m>
  tape_num = -1; n1 = -1;
  if (sscanf(name, "tape%d%n", &tape_num, &n1) == 1 && tape_num >= 0 && n1 == len) {
    return open(-1, -1, tape_num, -1);
  }

  return set_err(EINVAL);
}

bool win_scsi_device::open(int pd_num, int ld_num, int tape_num, int /*sub_addr*/)
{
  char b[128];
  b[sizeof(b) - 1] = '\0';
  if (pd_num >= 0)
    snprintf(b, sizeof(b) - 1, "\\\\.\\PhysicalDrive%d", pd_num);
  else if (ld_num >= 0)
    snprintf(b, sizeof(b) - 1, "\\\\.\\%c:", 'A' + ld_num);
  else if (tape_num >= 0)
    snprintf(b, sizeof(b) - 1, "\\\\.\\TAPE%d", tape_num);
  else {
    set_err(EINVAL);
    return false;
  }

  // Open device
  HANDLE h = CreateFileA(b, GENERIC_READ|GENERIC_WRITE,
           FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
           OPEN_EXISTING, 0, 0);
  if (h == INVALID_HANDLE_VALUE) {
    set_err(ENODEV, "%s: Open failed, Error=%u", b, (unsigned)GetLastError());
    return false;
  }
  set_fh(h);
  return true;
}


typedef struct {
  SCSI_PASS_THROUGH_DIRECT spt;
  ULONG           Filler;
  UCHAR           ucSenseBuf[64];
} SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER;


// Issue command via IOCTL_SCSI_PASS_THROUGH instead of *_DIRECT.
// Used if DataTransferLength not supported by *_DIRECT.
static long scsi_pass_through_indirect(HANDLE h,
  SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER * sbd)
{
  struct SCSI_PASS_THROUGH_WITH_BUFFERS {
    SCSI_PASS_THROUGH spt;
    ULONG Filler;
    UCHAR ucSenseBuf[sizeof(sbd->ucSenseBuf)];
    UCHAR ucDataBuf[512];
  };

  SCSI_PASS_THROUGH_WITH_BUFFERS sb;
  memset(&sb, 0, sizeof(sb));

  // DATA_OUT not implemented yet
  if (!(   sbd->spt.DataIn == SCSI_IOCTL_DATA_IN
        && sbd->spt.DataTransferLength <= sizeof(sb.ucDataBuf)))
    return ERROR_INVALID_PARAMETER;

  sb.spt.Length = sizeof(sb.spt);
  sb.spt.CdbLength = sbd->spt.CdbLength;
  memcpy(sb.spt.Cdb, sbd->spt.Cdb, sizeof(sb.spt.Cdb));
  sb.spt.SenseInfoLength = sizeof(sb.ucSenseBuf);
  sb.spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucSenseBuf);
  sb.spt.DataIn = sbd->spt.DataIn;
  sb.spt.DataTransferLength = sbd->spt.DataTransferLength;
  sb.spt.DataBufferOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucDataBuf);
  sb.spt.TimeOutValue = sbd->spt.TimeOutValue;

  DWORD num_out;
  if (!DeviceIoControl(h, IOCTL_SCSI_PASS_THROUGH,
         &sb, sizeof(sb), &sb, sizeof(sb), &num_out, 0))
    return GetLastError();

  sbd->spt.ScsiStatus = sb.spt.ScsiStatus;
  if (sb.spt.ScsiStatus & SCSI_STATUS_CHECK_CONDITION)
    memcpy(sbd->ucSenseBuf, sb.ucSenseBuf, sizeof(sbd->ucSenseBuf));

  sbd->spt.DataTransferLength = sb.spt.DataTransferLength;
  if (sbd->spt.DataIn == SCSI_IOCTL_DATA_IN && sb.spt.DataTransferLength > 0)
    memcpy(sbd->spt.DataBuffer, sb.ucDataBuf, sb.spt.DataTransferLength);
  return 0;
}


// Interface to SPT SCSI devices.  See scsicmds.h and os_linux.c
bool win_scsi_device::scsi_pass_through(struct scsi_cmnd_io * iop)
{
  int report = scsi_debugmode; // TODO

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
      dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
    }
    else
      j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n");
    pout("%s", buff);
  }

  SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sb;
  if (iop->cmnd_len > (int)sizeof(sb.spt.Cdb)) {
    set_err(EINVAL, "cmnd_len too large");
    return false;
  }

  memset(&sb, 0, sizeof(sb));
  sb.spt.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
  sb.spt.CdbLength = iop->cmnd_len;
  memcpy(sb.spt.Cdb, iop->cmnd, iop->cmnd_len);
  sb.spt.SenseInfoLength = sizeof(sb.ucSenseBuf);
  sb.spt.SenseInfoOffset =
    offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
  sb.spt.TimeOutValue = (iop->timeout ? iop->timeout : 60);

  bool direct = true;
  switch (iop->dxfer_dir) {
    case DXFER_NONE:
      sb.spt.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
      break;
    case DXFER_FROM_DEVICE:
      sb.spt.DataIn = SCSI_IOCTL_DATA_IN;
      sb.spt.DataTransferLength = iop->dxfer_len;
      sb.spt.DataBuffer = iop->dxferp;
      // IOCTL_SCSI_PASS_THROUGH_DIRECT does not support single byte
      // transfers (needed for SMART STATUS check of JMicron USB bridges)
      if (sb.spt.DataTransferLength == 1)
        direct = false;
      break;
    case DXFER_TO_DEVICE:
      sb.spt.DataIn = SCSI_IOCTL_DATA_OUT;
      sb.spt.DataTransferLength = iop->dxfer_len;
      sb.spt.DataBuffer = iop->dxferp;
      break;
    default:
      set_err(EINVAL, "bad dxfer_dir");
      return false;
  }

  long err = 0;
  if (direct) {
    DWORD num_out;
    if (!DeviceIoControl(get_fh(), IOCTL_SCSI_PASS_THROUGH_DIRECT,
           &sb, sizeof(sb), &sb, sizeof(sb), &num_out, 0))
      err = GetLastError();
  }
  else
    err = scsi_pass_through_indirect(get_fh(), &sb);

  if (err)
    return set_err((err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO),
      "IOCTL_SCSI_PASS_THROUGH%s failed, Error=%ld",
      (direct ? "_DIRECT" : ""), err);

  iop->scsi_status = sb.spt.ScsiStatus;
  if (SCSI_STATUS_CHECK_CONDITION & iop->scsi_status) {
    int slen = sb.ucSenseBuf[7] + 8;

    if (slen > (int)sizeof(sb.ucSenseBuf))
      slen = sizeof(sb.ucSenseBuf);
    if (slen > (int)iop->max_sense_len)
      slen = iop->max_sense_len;
    memcpy(iop->sensep, sb.ucSenseBuf, slen);
    iop->resp_sense_len = slen;
    if (report) {
      if (report > 1) {
        pout("  >>> Sense buffer, len=%d:\n", slen);
        dStrHex(iop->sensep, slen , 1);
      }
      if ((iop->sensep[0] & 0x7f) > 0x71)
        pout("  status=%x: [desc] sense_key=%x asc=%x ascq=%x\n",
             iop->scsi_status, iop->sensep[1] & 0xf,
             iop->sensep[2], iop->sensep[3]);
      else
        pout("  status=%x: sense_key=%x asc=%x ascq=%x\n",
             iop->scsi_status, iop->sensep[2] & 0xf,
             iop->sensep[12], iop->sensep[13]);
    }
  } else
    iop->resp_sense_len = 0;

  if (iop->dxfer_len > sb.spt.DataTransferLength)
    iop->resid = iop->dxfer_len - sb.spt.DataTransferLength;
  else
    iop->resid = 0;

  if ((iop->dxfer_dir == DXFER_FROM_DEVICE) && (report > 1)) {
     int trunc = (iop->dxfer_len > 256) ? 1 : 0;
     pout("  Incoming data, len=%d, resid=%d%s:\n", (int)iop->dxfer_len, iop->resid,
        (trunc ? " [only first 256 bytes shown]" : ""));
        dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
  }
  return true;
}

// Interface to SPT SCSI devices.  See scsicmds.h and os_linux.c
static long scsi_pass_through_direct(HANDLE fd, UCHAR targetid, struct scsi_cmnd_io * iop)
{
  int report = scsi_debugmode; // TODO

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
      dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
    }
    else
      j += snprintf(&buff[j], (sz > j ? (sz - j) : 0), "]\n");
    pout("%s", buff);
  }

  SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sb;
  if (iop->cmnd_len > (int)sizeof(sb.spt.Cdb)) {
    return EINVAL;
  }

  memset(&sb, 0, sizeof(sb));
  sb.spt.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
  //sb.spt.PathId = 0;
  sb.spt.TargetId = targetid;
  //sb.spt.Lun = 0;
  sb.spt.CdbLength = iop->cmnd_len;
  memcpy(sb.spt.Cdb, iop->cmnd, iop->cmnd_len);
  sb.spt.SenseInfoLength = sizeof(sb.ucSenseBuf);
  sb.spt.SenseInfoOffset =
    offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
  sb.spt.TimeOutValue = (iop->timeout ? iop->timeout : 60);

  bool direct = true;
  switch (iop->dxfer_dir) {
    case DXFER_NONE:
      sb.spt.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
      break;
    case DXFER_FROM_DEVICE:
      sb.spt.DataIn = SCSI_IOCTL_DATA_IN;
      sb.spt.DataTransferLength = iop->dxfer_len;
      sb.spt.DataBuffer = iop->dxferp;
      // IOCTL_SCSI_PASS_THROUGH_DIRECT does not support single byte
      // transfers (needed for SMART STATUS check of JMicron USB bridges)
      if (sb.spt.DataTransferLength == 1)
        direct = false;
      break;
    case DXFER_TO_DEVICE:
      sb.spt.DataIn = SCSI_IOCTL_DATA_OUT;
      sb.spt.DataTransferLength = iop->dxfer_len;
      sb.spt.DataBuffer = iop->dxferp;
      break;
    default:
      return EINVAL;
  }

  long err = 0;
  if (direct) {
    DWORD num_out;
    if (!DeviceIoControl(fd, IOCTL_SCSI_PASS_THROUGH_DIRECT,
           &sb, sizeof(sb), &sb, sizeof(sb), &num_out, 0))
      err = GetLastError();
  }
  else
    err = scsi_pass_through_indirect(fd, &sb);

  if (err)
  {
    return err;
  }

  iop->scsi_status = sb.spt.ScsiStatus;
  if (SCSI_STATUS_CHECK_CONDITION & iop->scsi_status) {
    int slen = sb.ucSenseBuf[7] + 8;

    if (slen > (int)sizeof(sb.ucSenseBuf))
      slen = sizeof(sb.ucSenseBuf);
    if (slen > (int)iop->max_sense_len)
      slen = iop->max_sense_len;
    memcpy(iop->sensep, sb.ucSenseBuf, slen);
    iop->resp_sense_len = slen;
    if (report) {
      if (report > 1) {
        pout("  >>> Sense buffer, len=%d:\n", slen);
        dStrHex(iop->sensep, slen , 1);
      }
      if ((iop->sensep[0] & 0x7f) > 0x71)
        pout("  status=%x: [desc] sense_key=%x asc=%x ascq=%x\n",
             iop->scsi_status, iop->sensep[1] & 0xf,
             iop->sensep[2], iop->sensep[3]);
      else
        pout("  status=%x: sense_key=%x asc=%x ascq=%x\n",
             iop->scsi_status, iop->sensep[2] & 0xf,
             iop->sensep[12], iop->sensep[13]);
    }
  } else
    iop->resp_sense_len = 0;

  if (iop->dxfer_len > sb.spt.DataTransferLength)
    iop->resid = iop->dxfer_len - sb.spt.DataTransferLength;
  else
    iop->resid = 0;

  if ((iop->dxfer_dir == DXFER_FROM_DEVICE) && (report > 1)) {
     int trunc = (iop->dxfer_len > 256) ? 1 : 0;
     pout("  Incoming data, len=%d, resid=%d%s:\n", (int)iop->dxfer_len, iop->resid,
        (trunc ? " [only first 256 bytes shown]" : ""));
        dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
  }

  return 0;
}

// Areca RAID Controller(SAS Device)
win_areca_scsi_device::win_areca_scsi_device(smart_interface * intf, const char * dev_name, int disknum, int encnum)
: smart_device(intf, dev_name, "areca", "areca")
{
    set_fh(INVALID_HANDLE_VALUE);
    set_disknum(disknum);
    set_encnum(encnum);
    set_info().info_name = strprintf("%s [areca_disk#%02d_enc#%02d]", dev_name, disknum, encnum);
}

bool win_areca_scsi_device::open()
{
  HANDLE hFh;

  if( is_open() )
  {
    return true;
  }
  hFh = CreateFile( get_dev_name(),
                    GENERIC_READ|GENERIC_WRITE,
                    FILE_SHARE_READ|FILE_SHARE_WRITE,
                    NULL,
                    OPEN_EXISTING,
                    0,
                    NULL );
  if(hFh == INVALID_HANDLE_VALUE)
  {
    return false;
  }

  set_fh(hFh);
  return true;
}

smart_device * win_areca_scsi_device::autodetect_open()
{
  return this;
}

int win_areca_scsi_device::arcmsr_do_scsi_io(struct scsi_cmnd_io * iop)
{
   int ioctlreturn = 0;

   ioctlreturn = scsi_pass_through_direct(get_fh(), 16, iop);
   if ( ioctlreturn || iop->scsi_status )
   {
     ioctlreturn = scsi_pass_through_direct(get_fh(), 127, iop);
     if ( ioctlreturn || iop->scsi_status )
     {
       // errors found
       return -1;
     }
   }

   return ioctlreturn;
}

bool win_areca_scsi_device::arcmsr_lock()
{
#define    SYNCOBJNAME "Global\\SynIoctlMutex"
  int ctlrnum = -1;
  char mutexstr[64];

  if (sscanf(get_dev_name(), "\\\\.\\scsi%d:", &ctlrnum) < 1)
    return set_err(EINVAL, "unable to parse device name");

  snprintf(mutexstr, sizeof(mutexstr), "%s%d", SYNCOBJNAME, ctlrnum);
  m_mutex = CreateMutex(NULL, FALSE, mutexstr);
  if ( m_mutex == NULL )
  {
    return set_err(EIO, "CreateMutex failed");
  }

  // atomic access to driver
  WaitForSingleObject(m_mutex, INFINITE);

  return true;
}


bool win_areca_scsi_device::arcmsr_unlock()
{
  if( m_mutex != NULL)
  {
      ReleaseMutex(m_mutex);
      CloseHandle(m_mutex);
  }

  return true;
}


// Areca RAID Controller(SATA Disk)
win_areca_ata_device::win_areca_ata_device(smart_interface * intf, const char * dev_name, int disknum, int encnum)
: smart_device(intf, dev_name, "areca", "areca")
{
  set_fh(INVALID_HANDLE_VALUE);
  set_disknum(disknum);
  set_encnum(encnum);
  set_info().info_name = strprintf("%s [areca_disk#%02d_enc#%02d]", dev_name, disknum, encnum);
}

bool win_areca_ata_device::open()
{
  HANDLE hFh;

  if( is_open() )
  {
    return true;
  }
  hFh = CreateFile( get_dev_name(),
                    GENERIC_READ|GENERIC_WRITE,
                    FILE_SHARE_READ|FILE_SHARE_WRITE,
                    NULL,
                    OPEN_EXISTING,
                    0,
                    NULL );
  if(hFh == INVALID_HANDLE_VALUE)
  {
    return false;
  }

  set_fh(hFh);
  return true;
}

smart_device * win_areca_ata_device::autodetect_open()
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
  smart_device_auto_ptr newdev(new win_areca_scsi_device(smi(), get_dev_name(), get_disknum(), get_encnum()));
  close();
  delete this;
  newdev->open(); // TODO: Can possibly pass open fd

  return newdev.release();
}

int win_areca_ata_device::arcmsr_do_scsi_io(struct scsi_cmnd_io * iop)
{
   int ioctlreturn = 0;

   ioctlreturn = scsi_pass_through_direct(get_fh(), 16, iop);
   if ( ioctlreturn || iop->scsi_status )
   {
     ioctlreturn = scsi_pass_through_direct(get_fh(), 127, iop);
     if ( ioctlreturn || iop->scsi_status )
     {
       // errors found
       return -1;
     }
   }

   return ioctlreturn;
}

bool win_areca_ata_device::arcmsr_lock()
{
#define    SYNCOBJNAME "Global\\SynIoctlMutex"
  int ctlrnum = -1;
  char mutexstr[64];

  if (sscanf(get_dev_name(), "\\\\.\\scsi%d:", &ctlrnum) < 1)
    return set_err(EINVAL, "unable to parse device name");

  snprintf(mutexstr, sizeof(mutexstr), "%s%d", SYNCOBJNAME, ctlrnum);
  m_mutex = CreateMutex(NULL, FALSE, mutexstr);
  if ( m_mutex == NULL )
  {
    return set_err(EIO, "CreateMutex failed");
  }

  // atomic access to driver
  WaitForSingleObject(m_mutex, INFINITE);

  return true;
}


bool win_areca_ata_device::arcmsr_unlock()
{
  if( m_mutex != NULL)
  {
      ReleaseMutex(m_mutex);
      CloseHandle(m_mutex);
  }

  return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////////


} // namespace

/////////////////////////////////////////////////////////////////////////////

// Initialize platform interface and register with smi()
void smart_interface::init()
{
  {
    // Remove "." from DLL search path if supported
    // to prevent DLL preloading attacks
    BOOL (WINAPI * SetDllDirectoryA_p)(LPCSTR) = (BOOL (WINAPI *)(LPCSTR))
      GetProcAddress(GetModuleHandleA("kernel32.dll"), "SetDllDirectoryA");
    if (SetDllDirectoryA_p)
      SetDllDirectoryA_p("");
  }

  static os_win32::win_smart_interface the_win_interface;
  smart_interface::set(&the_win_interface);
}


#ifndef __CYGWIN__

// Get exe directory
// (prototype in utiliy.h)
std::string get_exe_dir()
{
  char path[MAX_PATH];
  // Get path of this exe
  if (!GetModuleFileNameA(GetModuleHandleA(0), path, sizeof(path)))
    throw std::runtime_error("GetModuleFileName() failed");
  // Replace backslash by slash
  int sl = -1;
  for (int i = 0; path[i]; i++)
    if (path[i] == '\\') {
      path[i] = '/'; sl = i;
    }
  // Remove filename
  if (sl >= 0)
    path[sl] = 0;
  return path;
}

#endif
