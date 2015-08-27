/*
 * os_openbsd.c
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2004-10 David Snyder <smartmontools-support@lists.sourceforge.net>
 *
 * Derived from os_netbsd.cpp by Sergey Svishchev <smartmontools-support@lists.sourceforge.net>, Copyright (C) 2003-8 
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

#include "config.h"
#include "int64.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"
#include "os_openbsd.h"

#include <errno.h>

const char * os_openbsd_cpp_cvsid = "$Id$"
  OS_OPENBSD_H_CVSID;

/* global variable holding byte count of allocated memory */
extern long long bytes;

enum warnings {
  BAD_SMART, NO_3WARE, NO_ARECA, MAX_MSG
};

/* Utility function for printing warnings */
void
printwarning(int msgNo, const char *extra)
{
  static int printed[] = {0, 0};
  static const char *message[] = {
    "Error: SMART Status command failed.\nPlease get assistance from \n" PACKAGE_HOMEPAGE "\nRegister values returned from SMART Status command are:\n",
    PACKAGE_STRING " does not currentlly support twe(4) devices (3ware Escalade)\n",
  };

  if (msgNo >= 0 && msgNo <= MAX_MSG) {
    if (!printed[msgNo]) {
      printed[msgNo] = 1;
      pout("%s", message[msgNo]);
      if (extra)
	pout("%s", extra);
    }
  }
  return;
}

static const char *net_dev_prefix = "/dev/";
static const char *net_dev_ata_disk = "wd";
static const char *net_dev_scsi_disk = "sd";
static const char *net_dev_scsi_tape = "st";

/* Guess device type(ata or scsi) based on device name */
int
guess_device_type(const char *dev_name)
{
  int len;
  int dev_prefix_len = strlen(net_dev_prefix);

  if (!dev_name || !(len = strlen(dev_name)))
    return CONTROLLER_UNKNOWN;

  if (!strncmp(net_dev_prefix, dev_name, dev_prefix_len)) {
    if (len <= dev_prefix_len)
      return CONTROLLER_UNKNOWN;
    else
      dev_name += dev_prefix_len;
  }
  if (!strncmp(net_dev_ata_disk, dev_name, strlen(net_dev_ata_disk)))
    return CONTROLLER_ATA;

  if (!strncmp(net_dev_scsi_disk, dev_name, strlen(net_dev_scsi_disk)))
    return CONTROLLER_SCSI;

  if (!strncmp(net_dev_scsi_tape, dev_name, strlen(net_dev_scsi_tape)))
    return CONTROLLER_SCSI;

  return CONTROLLER_UNKNOWN;
}

int
get_dev_names(char ***names, const char *prefix)
{
  char *disknames, *p, **mp;
  int n = 0;
  int sysctl_mib[2];
  size_t sysctl_len;

  *names = NULL;

  sysctl_mib[0] = CTL_HW;
  sysctl_mib[1] = HW_DISKNAMES;
  if (-1 == sysctl(sysctl_mib, 2, NULL, &sysctl_len, NULL, 0)) {
    pout("Failed to get value of sysctl `hw.disknames'\n");
    return -1;
  }
  if (!(disknames = (char *)malloc(sysctl_len))) {
    pout("Out of memory constructing scan device list\n");
    return -1;
  }
  if (-1 == sysctl(sysctl_mib, 2, disknames, &sysctl_len, NULL, 0)) {
    pout("Failed to get value of sysctl `hw.disknames'\n");
    return -1;
  }
  if (!(mp = (char **) calloc(strlen(disknames) / 2, sizeof(char *)))) {
    pout("Out of memory constructing scan device list\n");
    return -1;
  }
  for (p = strtok(disknames, ","); p; p = strtok(NULL, ",")) {
    if (strncmp(p, prefix, strlen(prefix))) {
      continue;
    }
    char * u = strchr(p, ':');
    if (u)
      *u = 0;
    mp[n] = (char *)malloc(strlen(net_dev_prefix) + strlen(p) + 2);
    if (!mp[n]) {
      pout("Out of memory constructing scan device list\n");
      return -1;
    }
    sprintf(mp[n], "%s%s%c", net_dev_prefix, p, 'a' + getrawpartition());
    bytes += strlen(mp[n]) + 1;
    n++;
  }

  mp = (char **)realloc(mp, n * (sizeof(char *)));
  bytes += (n) * (sizeof(char *));
  *names = mp;
  return n;
}

int
make_device_names(char ***devlist, const char *name)
{
  if (!strcmp(name, "SCSI"))
    return get_dev_names(devlist, net_dev_scsi_disk);
  else if (!strcmp(name, "ATA"))
    return get_dev_names(devlist, net_dev_ata_disk);
  else
    return 0;
}

int
deviceopen(const char *pathname, char *type)
{
  if (!strcmp(type, "SCSI")) {
    int fd = open(pathname, O_RDWR | O_NONBLOCK);
    if (fd < 0 && errno == EROFS)
      fd = open(pathname, O_RDONLY | O_NONBLOCK);
    return fd;
  } else if (!strcmp(type, "ATA"))
    return open(pathname, O_RDWR | O_NONBLOCK);
  else
    return -1;
}

int
deviceclose(int fd)
{
  return close(fd);
}

int
ata_command_interface(int fd, smart_command_set command, int select, char *data)
{
  struct atareq req;
  unsigned char inbuf[DEV_BSIZE];
  int retval, copydata = 0;

  memset(&req, 0, sizeof(req));
  memset(&inbuf, 0, sizeof(inbuf));

  switch (command) {
  case READ_VALUES:
    req.flags = ATACMD_READ;
    req.features = ATA_SMART_READ_VALUES;
    req.command = ATAPI_SMART;
    req.databuf = (caddr_t) inbuf;
    req.datalen = sizeof(inbuf);
    req.cylinder = htole16(WDSMART_CYL);
    req.timeout = 1000;
    copydata = 1;
    break;
  case READ_THRESHOLDS:
    req.flags = ATACMD_READ;
    req.features = ATA_SMART_READ_THRESHOLDS;
    req.command = ATAPI_SMART;
    req.databuf = (caddr_t) inbuf;
    req.datalen = sizeof(inbuf);
    req.cylinder = htole16(WDSMART_CYL);
    req.timeout = 1000;
    copydata = 1;
    break;
  case READ_LOG:
    req.flags = ATACMD_READ;
    req.features = ATA_SMART_READ_LOG_SECTOR;	/* XXX missing from wdcreg.h */
    req.command = ATAPI_SMART;
    req.databuf = (caddr_t) inbuf;
    req.datalen = sizeof(inbuf);
    req.cylinder = htole16(WDSMART_CYL);
    req.sec_num = select;
    req.sec_count = 1;
    req.timeout = 1000;
    copydata = 1;
    break;
  case WRITE_LOG:
    memcpy(inbuf, data, 512);
    req.flags = ATACMD_WRITE;
    req.features = ATA_SMART_WRITE_LOG_SECTOR;	/* XXX missing from wdcreg.h */
    req.command = ATAPI_SMART;
    req.databuf = (caddr_t) inbuf;
    req.datalen = sizeof(inbuf);
    req.cylinder = htole16(WDSMART_CYL);
    req.sec_num = select;
    req.sec_count = 1;
    req.timeout = 1000;
    break;
  case IDENTIFY:
    req.flags = ATACMD_READ;
    req.command = WDCC_IDENTIFY;
    req.databuf = (caddr_t) inbuf;
    req.datalen = sizeof(inbuf);
    req.timeout = 1000;
    copydata = 1;
    break;
  case PIDENTIFY:
    req.flags = ATACMD_READ;
    req.command = ATAPI_IDENTIFY_DEVICE;
    req.databuf = (caddr_t) inbuf;
    req.datalen = sizeof(inbuf);
    req.timeout = 1000;
    copydata = 1;
    break;
  case ENABLE:
    req.flags = ATACMD_READ;
    req.features = ATA_SMART_ENABLE;
    req.command = ATAPI_SMART;
    req.cylinder = htole16(WDSMART_CYL);
    req.timeout = 1000;
    break;
  case DISABLE:
    req.flags = ATACMD_READ;
    req.features = ATA_SMART_DISABLE;
    req.command = ATAPI_SMART;
    req.cylinder = htole16(WDSMART_CYL);
    req.timeout = 1000;
    break;
  case AUTO_OFFLINE:
    /* NOTE: According to ATAPI 4 and UP, this command is obsolete */
    req.flags = ATACMD_READ;
    req.features = ATA_SMART_AUTO_OFFLINE;	/* XXX missing from wdcreg.h */
    req.command = ATAPI_SMART;
    req.databuf = (caddr_t) inbuf;
    req.datalen = sizeof(inbuf);
    req.cylinder = htole16(WDSMART_CYL);
    req.sec_num = select;
    req.sec_count = 1;
    req.timeout = 1000;
    break;
  case AUTOSAVE:
    req.flags = ATACMD_READ;
    req.features = ATA_SMART_AUTOSAVE;	/* XXX missing from wdcreg.h */
    req.command = ATAPI_SMART;
    req.cylinder = htole16(WDSMART_CYL);
    req.sec_count = 0xf1;
    /* to enable autosave */
    req.timeout = 1000;
    break;
  case IMMEDIATE_OFFLINE:
    /* NOTE: According to ATAPI 4 and UP, this command is obsolete */
    req.flags = ATACMD_READ;
    req.features = ATA_SMART_IMMEDIATE_OFFLINE;	/* XXX missing from wdcreg.h */
    req.command = ATAPI_SMART;
    req.databuf = (caddr_t) inbuf;
    req.datalen = sizeof(inbuf);
    req.cylinder = htole16(WDSMART_CYL);
    req.sec_num = select;
    req.sec_count = 1;
    req.timeout = 1000;
    break;
  case STATUS_CHECK:
    /* same command, no HDIO in NetBSD */
  case STATUS:
    req.flags = ATACMD_READ;
    req.features = ATA_SMART_STATUS;
    req.command = ATAPI_SMART;
    req.cylinder = htole16(WDSMART_CYL);
    req.timeout = 1000;
    break;
  case CHECK_POWER_MODE:
    req.flags = ATACMD_READREG;
    req.command = WDCC_CHECK_PWR;
    req.timeout = 1000;
    break;
  default:
    pout("Unrecognized command %d in ata_command_interface()\n", command);
    errno = ENOSYS;
    return -1;
  }

  if (command == STATUS_CHECK) {
    char buf[512];

    unsigned const short normal = WDSMART_CYL, failed = 0x2cf4;

    if ((retval = ioctl(fd, ATAIOCCOMMAND, &req))) {
      perror("Failed command");
      return -1;
    }
    /* Cyl low and Cyl high unchanged means "Good SMART status" */
    if (letoh16(req.cylinder) == normal)
      return 0;

    /* These values mean "Bad SMART status" */
    if (letoh16(req.cylinder) == failed)
      return 1;

    /* We haven't gotten output that makes sense; 
     * print out some debugging info */
    snprintf(buf, sizeof(buf),
      "CMD=0x%02x\nFR =0x%02x\nNS =0x%02x\nSC =0x%02x\nCL =0x%02x\nCH =0x%02x\nRETURN =0x%04x\n",
      (int) req.command, (int) req.features, (int) req.sec_count, (int) req.sec_num,
      (int) (letoh16(req.cylinder) & 0xff), (int) ((letoh16(req.cylinder) >> 8) & 0xff),
      (int) req.error);
    printwarning(BAD_SMART, buf);
    return 0;
  }
  if ((retval = ioctl(fd, ATAIOCCOMMAND, &req))) {
    perror("Failed command");
    return -1;
  }
  if (command == CHECK_POWER_MODE)
    data[0] = req.sec_count;

  if (copydata)
    memcpy(data, inbuf, 512);

  return 0;
}

int
do_scsi_cmnd_io(int fd, struct scsi_cmnd_io * iop, int report)
{
  struct scsireq sc;

  if (report > 0) {
    size_t k;

    const unsigned char *ucp = iop->cmnd;
    const char *np;

    np = scsi_get_opcode_name(ucp[0]);
    pout(" [%s: ", np ? np : "<unknown opcode>");
    for (k = 0; k < iop->cmnd_len; ++k)
      pout("%02x ", ucp[k]);
    if ((report > 1) &&
      (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
      int trunc = (iop->dxfer_len > 256) ? 1 : 0;

      pout("]\n  Outgoing data, len=%d%s:\n", (int) iop->dxfer_len,
	(trunc ? " [only first 256 bytes shown]" : ""));
      dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len), 1);
    } else
      pout("]");
  }
  memset(&sc, 0, sizeof(sc));
  memcpy(sc.cmd, iop->cmnd, iop->cmnd_len);
  sc.cmdlen = iop->cmnd_len;
  sc.databuf = (char *)iop->dxferp;
  sc.datalen = iop->dxfer_len;
  sc.senselen = iop->max_sense_len;
  sc.timeout = iop->timeout == 0 ? 60000 : iop->timeout;	/* XXX */
  sc.flags =
    (iop->dxfer_dir == DXFER_NONE ? SCCMD_READ :
    (iop->dxfer_dir == DXFER_FROM_DEVICE ? SCCMD_READ : SCCMD_WRITE));

  if (ioctl(fd, SCIOCCOMMAND, &sc) < 0) {
    warn("error sending SCSI ccb");
    return -1;
  }
  iop->resid = sc.datalen - sc.datalen_used;
  iop->scsi_status = sc.status;
  if (iop->sensep) {
    memcpy(iop->sensep, sc.sense, sc.senselen_used);
    iop->resp_sense_len = sc.senselen_used;
  }
  if (report > 0) {
    int trunc;

    pout("  status=0\n");
    trunc = (iop->dxfer_len > 256) ? 1 : 0;

    pout("  Incoming data, len=%d%s:\n", (int) iop->dxfer_len,
      (trunc ? " [only first 256 bytes shown]" : ""));
    dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len), 1);
  }
  return 0;
}

/* print examples for smartctl */
void 
print_smartctl_examples()
{
  char p;

  p = 'a' + getrawpartition();
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
    "  smartctl -a /dev/wd0%c                      (Prints all SMART information)\n\n"
    "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/wd0%c\n"
    "                                              (Enables SMART on first disk)\n\n"
    "  smartctl -t long /dev/wd0%c             (Executes extended disk self-test)\n\n"
    "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/wd0%c\n"
    "                                      (Prints Self-Test & Attribute errors)\n",
    p, p, p, p
    );
#else
  printf(
    "  smartctl -a /dev/wd0%c                     (Prints all SMART information)\n"
    "  smartctl -s on -o on -S on /dev/wd0%c        (Enables SMART on first disk)\n"
    "  smartctl -t long /dev/wd0%c            (Executes extended disk self-test)\n"
    "  smartctl -A -l selftest -q errorsonly /dev/wd0%c"
    "                                      (Prints Self-Test & Attribute errors)\n",
    p, p, p, p
    );
#endif
  return;
}
