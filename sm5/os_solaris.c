#include "os_freebsd.h"
#include "os_freebsd.h"
#include "utility.h"

const char *os_XXXX_c_cvsid="$Id: os_solaris.c,v 1.1 2003/10/08 01:30:00 ballen4705 Exp $" OS_XXXX_H_CVSID UTILITY_H_CVSID;

// Like open().  Return positive integer handle, used by functions below only.  type="ATA" or "SCSI".
int deviceopen(const char *pathname, char *type){
  return -1;
}

// Like close().  Acts on handles returned by above function.
int deviceclose(int fd){
  return 0;
}

// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int fd, smart_command_set command, int select, char *data){
  return -1;
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c
int escalade_command_interface(int fd, int disknum, smart_command_set command, int select, char *data){
  return -1;
}

#include <errno.h>
// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report) {
  return -ENOSYS;
}
