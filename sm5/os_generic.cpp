/*
 * os_generic.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) YEAR YOUR_NAME <smartmontools-support@lists.sourceforge.net>
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

/*  PORTING NOTES AND COMMENTS

    To port smartmontools to the OS of your choice, please:

 [0] Contact smartmontools-support@lists.sourceforge.net to check
     that it's not already been done.

 [1] Make copies of os_generic.[hc] called os_myOS.[hc].

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

 [4] Provide the functions defined in this file by fleshing out the
     skeletons below.  You can entirely eliminate the function
     'unsupported()'.

 [5] Contact smartmontools-support@lists.sourceforge.net to see
     about checking your code into the smartmontools CVS archive.
*/

// These are needed to define prototypes for the functions defined below
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

// This is to include whatever prototypes you define in os_generic.h
#include "os_generic.h"

// Needed by '-V' option (CVS versioning) of smartd/smartctl
const char *os_XXXX_c_cvsid="$Id: os_generic.cpp,v 1.14 2004/07/16 05:55:00 ballen4705 Exp $" \
ATACMDS_H_CVSID OS_XXXX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;


// Please eliminate the following block: both the two #includes and
// the 'unsupported()' function.  They are only here to warn
// unsuspecting users that their Operating System is not supported! If
// you wish, you can use a similar warning mechanism for any of the
// functions in this file that you can not (or choose not to)
// implement.

#include "config.h"
#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

static void unsupported(){
  static int warninggiven;

  if (!warninggiven) {
    char *osname;
    extern unsigned char debugmode;
    unsigned char savedebugmode=debugmode;
    
#ifdef HAVE_UNAME
    struct utsname ostype;
    uname(&ostype);
    osname=ostype.sysname;
#else
    osname="host's";
#endif

    debugmode=1;
    pout("\n"
         "############################################################################\n"
         "WARNING: smartmontools has not been ported to the %s Operating System.\n"
         "Please see the files os_generic.c and os_generic.h for porting instructions.\n"
         "############################################################################\n\n",
         osname);
    debugmode=savedebugmode;
    warninggiven=1;
  }
  
  return;
}
// End of the 'unsupported()' block that you should eliminate.


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

// tries to guess device type given the name (a path).  See utility.h
// for return values.
int guess_device_type (const char* dev_name) {
  unsupported();
  return GUESS_DEVTYPE_DONT_KNOW;
}

// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number N of devices, or -1 if out of
// memory. Allocates N+1 arrays: one of N pointers (devlist); the
// other N arrays each contain null-terminated character strings.  In
// the case N==0, no arrays are allocated because the array of 0
// pointers has zero length, equivalent to calling malloc(0).
int make_device_names (char*** devlist, const char* name) {
  unsupported();
  return 0;
}

// Like open().  Return non-negative integer handle, only used by the
// functions below.  type=="ATA" or "SCSI".  If you need to store
// extra information about your devices, create a private internal
// array within this file (see os_freebsd.c for an example).  If you
// can not open the device (permission denied, does not exist, etc)
// set errno as open() does and return <0.
int deviceopen(const char *pathname, char *type){
  unsupported();
  return -1;
}

// Like close().  Acts only on integer handles returned by
// deviceopen() above.
int deviceclose(int fd){
  unsupported();
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
int ata_command_interface(int fd, smart_command_set command, int select, char *data){
  unsupported();
  return -1;
}

// Interface to ATA devices behind 3ware escalade RAID controller
// cards.  Same description as ata_command_interface() above except
// that 0 <= disknum <= 15 specifies the ATA disk attached to the
// controller.
int escalade_command_interface(int fd, int disknum, int escalade_type, smart_command_set command, int select, char *data){
  unsupported();
  return -1;
}

#include <errno.h>
// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report) {
  unsupported();
  return -ENOSYS;
}
