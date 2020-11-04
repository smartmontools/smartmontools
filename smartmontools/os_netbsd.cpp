/*
 * os_netbsd.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2003-8 Sergey Svishchev
 * Copyright (C) 2016 Kimihiro Nonaka
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"
#include "os_netbsd.h"

#include <sys/drvctlio.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

// based on "sys/dev/ic/nvmeio.h" from NetBSD kernel sources
#include "netbsd_nvme_ioctl.h" // NVME_PASSTHROUGH_CMD, nvme_completion_is_error

const char * os_netbsd_cpp_cvsid = "$Id$"
  OS_NETBSD_H_CVSID;

#define ARGUSED(x) ((void)(x))

/////////////////////////////////////////////////////////////////////////////

namespace os_netbsd { // No need to publish anything, name provided for Doxygen

static const char *net_dev_prefix = "/dev/";
static const char *net_dev_raw_prefix = "/dev/r";
static const char *net_dev_ata_disk = "wd";
static const char *net_dev_scsi_disk = "sd";
static const char *net_dev_scsi_tape = "enrst";
static const char *net_dev_nvme_ctrl = "nvme";

/////////////////////////////////////////////////////////////////////////////
/// Implement shared open/close routines with old functions.

class netbsd_smart_device
: virtual public /*implements*/ smart_device
{
public:
  explicit netbsd_smart_device()
    : smart_device(never_called),
      m_fd(-1) { }

  virtual ~netbsd_smart_device();

  virtual bool is_open() const;

  virtual bool open();

  virtual bool close();

protected:
  /// Return filedesc for derived classes.
  int get_fd() const
    { return m_fd; }

  void set_fd(int fd)
    { m_fd = fd; }

private:
  int m_fd; ///< filedesc, -1 if not open.
};

netbsd_smart_device::~netbsd_smart_device()
{
  if (m_fd >= 0)
    os_netbsd::netbsd_smart_device::close();
}

bool netbsd_smart_device::is_open() const
{
  return (m_fd >= 0);
}


bool netbsd_smart_device::open()
{
  const char *dev = get_dev_name();
  int fd;

  if (is_scsi()) {
    fd = ::open(dev,O_RDWR|O_NONBLOCK);
    if (fd < 0 && errno == EROFS)
      fd = ::open(dev,O_RDONLY|O_NONBLOCK);
    if (fd < 0) {
      set_err(errno);
      return false;
    }
  } else if (is_ata() || is_nvme()) {
    if ((fd = ::open(dev,O_RDWR|O_NONBLOCK))<0) {
      set_err(errno);
      return false;
    }
  } else
    return false;

  set_fd(fd);
  return true;
}

bool netbsd_smart_device::close()
{
  int failed = 0;
  // close device, if open
  if (is_open())
    failed=::close(get_fd());

  set_fd(-1);

  if(failed) return false;
    else return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Implement standard ATA support

class netbsd_ata_device
: public /*implements*/ ata_device,
  public /*extends*/ netbsd_smart_device
{
public:
  netbsd_ata_device(smart_interface * intf, const char * dev_name, const char * req_type);
  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out);

protected:
  virtual int do_cmd(struct atareq* request, bool is_48bit_cmd);
};

netbsd_ata_device::netbsd_ata_device(smart_interface * intf, const char * dev_name, const char * req_type)
: smart_device(intf, dev_name, "ata", req_type),
  netbsd_smart_device()
{
}

int netbsd_ata_device::do_cmd( struct atareq* request, bool is_48bit_cmd)
{
  int fd = get_fd(), ret;
  ARGUSED(is_48bit_cmd); // no support for 48 bit commands in the ATAIOCCOMMAND
  ret = ioctl(fd, ATAIOCCOMMAND, request);
  if (ret) set_err(errno);
  return ret;
}

bool netbsd_ata_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  bool ata_48bit = false; // no ata_48bit_support via ATAIOCCOMMAND

  if (!ata_cmd_is_ok(in,
    true,  // data_out_support
    true,  // multi_sector_support
    ata_48bit)
    ) {
      set_err(ENOSYS, "48-bit ATA commands not implemented");
      return false;
    }

  struct atareq req;
  memset(&req, 0, sizeof(req));

  req.timeout = SCSI_TIMEOUT_DEFAULT * 1000;
  req.command = in.in_regs.command;
  req.features = in.in_regs.features;
  req.sec_count = in.in_regs.sector_count;
  req.sec_num = in.in_regs.lba_low;
  req.head = in.in_regs.device;
  req.cylinder = in.in_regs.lba_mid | (in.in_regs.lba_high << 8);

  switch (in.direction) {
    case ata_cmd_in::no_data:
      req.flags = ATACMD_READREG;
      break;
    case ata_cmd_in::data_in:
      req.flags = ATACMD_READ | ATACMD_READREG;
      req.databuf = (char *)in.buffer;
      req.datalen = in.size;
      break;
    case ata_cmd_in::data_out:
      req.flags = ATACMD_WRITE | ATACMD_READREG;
      req.databuf = (char *)in.buffer;
      req.datalen = in.size;
      break;
    default:
      return set_err(ENOSYS);
  }

  clear_err();
  errno = 0;
  if (do_cmd(&req, in.in_regs.is_48bit_cmd()))
      return false;
  if (req.retsts != ATACMD_OK)
      return set_err(EIO, "request failed, error code 0x%02x", req.retsts);

  out.out_regs.error = req.error;
  out.out_regs.sector_count = req.sec_count;
  out.out_regs.lba_low = req.sec_num;
  out.out_regs.device = req.head;
  out.out_regs.lba_mid = req.cylinder;
  out.out_regs.lba_high = req.cylinder >> 8;
  out.out_regs.status = req.command;
  /* Undo byte-swapping for IDENTIFY */
  if (in.in_regs.command == ATA_IDENTIFY_DEVICE && isbigendian()) {
     for (int i = 0; i < 256; i+=2)
      swap2 ((char *)req.databuf + i);
  }
  return true;
}

/////////////////////////////////////////////////////////////////////////////
/// NVMe support

class netbsd_nvme_device
: public /*implements*/ nvme_device,
  public /*extends*/ netbsd_smart_device
{
public:
  netbsd_nvme_device(smart_interface * intf, const char * dev_name,
    const char * req_type, unsigned nsid);

  virtual bool open();

  virtual bool nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out);
};

netbsd_nvme_device::netbsd_nvme_device(smart_interface * intf, const char * dev_name,
  const char * req_type, unsigned nsid)
: smart_device(intf, dev_name, "nvme", req_type),
  nvme_device(nsid),
  netbsd_smart_device()
{
}

bool netbsd_nvme_device::open()
{
  const char *dev = get_dev_name();
  if (strncmp(dev, NVME_PREFIX, strlen(NVME_PREFIX))) {
    set_err(EINVAL, "NVMe controller controller/namespace ids must begin with '%s'",
      NVME_PREFIX);
    return false;
  }

  int nsid = -1, ctrlid = -1;
  char tmp;

  if(sscanf(dev, NVME_PREFIX"%d%c", &ctrlid, &tmp) == 1)
  {
    if(ctrlid < 0) {
      set_err(EINVAL, "Invalid NVMe controller number");
      return false;
    }
    nsid = 0xFFFFFFFF; // broadcast id
  }
  else if (sscanf(dev, NVME_PREFIX "%d" NVME_NS_PREFIX "%d%c",
    &ctrlid, &nsid, &tmp) == 2)
  {
    if(ctrlid < 0 || nsid <= 0) {
      set_err(EINVAL, "Invalid NVMe controller/namespace number");
      return false;
    }
  }
  else {
    set_err(EINVAL, "Invalid NVMe controller/namespace syntax");
    return false;
  }

  // we should always open controller, not namespace device
  char full_path[64];
  snprintf(full_path, sizeof(full_path), NVME_PREFIX"%d", ctrlid);

  int fd;
  if ((fd = ::open(full_path, O_RDWR))<0) {
    set_err(errno);
    return false;
  }
  set_fd(fd);

  if (!get_nsid()) {
    set_nsid(nsid);
  }

  return true;
}

bool netbsd_nvme_device::nvme_pass_through(const nvme_cmd_in & in, nvme_cmd_out & out)
{
  struct nvme_pt_command pt;
  memset(&pt, 0, sizeof(pt));

  pt.cmd.opcode = in.opcode;
  pt.cmd.nsid = in.nsid;
  pt.buf = in.buffer;
  pt.len = in.size;
  pt.cmd.cdw10 = in.cdw10;
  pt.cmd.cdw11 = in.cdw11;
  pt.cmd.cdw12 = in.cdw12;
  pt.cmd.cdw13 = in.cdw13;
  pt.cmd.cdw14 = in.cdw14;
  pt.cmd.cdw15 = in.cdw15;
  pt.is_read = 1; // should we use in.direction()?

  int status = ioctl(get_fd(), NVME_PASSTHROUGH_CMD, &pt);

  if (status < 0)
    return set_err(errno, "NVME_PASSTHROUGH_CMD: %s", strerror(errno));

  out.result=pt.cpl.cdw0; // Command specific result (DW0)

  if (nvme_completion_is_error(&pt.cpl))
    return set_nvme_err(out, nvme_completion_is_error(&pt.cpl));

  return true;
}

/////////////////////////////////////////////////////////////////////////////
/// Standard SCSI support

class netbsd_scsi_device
: public /*implements*/ scsi_device,
  public /*extends*/ netbsd_smart_device
{
public:
  netbsd_scsi_device(smart_interface * intf, const char * dev_name, const char * req_type, bool scanning = false);

  virtual smart_device * autodetect_open();

  virtual bool scsi_pass_through(scsi_cmnd_io * iop);

private:
  bool m_scanning; ///< true if created within scan_smart_devices
};

netbsd_scsi_device::netbsd_scsi_device(smart_interface * intf,
  const char * dev_name, const char * req_type, bool scanning /* = false */)
: smart_device(intf, dev_name, "scsi", req_type),
  netbsd_smart_device(),
  m_scanning(scanning)
{
}

bool netbsd_scsi_device::scsi_pass_through(scsi_cmnd_io * iop)
{
  struct scsireq sc;
  int fd = get_fd();

  if (scsi_debugmode) {
    unsigned int k;
    const unsigned char * ucp = iop->cmnd;
    const char * np;

    np = scsi_get_opcode_name(ucp[0]);
    pout(" [%s: ", np ? np : "<unknown opcode>");
    for (k = 0; k < iop->cmnd_len; ++k)
      pout("%02x ", ucp[k]);
    if ((scsi_debugmode > 1) &&
      (DXFER_TO_DEVICE == iop->dxfer_dir) && (iop->dxferp)) {
    int trunc = (iop->dxfer_len > 256) ? 1 : 0;

    pout("]\n  Outgoing data, len=%d%s:\n", (int)iop->dxfer_len,
      (trunc ? " [only first 256 bytes shown]" : ""));
    dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
      }
      else
        pout("]\n");
  }

  memset(&sc, 0, sizeof(sc));
  memcpy(sc.cmd, iop->cmnd, iop->cmnd_len);
  sc.cmdlen = iop->cmnd_len;
  sc.databuf = (char *)iop->dxferp;
  sc.datalen = iop->dxfer_len;
  sc.senselen = iop->max_sense_len;
  sc.timeout = iop->timeout == 0 ? 60000 : (1000 * iop->timeout);
  sc.flags =
    (iop->dxfer_dir == DXFER_NONE ? SCCMD_READ :
    (iop->dxfer_dir == DXFER_FROM_DEVICE ? SCCMD_READ : SCCMD_WRITE));

  if (ioctl(fd, SCIOCCOMMAND, &sc) < 0) {
    if (scsi_debugmode) {
      pout("  error sending SCSI ccb\n");
    }
    return set_err(EIO);
  }
  iop->resid = sc.datalen - sc.datalen_used;
  iop->scsi_status = sc.status;
  if (iop->sensep) {
    memcpy(iop->sensep, sc.sense, sc.senselen_used);
    iop->resp_sense_len = sc.senselen_used;
  }
  if (scsi_debugmode) {
    int trunc;

    pout("  status=0\n");
    trunc = (iop->dxfer_len > 256) ? 1 : 0;

    pout("  Incoming data, len=%d%s:\n", (int) iop->dxfer_len,
      (trunc ? " [only first 256 bytes shown]" : ""));
    dStrHex(iop->dxferp, (trunc ? 256 : iop->dxfer_len), 1);
  }
  switch (sc.retsts) {
    case SCCMD_OK:
      break;
    case SCCMD_TIMEOUT:
      return set_err(ETIMEDOUT);
    case SCCMD_BUSY:
      return set_err(EBUSY);
    default:
      return set_err(EIO);
  }

  return true;
}

/////////////////////////////////////////////////////////////////////////////
///// SCSI open with autodetection support

smart_device * netbsd_scsi_device::autodetect_open()
{
  // Open device
  if (!open())
    return this;

  // No Autodetection if device type was specified by user
  bool sat_only = false;
  if (*get_req_type()) {
    // Detect SAT if device object was created by scan_smart_devices().
    if (!(m_scanning && !strcmp(get_req_type(), "sat")))
      return this;
    sat_only = true;
  }

  // The code below is based on smartd.cpp:SCSIFilterKnown()

  // Get INQUIRY
  unsigned char req_buff[64] = {0, };
  int req_len = 36;
  if (scsiStdInquiry(this, req_buff, req_len)) {
    // Marvell controllers fail on a 36 bytes StdInquiry, but 64 suffices
    // watch this spot ... other devices could lock up here
    req_len = 64;
    if (scsiStdInquiry(this, req_buff, req_len)) {
      // device doesn't like INQUIRY commands
      close();
      set_err(EIO, "INQUIRY failed");
      return this;
    }
  }

  int avail_len = req_buff[4] + 5;
  int len = (avail_len < req_len ? avail_len : req_len);
  if (len < 36) {
    if (sat_only) {
      close();
      set_err(EIO, "INQUIRY too short for SAT");
    }
    return this;
  }

  // Use INQUIRY to detect type

  // SAT or USB, skip MFI controllers because of bugs
  {
    smart_device * newdev = smi()->autodetect_sat_device(this, req_buff, len);
    if (newdev) {
      // NOTE: 'this' is now owned by '*newdev'
      return newdev;
    }
  }

  // Nothing special found

  if (sat_only) {
    close();
    set_err(EIO, "Not a SAT device");
  }
  return this;
}

/////////////////////////////////////////////////////////////////////////////
/// Implement platform interface with old functions.

class netbsd_smart_interface
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

  virtual smart_device * get_custom_smart_device(const char * name, const char * type);

  virtual std::string get_valid_custom_dev_types_str();

private:
  int get_dev_names(char ***, const char *);

  bool get_nvme_devlist(smart_device_list & devlist, const char * type);
};


//////////////////////////////////////////////////////////////////////

std::string netbsd_smart_interface::get_os_version_str()
{
  struct utsname osname;
  uname(&osname);
  return strprintf("%s %s %s", osname.sysname, osname.release, osname.machine);
}

std::string netbsd_smart_interface::get_app_examples(const char * appname)
{
  if (!strcmp(appname, "smartctl")) {
    char p;

    p = 'a' + getrawpartition();
    return strprintf(
      "=================================================== SMARTCTL EXAMPLES =====\n\n"
      "  smartctl -a /dev/wd0%c                      (Prints all SMART information)\n\n"
      "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/wd0%c\n"
      "                                              (Enables SMART on first disk)\n\n"
      "  smartctl -t long /dev/wd0%c             (Executes extended disk self-test)\n\n"
      "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/wd0%c\n"
      "                                      (Prints Self-Test & Attribute errors)\n"
      , p, p, p, p);
  }
  return "";
}

ata_device * netbsd_smart_interface::get_ata_device(const char * name, const char * type)
{
  return new netbsd_ata_device(this, name, type);
}

scsi_device * netbsd_smart_interface::get_scsi_device(const char * name, const char * type)
{
  return new netbsd_scsi_device(this, name, type);
}

nvme_device * netbsd_smart_interface::get_nvme_device(const char * name, const char * type, unsigned nsid)
{
  return new netbsd_nvme_device(this, name, type, nsid);
}

int netbsd_smart_interface::get_dev_names(char ***names, const char *prefix)
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
  for (p = strtok(disknames, " "); p; p = strtok(NULL, " ")) {
    if (strncmp(p, prefix, strlen(prefix))) {
      continue;
    }
    mp[n] = (char *)malloc(strlen(net_dev_raw_prefix) + strlen(p) + 2);
    if (!mp[n]) {
      pout("Out of memory constructing scan device list\n");
      return -1;
    }
    sprintf(mp[n], "%s%s%c", net_dev_raw_prefix, p, 'a' + getrawpartition());
    n++;
  }
  free(disknames);

  if (n == 0) {
    free(mp);
    return 0;
  }

  char ** tmp = (char **)realloc(mp, n * (sizeof(char *)));
  if (NULL == tmp) {
    pout("Out of memory constructing scan device list\n");
    free(mp);
    return -1;
  }
  else
    mp = tmp;
  *names = mp;
  return n;
}

bool netbsd_smart_interface::get_nvme_devlist(smart_device_list & devlist,
    const char * type)
{
  char ctrlpath[64], nspath[64];
  struct stat sb;
  struct devlistargs laa;
  nvme_device * nvmedev;

  int drvfd = ::open(DRVCTLDEV, O_RDONLY, 0);
  if (drvfd < 0) {
    set_err(errno);
    return false;
  }

  for (int ctrl = 0;; ctrl++) {
    snprintf(ctrlpath, sizeof(ctrlpath), NVME_PREFIX"%d", ctrl);
    if (stat(ctrlpath, &sb) == -1 || !S_ISCHR(sb.st_mode))
      break;

    snprintf(laa.l_devname, sizeof(laa.l_devname), "%s%d", net_dev_nvme_ctrl,
      ctrl);
    laa.l_childname = NULL;
    laa.l_children = 0;
    if (ioctl(drvfd, DRVLISTDEV, &laa) == -1) {
      if (errno == ENXIO)
        continue;
      break;
    }

    nvmedev = get_nvme_device(ctrlpath, type, 0);
    if (nvmedev)
      devlist.push_back(nvmedev);

    uint32_t n = 0;
    for (int nsid = 1; n < laa.l_children; nsid++) {
      snprintf(nspath, sizeof(nspath), NVME_PREFIX "%d" NVME_NS_PREFIX "%d",
        ctrl, nsid);
      if (stat(nspath, &sb) == -1 || !S_ISCHR(sb.st_mode))
        break;
      int nsfd = ::open(nspath, O_RDONLY, 0);
      if (nsfd < 0)
        continue;
      ::close(nsfd);

      n++;
      nvmedev = get_nvme_device(nspath, type, nsid);
      if (nvmedev)
        devlist.push_back(nvmedev);
    }
  }

  ::close(drvfd);
  return true;
}

bool netbsd_smart_interface::scan_smart_devices(smart_device_list & devlist,
  const char * type, const char * pattern /*= 0*/)
{
  if (pattern) {
    set_err(EINVAL, "DEVICESCAN with pattern not implemented yet");
    return false;
  }

  if (type == NULL)
    type = "";

  bool scan_ata = !*type || !strcmp(type, "ata");
  bool scan_scsi = !*type || !strcmp(type, "scsi") || !strcmp(type, "sat");

#ifdef WITH_NVME_DEVICESCAN // TODO: Remove when NVMe support is no longer EXPERIMENTAL
  bool scan_nvme = !*type || !strcmp(type, "nvme");
#else
  bool scan_nvme =          !strcmp(type, "nvme");
#endif

  // Make namelists
  char * * atanames = 0; int numata = 0;
  if (scan_ata) {
    numata = get_dev_names(&atanames, net_dev_ata_disk);
    if (numata < 0) {
      set_err(ENOMEM);
      return false;
    }
  }

  char * * scsinames = 0; int numscsi = 0;
  char * * scsitapenames = 0; int numscsitape = 0;
  if (scan_scsi) {
    numscsi = get_dev_names(&scsinames, net_dev_scsi_disk);
    if (numscsi < 0) {
      set_err(ENOMEM);
      return false;
    }
    numscsitape = get_dev_names(&scsitapenames, net_dev_scsi_tape);
    if (numscsitape < 0) {
      set_err(ENOMEM);
      return false;
    }
  }

  // Add to devlist
  int i;
  for (i = 0; i < numata; i++) {
    ata_device * atadev = get_ata_device(atanames[i], type);
    if (atadev)
      devlist.push_back(atadev);
    free(atanames[i]);
  }
  if(numata) free(atanames);

  for (i = 0; i < numscsi; i++) {
    scsi_device * scsidev = new netbsd_scsi_device(this, scsinames[i], type, true /*scanning*/);
    if (scsidev)
      devlist.push_back(scsidev);
    free(scsinames[i]);
  }
  if(numscsi) free(scsinames);

  for (i = 0; i < numscsitape; i++) {
    scsi_device * scsidev = get_scsi_device(scsitapenames[i], type);
    if (scsidev)
      devlist.push_back(scsidev);
    free(scsitapenames[i]);
  }
  if(numscsitape) free(scsitapenames);

  if (scan_nvme)
    get_nvme_devlist(devlist, type);

  return true;
}

smart_device * netbsd_smart_interface::autodetect_smart_device(const char * name)
{
  const char * test_name = name;

  // if dev_name null, or string length zero
  if (!name || !*name)
    return 0;

  // Dereference symlinks
  struct stat st;
  std::string pathbuf;
  if (!lstat(name, &st) && S_ISLNK(st.st_mode)) {
    char * p = realpath(name, (char *)0);
    if (p) {
      pathbuf = p;
      free(p);
      test_name = pathbuf.c_str();
    }
  }

  if (str_starts_with(test_name, net_dev_raw_prefix))
    test_name += strlen(net_dev_raw_prefix);
  else if (str_starts_with(test_name, net_dev_prefix))
    test_name += strlen(net_dev_prefix);
  else
    return 0; // device is not starting with /dev/ or /dev/r*

  if (!strncmp(net_dev_ata_disk, test_name, strlen(net_dev_ata_disk)))
    return get_ata_device(name, "ata");

  if (!strncmp(net_dev_scsi_disk, test_name, strlen(net_dev_scsi_disk))) {
      // XXX Try to detect possible USB->(S)ATA bridge
      // XXX get USB vendor ID, product ID and version from sd(4)/umass(4).
      // XXX check sat device via get_usb_dev_type_by_id().
      // No USB bridge found, assume regular SCSI device
      return get_scsi_device(name, "scsi");
  }

  if (!strncmp(net_dev_scsi_tape, test_name, strlen(net_dev_scsi_tape)))
    return get_scsi_device(name, "scsi");

  if (!strncmp(net_dev_nvme_ctrl, test_name, strlen(net_dev_nvme_ctrl)))
    return get_nvme_device(name, "nvme", 0 /* use default nsid */);

  // device type unknown
  return 0;
}

smart_device * netbsd_smart_interface::get_custom_smart_device(const char * name, const char * type)
{
  ARGUSED(name);
  ARGUSED(type);
  return 0;
}

std::string netbsd_smart_interface::get_valid_custom_dev_types_str()
{
  return "";
}

} // namespace

/////////////////////////////////////////////////////////////////////////////
/// Initialize platform interface and register with smi()

void smart_interface::init()
{
  static os_netbsd::netbsd_smart_interface the_interface;
  smart_interface::set(&the_interface);
}

/* vim: set ts=2 sw=2 et ff=unix : */
