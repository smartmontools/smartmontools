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

#include <string>

namespace smartmon {

struct scsi_cmnd_io;

namespace os_darwin {

struct darwin_usb_handle;

// Return true for the Darwin disk names accepted by the raw USB backend and
// for its explicit usbraw selector namespace.
bool darwin_usb_is_device_name(const char * selector);

// Resolve a Darwin whole-disk name or explicit usbraw selector, capture the
// complete USB device, and locate its active SCSI mass-storage interface.
// Capture intentionally detaches the normal macOS drivers until close.
darwin_usb_handle * darwin_usb_open(const char * selector, int & error_number,
  std::string & error_message);

// Destroying the captured device resets it and lets macOS match its drivers
// again.  This function is safe to call only with a non-null handle.
void darwin_usb_close(darwin_usb_handle * handle);

// Stable diagnostic name for the active wire protocol.
const char * darwin_usb_transport_name(const darwin_usb_handle * handle);

// BOT and UASP commands are transferred at queue depth 1.  The implementation
// enforces the PoC's read-oriented command policy before touching the device.
bool darwin_usb_scsi_pass_through(darwin_usb_handle * handle,
  scsi_cmnd_io * iop, int & error_number, std::string & error_message);

} // namespace os_darwin
} // namespace smartmon

#endif // SMARTMON_OS_DARWIN_USB_H
