/*
 * Darwin scan policy with a simulated raw USB device.  No USB I/O is used.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include <cstdio>
#include <cstdlib>
#include <smartmon/dev_interface.h>
#include "../os_darwin_usb.h"

using namespace smartmon;

namespace smartmon {
namespace os_darwin {

static unsigned usb_scans = 0;

bool darwin_usb_scan_devices(std::vector<darwin_usb_device_info> & devices,
  int &, std::string &)
{
  ++usb_scans;
  darwin_usb_device_info info = {};
  info.device_name = "usbraw:123456789";
  info.registry_id = 123456789;
  info.protocol = darwin_usb_protocol::bot;
  devices.push_back(info);
  return true;
}

bool darwin_usb_is_device_name(const char *) { return true; }
bool darwin_usb_get_device_info(const char *, darwin_usb_device_info &,
  int &, std::string &) { std::abort(); }
darwin_usb_handle * darwin_usb_open(const char *, int &, std::string &)
  { std::abort(); }
bool darwin_usb_close(darwin_usb_handle *, int &, std::string &)
  { std::abort(); }
bool darwin_usb_scsi_pass_through(darwin_usb_handle *, scsi_cmnd_io *,
  int &, std::string &) { std::abort(); }
const char * darwin_usb_transport_name(const darwin_usb_handle *)
  { return "simulated"; }

} // namespace os_darwin
} // namespace smartmon

int main()
{
  smart_interface::init();
  // smartctl's default --scan/--scan-open and smartd's DEVICESCAN all use
  // the empty type list.  Neither default nor other types may request USB.
  const char * types[] = { nullptr, "ata", "nvme", "scsi", "sat" };
  for (const char * type : types) {
    smart_device_list devices;
    smart_devtype_list scan_types;
    if (type)
      scan_types.push_back(type);
    if (!smi()->scan_smart_devices(devices, scan_types)
        || os_darwin::usb_scans != 0) {
      std::fprintf(stderr, "unexpected raw USB scan for %s\n", type ? type : "default");
      return 1;
    }
  }
  smart_devtype_list types_to_scan;
  types_to_scan.push_back("scsi");
  types_to_scan.push_back("usb");
  smart_device_list devices;
  if (!smi()->scan_smart_devices(devices, types_to_scan)
      || os_darwin::usb_scans != 1 || devices.size() != 1
      || devices.at(0)->is_open()) {
    std::fprintf(stderr, "explicit USB scan did not return one unopened device\n");
    return 1;
  }
  return 0;
}
