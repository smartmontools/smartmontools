/*
 * os_darwin.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004 Geoffrey Keating <geoffk@geoffk.org>
 * Copyright (C) 2003-4 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/*
  Geoff, does the nex paragraph still have any relevance or interest?
  Should I move sm5_Darwin into the CVS Attic/?  Also, should Peter
  Cassidy's name be added to the Copyright above?  I don't know if you
  made any use of his code in writing this.
  -- BA

  Note that for Darwin much of this already exists. See some partially
  developed but incomplete code at:
  http://cvs.sourceforge.net/viewcvs.py/smartmontools/sm5_Darwin/.
*/

#include <stdbool.h>
#include <errno.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/ata/IOATAStorageDefines.h>
#include <IOKit/storage/ata/ATASMARTLib.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOMedia.h>
#include <CoreFoundation/CoreFoundation.h>

  // No, I don't know why there isn't a header for this.
#define kIOATABlockStorageDeviceClass   "IOATABlockStorageDevice"

#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

#include "os_darwin.h"

// Needed by '-V' option (CVS versioning) of smartd/smartctl
const char *os_XXXX_c_cvsid="$Id: os_darwin.c,v 1.2 2004/07/16 05:55:00 ballen4705 Exp $" \
ATACMDS_H_CVSID OS_XXXX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;


// Print examples for smartctl.
void print_smartctl_examples(){
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n");
  printf(
         "  smartctl -a disk0                            (Prints all SMART information)\n\n"
         "  smartctl -t long /dev/disk0              (Executes extended disk self-test)\n\n"
#ifdef HAVE_GETOPT_LONG
         "  smartctl --smart=on --saveauto=on /dev/rdisk0 (Enables SMART on first disk)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/disk0\n"
         "                                        (Prints Self-Test & Attribute errors)\n\n"
#else
         "  smartctl -s on -S on /dev/rdisk0              (Enables SMART on first disk)\n\n"
         "  smartctl -A -l selftest -q errorsonly /dev/disk0\n"
         "                                        (Prints Self-Test & Attribute errors)\n\n"
#endif
         "  smartctl -a IOService:/MacRISC2PE/pci@f4000000/AppleMacRiscPCI/ata-6@D/AppleKauaiATA/ATADeviceNub@0/IOATABlockStorageDriver/IOATABlockStorageDevice\n"
         "                                                 (You can use IOService: ...)\n\n"
         "  smartctl -c IODeviceTree:/pci@f4000000/ata-6@D/@0:0\n"
         "                                                       (... Or IODeviceTree:)\n"
         );
  return;
}

// tries to guess device type given the name (a path).  See utility.h
// for return values.
int guess_device_type (const char* dev_name) {
  // Only ATA is supported right now, so that's what it'd better be.
  dev_name = dev_name;  // suppress unused warning.
  return GUESS_DEVTYPE_ATA;
}

// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number N of devices, or -1 if out of
// memory. Allocates N+1 arrays: one of N pointers (devlist); the
// other N arrays each contain null-terminated character strings.  In
// the case N==0, no arrays are allocated because the array of 0
// pointers has zero length, equivalent to calling malloc(0).
int make_device_names (char*** devlist, const char* name) {
  IOReturn err;
  io_iterator_t i;
  io_object_t device;
  int result;
  int index;
  const char * cls;

  if (strcmp (name, "ATA") == 0)
    cls = kIOATABlockStorageDeviceClass;
  else  // only ATA supported right now.
    return 0;

  err = IOServiceGetMatchingServices (kIOMasterPortDefault,
				      IOServiceMatching (cls),
				      &i);
  if (err != kIOReturnSuccess)
    return -1;

  // Count the devices.
  for (result = 0; (device = IOIteratorNext (i)) != MACH_PORT_NULL; result++)
    IOObjectRelease (device);

  // Create an array of service names.
  IOIteratorReset (i);
  *devlist = calloc (result, sizeof (char *));
  if (*devlist == NULL)
    goto error;
  for (index = 0; (device = IOIteratorNext (i)) != MACH_PORT_NULL; index++)
    {
      io_string_t devName;
      IORegistryEntryGetPath(device, kIOServicePlane, devName);
      IOObjectRelease (device);

      (*devlist)[index] = strdup (devName);
      if ((*devlist)[index] == NULL)
	goto error;
    }
  IOObjectRelease (i);

  return result;

 error:
  IOObjectRelease (i);
  if (*devlist != NULL)
    {
      for (index = 0; index < result; index++)
	if ((*devlist)[index] != NULL)
	  free ((*devlist)[index]);
      free (*devlist);
    }
  return -1;
}

// Information that we keep about each device.

static struct {
  io_object_t ioob;
  bool hassmart;
  IOCFPlugInInterface **plugin;
  IOATASMARTInterface **smartIf;
} devices[20];

// Like open().  Return non-negative integer handle, only used by the
// functions below.  type=="ATA" or "SCSI".  The return value is
// an index into the devices[] array.  If the device can't be opened,
// sets errno and returns -1.
// Acceptable device names are:
// /dev/disk*
// /dev/rdisk*
// disk*
// IOService:*
// IODeviceTree:*
int deviceopen(const char *pathname, char *type){
  size_t devnum;
  const char *devname;
  io_object_t disk;
  
  if (strcmp (type, "ATA") != 0)
    {
      errno = EINVAL;
      return -1;
    }
  
  // Find a free device number.
  for (devnum = 0; devnum < sizeof (devices) / sizeof (devices[0]); devnum++)
    if (devices[devnum].ioob == MACH_PORT_NULL)
      break;
  if (devnum == sizeof (devices) / sizeof (devices[0]))
    {
      errno = EMFILE;
      return -1;
    }
  
  devname = NULL;
  if (strncmp (pathname, "/dev/rdisk", 10) == 0)
    devname = pathname + 6;
  else if (strncmp (pathname, "/dev/disk", 9) == 0)
    devname = pathname + 5;
  else if (strncmp (pathname, "disk", 4) == 0)
    // allow user to just say 'disk0'
    devname = pathname;

  // Find the device.
  if (devname != NULL)
    {
      CFMutableDictionaryRef matcher;
      matcher = IOBSDNameMatching (kIOMasterPortDefault, 0, devname);
      disk = IOServiceGetMatchingService (kIOMasterPortDefault, matcher);
    }
  else
    {
      disk = IORegistryEntryFromPath (kIOMasterPortDefault, pathname);
    }

  if (disk == MACH_PORT_NULL)
    {
      errno = ENOENT;
      return -1;
    }
  
  // Find the ATA block storage driver that is the parent of this device
  while (! IOObjectConformsTo (disk, kIOATABlockStorageDeviceClass))
    {
      IOReturn err;
      io_object_t notdisk = disk;

      err = IORegistryEntryGetParentEntry (notdisk, kIOServicePlane, &disk);
      if (err != kIOReturnSuccess || disk == MACH_PORT_NULL)
	{
	  errno = ENODEV;
	  IOObjectRelease (notdisk);
	  return -1;
	}
    }

  devices[devnum].ioob = disk;
  
  {
    CFMutableDictionaryRef diskProps = NULL;
    CFDictionaryRef diskChars = NULL;
    CFNumberRef diskFeatures = NULL;
    UInt32 ataFeatures;

    // Determine whether the drive actually supports SMART.
    if (IORegistryEntryCreateCFProperties (disk, &diskProps,
					   kCFAllocatorDefault,
					   kNilOptions) == kIOReturnSuccess
	&& CFDictionaryGetValueIfPresent (diskProps,
				   CFSTR (kIOPropertyDeviceCharacteristicsKey),
					  (const void **)&diskChars)
	&& CFDictionaryGetValueIfPresent (diskChars, CFSTR ("ATA Features"),
					  (const void **)&diskFeatures)
	&& CFNumberGetValue (diskFeatures, kCFNumberLongType, &ataFeatures)
	&& (ataFeatures & kIOATAFeatureSMART))
      devices[devnum].hassmart = true;
    else
      devices[devnum].hassmart = false;

    if (diskProps != NULL)
      CFRelease (diskProps);
  }
  
  {
    SInt32 dummy;
  
    devices[devnum].plugin = NULL;
    devices[devnum].smartIf = NULL;

    // Create an interface to the ATA SMART library.
    if (devices[devnum].hassmart
	&& IOCreatePlugInInterfaceForService (disk,
					      kIOATASMARTUserClientTypeID,
					      kIOCFPlugInInterfaceID,
					      &devices[devnum].plugin,
					      &dummy) == kIOReturnSuccess)
      (*devices[devnum].plugin)->QueryInterface
	(devices[devnum].plugin,
	 CFUUIDGetUUIDBytes ( kIOATASMARTInterfaceID),
	 (LPVOID) &devices[devnum].smartIf);
  }
  
  return devnum;
}

// Like close().  Acts only on integer handles returned by
// deviceopen() above.
int deviceclose(int fd){
  if (devices[fd].smartIf)
    (*devices[fd].smartIf)->Release (devices[fd].smartIf);
  if (devices[fd].plugin)
    IODestroyPlugInInterface (devices[fd].plugin);
  IOObjectRelease (devices[fd].ioob);
  devices[fd].ioob = MACH_PORT_NULL;
  return 0;
}

// Interface to ATA devices.  See os_linux.c for the cannonical example.
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
int
ata_command_interface(int fd, smart_command_set command,
		      int select, char *data)
{
  IOATASMARTInterface **ifp = devices[fd].smartIf;
  IOATASMARTInterface *smartIf;
  IOReturn err;
  
  if (! ifp)
    return -1;
  smartIf = *ifp;

  switch (command)
    {
    case STATUS:
      return 0;
    case STATUS_CHECK:
      {
	Boolean is_failing;
	err = smartIf->SMARTReturnStatus (ifp, &is_failing);
	if (err == kIOReturnSuccess && is_failing)
	  return 1;
	break;
      }
    case ENABLE:
    case DISABLE:
      err = smartIf->SMARTEnableDisableOperations (ifp, command == ENABLE);
      break;
    case AUTOSAVE:
      err = smartIf->SMARTEnableDisableAutosave (ifp, select != 0);
      break;
    case IMMEDIATE_OFFLINE:
      if (select != SHORT_SELF_TEST && select != EXTEND_SELF_TEST)
	{
	  errno = EINVAL;
	  return -1;
	}
      err = smartIf->SMARTExecuteOffLineImmediate (ifp, 
						   select == EXTEND_SELF_TEST);
      break;
    case READ_VALUES:
      err = smartIf->SMARTReadData (ifp, (ATASMARTData *)data);
      break;
    case READ_THRESHOLDS:
      err = smartIf->SMARTReadDataThresholds (ifp, 
					      (ATASMARTDataThresholds *)data);
      break;
    case READ_LOG:
      err = smartIf->SMARTReadLogAtAddress (ifp, select, data, 512);
      break;
    case WRITE_LOG:
      err = smartIf->SMARTWriteLogAtAddress (ifp, select, data, 512);
      break;
    case IDENTIFY:
      {
	UInt32 dummy;
	err = smartIf->GetATAIdentifyData (ifp, data, 512, &dummy);
	if (err == kIOReturnSuccess && isbigendian())
	  {
	    int i;
	    /* The system has already byte-swapped, undo it.  */
	    for (i = 0; i < 256; i+=2)
	      {
		char d = data[i];
		data[i] = data[i+1];
		data[i+1] = d;
	      }
	  }
      }
      break;
    case CHECK_POWER_MODE:
      ;
    default:
      errno = ENOTSUP;
      return -1;
    }
  if (err == kIOReturnExclusiveAccess)
    errno = EBUSY;
  return err == kIOReturnSuccess ? 0 : -1;
}

// There's no special handling needed for hidden devices, the kernel
// must deal with them.
int escalade_command_interface(int fd, int escalade_port, int escalade_type,
			       smart_command_set command, int select,
			       char *data)
{
  fd = fd;
  escalade_port = escalade_port;
  escalade_type = escalade_type;
  command = command;
  select = select;
  data = data;
  return -1;
}

// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report) {
  return -ENOSYS;
}
