/*
 * os_osf1.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
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

#include "config.h"
#include "int64.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"
#include "os_osf1.h"

const char *os_XXXX_c_cvsid="$Id: os_osf1.cpp,v 1.1.2.1 2006/10/06 18:09:44 shattered Exp $" \
ATACMDS_H_CVSID OS_XXXX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;


#define OSF1_MAXDEV 64
#define AUTO_SENSE_LEN 32

static char *camdev = "/dev/cam";
static int camfd;
static int camopened = 0;

static struct {
  int inUse;
  int bus;
  int tgt;
  int lun;
} devicetable[OSF1_MAXDEV];

// print examples for smartctl.  You should modify this function so
// that the device paths are sensible for your OS, and to eliminate
// unsupported commands (eg, 3ware controllers).
void print_smartctl_examples() {
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
      "  smartctl -a /dev/rdisk/dsk0c                (Prints all SMART information)\n\n"
      "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/rdisk/dsk0c\n"
      "                                              (Enables SMART on first disk)\n\n"
      "  smartctl -t long /dev/rdisk/dsk0c       (Executes extended disk self-test)\n\n"
      "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/rdisk/dsk0c\n"
      "                                      (Prints Self-Test & Attribute errors)\n"
        );
#else
  printf(
      "  smartctl -a /dev/rdisk/dsk0c                (Prints all SMART information)\n"
      "  smartctl -s on -o on -S on /dev/rdisk/dsk0c  (Enables SMART on first disk)\n"
      "  smartctl -t long /dev/rdisk/dsk0c       (Executes extended disk self-test)\n"
      "  smartctl -A -l selftest -q errorsonly /dev/rdisk/dsk0c\n"
      "                                      (Prints Self-Test & Attribute errors)\n"
        );
#endif
  return;
}

// tries to guess device type given the name (a path).  See utility.h
// for return values.
int guess_device_type(const char* dev_name) {
  int fd = open(dev_name, O_RDWR, 0);
  int ctrl_type = CONTROLLER_UNKNOWN;
  if (fd > 0) {
    device_info_t devinfo;
    bzero(&devinfo, sizeof(devinfo));

    if (ioctl(fd, DEVGETINFO, &devinfo) == 0) {
      if (strncmp("SCSI", devinfo.v1.interface, DEV_STRING_SIZE) == 0)
        ctrl_type = CONTROLLER_SCSI;
      else if (strncmp("EIDE", devinfo.v1.interface, DEV_STRING_SIZE) == 0)
        ctrl_type = CONTROLLER_ATA;
    }
    close (fd);
  }
  return ctrl_type;
}

extern long long bytes;

int make_device_names(char*** devlist, const char* name) {
  int n = 0, i;
  char **mp;
  struct {
    char *dir;
    char *name;
  } checklist[] = {
    {"/dev/rdisk", "dsk"},
    {"/dev", "rrz"},
    {NULL, NULL}
  };

  *devlist = NULL;
  if (!(mp = malloc(OSF1_MAXDEV * sizeof(char *)))) {
    pout("Out of memory constructing scan device list\n");
    return -1;
  }
  for (i = 0; checklist[i].dir; ++i) {
    DIR *devdir;
    struct dirent *devent;
    int name_len;

    devdir = opendir(checklist[i].dir);
    if (devdir) {
      name_len = strlen(checklist[i].name);
      while (devent = readdir(devdir))
        if (memcmp(checklist[i].name, devent->d_name, name_len) == 0)
          if (devent->d_name[devent->d_namlen-1] == 'c') {
            if (!(mp[n] = malloc(strlen(checklist[i].dir) + strlen(devent->d_name) + 2))) {
              pout("Out of memory constructing scan device list\n");
              return -1;
            }
            sprintf(mp[n], "%s/%s", checklist[i].dir, devent->d_name);
            if (guess_device_type(mp[n]) == CONTROLLER_SCSI) {
              if (strcmp(name, "SCSI")) {
                free(mp[n]);
                continue;
              }
            } else if (guess_device_type(mp[n]) == CONTROLLER_ATA) {
              if (strcmp(name, "ATA")) {
                free(mp[n]);
                continue;
              }
            } else {
              free(mp[n]);
              continue;
            }
            bytes += strlen(mp[n]) + 1;
            n++;
          }
      closedir(devdir);
    }
  }
  mp = realloc(mp, n * sizeof(char*));
  bytes += n * sizeof(char *);
  *devlist = mp;
  return n;
}

// Like open().  Return non-negative integer handle, only used by the
// functions below.  type=="ATA" or "SCSI".  If you need to store
// extra information about your devices, create a private internal
// array within this file (see os_freebsd.c for an example).  If you
// can not open the device (permission denied, does not exist, etc)
// set errno as open() does and return <0.
int deviceopen(const char *pathname, char *type) {
  int i;
  if (!camopened) {
    camfd = open(camdev, O_RDWR, 0);
    if (camfd < 0)
      return -1;
    camopened++;
    bzero(devicetable, sizeof(devicetable));
  }

  for (i = 0; i < OSF1_MAXDEV; i++) {
    if (!devicetable[i].inUse) {
      int fd = open(pathname, O_RDWR, 0);
      if (fd > 0) {
        device_info_t devinfo;
        bzero(&devinfo, sizeof(devinfo));
        if (ioctl(fd, DEVGETINFO, &devinfo) == 0) {
          devicetable[i].inUse++;
          devicetable[i].bus = devinfo.v1.businfo.bus.scsi.bus_num;
          devicetable[i].tgt = devinfo.v1.businfo.bus.scsi.tgt_id;
          devicetable[i].lun = devinfo.v1.businfo.bus.scsi.lun;
        }
        close (fd);
        return i;
      }
      return -1;
    }
  }
  return -1;
}

// Like close().  Acts only on integer handles returned by
// deviceopen() above.
int deviceclose(int fd) {
  int i;
  devicetable[fd].inUse = 0;
  for (i = 0; i < OSF1_MAXDEV; i++) {
    if (devicetable[i].inUse)
      break;
  }
  if (i == OSF1_MAXDEV) {
    close(camfd);
    camopened = 0;
  }

  return 0;
}

// Interface to ATA devices.  See os_linux.c for the cannonical example.
int ata_command_interface(int fd, smart_command_set command, int select, char *data) {
  return -1;
}

int marvell_command_interface(int fd, smart_command_set command, int select, char *data) {
  return -1;
}

int escalade_command_interface(int fd, int disknum, int escalade_type, smart_command_set command, int select, char *data) {
  return -1;
}

static int release_sim(int fd) {
  UAGT_CAM_CCB uagt;
  CCB_RELSIM relsim;
  int retval;

  bzero(&uagt, sizeof(uagt));
  bzero(&relsim, sizeof(relsim));

  uagt.uagt_ccb = (CCB_HEADER *) &relsim;
  uagt.uagt_ccblen = sizeof(relsim);
  uagt.uagt_buffer = NULL;
  uagt.uagt_buflen = 0;

  relsim.cam_ch.cam_ccb_len = sizeof(relsim);
  relsim.cam_ch.cam_func_code = XPT_REL_SIMQ;
  relsim.cam_ch.cam_flags = CAM_DIR_IN | CAM_DIS_CALLBACK;
  relsim.cam_ch.cam_path_id = devicetable[fd].bus;
  relsim.cam_ch.cam_target_id = devicetable[fd].tgt;
  relsim.cam_ch.cam_target_lun = devicetable[fd].lun;

  retval = ioctl(camfd, UAGT_CAM_IO, &uagt);
  if (retval < 0)
    pout("CAM ioctl error [Release SIM Queue]\n");
  return retval;
}

// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io *iop, int report) {
  CCB_SCSIIO ccb;
  UAGT_CAM_CCB uagt;
  int retval;
  uint8_t sensep[AUTO_SENSE_LEN];

  if (report > 0) {
    unsigned int k;
    const unsigned char * ucp = iop->cmnd;
    const char * np;

    np = scsi_get_opcode_name(ucp[0]);
    pout(" [%s: ", np ? np : "<unknown opcode>");
    for (k = 0; k < iop->cmnd_len; ++k)
      pout("%02x ", ucp[k]);
    if ((report > 1) && 
        (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
      int trunc = (iop->dxfer_len > 256) ? 1 : 0;

      pout("]\n  Outgoing data, len=%d%s:\n", (int)iop->dxfer_len,
          (trunc ? " [only first 256 bytes shown]" : ""));
      dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
    }
    else
      pout("]");
  }

  bzero(&uagt, sizeof(uagt));
  bzero(&ccb, sizeof(ccb));

  uagt.uagt_ccb = (CCB_HEADER *) &ccb;
  uagt.uagt_ccblen = sizeof(ccb);
  uagt.uagt_snsbuf = ccb.cam_sense_ptr = sensep;
  uagt.uagt_snslen = ccb.cam_sense_len = AUTO_SENSE_LEN;
  uagt.uagt_buffer = ccb.cam_data_ptr =  iop->dxferp;
  uagt.uagt_buflen = ccb.cam_dxfer_len = iop->dxfer_len;

  ccb.cam_timeout = iop->timeout;
  ccb.cam_ch.my_addr = (CCB_HEADER *) &ccb;
  ccb.cam_ch.cam_ccb_len = sizeof(ccb);
  ccb.cam_ch.cam_func_code = XPT_SCSI_IO;
  if (iop->dxfer_dir == DXFER_NONE)
    ccb.cam_ch.cam_flags = CAM_DIR_NONE;
  else if (iop->dxfer_dir == DXFER_TO_DEVICE)
      ccb.cam_ch.cam_flags = CAM_DIR_OUT;
  else if (iop->dxfer_dir == DXFER_FROM_DEVICE)
      ccb.cam_ch.cam_flags = CAM_DIR_IN;
  ccb.cam_cdb_len = iop->cmnd_len;
  memcpy(ccb.cam_cdb_io.cam_cdb_bytes, iop->cmnd, iop->cmnd_len);
  ccb.cam_ch.cam_path_id = devicetable[fd].bus;
  ccb.cam_ch.cam_target_id = devicetable[fd].tgt;
  ccb.cam_ch.cam_target_lun = devicetable[fd].lun;

  retval = ioctl(camfd, UAGT_CAM_IO, &uagt);
  if (retval < 0)
    pout("CAM ioctl error\n");

  iop->scsi_status = ccb.cam_ch.cam_status & CAM_STATUS_MASK;

  iop->resid = ccb.cam_resid;
  if (ccb.cam_ch.cam_status & CAM_AUTOSNS_VALID)
    if (iop->sensep) {
      iop->resp_sense_len = ccb.cam_sense_len - ccb.cam_resid;
      if (iop->resp_sense_len > iop->max_sense_len)
        iop->resp_sense_len = iop->max_sense_len;
      memcpy(iop->sensep, sensep, iop->resp_sense_len);
    }

  /* If the SIM queue is frozen, releases SIM queue. */
  if (ccb.cam_ch.cam_status & CAM_SIM_QFRZN)
    release_sim(fd);

  if (report > 0) {
    int trunc;

    pout("  status=0\n");
    trunc = (iop->dxfer_len > 256) ? 1 : 0;

    pout("  Incoming data, len=%d%s:\n", (int)iop->dxfer_len,
        (trunc ? " [only first 256 bytes shown]" : ""));
    dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
  }

  return retval;
}

#ifndef HAVE_STRSIGNAL
#include <signal.h>
char *strsignal(int sig) {
  char *sig_str = "unknown signal";
  if (0 <= sig && sig < 64)
    sig_str = __sys_siglist[sig];
  return sig_str;
}
#endif
