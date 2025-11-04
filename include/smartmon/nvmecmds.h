/*
 * nvmecmds.h
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2016-2025 Christian Franke
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SMARTMON_NVMECMDS_H
#define SMARTMON_NVMECMDS_H

#include <smartmon/nvme.h>

#include <errno.h>
#include <stddef.h>

namespace smartmon {

class nvme_device;

// Broadcast namespace ID.
constexpr uint32_t nvme_broadcast_nsid = 0xffffffffU;

// Print NVMe debug messages?
extern unsigned char nvme_debugmode;

// Read NVMe Identify Controller data structure.
bool nvme_read_id_ctrl(nvme_device * device, nvme_id_ctrl & id_ctrl);

// Read NVMe Identify Namespace data structure for namespace NSID.
bool nvme_read_id_ns(nvme_device * device, unsigned nsid, nvme_id_ns & id_ns);

// Read NVMe log page with identifier LID.
unsigned nvme_read_log_page(nvme_device * device, unsigned nsid, unsigned char lid,
  void * data, unsigned size, bool lpo_sup, unsigned offset = 0);

// Read NVMe Error Information Log.
unsigned nvme_read_error_log(nvme_device * device, nvme_error_log_page * error_log,
  unsigned num_entries, bool lpo_sup);

// Read NVMe SMART/Health Information log.
bool nvme_read_smart_log(nvme_device * device, uint32_t nsid,
  nvme_smart_log & smart_log);

// Read NVMe Self-test Log.
bool nvme_read_self_test_log(nvme_device * device, uint32_t nsid,
  nvme_self_test_log & self_test_log);

// Start Self-test
bool nvme_self_test(nvme_device * device, uint8_t stc, uint32_t nsid);

// Return true if NVMe status indicates an error.
constexpr bool nvme_status_is_error(uint16_t status)
  { return !!(status & 0x07ff); }

// Return errno for NVMe status SCT/SC fields: 0, EINVAL or EIO.
int nvme_status_to_errno(uint16_t status);

// Return error message for NVMe status SCT/SC fields or nullptr if unknown.
const char * nvme_status_to_str(uint16_t status);

// Return error message for NVMe status SCT/SC fields or explanatory message if unknown.
const char * nvme_status_to_info_str(char * buf, size_t bufsize, uint16_t status);

// Version of above for fixed size buffers.
template <size_t SIZE>
inline const char * nvme_status_to_info_str(char (& buf)[SIZE], unsigned status)
  { return nvme_status_to_info_str(buf, SIZE, status); }

} // namespace smartmon

#endif // SMARTMON_NVMECMDS_H
