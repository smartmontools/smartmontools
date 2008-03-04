/*
 * os_darwin.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2004-8 Geoffrey Keating <geoffk@geoffk.org>
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

#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOReturn.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/ata/IOATAStorageDefines.h>
#include <IOKit/storage/ata/ATASMARTLib.h>
#include <CoreFoundation/CoreFoundation.h>

  // No, I don't know why there isn't a header for this.
#define kIOATABlockStorageDeviceClass   "IOATABlockStorageDevice"

#include "config.h"
#include "int64.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

#include "os_darwin.h"

// Needed by '-V' option (CVS versioning) of smartd/smartctl
const char *os_XXXX_c_cvsid="$Id: os_darwin.cpp,v 1.20 2008/03/04 22:09:47 ballen4705 Exp $" \
ATACMDS_H_CVSID CONFIG_H_CVSID INT64_H_CVSID OS_DARWIN_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

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
  return CONTROLLER_ATA;
}

// Determine whether 'dev' is a SMART-capable device.
static bool is_smart_capable (io_object_t dev) {
  CFTypeRef smartCapableKey;
  CFDictionaryRef diskChars;

  // If the device has kIOPropertySMARTCapableKey, then it's capable,
  // no matter what it looks like.
  smartCapableKey = IORegistryEntryCreateCFProperty
    (dev, CFSTR (kIOPropertySMARTCapableKey),
     kCFAllocatorDefault, 0);
  if (smartCapableKey)
    {
      CFRelease (smartCapableKey);
      return true;
    }

  // If it's an kIOATABlockStorageDeviceClass then we're successful
  // only if its ATA features indicate it supports SMART.
  if (IOObjectConformsTo (dev, kIOATABlockStorageDeviceClass)
      && (diskChars = (CFDictionaryRef)IORegistryEntryCreateCFProperty                                                                                                           
	  (dev, CFSTR (kIOPropertyDeviceCharacteristicsKey),
	   kCFAllocatorDefault, kNilOptions)) != NULL)
    {
      CFNumberRef diskFeatures = NULL;
      UInt32 ataFeatures = 0;

      if (CFDictionaryGetValueIfPresent (diskChars, CFSTR ("ATA Features"),
					 (const void **)&diskFeatures))
	CFNumberGetValue (diskFeatures, kCFNumberLongType,
			  &ataFeatures);
      CFRelease (diskChars);
      if (diskFeatures)
	CFRelease (diskFeatures);
      
      return (ataFeatures & kIOATAFeatureSMART) != 0;
    }
  return false;
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
  io_object_t device = MACH_PORT_NULL;
  int result;
  int index;

  // We treat all devices as ATA so long as they support SMARTLib.
  if (strcmp (name, "ATA") != 0)
    return 0;

  err = IOServiceGetMatchingServices 
    (kIOMasterPortDefault, IOServiceMatching (kIOBlockStorageDeviceClass), &i);
  if (err != kIOReturnSuccess)
    return -1;

  // Count the devices.
  result = 0;
  while ((device = IOIteratorNext (i)) != MACH_PORT_NULL) {
    if (is_smart_capable (device))
      result++;
    IOObjectRelease (device);
  }

  // Create an array of service names.
  IOIteratorReset (i);
  *devlist = (char**)Calloc (result, sizeof (char *)); 
  if (! *devlist)
    goto error;
  index = 0;
  while ((device = IOIteratorNext (i)) != MACH_PORT_NULL) {
    if (is_smart_capable (device))
      {
	io_string_t devName;
	IORegistryEntryGetPath(device, kIOServicePlane, devName);
	(*devlist)[index] = CustomStrDup (devName, true, __LINE__, __FILE__);
	if (! (*devlist)[index])
	  goto error;
	index++;
      }
    IOObjectRelease (device);
  }

  IOObjectRelease (i);
  return result;

 error:
  if (device != MACH_PORT_NULL)
    IOObjectRelease (device);
  IOObjectRelease (i);
  if (*devlist)
    {
      for (index = 0; index < result; index++)
	if ((*devlist)[index])
	  FreeNonZero ((*devlist)[index], 0, __LINE__, __FILE__);
      FreeNonZero (*devlist, result * sizeof (char *), __LINE__, __FILE__);
    }
  return -1;
}

// Information that we keep about each device.

static struct {
  io_object_t ioob;
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
    if (! devices[devnum].ioob)
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
  if (devname)
    {
      CFMutableDictionaryRef matcher;
      matcher = IOBSDNameMatching (kIOMasterPortDefault, 0, devname);
      disk = IOServiceGetMatchingService (kIOMasterPortDefault, matcher);
    }
  else
    {
      disk = IORegistryEntryFromPath (kIOMasterPortDefault, pathname);
    }

  if (! disk)
    {
      errno = ENOENT;
      return -1;
    }
  
  // Find a SMART-capable driver which is a parent of this device.
  while (! is_smart_capable (disk))
    {
      IOReturn err;
      io_object_t prevdisk = disk;

      // Find this device's parent and try again.
      err = IORegistryEntryGetParentEntry (disk, kIOServicePlane, &disk);
      if (err != kIOReturnSuccess || ! disk)
	{
	  errno = ENODEV;
	  IOObjectRelease (prevdisk);
	  return -1;
	}
    }
  
  devices[devnum].ioob = disk;

  {
    SInt32 dummy;
  
    devices[devnum].plugin = NULL;
    devices[devnum].smartIf = NULL;

    // Create an interface to the ATA SMART library.
    if (IOCreatePlugInInterfaceForService (disk,
					   kIOATASMARTUserClientTypeID,
					   kIOCFPlugInInterfaceID,
					   &devices[devnum].plugin,
					   &dummy) == kIOReturnSuccess)
      (*devices[devnum].plugin)->QueryInterface
	(devices[devnum].plugin,
	 CFUUIDGetUUIDBytes ( kIOATASMARTInterfaceID),
         (void **)&devices[devnum].smartIf);
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

// Things that aren't available in the Darwin interfaces:
// - Tests other than short and extended (in particular, can't run
//   an immediate offline test)
// - Captive-mode tests, aborting tests
// - ability to switch automatic offline testing on or off

// Note that some versions of Darwin, at least 7H63 and earlier,
// have a buggy library that treats the boolean value in
// SMARTEnableDisableOperations, SMARTEnableDisableAutosave, and
// SMARTExecuteOffLineImmediate as always being true.
int
ata_command_interface(int fd, smart_command_set command,
		      int select, char *data)
{
  IOATASMARTInterface **ifp = devices[fd].smartIf;
  IOATASMARTInterface *smartIf;
  IOReturn err;
  int timeoutCount = 5;
  
  if (! ifp)
    return -1;
  smartIf = *ifp;

  do {
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
	  if (err != kIOReturnSuccess && err != kIOReturnTimeout
	      && err != kIOReturnNotResponding)
	    printf ("identify failed: %#x\n", (unsigned) err);
	  if (err == kIOReturnSuccess && isbigendian())
	    {
	      int i;
	      /* The system has already byte-swapped, undo it.  */
	      for (i = 0; i < 256; i+=2)
		swap2 (data + i);
	    }
	}
	break;
      case CHECK_POWER_MODE:
	// The information is right there in the device registry, but how
	// to get to it portably?
      default:
	errno = ENOTSUP;
	return -1;
      }
    /* This bit is a bit strange.  Apparently, when the drive is spun
       down, the intended behaviour of these calls is that they fail,
       return kIOReturnTimeout and then power the drive up.  So if
       you get a timeout, you have to try again to get the actual
       command run, but the drive is already powering up so you can't
       use this for CHECK_POWER_MODE.  */
    if (err == kIOReturnTimeout || err == kIOReturnNotResponding)
      sleep (1);
  } while ((err == kIOReturnTimeout || err == kIOReturnNotResponding)
	   && timeoutCount-- > 0);
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

int marvell_command_interface(int fd, smart_command_set command,
		      int select, char *data)
{ 
  fd = fd;
  command = command;
  select = select;
  data = data;
  return -1;
}

int highpoint_command_interface(int fd, smart_command_set command, int select, char *data)
{
  fd = fd;
  command = command;
  select = select;
  data = data;
  return -1;
}

// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report) {
  fd = fd;
  iop = iop;
  report = report;
  return -ENOSYS;
}
