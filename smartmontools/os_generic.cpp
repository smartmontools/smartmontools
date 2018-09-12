/*
 * os_generic.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) YEAR YOUR_NAME
 * Copyright (C) 2003-8 Bruce Allen
 * Copyright (C) 2008-18 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


/*
    NOTE: The code in this file is only called when smartmontools has
    been compiled on an unrecognized/unsupported platform.  This file
    can then serve as a "template" to make os_myOS.cpp if you wish to
    build support for that platform.


 PORTING NOTES AND COMMENTS
 --------------------------

 To port smartmontools to the OS of your choice, please:

 [0] Contact smartmontools-support@listi.jpberlin.de to check
     that it's not already been done.

 [1] Make copies of os_generic.h and os_generic.cpp called os_myOS.h
     and os_myOS.cpp .

 [2] Modify configure.in so that case "${host}" includes myOS.

 [3] Verify that ./autogen.sh && ./configure && make compiles the
     code.  If not, fix any compilation problems.  If your OS lacks
     some function that is used elsewhere in the code, then add a
     AC_CHECK_FUNCS([missingfunction]) line to configure.in, and
     surround uses of the function with:
     #ifdef HAVE_MISSINGFUNCTION
     ... 
     #endif
     where the macro HAVE_MISSINGFUNCTION is (or is not) defined in
     config.h.

 [4] Now that you have a working build environment, you have to
     replace the 'stub' function calls provided in this file.

     Provide the functions defined in this file by fleshing out the
     skeletons below.

 [5] Contact smartmontools-support@listi.jpberlin.de to see
     about checking your code into the smartmontools CVS archive.
*/

/*
 Developer's note: for testing this file, use an unsupported system,
 for example: ./configure --build=rs6000-ibm-aix && make
*/


// This is needed for the various HAVE_* macros and PROJECT_* macros.
#include "config.h"

// These are needed to define prototypes and structures for the
// functions defined below
#include "atacmds.h"
#include "utility.h"

// This is to include whatever structures and prototypes you define in
// os_generic.h
#include "os_generic.h"

// Needed by '-V' option (CVS versioning) of smartd/smartctl.  You
// should have one *_H_CVSID macro appearing below for each file
// appearing with #include "*.h" above.  Please list these (below) in
// alphabetic/dictionary order.
const char * os_XXXX_cpp_cvsid="$Id$"
  ATACMDS_H_CVSID CONFIG_H_CVSID OS_GENERIC_H_CVSID UTILITY_H_CVSID;

// This is here to prevent compiler warnings for unused arguments of
// functions.
#define ARGUSED(x) ((void)(x))

// print examples for smartctl.  You should modify this function so
// that the device paths are sensible for your OS, and to eliminate
// unsupported commands (eg, 3ware controllers).
static void print_smartctl_examples(){
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

/////////////////////////////////////////////////////////////////////////////

namespace generic { // No need to publish anything, name provided for Doxygen

class generic_smart_interface
: public /*implements*/ smart_interface
{
public:
#ifdef HAVE_GET_OS_VERSION_STR
  virtual const char * get_os_version_str();
#endif

  virtual std::string get_app_examples(const char * appname);

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

#ifdef HAVE_GET_OS_VERSION_STR
/// Return build host and OS version as static string
const char * generic_smart_interface::get_os_version_str()
{
  return ::get_os_version_str();
}
#endif

std::string generic_smart_interface::get_app_examples(const char * appname)
{
  if (!strcmp(appname, "smartctl"))
    ::print_smartctl_examples(); // this prints to stdout ...
  return ""; // ... so don't print again.
}

// Return ATA device object for the given device name or NULL
// the type is always set to "ata"
ata_device * generic_smart_interface::get_ata_device(const char * name, const char * type)
{
  ARGUSED(name);
  ARGUSED(type);
  return NULL;
}

// Return SCSI device object for the given device name or NULL
// the type is always set to "scsi"
scsi_device * generic_smart_interface::get_scsi_device(const char * name, const char * type)
{
  ARGUSED(name);
  ARGUSED(type);
  return NULL;
}


// Return device object for the given device name (autodetect the device type)
smart_device * generic_smart_interface::autodetect_smart_device(const char * name)
{
  ARGUSED(name);
  // for the given name return the apropriate device type 
  return NULL;
}


// Fill devlist with all OS's disk devices of given type that match the pattern
bool generic_smart_interface::scan_smart_devices(smart_device_list & devlist,
  const char * type, const char * pattern /*= 0*/)
{
  ARGUSED(devlist);
  ARGUSED(type);
  ARGUSED(pattern);
  return false;
}


// Return device object of the given type with specified name or NULL
smart_device * generic_smart_interface::get_custom_smart_device(const char * name, const char * type)
{
  ARGUSED(name);
  ARGUSED(type);
  return NULL;
}

std::string generic_smart_interface::get_valid_custom_dev_types_str()
{
  return "";
}

} // namespace


/////////////////////////////////////////////////////////////////////////////
/// Initialize platform interface and register with smi()

void smart_interface::init()
{
  static generic::generic_smart_interface the_interface;
  smart_interface::set(&the_interface);
}
