/*
 * Darwin scan policy with a simulated raw USB device.  No USB I/O is used.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <smartmon/nvmecmds.h>
#include <smartmon/dev_interface.h>
#include "../os_darwin_usb.h"

using namespace smartmon;

namespace smartmon {
namespace os_darwin {

static unsigned usb_scans = 0;
static unsigned usb_opens = 0;
static unsigned usb_closes = 0;
static std::vector<uint64_t> opened_ids;
static std::vector<darwin_usb_protocol> opened_protocols;

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
darwin_usb_handle * darwin_usb_open(const char *, uint64_t & registry_id,
  int &, std::string &, darwin_usb_protocol protocol)
{
  opened_protocols.push_back(protocol);
  ++usb_opens;
  opened_ids.push_back(registry_id);
  if (!registry_id)
    registry_id = 987654321;
  return reinterpret_cast<darwin_usb_handle *>(&usb_opens);
}
bool darwin_usb_close(darwin_usb_handle *, int &, std::string &)
  { ++usb_closes; return true; }
bool darwin_usb_scsi_pass_through(darwin_usb_handle *, scsi_cmnd_io *,
  int & error, std::string & message)
  { error = EIO; message = "simulated transport failure"; return false; }
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
      || devices.at(0)->is_open() || os_darwin::usb_opens) {
    std::fprintf(stderr, "explicit USB scan did not return one unopened device\n");
    return 1;
  }
  smart_device * scanned = devices.at(0);
  if (!scanned->open() || !scanned->open() || !scanned->close()
      || !scanned->close() || !scanned->open() || !scanned->close()
      || os_darwin::usb_opens != 2 || os_darwin::usb_closes != 2
      || os_darwin::opened_ids != std::vector<uint64_t>(2, 123456789)) {
    std::fprintf(stderr, "scanned identity or repeated open/close was not preserved\n");
    return 1;
  }
  smart_device_auto_ptr explicit_device(
    smi()->get_smart_device("usbraw:disk999", "scsi"));
  if (!explicit_device || !explicit_device->open() || !explicit_device->close()
      || !explicit_device->open() || !explicit_device->close()
      || os_darwin::opened_ids.size() != 4
      || os_darwin::opened_ids[2] != 0
      || os_darwin::opened_ids[3] != 987654321) {
    std::fprintf(stderr, "first explicit open did not pin subsequent opens\n");
    return 1;
  }
  for (auto protocol : os_darwin::opened_protocols) {
    if (protocol != os_darwin::darwin_usb_protocol::none)
      return 1; // Default must follow the system, never prefer a wire protocol.
  }
  struct selection_case { const char * type; os_darwin::darwin_usb_protocol protocol; };
  const selection_case cases[] = {
    {"sntjmicron+usb,uasp", os_darwin::darwin_usb_protocol::uasp},
    {"sntjmicron,0x1+usb,bot", os_darwin::darwin_usb_protocol::bot},
    {"sntasmedia+usb,uasp", os_darwin::darwin_usb_protocol::uasp},
    {"sntrealtek+usb,bot", os_darwin::darwin_usb_protocol::bot},
    {"sntjmicron/sat+usb,bot", os_darwin::darwin_usb_protocol::bot},
    {"sat,16+usb,uasp", os_darwin::darwin_usb_protocol::uasp},
    {"scsi+usb,bot", os_darwin::darwin_usb_protocol::bot},
  };
  for (const auto & test : cases) {
    const unsigned opens = os_darwin::usb_opens;
    smart_device_auto_ptr dev(smi()->get_smart_device("usbraw:123456789", test.type));
    if (!dev || !dev->open() || os_darwin::opened_protocols.back() != test.protocol)
      return 1;
    if (dev->is_nvme()) {
      nvme_id_ctrl id = {};
      if (nvme_read_id_ctrl(dev->to_nvme(), id) || os_darwin::usb_opens != opens + 1)
        return 1; // Failure must not recapture using an alternative protocol.
    }
    if (!dev->close())
      return 1;
  }
  for (const char * type : {"sntjmicron+usb,auto", "sntjmicron+usb,typo",
      "nvme+usb,bot", "sntjmicron,invalid+usb,bot", "sat,32+usb,uasp"}) {
    const unsigned opens = os_darwin::usb_opens;
    smart_device_auto_ptr invalid(smi()->get_smart_device("usbraw:123456789", type));
    if (invalid || os_darwin::usb_opens != opens)
      return 1;
  }
  return 0;
}
