/*
 * nvme.h - NVMe constants and types
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2016-2025 Christian Franke
 *
 * Originally based on <linux/nvme.h>:
 *   Copyright (C) 2011-2014 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SMARTMON_NVME_H
#define SMARTMON_NVME_H

#include <smartmon/byteorder.h>

namespace smartmon {

struct nvme_error_log_page {
  uint64_t  error_count;
  uint16_t  sqid;
  uint16_t  cmdid;
  uint16_t  status_field;
  uint16_t  parm_error_location;
  uint64_t  lba;
  uint32_t  nsid;
  uint8_t   vs;
  uint8_t   resv[35];
};
SMARTMON_ASSERT_SIZEOF(nvme_error_log_page, 64);

struct nvme_id_power_state {
  uint16_t  max_power;
  uint8_t   rsvd2;
  uint8_t   flags;
  uint32_t  entry_lat;
  uint32_t  exit_lat;
  uint8_t   read_tput;
  uint8_t   read_lat;
  uint8_t   write_tput;
  uint8_t   write_lat;
  uint16_t  idle_power;
  uint8_t   idle_scale;
  uint8_t   rsvd19;
  uint16_t  active_power;
  uint8_t   active_work_scale;
  uint8_t   rsvd23[9];
};
SMARTMON_ASSERT_SIZEOF(nvme_id_power_state, 32);

struct nvme_id_ctrl {
  uint16_t  vid;
  uint16_t  ssvid;
  char      sn[20];
  char      mn[40];
  char      fr[8];
  uint8_t   rab;
  uint8_t   ieee[3];
  uint8_t   cmic;
  uint8_t   mdts;
  uint16_t  cntlid;
  uint32_t  ver;
  uint32_t  rtd3r;
  uint32_t  rtd3e;
  uint32_t  oaes;
  uint32_t  ctratt;
  uint8_t   rsvd100[156];
  uint16_t  oacs;
  uint8_t   acl;
  uint8_t   aerl;
  uint8_t   frmw;
  uint8_t   lpa;
  uint8_t   elpe;
  uint8_t   npss;
  uint8_t   avscc;
  uint8_t   apsta;
  uint16_t  wctemp;
  uint16_t  cctemp;
  uint16_t  mtfa;
  uint32_t  hmpre;
  uint32_t  hmmin;
  uint8_t   tnvmcap[16];
  uint8_t   unvmcap[16];
  uint32_t  rpmbs;
  uint16_t  edstt;
  uint8_t   dsto;
  uint8_t   fwug;
  uint16_t  kas;
  uint16_t  hctma;
  uint16_t  mntmt;
  uint16_t  mxtmt;
  uint32_t  sanicap;
  uint8_t   rsvd332[180];
  uint8_t   sqes;
  uint8_t   cqes;
  uint16_t  maxcmd;
  uint32_t  nn;
  uint16_t  oncs;
  uint16_t  fuses;
  uint8_t   fna;
  uint8_t   vwc;
  uint16_t  awun;
  uint16_t  awupf;
  uint8_t   nvscc;
  uint8_t   rsvd531;
  uint16_t  acwu;
  uint8_t   rsvd534[2];
  uint32_t  sgls;
  uint8_t   rsvd540[228];
  char      subnqn[256];
  uint8_t   rsvd1024[768];
  uint32_t  ioccsz;
  uint32_t  iorcsz;
  uint16_t  icdoff;
  uint8_t   ctrattr;
  uint8_t   msdbd;
  uint8_t   rsvd1804[244];
  nvme_id_power_state psd[32];
  uint8_t   vs[1024];
};
SMARTMON_ASSERT_SIZEOF(nvme_id_ctrl, 4096);

struct nvme_lbaf {
  uint16_t  ms;
  uint8_t   ds;
  uint8_t   rp;
};
SMARTMON_ASSERT_SIZEOF(nvme_lbaf, 4);

struct nvme_id_ns {
  uint64_t  nsze;
  uint64_t  ncap;
  uint64_t  nuse;
  uint8_t   nsfeat;
  uint8_t   nlbaf;
  uint8_t   flbas;
  uint8_t   mc;
  uint8_t   dpc;
  uint8_t   dps;
  uint8_t   nmic;
  uint8_t   rescap;
  uint8_t   fpi;
  uint8_t   rsvd33;
  uint16_t  nawun;
  uint16_t  nawupf;
  uint16_t  nacwu;
  uint16_t  nabsn;
  uint16_t  nabo;
  uint16_t  nabspf;
  uint8_t   rsvd46[2];
  uint8_t   nvmcap[16];
  uint8_t   rsvd64[40];
  uint8_t   nguid[16];
  uint8_t   eui64[8];
  nvme_lbaf lbaf[16];
  uint8_t   rsvd192[192];
  uint8_t   vs[3712];
};
SMARTMON_ASSERT_SIZEOF(nvme_id_ns, 4096);

struct nvme_smart_log {
  uint8_t   critical_warning;
  uile16_t  temperature;
  uint8_t   avail_spare;
  uint8_t   spare_thresh;
  uint8_t   percent_used;
  uint8_t   rsvd6[26];
  uint8_t   data_units_read[16];
  uint8_t   data_units_written[16];
  uint8_t   host_reads[16];
  uint8_t   host_writes[16];
  uint8_t   ctrl_busy_time[16];
  uint8_t   power_cycles[16];
  uint8_t   power_on_hours[16];
  uint8_t   unsafe_shutdowns[16];
  uint8_t   media_errors[16];
  uint8_t   num_err_log_entries[16];
  uint32_t  warning_temp_time;
  uint32_t  critical_comp_time;
  uint16_t  temp_sensor[8];
  uint32_t  thm_temp1_trans_count;
  uint32_t  thm_temp2_trans_count;
  uint32_t  thm_temp1_total_time;
  uint32_t  thm_temp2_total_time;
  uint8_t   rsvd232[280];
};
SMARTMON_ASSERT_SIZEOF(nvme_smart_log, 512);

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
  nvme_admin_dev_self_test = 0x14, // NVMe 1.3
//nvme_admin_ns_attach     = 0x15,
//nvme_admin_format_nvm    = 0x80,
//nvme_admin_security_send = 0x81,
//nvme_admin_security_recv = 0x82,
};

// Figure 213 of NVM Express(TM) Base Specification, revision 2.0a, July 2021
struct nvme_self_test_result {
  uint8_t   self_test_status;
  uint8_t   segment;
  uint8_t   valid;
  uint8_t   rsvd3;
  uile64_t  power_on_hours;
  uint32_t  nsid;
  uile64_t  lba;
  uint8_t   status_code_type;
  uint8_t   status_code;
  uint8_t   vendor_specific[2];
};
SMARTMON_ASSERT_SIZEOF(nvme_self_test_result, 28);

// Figure 212 of NVM Express(TM) Base Specification, revision 2.0a, July 2021
struct nvme_self_test_log {
  uint8_t   current_operation;
  uint8_t   current_completion;
  uint8_t   rsvd2[2];
  nvme_self_test_result results[20]; // [0] = newest
};
SMARTMON_ASSERT_SIZEOF(nvme_self_test_log, 564);

} // namespace smartmon

#endif // SMARTMON_NVME_H
