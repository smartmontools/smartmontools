/*
 * os_os2.c
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2004-8 Yuri Dario
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*
 *
 * Thanks to Daniela Engert for providing sample code for SMART ioctl access.
 *
 */

// These are needed to define prototypes for the functions defined below
#include "config.h"

#include <ctype.h>
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

// print examples for smartctl.  You should modify this function so
// that the device paths are sensible for your OS, and to eliminate
// unsupported commands (eg, 3ware controllers).
void print_smartctl_examples(){
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
         "  smartctl -a hd0                       (Prints all SMART information)\n\n"
         "  smartctl --smart=on --offlineauto=on --saveauto=on hd0\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long hd0              (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly hd0\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         );
#else
  printf(
         "  smartctl -a hd0                       (Prints all SMART on first disk with DANIS506)\n"
         "  smartctl -a ahci0                     (Prints all SMART on first disk with OS2AHCI)\n"
         "  smartctl -s on -o on -S on hd0         (Enables SMART on first disk)\n"
         "  smartctl -t long hd0              (Executes extended disk self-test)\n"
         "  smartctl -A -l selftest -q errorsonly hd0\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
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
	if (!strncmp(dev_name, "hd", 2) || !strncmp(dev_name, "ahci", 4))
		return CONTROLLER_ATA;
  return CONTROLLER_UNKNOWN;
}

// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number N of devices, or -1 if out of
// memory. Allocates N+1 arrays: one of N pointers (devlist); the
// other N arrays each contain null-terminated character strings.  In
// the case N==0, no arrays are allocated because the array of 0
// pointers has zero length, equivalent to calling malloc(0).

int make_device_names (char*** devlist, const char* name) {

  int result;
  int index;
  const int max_dev = 32; // scan only first 32 devices

  // SCSI is not supported
  if (strcmp (name, "ATA") != 0)
    return 0;
  
  // try to open DANIS
  APIRET rc;
  ULONG ActionTaken;
  HFILE danisDev, ahciDev;
  bool is_danis = 0, is_ahci = 0;

  rc = DosOpen ((const char unsigned *)danisdev, &danisDev, &ActionTaken, 0,  FILE_SYSTEM,
	       OPEN_ACTION_OPEN_IF_EXISTS, OPEN_SHARE_DENYNONE |
	       OPEN_FLAGS_NOINHERIT | OPEN_ACCESS_READONLY, NULL);
  if (!rc)
    is_danis = 1;

  rc = DosOpen ((const char unsigned *)ahcidev, &ahciDev, &ActionTaken, 0,  FILE_SYSTEM,
	       OPEN_ACTION_OPEN_IF_EXISTS, OPEN_SHARE_DENYNONE |
	       OPEN_FLAGS_NOINHERIT | OPEN_ACCESS_READONLY, NULL);
  if (!rc)
    is_ahci = 1;
 
  // Count the devices.
  result = 0;
  
  DSKSP_CommandParameters Parms;
  ULONG PLen = 1;
  ULONG IDLen = 512;
  struct ata_identify_device Id;

  for(int i = 0; i < max_dev; i++) {
    if (is_ahci) {
      Parms.byPhysicalUnit = i;
      rc = DosDevIOCtl (ahciDev, DSKSP_CAT_GENERIC, DSKSP_GET_INQUIRY_DATA,
   		     (PVOID)&Parms, PLen, &PLen, (PVOID)&Id, IDLen, &IDLen);
      if (!rc) result++;
    }
    if (is_danis) {
      Parms.byPhysicalUnit = i + 0x80;
      rc = DosDevIOCtl (danisDev, DSKSP_CAT_GENERIC, DSKSP_GET_INQUIRY_DATA,
   		     (PVOID)&Parms, PLen, &PLen, (PVOID)&Id, IDLen, &IDLen);
      if (!rc) result++;
    }
  }
  *devlist = (char**)calloc (result, sizeof (char *));
  if (! *devlist)
    goto error;
  index = 0;

  // add devices
  for(int i = 0; i < max_dev; i++) {
    if (is_ahci) {
      Parms.byPhysicalUnit = i;
      rc = DosDevIOCtl (ahciDev, DSKSP_CAT_GENERIC, DSKSP_GET_INQUIRY_DATA,
   		     (PVOID)&Parms, PLen, &PLen, (PVOID)&Id, IDLen, &IDLen);
      if (!rc) {
        asprintf(&(*devlist)[index], "ahci%d", i);
        if (! (*devlist)[index])
          goto error;
        index++;
      }
    }
    if (is_danis) {
      Parms.byPhysicalUnit = i + 0x80;
      rc = DosDevIOCtl (danisDev, DSKSP_CAT_GENERIC, DSKSP_GET_INQUIRY_DATA,
   		     (PVOID)&Parms, PLen, &PLen, (PVOID)&Id, IDLen, &IDLen);
      if (!rc) {
        asprintf(&(*devlist)[index], "hd%d", i);
        if (! (*devlist)[index])
          goto error;
        index++;
      }
    }
  }

  if (is_danis)
      DosClose( danisDev);

  if (is_ahci)
      DosClose( ahciDev);

  return result;

 error:
  if (*devlist)
    {
      for (index = 0; index < result; index++)
        if ((*devlist)[index])
          free ((*devlist)[index]);
      free (*devlist);
    }
  if (is_danis)
      DosClose( danisDev);

  if (is_ahci)
      DosClose( ahciDev);

  return -1;
}

// Like open().  Return non-negative integer handle, only used by the
// functions below.  type=="ATA" or "SCSI".  If you need to store
// extra information about your devices, create a private internal
// array within this file (see os_freebsd.cpp for an example).  If you
// can not open the device (permission denied, does not exist, etc)
// set errno as open() does and return <0.
int deviceopen(const char *pathname, char * /* type */ ){

  int fd = 0;
  APIRET rc;
  ULONG ActionTaken;
 
  char * activedev = NULL;

  pathname = skipdev(pathname);
  // DANIS506 driver
  if(strlen(pathname) > strlen(danispref)
    && strncmp(pathname, danispref, strlen(danispref)) == 0) {
    fd = strtol(pathname + strlen(danispref), NULL, 10) + 0x80;
    activedev = (char *)danisdev;
  }
  // OS2AHCI driver
  if(strlen(pathname) > strlen(ahcipref)
    && strncmp(pathname, ahcipref, strlen(ahcipref)) == 0) {
    fd = strtol(pathname + strlen(ahcipref), NULL, 10);
    activedev = (char *)ahcidev;
  }

  if(!activedev) {
     pout("Error: please specify hdX or ahciX device name\n");
     return -1;
  }
  //printf( "deviceopen pathname %s\n", pathname);
  rc = DosOpen ((const char unsigned *)activedev, &hDevice, &ActionTaken, 0,  FILE_SYSTEM,
	       OPEN_ACTION_OPEN_IF_EXISTS, OPEN_SHARE_DENYNONE |
	       OPEN_FLAGS_NOINHERIT | OPEN_ACCESS_READONLY, NULL);
  if (rc) {
    char errmsg[256];
    snprintf(errmsg,256,"Smartctl open driver %s failed (%lu)", activedev, rc);
    errmsg[255]='\0';
    syserror(errmsg);
    return -1;
  }

  return fd;
}

// Like close().  Acts only on integer handles returned by
// deviceopen() above.
int deviceclose(int /* fd */){

  DosClose( hDevice);
  hDevice = NULL;

  return 0;
}

//
// OS/2 direct ioctl interface to IBMS506$/OS2AHCI$
//
static int dani_ioctl( int device, void* arg)
{
   unsigned char* buff = (unsigned char*) arg;
   APIRET rc;
   DSKSP_CommandParameters Parms;
   ULONG PLen = 1;
   ULONG DLen = 512; //sizeof (*buf);
   ULONG value = 0;

   // printf( "device %d, request 0x%x, arg[0] 0x%x, arg[2] 0x%x\n", device, request, buff[0], buff[2]);

   Parms.byPhysicalUnit = device;
   switch( buff[0]) {
   case ATA_IDENTIFY_DEVICE:
      rc = DosDevIOCtl (hDevice, DSKSP_CAT_GENERIC, DSKSP_GET_INQUIRY_DATA,
   		     (PVOID)&Parms, PLen, &PLen, (UCHAR *)arg+4, DLen, &DLen);
      if (rc != 0)
      {
          printf ("DANIS506 ATA DSKSP_GET_INQUIRY_DATA failed (%lu)\n", rc);
          return -1;
      }
      break;
   case ATA_SMART_CMD:
      switch( buff[2]) {
      case ATA_SMART_STATUS:
         DLen = sizeof(value);
         // OS/2 already checks CL/CH in IBM1S506 code!! see s506rte.c (ddk)
         // value: -1=not supported, 0=ok, 1=failing
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_GETSTATUS,
      		     (PVOID)&Parms, PLen, &PLen, (PVOID)&value, DLen, &DLen);
         if (rc)
         {
             printf ("DANIS506 ATA GET SMART_STATUS failed (%lu)\n", rc);
             return -1;
         }
         buff[4] = (unsigned char)value;
         break;
      case ATA_SMART_READ_VALUES:
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_GET_ATTRIBUTES,
      		     (PVOID)&Parms, PLen, &PLen, (UCHAR *)arg+4, DLen, &DLen);
         if (rc)
         {
             printf ("DANIS506 ATA GET DSKSP_SMART_GET_ATTRIBUTES failed (%lu)\n", rc);
             return -1;
         }
         break;
      case ATA_SMART_READ_THRESHOLDS:
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_GET_THRESHOLDS,
      		     (PVOID)&Parms, PLen, &PLen, (UCHAR *)arg+4, DLen, &DLen);
         if (rc)
         {
             printf ("DANIS506 ATA GET DSKSP_SMART_GET_THRESHOLDS failed (%lu)\n", rc);
             return -1;
         }
         break;
      case ATA_SMART_READ_LOG_SECTOR:
         buff[4] = buff[1]; // copy select field
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_GET_LOG,
      		     (PVOID)&Parms, PLen, &PLen, (UCHAR *)arg+4, DLen, &DLen);
         if (rc)
         {
             printf ("DANIS506 ATA GET DSKSP_SMART_GET_LOG failed (%lu)\n", rc);
             return -1;
         }
         break;
      case ATA_SMART_ENABLE:
         buff[0] = 1; // enable
         DLen = 1;
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_ONOFF,
		            (PVOID)&Parms, PLen, &PLen, (PVOID)buff, DLen, &DLen);
         if (rc) {
             printf ("DANIS506 ATA GET DSKSP_SMART_ONOFF failed (%lu)\n", rc);
             return -1;
         }
         break;
      case ATA_SMART_DISABLE:
         buff[0] = 0; // disable
         DLen = 1;
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_ONOFF,
		            (PVOID)&Parms, PLen, &PLen, (PVOID)buff, DLen, &DLen);
         if (rc) {
             printf ("DANIS506 ATA GET DSKSP_SMART_ONOFF failed (%lu)\n", rc);
             return -1;
         }
         break;
#if 0
      case ATA_SMART_AUTO_OFFLINE:
         buff[0] = buff[3];   // select field
         DLen = 1;
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_AUTO_OFFLINE,
		            (PVOID)&Parms, PLen, &PLen, (PVOID)buff, DLen, &DLen);
         if (rc) {
             printf ("DANIS506 ATA GET DSKSP_SMART_ONOFF failed (%lu)\n", rc);
             return -1;
         }
         break;
#endif
      case ATA_SMART_AUTOSAVE:
         buff[0] = buff[3];   // select field
         DLen = 1;
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_AUTOSAVE_ONOFF,
		            (PVOID)&Parms, PLen, &PLen, (PVOID)buff, DLen, &DLen);
         if (rc) {
             printf ("DANIS506 ATA DSKSP_SMART_AUTOSAVE_ONOFF failed (%lu)\n", rc);
             return -1;
         }
         break;
      case ATA_SMART_IMMEDIATE_OFFLINE:
         buff[0] = buff[1];   // select field
         DLen = 1;
         rc = DosDevIOCtl (hDevice, DSKSP_CAT_SMART, DSKSP_SMART_EXEC_OFFLINE,
		            (PVOID)&Parms, PLen, &PLen, (PVOID)buff, DLen, &DLen);
         if (rc) {
             printf ("DANIS506 ATA GET DSKSP_SMART_EXEC_OFFLINE failed (%lu)\n", rc);
             return -1;
         }
         break;

      default:
         fprintf( stderr, "device %d, arg[0] 0x%x, arg[2] 0x%x\n", device, buff[0], buff[2]);
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

  // We are now calling ioctl wrapper to the driver.
  // TODO: use PASSTHRU in case of OS2AHCI driver
  if ((dani_ioctl(device, buff)))
    return -1;

  // There are two different types of ioctls().  The HDIO_DRIVE_TASK
  // one is this:
  if (command==STATUS_CHECK){
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

// Interface to SCSI devices. N/A under OS/2
int do_scsi_cmnd_io(int /* fd */, struct scsi_cmnd_io * /* iop */, int /* report */) {
  pout("SCSI interface is not implemented\n");
  return -ENOSYS;
}
