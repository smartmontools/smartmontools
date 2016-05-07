/*
 * os_os2.c
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2004-8 Yuri Dario <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 *
 * Thanks to Daniela Engert for providing sample code for SMART ioctl access.
 *
 */

// These are needed to define prototypes for the functions defined below
#include <errno.h>
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

// This is to include whatever prototypes you define in os_generic.h
#include "os_os2.h"

// Needed by '-V' option (CVS versioning) of smartd/smartctl
const char *os_XXXX_c_cvsid="$Id$" \
ATACMDS_H_CVSID OS_XXXX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

// global handle to device driver
static HFILE hDevice;

// Please eliminate the following block: both the two #includes and
// the 'unsupported()' function.  They are only here to warn
// unsuspecting users that their Operating System is not supported! If
// you wish, you can use a similar warning mechanism for any of the
// functions in this file that you can not (or choose not to)
// implement.

#include "config.h"

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

static void unsupported(int which){
  static int warninggiven[4];

  if (which<0 || which>3)
    return;

  if (!warninggiven[which]) {
    char msg;
    warninggiven[which]=1;

    switch (which) {
    case 0:
      msg="generate a list of devices";
      break;
    case 1:
      msg="interface to Marvell-based SATA controllers";
      break;
    case 2:
      msg="interface to 3ware-based RAID controllers";
      break;
    case 3:
      msg="interface to SCSI devices";
      break;
    }
    pout("Under OS/2, smartmontools can not %s\n");
  }
  return;
}

// print examples for smartctl.  You should modify this function so
// that the device paths are sensible for your OS, and to eliminate
// unsupported commands (eg, 3ware controllers).
void print_smartctl_examples(){
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
         "  smartctl -a /dev/hda                       (Prints all SMART information)\n\n"
         "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/hda\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long /dev/hda              (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/hda\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         "  smartctl -a --device=3ware,2 /dev/sda\n"
         "          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
         );
#else
  printf(
         "  smartctl -a /dev/hda                       (Prints all SMART information)\n"
         "  smartctl -s on -o on -S on /dev/hda         (Enables SMART on first disk)\n"
         "  smartctl -t long /dev/hda              (Executes extended disk self-test)\n"
         "  smartctl -A -l selftest -q errorsonly /dev/hda\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         "  smartctl -a -d 3ware,2 /dev/sda\n"
         "          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
         );
#endif
  return;
}

static const char * skipdev(const char * s)
{
	return (!strncmp(s, "/dev/", 5) ? s + 5 : s);
}

// tries to guess device type given the name (a path).  See utility.h
// for return values.
int guess_device_type (const char* dev_name) {

   //printf( "dev_name %s\n", dev_name);
   dev_name = skipdev(dev_name);
	if (!strncmp(dev_name, "hd", 2))
		return CONTROLLER_ATA;
	if (!strncmp(dev_name, "scsi", 4))
		return CONTROLLER_SCSI;
  return CONTROLLER_UNKNOWN;
}

// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number N of devices, or -1 if out of
// memory. Allocates N+1 arrays: one of N pointers (devlist); the
// other N arrays each contain null-terminated character strings.  In
// the case N==0, no arrays are allocated because the array of 0
// pointers has zero length, equivalent to calling malloc(0).
int make_device_names (char*** devlist, const char* name) {
  unsupported(0);
  return 0;
}

// Like open().  Return non-negative integer handle, only used by the
// functions below.  type=="ATA" or "SCSI".  If you need to store
// extra information about your devices, create a private internal
// array within this file (see os_freebsd.cpp for an example).  If you
// can not open the device (permission denied, does not exist, etc)
// set errno as open() does and return <0.
int deviceopen(const char *pathname, char *type){

  int fd;
  APIRET rc;
  ULONG ActionTaken;

  //printf( "deviceopen pathname %s\n", pathname);
  rc = DosOpen ("\\DEV\\IBMS506$", &hDevice, &ActionTaken, 0,  FILE_SYSTEM,
	       OPEN_ACTION_OPEN_IF_EXISTS, OPEN_SHARE_DENYNONE |
	       OPEN_FLAGS_NOINHERIT | OPEN_ACCESS_READONLY, NULL);
  if (rc) {
    char errmsg[256];
    snprintf(errmsg,256,"Smartctl open driver IBMS506$ failed (%d)", rc);
    errmsg[255]='\0';
    syserror(errmsg);
    return -1;
  }

  pathname = skipdev(pathname);
  fd = tolower(pathname[2]) - 'a';

  return fd;
}

// Like close().  Acts only on integer handles returned by
// deviceopen() above.
int deviceclose(int fd){

  DosClose( hDevice);
  hDevice = NULL;

  return 0;
}

static void print_ide_regs(const IDEREGS * r, int out)
{
	pout("%s=0x%02x,%s=0x%02x, SC=0x%02x, NS=0x%02x, CL=0x%02x, CH=0x%02x, SEL=0x%02x\n",
	(out?"STS":"CMD"), r->bCommandReg, (out?"ERR":" FR"), r->bFeaturesReg,
	r->bSectorCountReg, r->bSectorNumberReg, r->bCylLowReg, r->bCylHighReg, r->bDriveHeadReg);
}

//
// OS/2 direct ioctl interface to IBMS506$
//
int dani_ioctl( int device, int request, void* arg)
{
   unsigned char* buff = (unsigned char*) arg;
   APIRET rc;
   DSKSP_CommandParameters Parms;
   ULONG PLen = 1;
   ULONG DLen = 512; //sizeof (*buf);
   UCHAR temp;
   ULONG value = 0;
   IDEREGS  regs;

   //printf( "device %d, request 0x%x, arg[0] 0x%x, arg[2] 0x%x\n", device, request, buff[0], buff[2]);

   Parms.byPhysicalUnit = device;
   switch( buff[0]) {
   case WIN_IDENTIFY:
      rc = DosDevIOCtl (hDevice, DSKSP_CAT_GENERIC, DSKSP_GET_INQUIRY_DATA,
   		     (PVOID)&Parms, PLen, &PLen, (PVOID)arg+4, DLen, &DLen);
      if (rc != 0)
      {
          printf ("DANIS506 ATA GET HD Failed (%d,0x%x)\n", rc, rc);
          return -1;
      }
      break;
   case WIN_SMART:
      switch( buff[2]) {
      case SMART_STATUS:
         DLen = sizeof(value);
         // OS/2 already checks CL/CH in IBM1S506 code!! see s506rte.c (ddk)
         // value: -1=not supported, 0=ok, 1=failing
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_GETSTATUS,
      		     (PVOID)&Parms, PLen, &PLen, (PVOID)&value, DLen, &DLen);
         if (rc)
         {
             printf ("DANIS506 ATA GET SMART_STATUS failed (%d,0x%x)\n", rc, rc);
             return -1;
         }
         buff[4] = (unsigned char)value;
         break;
      case SMART_READ_VALUES:
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_GET_ATTRIBUTES,
      		     (PVOID)&Parms, PLen, &PLen, (PVOID)arg+4, DLen, &DLen);
         if (rc)
         {
             printf ("DANIS506 ATA GET DSKSP_SMART_GET_ATTRIBUTES failed (%d,0x%x)\n", rc, rc);
             return -1;
         }
         break;
      case SMART_READ_THRESHOLDS:
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_GET_THRESHOLDS,
      		     (PVOID)&Parms, PLen, &PLen, (PVOID)arg+4, DLen, &DLen);
         if (rc)
         {
             printf ("DANIS506 ATA GET DSKSP_SMART_GET_THRESHOLDS failed (%d,0x%x)\n", rc, rc);
             return -1;
         }
         break;
      case SMART_READ_LOG_SECTOR:
         buff[4] = buff[1]; // copy select field
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_READ_LOG,
      		     (PVOID)&Parms, PLen, &PLen, (PVOID)arg+4, DLen, &DLen);
         if (rc)
         {
             printf ("DANIS506 ATA GET DSKSP_SMART_READ_LOG failed (%d,0x%x)\n", rc, rc);
             return -1;
         }
         break;
      case SMART_ENABLE:
         buff[0] = 1; // enable
         DLen = 1;
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_ONOFF,
		            (PVOID)&Parms, PLen, &PLen, (PVOID)buff, DLen, &DLen);
         if (rc) {
             printf ("DANIS506 ATA GET DSKSP_SMART_ONOFF failed (%d,0x%x)\n", rc, rc);
             return -1;
         }
         break;
      case SMART_DISABLE:
         buff[0] = 0; // disable
         DLen = 1;
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_ONOFF,
		            (PVOID)&Parms, PLen, &PLen, (PVOID)buff, DLen, &DLen);
         if (rc) {
             printf ("DANIS506 ATA GET DSKSP_SMART_ONOFF failed (%d,0x%x)\n", rc, rc);
             return -1;
         }
         break;
#if 0
      case SMART_AUTO_OFFLINE:
         buff[0] = buff[3];   // select field
         DLen = 1;
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_AUTO_OFFLINE,
		            (PVOID)&Parms, PLen, &PLen, (PVOID)buff, DLen, &DLen);
         if (rc) {
             printf ("DANIS506 ATA GET DSKSP_SMART_ONOFF failed (%d,0x%x)\n", rc, rc);
             return -1;
         }
         break;
#endif
      case SMART_AUTOSAVE:
         buff[0] = buff[3];   // select field
         DLen = 1;
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_AUTOSAVE_ONOFF,
		            (PVOID)&Parms, PLen, &PLen, (PVOID)buff, DLen, &DLen);
         if (rc) {
             printf ("DANIS506 ATA DSKSP_SMART_AUTOSAVE_ONOFF failed (%d,0x%x)\n", rc, rc);
             return -1;
         }
         break;
      case SMART_IMMEDIATE_OFFLINE:
         buff[0] = buff[1];   // select field
         DLen = 1;
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_EOLI,
		            (PVOID)&Parms, PLen, &PLen, (PVOID)buff, DLen, &DLen);
         if (rc) {
             printf ("DANIS506 ATA GET DSKSP_SMART_EXEC_OFFLINE failed (%d,0x%x)\n", rc, rc);
             return -1;
         }
         break;

      default:
         fprintf( stderr, "device %d, request 0x%x, arg[0] 0x%x, arg[2] 0x%x\n", device, request, buff[0], buff[2]);
         fprintf( stderr, "unknown ioctl\n");
         return -1;
         break;
      }
      break;
   //case WIN_PIDENTIFY:
   //   break;
   default:
      fprintf( stderr, "unknown ioctl\n");
      return -1;
      break;
   }

   // ok
   return 0;
}

// Interface to ATA devices.  See os_linux.cpp for the cannonical example.
// DETAILED DESCRIPTION OF ARGUMENTS
//   device: is the integer handle provided by deviceopen()
//   command: defines the different operations, see atacmds.h
//   select: additional input data IF NEEDED (which log, which type of
//           self-test).
//   data:   location to write output data, IF NEEDED (1 or 512 bytes).
//   Note: not all commands use all arguments.
// RETURN VALUES (for all commands BUT command==STATUS_CHECK)
//  -1 if the command failed
//   0 if the command succeeded,
// RETURN VALUES if command==STATUS_CHECK
//  -1 if the command failed OR the disk SMART status can't be determined
//   0 if the command succeeded and disk SMART status is "OK"
//   1 if the command succeeded and disk SMART status is "FAILING"

// huge value of buffer size needed because HDIO_DRIVE_CMD assumes
// that buff[3] is the data size.  Since the ATA_SMART_AUTOSAVE and
// ATA_SMART_AUTO_OFFLINE use values of 0xf1 and 0xf8 we need the space.
// Otherwise a 4+512 byte buffer would be enough.
#define STRANGE_BUFFER_LENGTH (4+512*0xf8)

int ata_command_interface(int device, smart_command_set command, int select, char *data){
  unsigned char buff[STRANGE_BUFFER_LENGTH];
  // positive: bytes to write to caller.  negative: bytes to READ from
  // caller. zero: non-data command
  int copydata=0;

  const int HDIO_DRIVE_CMD_OFFSET = 4;

  // See struct hd_drive_cmd_hdr in hdreg.h.  Before calling ioctl()
  // buff[0]: ATA COMMAND CODE REGISTER
  // buff[1]: ATA SECTOR NUMBER REGISTER == LBA LOW REGISTER
  // buff[2]: ATA FEATURES REGISTER
  // buff[3]: ATA SECTOR COUNT REGISTER

  // Note that on return:
  // buff[2] contains the ATA SECTOR COUNT REGISTER

  // clear out buff.  Large enough for HDIO_DRIVE_CMD (4+512 bytes)
  memset(buff, 0, STRANGE_BUFFER_LENGTH);

  //printf( "command, select %d,%d\n", command, select);
  buff[0]=ATA_SMART_CMD;
  switch (command){
  case CHECK_POWER_MODE:
    buff[0]=ATA_CHECK_POWER_MODE;
    copydata=1;
    break;
  case READ_VALUES:
    buff[2]=ATA_SMART_READ_VALUES;
    buff[3]=1;
    copydata=512;
    break;
  case READ_THRESHOLDS:
    buff[2]=ATA_SMART_READ_THRESHOLDS;
    buff[1]=buff[3]=1;
    copydata=512;
    break;
  case READ_LOG:
    buff[2]=ATA_SMART_READ_LOG_SECTOR;
    buff[1]=select;
    buff[3]=1;
    copydata=512;
    break;
  case WRITE_LOG:
    break;
  case IDENTIFY:
    buff[0]=ATA_IDENTIFY_DEVICE;
    buff[3]=1;
    copydata=512;
    break;
  case PIDENTIFY:
    buff[0]=ATA_IDENTIFY_PACKET_DEVICE;
    buff[3]=1;
    copydata=512;
    break;
  case ENABLE:
    buff[2]=ATA_SMART_ENABLE;
    buff[1]=1;
    break;
  case DISABLE:
    buff[2]=ATA_SMART_DISABLE;
    buff[1]=1;
    break;
  case STATUS:
  case STATUS_CHECK:
    // this command only says if SMART is working.  It could be
    // replaced with STATUS_CHECK below.
    buff[2]=ATA_SMART_STATUS;
    buff[4]=0;
    break;
  case AUTO_OFFLINE:
    buff[2]=ATA_SMART_AUTO_OFFLINE;
    buff[3]=select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
    break;
  case AUTOSAVE:
    buff[2]=ATA_SMART_AUTOSAVE;
    buff[3]=select;   // YET NOTE - THIS IS A NON-DATA COMMAND!!
    break;
  case IMMEDIATE_OFFLINE:
    buff[2]=ATA_SMART_IMMEDIATE_OFFLINE;
    buff[1]=select;
    break;
  //case STATUS_CHECK:
  //  // This command uses HDIO_DRIVE_TASK and has different syntax than
  //  // the other commands.
  //  buff[1]=ATA_SMART_STATUS;
  //  break;
  default:
    pout("Unrecognized command %d in linux_ata_command_interface()\n"
         "Please contact " PACKAGE_BUGREPORT "\n", command);
    errno=ENOSYS;
    return -1;
  }

#if 0
  // This command uses the HDIO_DRIVE_TASKFILE ioctl(). This is the
  // only ioctl() that can be used to WRITE data to the disk.
  if (command==WRITE_LOG) {
    unsigned char task[sizeof(ide_task_request_t)+512];
    ide_task_request_t *reqtask=(ide_task_request_t *) task;
    task_struct_t      *taskfile=(task_struct_t *) reqtask->io_ports;
    int retval;

    memset(task,      0, sizeof(task));

    taskfile->data           = 0;
    taskfile->feature        = ATA_SMART_WRITE_LOG_SECTOR;
    taskfile->sector_count   = 1;
    taskfile->sector_number  = select;
    taskfile->low_cylinder   = 0x4f;
    taskfile->high_cylinder  = 0xc2;
    taskfile->device_head    = 0;
    taskfile->command        = ATA_SMART_CMD;

    reqtask->data_phase      = TASKFILE_OUT;
    reqtask->req_cmd         = IDE_DRIVE_TASK_OUT;
    reqtask->out_size        = 512;
    reqtask->in_size         = 0;

    // copy user data into the task request structure
    memcpy(task+sizeof(ide_task_request_t), data, 512);

    if ((retval=dani_ioctl(device, HDIO_DRIVE_TASKFILE, task))) {
      if (retval==-EINVAL)
	pout("Kernel lacks HDIO_DRIVE_TASKFILE support; compile kernel with CONFIG_IDE_TASKFILE_IO set\n");
      return -1;
    }
    return 0;
  }
#endif // 0

  // We are now doing the HDIO_DRIVE_CMD type ioctl.
  if ((dani_ioctl(device, HDIO_DRIVE_CMD, buff)))
    return -1;

  // There are two different types of ioctls().  The HDIO_DRIVE_TASK
  // one is this:
  if (command==STATUS_CHECK){
    int retval;

    // Cyl low and Cyl high unchanged means "Good SMART status"
    if (buff[4]==0)
      return 0;

    // These values mean "Bad SMART status"
    if (buff[4]==1)
      return 1;

    // We haven't gotten output that makes sense; print out some debugging info
    syserror("Error SMART Status command failed");
    pout("Please get assistance from " PACKAGE_HOMEPAGE "\n");
    return -1;
  }

  // CHECK POWER MODE command returns information in the Sector Count
  // register (buff[3]).  Copy to return data buffer.
  if (command==CHECK_POWER_MODE)
    buff[HDIO_DRIVE_CMD_OFFSET]=buff[2];

  // if the command returns data then copy it back
  if (copydata)
    memcpy(data, buff+HDIO_DRIVE_CMD_OFFSET, copydata);

  return 0;
}

// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report) {
  unsupported(3);
  return -ENOSYS;
}
