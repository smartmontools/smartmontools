#ifndef _OS_FREEBSD_H_
#define _OS_FREEBSD_H_

#include "config.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

#define OS_XXXX_H_CVSID "$Id: os_freebsd.h,v 1.6 2003/10/11 05:41:02 arvoreen Exp $\n"

#include <sys/ata.h>

struct freebsd_dev_channel {
  int	channel;		// the ATA channel to work with
  int	device;			// the device on the channel
  int	atacommand;		// the ATA Command file descriptor (/dev/ata)
  char* devname;		// the SCSI device name
  int   unitnum;		// the SCSI unit number
  int	scsicontrol;		// the SCSI control interface
};

#define FREEBSD_MAXDEV 64
#define FREEBSD_FDOFFSET 16;
#define MAX_NUM_DEV 26


#endif /* _OS_FREEBSD_H_ */
