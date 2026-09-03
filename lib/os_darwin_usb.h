/*
 * os_darwin_usb.h
 *
 * Raw USB transport boundary for the Darwin smartmontools backend.
 *
 * Copyright (C) 2026 PeratX <peratx@itxtech.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SMARTMON_OS_DARWIN_USB_H
#define SMARTMON_OS_DARWIN_USB_H

#include <cstdint>
#include <string>
#include <vector>

namespace smartmon {

struct scsi_cmnd_io;

namespace os_darwin {

struct darwin_usb_handle;

enum darwin_usb_protocol
{
  darwin_usb_protocol_none,
  darwin_usb_protocol_bot,
  darwin_usb_protocol_uasp
};

struct darwin_usb_device_info
{
  std::string device_name;
  std::string vendor_name;
  std::string product_name;
  std::string serial_number;
  uint64_t registry_id;
  uint16_t vendor_id;
  uint16_t product_id;
  uint16_t device_version;
  darwin_usb_protocol protocol;
};

// Return true for the Darwin disk names accepted by the raw USB backend and
// for its explicit usbraw selector namespace.
bool darwin_usb_is_device_name(const char * selector);

// Read USB identity and mass-storage protocol information without capturing
// the device.  scan returns one entry per whole IOMedia descendant.
bool darwin_usb_get_device_info(const char * selector,
  darwin_usb_device_info & info, int & error_number,
  std::string & error_message);
bool darwin_usb_scan_devices(std::vector<darwin_usb_device_info> & devices,
  int & error_number, std::string & error_message);

const char * darwin_usb_protocol_name(darwin_usb_protocol protocol);

// Resolve a Darwin whole-disk name or explicit usbraw selector, capture the
// complete USB device, and locate its active SCSI mass-storage interface.
// Capture intentionally detaches the normal macOS drivers until close.
darwin_usb_handle * darwin_usb_open(const char * selector, int & error_number,
  std::string & error_message);

// Destroying the captured device resets it, lets macOS match its drivers again,
// and restores volumes that were mounted before capture.
bool darwin_usb_close(darwin_usb_handle * handle, int & error_number,
  std::string & error_message);

// Stable diagnostic name for the active wire protocol.
const char * darwin_usb_transport_name(const darwin_usb_handle * handle);

// BOT and UASP commands are transferred at queue depth 1.  The implementation
// enforces the PoC's read-oriented command policy before touching the device.
bool darwin_usb_scsi_pass_through(darwin_usb_handle * handle,
  scsi_cmnd_io * iop, int & error_number, std::string & error_message);

} // namespace os_darwin
} // namespace smartmon

#endif // SMARTMON_OS_DARWIN_USB_H
