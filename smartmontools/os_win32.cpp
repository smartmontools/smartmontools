/*
 * os_win32.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004-10 Christian Franke <smartmontools-support@lists.sourceforge.net>
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
#include "atacmds.h"
#include "extern.h"
extern smartmonctrl * con; // con->permissive,reportataioctl
#include "scsicmds.h"
#include "utility.h"

#include "dev_interface.h"
#include "dev_ata_cmd_set.h"

#include <errno.h>

#ifdef _DEBUG
#include <assert.h>
#else
#undef assert
#define assert(x) /* */
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h> // offsetof()
#include <io.h> // access()

#ifdef __CYGWIN__
#include <cygwin/version.h> // CYGWIN_VERSION_DLL_MAJOR
#endif

// Macro to check constants at compile time using a dummy typedef
#define ASSERT_CONST(c, n) \
  typedef char assert_const_##c[((c) == (n)) ? 1 : -1]
#define ASSERT_SIZEOF(t, n) \
  typedef char assert_sizeof_##t[(sizeof(t) == (n)) ? 1 : -1]

const char * os_win32_cpp_cvsid = "$Id$";

// Disable Win9x/ME specific code if no longer supported by compiler.
#ifndef WIN9X_SUPPORT
  #if defined(CYGWIN_VERSION_DLL_MAJOR) && (CYGWIN_VERSION_DLL_MAJOR >= 1007)
    // Win9x/ME support was dropped in Cygwin 1.7
  #elif defined(_MSC_VER) && (_MSC_VER >= 1500)
    // Win9x/ME support was dropped in MSVC9 (cl.exe 15.0)
  #else
    #define WIN9X_SUPPORT 1
  #endif
#endif

/////////////////////////////////////////////////////////////////////////////

namespace os_win32 { // no need to publish anything, name provided for Doxygen

#ifdef _MSC_VER
#pragma warning(disable:4250)
#endif

// Running on Win9x/ME ?
#if WIN9X_SUPPORT
// Set true in win9x_smart_interface ctor.
static bool win9x = false;
#else
// Never true (const allows compiler to remove dead code).
const  bool win9x = false;
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
  bool m_id_is_cached; // ata_identify_is_cached() return value.
  int m_drive, m_port;
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

#if WIN9X_SUPPORT

class win_aspi_device
: public /*implements*/ scsi_device
{
public:
  win_aspi_device(smart_interface * intf, const char * dev_name, const char * req_type);

  virtual bool is_open() const;

  virtual bool open();

  virtual bool close();

  virtual bool scsi_pass_through(scsi_cmnd_io * iop);

private:
  int m_adapter;
  unsigned char m_id;
};

#endif // WIN9X_SUPPORT

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


//////////////////////////////////////////////////////////////////////
// Platform specific interfaces

// Common to all windows flavors
class win_smart_interface
: public /*implements part of*/ smart_interface
{
public:
  virtual std::string get_os_version_str();

  virtual std::string get_app_examples(const char * appname);

  virtual bool scan_smart_devices(smart_device_list & devlist, const char * type,
    const char * pattern = 0);

protected:
  virtual ata_device * get_ata_device(const char * name, const char * type);

//virtual scsi_device * get_scsi_device(const char * name, const char * type);

  virtual smart_device * autodetect_smart_device(const char * name);

  virtual bool ata_scan(smart_device_list & devlist) = 0;

  virtual bool scsi_scan(smart_device_list & devlist) = 0;
};

#if WIN9X_SUPPORT

// Win9x/ME reduced functionality
class win9x_smart_interface
: public /*extends*/ win_smart_interface
{
public:
  win9x_smart_interface()
    { win9x = true; }

protected:
  virtual scsi_device * get_scsi_device(const char * name, const char * type);

  virtual bool ata_scan(smart_device_list & devlist);

  virtual bool scsi_scan(smart_device_list & devlist);
};

#endif // WIN9X_SUPPORT

// WinNT,2000,XP,...
class winnt_smart_interface
: public /*extends*/ win_smart_interface
{
protected:
  virtual scsi_device * get_scsi_device(const char * name, const char * type);

  virtual smart_device * autodetect_smart_device(const char * name);

  virtual bool ata_scan(smart_device_list & devlist);

  virtual bool scsi_scan(smart_device_list & devlist);
};


//////////////////////////////////////////////////////////////////////

// Running on 64-bit Windows as 32-bit app ?
static bool is_wow64()
{
  HMODULE hk = GetModuleHandleA("kernel32");
  if (!hk)
    return false;
  BOOL (WINAPI * IsWow64Process_p)(HANDLE, PBOOL) =
    (BOOL (WINAPI *)(HANDLE, PBOOL))GetProcAddress(hk, "IsWow64Process");
  if (!IsWow64Process_p)
    return false;
  BOOL w64 = FALSE;
  if (!IsWow64Process_p(GetCurrentProcess(), &w64))
    return false;
  return !!w64;
}

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

  if (vi.dwPlatformId > 0xff || vi.dwMajorVersion > 0xff || vi.dwMinorVersion > 0xff)
    return vstr;

  const char * w;
  switch (vi.dwPlatformId << 16 | vi.dwMajorVersion << 8 | vi.dwMinorVersion) {
    case VER_PLATFORM_WIN32_WINDOWS<<16|0x0400| 0:
      w = (vi.szCSDVersion[1] == 'B' ||
           vi.szCSDVersion[1] == 'C'     ? "95-osr2" : "95");    break;
    case VER_PLATFORM_WIN32_WINDOWS<<16|0x0400|10:
      w = (vi.szCSDVersion[1] == 'A'     ? "98se"    : "98");    break;
    case VER_PLATFORM_WIN32_WINDOWS<<16|0x0400|90: w = "me";     break;
  //case VER_PLATFORM_WIN32_NT     <<16|0x0300|51: w = "nt3.51"; break;
    case VER_PLATFORM_WIN32_NT     <<16|0x0400| 0: w = "nt4";    break;
    case VER_PLATFORM_WIN32_NT     <<16|0x0500| 0: w = "2000";   break;
    case VER_PLATFORM_WIN32_NT     <<16|0x0500| 1:
      w = (!GetSystemMetrics(87/*SM_MEDIACENTER*/) ?   "xp"
                                                   :   "xp-mc"); break;
    case VER_PLATFORM_WIN32_NT     <<16|0x0500| 2:
      w = (!GetSystemMetrics(89/*SM_SERVERR2*/)    ?   "2003"
                                                   :   "2003r2"); break;
    case VER_PLATFORM_WIN32_NT     <<16|0x0600| 0:
      w = (vi.wProductType == VER_NT_WORKSTATION   ?   "vista"
                                                   :   "2008" );  break;
    case VER_PLATFORM_WIN32_NT     <<16|0x0600| 1:
      w = (vi.wProductType == VER_NT_WORKSTATION   ?   "win7"
                                                   :   "2008r2"); break;
    default: w = 0; break;
  }

  const char * w64 = (is_wow64() ? "(64)" : "");
  if (!w)
    snprintf(vptr, vlen, "-%s%lu.%lu%s",
      (vi.dwPlatformId==VER_PLATFORM_WIN32_NT ? "nt" : "9x"),
      vi.dwMajorVersion, vi.dwMinorVersion, w64);
  else if (vi.wServicePackMinor)
    snprintf(vptr, vlen, "-%s%s-sp%u.%u", w, w64, vi.wServicePackMajor, vi.wServicePackMinor);
  else if (vi.wServicePackMajor)
    snprintf(vptr, vlen, "-%s%s-sp%u", w, w64, vi.wServicePackMajor);
  else
    snprintf(vptr, vlen, "-%s%s", w, w64);
  return vstr;
}

// Return value for device detection functions
enum win_dev_type { DEV_UNKNOWN = 0, DEV_ATA, DEV_SCSI, DEV_USB };

static win_dev_type get_phy_drive_type(int drive);
static win_dev_type get_log_drive_type(int drive);
static bool get_usb_id(int drive, unsigned short & vendor_id,
                       unsigned short & product_id);

static const char * ata_get_def_options(void);


static int is_permissive()
{
  if (!con->permissive) {
    pout("To continue, add one or more '-T permissive' options.\n");
    return 0;
  }
  con->permissive--;
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
  if (!strncmp(testname, "tw_cli", 6))
    return new win_tw_cli_device(this, name, type);
  return new win_ata_device(this, name, type);
}

#ifdef WIN9X_SUPPORT

scsi_device * win9x_smart_interface::get_scsi_device(const char * name, const char * type)
{
  return new win_aspi_device(this, name, type);
}

#endif

scsi_device * winnt_smart_interface::get_scsi_device(const char * name, const char * type)
{
  const char * testname = skipdev(name);
  if (!strncmp(testname, "scsi", 4))
#if WIN9X_SUPPORT
    return new win_aspi_device(this, name, type);
#else
    return (set_err(EINVAL, "ASPI interface not supported"), (scsi_device *)0);
#endif
  return new win_scsi_device(this, name, type);
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

  char drive[1+1] = "";
  if (sscanf(name, "sd%1[a-z]", drive) == 1) {
    phydrive = drive[0] - 'a';
    return get_phy_drive_type(phydrive);
  }

  phydrive = -1;
  if (sscanf(name, "pd%d", &phydrive) == 1 && phydrive >= 0)
    return get_phy_drive_type(phydrive);
  return DEV_UNKNOWN;
}

smart_device * win_smart_interface::autodetect_smart_device(const char * name)
{
  const char * testname = skipdev(name);
  if (!strncmp(testname, "hd", 2))
    return new win_ata_device(this, name, "");
#if WIN9X_SUPPORT
  if (!strncmp(testname, "scsi", 4))
    return new win_aspi_device(this, name, "");
#endif
  if (!strncmp(testname, "tw_cli", 6))
    return new win_tw_cli_device(this, name, "");
  return 0;
}

smart_device * winnt_smart_interface::autodetect_smart_device(const char * name)
{
  smart_device * dev = win_smart_interface::autodetect_smart_device(name);
  if (dev)
    return dev;

  int phydrive = -1;
  win_dev_type type = get_dev_type(name, phydrive);

  if (type == DEV_ATA)
    return new win_ata_device(this, name, "");
  if (type == DEV_SCSI)
    return new win_scsi_device(this, name, "");

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


// makes a list of ATA or SCSI devices for the DEVICESCAN directive
bool win_smart_interface::scan_smart_devices(smart_device_list & devlist,
  const char * type, const char * pattern /* = 0*/)
{
  if (pattern) {
    set_err(EINVAL, "DEVICESCAN with pattern not implemented yet");
    return false;
  }

  if (!type || !strcmp(type, "ata")) {
    if (!ata_scan(devlist))
      return false;
  }

  if (!type || !strcmp(type, "scsi")) {
    if (!scsi_scan(devlist))
      return false;
  }
  return true;
}


// get examples for smartctl
std::string win_smart_interface::get_app_examples(const char * appname)
{
  if (strcmp(appname, "smartctl"))
    return "";
  return "=================================================== SMARTCTL EXAMPLES =====\n\n"
         "  smartctl -a /dev/hda                       (Prints all SMART information)\n\n"
         "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/hda\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long /dev/hda              (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/hda\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         "  smartctl -a /dev/scsi21\n"
         "             (Prints all information for SCSI disk on ASPI adapter 2, ID 1)\n"
         "  smartctl -a /dev/sda\n"
         "             (Prints all information for SCSI disk on PhysicalDrive 0)\n"
         "  smartctl -a /dev/pd3\n"
         "             (Prints all information for SCSI disk on PhysicalDrive 3)\n"
         "  smartctl -a /dev/tape1\n"
         "             (Prints all information for SCSI tape on Tape 1)\n"
         "  smartctl -A /dev/hdb,3\n"
         "                (Prints Attributes for physical drive 3 on 3ware 9000 RAID)\n"
         "  smartctl -A /dev/tw_cli/c0/p1\n"
         "            (Prints Attributes for 3ware controller 0, port 1 using tw_cli)\n"
         "\n"
         "  ATA SMART access methods and ordering may be specified by modifiers\n"
         "  following the device name: /dev/hdX:[saicm], where\n"
         "  's': SMART_* IOCTLs,         'a': IOCTL_ATA_PASS_THROUGH,\n"
         "  'i': IOCTL_IDE_PASS_THROUGH, 'c': ATA via IOCTL_SCSI_PASS_THROUGH,\n"
         "  'f': IOCTL_STORAGE_*,        'm': IOCTL_SCSI_MINIPORT_*.\n"
      + strprintf(
         "  The default on this system is /dev/sdX:%s\n", ata_get_def_options()
        );
}


/////////////////////////////////////////////////////////////////////////////
// ATA Interface
/////////////////////////////////////////////////////////////////////////////

// SMART_* IOCTLs, also known as DFP_* (Disk Fault Protection)

#define FILE_READ_ACCESS       0x0001
#define FILE_WRITE_ACCESS      0x0002
#define METHOD_BUFFERED             0
#define CTL_CODE(DeviceType, Function, Method, Access) (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))

#define FILE_DEVICE_DISK       7
#define IOCTL_DISK_BASE        FILE_DEVICE_DISK

#define SMART_GET_VERSION \
  CTL_CODE(IOCTL_DISK_BASE, 0x0020, METHOD_BUFFERED, FILE_READ_ACCESS)

#define SMART_SEND_DRIVE_COMMAND \
  CTL_CODE(IOCTL_DISK_BASE, 0x0021, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define SMART_RCV_DRIVE_DATA \
  CTL_CODE(IOCTL_DISK_BASE, 0x0022, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

ASSERT_CONST(SMART_GET_VERSION       , 0x074080);
ASSERT_CONST(SMART_SEND_DRIVE_COMMAND, 0x07c084);
ASSERT_CONST(SMART_RCV_DRIVE_DATA    , 0x07c088);

#define SMART_CYL_LOW  0x4F
#define SMART_CYL_HI   0xC2


#pragma pack(1)

typedef struct _GETVERSIONOUTPARAMS {
  UCHAR  bVersion;
  UCHAR  bRevision;
  UCHAR  bReserved;
  UCHAR  bIDEDeviceMap;
  ULONG  fCapabilities;
  ULONG  dwReserved[4];
} GETVERSIONOUTPARAMS, *PGETVERSIONOUTPARAMS, *LPGETVERSIONOUTPARAMS;

ASSERT_SIZEOF(GETVERSIONOUTPARAMS, 24);


#define SMART_VENDOR_3WARE      0x13C1  // identifies 3ware specific parameters

typedef struct _GETVERSIONINPARAMS_EX {
  BYTE    bVersion;
  BYTE    bRevision;
  BYTE    bReserved;
  BYTE    bIDEDeviceMap;
  DWORD   fCapabilities;
  DWORD   dwDeviceMapEx;  // 3ware specific: RAID drive bit map
  WORD    wIdentifier;    // Vendor specific identifier
  WORD    wControllerId;  // 3ware specific: Controller ID (0,1,...)
  ULONG   dwReserved[2];
} GETVERSIONINPARAMS_EX, *PGETVERSIONINPARAMS_EX, *LPGETVERSIONINPARAMS_EX;

ASSERT_SIZEOF(GETVERSIONINPARAMS_EX, sizeof(GETVERSIONOUTPARAMS));


typedef struct _IDEREGS {
  UCHAR  bFeaturesReg;
  UCHAR  bSectorCountReg;
  UCHAR  bSectorNumberReg;
  UCHAR  bCylLowReg;
  UCHAR  bCylHighReg;
  UCHAR  bDriveHeadReg;
  UCHAR  bCommandReg;
  UCHAR  bReserved;
} IDEREGS, *PIDEREGS, *LPIDEREGS;

typedef struct _SENDCMDINPARAMS {
  ULONG  cBufferSize;
  IDEREGS  irDriveRegs;
  UCHAR  bDriveNumber;
  UCHAR  bReserved[3];
  ULONG  dwReserved[4];
  UCHAR  bBuffer[1];
} SENDCMDINPARAMS, *PSENDCMDINPARAMS, *LPSENDCMDINPARAMS;

ASSERT_SIZEOF(SENDCMDINPARAMS, 32+1);

typedef struct _SENDCMDINPARAMS_EX {
  DWORD   cBufferSize;
  IDEREGS irDriveRegs;
  BYTE    bDriveNumber;
  BYTE    bPortNumber;   // 3ware specific: port number
  WORD    wIdentifier;   // Vendor specific identifier
  DWORD   dwReserved[4];
  BYTE    bBuffer[1];
} SENDCMDINPARAMS_EX, *PSENDCMDINPARAMS_EX, *LPSENDCMDINPARAMS_EX;

ASSERT_SIZEOF(SENDCMDINPARAMS_EX, sizeof(SENDCMDINPARAMS));


/* DRIVERSTATUS.bDriverError constants (just for info, not used)
#define SMART_NO_ERROR                    0
#define SMART_IDE_ERROR                   1
#define SMART_INVALID_FLAG                2
#define SMART_INVALID_COMMAND             3
#define SMART_INVALID_BUFFER              4
#define SMART_INVALID_DRIVE               5
#define SMART_INVALID_IOCTL               6
#define SMART_ERROR_NO_MEM                7
#define SMART_INVALID_REGISTER            8
#define SMART_NOT_SUPPORTED               9
#define SMART_NO_IDE_DEVICE               10
*/

typedef struct _DRIVERSTATUS {
  UCHAR  bDriverError;
  UCHAR  bIDEError;
  UCHAR  bReserved[2];
  ULONG  dwReserved[2];
} DRIVERSTATUS, *PDRIVERSTATUS, *LPDRIVERSTATUS;

typedef struct _SENDCMDOUTPARAMS {
  ULONG  cBufferSize;
  DRIVERSTATUS  DriverStatus;
  UCHAR  bBuffer[1];
} SENDCMDOUTPARAMS, *PSENDCMDOUTPARAMS, *LPSENDCMDOUTPARAMS;

ASSERT_SIZEOF(SENDCMDOUTPARAMS, 16+1);

#pragma pack()


/////////////////////////////////////////////////////////////////////////////

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
  GETVERSIONOUTPARAMS vers; memset(&vers, 0, sizeof(vers));
  const GETVERSIONINPARAMS_EX & vers_ex = (const GETVERSIONINPARAMS_EX &)vers;
  DWORD num_out;

  if (!DeviceIoControl(hdevice, SMART_GET_VERSION,
    NULL, 0, &vers, sizeof(vers), &num_out, NULL)) {
    if (con->reportataioctl)
      pout("  SMART_GET_VERSION failed, Error=%ld\n", GetLastError());
    errno = ENOSYS;
    return -1;
  }
  assert(num_out == sizeof(GETVERSIONOUTPARAMS));

  if (con->reportataioctl > 1) {
    pout("  SMART_GET_VERSION suceeded, bytes returned: %lu\n"
         "    Vers = %d.%d, Caps = 0x%lx, DeviceMap = 0x%02x\n",
      num_out, vers.bVersion, vers.bRevision,
      vers.fCapabilities, vers.bIDEDeviceMap);
    if (vers_ex.wIdentifier == SMART_VENDOR_3WARE)
      pout("    Identifier = %04x(3WARE), ControllerId=%u, DeviceMapEx = 0x%08lx\n",
      vers_ex.wIdentifier, vers_ex.wControllerId, vers_ex.dwDeviceMapEx);
  }

  if (ata_version_ex)
    *ata_version_ex = vers_ex;

  // TODO: Check vers.fCapabilities here?
  return vers.bIDEDeviceMap;
}


// call SMART_* ioctl

static int smart_ioctl(HANDLE hdevice, int drive, IDEREGS * regs, char * data, unsigned datasize, int port)
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
  // drive is set to 0-3 on Win9x only
  inpar.irDriveRegs.bDriveHeadReg = 0xA0 | ((drive & 1) << 4);
  inpar.bDriveNumber = drive;

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
    // CAUTION: DO NOT change "regs" Parameter in this case, see ata_command_interface()
    long err = GetLastError();
    if (con->reportataioctl && (err != ERROR_INVALID_PARAMETER || con->reportataioctl > 1)) {
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
    if (con->reportataioctl) {
      pout("  %s failed, DriverError=0x%02x, IDEError=0x%02x\n", name,
        outpar->DriverStatus.bDriverError, outpar->DriverStatus.bIDEError);
      print_ide_regs_io(regs, NULL);
    }
    errno = (!outpar->DriverStatus.bIDEError ? ENOSYS : EIO);
    return -1;
  }

  if (con->reportataioctl > 1) {
    pout("  %s suceeded, bytes returned: %lu (buffer %lu)\n", name,
      num_out, outpar->cBufferSize);
    print_ide_regs_io(regs, (regs->bFeaturesReg == ATA_SMART_STATUS ?
      (const IDEREGS *)(outpar->bBuffer) : NULL));
  }

  if (datasize)
    memcpy(data, outpar->bBuffer, 512);
  else if (regs->bFeaturesReg == ATA_SMART_STATUS) {
    if (nonempty(outpar->bBuffer, sizeof(IDEREGS)))
      *regs = *(const IDEREGS *)(outpar->bBuffer);
    else {  // Workaround for driver not returning regs
      if (con->reportataioctl)
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

#define FILE_DEVICE_CONTROLLER  4
#define IOCTL_SCSI_BASE         FILE_DEVICE_CONTROLLER

#define IOCTL_IDE_PASS_THROUGH \
  CTL_CODE(IOCTL_SCSI_BASE, 0x040A, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

ASSERT_CONST(IOCTL_IDE_PASS_THROUGH, 0x04d028);

#pragma pack(1)

typedef struct {
  IDEREGS IdeReg;
  ULONG   DataBufferSize;
  UCHAR   DataBuffer[1];
} ATA_PASS_THROUGH;

ASSERT_SIZEOF(ATA_PASS_THROUGH, 12+1);

#pragma pack()


/////////////////////////////////////////////////////////////////////////////

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
    if (con->reportataioctl) {
      pout("  IOCTL_IDE_PASS_THROUGH failed, Error=%ld\n", err);
      print_ide_regs_io(regs, NULL);
    }
    VirtualFree(buf, 0, MEM_RELEASE);
    errno = (err == ERROR_INVALID_FUNCTION || err == ERROR_NOT_SUPPORTED ? ENOSYS : EIO);
    return -1;
  }

  // Check ATA status
  if (buf->IdeReg.bCommandReg/*Status*/ & 0x01) {
    if (con->reportataioctl) {
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
      if (con->reportataioctl) {
        pout("  IOCTL_IDE_PASS_THROUGH output data missing (%lu, %lu)\n",
          num_out, buf->DataBufferSize);
        print_ide_regs_io(regs, &buf->IdeReg);
      }
      VirtualFree(buf, 0, MEM_RELEASE);
      errno = EIO;
      return -1;
    }
    memcpy(data, buf->DataBuffer, datasize);
  }

  if (con->reportataioctl > 1) {
    pout("  IOCTL_IDE_PASS_THROUGH suceeded, bytes returned: %lu (buffer %lu)\n",
      num_out, buf->DataBufferSize);
    print_ide_regs_io(regs, &buf->IdeReg);
  }
  *regs = buf->IdeReg;

  // Caution: VirtualFree() fails if parameter "dwSize" is nonzero
  VirtualFree(buf, 0, MEM_RELEASE);
  return 0;
}


/////////////////////////////////////////////////////////////////////////////

// ATA PASS THROUGH (Win2003, XP SP2)

#define IOCTL_ATA_PASS_THROUGH \
  CTL_CODE(IOCTL_SCSI_BASE, 0x040B, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

ASSERT_CONST(IOCTL_ATA_PASS_THROUGH, 0x04d02c);

typedef struct _ATA_PASS_THROUGH_EX {
  USHORT  Length;
  USHORT  AtaFlags;
  UCHAR  PathId;
  UCHAR  TargetId;
  UCHAR  Lun;
  UCHAR  ReservedAsUchar;
  ULONG  DataTransferLength;
  ULONG  TimeOutValue;
  ULONG  ReservedAsUlong;
  ULONG/*_PTR*/ DataBufferOffset;
  UCHAR  PreviousTaskFile[8];
  UCHAR  CurrentTaskFile[8];
} ATA_PASS_THROUGH_EX, *PATA_PASS_THROUGH_EX;

ASSERT_SIZEOF(ATA_PASS_THROUGH_EX, 40);

#define ATA_FLAGS_DRDY_REQUIRED 0x01
#define ATA_FLAGS_DATA_IN       0x02
#define ATA_FLAGS_DATA_OUT      0x04
#define ATA_FLAGS_48BIT_COMMAND 0x08
#define ATA_FLAGS_USE_DMA       0x10
#define ATA_FLAGS_NO_MULTIPLE   0x20 // Vista


/////////////////////////////////////////////////////////////////////////////

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
    if (con->reportataioctl) {
      pout("  IOCTL_ATA_PASS_THROUGH failed, Error=%ld\n", err);
      print_ide_regs_io(regs, NULL);
    }
    errno = (err == ERROR_INVALID_FUNCTION || err == ERROR_NOT_SUPPORTED ? ENOSYS : EIO);
    return -1;
  }

  // Check ATA status
  if (ctfregs->bCommandReg/*Status*/ & (0x01/*Err*/|0x08/*DRQ*/)) {
    if (con->reportataioctl) {
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
      if (con->reportataioctl) {
        pout("  IOCTL_ATA_PASS_THROUGH output data missing (%lu)\n", num_out);
        print_ide_regs_io(regs, ctfregs);
      }
      errno = EIO;
      return -1;
    }
    memcpy(data, ab.ucDataBuf, datasize);
  }

  if (con->reportataioctl > 1) {
    pout("  IOCTL_ATA_PASS_THROUGH suceeded, bytes returned: %lu\n", num_out);
    print_ide_regs_io(regs, ctfregs);
  }
  *regs = *ctfregs;
  if (prev_regs)
    *prev_regs = *ptfregs;

  return 0;
}


/////////////////////////////////////////////////////////////////////////////

// ATA PASS THROUGH via SCSI PASS THROUGH (WinNT4 only)

#define IOCTL_SCSI_PASS_THROUGH \
  CTL_CODE(IOCTL_SCSI_BASE, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

ASSERT_CONST(IOCTL_SCSI_PASS_THROUGH, 0x04d004);

#define SCSI_IOCTL_DATA_OUT          0
#define SCSI_IOCTL_DATA_IN           1
#define SCSI_IOCTL_DATA_UNSPECIFIED  2
// undocumented SCSI opcode to for ATA passthrough
#define SCSIOP_ATA_PASSTHROUGH    0xCC

typedef struct _SCSI_PASS_THROUGH {
  USHORT  Length;
  UCHAR  ScsiStatus;
  UCHAR  PathId;
  UCHAR  TargetId;
  UCHAR  Lun;
  UCHAR  CdbLength;
  UCHAR  SenseInfoLength;
  UCHAR  DataIn;
  ULONG  DataTransferLength;
  ULONG  TimeOutValue;
  ULONG/*_PTR*/ DataBufferOffset;
  ULONG  SenseInfoOffset;
  UCHAR  Cdb[16];
} SCSI_PASS_THROUGH, *PSCSI_PASS_THROUGH;

ASSERT_SIZEOF(SCSI_PASS_THROUGH, 44);


/////////////////////////////////////////////////////////////////////////////

static int ata_via_scsi_pass_through_ioctl(HANDLE hdevice, IDEREGS * regs, char * data, unsigned datasize)
{
  typedef struct {
    SCSI_PASS_THROUGH spt;
    ULONG Filler;
    UCHAR ucSenseBuf[32];
    UCHAR ucDataBuf[512];
  } SCSI_PASS_THROUGH_WITH_BUFFERS;

  SCSI_PASS_THROUGH_WITH_BUFFERS sb;
  IDEREGS * cdbregs;
  unsigned int size;
  DWORD num_out;
  const unsigned char magic = 0xcf;

  memset(&sb, 0, sizeof(sb));
  sb.spt.Length = sizeof(SCSI_PASS_THROUGH);
  //sb.spt.PathId = 0;
  sb.spt.TargetId = 1;
  //sb.spt.Lun = 0;
  sb.spt.CdbLength = 10; sb.spt.SenseInfoLength = 24;
  sb.spt.TimeOutValue = 10;
  sb.spt.SenseInfoOffset = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucSenseBuf);
  size = offsetof(SCSI_PASS_THROUGH_WITH_BUFFERS, ucDataBuf);
  sb.spt.DataBufferOffset = size;

  if (datasize) {
    if (datasize > sizeof(sb.ucDataBuf)) {
      errno = EINVAL;
      return -1;
    }
    sb.spt.DataIn = SCSI_IOCTL_DATA_IN;
    sb.spt.DataTransferLength = datasize;
    size += datasize;
    sb.ucDataBuf[0] = magic;
  }
  else {
    sb.spt.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
    //sb.spt.DataTransferLength = 0;
  }

  // Use pseudo SCSI command followed by registers
  sb.spt.Cdb[0] = SCSIOP_ATA_PASSTHROUGH;
  cdbregs = (IDEREGS *)(sb.spt.Cdb+2);
  *cdbregs = *regs;

  if (!DeviceIoControl(hdevice, IOCTL_SCSI_PASS_THROUGH,
    &sb, size, &sb, size, &num_out, NULL)) {
    long err = GetLastError();
    if (con->reportataioctl)
      pout("  ATA via IOCTL_SCSI_PASS_THROUGH failed, Error=%ld\n", err);
    errno = (err == ERROR_INVALID_FUNCTION || err == ERROR_NOT_SUPPORTED ? ENOSYS : EIO);
    return -1;
  }

  // Cannot check ATA status, because command does not return IDEREGS

  // Check and copy data
  if (datasize) {
    if (   num_out != size
        || (sb.ucDataBuf[0] == magic && !nonempty(sb.ucDataBuf+1, datasize-1))) {
      if (con->reportataioctl) {
        pout("  ATA via IOCTL_SCSI_PASS_THROUGH output data missing (%lu)\n", num_out);
        print_ide_regs_io(regs, NULL);
      }
      errno = EIO;
      return -1;
    }
    memcpy(data, sb.ucDataBuf, datasize);
  }

  if (con->reportataioctl > 1) {
    pout("  ATA via IOCTL_SCSI_PASS_THROUGH suceeded, bytes returned: %lu\n", num_out);
    print_ide_regs_io(regs, NULL);
  }
  return 0;
}


/////////////////////////////////////////////////////////////////////////////

// SMART IOCTL via SCSI MINIPORT ioctl

// This function is handled by ATAPI port driver (atapi.sys) or by SCSI
// miniport driver (via SCSI port driver scsiport.sys).
// It can be used to skip the missing or broken handling of some SMART
// command codes (e.g. READ_LOG) in the disk class driver (disk.sys)

#define IOCTL_SCSI_MINIPORT \
  CTL_CODE(IOCTL_SCSI_BASE, 0x0402, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

ASSERT_CONST(IOCTL_SCSI_MINIPORT, 0x04d008);

typedef struct _SRB_IO_CONTROL {
  ULONG HeaderLength;
  UCHAR Signature[8];
  ULONG Timeout;
  ULONG ControlCode;
  ULONG ReturnCode;
  ULONG Length;
} SRB_IO_CONTROL, *PSRB_IO_CONTROL;

ASSERT_SIZEOF(SRB_IO_CONTROL, 28);

#define FILE_DEVICE_SCSI 0x001b

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

/////////////////////////////////////////////////////////////////////////////

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
    if (con->reportataioctl) {
      pout("  IOCTL_SCSI_MINIPORT_%s failed, Error=%ld\n", name, err);
      print_ide_regs_io(regs, NULL);
    }
    errno = (err == ERROR_INVALID_FUNCTION || err == ERROR_NOT_SUPPORTED ? ENOSYS : EIO);
    return -1;
  }

  // Check result
  if (sb.srbc.ReturnCode) {
    if (con->reportataioctl) {
      pout("  IOCTL_SCSI_MINIPORT_%s failed, ReturnCode=0x%08lx\n", name, sb.srbc.ReturnCode);
      print_ide_regs_io(regs, NULL);
    }
    errno = EIO;
    return -1;
  }

  if (sb.params.out.DriverStatus.bDriverError) {
    if (con->reportataioctl) {
      pout("  IOCTL_SCSI_MINIPORT_%s failed, DriverError=0x%02x, IDEError=0x%02x\n", name,
        sb.params.out.DriverStatus.bDriverError, sb.params.out.DriverStatus.bIDEError);
      print_ide_regs_io(regs, NULL);
    }
    errno = (!sb.params.out.DriverStatus.bIDEError ? ENOSYS : EIO);
    return -1;
  }

  if (con->reportataioctl > 1) {
    pout("  IOCTL_SCSI_MINIPORT_%s suceeded, bytes returned: %lu (buffer %lu)\n", name,
      num_out, sb.params.out.cBufferSize);
    print_ide_regs_io(regs, (code == IOCTL_SCSI_MINIPORT_RETURN_STATUS ?
                             (const IDEREGS *)(sb.params.out.bBuffer) : 0));
  }

  if (datasize > 0)
    memcpy(data, sb.params.out.bBuffer, datasize);
  else if (datasize == 0 && code == IOCTL_SCSI_MINIPORT_RETURN_STATUS)
    *regs = *(const IDEREGS *)(sb.params.out.bBuffer);

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
  strcpy((char *)sb.srbc.Signature, "<3ware>");
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
    if (con->reportataioctl) {
      pout("  ATA via IOCTL_SCSI_MINIPORT failed, Error=%ld\n", err);
      print_ide_regs_io(regs, NULL);
    }
    errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
    return -1;
  }

  if (sb.srbc.ReturnCode) {
    if (con->reportataioctl) {
      pout("  ATA via IOCTL_SCSI_MINIPORT failed, ReturnCode=0x%08lx\n", sb.srbc.ReturnCode);
      print_ide_regs_io(regs, NULL);
    }
    errno = EIO;
    return -1;
  }

  // Copy data
  if (datasize > 0)
    memcpy(data, sb.buffer, datasize);

  if (con->reportataioctl > 1) {
    pout("  ATA via IOCTL_SCSI_MINIPORT suceeded, bytes returned: %lu\n", num_out);
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
  strcpy((char *)srbc.Signature, "<3ware>");
  srbc.HeaderLength = sizeof(SRB_IO_CONTROL);
  srbc.Timeout = 60; // seconds
  srbc.ControlCode = 0xCC010014;
  srbc.ReturnCode = 0;
  srbc.Length = 0;

  DWORD num_out;
  if (!DeviceIoControl(hdevice, IOCTL_SCSI_MINIPORT,
    &srbc, sizeof(srbc), &srbc, sizeof(srbc), &num_out, NULL)) {
    long err = GetLastError();
    if (con->reportataioctl)
      pout("  UPDATE DEVICEMAP via IOCTL_SCSI_MINIPORT failed, Error=%ld\n", err);
    errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
    return -1;
  }
  if (srbc.ReturnCode) {
    if (con->reportataioctl)
      pout("  UPDATE DEVICEMAP via IOCTL_SCSI_MINIPORT failed, ReturnCode=0x%08lx\n", srbc.ReturnCode);
    errno = EIO;
    return -1;
  }
  if (con->reportataioctl > 1)
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
    if (con->reportataioctl > 1)
      pout("%s: Run: \"%s\"\n", name, cmd);
    size = run_cmd(cmd, buffer, sizeof(buffer));
  }
  else {
    return set_err(EINVAL);
  }

  if (con->reportataioctl > 1)
    pout("%s: Read %d bytes\n", name, size);
  if (size <= 0)
    return set_err(ENOENT);
  if (size >= (int)sizeof(buffer))
    return set_err(EIO);

  buffer[size] = 0;
  if (con->reportataioctl > 1)
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
        return set_err(EIO, err);
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
    case READ_THRESHOLDS:
      if (!m_smart_valid)
        break;
      // Fake zero thresholds
      {
        const ata_smart_values   * sv = &m_smart_buf;
        ata_smart_thresholds_pvt * tr = (ata_smart_thresholds_pvt *)data;
        memset(tr, 0, 512);
        // TODO: Indicate missing thresholds in ataprint.cpp:PrintSmartAttribWithThres()
        // (ATA_SMART_READ_THRESHOLDS is marked obsolete since ATA-5)
        for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++)
          tr->chksum -= tr->thres_entries[i].id = sv->vendor_attributes[i].id;
      }
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

#define FILE_DEVICE_MASS_STORAGE    0x0000002d
#define IOCTL_STORAGE_BASE          FILE_DEVICE_MASS_STORAGE
#define FILE_ANY_ACCESS             0

#define IOCTL_STORAGE_QUERY_PROPERTY \
  CTL_CODE(IOCTL_STORAGE_BASE, 0x0500, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef enum _STORAGE_BUS_TYPE {
  BusTypeUnknown      = 0x00,
  BusTypeScsi         = 0x01,
  BusTypeAtapi        = 0x02,
  BusTypeAta          = 0x03,
  BusType1394         = 0x04,
  BusTypeSsa          = 0x05,
  BusTypeFibre        = 0x06,
  BusTypeUsb          = 0x07,
  BusTypeRAID         = 0x08,
  BusTypeiScsi        = 0x09,
  BusTypeSas          = 0x0A,
  BusTypeSata         = 0x0B,
  BusTypeSd           = 0x0C,
  BusTypeMmc          = 0x0D,
  BusTypeMax          = 0x0E,
  BusTypeMaxReserved  = 0x7F
} STORAGE_BUS_TYPE, *PSTORAGE_BUS_TYPE;

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
} STORAGE_DEVICE_DESCRIPTOR, *PSTORAGE_DEVICE_DESCRIPTOR;

typedef enum _STORAGE_QUERY_TYPE {
  PropertyStandardQuery = 0,
  PropertyExistsQuery,
  PropertyMaskQuery,
  PropertyQueryMaxDefined
} STORAGE_QUERY_TYPE, *PSTORAGE_QUERY_TYPE;

typedef enum _STORAGE_PROPERTY_ID {
  StorageDeviceProperty = 0,
  StorageAdapterProperty,
  StorageDeviceIdProperty,
  StorageDeviceUniqueIdProperty,
  StorageDeviceWriteCacheProperty,
  StorageMiniportProperty,
  StorageAccessAlignmentProperty
} STORAGE_PROPERTY_ID, *PSTORAGE_PROPERTY_ID;

typedef struct _STORAGE_PROPERTY_QUERY {
  STORAGE_PROPERTY_ID PropertyId;
  STORAGE_QUERY_TYPE QueryType;
  UCHAR AdditionalParameters[1];
} STORAGE_PROPERTY_QUERY, *PSTORAGE_PROPERTY_QUERY;


/////////////////////////////////////////////////////////////////////////////

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
    if (con->reportataioctl > 1 || con->reportscsiioctl > 1)
      pout("  IOCTL_STORAGE_QUERY_PROPERTY failed, Error=%ld\n", GetLastError());
    errno = ENOSYS;
    return -1;
  }

  if (con->reportataioctl > 1 || con->reportscsiioctl > 1) {
    pout("  IOCTL_STORAGE_QUERY_PROPERTY returns:\n"
         "    Vendor:   \"%s\"\n"
         "    Product:  \"%s\"\n"
         "    Revision: \"%s\"\n"
         "    Removable: %s\n"
         "    BusType:   0x%02x\n",
         (data->desc.VendorIdOffset        ? data->raw+data->desc.VendorIdOffset : ""),
         (data->desc.ProductIdOffset       ? data->raw+data->desc.ProductIdOffset : ""),
         (data->desc.ProductRevisionOffset ? data->raw+data->desc.ProductRevisionOffset : ""),
         (data->desc.RemovableMedia? "Yes":"No"), data->desc.BusType
    );
  }
  return 0;
}


/////////////////////////////////////////////////////////////////////////////

// IOCTL_STORAGE_PREDICT_FAILURE

#define IOCTL_STORAGE_PREDICT_FAILURE \
  CTL_CODE(IOCTL_STORAGE_BASE, 0x0440, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _STORAGE_PREDICT_FAILURE {
  ULONG  PredictFailure;
  UCHAR  VendorSpecific[512];
} STORAGE_PREDICT_FAILURE, *PSTORAGE_PREDICT_FAILURE;

ASSERT_SIZEOF(STORAGE_PREDICT_FAILURE, 4+512);


/////////////////////////////////////////////////////////////////////////////


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
    if (con->reportataioctl > 1)
      pout("  IOCTL_STORAGE_PREDICT_FAILURE failed, Error=%ld\n", GetLastError());
    errno = ENOSYS;
    return -1;
  }

  if (con->reportataioctl > 1) {
    pout("  IOCTL_STORAGE_PREDICT_FAILURE returns:\n"
         "    PredictFailure: 0x%08lx\n"
         "    VendorSpecific: 0x%02x,0x%02x,0x%02x,...,0x%02x\n",
         pred.PredictFailure,
         pred.VendorSpecific[0], pred.VendorSpecific[1], pred.VendorSpecific[2],
         pred.VendorSpecific[sizeof(pred.VendorSpecific)-1]
    );
  }
  if (data)
    memcpy(data, pred.VendorSpecific, sizeof(pred.VendorSpecific));
  return (!pred.PredictFailure ? 0 : 1);
}


/////////////////////////////////////////////////////////////////////////////

// get DEV_* for open handle
static win_dev_type get_controller_type(HANDLE hdevice, bool admin, GETVERSIONINPARAMS_EX * ata_version_ex)
{
  // Try SMART_GET_VERSION first to detect ATA SMART support
  // for drivers reporting BusTypeScsi (3ware)
  if (admin && smart_get_version(hdevice, ata_version_ex) >= 0)
    return DEV_ATA;

  // Get BusType from device descriptor
  STORAGE_DEVICE_DESCRIPTOR_DATA data;
  if (storage_query_property_ioctl(hdevice, &data))
    return DEV_UNKNOWN;

  switch (data.desc.BusType) {
    case BusTypeAta:
    case BusTypeSata:
      if (ata_version_ex)
        memset(ata_version_ex, 0, sizeof(*ata_version_ex));
      return DEV_ATA;
    case BusTypeScsi:
    case BusTypeiScsi:
    case BusTypeSas:
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
  if (con->reportataioctl > 1 || con->reportscsiioctl > 1)
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
  if (data.desc.ProductIdOffset)
    copy_swapped(id->model, data.raw+data.desc.ProductIdOffset, sizeof(id->model));
  if (data.desc.ProductRevisionOffset)
    copy_swapped(id->fw_rev, data.raw+data.desc.ProductRevisionOffset, sizeof(id->fw_rev));
  id->command_set_1 = 0x0001; id->command_set_2 = 0x4000; // SMART supported, words 82,83 valid
  id->cfs_enable_1  = 0x0001; id->csf_default   = 0x4000; // SMART enabled, words 85,87 valid
  return 0;
}


/////////////////////////////////////////////////////////////////////////////
// USB ID detection using WMI

// Run a command, split stdout into lines.
// Return number of lines read, -1 on error.
static int run_cmd(std::vector<std::string> & lines, const char * cmd, ...)
{
  lines.clear();

  va_list ap; va_start(ap, cmd);
  std::string cmdline = vstrprintf(cmd, ap);
  va_end(ap);

  if (con->reportscsiioctl > 1)
    pout("Run: \"%s\"\n", cmdline.c_str());

  char buffer[16*1024];
  int size = run_cmd(cmdline.c_str(), buffer, sizeof(buffer));

  if (con->reportscsiioctl > 1)
    pout("Read %d bytes\n", size);
  if (!(0 < size && size < (int)sizeof(buffer)-1))
    return -1;

  buffer[size] = 0;

  for (int i = 0; buffer[i]; ) {
      int len = strcspn(buffer+i, "\r\n");
      lines.push_back(std::string(buffer+i, len));
      i += len;
      i += strspn(buffer+i, "\r\n");
  }
  if (con->reportscsiioctl > 1) {
    for (unsigned i = 0; i < lines.size(); i++)
      printf("'%s'\n", lines[i].c_str());
  }
  return lines.size();
}

// Quote string for WMI
static std::string wmi_quote(const char * s, int len)
{
  std::string r;
  for (int i = 0; i < len; i++) {
    char c = s[i];
    if (c == '\\')
      r += '\\';
    r += c;
  }
  return r;
}

// Get USB ID for a physical drive number
static bool get_usb_id(int drive, unsigned short & vendor_id, unsigned short & product_id)
{
  // Get device name
  std::vector<std::string> result;
  if (run_cmd(result,
        "wmic PATH Win32_DiskDrive WHERE DeviceID=\"\\\\\\\\.\\\\PHYSICALDRIVE%d\" GET Model",
        drive) != 2)
    return false;

  std::string name = result[1];

  // Get USB_CONTROLLER -> DEVICE associations
  std::vector<std::string> assoc;
  int n = run_cmd(assoc, "wmic PATH Win32_USBControllerDevice GET Antecedent,Dependent");
  if (n < 2)
    return false;

  regular_expression regex("^([^ ]+) .*Win32_PnPEntity.DeviceID=\"(USBSTOR\\\\[^\"]*)\" *$",
                           REG_EXTENDED);
  if (regex.empty()) // TODO: throw in constructor?
    return false;

  int usbstoridx = -1;
  std::string usbcontr;
  for (int i = 2; i < n; i++) {
    // Find next 'USB_CONTROLLER  USBSTORAGE_DEVICE' pair
    regmatch_t match[3];
    const char * s = assoc[i].c_str();
    if (!regex.execute(s, 3, match))
      continue;

    // USBSTOR device found, compare Name
    if (run_cmd(result,
          "wmic PATH Win32_PnPEntity WHERE DeviceID=\"%s\" GET Name",
          wmi_quote(s + match[2].rm_so, match[2].rm_eo - match[2].rm_so).c_str()
          ) != 2)
      continue;
    if (result[1] != name)
      continue;

    // Name must be uniqe
    if (usbstoridx >= 0)
      return false;

    usbstoridx = i;
    usbcontr.assign(s + match[1].rm_so, match[1].rm_eo - match[1].rm_so);
  }

  // Found ?
  if (usbstoridx <= 0)
    return false;

  // The entry preceding USBSTOR should be the USB bridge device
  regex.compile("^([^ ]+) .*Win32_PnPEntity.DeviceID=\"USB\\\\VID_(....&PID_....)[^\"]*\" *$",
                REG_EXTENDED);
  if (regex.empty())
    return false;
  regmatch_t match[3];
  const char * s = assoc[usbstoridx-1].c_str();
  if (!regex.execute(s, 3, match))
    return false;

  // Both devices must be associated to same controller
  if (usbcontr != std::string(s + match[1].rm_so, match[1].rm_eo - match[1].rm_so))
    return false;

  // Parse USB ID
  int nc = -1;
  if (!(sscanf(s + match[2].rm_so, "%4hx&PID_%4hx%n",
               &vendor_id, &product_id, &nc) == 2 && nc == 4+5+4))
    return false;

  if (con->reportscsiioctl > 1)
    pout("USB ID = 0x%04x:0x%04x\n", vendor_id, product_id);
  return true;
}


/////////////////////////////////////////////////////////////////////////////

// Call GetDevicePowerState() if available (Win98/ME/2000/XP/2003)
// returns: 1=active, 0=standby, -1=error
// (This would also work for SCSI drives)

static int get_device_power_state(HANDLE hdevice)
{
  static HINSTANCE h_kernel_dll = 0;
#ifdef __CYGWIN__
  static DWORD kernel_dll_pid = 0;
#endif
  static BOOL (WINAPI * GetDevicePowerState_p)(HANDLE, BOOL *) = 0;

  BOOL state = TRUE;

  if (!GetDevicePowerState_p
#ifdef __CYGWIN__
      || kernel_dll_pid != GetCurrentProcessId() // detect fork()
#endif
     ) {
    if (h_kernel_dll == INVALID_HANDLE_VALUE) {
      errno = ENOSYS;
      return -1;
    }
    if (!(h_kernel_dll = LoadLibraryA("KERNEL32.DLL"))) {
      pout("Cannot load KERNEL32.DLL, Error=%ld\n", GetLastError());
      h_kernel_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
      errno = ENOSYS;
      return -1;
    }
    if (!(GetDevicePowerState_p = (BOOL (WINAPI *)(HANDLE, BOOL *))
                                  GetProcAddress(h_kernel_dll, "GetDevicePowerState"))) {
      if (con->reportataioctl)
        pout("  GetDevicePowerState() not found, Error=%ld\n", GetLastError());
      FreeLibrary(h_kernel_dll);
      h_kernel_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
      errno = ENOSYS;
      return -1;
    }
#ifdef __CYGWIN__
    kernel_dll_pid = GetCurrentProcessId();
#endif
  }

  if (!GetDevicePowerState_p(hdevice, &state)) {
    long err = GetLastError();
    if (con->reportataioctl)
      pout("  GetDevicePowerState() failed, Error=%ld\n", err);
    errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
    // TODO: This may not work as expected on transient errors,
    // because smartd interprets -1 as SLEEP mode regardless of errno.
    return -1;
  }

  if (con->reportataioctl > 1)
    pout("  GetDevicePowerState() succeeded, state=%d\n", state);
  return state;
}


/////////////////////////////////////////////////////////////////////////////

#if WIN9X_SUPPORT
// Print SMARTVSD error message, return errno

static int smartvsd_error()
{
  char path[MAX_PATH];
  unsigned len;
  if (!(5 <= (len = GetSystemDirectoryA(path, MAX_PATH)) && len < MAX_PATH/2))
    return ENOENT;
  // SMARTVSD.VXD present?
  strcpy(path+len, "\\IOSUBSYS\\SMARTVSD.VXD");
  if (!access(path, 0)) {
    // Yes, standard IDE driver used?
    HANDLE h;
    if (   (h = CreateFileA("\\\\.\\ESDI_506",
                 GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
                 NULL, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE
        && GetLastError() == ERROR_FILE_NOT_FOUND                             ) {
      pout("Standard IDE driver ESDI_506.PDR not used, or no IDE/ATA drives present.\n");
      return ENOENT;
    }
    else {
      if (h != INVALID_HANDLE_VALUE) // should not happen
        CloseHandle(h);
      pout("SMART driver SMARTVSD.VXD is installed, but not loaded.\n");
      return ENOSYS;
    }
  }
  else {
    strcpy(path+len, "\\SMARTVSD.VXD");
    if (!access(path, 0)) {
      // Some Windows versions install SMARTVSD.VXD in SYSTEM directory
      // (http://support.microsoft.com/kb/265854/en-us).
      path[len] = 0;
      pout("SMART driver is not properly installed,\n"
         " move SMARTVSD.VXD from \"%s\" to \"%s\\IOSUBSYS\"\n"
         " and reboot Windows.\n", path, path);
    }
    else {
      // Some Windows versions do not provide SMARTVSD.VXD
      // (http://support.microsoft.com/kb/199886/en-us).
      path[len] = 0;
      pout("SMARTVSD.VXD is missing in folder \"%s\\IOSUBSYS\".\n", path);
    }
    return ENOSYS;
  }
}

#endif // WIN9X_SUPPORT

// Get default ATA device options

static const char * ata_get_def_options()
{
  DWORD ver = GetVersion();
  if ((ver & 0x80000000) || (ver & 0xff) < 4) // Win9x/ME
    return "s"; // SMART_* only
  else if ((ver & 0xff) == 4) // WinNT4
    return "sc"; // SMART_*, SCSI_PASS_THROUGH
  else // WinXP, 2003, Vista
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
  m_id_is_cached(false),
  m_drive(0),
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
  // [sh]d[a-z](:[saicmfp]+)? => Physical drive 0-25, with options
  char drive[1+1] = "", options[8+1] = ""; int n1 = -1, n2 = -1;
  if (   sscanf(name, "%*[sh]d%1[a-z]%n:%7[saicmfp]%n", drive, &n1, options, &n2) >= 1
      && ((n1 == len && !options[0]) || n2 == len)                                       ) {
    return open(drive[0] - 'a', -1, options, -1);
  }
  // [sh]d[a-z],N(:[saicmfp3]+)? => Physical drive 0-25, RAID port N, with options
  drive[0] = 0; options[0] = 0; n1 = -1; n2 = -1;
  unsigned port = ~0;
  if (   sscanf(name, "%*[sh]d%1[a-z],%u%n:%8[saicmfp3]%n", drive, &port, &n1, options, &n2) >= 2
      && port < 32 && ((n1 == len && !options[0]) || n2 == len)                                     ) {
    return open(drive[0] - 'a', -1, options, port);
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
  // path depends on Windows Version
  char devpath[30];
  if (win9x && 0 <= phydrive && phydrive <= 7)
    // Use patched "smartvse.vxd" for drives 4-7, see INSTALL file for details
    strcpy(devpath, (phydrive <= 3 ? "\\\\.\\SMARTVSD" : "\\\\.\\SMARTVSE"));
  else if (!win9x && 0 <= phydrive && phydrive <= 255)
    snprintf(devpath, sizeof(devpath)-1, "\\\\.\\PhysicalDrive%d", phydrive);
  else if (!win9x && 0 <= logdrive && logdrive <= 'Z'-'A')
    snprintf(devpath, sizeof(devpath)-1, "\\\\.\\%c:", 'A'+logdrive);
  else
    return set_err(ENOENT);

  // Open device
  HANDLE h = INVALID_HANDLE_VALUE;
  if (win9x || !(*options && !options[strspn(options, "fp")])) {
    // Open with admin rights
    m_admin = true;
    h = CreateFileA(devpath, GENERIC_READ|GENERIC_WRITE,
      FILE_SHARE_READ|FILE_SHARE_WRITE,
      NULL, OPEN_EXISTING, 0, 0);
  }
  if (!win9x && h == INVALID_HANDLE_VALUE) {
    // Open without admin rights
    m_admin = false;
    h = CreateFileA(devpath, 0,
      FILE_SHARE_READ|FILE_SHARE_WRITE,
      NULL, OPEN_EXISTING, 0, 0);
  }
  if (h == INVALID_HANDLE_VALUE) {
    long err = GetLastError();
#if WIN9X_SUPPORT
    if (win9x && phydrive <= 3 && err == ERROR_FILE_NOT_FOUND)
      smartvsd_error();
#endif
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

  if (con->reportataioctl > 1)
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

  // NT4/2000/XP: SMART_GET_VERSION may spin up disk, so delay until first real SMART_* call
  m_drive = 0; m_port = port;
  if (!win9x && port < 0)
    return true;

  // Win9X/ME: Get drive map
  // RAID: Get port map
  GETVERSIONINPARAMS_EX vers_ex;
  int devmap = smart_get_version(h, (port >= 0 ? &vers_ex : 0));

  unsigned long portmap = 0;
  if (port >= 0 && devmap >= 0) {
    // 3ware RAID: check vendor id
    if (vers_ex.wIdentifier != SMART_VENDOR_3WARE) {
      pout("SMART_GET_VERSION returns unknown Identifier = %04x\n"
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

  if (port >= 0) {
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
    return true;
  }

  // Win9x/ME: Check device presence & type
  if (((devmap >> (phydrive & 0x3)) & 0x11) != 0x01) {
    unsigned char atapi = (devmap >> (phydrive & 0x3)) & 0x10;
    // Win9x drive existence check may not work as expected
    // The atapi.sys driver incorrectly fills in the bIDEDeviceMap with 0x01
    // (The related KB Article Q196120 is no longer available)
    if (!is_permissive()) {
      close();
      return set_err((atapi ? ENOSYS : ENOENT), "%s: Drive %d %s (IDEDeviceMap=0x%02x)",
        devpath, phydrive, (atapi?"is an ATAPI device":"does not exist"), devmap);
    }
  }
  // Drive number must be passed to ioctl
  m_drive = (phydrive & 0x3);
  return true;
}


#if WIN9X_SUPPORT

// Scan for ATA drives on Win9x/ME

bool win9x_smart_interface::ata_scan(smart_device_list & devlist)
{
  // Open device
  const char devpath[] = "\\\\.\\SMARTVSD";
  HANDLE h = CreateFileA(devpath, GENERIC_READ|GENERIC_WRITE,
    FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
  if (h == INVALID_HANDLE_VALUE) {
    if (con->reportataioctl > 1)
      pout(" %s: Open failed, Error=%ld\n", devpath, GetLastError());
    return true; // SMARTVSD.VXD missing or no ATA devices
  }

  // Get drive map
  int devmap = smart_get_version(h);
  CloseHandle(h);
  if (devmap < 0)
    return true; // Should not happen

  // Check ATA device presence, remove ATAPI devices
  devmap = (devmap & 0xf) & ~((devmap >> 4) & 0xf);
  char name[20];
  for (int i = 0; i < 4; i++) {
    if (!(devmap & (1 << i)))
      continue;
    sprintf(name, "/dev/hd%c", 'a'+i);
    devlist.push_back( new win_ata_device(this, name, "ata") );
  }
  return true;
}

#endif // WIN9X_SUPPORT


// Scan for ATA drives

bool winnt_smart_interface::ata_scan(smart_device_list & devlist)
{
  const int max_raid = 2;
  bool raid_seen[max_raid] = {false, false};

  char name[20];
  for (int i = 0; i <= 9; i++) {
    GETVERSIONINPARAMS_EX vers_ex;
    if (get_phy_drive_type(i, &vers_ex) != DEV_ATA)
      continue;

    // Interpret RAID drive map if present
    if (vers_ex.wIdentifier == SMART_VENDOR_3WARE) {
      // Skip if more than 2 controllers or logical drive from this controller already seen
      if (vers_ex.wControllerId >= max_raid || raid_seen[vers_ex.wControllerId])
        continue;
      raid_seen[vers_ex.wControllerId] = true;
      // Add physical drives
      for (int pi = 0; pi < 32; pi++) {
        if (vers_ex.dwDeviceMapEx & (1L << pi)) {
            sprintf(name, "/dev/sd%c,%u", 'a'+i, pi);
            devlist.push_back( new win_ata_device(this, name, "ata") );
        }
      }
      continue;
    }

    // Driver supports SMART_GET_VERSION or STORAGE_QUERY_PROPERTY returns ATA/SATA
    sprintf(name, "/dev/sd%c", 'a'+i);
    devlist.push_back( new win_ata_device(this, name, "ata") );
  }

  return true;
}


/////////////////////////////////////////////////////////////////////////////

// Interface to ATA devices
bool win_ata_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  // No multi-sector support for now, see above
  // warning about IOCTL_ATA_PASS_THROUGH
  if (!ata_cmd_is_ok(in,
    true, // data_out_support
    false, // !multi_sector_support
    true) // ata_48bit_support
  )
    return false;

  // Determine ioctl functions valid for this ATA cmd
  const char * valid_options = 0;

  switch (in.in_regs.command) {
    case ATA_IDENTIFY_DEVICE:
    case ATA_IDENTIFY_PACKET_DEVICE:
      // SMART_*, ATA_, IDE_, SCSI_PASS_THROUGH, STORAGE_PREDICT_FAILURE
      // and SCSI_MINIPORT_* if requested by user
      valid_options = (m_usr_options ? "saicmf" : "saicf");
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
          valid_options = (m_usr_options ? "saicmf" : "saicf");
          break;

        case ATA_SMART_IMMEDIATE_OFFLINE:
          // SMART_SEND_DRIVE_COMMAND supports ABORT_SELF_TEST only on Win9x/ME
          valid_options = (m_usr_options || in.in_regs.lba_low != 127/*ABORT*/ || win9x ?
                           "saicm3" : "aicm3");
          break;

        case ATA_SMART_READ_LOG_SECTOR:
          // SMART_RCV_DRIVE_DATA supports this only on Win9x/ME
          // Try SCSI_MINIPORT also to skip buggy class driver
          // SMART functions do not support multi sector I/O.
          if (in.size == 512)
            valid_options = (m_usr_options || win9x ? "saicm3" : "aicm3");
          else
            valid_options = "a";
          break;

        case ATA_SMART_WRITE_LOG_SECTOR:
          // ATA_PASS_THROUGH, SCSI_MINIPORT, others don't support DATA_OUT
          // but SCSI_MINIPORT_* only if requested by user and single sector.
          valid_options = (in.size == 512 && m_usr_options ? "am" : "a");
          break;

        case ATA_SMART_STATUS:
          // May require lba_mid,lba_high register return
          if (in.out_needed.is_set())
            valid_options = (m_usr_options ? "saimf" : "saif");
          else
            valid_options = (m_usr_options ? "saicmf" : "saicf");
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
    else if (in.out_needed.is_set())
      // Need output registers: ATA/IDE_PASS_THROUGH
      valid_options = "ai";
    else
      valid_options = "aic";
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
          if (smart_get_version(get_fh()) < 0) {
            if (!con->permissive) {
              m_smartver_state = 2;
              rc = -1; errno = ENOSYS;
              break;
            }
            con->permissive--;
          }
          m_smartver_state = 1;
        }
        rc = smart_ioctl(get_fh(), m_drive, &regs, data, datasize, m_port);
        out_regs_set = (in.in_regs.features == ATA_SMART_STATUS);
        id_is_cached = (m_port < 0 && !win9x); // Not cached by 3ware or Win9x/ME driver
        break;
      case 'm':
        rc = ata_via_scsi_miniport_smart_ioctl(get_fh(), &regs, data, datasize);
        id_is_cached = (m_port < 0 && !win9x);
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
      case 'c':
        rc = ata_via_scsi_pass_through_ioctl(get_fh(), &regs, data, datasize);
        break;
      case 'f':
        if (in.in_regs.command == ATA_IDENTIFY_DEVICE) {
            rc = get_identify_from_device_property(get_fh(), (ata_identify_device *)data);
            id_is_cached = true;
        }
        else if (in.in_regs.command == ATA_SMART_CMD) switch (in.in_regs.features) {
          case ATA_SMART_READ_VALUES:
            rc = storage_predict_failure_ioctl(get_fh(), data);
            if (rc > 0)
              rc = 0;
            break;
          case ATA_SMART_READ_THRESHOLDS:
            {
              ata_smart_values sv;
              rc = storage_predict_failure_ioctl(get_fh(), (char *)&sv);
              if (rc < 0)
                break;
              rc = 0;
              // Fake zero thresholds
              ata_smart_thresholds_pvt * tr = (ata_smart_thresholds_pvt *)data;
              memset(tr, 0, 512);
              for (int i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++)
                tr->chksum -= tr->thres_entries[i].id = sv.vendor_attributes[i].id;
            }
            break;
          case ATA_SMART_ENABLE:
            rc = 0;
            break;
          case ATA_SMART_STATUS:
            rc = storage_predict_failure_ioctl(get_fh());
            if (rc >= 0) {
              if (rc > 0) {
                regs.bCylHighReg = 0x2c; regs.bCylLowReg = 0xf4;
                rc = 0;
              }
              out_regs_set = true;
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


/////////////////////////////////////////////////////////////////////////////
// ASPI Interface (for SCSI devices on 9x/ME)
/////////////////////////////////////////////////////////////////////////////

#if WIN9X_SUPPORT

#pragma pack(1)

#define ASPI_SENSE_SIZE 18

// ASPI SCSI Request block header

typedef struct {
  unsigned char cmd;             // 00: Command code
  unsigned char status;          // 01: ASPI status
  unsigned char adapter;         // 02: Host adapter number
  unsigned char flags;           // 03: Request flags
  unsigned char reserved[4];     // 04: 0
} ASPI_SRB_HEAD;

// SRB for host adapter inquiry

typedef struct {
  ASPI_SRB_HEAD h;               // 00: Header
  unsigned char adapters;        // 08: Number of adapters
  unsigned char target_id;       // 09: Target ID ?
  char manager_id[16];           // 10: SCSI manager ID
  char adapter_id[16];           // 26: Host adapter ID
  unsigned char parameters[16];  // 42: Host adapter unique parmameters
} ASPI_SRB_INQUIRY;

// SRB for get device type

typedef struct {
  ASPI_SRB_HEAD h;               // 00: Header
  unsigned char target_id;       // 08: Target ID
  unsigned char lun;             // 09: LUN
  unsigned char devtype;         // 10: Device type
  unsigned char reserved;        // 11: Reserved
} ASPI_SRB_DEVTYPE;

// SRB for SCSI I/O

typedef struct {
  ASPI_SRB_HEAD h;               // 00: Header
  unsigned char target_id;       // 08: Target ID
  unsigned char lun;             // 09: LUN
  unsigned char reserved[2];     // 10: Reserved
  unsigned long data_size;       // 12: Data alloc. lenght
  void * data_addr;              // 16: Data buffer pointer
  unsigned char sense_size;      // 20: Sense alloc. length
  unsigned char cdb_size;        // 21: CDB length
  unsigned char host_status;     // 22: Host status
  unsigned char target_status;   // 23: Target status
  void * event_handle;           // 24: Event handle
  unsigned char workspace[20];   // 28: ASPI workspace
  unsigned char cdb[16+ASPI_SENSE_SIZE];
} ASPI_SRB_IO;

// Macro to retrieve start of sense information
#define ASPI_SRB_SENSE(srb,cdbsz) ((srb)->cdb + 16)

// SRB union

typedef union {
  ASPI_SRB_HEAD h;       // Common header
  ASPI_SRB_INQUIRY q;    // Inquiry
  ASPI_SRB_DEVTYPE t;    // Device type
  ASPI_SRB_IO i;         // I/O
} ASPI_SRB;

#pragma pack()

// ASPI commands
#define ASPI_CMD_ADAPTER_INQUIRE        0x00
#define ASPI_CMD_GET_DEVICE_TYPE        0x01
#define ASPI_CMD_EXECUTE_IO             0x02
#define ASPI_CMD_ABORT_IO               0x03

// Request flags
#define ASPI_REQFLAG_DIR_TO_HOST        0x08
#define ASPI_REQFLAG_DIR_TO_TARGET      0x10
#define ASPI_REQFLAG_DIR_NO_XFER        0x18
#define ASPI_REQFLAG_EVENT_NOTIFY       0x40

// ASPI status
#define ASPI_STATUS_IN_PROGRESS         0x00
#define ASPI_STATUS_NO_ERROR            0x01
#define ASPI_STATUS_ABORTED             0x02
#define ASPI_STATUS_ABORT_ERR           0x03
#define ASPI_STATUS_ERROR               0x04
#define ASPI_STATUS_INVALID_COMMAND     0x80
#define ASPI_STATUS_INVALID_ADAPTER     0x81
#define ASPI_STATUS_INVALID_TARGET      0x82
#define ASPI_STATUS_NO_ADAPTERS         0xE8

// Adapter (host) status
#define ASPI_HSTATUS_NO_ERROR           0x00
#define ASPI_HSTATUS_SELECTION_TIMEOUT  0x11
#define ASPI_HSTATUS_DATA_OVERRUN       0x12
#define ASPI_HSTATUS_BUS_FREE           0x13
#define ASPI_HSTATUS_BUS_PHASE_ERROR    0x14
#define ASPI_HSTATUS_BAD_SGLIST         0x1A

// Target status
#define ASPI_TSTATUS_NO_ERROR           0x00
#define ASPI_TSTATUS_CHECK_CONDITION    0x02
#define ASPI_TSTATUS_BUSY               0x08
#define ASPI_TSTATUS_RESERV_CONFLICT    0x18


static HINSTANCE h_aspi_dll; // DLL handle
static UINT (* aspi_entry)(ASPI_SRB * srb); // ASPI entrypoint
static unsigned num_aspi_adapters;

#ifdef __CYGWIN__
// h_aspi_dll+aspi_entry is not inherited by Cygwin's fork()
static DWORD aspi_dll_pid; // PID of DLL owner to detect fork()
#define aspi_entry_valid() (aspi_entry && (aspi_dll_pid == GetCurrentProcessId()))
#else
#define aspi_entry_valid() (!!aspi_entry)
#endif


static int aspi_call(ASPI_SRB * srb)
{
  int i;
  aspi_entry(srb);
  i = 0;
  while (((volatile ASPI_SRB *)srb)->h.status == ASPI_STATUS_IN_PROGRESS) {
    if (++i > 100/*10sek*/) {
      pout("ASPI Adapter %u: Timed out\n", srb->h.adapter);
      aspi_entry = 0;
      h_aspi_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
      errno = EIO;
      return -1;
    }
    if (con->reportscsiioctl > 1)
      pout("ASPI Adapter %u: Waiting (%d) ...\n", srb->h.adapter, i);
    Sleep(100);
  }
  return 0;
}


// Get ASPI entrypoint from wnaspi32.dll

static FARPROC aspi_get_address(const char * name, int verbose)
{
  FARPROC addr;
  assert(h_aspi_dll && h_aspi_dll != INVALID_HANDLE_VALUE);

  if (!(addr = GetProcAddress(h_aspi_dll, name))) {
    if (verbose)
      pout("Missing %s() in WNASPI32.DLL\n", name);
    aspi_entry = 0;
    FreeLibrary(h_aspi_dll);
    h_aspi_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
    errno = ENOSYS;
    return 0;
  }
  return addr;
}


static int aspi_open_dll(int verbose)
{
  UINT (*aspi_info)(void);
  UINT info, rc;

  assert(!aspi_entry_valid());

  // Check structure layout
  assert(sizeof(ASPI_SRB_HEAD) == 8);
  assert(sizeof(ASPI_SRB_INQUIRY) == 58);
  assert(sizeof(ASPI_SRB_DEVTYPE) == 12);
  assert(sizeof(ASPI_SRB_IO) == 64+ASPI_SENSE_SIZE);
  assert(offsetof(ASPI_SRB,h.cmd) == 0);
  assert(offsetof(ASPI_SRB,h.flags) == 3);
  assert(offsetof(ASPI_SRB_IO,lun) == 9);
  assert(offsetof(ASPI_SRB_IO,data_addr) == 16);
  assert(offsetof(ASPI_SRB_IO,workspace) == 28);
  assert(offsetof(ASPI_SRB_IO,cdb) == 48);

  if (h_aspi_dll == INVALID_HANDLE_VALUE) {
    // do not retry
    errno = ENOENT;
    return -1;
  }

  // Load ASPI DLL
  if (!(h_aspi_dll = LoadLibraryA("WNASPI32.DLL"))) {
    if (verbose)
      pout("Cannot load WNASPI32.DLL, Error=%ld\n", GetLastError());
    h_aspi_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
    errno = ENOENT;
    return -1;
  }
  if (con->reportscsiioctl > 1) {
    // Print full path of WNASPI32.DLL
    char path[MAX_PATH];
    if (!GetModuleFileName(h_aspi_dll, path, sizeof(path)))
      strcpy(path, "*unknown*");
    pout("Using ASPI interface \"%s\"\n", path);
  }

  // Get ASPI entrypoints
  if (!(aspi_info = (UINT (*)(void))aspi_get_address("GetASPI32SupportInfo", verbose)))
    return -1;
  if (!(aspi_entry = (UINT (*)(ASPI_SRB *))aspi_get_address("SendASPI32Command", verbose)))
    return -1;

  // Init ASPI manager and get number of adapters
  info = (aspi_info)();
  if (con->reportscsiioctl > 1)
    pout("GetASPI32SupportInfo() returns 0x%04x\n", info);
  rc = (info >> 8) & 0xff;
  if (rc == ASPI_STATUS_NO_ADAPTERS) {
    num_aspi_adapters = 0;
  }
  else if (rc == ASPI_STATUS_NO_ERROR) {
    num_aspi_adapters = info & 0xff;
  }
  else {
    if (verbose)
      pout("Got strange 0x%04x from GetASPI32SupportInfo()\n", info);
    aspi_entry = 0;
    FreeLibrary(h_aspi_dll);
    h_aspi_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
    errno = ENOENT;
    return -1;
  }

  if (con->reportscsiioctl)
    pout("%u ASPI Adapter%s detected\n",num_aspi_adapters, (num_aspi_adapters!=1?"s":""));

#ifdef __CYGWIN__
  // save PID to detect fork() in aspi_entry_valid()
  aspi_dll_pid = GetCurrentProcessId();
#endif
  assert(aspi_entry_valid());
  return 0;
}


static int aspi_io_call(ASPI_SRB * srb, unsigned timeout)
{
  HANDLE event;
  // Create event
  if (!(event = CreateEventA(NULL, FALSE, FALSE, NULL))) {
    pout("CreateEvent(): Error=%ld\n", GetLastError()); return -EIO;
  }
  srb->i.event_handle = event;
  srb->h.flags |= ASPI_REQFLAG_EVENT_NOTIFY;
  // Start ASPI request
  aspi_entry(srb);
  if (((volatile ASPI_SRB *)srb)->h.status == ASPI_STATUS_IN_PROGRESS) {
    // Wait for event
    DWORD rc = WaitForSingleObject(event, timeout*1000L);
    if (rc != WAIT_OBJECT_0) {
      if (rc == WAIT_TIMEOUT) {
        pout("ASPI Adapter %u, ID %u: Timed out after %u seconds\n",
          srb->h.adapter, srb->i.target_id, timeout);
      }
      else {
        pout("WaitForSingleObject(%lx) = 0x%lx,%ld, Error=%ld\n",
          (unsigned long)event, rc, rc, GetLastError());
      }
      // TODO: ASPI_ABORT_IO command
      aspi_entry = 0;
      h_aspi_dll = (HINSTANCE)INVALID_HANDLE_VALUE;
      return -EIO;
    }
  }
  CloseHandle(event);
  return 0;
}


win_aspi_device::win_aspi_device(smart_interface * intf,
  const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "scsi", req_type),
  m_adapter(-1), m_id(0)
{
}

bool win_aspi_device::is_open() const
{
  return (m_adapter >= 0);
}

bool win_aspi_device::open()
{
  // scsi[0-9][0-f] => ASPI Adapter 0-9, ID 0-15, LUN 0
  unsigned adapter = ~0, id = ~0; int n1 = -1;
  const char * name = skipdev(get_dev_name());
  if (!(sscanf(name,"scsi%1u%1x%n", &adapter, &id, &n1) == 2 && n1 == (int)strlen(name)
        && adapter <= 9 && id < 16))
    return set_err(EINVAL);

  if (!aspi_entry_valid()) {
    if (aspi_open_dll(1/*verbose*/))
      return set_err(ENOENT);
  }

  // Adapter OK?
  if (adapter >= num_aspi_adapters) {
    pout("ASPI Adapter %u does not exist (%u Adapter%s detected).\n",
      adapter, num_aspi_adapters, (num_aspi_adapters!=1?"s":""));
    if (!is_permissive())
      return set_err(ENOENT);
  }

  // Device present ?
  ASPI_SRB srb;
  memset(&srb, 0, sizeof(srb));
  srb.h.cmd = ASPI_CMD_GET_DEVICE_TYPE;
  srb.h.adapter = adapter; srb.i.target_id = id;
  if (aspi_call(&srb))
    return set_err(EIO);
  if (srb.h.status != ASPI_STATUS_NO_ERROR) {
    pout("ASPI Adapter %u, ID %u: No such device (Status=0x%02x)\n", adapter, id, srb.h.status);
    if (!is_permissive())
      return set_err(srb.h.status == ASPI_STATUS_INVALID_TARGET ? ENOENT : EIO);
  }
  else if (con->reportscsiioctl)
    pout("ASPI Adapter %u, ID %u: Device Type=0x%02x\n", adapter, id, srb.t.devtype);

  m_adapter = (int)adapter; m_id = (unsigned char)id;
  return true;
}


bool win_aspi_device::close()
{
  // No FreeLibrary(h_aspi_dll) to prevent problems with ASPI threads
  return true;
}


// Scan for ASPI drives

bool win9x_smart_interface::scsi_scan(smart_device_list & devlist)
{
  if (!aspi_entry_valid()) {
    if (aspi_open_dll(con->reportscsiioctl/*default is quiet*/))
      return true;
  }

  for (unsigned ad = 0; ad < num_aspi_adapters; ad++) {
    ASPI_SRB srb;

    if (ad > 9) {
      if (con->reportscsiioctl)
        pout(" ASPI Adapter %u: Ignored\n", ad);
      continue;
    }

    // Get adapter name
    memset(&srb, 0, sizeof(srb));
    srb.h.cmd = ASPI_CMD_ADAPTER_INQUIRE;
    srb.h.adapter = ad;
    if (aspi_call(&srb))
      break;

    if (srb.h.status != ASPI_STATUS_NO_ERROR) {
      if (con->reportscsiioctl)
        pout(" ASPI Adapter %u: Status=0x%02x\n", ad, srb.h.status);
      continue;
    }

    if (con->reportscsiioctl) {
      for (int i = 1; i < 16 && srb.q.adapter_id[i]; i++)
        if (!(' ' <= srb.q.adapter_id[i] && srb.q.adapter_id[i] <= '~'))
          srb.q.adapter_id[i] = '?';
      pout(" ASPI Adapter %u (\"%.16s\"):\n", ad, srb.q.adapter_id);
    }

    bool ignore = !strnicmp(srb.q.adapter_id, "3ware", 5);

    for (unsigned id = 0; id <= 7; id++) {
      // Get device type
      memset(&srb, 0, sizeof(srb));
      srb.h.cmd = ASPI_CMD_GET_DEVICE_TYPE;
      srb.h.adapter = ad; srb.i.target_id = id;
      if (aspi_call(&srb))
        return 0;
      if (srb.h.status != ASPI_STATUS_NO_ERROR) {
        if (con->reportscsiioctl > 1)
          pout("  ID %u: No such device (Status=0x%02x)\n", id, srb.h.status);
        continue;
      }

      if (!ignore && srb.t.devtype == 0x00/*HDD*/) {
        if (con->reportscsiioctl)
          pout("  ID %u: Device Type=0x%02x\n", id, srb.t.devtype);
        char name[20];
        sprintf(name, "/dev/scsi%u%u", ad, id);
        devlist.push_back( new win_aspi_device(this, name, "scsi") );
      }
      else if (con->reportscsiioctl)
        pout("  ID %u: Device Type=0x%02x (ignored)\n", id, srb.t.devtype);
    }
  }
  return true;
}


// Interface to ASPI SCSI devices
bool win_aspi_device::scsi_pass_through(scsi_cmnd_io * iop)
{
  int report = con->reportscsiioctl; // TODO

  if (m_adapter < 0) {
    set_err(EBADF);
    return false;
  }

  if (!aspi_entry_valid()) {
    set_err(EBADF);
    return false;
  }

  if (!(iop->cmnd_len == 6 || iop->cmnd_len == 10 || iop->cmnd_len == 12 || iop->cmnd_len == 16)) {
    set_err(EINVAL, "bad CDB length");
    return false;
  }

  if (report > 0) {
    // From os_linux.c
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
    pout(buff);
  }

  ASPI_SRB srb;
  memset(&srb, 0, sizeof(srb));
  srb.h.cmd = ASPI_CMD_EXECUTE_IO;
  srb.h.adapter = m_adapter;
  srb.i.target_id = m_id;
  //srb.i.lun = 0;
  srb.i.sense_size = ASPI_SENSE_SIZE;
  srb.i.cdb_size = iop->cmnd_len;
  memcpy(srb.i.cdb, iop->cmnd, iop->cmnd_len);

  switch (iop->dxfer_dir) {
    case DXFER_NONE:
      srb.h.flags = ASPI_REQFLAG_DIR_NO_XFER;
      break;
    case DXFER_FROM_DEVICE:
      srb.h.flags = ASPI_REQFLAG_DIR_TO_HOST;
      srb.i.data_size = iop->dxfer_len;
      srb.i.data_addr = iop->dxferp;
      break;
    case DXFER_TO_DEVICE:
      srb.h.flags = ASPI_REQFLAG_DIR_TO_TARGET;
      srb.i.data_size = iop->dxfer_len;
      srb.i.data_addr = iop->dxferp;
      break;
    default:
      set_err(EINVAL, "bad dxfer_dir");
      return false;
  }

  iop->resp_sense_len = 0;
  iop->scsi_status = 0;
  iop->resid = 0;

  if (aspi_io_call(&srb, (iop->timeout ? iop->timeout : 60))) {
    // Timeout
    set_err(EIO, "ASPI Timeout"); return false;
  }

  if (srb.h.status != ASPI_STATUS_NO_ERROR) {
    if (   srb.h.status        == ASPI_STATUS_ERROR
        && srb.i.host_status   == ASPI_HSTATUS_NO_ERROR
        && srb.i.target_status == ASPI_TSTATUS_CHECK_CONDITION) {
      // Sense valid
      const unsigned char * sense = ASPI_SRB_SENSE(&srb.i, iop->cmnd_len);
      int len = (ASPI_SENSE_SIZE < iop->max_sense_len ? ASPI_SENSE_SIZE : iop->max_sense_len);
      iop->scsi_status = SCSI_STATUS_CHECK_CONDITION;
      if (len > 0 && iop->sensep) {
        memcpy(iop->sensep, sense, len);
        iop->resp_sense_len = len;
        if (report > 1) {
          pout("  >>> Sense buffer, len=%d:\n", (int)len);
          dStrHex(iop->sensep, len , 1);
        }
      }
      if (report) {
        pout("  sense_key=%x asc=%x ascq=%x\n",
         sense[2] & 0xf, sense[12], sense[13]);
      }
      return true;
    }
    else {
      if (report)
        pout("  ASPI call failed, (0x%02x,0x%02x,0x%02x)\n", srb.h.status, srb.i.host_status, srb.i.target_status);
      set_err(EIO);
      return false;
    }
  }

  if (report > 0)
    pout("  OK\n");

  if (iop->dxfer_dir == DXFER_FROM_DEVICE && report > 1) {
     int trunc = (iop->dxfer_len > 256) ? 1 : 0;
     pout("  Incoming data, len=%d%s:\n", (int)iop->dxfer_len,
        (trunc ? " [only first 256 bytes shown]" : ""));
        dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
  }

  return true;
}

#endif // WIN9X_SUPPORT

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
  // sd[a-z],N => Physical drive 0-26, RAID port N
  char drive[1+1] = ""; int sub_addr = -1; int n1 = -1; int n2 = -1;
  if (   sscanf(name, "sd%1[a-z]%n,%d%n", drive, &n1, &sub_addr, &n2) >= 1
      && ((n1 == len && sub_addr == -1) || (n2 == len && sub_addr >= 0))  ) {
    return open(drive[0] - 'a', -1, -1, sub_addr);
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
    set_err(ENODEV, "%s: Open failed, Error=%ld", b, GetLastError());
    return false;
  }
  set_fh(h);
  return true;
}


bool winnt_smart_interface::scsi_scan(smart_device_list & devlist)
{
  char name[20];
  for (int i = 0; i <= 9; i++) {
    if (get_phy_drive_type(i) != DEV_SCSI)
      continue;
    // STORAGE_QUERY_PROPERTY returned SCSI/SAS/...
    sprintf(name, "/dev/sd%c", 'a'+i);
    devlist.push_back( new win_scsi_device(this, name, "scsi") );
  }
  return true;
}


#define IOCTL_SCSI_PASS_THROUGH_DIRECT  \
  CTL_CODE(IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

typedef struct _SCSI_PASS_THROUGH_DIRECT {
  USHORT          Length;
  UCHAR           ScsiStatus;
  UCHAR           PathId;
  UCHAR           TargetId;
  UCHAR           Lun;
  UCHAR           CdbLength;
  UCHAR           SenseInfoLength;
  UCHAR           DataIn;
  ULONG           DataTransferLength;
  ULONG           TimeOutValue;
  PVOID           DataBuffer;
  ULONG           SenseInfoOffset;
  UCHAR           Cdb[16];
} SCSI_PASS_THROUGH_DIRECT;

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
  int report = con->reportscsiioctl; // TODO

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
    pout(buff);
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

  if ((iop->dxfer_len > 0) && (sb.spt.DataTransferLength > 0))
    iop->resid = iop->dxfer_len - sb.spt.DataTransferLength;
  else
    iop->resid = 0;

  if ((iop->dxfer_dir == DXFER_FROM_DEVICE) && (report > 1)) {
     int trunc = (iop->dxfer_len > 256) ? 1 : 0;
     pout("  Incoming data, len=%d%s:\n", (int)iop->dxfer_len,
        (trunc ? " [only first 256 bytes shown]" : ""));
        dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
  }
  return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////////


} // namespace

/////////////////////////////////////////////////////////////////////////////

// Initialize platform interface and register with smi()
void smart_interface::init()
{
  // Select interface for Windows flavor
  if (GetVersion() & 0x80000000) {
#if WIN9X_SUPPORT
    static os_win32::win9x_smart_interface the_win9x_interface;
    smart_interface::set(&the_win9x_interface);
#else
    throw std::runtime_error("Win9x/ME not supported");
#endif
  }
  else {
    static os_win32::winnt_smart_interface the_winnt_interface;
    smart_interface::set(&the_winnt_interface);
  }
}

