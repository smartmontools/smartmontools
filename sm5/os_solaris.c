/*
 * os_solaris.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2003-4 Casper Dik <smartmontools-support@lists.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
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
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

// This is to include whatever prototypes you define in os_solaris.h
#include "os_solaris.h"

extern long long bytes;

static const char *filenameandversion="$Id: os_solaris.c,v 1.14 2004/02/04 13:21:30 ballen4705 Exp $";

const char *os_XXXX_c_cvsid="$Id: os_solaris.c,v 1.14 2004/02/04 13:21:30 ballen4705 Exp $" \
ATACMDS_H_CVSID CONFIG_H_CVSID OS_XXXX_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;

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
    return GUESS_DEVTYPE_SCSI;
  else if (isatadev(dev_name))
    return GUESS_DEVTYPE_ATA;
  else
    return GUESS_DEVTYPE_DONT_KNOW;
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
                res->names = realloc(res->names, res->maxnames * sizeof (char *));
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

                // shrink array to min possible size
                res.names = realloc(res.names, res.nnames * sizeof (char *));
                bytes -= sizeof(char *)*(res.maxnames-res.nnames);

                // pass list back
                *devlist = res.names;
                return res.nnames;
        }
        
        // ATA case not implemented
        *devlist=NULL;
        return 0;
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


// Interface to ATA devices.  See os_linux.c
int ata_command_interface(int fd, smart_command_set command, int select, char *data){
  // avoid gcc warnings//
  fd=command=select=0;
  data=NULL;

  if (printwarning(0))
    return -1;
  return -1;
}

// Interface to ATA devices behind 3ware escalade RAID controller cards.  See os_linux.c
int escalade_command_interface(int fd, int disknum, smart_command_set command, int select, char *data){
  // avoid gcc warnings//
  fd=disknum=command=select=0;
  data=NULL;

  if (printwarning(1))
    return -1;
  return -1;
}

#include <errno.h>
#include <sys/scsi/generic/commands.h>
#include <sys/scsi/generic/status.h>
#include <sys/scsi/impl/types.h>
#include <sys/scsi/impl/uscsi.h>

// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report) {
  struct uscsi_cmd uscsi;

    if (report > 0) {
        int k;
        const unsigned char * ucp = iop->cmnd;
        const char * np;

        np = scsi_get_opcode_name(ucp[0]);
        pout(" [%s: ", np ? np : "<unknown opcode>");
        for (k = 0; k < (int)iop->cmnd_len; ++k)
            pout("%02x ", ucp[k]);
        if ((report > 1) && 
            (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
            int trunc = (iop->dxfer_len > 256) ? 1 : 0;

            pout("]\n  Outgoing data, len=%d%s:\n", (int)iop->dxfer_len,
                 (trunc ? " [only first 256 bytes shown]" : ""));
            dStrHex((char *)iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
        }
        else
            pout("]");
    }


  memset(&uscsi, 0, sizeof (uscsi));

  uscsi.uscsi_cdb = (void *)iop->cmnd;
  uscsi.uscsi_cdblen = iop->cmnd_len;
  if (iop->timeout == 0)
    uscsi.uscsi_timeout = 60; /* XXX */
  else
    uscsi.uscsi_timeout = iop->timeout;
  uscsi.uscsi_bufaddr = (void *)iop->dxferp;
  uscsi.uscsi_buflen = iop->dxfer_len;
  uscsi.uscsi_rqbuf = (void *)iop->sensep;
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
  uscsi.uscsi_flags |= USCSI_ISOLATE;

  if (ioctl(fd, USCSICMD, &uscsi))
    return -errno;

  iop->scsi_status = uscsi.uscsi_status;
  iop->resid = uscsi.uscsi_resid;
  iop->resp_sense_len = iop->max_sense_len - uscsi.uscsi_rqresid;

  if (report > 0) {
    int trunc = (iop->dxfer_len > 256) ? 1 : 0;
    pout("  status=0\n");
    
    pout("  Incoming data, len=%d%s:\n", (int)iop->dxfer_len,
         (trunc ? " [only first 256 bytes shown]" : ""));
    dStrHex((char *)iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
  }

  return (0);
}
