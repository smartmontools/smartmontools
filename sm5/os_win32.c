/*
 * os_win32.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004 Christian Franke <smartmontools-support@lists.sourceforge.net>
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
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 *
 */

#include "config.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"
#include "extern.h"
extern smartmonctrl * con; // con->permissive

#include <errno.h>
#include <assert.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h> // offsetof()


// Needed by '-V' option (CVS versioning) of smartd/smartctl
const char *os_XXXX_c_cvsid="$Id: os_win32.c,v 1.2 2004/02/25 13:44:13 chrfranke Exp $" \
ATACMDS_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID EXTERN_H_CVSID;


static int ata_open(int drive);
static void ata_close(int fd);
static int aspi_open(unsigned adapter, unsigned id);
static void aspi_close(int fd);


static int is_permissive()
{
	if (con->permissive <= 0) {
		pout("To continue, add one or more '-T permissive' options.\n");
		return 0;
	}
	con->permissive--;
	return 1;
}

static const char * skipdev(const char * s)
{
	return (!strncmp(s, "/dev/", 5) ? s + 5 : s);
}


// tries to guess device type given the name (a path).  See utility.h
// for return values.
int guess_device_type (const char * dev_name)
{
	dev_name = skipdev(dev_name);
	if (!strncmp(dev_name, "hd", 2))
		return GUESS_DEVTYPE_ATA;
	if (!strncmp(dev_name, "scsi", 4))
		return GUESS_DEVTYPE_SCSI;
	return GUESS_DEVTYPE_DONT_KNOW;
}


// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number N of devices, or -1 if out of
// memory. Allocates N+1 arrays: one of N pointers (devlist), the
// others each contain null-terminated character strings.
int make_device_names (char*** devlist, const char* name)
{
	return 0; // TODO!
}



// Like open().  Return positive integer handle, only used by
// functions below.  type="ATA" or "SCSI".  If you need to store extra
// information about your devices, create a private internal array
// within this file (see os_freebsd.c for an example).
int deviceopen(const char * pathname, char *type)
{
	int len;
	pathname = skipdev(pathname);
	len = strlen(pathname);

	if (!strcmp(type, "ATA")) {
		// hd[a-z] => ATA 0-9
		if (!(len  == 3 && pathname[0] == 'h' && pathname[1] == 'd'
			  && 'a' <= pathname[2] && pathname[2] <= 'j')) {
			errno = ENOENT;
			return -1;
		}
		return ata_open(pathname[2] - 'a');
	}

	if (!strcmp(type, "SCSI")) {
		// scsi[0-9][0-f] => SCSI Adapter 0-9, ID 0-15, LUN 0
		unsigned adapter = ~0, id = ~0; int n = -1;
		if (!(sscanf(pathname,"scsi%1u%1x%n", &adapter, &id, &n) == 2 && n == len)) {
			errno = ENOENT;
			return -1;
		}
		return aspi_open(adapter, id);
	}
	errno = ENOENT;
	return -1;
}


// Like close().  Acts only on handles returned by above function.
// (Never called in smartctl!)
int deviceclose(int fd)
{
	if (fd < 0x100) {
		ata_close(fd);
	}
	else {
		aspi_close(fd);
	}
	return 0;
}


/////////////////////////////////////////////////////////////////////////////
// ATA Interface
/////////////////////////////////////////////////////////////////////////////

// SMART_* IOCTLs, also known as DFP_* (Disk Fault Protection)

// Deklarations from:
// http://cvs.sourceforge.net/viewcvs.py/mingw/w32api/include/ddk/ntdddisk.h?rev=1.3

#define FILE_READ_ACCESS       0x0001
#define FILE_WRITE_ACCESS      0x0002
#define METHOD_BUFFERED             0
#define CTL_CODE(DeviceType, Function, Method, Access) (((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method))

#define FILE_DEVICE_DISK	   7
#define IOCTL_DISK_BASE        FILE_DEVICE_DISK

#define SMART_GET_VERSION \
  CTL_CODE(IOCTL_DISK_BASE, 0x0020, METHOD_BUFFERED, FILE_READ_ACCESS)

#define SMART_RCV_DRIVE_DATA \
  CTL_CODE(IOCTL_DISK_BASE, 0x0022, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#define SMART_SEND_DRIVE_COMMAND \
  CTL_CODE(IOCTL_DISK_BASE, 0x0021, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

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

#pragma pack()


/////////////////////////////////////////////////////////////////////////////

static void print_ide_regs(const IDEREGS * r, int out)
{
	pout("%s=0x%02x,%s=0x%02x, SC=0x%02x, NS=0x%02x, CL=0x%02x, CH=0x%02x, SEL=0x%02x\n",
	(out?"STS":"CMD"), r->bCommandReg, (out?"ERR":" FR"), r->bFeaturesReg,
	r->bSectorCountReg, r->bSectorNumberReg, r->bCylLowReg, r->bCylHighReg, r->bDriveHeadReg);
}


/////////////////////////////////////////////////////////////////////////////

// call SMART_* ioctl

static int smart_ioctl(HANDLE hdevice, int drive, IDEREGS * regs, char * data, unsigned datasize)
{
	SENDCMDINPARAMS inpar;
	unsigned char outbuf[sizeof(SENDCMDOUTPARAMS)-1 + 512];
	const SENDCMDOUTPARAMS * outpar;
	DWORD code, num_out;
	unsigned int size_out;

	assert(SMART_SEND_DRIVE_COMMAND == 0x07c084);
	assert(SMART_RCV_DRIVE_DATA == 0x07c088);
	assert(sizeof(SENDCMDINPARAMS)-1 == 32);
	assert(sizeof(SENDCMDOUTPARAMS)-1 == 16);

	memset(&inpar, 0, sizeof(inpar));
	inpar.irDriveRegs = *regs;
	// drive is set to 0-3 on Win9x only
	inpar.irDriveRegs.bDriveHeadReg = 0xA0 | ((drive & 1) << 4);
	inpar.bDriveNumber = drive;

	assert(datasize == 0 || datasize == 512);
	if (datasize) {
		inpar.cBufferSize = size_out = 512;
		code = SMART_RCV_DRIVE_DATA;
	}
	else if (regs->bFeaturesReg == SMART_STATUS) {
		size_out = sizeof(IDEREGS); // ioctl returns new IDEREGS as data
		code = SMART_SEND_DRIVE_COMMAND;
	}
	else {
		size_out = 0;
		code = SMART_SEND_DRIVE_COMMAND;
	}

	memset(&outbuf, 0, sizeof(outbuf));

#ifdef _DEBUG
	pout("DeviceIoControl(.,0x%lx,.,%lu,.,%lu,.,NULL)\n",
		code, sizeof(SENDCMDINPARAMS)-1, sizeof(SENDCMDOUTPARAMS)-1 + size_out);
	print_ide_regs(&inpar.irDriveRegs, 0);
#endif
	if (!DeviceIoControl(hdevice, code,
             		&inpar, sizeof(SENDCMDINPARAMS)-1,
               		outbuf, sizeof(SENDCMDOUTPARAMS)-1 + size_out,
               		&num_out, NULL)) {
		// CAUTION: DO NOT change "regs" Parameter in this case, see ata_command_interface()
		long err = GetLastError();
#ifdef _DEBUG
		pout("DeviceIoControl failed, Error=%ld\n", err);
#endif
		errno = (   err == ERROR_INVALID_FUNCTION /*9x*/
		         || err == ERROR_INVALID_PARAMETER/*NT/2K/XP*/ ? ENOSYS : EIO);
		return -1;
	}
	// NOTE: On Win9x, inpar.irDriveRegs now contains the returned regs

	outpar = (const SENDCMDOUTPARAMS *)outbuf;
#ifdef _DEBUG
	pout("DeviceIoControl returns %lu (%lu) bytes\n", num_out, outpar->cBufferSize);
#endif

	if (outpar->DriverStatus.bDriverError) {
		pout("Error SMART IOCTL DriverError=0x%02x, IDEError=0x%02x\n",
			outpar->DriverStatus.bDriverError, outpar->DriverStatus.bIDEError);
		errno = EIO;
		return -1;
	}

	if (datasize)
		memcpy(data, outpar->bBuffer, 512);
	else if (regs->bFeaturesReg == SMART_STATUS) {
		*regs = *(const IDEREGS *)(outpar->bBuffer);
#ifdef _DEBUG
		print_ide_regs(regs, 1);
#endif
	}

	return 0;
}


/////////////////////////////////////////////////////////////////////////////

// IDE PASS THROUGH for W2K/XP (does not work on W9x/NT4)
// Only used for SMART commands not supported by SMART_* IOCTLs
//
// Based on WinATA.cpp, 2002 c't/Matthias Withopf
// ftp://ftp.heise.de/pub/ct/listings/0207-218.zip

#define FILE_DEVICE_CONTROLLER  4
#define IOCTL_SCSI_BASE         FILE_DEVICE_CONTROLLER

#define IOCTL_IDE_PASS_THROUGH \
  CTL_CODE(IOCTL_SCSI_BASE, 0x040A, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

#pragma pack(1)

typedef struct {
	IDEREGS IdeReg;
	ULONG   DataBufferSize;
	UCHAR   DataBuffer[1];
} ATA_PASS_THROUGH;

#pragma pack()


/////////////////////////////////////////////////////////////////////////////

static int ide_pass_through_ioctl(HANDLE hdevice, IDEREGS * regs, char * data, unsigned datasize)
{ 
	unsigned int size = sizeof(ATA_PASS_THROUGH)-1 + datasize;
	ATA_PASS_THROUGH * buf = (ATA_PASS_THROUGH *)VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
	DWORD num_out;
	assert(sizeof(ATA_PASS_THROUGH)-1 == 12);
	assert(IOCTL_IDE_PASS_THROUGH == 0x04d028);

	buf->IdeReg = *regs;
	buf->DataBufferSize = datasize;

#ifdef _DEBUG
	pout("DeviceIoControl(.,0x%x,.,%u,.,%u,.,NULL)\n",
		IOCTL_IDE_PASS_THROUGH, size, size);
	print_ide_regs(&buf->IdeReg, 0);
#endif

	if (!DeviceIoControl(hdevice, IOCTL_IDE_PASS_THROUGH, 
		buf, size, buf, size, &num_out, NULL)) {
		long err = GetLastError();
#ifdef _DEBUG
		pout("DeviceIoControl failed, Error=%ld\n", err);
#endif
		VirtualFree(buf, size, MEM_RELEASE);
		errno = (err == ERROR_INVALID_FUNCTION ? ENOSYS : EIO);
		return -1;
	}

#ifdef _DEBUG
	pout("DeviceIoControl returns %lu bytes\n", num_out);
	print_ide_regs(&buf->IdeReg, 1);
#endif

	if (datasize)
		memcpy(data, buf->DataBuffer, datasize);
	VirtualFree(buf, size, MEM_RELEASE);
	return 0;
}


/////////////////////////////////////////////////////////////////////////////

static HANDLE h_ata_ioctl = 0;

static int ata_open(int drive)
{
	int win9x;
	char devpath[30];
	GETVERSIONOUTPARAMS vers;
	DWORD num_out;

	assert(SMART_GET_VERSION == 0x074080);
	assert(sizeof(GETVERSIONOUTPARAMS) == 24);

	// TODO: This version does not allow to open more than 1 ATA devices
	if (h_ata_ioctl) {
		errno = ENFILE;
		return -1;
	}

	win9x = ((GetVersion() & 0x80000000) != 0);

	if (!(0 <= drive && drive <= (win9x ? 3 : 9))) {
		errno = ENOENT;
		return -1;
	}
	// path depends on Windows Version
	if (win9x)
		strcpy(devpath, "\\\\.\\SMARTVSD");
	else
		snprintf(devpath, sizeof(devpath)-1, "\\\\.\\PhysicalDrive%d", drive);

	// Open device
	if ((h_ata_ioctl = CreateFile(devpath,
		GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, 0, 0)) == INVALID_HANDLE_VALUE) {
		pout("Cannot open device %s, Error=%ld\n", devpath, GetLastError());
		if (win9x)
			pout("Possibly missing SMARTVSD.VXD in Windows directory SYSTEM\\IOSUBSYS.\n");
		h_ata_ioctl = 0;
		errno = ENOENT;
		return -1;
	}

	// Get drive map
	memset(&vers, 0, sizeof(vers));
	if (!DeviceIoControl(h_ata_ioctl, SMART_GET_VERSION,
		NULL, 0, &vers, sizeof(vers), &num_out, NULL)) {
		pout("%s: SMART_GET_VERSION failed, Error=%ld\n", devpath, GetLastError());
		if (!win9x)
			pout("If this is a SCSI disk, try \"scsi<adapter><id>\".\n");
		if (!is_permissive()) {
			CloseHandle(h_ata_ioctl); h_ata_ioctl = 0;
			errno = ENOSYS;
			return -1;
		}
	}

#ifdef _DEBUG
	pout("SMART_GET_VERSION (%ld bytes): Vers = %d.%d, Caps = 0x%lx, DeviceMap = 0x%02x\n",
		num_out, vers.bVersion, vers.bRevision, vers.fCapabilities, vers.bIDEDeviceMap);
#endif

	// TODO: Check vers.fCapabilities here?

	if (!win9x)
		// NT4/2K/XP: Drive exists, Drive number not necessary for ioctl
		return 0;

	// Win9x/ME: Check device presence & type
	if (((vers.bIDEDeviceMap >> drive) & 0x11) != 0x01) {
		unsigned char atapi = (vers.bIDEDeviceMap >> drive) & 0x10;
		pout((  atapi
		      ? "Drive %d is an ATAPI device (IDEDeviceMap=0x%02x).\n"
			  : "Drive %d does not exist (IDEDeviceMap=0x%02x).\n"),
			drive, vers.bIDEDeviceMap);
		// Win9x Drive existence check may not work as expected
		// The atapi.sys driver incorrectly fills in the bIDEDeviceMap with 0x01
		// http://support.microsoft.com/support/kb/articles/Q196/1/20.ASP
		if (!is_permissive()) {
			CloseHandle(h_ata_ioctl); h_ata_ioctl = 0;
			errno = (atapi ? ENOSYS : ENOENT);
			return -1;
		}
	}
	// Use drive number as fd for ioctl
	return drive;
}


static void ata_close(int fd)
{
	CloseHandle(h_ata_ioctl);
	h_ata_ioctl = 0;
}


/////////////////////////////////////////////////////////////////////////////

// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int fd, smart_command_set command, int select, char * data)
{
	IDEREGS regs;
	int copydata;

	if (!(0 <= fd && fd <= 3)) {
	  errno = EBADF;
	  return -1;
	}

	// CMD,CYL default to SMART, changed by P?IDENTIFY
	memset(&regs, 0, sizeof(regs));
	regs.bCommandReg = WIN_SMART;
	regs.bCylHighReg = SMART_CYL_HI; regs.bCylLowReg = SMART_CYL_LOW;
	copydata = 0;

	switch (command) {
	  case READ_VALUES:
		regs.bFeaturesReg = SMART_READ_VALUES;
		regs.bSectorNumberReg = regs.bSectorCountReg = 1;
		copydata = 1;
		break;
	  case READ_THRESHOLDS:
		regs.bFeaturesReg = SMART_READ_THRESHOLDS;
		regs.bSectorNumberReg = regs.bSectorCountReg = 1;
		copydata = 1;
		break;
	  case READ_LOG:
		regs.bFeaturesReg = SMART_READ_LOG_SECTOR;
		regs.bSectorNumberReg = select;
		regs.bSectorCountReg = 1;
		copydata = 1;
		break;
	  case IDENTIFY:
		regs.bCommandReg = WIN_IDENTIFY;
		regs.bCylLowReg = regs.bCylHighReg = 0;
		regs.bSectorCountReg = 1;
		copydata = 1;
		break;
	  case PIDENTIFY:
		regs.bCommandReg = WIN_PIDENTIFY;
		regs.bCylLowReg = regs.bCylHighReg = 0;
		regs.bSectorCountReg = 1;
		copydata = 1;
		break;
	  case ENABLE:
		regs.bFeaturesReg = SMART_ENABLE;
		regs.bSectorNumberReg = 1;
		break;
	  case DISABLE:
		regs.bFeaturesReg = SMART_DISABLE;
		regs.bSectorNumberReg = 1;
		break;
	  case STATUS:
	  case STATUS_CHECK:
		regs.bFeaturesReg = SMART_STATUS;
		break;
	  case AUTO_OFFLINE:
		regs.bFeaturesReg = SMART_AUTO_OFFLINE;
		regs.bSectorCountReg = select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
		break;
	  case AUTOSAVE:
		regs.bFeaturesReg = SMART_AUTOSAVE;
		regs.bSectorCountReg = select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
		break;
	  case IMMEDIATE_OFFLINE:
		regs.bFeaturesReg = SMART_IMMEDIATE_OFFLINE;
		regs.bSectorNumberReg = select;
		break;
	  default:
		pout("Unrecognized command %d in win32_ata_command_interface()\n"
		 "Please contact " PACKAGE_BUGREPORT "\n", command);
		errno = ENOSYS;
		return -1;
	}

	if (smart_ioctl(h_ata_ioctl, fd, &regs, data, (copydata?512:0))) {
		// Read log only supported on Win9x, retry with pass through command
		// CAUTION: smart_ioctl() MUST NOT change "regs" Parameter in this case
		if (errno == ENOSYS && command == READ_LOG) {
			errno = 0;
			return ide_pass_through_ioctl(h_ata_ioctl, &regs, data, 512);
		}
		return -1;
	}

	if (command == STATUS_CHECK) {
		// Cyl low and Cyl high unchanged means "Good SMART status"
		if (regs.bCylHighReg == SMART_CYL_HI && regs.bCylLowReg == SMART_CYL_LOW)
		  return 0;

		// These values mean "Bad SMART status"
		if (regs.bCylHighReg == 0x2c && regs.bCylLowReg == 0xf4)
		  return 1;

		// We haven't gotten output that makes sense; print out some debugging info
		syserror("Error SMART Status command failed");
		pout("Please get assistance from %s\n",PROJECTHOME);
		print_ide_regs(&regs, 1);
		errno = EIO;
		return -1;
	}

	return 0;
}


// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c
int escalade_command_interface(int fd, int disknum, smart_command_set command, int select, char *data)
{
	static int warned = 0;
	if (!warned) {
		  pout(
		"#######################################################################\n"
		"3ware Escalade Controller command routine escalade_command_interface()\n"
		"NOT IMPLEMENTED under Win32.\n"
		"Please contact " PACKAGE_BUGREPORT " if\n"
		"you want to help in porting smartmontools to Win32.\n"
		"#######################################################################\n"
		"\n");
		warned = 1;
	}
	errno = ENOSYS;
	return -1;
}


/////////////////////////////////////////////////////////////////////////////
// ASPI Interface
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


static int aspi_open_dll()
{
	ASPI_SRB srb;

	// Check structure layout
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

	// Get ASPI entrypoint from winaspi.dll
	if (!h_aspi_dll && !(h_aspi_dll = LoadLibrary("WNASPI32.DLL"))) {
		pout("Cannot Load WNASPI32.DLL, Error=%ld\n", GetLastError());
		h_aspi_dll = INVALID_HANDLE_VALUE;
		errno = ENOENT;
		return -1;
	}

	if (!((FARPROC)aspi_entry = GetProcAddress(h_aspi_dll, "SendASPI32Command"))) {
		pout("Missing SendASPI32Command() in WNASPI32.DLL\n");
		FreeLibrary(h_aspi_dll); h_aspi_dll = INVALID_HANDLE_VALUE;
		errno = ENOENT;
		return -1;
	}

	// Get number of adapters
	memset(&srb, 0, sizeof(srb));
	srb.h.cmd = ASPI_CMD_ADAPTER_INQUIRE;
	aspi_entry(&srb);
	while (((volatile ASPI_SRB *)&srb)->h.status == ASPI_STATUS_IN_PROGRESS) {
		// TODO: Timeout!
		Sleep(1);
	}
	if (srb.h.status != ASPI_STATUS_NO_ERROR) {
		pout("ASPI Adapter Inquriy failed, Error=0x%02x\n", srb.h.status);
		if (!is_permissive()) {
			aspi_entry = 0;
			FreeLibrary(h_aspi_dll); h_aspi_dll = INVALID_HANDLE_VALUE;
			errno = ENOSYS;
			return -1;
		}
		srb.q.adapters = 10;
	}

	num_aspi_adapters = srb.q.adapters;
	return 0;
}


static int aspi_io_call(ASPI_SRB * srb)
{
	HANDLE event;
	// Create event
	if (!(event = CreateEvent(NULL, FALSE, FALSE, NULL))) {
		pout("CreateEvent(): Error=%ld\n", GetLastError()); return -EIO;
	}
	srb->i.event_handle = event;
	srb->h.flags |= ASPI_REQFLAG_EVENT_NOTIFY;
	// Start ASPI request
	aspi_entry(srb);
	if (((volatile ASPI_SRB *)srb)->h.status == ASPI_STATUS_IN_PROGRESS) {
		// Wait for event
		DWORD rc = WaitForSingleObject(event, 30*1000L);
		if (rc != WAIT_OBJECT_0) {
			if (rc == WAIT_TIMEOUT) {
				pout("ASPI Timeout\n");
			}
			else {
				pout("WaitForSingleObject(%lx) = 0x%lx,%ld, Error=%ld\n",
					(unsigned long)event, rc, rc, GetLastError());
			}
			// TODO: ASPI_ABORT_IO command
			aspi_entry = 0;
			h_aspi_dll = INVALID_HANDLE_VALUE;
			return -EIO;
		}
	}
	CloseHandle(event);
	return 0;
}


static int aspi_open(unsigned adapter, unsigned id)
{
	if (!(adapter <= 9 && id < 16)) {
		errno = ENOENT;
		return -1;
	}

	if (!aspi_entry) {
		if (aspi_open_dll())
			return -1;
	}

	// Adapter OK?
	if (adapter >= num_aspi_adapters) {
		pout("ASPI Adapter %d does not exist (%d Adapter(s) detected).\n", adapter, num_aspi_adapters);
		if (!is_permissive()) {
			errno = ENOENT;
			return -1;
		}
	}

	return (0x0100 | ((adapter & 0xf)<<4) | (id & 0xf));
}


static void aspi_close(int fd)
{
}


/////////////////////////////////////////////////////////////////////////////

// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report)
{
	ASPI_SRB srb;

	if (!aspi_entry)
		return -EBADF;
	if (!((fd & ~0xff) == 0x100))
		return -EBADF;

	if (!(iop->cmnd_len == 6 || iop->cmnd_len == 10 || iop->cmnd_len == 12)) {
		pout("do_scsi_cmnd_io: bad CDB length\n");
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
			pout("do_scsi_cmnd_io: bad dxfer_dir\n");
			return -EINVAL;
	}

	iop->resp_sense_len = 0;
	iop->scsi_status = 0;
	iop->resid = 0;

	if (aspi_io_call(&srb)) {
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
