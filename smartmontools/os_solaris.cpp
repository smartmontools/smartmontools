/*
 * os_solaris.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2003-08 SAWADA Keiji
 * Copyright (C) 2003-15 Casper Dik
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

// These are needed to define prototypes for the functions defined below
#include "config.h"

#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

const char * os_solaris_cpp_cvsid = "$Id$";

// print examples for smartctl
void print_smartctl_examples(){
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n"
         "  smartctl -a /dev/rdsk/c0t0d0s0             (Prints all SMART information)\n\n"
         "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/rdsk/c0t0d0s0\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long /dev/rdsk/c0t0d0s0 (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/rdsk/c0t0d0s0\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         );
  return;
}

static const char *uscsidrvrs[] = {
        "sd",
        "ssd",
        "disk",       // SATA devices
        "st"
};

static const char *atadrvrs[] = {
        "cmdk",
        "dad",
};

static int
isdevtype(const char *dev_name, const char *table[], int tsize)
{
  char devpath[MAXPATHLEN];
  int i;
  char *basename;

  if (realpath(dev_name, devpath) == NULL)
    return 0;
 
  if ((basename = strrchr(devpath, '/')) == NULL)
    return 0;

  basename++;

  for (i = 0; i < tsize; i++) {
    int l = strlen(table[i]);
    if (strncmp(basename, table[i], l) == 0 && basename[l] == '@')
      return 1;
  }
  return 0;
}

static int
isscsidev(const char *path)
{
  return isdevtype(path, uscsidrvrs, sizeof (uscsidrvrs) / sizeof (char *));
}

static int
isatadev(const char *path)
{
  return isdevtype(path, atadrvrs, sizeof (atadrvrs) / sizeof (char *));
}

// tries to guess device type given the name (a path)
int guess_device_type (const char* dev_name) {
  if (isscsidev(dev_name))
    return CONTROLLER_SCSI;
  else if (isatadev(dev_name))
    return CONTROLLER_ATA;
  else
    return CONTROLLER_UNKNOWN;
}

struct pathlist {
        char **names;
        int  nnames;
        int  maxnames;
};

static int
addpath(const char *path, struct pathlist *res)
{
        if (++res->nnames > res->maxnames) {
                res->maxnames += 16;
                res->names = static_cast<char**>(realloc(res->names, res->maxnames * sizeof (char *)));
                if (res->names == NULL)
                        return -1;
        }
        if (!(res->names[res->nnames-1] = strdup(path)))
                return -1;
        return 0;
}

static int 
grokdir(const char *dir, struct pathlist *res, int testfun(const char *))
{
        char pathbuf[MAXPATHLEN];
        size_t len;
        DIR *dp;
        struct dirent *de;
        int isdisk = strstr(dir, "dsk") != NULL;
        char *p;

        len = snprintf(pathbuf, sizeof (pathbuf), "%s/", dir);
        if (len >= sizeof (pathbuf))
                return -1;

        dp = opendir(dir);
        if (dp == NULL)
                return 0;

        while ((de = readdir(dp)) != NULL) {
                if (de->d_name[0] == '.')
                        continue;

                if (strlen(de->d_name) + len >= sizeof (pathbuf))
                        continue;

                if (isdisk) {
                        /* Disk represented by slice 0 */
                        p = strstr(de->d_name, "s0");
                        /* String doesn't end in "s0\0" */
                        if (p == NULL || p[2] != '\0')
                                continue;
                } else {
                        /* Tape drive represented by the all-digit device */
                        for (p = de->d_name; *p; p++)
                                if (!isdigit((int)(*p)))
                                        break;
                        if (*p != '\0')
                                continue;
                }
                strcpy(&pathbuf[len], de->d_name);
                if (testfun(pathbuf)) {
                        if (addpath(pathbuf, res) == -1) {
                                closedir(dp);
                                return -1;
                        }
                }
        }
        closedir(dp);

        return 0;
}

// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number of devices, or -1 if out of memory.
int make_device_names (char*** devlist, const char* name) {
        struct pathlist res;

        res.nnames = res.maxnames = 0;
        res.names = NULL;
        if (strcmp(name, "SCSI") == 0) {
                if (grokdir("/dev/rdsk", &res, isscsidev) == -1)
                        return -1;
                if (grokdir("/dev/rmt", &res, isscsidev) == -1)
                        return -1;
	} else if (strcmp(name, "ATA") == 0) {
                if (grokdir("/dev/rdsk", &res, isatadev) == -1)
                        return -1;
	} else {
                // non-SCSI and non-ATA case not implemented
                *devlist=NULL;
                return 0;
	}

	// shrink array to min possible size
	res.names = static_cast<char**>(realloc(res.names, res.nnames * sizeof (char *)));

	// pass list back
	*devlist = res.names;
	return res.nnames;
}

// Like open().  Return integer handle, used by functions below only.
// type="ATA" or "SCSI".
int deviceopen(const char *pathname, char *type){
  if (!strcmp(type,"SCSI")) 
    return open(pathname, O_RDWR | O_NONBLOCK);
  else if (!strcmp(type,"ATA")) 
    return open(pathname, O_RDONLY | O_NONBLOCK);
  else
    return -1;
}

// Like close().  Acts on handles returned by above function.
int deviceclose(int fd){
    return close(fd);
}

// Interface to ATA devices.
int ata_command_interface(int, smart_command_set, int, char *)
{
    pout("Device type 'ata' not implemented, try '-d sat' or '-d sat,12' instead.\n");
    errno = ENOSYS;
    return -1;
}

#include <errno.h>
#include <sys/scsi/generic/commands.h>
#include <sys/scsi/generic/status.h>
#include <sys/scsi/impl/types.h>
#include <sys/scsi/impl/uscsi.h>

// Interface to SCSI devices.
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report)
{
  struct uscsi_cmd uscsi;

  if (report > 0) {
    int k;
    const unsigned char * ucp = iop->cmnd;
    const char * np;

    np = scsi_get_opcode_name(ucp);
    pout(" [%s: ", np ? np : "<unknown opcode>");
    for (k = 0; k < (int)iop->cmnd_len; ++k)
      pout("%02x ", ucp[k]);
    pout("]\n");
    if ((report > 1) && 
        (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
      int trunc = (iop->dxfer_len > 256) ? 1 : 0;

      pout("  Outgoing data, len=%d%s:\n", (int)iop->dxfer_len,
           (trunc ? " [only first 256 bytes shown]" : ""));
      dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
    }
  }
  memset(&uscsi, 0, sizeof (uscsi));

  uscsi.uscsi_cdb = reinterpret_cast<char*>(iop->cmnd);
  uscsi.uscsi_cdblen = iop->cmnd_len;
  if (iop->timeout == 0)
    uscsi.uscsi_timeout = 60; /* 60 seconds */
  else
    uscsi.uscsi_timeout = iop->timeout;
  uscsi.uscsi_bufaddr = reinterpret_cast<char*>(iop->dxferp);
  uscsi.uscsi_buflen = iop->dxfer_len;
  uscsi.uscsi_rqbuf = reinterpret_cast<char*>(iop->sensep);
  uscsi.uscsi_rqlen = iop->max_sense_len;

  switch (iop->dxfer_dir) {
  case DXFER_NONE:
  case DXFER_FROM_DEVICE:
    uscsi.uscsi_flags = USCSI_READ;
    break;
  case DXFER_TO_DEVICE:
    uscsi.uscsi_flags = USCSI_WRITE;
    break;
  default:
    return -EINVAL;
  }
  uscsi.uscsi_flags |= (USCSI_ISOLATE | USCSI_RQENABLE | USCSI_SILENT);

  if (ioctl(fd, USCSICMD, &uscsi)) {
    int err = errno;

    if (! ((EIO == err) && uscsi.uscsi_status))
      return -err;
    /* errno is set to EIO when a non-zero SCSI completion status given */
  }

  iop->scsi_status = uscsi.uscsi_status;
  iop->resid = uscsi.uscsi_resid;
  iop->resp_sense_len = iop->max_sense_len - uscsi.uscsi_rqresid;

  if (report > 0) {
    int trunc;
    int len = iop->resp_sense_len;

    if ((SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) &&
        iop->sensep && (len > 3)) {
      if ((iop->sensep[0] & 0x7f) > 0x71)
        pout("  status=%x: [desc] sense_key=%x asc=%x ascq=%x\n",
             iop->scsi_status, iop->sensep[1] & 0xf,
             iop->sensep[2], iop->sensep[3]);
      else
        pout("  status=%x: sense_key=%x asc=%x ascq=%x\n",
             iop->scsi_status, iop->sensep[2] & 0xf,
             iop->sensep[12], iop->sensep[13]);
      if (report > 1) {
          pout("  >>> Sense buffer, len=%d:\n", len);
          dStrHex(iop->sensep, ((len > 252) ? 252 : len) , 1);
      }
    } else if (iop->scsi_status)
      pout("  status=%x\n", iop->scsi_status);
    if (iop->resid)
      pout("  dxfer_len=%d, resid=%d\n", iop->dxfer_len, iop->resid);
    if (report > 1) {
      len = iop->dxfer_len - iop->resid;
      if (len > 0) {
        trunc = (len > 256) ? 1 : 0;
        pout("  Incoming data, len=%d%s:\n", len,
             (trunc ? " [only first 256 bytes shown]" : ""));
        dStrHex(iop->dxferp, (trunc ? 256 : len) , 1);
      }
    }
  }
  return 0;
}
