/*
 * os_win32.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004-7 Christian Franke <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include "config.h"
#include "int64.h"
#include "atacmds.h"
#include "extern.h"
extern smartmonctrl * con; // con->permissive,reportataioctl
#include "scsicmds.h"
#include "utility.h"
extern int64_t bytes; // malloc() byte count

#include <errno.h>
#ifdef _DEBUG
#include <assert.h>
#else
#define assert(x) /**/
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h> // offsetof()
#include <io.h> // access()

// Macro to check constants at compile time using a dummy typedef
#define ASSERT_CONST(c, n) \
  typedef char assert_const_##c[((c) == (n)) ? 1 : -1]
#define ASSERT_SIZEOF(t, n) \
  typedef char assert_sizeof_##t[(sizeof(t) == (n)) ? 1 : -1]


// Needed by '-V' option (CVS versioning) of smartd/smartctl
const char *os_XXXX_c_cvsid="$Id: os_win32.cpp,v 1.58 2007/09/14 20:52:14 chrfranke Exp $"
ATACMDS_H_CVSID CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;


// Running on Win9x/ME ?
static inline bool is_win9x()
{
	return !!(GetVersion() & 0x80000000);
}

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


#ifndef HAVE_GET_OS_VERSION_STR
#error define of HAVE_GET_OS_VERSION_STR missing in config.h
#endif

// Return build host and OS version as static string
const char * get_os_version_str()
{
	static char vstr[sizeof(SMARTMONTOOLS_BUILD_HOST)-3-1+sizeof("-2003r2(64)-sp2.1")+13];
	char * const vptr = vstr+sizeof(SMARTMONTOOLS_BUILD_HOST)-3-1;
	const int vlen = sizeof(vstr)-(sizeof(SMARTMONTOOLS_BUILD_HOST)-3);

	// remove "-pc" to avoid long lines
	assert(!strncmp(SMARTMONTOOLS_BUILD_HOST+5, "pc-", 3));
	strcpy(vstr, "i686-"); strcpy(vstr+5, SMARTMONTOOLS_BUILD_HOST+5+3);
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
		w = (!GetSystemMetrics(89/*SM_SERVERR2*/) ?      "2003"
		                                             :   "2003r2"); break;
	  case VER_PLATFORM_WIN32_NT     <<16|0x0600| 0: w = "vista";  break;
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


static int get_controller_type(int phydrive, int logdrive);

#define ATARAID_FDOFFSET 0x0200

static int ata_open(int phydrive, int logdrive, const char * options, int port);
static void ata_close(int fd);
static int ata_scan_win9x(unsigned long * drives);
static int ata_scan(unsigned long * drives, int * rdriveno, unsigned long * rdrives);
static const char * ata_get_def_options(void);

#define TW_CLI_FDOFFSET 0x0300

static int tw_cli_open(const char * name);
static void tw_cli_close();

#define ASPI_FDOFFSET 0x0100

static int aspi_open(unsigned adapter, unsigned id);
static void aspi_close(int fd);
static int aspi_scan(unsigned long * drives);

#define SPT_FDOFFSET 0x0400

static int spt_open(int pd_num, int ld_num, int tape_num, int sub_addr);
static void spt_close(int fd);
static int spt_scan(unsigned long * drives);


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


// tries to guess device type given the name (a path).  See utility.h
// for return values.
int guess_device_type (const char * dev_name)
{
	dev_name = skipdev(dev_name);
	if (!strncmp(dev_name, "scsi", 4))
		return CONTROLLER_SCSI;
	if (is_win9x())
		return CONTROLLER_ATA;
	if (!strncmp(dev_name, "hd", 2))
		return CONTROLLER_ATA;
	if (!strncmp(dev_name, "tw_cli", 6))
		return CONTROLLER_ATA;
	if (!strncmp(dev_name, "st", 2))
		return CONTROLLER_SCSI;
	if (!strncmp(dev_name, "nst", 3))
		return CONTROLLER_SCSI;
	if (!strncmp(dev_name, "tape", 4))
		return CONTROLLER_SCSI;
	int logdrive = drive_letter(dev_name);
	if (logdrive >= 0) {
		int type = get_controller_type(-1, logdrive);
		return (type != CONTROLLER_UNKNOWN ? type : CONTROLLER_SCSI);
	}
	char drive[1+1] = "";
	if (sscanf(dev_name, "sd%1[a-z]", drive) == 1)
		return get_controller_type(drive[0]-'a', -1);
	int phydrive = -1;
	if (sscanf(dev_name, "pd%d", &phydrive) == 1 && phydrive >= 0)
		return get_controller_type(phydrive, -1);
	return CONTROLLER_UNKNOWN;
}


// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number N of devices, or -1 if out of
// memory. Allocates N+1 arrays: one of N pointers (devlist), the
// others each contain null-terminated character strings.
int make_device_names (char*** devlist, const char* type)
{
	unsigned long drives[3];
	int rdriveno[2];
	unsigned long rdrives[2];
	int i, j, n, nmax, sz;
	const char * path;

	drives[0] = drives[1] = drives[2] = 0;
	rdriveno[0] = rdriveno[1] = -1;
	rdrives[0] = rdrives[1] = 0;
	
	bool win9x = is_win9x();
	if (!strcmp(type, "ATA")) {
		// bit i set => drive i present
		if (win9x) {
			n = ata_scan_win9x(drives);
			path = "/dev/hda";
		}
		else {
			n = ata_scan(drives, rdriveno, rdrives);
			path = "/dev/sda";
		}
		nmax = 10;
	}
	else if (!strcmp(type, "SCSI")) {
		if (win9x) {
			// bit i set => drive with ID (i & 0x7) on adapter (i >> 3) present
			n = aspi_scan(drives);
			path = "/dev/scsi00";
			nmax = 10*8;
		}
		else {
			// bit i set => drive i present
			n = spt_scan(drives);
			path = "/dev/sda";
			nmax = 10;
		}
	}
	else
		return -1;

	if (n <= 0)
		return 0;

	// Alloc devlist
	sz = n * sizeof(char **);
	*devlist = (char **)malloc(sz); bytes += sz;

	// Add devices
	for (i = j = 0; i < n; ) {
		while (j < nmax && !(drives[j >> 5] & (1L << (j & 0x1f))))
			j++;
		assert(j < nmax);

		if (j == rdriveno[0] || j == rdriveno[1]) {
			// Add physical drives behind this logical drive
			int ci = (j == rdriveno[0] ? 0 : 1);
			for (int pi = 0; pi < 32 && i < n; pi++) {
				if (!(rdrives[ci] & (1L << pi)))
					continue;
				char rpath[20];
				sprintf(rpath, "/dev/sd%c,%u", 'a'+j, pi);
				sz = strlen(rpath)+1;
				char * s = (char *)malloc(sz); bytes += sz;
				strcpy(s, rpath);
				(*devlist)[i++] = s;
			}
		}
		else {
			sz = strlen(path)+1;
			char * s = (char *)malloc(sz); bytes += sz;
			strcpy(s, path);

			if (nmax <= 10) {
				assert(j <= 9);
				s[sz-2] += j; // /dev/hd[a-j]
			}
			else {
				assert((j >> 3) <= 9);
				s[sz-3] += (j >> 3);  // /dev/scsi[0-9].....
				s[sz-2] += (j & 0x7); //          .....[0-7]
			}
			(*devlist)[i++] = s;
		}
		j++;
	}

	return n;
}


// Like open().  Return positive integer handle, only used by
// functions below.  type="ATA" or "SCSI".  If you need to store extra
// information about your devices, create a private internal array
// within this file (see os_freebsd.cpp for an example).
int deviceopen(const char * pathname, char *type)
{
	pathname = skipdev(pathname);
	int len = strlen(pathname);

	if (!strcmp(type, "ATA")) {
		// [sh]d[a-z](:[saicp]+)? => Physical drive 0-25, with options
		char drive[1+1] = "", options[7+1] = ""; int n1 = -1, n2 = -1;
		if (   sscanf(pathname, "%*[sh]d%1[a-z]%n:%6[saicmp]%n", drive, &n1, options, &n2) >= 1
		    && ((n1 == len && !options[0]) || n2 == len)                                       ) {
			return ata_open(drive[0] - 'a', -1, options, -1);
		}
		// [sh]d[a-z],N(:[saicp]+)? => Physical drive 0-25, RAID port N, with options
		drive[0] = 0; options[0] = 0; n1 = -1; n2 = -1;
		unsigned port = ~0;
		if (   sscanf(pathname, "%*[sh]d%1[a-z],%u%n:%7[saicmp3]%n", drive, &port, &n1, options, &n2) >= 2
		    && port < 32 && ((n1 == len && !options[0]) || n2 == len)                                     ) {
			return ata_open(drive[0] - 'a', -1, options, port);
		}
		// pd<m>,N => Physical drive <m>, RAID port N
		int phydrive = -1; port = ~0; n1 = -1; n2 = -1;
		if (   sscanf(pathname, "pd%d%n,%u%n", &phydrive, &n1, &port, &n2) >= 1
		    && phydrive >= 0 && ((n1 == len && (int)port < 0) || (n2 == len && port < 32))) {
			return ata_open(phydrive, -1, "", (int)port);
		}
		// [a-zA-Z]: => Physical drive behind logical drive 0-25
		int logdrive = drive_letter(pathname);
		if (logdrive >= 0) {
			return ata_open(-1, logdrive, "", -1);
		}
		// tw_cli/... => Parse tw_cli output
		if (!strncmp(pathname, "tw_cli/", 7)) {
			return tw_cli_open(pathname+7);
		}
	} else if (!strcmp(type, "SCSI")) {
		// scsi[0-9][0-f] => ASPI Adapter 0-9, ID 0-15, LUN 0
		unsigned adapter = ~0, id = ~0; int n1 = -1;
		if (sscanf(pathname,"scsi%1u%1x%n", &adapter, &id, &n1) == 2 && n1 == len) {
			return aspi_open(adapter, id);
		}
		// sd[a-z],N => Physical drive 0-25, RAID port N
		char drive[1+1] = ""; int sub_addr = -1; n1 = -1; int n2 = -1;
		if (   sscanf(pathname, "sd%1[a-z]%n,%d%n", drive, &n1, &sub_addr, &n2) >= 1
		    && ((n1 == len && sub_addr == -1) || (n2 == len && sub_addr >= 0))      ) {
			return spt_open(drive[0] - 'a', -1, -1, sub_addr);
		}
		// pd<m>,N => Physical drive <m>, RAID port N
		int pd_num = -1; sub_addr = -1; n1 = -1; n2 = -1;
		if (   sscanf(pathname, "pd%d%n,%d%n", &pd_num, &n1, &sub_addr, &n2) >= 1
		    && pd_num >= 0 && ((n1 == len && sub_addr == -1) || (n2 == len && sub_addr >= 0))) {
			return spt_open(pd_num, -1, -1, sub_addr);
		}
		// [a-zA-Z]: => Physical drive behind logical drive 0-25
		int logdrive = drive_letter(pathname);
		if (logdrive >= 0) {
			return spt_open(-1, logdrive, -1, -1);
		}
		// n?st<m> => tape drive <m> (same names used in Cygwin's /dev emulation)
		int tape_num = -1; n1 = -1;
		if (sscanf(pathname, "st%d%n", &tape_num, &n1) == 1 && tape_num >= 0 && n1 == len) {
			return spt_open(-1, -1, tape_num, -1);
		}
		tape_num = -1; n1 = -1;
		if (sscanf(pathname, "nst%d%n", &tape_num, &n1) == 1 && tape_num >= 0 && n1 == len) {
			return spt_open(-1, -1, tape_num, -1);
		}
		// tape<m> => tape drive <m>
		tape_num = -1; n1 = -1;
		if (sscanf(pathname, "tape%d%n", &tape_num, &n1) == 1 && tape_num >= 0 && n1 == len) {
			return spt_open(-1, -1, tape_num, -1);
		}
	}

	errno = EINVAL;
	return -1;
}


// Like close().  Acts only on handles returned by above function.
// (Never called in smartctl!)
int deviceclose(int fd)
{
	if ((fd & 0xff00) == ASPI_FDOFFSET)
		aspi_close(fd);
	else if (fd >= SPT_FDOFFSET)
 		spt_close(fd);
	else if (fd == TW_CLI_FDOFFSET)
		tw_cli_close();
	else
		ata_close(fd);
	return 0;
}


// print examples for smartctl
void print_smartctl_examples(){
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n"
         "  smartctl -a /dev/hda                       (Prints all SMART information)\n\n"
#ifdef HAVE_GETOPT_LONG
         "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/hda\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long /dev/hda              (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/hda\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
#else
         "  smartctl -s on -o on -S on /dev/hda         (Enables SMART on first disk)\n"
         "  smartctl -t long /dev/hda              (Executes extended disk self-test)\n"
         "  smartctl -A -l selftest -q errorsonly /dev/hda\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
#endif
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
         "  'm': IOCTL_SCSI_MINIPORT_*.\n"
         "  The default on this system is /dev/hdX:%s\n", ata_get_def_options()
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

#define FILE_DEVICE_DISK	   7
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

static int smart_get_version(HANDLE hdevice, unsigned long * portmap = 0)
{
	GETVERSIONOUTPARAMS vers;
	const GETVERSIONINPARAMS_EX & vers_ex = (const GETVERSIONINPARAMS_EX &)vers;
	DWORD num_out;

	memset(&vers, 0, sizeof(vers));
	if (!DeviceIoControl(hdevice, SMART_GET_VERSION,
		NULL, 0, &vers, sizeof(vers), &num_out, NULL)) {
		pout("  SMART_GET_VERSION failed, Error=%ld\n", GetLastError());
		errno = ENOSYS;
		return -1;
	}
	assert(num_out == sizeof(GETVERSIONOUTPARAMS));

	if (portmap) {
		// Return bitmask of valid RAID ports
		if (vers_ex.wIdentifier != SMART_VENDOR_3WARE) {
			pout("  SMART_GET_VERSION returns unknown Identifier = %04x\n"
				 "  This is no 3ware 9000 controller or driver has no SMART support.\n", vers_ex.wIdentifier);
			errno = ENOENT;
			return -1;
		}
		*portmap = vers_ex.dwDeviceMapEx;
	}

	if (con->reportataioctl > 1) {
		pout("  SMART_GET_VERSION suceeded, bytes returned: %lu\n"
		     "    Vers = %d.%d, Caps = 0x%lx, DeviceMap = 0x%02x\n",
			num_out, vers.bVersion, vers.bRevision,
			vers.fCapabilities, vers.bIDEDeviceMap);
		if (vers_ex.wIdentifier == SMART_VENDOR_3WARE)
			pout("    Identifier = %04x(3WARE), ControllerId=%u, DeviceMapEx = 0x%08lx\n",
			vers_ex.wIdentifier, vers_ex.wControllerId, vers_ex.dwDeviceMapEx);
	}

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

	assert(datasize == 0 || datasize == 512);
	if (datasize) {
		code = SMART_RCV_DRIVE_DATA; name = "SMART_RCV_DRIVE_DATA";
		inpar.cBufferSize = size_out = 512;
	}
	else {
		code = SMART_SEND_DRIVE_COMMAND; name = "SMART_SEND_DRIVE_COMMAND";
		if (regs->bFeaturesReg == ATA_SMART_STATUS)
			size_out = sizeof(IDEREGS); // ioctl returns new IDEREGS as data
			// Note: cBufferSize must be 0 on Win9x
		else
			size_out = 0;
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
		if (nonempty(const_cast<unsigned char *>(outpar->bBuffer), sizeof(IDEREGS)))
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


/////////////////////////////////////////////////////////////////////////////

static int ata_pass_through_ioctl(HANDLE hdevice, IDEREGS * regs, char * data, int datasize)
{ 
	typedef struct {
		ATA_PASS_THROUGH_EX apt;
		ULONG Filler;
		UCHAR ucDataBuf[512];
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
	*ctfregs = *regs;

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
	if (ctfregs->bCommandReg/*Status*/ & 0x01) {
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
	//	code = IOCTL_SCSI_MINIPORT_SAVE_ATTRIBUTE_VALUES; name = "SAVE_ATTRIBUTE_VALUES"; break;
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
	STARTUPINFO si;	memset(&si, 0, sizeof(si)); si.cb = sizeof(si);
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


static ata_identify_device * tw_cli_identbuf = 0;
static ata_smart_values * tw_cli_smartbuf = 0;

static int tw_cli_open(const char * name)
{
	// Read tw_cli or 3DM browser output into buffer
	char buffer[4096];
	int size = -1, n1 = -1;
	if (!strcmp(name, "clip")) { // tw_cli/clip => read clipboard
		size = get_clipboard(buffer, sizeof(buffer));
	}
	else if (!strcmp(name, "stdin")) {  // tw_cli/stdin => read stdin
		size = fread(buffer, 1, sizeof(buffer), stdin);
	}
	else if (sscanf(name, "c%*u/p%*u%n", &n1) >= 0 && n1 == (int)strlen(name)) {
		// tw_cli/cx/py => read output from "tw_cli /cx/py show all"
		char cmd[100];
		snprintf(cmd, sizeof(cmd), "tw_cli /%s show all", name);
		if (con->reportataioctl > 1)
			pout("tw_cli/%s: Run: \"%s\"\n", name, cmd);
		size = run_cmd(cmd, buffer, sizeof(buffer));
	}
	else {
		errno = EINVAL; return -1;
	}

	if (con->reportataioctl > 1)
		pout("tw_cli/%s: Read %d bytes\n", name, size);
	if (size <= 0) {
		errno = ENOENT; return -1;
	}
	if (size >= (int)sizeof(buffer)) {
		errno = EIO; return -1;
	}
	buffer[size] = 0;
	if (con->reportataioctl > 1)
		pout("[\n%.100s%s\n]\n", buffer, (size>100?"...":""));

	// Fake identify sector
	ASSERT_SIZEOF(ata_identify_device, 512);
	ata_identify_device * id = (ata_identify_device *)malloc(sizeof(ata_identify_device));
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
	id->major_rev_num = 0x1<<3; // ATA-3
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
	unsigned char * sd = (unsigned char *)malloc(512);
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
		free(sd);
		if (!id->model[1]) {
			// No useful data found
			free(id);
			char * err = strstr(buffer, "Error:");
			if (!err)
				err = strstr(buffer, "error :");
			if (err) {
				// Print tw_cli error message
				err[strcspn(err, "\r\n")] = 0;
				pout("%s\n", err);
			}
			errno = EIO;
			return -1;
		}
		sd = 0;
	}

	tw_cli_identbuf = id;
	tw_cli_smartbuf = (ata_smart_values *)sd;
	return TW_CLI_FDOFFSET;
}


static void tw_cli_close()
{
	if (tw_cli_identbuf) {
		free(tw_cli_identbuf); tw_cli_identbuf = 0;
	}
	if (tw_cli_smartbuf) {
		free(tw_cli_smartbuf); tw_cli_smartbuf = 0;
	}
}


static int tw_cli_command_interface(smart_command_set command, int /*select*/, char * data)
{
	switch (command) {
	  case IDENTIFY:
		if (!tw_cli_identbuf)
			break;
		memcpy(data, tw_cli_identbuf, 512);
		return 0;
	  case READ_VALUES:
		if (!tw_cli_smartbuf)
			break;
		memcpy(data, tw_cli_smartbuf, 512);
		return 0;
	  case READ_THRESHOLDS:
		if (!tw_cli_smartbuf)
			break;
		// Fake zero thresholds
		{
			const ata_smart_values   * sv = tw_cli_smartbuf;
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
	errno = ENOSYS;
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

// Return STORAGE_BUS_TYPE for device, BusTypeUnknown on error.
// (HANDLE does not need any access rights, therefore this works
// without admin rights)

static STORAGE_BUS_TYPE ioctl_get_storage_bus_type(HANDLE hdevice)
{
	STORAGE_PROPERTY_QUERY query = {StorageDeviceProperty, PropertyStandardQuery, 0};

	union {
		STORAGE_DEVICE_DESCRIPTOR dev;
		char raw[256];
	} prop;
	memset(&prop, 0, sizeof(prop));

	DWORD num_out;
	if (!DeviceIoControl(hdevice, IOCTL_STORAGE_QUERY_PROPERTY,
		&query, sizeof(query), &prop, sizeof(prop), &num_out, NULL)) {
		if (con->reportataioctl > 1 || con->reportscsiioctl > 1)
			pout("  IOCTL_STORAGE_QUERY_PROPERTY failed, Error=%ld\n", GetLastError());
		return BusTypeUnknown;
	}

	if (con->reportataioctl > 1 || con->reportscsiioctl > 1) {
		pout("  IOCTL_STORAGE_QUERY_PROPERTY returns:\n"
		     "    Vendor:   \"%s\"\n"
		     "    Product:  \"%s\"\n"
		     "    Revision: \"%s\"\n"
		     "    Removable: %s\n"
		     "    BusType:   0x%02x\n",
		     (prop.dev.VendorIdOffset        ? prop.raw+prop.dev.VendorIdOffset : ""),
		     (prop.dev.ProductIdOffset       ? prop.raw+prop.dev.ProductIdOffset : ""),
		     (prop.dev.ProductRevisionOffset ? prop.raw+prop.dev.ProductRevisionOffset : ""),
		     (prop.dev.RemovableMedia? "Yes":"No"), prop.dev.BusType
		);
	}
	return prop.dev.BusType;
}

// get CONTROLLER_* for open handle
static int get_controller_type(HANDLE hdevice)
{
	STORAGE_BUS_TYPE type = ioctl_get_storage_bus_type(hdevice);
	switch (type) {
		case BusTypeAta:
		case BusTypeSata:
			return CONTROLLER_ATA;
		case BusTypeScsi:
		case BusTypeiScsi:
		case BusTypeSas:
			return CONTROLLER_SCSI;
		default:
			return CONTROLLER_UNKNOWN;
	}
	/*NOTREACHED*/
}

// get CONTROLLER_* for device path
static int get_controller_type(const char * path)
{
	HANDLE h = CreateFileA(path, 0/*NO ACCESS*/, FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, NULL);
	if (h == INVALID_HANDLE_VALUE)
		return CONTROLLER_UNKNOWN;
	if (con->reportataioctl > 1 || con->reportscsiioctl > 1)
		pout(" %s: successfully opened\n", path);
	int type = get_controller_type(h);
	CloseHandle(h);
	return type;
}

// get CONTROLLER_* for physical or logical drive number
static int get_controller_type(int phydrive, int logdrive)
{
	char path[30];
	if (phydrive >= 0)
		snprintf (path, sizeof(path)-1, "\\\\.\\PhysicalDrive%d", phydrive);
	else if (logdrive >= 0)
		snprintf(path, sizeof(path)-1, "\\\\.\\%c:", 'A'+logdrive);
	else
		return CONTROLLER_UNKNOWN;
	return get_controller_type(path);
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

// TODO: Put in a struct indexed by fd (or better a C++ object of course ;-)
static HANDLE h_ata_ioctl = 0;
static const char * ata_def_options;
static char * ata_usr_options;
const int max_ata_driveno = 25;
static int ata_driveno; // Drive number
static int ata_driveno_is_log = -1; // 0=physical drivenumber, 1=logical drive number, -1=unknown
static char ata_smartver_state[max_ata_driveno+1]; // SMART_GET_VERSION: 0=unknown, 1=OK, 2=failed

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


// Get default ATA device options

static const char * ata_get_def_options()
{
	DWORD ver = GetVersion();
	if ((ver & 0x80000000) || (ver & 0xff) < 4) // Win9x/ME
		return "s"; // SMART_* only
	else if ((ver & 0xff) == 4) // WinNT4
		return "sc"; // SMART_*, SCSI_PASS_THROUGH
	else // WinXP, 2003, Vista
		return "psaim"; // GetDevicePowerState(), SMART_*, ATA_, IDE_PASS_THROUGH, SCSI_MINIPORT_*
}


// Open ATA device

static int ata_open(int phydrive, int logdrive, const char * options, int port)
{
	// TODO: This version does not allow to open more than 1 ATA devices
	if (h_ata_ioctl) {
		errno = ENFILE;
		return -1;
	}

	// Using both physical and logical drive names (in smartd.conf) not supported yet
	if (!(   ata_driveno_is_log < 0
	      || (phydrive >= 0 && !ata_driveno_is_log)
	      || (logdrive >= 0 &&  ata_driveno_is_log))) {
		pout("Using both /dev/hdX and X: is not supported\n");
		errno = EINVAL;
		return -1;
	}

	// path depends on Windows Version
	bool win9x = is_win9x();
	char devpath[30];
	if (win9x && 0 <= phydrive && phydrive <= 7)
		// Use patched "smartvse.vxd" for drives 4-7, see INSTALL file for details
		strcpy(devpath, (phydrive <= 3 ? "\\\\.\\SMARTVSD" : "\\\\.\\SMARTVSE"));
	else if (!win9x && 0 <= phydrive && phydrive <= max_ata_driveno)
		snprintf(devpath, sizeof(devpath)-1, "\\\\.\\PhysicalDrive%d", phydrive);
	else if (!win9x && 0 <= logdrive && logdrive <= max_ata_driveno) {
		snprintf(devpath, sizeof(devpath)-1, "\\\\.\\%c:", 'A'+logdrive);
	}
	else {
		errno = ENOENT;
		return -1;
	}

	// Open device
	if ((h_ata_ioctl = CreateFileA(devpath,
		GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE) {
		long err = GetLastError();	
		pout("Cannot open device %s, Error=%ld\n", devpath, err);
		if (err == ERROR_FILE_NOT_FOUND)
			errno = (win9x && phydrive <= 3 ? smartvsd_error() : ENOENT);
		else if (err == ERROR_ACCESS_DENIED) {
			if (!win9x)
				pout("Administrator rights are necessary to access physical drives.\n");
			errno = EACCES;
		}
		else
			errno = EIO;
		h_ata_ioctl = 0;
		return -1;
	}

	if (con->reportataioctl > 1)
		pout("%s: successfully opened\n", devpath);

	// Set default options according to Windows version
	if (!ata_def_options)
		ata_def_options = ata_get_def_options();
	// Save user options
	if (port >= 0 && !*options)
		options = "s3"; // RAID: SMART_* and SCSI_MINIPORT
	assert(!ata_usr_options);
	if (*options)
		ata_usr_options = strdup(options);

	// NT4/2000/XP: SMART_GET_VERSION may spin up disk, so delay until first real SMART_* call
	if (phydrive >= 0) {
		ata_driveno = phydrive; ata_driveno_is_log = 0;
	}
	else {
		assert(logdrive >= 0);
		ata_driveno = logdrive; ata_driveno_is_log = 1;
	}
	if (!win9x && port < 0)
		return 0;

	// Win9X/ME: Get drive map
	// RAID: Get port map
	unsigned long portmap = 0;
	int devmap = smart_get_version(h_ata_ioctl, (port >= 0 ? &portmap : 0));
	if (devmap < 0) {
		if (!is_permissive()) {
			ata_close(0);
			errno = ENOSYS;
			return -1;
		}
		devmap = 0x0f;
	}
	ata_smartver_state[ata_driveno] = 1;

	if (port >= 0) {
		// 3ware RAID: update devicemap first
		if (!update_3ware_devicemap_ioctl(h_ata_ioctl)) {
			unsigned long portmap1 = 0;
			if (smart_get_version(h_ata_ioctl, &portmap1) >= 0)
				portmap = portmap1;
		}	
		// Check port existence
		if (!(portmap & (1L << port))) {
			pout("%s: Port %d is empty or does not exist\n", devpath, port);
			if (!is_permissive()) {
				ata_close(0);
				errno = ENOENT;
				return -1;
			}
		}
		// Encode port into pseudo fd
		return (ATARAID_FDOFFSET | port);
	}

	// Win9x/ME: Check device presence & type
	if (((devmap >> (ata_driveno & 0x3)) & 0x11) != 0x01) {
		unsigned char atapi = (devmap >> (ata_driveno & 0x3)) & 0x10;
		pout("%s: Drive %d %s (IDEDeviceMap=0x%02x).\n", devpath,
		     ata_driveno, (atapi?"is an ATAPI device":"does not exist"), devmap);
		// Win9x drive existence check may not work as expected
		// The atapi.sys driver incorrectly fills in the bIDEDeviceMap with 0x01
		// (The related KB Article Q196120 is no longer available)
		if (!is_permissive()) {
			ata_close(0);
			errno = (atapi ? ENOSYS : ENOENT);
			return -1;
		}
	}
	// Use drive number as fd for ioctl
	return (ata_driveno & 0x3);
}


static void ata_close(int /*fd*/)
{
	CloseHandle(h_ata_ioctl);
	h_ata_ioctl = 0;
	if (ata_usr_options) {
		free(ata_usr_options);
		ata_usr_options = 0;
	}
}


// Scan for ATA drives on Win9x/ME, fill bitmask of drives present, return #drives

static int ata_scan_win9x(unsigned long * drives)
{
	// Open device
	const char devpath[] = "\\\\.\\SMARTVSD";
	HANDLE h = CreateFileA(devpath, GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
	if (h == INVALID_HANDLE_VALUE) {
		if (con->reportataioctl > 1)
			pout(" %s: Open failed, Error=%ld\n", devpath, GetLastError());
		return 0; // SMARTVSD.VXD missing or no ATA devices
	}

	// Get drive map
	GETVERSIONOUTPARAMS vers; memset(&vers, 0, sizeof(vers));
	DWORD num_out;
	if (!DeviceIoControl(h, SMART_GET_VERSION,
		NULL, 0, &vers, sizeof(vers), &num_out, NULL)) {
		if (con->reportataioctl)
			pout(" %s: SMART_GET_VERSION failed, Error=%ld\n", devpath, GetLastError());
		CloseHandle(h);
		return 0; // Should not happen
	}
	CloseHandle(h);

	if (con->reportataioctl) {
		pout(" %s: SMART_GET_VERSION (%ld bytes):\n"
		     "  Vers = %d.%d, Caps = 0x%lx, DeviceMap = 0x%02x\n",
			devpath, num_out, vers.bVersion, vers.bRevision,
			vers.fCapabilities, vers.bIDEDeviceMap);
	}

	// Check ATA device presence, remove ATAPI devices
	drives[0] = (vers.bIDEDeviceMap & 0xf) & ~((vers.bIDEDeviceMap >> 4) & 0xf);
	return (drives[0]&1) + ((drives[0]>>1)&1) + ((drives[0]>>2)&1) + ((drives[0]>>3)&1);
}


// Scan for ATA drives, fill bitmask of drives present, return #drives

static int ata_scan(unsigned long * drives, int * rdriveno, unsigned long * rdrives)
{
	int cnt = 0;
	for (int i = 0; i <= 9; i++) {
		// Open device
		char devpath[30];
		snprintf(devpath, sizeof(devpath)-1, "\\\\.\\PhysicalDrive%d", i);
		HANDLE h = CreateFileA(devpath, GENERIC_READ|GENERIC_WRITE,
			FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		if (h == INVALID_HANDLE_VALUE) {
			if (con->reportataioctl > 1)
				pout(" %s: Open failed, Error=%ld\n", devpath, GetLastError());
			continue;
		}
		if (con->reportataioctl)
			pout(" %s: successfully opened\n", devpath);

		// Skip SCSI
		int type = get_controller_type(h);
		if (type == CONTROLLER_SCSI) { // ATA RAID may return CONTROLLER_UNKNOWN
			CloseHandle(h);
			continue;
		}

		// Try SMART_GET_VERSION
		GETVERSIONOUTPARAMS vers; memset(&vers, 0, sizeof(vers));
		const GETVERSIONINPARAMS_EX & vers_ex = (const GETVERSIONINPARAMS_EX &)vers;
		DWORD num_out;
		BOOL smart_ok = DeviceIoControl(h, SMART_GET_VERSION,
			NULL, 0, &vers, sizeof(vers), &num_out, NULL);
		if (con->reportataioctl) {
			if (!smart_ok)
				pout(" %s: SMART_GET_VERSION failed, Error=%ld\n", devpath, GetLastError());
			else {
				pout(" %s: SMART_GET_VERSION (%ld bytes):\n"
				     "  Vers = %d.%d, Caps = 0x%lx, DeviceMap = 0x%02x\n",
					devpath, num_out, vers.bVersion, vers.bRevision,
					vers.fCapabilities, vers.bIDEDeviceMap);
				if (vers_ex.wIdentifier == SMART_VENDOR_3WARE)
					pout("  Identifier = %04x(3WARE), ControllerId=%u, DeviceMapEx = 0x%08lx\n",
						vers_ex.wIdentifier, vers_ex.wControllerId, vers_ex.dwDeviceMapEx);
			}
		}
		CloseHandle(h);

		// If SMART_GET_VERSION failed, driver may support ATA_PASS_THROUGH instead
		if (!(smart_ok || type == CONTROLLER_ATA))
			continue;

		// Interpret RAID drive map if present
		if (smart_ok && vers_ex.wIdentifier == SMART_VENDOR_3WARE) {
			// Skip if more than 2 controllers or logical drive from this controller already seen
			if (vers_ex.wControllerId >= 2 || rdriveno[vers_ex.wControllerId] >= 0)
				continue;
			assert(rdrives[vers_ex.wControllerId] == 0);
			// Count physical drives
			int pcnt = 0;
			for (int pi = 0; pi < 32; pi++) {
				if (vers_ex.dwDeviceMapEx & (1L << pi))
					pcnt++;
			}
			if (!pcnt)
				continue; // Should not happen
			rdrives[vers_ex.wControllerId] = vers_ex.dwDeviceMapEx;
			rdriveno[vers_ex.wControllerId] = i;
			cnt += pcnt-1;
		}

		// Driver supports SMART_GET_VERSION or STORAGE_QUERY_PROPERTY returns ATA/SATA
		drives[0] |= (1L << i);
		cnt++;
	}

	return cnt;
}


/////////////////////////////////////////////////////////////////////////////

// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int fd, smart_command_set command, int select, char * data)
{
	if (fd == TW_CLI_FDOFFSET) // Parse tw_cli output
		return tw_cli_command_interface(command, select, data);

	int port = -1;
	if ((fd & ~0x1f) == ATARAID_FDOFFSET) {
		// RAID Port encoded into pseudo fd
		port = fd & 0x1f;
		fd = 0;
	}

	if (!(0 <= fd && fd <= 3)) {
		errno = EBADF;
		return -1;
	}

	// CMD,CYL default to SMART, changed by P?IDENTIFY and CHECK_POWER_MODE
	IDEREGS regs; memset(&regs, 0, sizeof(regs));
	regs.bCommandReg = ATA_SMART_CMD;
	regs.bCylHighReg = SMART_CYL_HI; regs.bCylLowReg = SMART_CYL_LOW;
	int datasize = 0;

	// Try by default: SMART_*, ATA_, IDE_, SCSI_PASS_THROUGH,
	// and SCSI_MINIPORT_* if requested by user
	const char * valid_options = (ata_usr_options ? "saicm" : "saic");

	switch (command) {
	  case CHECK_POWER_MODE:
		// Not a SMART command, needs IDE register return
		regs.bCommandReg = ATA_CHECK_POWER_MODE;
		regs.bCylLowReg = regs.bCylHighReg = 0;
		valid_options = "pai3"; // Try GetDevicePowerState() first, ATA/IDE_PASS_THROUGH may spin up disk
		// Note: returns SectorCountReg in data[0]
		break;
	  case READ_VALUES:
		regs.bFeaturesReg = ATA_SMART_READ_VALUES;
		regs.bSectorNumberReg = regs.bSectorCountReg = 1;
		datasize = 512;
		break;
	  case READ_THRESHOLDS:
		regs.bFeaturesReg = ATA_SMART_READ_THRESHOLDS;
		regs.bSectorNumberReg = regs.bSectorCountReg = 1;
		datasize = 512;
		break;
	  case READ_LOG:
		regs.bFeaturesReg = ATA_SMART_READ_LOG_SECTOR;
		regs.bSectorNumberReg = select;
		regs.bSectorCountReg = 1;
		// SMART_RCV_DRIVE_DATA supports this only on Win9x/ME
		// Try SCSI_MINIPORT also to skip buggy class driver
		valid_options = (ata_usr_options || is_win9x() ? "saicm3" : "aicm3");
		datasize = 512;
		break;
	  case WRITE_LOG:
		regs.bFeaturesReg = ATA_SMART_WRITE_LOG_SECTOR;
		regs.bSectorNumberReg = select;
		regs.bSectorCountReg = 1;
		// ATA_PASS_THROUGH, SCSI_MINIPORT, others don't support DATA_OUT
		// but SCSI_MINIPORT_* only if requested by user
		valid_options = (ata_usr_options ? "am" : "a");
		datasize = -512; // DATA_OUT!
		break;
	  case IDENTIFY:
		// Note: WinNT4/2000/XP return identify data cached during boot
		// (true for SMART_RCV_DRIVE_DATA and IOCTL_IDE_PASS_THROUGH)
		regs.bCommandReg = ATA_IDENTIFY_DEVICE;
		regs.bCylLowReg = regs.bCylHighReg = 0;
		regs.bSectorCountReg = 1;
		datasize = 512;
		break;
	  case PIDENTIFY:
		regs.bCommandReg = ATA_IDENTIFY_PACKET_DEVICE;
		regs.bCylLowReg = regs.bCylHighReg = 0;
		regs.bSectorCountReg = 1;
		datasize = 512;
		break;
	  case ENABLE:
		regs.bFeaturesReg = ATA_SMART_ENABLE;
		regs.bSectorNumberReg = 1;
		break;
	  case DISABLE:
		regs.bFeaturesReg = ATA_SMART_DISABLE;
		regs.bSectorNumberReg = 1;
		break;
	  case STATUS_CHECK:
		// Requires CL,CH register return
		valid_options = (ata_usr_options ? "saim" : "sai");
	  case STATUS:
		regs.bFeaturesReg = ATA_SMART_STATUS;
		break;
	  case AUTO_OFFLINE:
		regs.bFeaturesReg = ATA_SMART_AUTO_OFFLINE;
		regs.bSectorCountReg = select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
		break;
	  case AUTOSAVE:
		regs.bFeaturesReg = ATA_SMART_AUTOSAVE;
		regs.bSectorCountReg = select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
		break;
	  case IMMEDIATE_OFFLINE:
		regs.bFeaturesReg = ATA_SMART_IMMEDIATE_OFFLINE;
		regs.bSectorNumberReg = select;
		// SMART_SEND_DRIVE_COMMAND supports ABORT_SELF_TEST only on Win9x/ME
		valid_options = (ata_usr_options || select != 127/*ABORT*/ || is_win9x() ?
			"saicm3" : "aicm3");
		break;
	  default:
		pout("Unrecognized command %d in win32_ata_command_interface()\n"
		 "Please contact " PACKAGE_BUGREPORT "\n", command);
		errno = ENOSYS;
		return -1;
	}

	// Try all valid ioctls in the order specified in dev_ioctls;
	bool powered_up = false;
	assert(ata_def_options);
	const char * options = (ata_usr_options ? ata_usr_options : ata_def_options);
	for (int i = 0; ; i++) {
		char opt = options[i];

		if (!opt) {
			if (command == CHECK_POWER_MODE && powered_up) {
				// Power up reported by GetDevicePowerState() and no ioctl available
				// to detect the actual mode of the drive => simulate ATA result ACTIVE/IDLE.
				regs.bSectorCountReg = 0xff;
				break;
			}
			// No IOCTL found
			errno = ENOSYS;
			return -1;
		}
		if (!strchr(valid_options, opt))
			// Invalid for this command
			continue;

		errno = 0;
		assert(datasize == 0 || datasize == 512 || (strchr("am", opt) && datasize == -512));
		int rc;
		switch (opt) {
		  default: assert(0);
		  case 's':
			// call SMART_GET_VERSION once for each drive
			assert(0 <= ata_driveno && ata_driveno < sizeof(ata_smartver_state));
			if (ata_smartver_state[ata_driveno] > 1) {
				rc = -1; errno = ENOSYS;
				break;
			}
			if (!ata_smartver_state[ata_driveno]) {
				assert(port == -1);
				if (smart_get_version(h_ata_ioctl) < 0) {
					if (!con->permissive) {
						pout("ATA/SATA driver is possibly a SCSI driver not supporting SMART.\n");
						pout("If this is a SCSI disk, try '/dev/sd%c\'.\n", 'a'+ata_driveno);
						ata_smartver_state[ata_driveno] = 2;
						rc = -1; errno = ENOSYS;
						break;
					}
					con->permissive--;
				}
				ata_smartver_state[ata_driveno] = 1;
			}
			rc = smart_ioctl(h_ata_ioctl, fd, &regs, data, datasize, port);
			break;
		  case 'm':
			rc = ata_via_scsi_miniport_smart_ioctl(h_ata_ioctl, &regs, data, datasize);
			break;
		  case 'a':
			rc = ata_pass_through_ioctl(h_ata_ioctl, &regs, data, datasize);
			break;
		  case 'i':
			rc = ide_pass_through_ioctl(h_ata_ioctl, &regs, data, datasize);
			break;
		  case 'c':
			rc = ata_via_scsi_pass_through_ioctl(h_ata_ioctl, &regs, data, datasize);
			break;
		  case '3':
			rc = ata_via_3ware_miniport_ioctl(h_ata_ioctl, &regs, data, datasize, port);
			break;
		  case 'p':
			assert(command == CHECK_POWER_MODE && datasize == 0);
			rc = get_device_power_state(h_ata_ioctl);
			if (rc == 0) {
				// Power down reported by GetDevicePowerState(), using a passthrough ioctl would
				// spin up the drive => simulate ATA result STANDBY.
				regs.bSectorCountReg = 0x00;
			}
			else if (rc > 0) {
				// Power up reported by GetDevicePowerState(), but this reflects the actual mode
				// only if it is selected by the device driver => try a passthrough ioctl to get the
				// actual mode, if none available simulate ACTIVE/IDLE.
				powered_up = true;
				errno = ENOSYS; rc = -1;
			}
			break;
		}

		if (!rc)
			// Working ioctl found
			break;

		if (errno != ENOSYS)
			// Abort on I/O error
			return -1;

		// CAUTION: *_ioctl() MUST NOT change "regs" Parameter in the ENOSYS case
	}

	switch (command) {
	  case CHECK_POWER_MODE:
		// Return power mode from SectorCountReg in data[0]
		data[0] = regs.bSectorCountReg;
		return 0;

	  case STATUS_CHECK:
		// Cyl low and Cyl high unchanged means "Good SMART status"
		if (regs.bCylHighReg == SMART_CYL_HI && regs.bCylLowReg == SMART_CYL_LOW)
		  return 0;

		// These values mean "Bad SMART status"
		if (regs.bCylHighReg == 0x2c && regs.bCylLowReg == 0xf4)
		  return 1;

		// We haven't gotten output that makes sense; print out some debugging info
		syserror("Error SMART Status command failed");
		pout("Please get assistance from %s\n", PACKAGE_HOMEPAGE);
		print_ide_regs(&regs, 1);
		errno = EIO;
		return -1;

	  default:
		return 0;
	}
	/*NOTREACHED*/
}


#ifndef HAVE_ATA_IDENTIFY_IS_CACHED
#error define of HAVE_ATA_IDENTIFY_IS_CACHED missing in config.h
#endif

// Return true if OS caches the ATA identify sector
int ata_identify_is_cached(int fd)
{
	// Not RAID and WinNT4/2000/XP => true, RAID or Win9x/ME => false
	return (!(fd & 0xff00) && !is_win9x());
}


// Print not implemeted warning once
static void pr_not_impl(const char * what, int * warned)
{
	if (*warned)
		return;
	pout(
		"#######################################################################\n"
		"%s\n"
		"NOT IMPLEMENTED under Win32.\n"
		"Please contact " PACKAGE_BUGREPORT " if\n"
		"you want to help in porting smartmontools to Win32.\n"
		"#######################################################################\n"
		"\n", what
	);
	*warned = 1;
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c
int escalade_command_interface(int /*fd*/, int disknum, int /*escalade_type*/, smart_command_set /*command*/, int /*select*/, char * /*data*/)
{
	static int warned = 0;
	if (!warned) {
		pout("Option '-d 3ware,%d' does not work on Windows.\n"
		     "Controller port can be specified in the device name: '/dev/hd%c,%d'.\n\n",
			disknum, 'a'+ata_driveno, disknum);
		warned = 1;
	}
	errno = ENOSYS;
	return -1;
}

// Interface to ATA devices behind Marvell chip-set based controllers.  See os_linux.c
int marvell_command_interface(int /*fd*/, smart_command_set /*command*/, int /*select*/, char * /*data*/)
{
	static int warned = 0;
	pr_not_impl("Marvell chip-set command routine marvell_command_interface()", &warned);
	errno = ENOSYS;
	return -1;
}

// Interface to ATA devices behind HighPoint Raid controllers.  See os_linux.c
int highpoint_command_interface(int /*fd*/, smart_command_set /*command*/, int /*select*/, char * /*data*/)
{
	static int warned = 0;
	pr_not_impl("HighPoint raid controller command routine highpoint_command_interface()", &warned);
	errno = ENOSYS;
	return -1;
}


/////////////////////////////////////////////////////////////////////////////
// ASPI Interface (for SCSI devices)
/////////////////////////////////////////////////////////////////////////////

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


static int aspi_open(unsigned adapter, unsigned id)
{
	ASPI_SRB srb;
	if (!(adapter <= 9 && id < 16)) {
		errno = ENOENT;
		return -1;
	}

	if (!aspi_entry_valid()) {
		if (aspi_open_dll(1/*verbose*/))
			return -1;
	}

	// Adapter OK?
	if (adapter >= num_aspi_adapters) {
		pout("ASPI Adapter %u does not exist (%u Adapter%s detected).\n",
			adapter, num_aspi_adapters, (num_aspi_adapters!=1?"s":""));
		if (!is_permissive()) {
			errno = ENOENT;
			return -1;
		}
	}

	// Device present ?
	memset(&srb, 0, sizeof(srb));
	srb.h.cmd = ASPI_CMD_GET_DEVICE_TYPE;
	srb.h.adapter = adapter; srb.i.target_id = id;
	if (aspi_call(&srb)) {
		errno = EIO;
		return -1;
	}
	if (srb.h.status != ASPI_STATUS_NO_ERROR) {
		pout("ASPI Adapter %u, ID %u: No such device (Status=0x%02x)\n", adapter, id, srb.h.status);
		if (!is_permissive()) {
			errno = (srb.h.status == ASPI_STATUS_INVALID_TARGET ? ENOENT : EIO);
			return -1;
		}
	}
	else if (con->reportscsiioctl)
		pout("ASPI Adapter %u, ID %u: Device Type=0x%02x\n", adapter, id, srb.t.devtype);

	return (ASPI_FDOFFSET | ((adapter & 0xf)<<4) | (id & 0xf));
}


static void aspi_close(int /*fd*/)
{
	// No FreeLibrary(h_aspi_dll) to prevent problems with ASPI threads
}


// Scan for SCSI drives, fill bitmask [adapter:0-9][id:0-7] of drives present,
// return #drives

static int aspi_scan(unsigned long * drives)
{
	int cnt = 0;
	unsigned ad;

	if (!aspi_entry_valid()) {
		if (aspi_open_dll(con->reportscsiioctl/*default is quiet*/))
			return 0;
	}

	for (ad = 0; ad < num_aspi_adapters; ad++) {
		ASPI_SRB srb; unsigned id;

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
			return 0;

		if (srb.h.status != ASPI_STATUS_NO_ERROR) {
			if (con->reportscsiioctl)
				pout(" ASPI Adapter %u: Status=0x%02x\n", ad, srb.h.status);
			continue;
		}

		if (con->reportscsiioctl) {
			int i;
			for (i = 1; i < 16 && srb.q.adapter_id[i]; i++)
				if (!(' ' <= srb.q.adapter_id[i] && srb.q.adapter_id[i] <= '~'))
					srb.q.adapter_id[i] = '?';
			pout(" ASPI Adapter %u (\"%.16s\"):\n", ad, srb.q.adapter_id);
		}

		bool ignore = !strnicmp(srb.q.adapter_id, "3ware", 5);

		for (id = 0; id <= 7; id++) {
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
				drives[ad >> 2] |= (1L << (((ad & 0x3) << 3) + id));
				cnt++;
			}
			else if (con->reportscsiioctl)
				pout("  ID %u: Device Type=0x%02x (ignored)\n", id, srb.t.devtype);
		}
	}
	return cnt;
}


/////////////////////////////////////////////////////////////////////////////

// Interface to ASPI SCSI devices.  See scsicmds.h and os_linux.c
static int do_aspi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report)
{
	ASPI_SRB srb;

	if (!aspi_entry_valid())
		return -EBADF;
	if (!((fd & ~0xff) == ASPI_FDOFFSET))
		return -EBADF;

	if (!(iop->cmnd_len == 6 || iop->cmnd_len == 10 || iop->cmnd_len == 12 || iop->cmnd_len == 16)) {
		pout("do_aspi_cmnd_io: bad CDB length\n");
		return -EINVAL;
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

	memset(&srb, 0, sizeof(srb));
	srb.h.cmd = ASPI_CMD_EXECUTE_IO;
	srb.h.adapter = ((fd >> 4) & 0xf);
	srb.i.target_id = (fd & 0xf);
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
			pout("do_aspi_cmnd_io: bad dxfer_dir\n");
			return -EINVAL;
	}

	iop->resp_sense_len = 0;
	iop->scsi_status = 0;
	iop->resid = 0;

	if (aspi_io_call(&srb, (iop->timeout ? iop->timeout : 60))) {
		// Timeout
		return -EIO;
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
			return 0;
		}
		else {
			if (report)
				pout("  ASPI call failed, (0x%02x,0x%02x,0x%02x)\n", srb.h.status, srb.i.host_status, srb.i.target_status);
			return -EIO;
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

	return 0;
}


/////////////////////////////////////////////////////////////////////////////
// SPT Interface (for SCSI devices and ATA devices behind SATLs)
// Only supported in NT and later
/////////////////////////////////////////////////////////////////////////////

#define SPT_MAXDEV 64

struct spt_dev_info {
	HANDLE h_spt_ioctl;
	int   sub_addr;		// addressing disks within a RAID, for example
};

// Private table of open devices: guaranteed zero on startup since
// part of static data.
static struct spt_dev_info * spt_dev_arr[SPT_MAXDEV];


static int spt_open(int pd_num, int ld_num, int tape_num, int sub_addr)
{
	int k;
	struct spt_dev_info * sdip;
	char b[128];
	HANDLE h;

	for (k = 0; k < SPT_MAXDEV; k++)
		if (! spt_dev_arr[k])
			break;

	// If no free entry found, return error.  We have max allowed number
	// of "file descriptors" already allocated.
	if (k == SPT_MAXDEV) {
		if (con->reportscsiioctl)
			pout("spt_open: too many open file descriptors (%d)\n",
			     SPT_MAXDEV);
		errno = EMFILE;
		return -1;
	}
	sdip = (struct spt_dev_info *)malloc(sizeof(struct spt_dev_info));
	if (NULL == sdip) {
		errno = ENOMEM;
		return -1;
	}
	spt_dev_arr[k] = sdip;
	sdip->sub_addr = sub_addr;

	b[sizeof(b) - 1] = '\0';
	if (pd_num >= 0)
		snprintf(b, sizeof(b) - 1, "\\\\.\\PhysicalDrive%d", pd_num);
	else if (ld_num >= 0)
		snprintf(b, sizeof(b) - 1, "\\\\.\\%c:", 'A' + ld_num);
	else if (tape_num >= 0)
		snprintf(b, sizeof(b) - 1, "\\\\.\\TAPE%d", tape_num);
	else {
		if (con->reportscsiioctl)
			pout("spt_open: bad parameters\n");
		errno = EINVAL;
		goto err_out;
	}

	// Open device
	if ((h = CreateFileA(b, GENERIC_READ|GENERIC_WRITE,
			     FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
			     OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE) {
		if (con->reportscsiioctl)
			pout(" %s: Open failed, Error=%ld\n", b, GetLastError());
		errno = ENODEV;
		goto err_out;
	}
	sdip->h_spt_ioctl = h;
	return k + SPT_FDOFFSET;

err_out:
	spt_dev_arr[k] = NULL;
	free(sdip);
	return -1;
}


static void spt_close(int fd)
{
	struct spt_dev_info * sdip;
	int index = fd - SPT_FDOFFSET;

	if ((index < 0) || (index >= SPT_MAXDEV)) {
		if (con->reportscsiioctl)
			pout("spt_close: bad fd range\n");
		return;
	}
	sdip = spt_dev_arr[index];
	if (NULL == sdip) {
		if (con->reportscsiioctl)
			pout("spt_close: fd already closed\n");
		return;
	}
	free(sdip);
	spt_dev_arr[index] = NULL;
}


static int spt_scan(unsigned long * drives)
{
	int cnt = 0;
	for (int i = 0; i <= 9; i++) {
		if (get_controller_type(i, -1) != CONTROLLER_SCSI)
			continue;
		// STORAGE_QUERY_PROPERTY returned SCSI/SAS/...
		drives[0] |= (1L << i);
		cnt++;
	}
	return cnt;
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


// Interface to SPT SCSI devices.  See scsicmds.h and os_linux.c
static int do_spt_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report)
{
	struct spt_dev_info * sdip;
	int index = fd - SPT_FDOFFSET;
	SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER sb;
	DWORD num_out;

	if ((index < 0) || (index >= SPT_MAXDEV)) {
		if (report)
			pout("do_spt_cmnd_io: bad fd range\n");
		return -EBADF;
	}
	sdip = spt_dev_arr[index];
	if (NULL == sdip) {
		if (report)
			pout("do_spt_cmnd_io: fd already closed\n");
		return -EBADF;
	}

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
	if (iop->cmnd_len > (int)sizeof(sb.spt.Cdb)) {
		if (report)
			pout("do_spt_cmnd_io: cmnd_len too large\n");
		return -EINVAL;
	}

	memset(&sb, 0, sizeof(sb));
	sb.spt.Length = sizeof(SCSI_PASS_THROUGH_DIRECT);
	sb.spt.CdbLength = iop->cmnd_len;
	memcpy(sb.spt.Cdb, iop->cmnd, iop->cmnd_len);
	sb.spt.SenseInfoLength = sizeof(sb.ucSenseBuf);
	sb.spt.SenseInfoOffset =
		offsetof(SCSI_PASS_THROUGH_DIRECT_WITH_BUFFER, ucSenseBuf);
	sb.spt.TimeOutValue = (iop->timeout ? iop->timeout : 60);
	switch (iop->dxfer_dir) {
		case DXFER_NONE:
			sb.spt.DataIn = SCSI_IOCTL_DATA_UNSPECIFIED;
			break;
		case DXFER_FROM_DEVICE:
			sb.spt.DataIn = SCSI_IOCTL_DATA_IN;
			sb.spt.DataTransferLength = iop->dxfer_len;
			sb.spt.DataBuffer = iop->dxferp;
			break;
		case DXFER_TO_DEVICE:
			sb.spt.DataIn = SCSI_IOCTL_DATA_OUT;
			sb.spt.DataTransferLength = iop->dxfer_len;
			sb.spt.DataBuffer = iop->dxferp;
			break;
		default:
			pout("do_spt_cmnd_io: bad dxfer_dir\n");
			return -EINVAL;
	}

	if (! DeviceIoControl(sdip->h_spt_ioctl, IOCTL_SCSI_PASS_THROUGH_DIRECT,
		&sb, sizeof(sb), &sb, sizeof(sb), &num_out, NULL)) {
		long err = GetLastError();

		if (report)
			pout("  IOCTL_SCSI_PASS_THROUGH_DIRECT failed, Error=%ld\n", err);
		return -(err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
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
	return 0;
}


// Decides which SCSI implementation based on pseudo fd.
// Declaration and explanation in scsicmds.h
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report)
{
	if ((fd & ~0xff) == ASPI_FDOFFSET)
		return do_aspi_cmnd_io(fd, iop, report);
	else
		return do_spt_cmnd_io(fd, iop, report);
}
