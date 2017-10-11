/*
 * os_darwin.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2004-8 Geoffrey Keating <geoffk@geoffk.org>
 * Copyright (C) 2014 Alex Samorukov <samm@os2.kiev.ua>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with smartmontools.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_init.h>
#include <sys/utsname.h>
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

#include "config.h"
#include "int64.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "nvmecmds.h"
#include "utility.h"
#include "os_darwin.h"
#include "dev_interface.h"

#define ARGUSED(x) ((void)(x))
// Needed by '-V' option (CVS versioning) of smartd/smartctl
const char *os_darwin_cpp_cvsid="$Id$" \
ATACMDS_H_CVSID CONFIG_H_CVSID INT64_H_CVSID OS_DARWIN_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

// examples for smartctl
static const char  smartctl_examples[] =
         "=================================================== SMARTCTL EXAMPLES =====\n\n"
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
         ;


// Information that we keep about each device.

static struct {
  io_object_t ioob;
  IOCFPlugInInterface **plugin;
  IOATASMARTInterface **smartIf; // ATA devices
  IONVMeSMARTInterface **smartIfNVMe;
} devices[20];

const char * dev_darwin_cpp_cvsid = "$Id$"
  DEV_INTERFACE_H_CVSID;

/////////////////////////////////////////////////////////////////////////////

namespace os { // No need to publish anything, name provided for Doxygen

/////////////////////////////////////////////////////////////////////////////
/// Implement shared open/close routines with old functions.

class darwin_smart_device
: virtual public /*implements*/ smart_device
{
public:
  explicit darwin_smart_device(const char * mode)
    : smart_device(never_called),
      m_fd(-1), m_mode(mode) { }

  virtual ~darwin_smart_device() throw();

  virtual bool is_open() const;

  virtual bool open();

  virtual bool close();
 
protected:
  /// Return filedesc for derived classes.
  int get_fd() const
    { return m_fd; }
    
private:
  int m_fd; ///< filedesc, -1 if not open.
  const char * m_mode; ///< Mode string for deviceopen().
};


darwin_smart_device::~darwin_smart_device() throw()
{
  if (m_fd >= 0)
    darwin_smart_device::close();
}

bool darwin_smart_device::is_open() const
{
  return (m_fd >= 0);
}

// Determine whether 'dev' is a SMART-capable device.
static bool is_smart_capable (io_object_t dev, const char * type) {
  CFTypeRef smartCapableKey = NULL;
  CFDictionaryRef diskChars;

  // If the device has kIOPropertySMARTCapableKey, then it's capable,
  // no matter what it looks like.
  if (!strcmp("ATA", type))  {
  smartCapableKey = IORegistryEntryCreateCFProperty
    (dev, CFSTR (kIOPropertySMARTCapableKey),
     kCFAllocatorDefault, 0);
  }

  else if (!strcmp("NVME", type)) {
    smartCapableKey = IORegistryEntryCreateCFProperty
      (dev, CFSTR (kIOPropertyNVMeSMARTCapableKey),
       kCFAllocatorDefault, 0);
  }

  if (smartCapableKey)
    {
      CFRelease (smartCapableKey);
      return true;
    }

  // If it's an kIOATABlockStorageDeviceClass then we're successful
  // only if its ATA features indicate it supports SMART.
  // This will be broken for NVMe, however it is not needed
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

bool darwin_smart_device::open()
{
  // Acceptable device names are:
  // /dev/disk*
  // /dev/rdisk*
  // disk*
  // IOService:*
  // IODeviceTree:*
  size_t devnum;
  const char *devname;
  io_object_t disk;
  const char *pathname = get_dev_name();
  char *type = const_cast<char*>(m_mode);
  
  if (!(strcmp("ATA", type) || strcmp("NVME", type)))
    {
      set_err (EINVAL);
      return false;
    }
  
  // Find a free device number.
  for (devnum = 0; devnum < sizeof (devices) / sizeof (devices[0]); devnum++)
    if (! devices[devnum].ioob)
      break;
  if (devnum == sizeof (devices) / sizeof (devices[0]))
    {
      set_err (EMFILE);
      return false;
    }
  
  devname = NULL;
  if (strncmp (pathname, "/dev/rdisk", 10) == 0)
    devname = pathname + 6;
  else if (strncmp (pathname, "/dev/disk", 9) == 0)
    devname = pathname + 5;
  else if (strncmp (pathname, "disk", 4) == 0)
    // allow user to just say 'disk0'
    devname = pathname;

  // Find the device. This part should be the same for the NVMe and ATA
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
      set_err(ENOENT);
      return false;
    }
  // Find a SMART-capable driver which is a parent of this device.
  while (! is_smart_capable (disk, type))
    {
      IOReturn err;
      io_object_t prevdisk = disk;

      // Find this device's parent and try again.
      err = IORegistryEntryGetParentEntry (disk, kIOServicePlane, &disk);
      if (err != kIOReturnSuccess || ! disk)
      {
        set_err(ENODEV);
        IOObjectRelease (prevdisk);
        return false;
      }
    }
  
  devices[devnum].ioob = disk;

  {
    SInt32 dummy;
  
    devices[devnum].plugin = NULL;
    devices[devnum].smartIf = NULL;
    devices[devnum].smartIfNVMe = NULL;

    CFUUIDRef pluginType = NULL;
    CFUUIDRef smartInterfaceId = NULL;
    void ** SMARTptr = NULL;

    if (!strcmp("ATA", type))  {
      pluginType = kIOATASMARTUserClientTypeID;
      smartInterfaceId = kIOATASMARTInterfaceID;
      SMARTptr = (void **)&devices[devnum].smartIf;
    }
    else if (!strcmp("NVME", type)) {
      pluginType = kIONVMeSMARTUserClientTypeID;
      smartInterfaceId = kIONVMeSMARTInterfaceID;
      SMARTptr = (void **)&devices[devnum].smartIfNVMe;
    }

    // Create an interface to the ATA SMART library.
    if (IOCreatePlugInInterfaceForService (disk,
      pluginType,
      kIOCFPlugInInterfaceID,
      &devices[devnum].plugin,
      &dummy) == kIOReturnSuccess)
    (*devices[devnum].plugin)->QueryInterface
    (devices[devnum].plugin,
      CFUUIDGetUUIDBytes ( smartInterfaceId),
      SMARTptr);
    else
      return set_err(ENOSYS, "IOCreatePlugInInterfaceForService failed");
  }


  m_fd = devnum;
  if (m_fd < 0) {
    set_err((errno==ENOENT || errno==ENOTDIR) ? ENODEV : errno);
    return false;
  }
  return true;
}

bool darwin_smart_device::close()
{
  int fd = m_fd; m_fd = -1;
  if (devices[fd].smartIf)
    (*devices[fd].smartIf)->Release (devices[fd].smartIf);
  if (devices[fd].smartIfNVMe)
    (*devices[fd].smartIfNVMe)->Release (devices[fd].smartIfNVMe);
  if (devices[fd].plugin)
    IODestroyPlugInInterface (devices[fd].plugin);
  IOObjectRelease (devices[fd].ioob);
  devices[fd].ioob = MACH_PORT_NULL;
  return true;
}

// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number N of devices, or -1 if out of
// memory. Allocates N+1 arrays: one of N pointers (devlist); the
// other N arrays each contain null-terminated character strings.  In
// the case N==0, no arrays are allocated because the array of 0
// pointers has zero length, equivalent to calling malloc(0).
static int make_device_names (char*** devlist, const char* name) {
  IOReturn err;
  io_iterator_t i;
  io_object_t device = MACH_PORT_NULL;
  int result;
  int index;

  if (!(strcmp("ATA", name) || strcmp("NVME", name))) {
    return 0;
  }

  err = IOServiceGetMatchingServices 
    (kIOMasterPortDefault, IOServiceMatching (kIOBlockStorageDeviceClass), &i);
  if (err != kIOReturnSuccess)
    return -1;

  // Count the devices.
  result = 0;
  while ((device = IOIteratorNext (i)) != MACH_PORT_NULL) {
    if (is_smart_capable (device, name))
      result++;
    IOObjectRelease (device);
  }

  // Create an array of service names.
  IOIteratorReset (i);
  if (! result)
    goto error;
  *devlist = (char**)calloc (result, sizeof (char *)); 
  index = 0;
  while ((device = IOIteratorNext (i)) != MACH_PORT_NULL) {
    if (is_smart_capable (device, name))
    {
      io_string_t devName;
      IORegistryEntryGetPath(device, kIOServicePlane, devName);
      (*devlist)[index] = strdup (devName);
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
      free ((*devlist)[index]);
      free (*devlist);
    }
  if(!result) // no devs found
    return 0;

  return -1;
}

/////////////////////////////////////////////////////////////////////////////
/// Implement standard ATA support

class darwin_ata_device
: public /*implements*/ ata_device,
  public /*extends*/ darwin_smart_device
{
public:
  darwin_ata_device(smart_interface * intf, const char * dev_name, const char * req_type);
  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

protected:
  // virtual int ata_command_interface(smart_command_set command, int select, char * data);
};

darwin_ata_device::darwin_ata_device(smart_interface * intf, const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "ata", req_type),
  darwin_smart_device("ATA")
{
}

bool darwin_ata_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  if (!ata_cmd_is_ok(in,
    true, // data_out_support
    true, // multi_sector_support
    false) // not supported by API
    )
  return false;
  
  int select = 0;
  char * data = (char *)in.buffer;
  int fd = get_fd();
  IOATASMARTInterface **ifp = devices[fd].smartIf;
  IOATASMARTInterface *smartIf;
  IOReturn err;
  int timeoutCount = 5;
  int rc = 0;
  
  if (! ifp)
    return -1;
  smartIf = *ifp;
  clear_err(); errno = 0;
  do {
  switch (in.in_regs.command) {
    case ATA_IDENTIFY_DEVICE:
      {
        UInt32 dummy;
        err = smartIf->GetATAIdentifyData (ifp, data, 512, &dummy);
        if (err != kIOReturnSuccess && err != kIOReturnTimeout
          && err != kIOReturnNotResponding)
        printf ("identify failed: %#x\n", (unsigned) rc);
        if (err == kIOReturnSuccess && isbigendian())
        {
          int i;
          /* The system has already byte-swapped, undo it.  */
          for (i = 0; i < 256; i+=2)
            swap2 (data + i);
        }
      }
      break;
    case ATA_IDENTIFY_PACKET_DEVICE:
    case ATA_CHECK_POWER_MODE:
      errno = ENOTSUP;
      err = -1;
      break;
    case ATA_SMART_CMD:
      switch (in.in_regs.features) {
      case ATA_SMART_READ_VALUES:
        err = smartIf->SMARTReadData (ifp, (ATASMARTData *)data);
        break;
      case ATA_SMART_READ_THRESHOLDS:
        err = smartIf->SMARTReadDataThresholds (ifp, 
          (ATASMARTDataThresholds *)data);
        break;
      case ATA_SMART_READ_LOG_SECTOR:
        err = smartIf->SMARTReadLogAtAddress (ifp, in.in_regs.lba_low, data, 512 * in.in_regs.sector_count);
        break;
      case ATA_SMART_WRITE_LOG_SECTOR:
        err = smartIf->SMARTWriteLogAtAddress (ifp, in.in_regs.lba_low, data, 512 * in.in_regs.sector_count);
        break;
      case ATA_SMART_ENABLE:
      case ATA_SMART_DISABLE:
        err = smartIf->SMARTEnableDisableOperations (ifp, in.in_regs.features == ATA_SMART_ENABLE);
        break;
      case ATA_SMART_STATUS:
        if (in.out_needed.lba_high) // statuscheck
        {
          Boolean is_failing;
          err = smartIf->SMARTReturnStatus (ifp, &is_failing);
          if (err == kIOReturnSuccess && is_failing) {
            err = -1; // thresholds exceeded condition
            out.out_regs.lba_high = 0x2c; out.out_regs.lba_mid = 0xf4;
          }
          else
            out.out_regs.lba_high = 0xc2; out.out_regs.lba_mid = 0x4f;
          break;
        }
        else err = 0;
        break;
      case ATA_SMART_AUTOSAVE:
        err = smartIf->SMARTEnableDisableAutosave (ifp, 
          (in.in_regs.sector_count == 241 ? true : false));
        break;
      case ATA_SMART_IMMEDIATE_OFFLINE:
        select = in.in_regs.lba_low;
        if (select != SHORT_SELF_TEST && select != EXTEND_SELF_TEST)
        {
          errno = EINVAL;
          return set_err(ENOSYS, "Unsupported SMART self-test mode");
        }
        err = smartIf->SMARTExecuteOffLineImmediate (ifp, 
          select == EXTEND_SELF_TEST);
        break;
      case ATA_SMART_AUTO_OFFLINE:
        return set_err(ENOSYS, "SMART command not supported");
      default:
        return set_err(ENOSYS, "Unknown SMART command");
      }
      break;
    default:
      return set_err(ENOSYS, "Non-SMART commands not implemented");
    }
  } while ((err == kIOReturnTimeout || err == kIOReturnNotResponding)
  && timeoutCount-- > 0);
  if (err == kIOReturnExclusiveAccess)
    errno = EBUSY;
  rc = err == kIOReturnSuccess ? 0 : -1;
  if (rc < 0) {
    if (!get_errno())
      set_err(errno);
    return false;
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Implement platform interface

class darwin_smart_interface
: public /*implements*/ smart_interface
{
public:
  virtual std::string get_os_version_str();

  virtual std::string get_app_examples(const char * appname);

  virtual bool scan_smart_devices(smart_device_list & devlist, const char * type,
    const char * pattern = 0);

protected:
  virtual ata_device * get_ata_device(const char * name, const char * type);
  
  virtual scsi_device * get_scsi_device(const char * name, const char * type);

  virtual nvme_device * get_nvme_device(const char * name, const char * type,
    unsigned nsid);

  virtual smart_device * autodetect_smart_device(const char * name);

};

/////////////////////////////////////////////////////////////////////////////
/// NVMe support

class darwin_nvme_device
: public /*implements*/ nvme_device,
  public /*extends*/ darwin_smart_device
{
public:
  darwin_nvme_device(smart_interface * intf, const char * dev_name,
    const char * req_type, unsigned nsid);

  virtual bool nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out);
};

darwin_nvme_device::darwin_nvme_device(smart_interface * intf, const char * dev_name,
  const char * req_type, unsigned nsid)
: smart_device(intf, dev_name, "nvme", req_type),
  nvme_device(nsid),
  darwin_smart_device("NVME")
{
}

bool darwin_nvme_device::nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out)
{
  ARGUSED(out);
  int fd = get_fd();
  IONVMeSMARTInterface **ifp = devices[fd].smartIfNVMe;
  IONVMeSMARTInterface *smartIfNVMe ;
  IOReturn err = 0;
  unsigned int page = in.cdw10 & 0xff;

  if (! ifp)
    return -1;
  smartIfNVMe = *ifp;
  // currently only GetIdentifyData and SMARTReadData are supported
  switch (in.opcode) {
    case smartmontools::nvme_admin_identify:
      err = smartIfNVMe->GetIdentifyData(ifp, (struct nvme_id_ctrl *) in.buffer, in.nsid); // FIXME
      break;
    case smartmontools::nvme_admin_get_log_page:
       if(page == 0x02)
         err = smartIfNVMe->SMARTReadData(ifp, (struct nvme_smart_log *) in.buffer);
       else /* GetLogPage() is not working yet */
         return set_err(ENOSYS, "NVMe admin command:0x%02x/page:0x%02x is not supported",
          in.opcode, page);
      break;
    default:
      return set_err(ENOSYS, "NVMe admin command 0x%02x is not supported", in.opcode);
  }
  return true;
}
//////////////////////////////////////////////////////////////////////

std::string darwin_smart_interface::get_os_version_str()
{
  // now we are just getting darwin runtime version, to get OSX version more things needs to be done, see
  // http://stackoverflow.com/questions/11072804/how-do-i-determine-the-os-version-at-runtime-in-os-x-or-ios-without-using-gesta
  struct utsname osname;
  uname(&osname);
  return strprintf("%s %s %s", osname.sysname, osname.release, osname.machine);
}

std::string darwin_smart_interface::get_app_examples(const char * appname)
{
  if (!strcmp(appname, "smartctl"))
    return smartctl_examples;
  return ""; // ... so don't print again.
}

ata_device * darwin_smart_interface::get_ata_device(const char * name, const char * type)
{
  return new darwin_ata_device(this, name, type);
}

scsi_device * darwin_smart_interface::get_scsi_device(const char *, const char *)
{
  return 0; // scsi devices are not supported [yet]
}

nvme_device * darwin_smart_interface::get_nvme_device(const char * name, const char * type,
  unsigned nsid)
{
  return new darwin_nvme_device(this, name, type, nsid);
}

smart_device * darwin_smart_interface::autodetect_smart_device(const char * name)
{ // TODO - refactor as a function
  // Acceptable device names are:
  // /dev/disk*
  // /dev/rdisk*
  // disk*
  // IOService:*
  // IODeviceTree:*
  const char *devname = NULL;
  io_object_t disk;
  
  if (strncmp (name, "/dev/rdisk", 10) == 0)
    devname = name + 6;
  else if (strncmp (name, "/dev/disk", 9) == 0)
    devname = name + 5;
  else if (strncmp (name, "disk", 4) == 0)
    // allow user to just say 'disk0'
    devname = name;
  // Find the device. This part should be the same for the NVMe and ATA
  if (devname) {
      CFMutableDictionaryRef matcher;
      matcher = IOBSDNameMatching (kIOMasterPortDefault, 0, devname);
      disk = IOServiceGetMatchingService (kIOMasterPortDefault, matcher);
  }
  else {
      disk = IORegistryEntryFromPath (kIOMasterPortDefault, name);
  }
  if (! disk) {
      return 0;
  }
  io_registry_entry_t tmpdisk=disk;
  
  
  while (! is_smart_capable (tmpdisk, "ATA"))
    {
      IOReturn err;
      io_object_t prevdisk = tmpdisk;

      // Find this device's parent and try again.
      err = IORegistryEntryGetParentEntry (tmpdisk, kIOServicePlane, &tmpdisk);
      if (err != kIOReturnSuccess || ! tmpdisk)
      {
        IOObjectRelease (prevdisk);
        break;
      }
    }
    if (tmpdisk)
      return new darwin_ata_device(this, name, "");
    tmpdisk=disk;
    while (! is_smart_capable (tmpdisk, "NVME"))
      {
        IOReturn err;
        io_object_t prevdisk = tmpdisk;

        // Find this device's parent and try again.
        err = IORegistryEntryGetParentEntry (tmpdisk, kIOServicePlane, &tmpdisk);
        if (err != kIOReturnSuccess || ! tmpdisk)
        {
          IOObjectRelease (prevdisk);
          break;
        }
      }  
    if (tmpdisk)
      return new darwin_nvme_device(this, name, "", 0);

  // try ATA as a last option, for compatibility
  return new darwin_ata_device(this, name, "");
}

static void free_devnames(char * * devnames, int numdevs)
{
  for (int i = 0; i < numdevs; i++)
    free(devnames[i]);
  free(devnames);
}

bool darwin_smart_interface::scan_smart_devices(smart_device_list & devlist,
  const char * type, const char * pattern /*= 0*/)
{
  if (pattern) {
    set_err(EINVAL, "DEVICESCAN with pattern not implemented yet");
    return false;
  }

  // Make namelists
  char * * atanames = 0; int numata = 0;
  if (!type || !strcmp(type, "ata")) {
    numata = make_device_names(&atanames, "ATA");
    if (numata < 0) {
      set_err(ENOMEM);
      return false;
    }
  }
  char * * nvmenames = 0; int numnvme = 0;
  if (
#ifdef WITH_NVME_DEVICESCAN // TODO: Remove when NVMe support is no longer EXPERIMENTAL
      !type ||
#else
      type &&
#endif
               !strcmp(type, "nvme")) {
    numnvme = make_device_names(&nvmenames, "NVME");
    if (numnvme < 0) {
      set_err(ENOMEM);
      return false;
    }
  }

  // Add to devlist
  int i;
  if (!type)
    type="";
  for (i = 0; i < numata; i++) {
    ata_device * atadev = get_ata_device(atanames[i], type);
    if (atadev)
      devlist.push_back(atadev);
  }
  free_devnames(atanames, numata);

  for (i = 0; i < numnvme; i++) {
    nvme_device * nvmedev = get_nvme_device(nvmenames[i], type, 0); // default nsid
    if (nvmedev)
      devlist.push_back(nvmedev);
  }  
  free_devnames(nvmenames, numnvme);

  return true;
}

} // namespace


/////////////////////////////////////////////////////////////////////////////
/// Initialize platform interface and register with smi()

void smart_interface::init()
{
  static os::darwin_smart_interface the_interface;
  smart_interface::set(&the_interface);
}
