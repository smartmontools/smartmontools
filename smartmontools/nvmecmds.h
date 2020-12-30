/*
 * nvmecmds.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2016-20 Christian Franke
 *
 * Original code from <linux/nvme.h>:
 *   Copyright (C) 2011-2014 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NVMECMDS_H
#define NVMECMDS_H

#define NVMECMDS_H_CVSID "$Id$"

#include "static_assert.h"

#include <stdint.h>

// The code below was originally imported from <linux/nvme.h> include file from
// Linux kernel sources.  Types from <linux/types.h> were replaced.
// Symbol names are unchanged but placed in a namespace to allow inclusion
// of the original <linux/nvme.h>.
namespace smartmontools {

////////////////////////////////////////////////////////////////////////////
// BEGIN: From <linux/nvme.h>

struct nvme_error_log_page {
  uint64_t        error_count;
  unsigned short  sqid;
  unsigned short  cmdid;
  unsigned short  status_field;
  unsigned short  parm_error_location;
  uint64_t        lba;
  unsigned int    nsid;
  unsigned char   vs;
  unsigned char   resv[35];
};
STATIC_ASSERT(sizeof(nvme_error_log_page) == 64);

struct nvme_id_power_state {
  unsigned short  max_power; // centiwatts
  unsigned char   rsvd2;
  unsigned char   flags;
  unsigned int    entry_lat; // microseconds
  unsigned int    exit_lat;  // microseconds
  unsigned char   read_tput;
  unsigned char   read_lat;
  unsigned char   write_tput;
  unsigned char   write_lat;
  unsigned short  idle_power;
  unsigned char   idle_scale;
  unsigned char   rsvd19;
  unsigned short  active_power;
  unsigned char   active_work_scale;
  unsigned char   rsvd23[9];
};
STATIC_ASSERT(sizeof(nvme_id_power_state) == 32);

struct nvme_id_ctrl {
  unsigned short  vid;
  unsigned short  ssvid;
  char            sn[20];
  char            mn[40];
  char            fr[8];
  unsigned char   rab;
  unsigned char   ieee[3];
  unsigned char   cmic;
  unsigned char   mdts;
  unsigned short  cntlid;
  unsigned int    ver;
  unsigned int    rtd3r;
  unsigned int    rtd3e;
  unsigned int    oaes;
  unsigned int    ctratt;
  unsigned char   rsvd100[156];
  unsigned short  oacs;
  unsigned char   acl;
  unsigned char   aerl;
  unsigned char   frmw;
  unsigned char   lpa;
  unsigned char   elpe;
  unsigned char   npss;
  unsigned char   avscc;
  unsigned char   apsta;
  unsigned short  wctemp;
  unsigned short  cctemp;
  unsigned short  mtfa;
  unsigned int    hmpre;
  unsigned int    hmmin;
  unsigned char   tnvmcap[16];
  unsigned char   unvmcap[16];
  unsigned int    rpmbs;
  unsigned short  edstt;
  unsigned char   dsto;
  unsigned char   fwug;
  unsigned short  kas;
  unsigned short  hctma;
  unsigned short  mntmt;
  unsigned short  mxtmt;
  unsigned int    sanicap;
  unsigned char   rsvd332[180];
  unsigned char   sqes;
  unsigned char   cqes;
  unsigned short  maxcmd;
  unsigned int    nn;
  unsigned short  oncs;
  unsigned short  fuses;
  unsigned char   fna;
  unsigned char   vwc;
  unsigned short  awun;
  unsigned short  awupf;
  unsigned char   nvscc;
  unsigned char   rsvd531;
  unsigned short  acwu;
  unsigned char   rsvd534[2];
  unsigned int    sgls;
  unsigned char   rsvd540[228];
  char			      subnqn[256];
  unsigned char   rsvd1024[768];
  unsigned int    ioccsz;
  unsigned int    iorcsz;
  unsigned short  icdoff;
  unsigned char   ctrattr;
  unsigned char   msdbd;
  unsigned char   rsvd1804[244];
  struct nvme_id_power_state  psd[32];
  unsigned char   vs[1024];
};
STATIC_ASSERT(sizeof(nvme_id_ctrl) == 4096);

struct nvme_lbaf {
  unsigned short  ms;
  unsigned char   ds;
  unsigned char   rp;
};
STATIC_ASSERT(sizeof(nvme_lbaf) == 4);

struct nvme_id_ns {
  uint64_t        nsze;
  uint64_t        ncap;
  uint64_t        nuse;
  unsigned char   nsfeat;
  unsigned char   nlbaf;
  unsigned char   flbas;
  unsigned char   mc;
  unsigned char   dpc;
  unsigned char   dps;
  unsigned char   nmic;
  unsigned char   rescap;
  unsigned char   fpi;
  unsigned char   rsvd33;
  unsigned short  nawun;
  unsigned short  nawupf;
  unsigned short  nacwu;
  unsigned short  nabsn;
  unsigned short  nabo;
  unsigned short  nabspf;
  unsigned char   rsvd46[2];
  unsigned char   nvmcap[16];
  unsigned char   rsvd64[40];
  unsigned char   nguid[16];
  unsigned char   eui64[8];
  struct nvme_lbaf  lbaf[16];
  unsigned char   rsvd192[192];
  unsigned char   vs[3712];
};
STATIC_ASSERT(sizeof(nvme_id_ns) == 4096);

struct nvme_smart_log {
  unsigned char  critical_warning;
  unsigned char  temperature[2];
  unsigned char  avail_spare;
  unsigned char  spare_thresh;
  unsigned char  percent_used;
  unsigned char  rsvd6[26];
  unsigned char  data_units_read[16];
  unsigned char  data_units_written[16];
  unsigned char  host_reads[16];
  unsigned char  host_writes[16];
  unsigned char  ctrl_busy_time[16];
  unsigned char  power_cycles[16];
  unsigned char  power_on_hours[16];
  unsigned char  unsafe_shutdowns[16];
  unsigned char  media_errors[16];
  unsigned char  num_err_log_entries[16];
  unsigned int   warning_temp_time;
  unsigned int   critical_comp_time;
  unsigned short temp_sensor[8];
  unsigned int   thm_temp1_trans_count;
  unsigned int   thm_temp2_trans_count;
  unsigned int   thm_temp1_total_time;
  unsigned int   thm_temp2_total_time;
  unsigned char  rsvd232[280];
};
STATIC_ASSERT(sizeof(nvme_smart_log) == 512);

enum nvme_admin_opcode {
//nvme_admin_delete_sq     = 0x00,
//nvme_admin_create_sq     = 0x01,
  nvme_admin_get_log_page  = 0x02,
//nvme_admin_delete_cq     = 0x04,
//nvme_admin_create_cq     = 0x05,
  nvme_admin_identify      = 0x06,
//nvme_admin_abort_cmd     = 0x08,
//nvme_admin_set_features  = 0x09,
//nvme_admin_get_features  = 0x0a,
//nvme_admin_async_event   = 0x0c,
//nvme_admin_ns_mgmt       = 0x0d,
//nvme_admin_activate_fw   = 0x10,
//nvme_admin_download_fw   = 0x11,
//nvme_admin_ns_attach     = 0x15,
//nvme_admin_format_nvm    = 0x80,
//nvme_admin_security_send = 0x81,
//nvme_admin_security_recv = 0x82,
};

// END: From <linux/nvme.h>
////////////////////////////////////////////////////////////////////////////

} // namespace smartmontools

class nvme_device;

// Print NVMe debug messages?
extern unsigned char nvme_debugmode;

// Read NVMe Identify Controller data structure.
bool nvme_read_id_ctrl(nvme_device * device, smartmontools::nvme_id_ctrl & id_ctrl);

// Read NVMe Identify Namespace data structure for namespace NSID.
bool nvme_read_id_ns(nvme_device * device, unsigned nsid, smartmontools::nvme_id_ns & id_ns);

// Read NVMe log page with identifier LID.
unsigned nvme_read_log_page(nvme_device * device, unsigned nsid, unsigned char lid,
  void * data, unsigned size, bool lpo_sup, unsigned offset = 0);

// Read NVMe Error Information Log.
unsigned nvme_read_error_log(nvme_device * device, smartmontools::nvme_error_log_page * error_log,
  unsigned num_entries, bool lpo_sup);

// Read NVMe SMART/Health Information log.
bool nvme_read_smart_log(nvme_device * device, smartmontools::nvme_smart_log & smart_log);

#endif // NVMECMDS_H
