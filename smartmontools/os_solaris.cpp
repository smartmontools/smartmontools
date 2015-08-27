/*
 * os_solaris.c
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2003-8 SAWADA Keiji <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2003-8 Casper Dik <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/param.h>

// These are needed to define prototypes for the functions defined below
#include "config.h"
#include "int64.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

// This is to include whatever prototypes you define in os_solaris.h
#include "os_solaris.h"

#define ARGUSED(x) ((void)(x))

extern long long bytes;

static const char *filenameandversion="$Id$";

const char *os_XXXX_c_cvsid="$Id$" \
ATACMDS_H_CVSID CONFIG_H_CVSID INT64_H_CVSID OS_SOLARIS_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

// The printwarning() function warns about unimplemented functions
int printedout[2];
char *unimplemented[2]={
  "ATA command routine ata_command_interface()",
  "3ware Escalade Controller command routine escalade_command_interface()",
};

int printwarning(int which){
  if (!unimplemented[which])
    return 0;

  if (printedout[which])
    return 1;
  
  printedout[which]=1;
  
  pout("\n"
       "#######################################################################\n"
       "%s NOT IMPLEMENTED under Solaris.\n"
       "Please contact " PACKAGE_BUGREPORT " if\n"
       "you want to help in porting smartmontools to Solaris.\n"
       "#######################################################################\n"
       "\n",
       unimplemented[which]);

  return 1;
}

// print examples for smartctl
void print_smartctl_examples(){
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
         "  smartctl -a /dev/rdsk/c0t0d0s0             (Prints all SMART information)\n\n"
         "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/rdsk/c0t0d0s0\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long /dev/rdsk/c0t0d0s0 (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/rdsk/c0t0d0s0\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         );
#else
  printf(
         "  smartctl -a /dev/rdsk/c0t0d0s0               (Prints all SMART information)\n"
         "  smartctl -s on -o on -S on /dev/rdsk/c0t0d0s0 (Enables SMART on first disk)\n"
         "  smartctl -t long /dev/rdsk/c0t0d0s0      (Executes extended disk self-test)\n"
         "  smartctl -A -l selftest -q errorsonly /dev/rdsk/c0t0d0s0\n"
         "                                        (Prints Self-Test & Attribute errors)\n"
         );
#endif
  return;
}

static const char *uscsidrvrs[] = {
        "sd",
        "ssd",
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
                bytes += 16*sizeof(char *);
        }
        if (!(res->names[res->nnames-1] = CustomStrDup((char *)path, 1, __LINE__, filenameandversion)))
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
	bytes -= sizeof(char *)*(res.maxnames-res.nnames);

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

#if defined(__sparc)
// swap each 2-byte pairs in a sector
static void swap_sector(void *p)
{
    int i;
    char t, *cp = static_cast<char*>(p);
    for(i = 0; i < 256; i++) {
        t = cp[0]; cp[0] = cp[1]; cp[1] = t;
        cp += 2;
    }
}
#endif

// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int fd, smart_command_set command, int select, char *data){
#if defined(__sparc)
    int err;
 
    switch (command){
    case CHECK_POWER_MODE:
	/* currently not recognized */
	return -1;
    case READ_VALUES:
	return smart_read_data(fd, data);
    case READ_THRESHOLDS:
	return smart_read_thresholds(fd, data);
    case READ_LOG:
	return smart_read_log(fd, select, 1, data);
    case IDENTIFY:
	err = ata_identify(fd, data);
	if(err) return err;
	swap_sector(static_cast<void*>(data));
	return 0;
    case PIDENTIFY:
	err = ata_pidentify(fd, data);
	if(err) return err;
	swap_sector(static_cast<void*>(data));
	return 0;
    case ENABLE:
	return smart_enable(fd);
    case DISABLE:
	return smart_disable(fd);
    case STATUS:
	return smart_status(fd);
    case AUTO_OFFLINE:
	return smart_auto_offline(fd, select);
    case AUTOSAVE:
	return smart_auto_save(fd, select);
    case IMMEDIATE_OFFLINE:
	return smart_immediate_offline(fd, select);
    case STATUS_CHECK:
	return smart_status_check(fd);
    default:
	pout("Unrecognized command %d in ata_command_interface() of os_solaris.c\n", command);
	EXIT(1);
	break;
    }
#else /* __sparc */
    ARGUSED(fd); ARGUSED(command); ARGUSED(select); ARGUSED(data);

    /* Above smart_* routines uses undocumented ioctls of "dada"
     * driver, which is specific to SPARC Solaris.  See
     * os_solaris_ata.s for further details. x86 Solaris seems not to
     * provide similar or alternative interface... */
    if (printwarning(0))
	return -1;
#endif
    return -1;
}

#include <errno.h>
#include <sys/scsi/generic/commands.h>
#include <sys/scsi/generic/status.h>
#include <sys/scsi/impl/types.h>
#include <sys/scsi/impl/uscsi.h>

// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report)
{
  struct uscsi_cmd uscsi;

  if (report > 0) {
    int k;
    const unsigned char * ucp = iop->cmnd;
    const char * np;

    np = scsi_get_opcode_name(ucp[0]);
    pout(" [%s: ", np ? np : "<unknown opcode>");
    for (k = 0; k < (int)iop->cmnd_len; ++k)
      pout("%02x ", ucp[k]);
    pout("]\n");
    if ((report > 1) && 
        (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
      int trunc = (iop->dxfer_len > 256) ? 1 : 0;

      pout("  Outgoing data, len=%d%s:\n", (int)iop->dxfer_len,
           (trunc ? " [only first 256 bytes shown]" : ""));
      dStrHex((char *)iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
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
  uscsi.uscsi_flags |= (USCSI_ISOLATE | USCSI_RQENABLE);

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
          dStrHex((const char *)iop->sensep, ((len > 252) ? 252 : len) , 1);
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
        dStrHex((char *)iop->dxferp, (trunc ? 256 : len) , 1);
      }
    }
  }
  return 0;
}
