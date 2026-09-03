/*
 * os_darwin_usb.mm
 *
 * Raw USB device capture for the Darwin smartmontools backend.
 *
 * Copyright (C) 2026 PeratX <peratx@itxtech.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include <unistd.h>

#include <dispatch/dispatch.h>

#import <Foundation/Foundation.h>
#import <IOKit/IOBSD.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/usb/USBSpec.h>
#import <IOUSBHost/IOUSBHost.h>

#include <smartmon/scsicmds.h>
#include "os_darwin_usb.h"

namespace smartmon {
namespace os_darwin {

enum darwin_usb_transport {
  darwin_usb_transport_none,
  darwin_usb_transport_bot,
  darwin_usb_transport_uasp
};

struct darwin_usb_handle
{
  IOUSBHostDevice * device;
  IOUSBHostInterface * interface;
  IOUSBHostPipe * bulk_in;
  IOUSBHostPipe * bulk_out;
  IOUSBHostPipe * uas_command;
  IOUSBHostPipe * uas_status;
  IOUSBHostPipe * uas_data_in;
  IOUSBHostPipe * uas_data_out;
  IOUSBHostStream * uas_status_stream;
  IOUSBHostStream * uas_data_in_stream;
  IOUSBHostStream * uas_data_out_stream;
  bool uas_streams_enabled;
  bool transport_failed;
  darwin_usb_transport transport;
  uint64_t registry_id;
  uint8_t interface_number;
  uint32_t next_tag;
};

struct bot_result
{
  uint8_t status;
  uint32_t residue;
};

static std::string ns_error_string(NSError * error)
{
  if (!error)
    return "unknown IOUSBHost error";
  NSString * text = [error localizedDescription];
  return text ? std::string([text UTF8String]) : "unknown IOUSBHost error";
}

static io_service_t find_usb_device_ancestor(io_service_t service)
{
  io_registry_entry_t current = service;
  IOObjectRetain(current);

  while (current) {
    if (IOObjectConformsTo(current, "IOUSBHostDevice"))
      return current;

    io_registry_entry_t parent = MACH_PORT_NULL;
    kern_return_t kr = IORegistryEntryGetParentEntry(current, kIOServicePlane,
      &parent);
    IOObjectRelease(current);
    if (kr != KERN_SUCCESS)
      return MACH_PORT_NULL;
    current = parent;
  }
  return MACH_PORT_NULL;
}

static bool parse_registry_id(const char * text, uint64_t & value)
{
  if (!text || !*text)
    return false;
  char * end = 0;
  errno = 0;
  unsigned long long parsed = strtoull(text, &end, 0);
  if (errno || !end || *end || !parsed)
    return false;
  value = parsed;
  return true;
}

static bool is_whole_disk_name(const char * text)
{
  if (!text || strncmp(text, "disk", 4) || !text[4])
    return false;
  for (const char * p = text + 4; *p; ++p) {
    if (*p < '0' || *p > '9')
      return false;
  }
  return true;
}

static const char * strip_bsd_device_path(const char * value)
{
  if (!strncmp(value, "/dev/rdisk", 10))
    return value + 6;
  if (!strncmp(value, "/dev/disk", 9))
    return value + 5;
  return value;
}

bool darwin_usb_is_device_name(const char * selector)
{
  static const char prefix[] = "usbraw:";
  if (!selector)
    return false;
  if (!strncmp(selector, prefix, sizeof(prefix) - 1))
    return true;
  return is_whole_disk_name(strip_bsd_device_path(selector));
}

static bool validate_selector_syntax(const char * selector, std::string & error)
{
  static const char prefix[] = "usbraw:";
  if (!selector) {
    error = "raw USB selector is missing";
    return false;
  }

  const bool explicit_raw = !strncmp(selector, prefix, sizeof(prefix) - 1);
  const char * value = strip_bsd_device_path(explicit_raw
    ? selector + sizeof(prefix) - 1 : selector);

  uint64_t registry_id = 0;
  if (is_whole_disk_name(value)
      || (explicit_raw && parse_registry_id(value, registry_id)))
    return true;
  error = explicit_raw
    ? "selector must be a whole disk (diskN, /dev/diskN, or /dev/rdiskN) or a non-zero I/O Registry entry ID"
    : "raw USB access requires a whole disk (diskN, /dev/diskN, or /dev/rdiskN)";
  return false;
}

static io_service_t resolve_selector(const char * selector, std::string & error)
{
  static const char prefix[] = "usbraw:";
  const bool explicit_raw = !strncmp(selector, prefix, sizeof(prefix) - 1);
  const char * value = explicit_raw
    ? selector + sizeof(prefix) - 1 : selector;
  io_service_t selected = MACH_PORT_NULL;

  value = strip_bsd_device_path(value);

  if (is_whole_disk_name(value)) {
    CFMutableDictionaryRef matching = IOBSDNameMatching(kIOMainPortDefault, 0,
      value);
    if (matching)
      selected = IOServiceGetMatchingService(kIOMainPortDefault, matching);
    if (!selected) {
      error = std::string("no I/O Registry service found for '") + value + "'";
      return MACH_PORT_NULL;
    }
  }
  else {
    uint64_t registry_id = 0;
    if (!parse_registry_id(value, registry_id)) {
      error = "selector must be a whole disk (diskN, /dev/diskN, or /dev/rdiskN) or a non-zero I/O Registry entry ID";
      return MACH_PORT_NULL;
    }
    CFMutableDictionaryRef matching = IORegistryEntryIDMatching(registry_id);
    selected = IOServiceGetMatchingService(kIOMainPortDefault, matching);
    if (!selected) {
      error = "no I/O Registry service found for the requested entry ID";
      return MACH_PORT_NULL;
    }
  }

  io_service_t usb_device = find_usb_device_ancestor(selected);
  IOObjectRelease(selected);
  if (!usb_device)
    error = "selected service is not attached to an IOUSBHostDevice";
  return usb_device;
}

static IOUSBHostInterface * find_mass_storage_interface(IOUSBHostDevice * device,
  darwin_usb_transport & transport, uint8_t & interface_number,
  std::string & error)
{
  io_iterator_t iterator = MACH_PORT_NULL;
  kern_return_t kr = IORegistryEntryCreateIterator([device ioService],
    kIOServicePlane, kIORegistryIterateRecursively, &iterator);
  if (kr != KERN_SUCCESS) {
    error = "unable to enumerate captured USB interfaces";
    return nil;
  }

  IOUSBHostInterface * bot_interface = nil;
  uint8_t bot_interface_number = 0;
  io_service_t service = MACH_PORT_NULL;
  while ((service = IOIteratorNext(iterator))) {
    if (!IOObjectConformsTo(service, "IOUSBHostInterface")) {
      IOObjectRelease(service);
      continue;
    }

    NSError * ns_error = nil;
    IOUSBHostInterface * candidate = [[IOUSBHostInterface alloc]
      initWithIOService:service
      options:IOUSBHostObjectInitOptionsNone
      queue:nil
      error:&ns_error
      interestHandler:nil];
    IOObjectRelease(service);
    if (!candidate)
      continue;

    const IOUSBInterfaceDescriptor * descriptor = [candidate interfaceDescriptor];
    if (!descriptor || descriptor->bInterfaceClass != kUSBMassStorageInterfaceClass
        || descriptor->bInterfaceSubClass != kUSBMassStorageSCSISubClass) {
      [candidate destroy];
      [candidate release];
      continue;
    }

    if (descriptor->bInterfaceProtocol == 0x62) {
      if (bot_interface) {
        [bot_interface destroy];
        [bot_interface release];
      }
      transport = darwin_usb_transport_uasp;
      interface_number = descriptor->bInterfaceNumber;
      IOObjectRelease(iterator);
      return candidate;
    }
    if (descriptor->bInterfaceProtocol == 0x50 && !bot_interface) {
      bot_interface = candidate;
      bot_interface_number = descriptor->bInterfaceNumber;
      continue;
    }

    [candidate destroy];
    [candidate release];
  }
  IOObjectRelease(iterator);

  if (bot_interface) {
    transport = darwin_usb_transport_bot;
    interface_number = bot_interface_number;
    return bot_interface;
  }

  error = "captured device has no active SCSI-transparent BOT or UASP interface";
  return nil;
}

static bool copy_bot_pipes(IOUSBHostInterface * interface,
  IOUSBHostPipe * & bulk_in, IOUSBHostPipe * & bulk_out, std::string & error)
{
  bulk_in = nil;
  bulk_out = nil;

  const IOUSBConfigurationDescriptor * configuration =
    [interface configurationDescriptor];
  const IOUSBInterfaceDescriptor * interface_descriptor =
    [interface interfaceDescriptor];
  if (!configuration || !interface_descriptor) {
    error = "BOT interface descriptors are unavailable";
    return false;
  }

  const IOUSBEndpointDescriptor * endpoint = 0;
  while ((endpoint = IOUSBGetNextEndpointDescriptor(configuration,
      interface_descriptor, (const IOUSBDescriptorHeader *)endpoint))) {
    if ((endpoint->bmAttributes & kIOUSBEndpointDescriptorTransferType)
        != kIOUSBEndpointDescriptorTransferTypeBulk)
      continue;

    NSError * ns_error = nil;
    IOUSBHostPipe * pipe = [interface
      copyPipeWithAddress:endpoint->bEndpointAddress error:&ns_error];
    if (!pipe) {
      error = std::string("unable to open BOT bulk pipe: ")
        + ns_error_string(ns_error);
      if (bulk_in)
        [bulk_in release];
      if (bulk_out)
        [bulk_out release];
      bulk_in = bulk_out = nil;
      return false;
    }

    if (endpoint->bEndpointAddress & kIOUSBEndpointDescriptorDirection) {
      if (!bulk_in)
        bulk_in = pipe;
      else
        [pipe release];
    }
    else {
      if (!bulk_out)
        bulk_out = pipe;
      else
        [pipe release];
    }
  }

  if (bulk_in && bulk_out)
    return true;

  if (bulk_in)
    [bulk_in release];
  if (bulk_out)
    [bulk_out release];
  bulk_in = bulk_out = nil;
  error = "BOT interface does not expose one bulk-in and one bulk-out pipe";
  return false;
}

static bool uas_pipe_direction_is_valid(uint8_t pipe_id, uint8_t address)
{
  const bool input = !!(address & kIOUSBEndpointDescriptorDirection);
  return ((pipe_id == 1 || pipe_id == 4) ? !input : input);
}

static bool assign_uas_pipe(uint8_t pipe_id, IOUSBHostPipe * pipe,
  IOUSBHostPipe * & command, IOUSBHostPipe * & status,
  IOUSBHostPipe * & data_in, IOUSBHostPipe * & data_out)
{
  IOUSBHostPipe ** destination = 0;
  switch (pipe_id) {
    case 1: destination = &command; break;
    case 2: destination = &status; break;
    case 3: destination = &data_in; break;
    case 4: destination = &data_out; break;
    default: return false;
  }
  if (*destination)
    return false;
  *destination = pipe;
  return true;
}

static void release_uas_pipes(IOUSBHostPipe * & command,
  IOUSBHostPipe * & status, IOUSBHostPipe * & data_in,
  IOUSBHostPipe * & data_out)
{
  if (command)
    [command release];
  if (status)
    [status release];
  if (data_in)
    [data_in release];
  if (data_out)
    [data_out release];
  command = status = data_in = data_out = nil;
}

static bool copy_uas_pipes(IOUSBHostInterface * interface,
  IOUSBHostPipe * & command, IOUSBHostPipe * & status,
  IOUSBHostPipe * & data_in, IOUSBHostPipe * & data_out, std::string & error)
{
  command = status = data_in = data_out = nil;
  const IOUSBConfigurationDescriptor * configuration =
    [interface configurationDescriptor];
  const IOUSBInterfaceDescriptor * interface_descriptor =
    [interface interfaceDescriptor];
  if (!configuration || !interface_descriptor) {
    error = "UASP interface descriptors are unavailable";
    return false;
  }

  const IOUSBDescriptorHeader * descriptor = 0;
  const IOUSBEndpointDescriptor * endpoint = 0;
  while ((descriptor = IOUSBGetNextAssociatedDescriptor(configuration,
      (const IOUSBDescriptorHeader *)interface_descriptor, descriptor))) {
    if (descriptor->bDescriptorType == kUSBEndpointDesc) {
      endpoint = (const IOUSBEndpointDescriptor *)descriptor;
      continue;
    }
    if (!endpoint || descriptor->bDescriptorType != kUSBClassSpecificDescriptor
        || descriptor->bLength < sizeof(UASPipeDescriptor))
      continue;

    const UASPipeDescriptor * usage = (const UASPipeDescriptor *)descriptor;
    if (usage->bPipeID < 1 || usage->bPipeID > 4)
      continue;
    if ((endpoint->bmAttributes & kIOUSBEndpointDescriptorTransferType)
          != kIOUSBEndpointDescriptorTransferTypeBulk
        || !uas_pipe_direction_is_valid(usage->bPipeID,
          endpoint->bEndpointAddress)) {
      release_uas_pipes(command, status, data_in, data_out);
      error = "UASP pipe-usage descriptor has an invalid endpoint";
      return false;
    }

    NSError * ns_error = nil;
    IOUSBHostPipe * pipe = [interface
      copyPipeWithAddress:endpoint->bEndpointAddress error:&ns_error];
    if (!pipe) {
      release_uas_pipes(command, status, data_in, data_out);
      error = std::string("unable to open UASP pipe: ")
        + ns_error_string(ns_error);
      return false;
    }
    if (!assign_uas_pipe(usage->bPipeID, pipe, command, status, data_in,
        data_out)) {
      [pipe release];
      release_uas_pipes(command, status, data_in, data_out);
      error = "UASP pipe-usage descriptors are duplicated or invalid";
      return false;
    }
    endpoint = 0;
  }

  if (command && status && data_in && data_out)
    return true;
  release_uas_pipes(command, status, data_in, data_out);
  error = "UASP interface does not expose all four required pipe usages";
  return false;
}

static void disable_uas_streams(darwin_usb_handle * handle)
{
  if (!handle->uas_streams_enabled)
    return;
  if (handle->uas_status_stream)
    [handle->uas_status_stream release];
  if (handle->uas_data_in_stream)
    [handle->uas_data_in_stream release];
  if (handle->uas_data_out_stream)
    [handle->uas_data_out_stream release];
  handle->uas_status_stream = nil;
  handle->uas_data_in_stream = nil;
  handle->uas_data_out_stream = nil;
  [handle->uas_status disableStreamsWithError:nil];
  [handle->uas_data_in disableStreamsWithError:nil];
  [handle->uas_data_out disableStreamsWithError:nil];
  handle->uas_streams_enabled = false;
}

static void try_enable_uas_streams(darwin_usb_handle * handle)
{
  NSError * error = nil;
  if (![handle->uas_status enableStreamsWithError:&error])
    return;
  if (![handle->uas_data_in enableStreamsWithError:&error]) {
    [handle->uas_status disableStreamsWithError:nil];
    return;
  }
  if (![handle->uas_data_out enableStreamsWithError:&error]) {
    [handle->uas_status disableStreamsWithError:nil];
    [handle->uas_data_in disableStreamsWithError:nil];
    return;
  }

  handle->uas_status_stream = [handle->uas_status
    copyStreamWithStreamID:1 error:&error];
  handle->uas_data_in_stream = [handle->uas_data_in
    copyStreamWithStreamID:1 error:&error];
  handle->uas_data_out_stream = [handle->uas_data_out
    copyStreamWithStreamID:1 error:&error];
  if (!handle->uas_status_stream || !handle->uas_data_in_stream
      || !handle->uas_data_out_stream) {
    handle->uas_streams_enabled = true;
    disable_uas_streams(handle);
    return;
  }
  handle->uas_streams_enabled = true;
}

static uint32_t get_le32(const uint8_t * value)
{
  return ((uint32_t)value[0] << 0) | ((uint32_t)value[1] << 8)
    | ((uint32_t)value[2] << 16) | ((uint32_t)value[3] << 24);
}

static void put_le32(uint8_t * value, uint32_t number)
{
  value[0] = (uint8_t)(number >> 0);
  value[1] = (uint8_t)(number >> 8);
  value[2] = (uint8_t)(number >> 16);
  value[3] = (uint8_t)(number >> 24);
}

static bool pipe_transfer(IOUSBHostPipe * pipe, void * buffer, size_t length,
  bool input, unsigned timeout, size_t & transferred, std::string & error)
{
  transferred = 0;
  NSMutableData * data = nil;
  if (length) {
    data = input
      ? [[NSMutableData alloc] initWithLength:length]
      : [[NSMutableData alloc] initWithBytes:buffer length:length];
  }

  NSError * ns_error = nil;
  NSUInteger count = 0;
  BOOL ok = [pipe sendIORequestWithData:data
    bytesTransferred:&count
    completionTimeout:(timeout ? timeout : 60)
    error:&ns_error];

  transferred = std::min(length, (size_t)count);
  if (input && data && transferred)
    memcpy(buffer, [data bytes], transferred);
  [data release];

  if (!ok) {
    error = ns_error_string(ns_error);
    return false;
  }
  return true;
}

static void bot_reset_recovery(darwin_usb_handle * handle)
{
  IOUSBDeviceRequest request = {};
  request.bmRequestType = 0x21; // host-to-device, class, interface
  request.bRequest = 0xff;      // Bulk-Only Mass Storage Reset
  request.wIndex = handle->interface_number;
  NSError * error = nil;
  [handle->device sendDeviceRequest:request data:nil bytesTransferred:nil
    completionTimeout:5 error:&error];
  [handle->bulk_in clearStallWithError:nil];
  [handle->bulk_out clearStallWithError:nil];
}

static bool bot_execute(darwin_usb_handle * handle, const uint8_t * cdb,
  size_t cdb_length, int direction, void * data, size_t data_length,
  unsigned timeout, bot_result & result, int & error_number,
  std::string & error_message)
{
  if (!cdb || !cdb_length || cdb_length > 16 || data_length > UINT32_MAX) {
    error_number = EINVAL;
    error_message = "BOT request has an invalid CDB or transfer length";
    return false;
  }

  uint32_t tag = ++handle->next_tag;
  if (!tag)
    tag = ++handle->next_tag;

  uint8_t cbw[31] = {};
  put_le32(cbw + 0, 0x43425355); // 'USBC'
  put_le32(cbw + 4, tag);
  put_le32(cbw + 8, (uint32_t)data_length);
  cbw[12] = (direction == DXFER_FROM_DEVICE ? 0x80 : 0x00);
  cbw[13] = 0; // LUN 0 is the PoC boundary.
  cbw[14] = (uint8_t)cdb_length;
  memcpy(cbw + 15, cdb, cdb_length);

  size_t transferred = 0;
  std::string transfer_error;
  if (!pipe_transfer(handle->bulk_out, cbw, sizeof(cbw), false, timeout,
      transferred, transfer_error) || transferred != sizeof(cbw)) {
    bot_reset_recovery(handle);
    error_number = EIO;
    error_message = std::string("BOT CBW transfer failed: ")
      + (transfer_error.empty() ? "short transfer" : transfer_error);
    return false;
  }

  bool data_ok = true;
  size_t data_transferred = 0;
  if (data_length) {
    IOUSBHostPipe * data_pipe = (direction == DXFER_FROM_DEVICE
      ? handle->bulk_in : handle->bulk_out);
    data_ok = pipe_transfer(data_pipe, data, data_length,
      direction == DXFER_FROM_DEVICE, timeout, data_transferred,
      transfer_error);
    if (!data_ok)
      [data_pipe clearStallWithError:nil];
  }

  uint8_t csw[13] = {};
  size_t csw_length = 0;
  std::string csw_error;
  bool csw_ok = pipe_transfer(handle->bulk_in, csw, sizeof(csw), true,
    timeout, csw_length, csw_error);
  if (!csw_ok || csw_length != sizeof(csw)
      || get_le32(csw + 0) != 0x53425355 || get_le32(csw + 4) != tag
      || csw[12] > 2) {
    bot_reset_recovery(handle);
    error_number = EIO;
    error_message = std::string("BOT CSW validation failed: ")
      + (csw_error.empty() ? "invalid signature, tag, length, or status"
                           : csw_error);
    return false;
  }

  result.residue = get_le32(csw + 8);
  result.status = csw[12];
  if (result.residue > data_length) {
    bot_reset_recovery(handle);
    error_number = EIO;
    error_message = "BOT CSW residue exceeds the requested transfer length";
    return false;
  }
  if (data_length && data_ok
      && data_transferred + result.residue != data_length) {
    bot_reset_recovery(handle);
    error_number = EIO;
    error_message = "BOT data length and CSW residue are inconsistent";
    return false;
  }
  if (result.status == 2) {
    bot_reset_recovery(handle);
    error_number = EIO;
    error_message = "BOT phase error";
    return false;
  }
  if (!data_ok && result.status == 0) {
    bot_reset_recovery(handle);
    error_number = EIO;
    error_message = std::string("BOT data transfer failed: ") + transfer_error;
    return false;
  }
  return true;
}

struct uas_async_transfer
{
  NSMutableData * data;
  dispatch_semaphore_t semaphore;
  IOUSBHostPipe * pipe;
  IOReturn status;
  NSUInteger bytes_transferred;
};

static uas_async_transfer * new_uas_async_transfer(void * buffer,
  size_t length, bool input, IOUSBHostPipe * pipe)
{
  uas_async_transfer * request = new uas_async_transfer;
  request->data = input
    ? [[NSMutableData alloc] initWithLength:length]
    : [[NSMutableData alloc] initWithBytes:buffer length:length];
  request->semaphore = dispatch_semaphore_create(0);
  request->pipe = pipe;
  request->status = kIOReturnNotReady;
  request->bytes_transferred = 0;
  return request;
}

static void delete_uas_async_transfer(uas_async_transfer * request)
{
  if (!request)
    return;
  [request->data release];
#if !__has_feature(objc_arc)
  dispatch_release(request->semaphore);
#endif
  delete request;
}

static uas_async_transfer * enqueue_uas_pipe_transfer(IOUSBHostPipe * pipe,
  void * buffer, size_t length, bool input, unsigned timeout,
  std::string & error_message)
{
  uas_async_transfer * request = new_uas_async_transfer(buffer, length, input,
    pipe);
  NSError * ns_error = nil;
  BOOL ok = [pipe enqueueIORequestWithData:request->data
    completionTimeout:(timeout ? timeout : 60)
    error:&ns_error
    completionHandler:^(IOReturn status, NSUInteger bytes_transferred) {
      request->status = status;
      request->bytes_transferred = bytes_transferred;
      dispatch_semaphore_signal(request->semaphore);
    }];
  if (!ok) {
    error_message = ns_error_string(ns_error);
    delete_uas_async_transfer(request);
    return 0;
  }
  return request;
}

static uas_async_transfer * enqueue_uas_stream_transfer(IOUSBHostStream * stream,
  void * buffer, size_t length, bool input, std::string & error_message)
{
  uas_async_transfer * request = new_uas_async_transfer(buffer, length, input,
    [stream hostPipe]);
  NSError * ns_error = nil;
  BOOL ok = [stream enqueueIORequestWithData:request->data
    error:&ns_error
    completionHandler:^(IOReturn status, NSUInteger bytes_transferred) {
      request->status = status;
      request->bytes_transferred = bytes_transferred;
      dispatch_semaphore_signal(request->semaphore);
    }];
  if (!ok) {
    error_message = ns_error_string(ns_error);
    delete_uas_async_transfer(request);
    return 0;
  }
  return request;
}

static void cancel_uas_async_transfer(uas_async_transfer * request)
{
  if (!request)
    return;
  [request->pipe abortWithOption:IOUSBHostAbortOptionSynchronous error:nil];
  dispatch_semaphore_wait(request->semaphore, DISPATCH_TIME_FOREVER);
  delete_uas_async_transfer(request);
}

static bool finish_uas_async_transfer(uas_async_transfer * request,
  void * buffer, size_t length, bool input, unsigned timeout,
  size_t & transferred, std::string & error_message)
{
  const unsigned timeout_seconds = (timeout ? timeout : 60);
  dispatch_time_t deadline = dispatch_time(DISPATCH_TIME_NOW,
    (int64_t)timeout_seconds * NSEC_PER_SEC);
  if (dispatch_semaphore_wait(request->semaphore, deadline)) {
    [request->pipe abortWithOption:IOUSBHostAbortOptionSynchronous error:nil];
    dispatch_semaphore_wait(request->semaphore, DISPATCH_TIME_FOREVER);
    error_message = "UASP transfer timed out";
    delete_uas_async_transfer(request);
    return false;
  }

  transferred = std::min(length, (size_t)request->bytes_transferred);
  if (input && transferred)
    memcpy(buffer, [request->data bytes], transferred);
  IOReturn status = request->status;
  delete_uas_async_transfer(request);
  if (status != kIOReturnSuccess) {
    char buffer_text[80];
    snprintf(buffer_text, sizeof(buffer_text),
      "UASP transfer failed with IOReturn 0x%08x", (unsigned)status);
    error_message = buffer_text;
    return false;
  }
  return true;
}

static uint16_t get_be16(const uint8_t * value)
{
  return ((uint16_t)value[0] << 8) | value[1];
}

static void put_be16(uint8_t * value, uint16_t number)
{
  value[0] = (uint8_t)(number >> 8);
  value[1] = (uint8_t)number;
}

static bool parse_uas_status(const uint8_t * status, size_t status_length,
  scsi_cmnd_io * iop, int & error_number, std::string & error_message)
{
  if (status_length < 4 || get_be16(status + 2) != 1) {
    error_number = EIO;
    error_message = "UASP status IU has an invalid length or tag";
    return false;
  }

  if (status[0] == 0x04) { // RESPONSE IU is reserved for task management.
    error_number = EIO;
    error_message = "unexpected UASP response IU for a SCSI command";
    return false;
  }
  if (status[0] != 0x03 || status_length < 16) {
    error_number = EIO;
    error_message = "UASP command completed with an unexpected IU";
    return false;
  }

  iop->scsi_status = status[6];
  const size_t sense_length = get_be16(status + 14);
  if (sense_length > status_length - 16) {
    error_number = EIO;
    error_message = "UASP sense IU length exceeds the received status data";
    return false;
  }
  if (iop->sensep && iop->max_sense_len && sense_length) {
    iop->resp_sense_len = std::min(iop->max_sense_len, sense_length);
    memcpy(iop->sensep, status + 16, iop->resp_sense_len);
  }
  return true;
}

static void abort_uas_transport(darwin_usb_handle * handle)
{
  [handle->uas_command abortWithOption:IOUSBHostAbortOptionSynchronous error:nil];
  [handle->uas_status abortWithOption:IOUSBHostAbortOptionSynchronous error:nil];
  [handle->uas_data_in abortWithOption:IOUSBHostAbortOptionSynchronous error:nil];
  [handle->uas_data_out abortWithOption:IOUSBHostAbortOptionSynchronous error:nil];
  handle->transport_failed = true;
}

static bool uas_execute_with_streams(darwin_usb_handle * handle,
  scsi_cmnd_io * iop, const uint8_t * command_iu, size_t command_iu_length,
  int & error_number, std::string & error_message)
{
  uint8_t status_iu[16 + 0xff] = {};
  uas_async_transfer * status_request = enqueue_uas_stream_transfer(
    handle->uas_status_stream, status_iu, sizeof(status_iu), true,
    error_message);
  if (!status_request) {
    error_number = EIO;
    return false;
  }

  uas_async_transfer * data_request = 0;
  if (iop->dxfer_len) {
    IOUSBHostStream * stream = (iop->dxfer_dir == DXFER_FROM_DEVICE
      ? handle->uas_data_in_stream : handle->uas_data_out_stream);
    data_request = enqueue_uas_stream_transfer(stream, iop->dxferp,
      iop->dxfer_len, iop->dxfer_dir == DXFER_FROM_DEVICE, error_message);
    if (!data_request) {
      cancel_uas_async_transfer(status_request);
      error_number = EIO;
      return false;
    }
  }

  size_t command_length = 0;
  std::string command_error;
  if (!pipe_transfer(handle->uas_command, (void *)command_iu,
      command_iu_length, false, iop->timeout, command_length, command_error)
      || command_length != command_iu_length) {
    cancel_uas_async_transfer(status_request);
    cancel_uas_async_transfer(data_request);
    error_number = EIO;
    error_message = std::string("UASP command IU transfer failed: ")
      + (command_error.empty() ? "short transfer" : command_error);
    return false;
  }

  size_t status_length = 0;
  if (!finish_uas_async_transfer(status_request, status_iu,
      sizeof(status_iu), true, iop->timeout, status_length, error_message)) {
    cancel_uas_async_transfer(data_request);
    error_number = EIO;
    return false;
  }
  if (!parse_uas_status(status_iu, status_length, iop, error_number,
      error_message)) {
    cancel_uas_async_transfer(data_request);
    return false;
  }

  if (!data_request)
    return true;
  if (iop->scsi_status) {
    cancel_uas_async_transfer(data_request);
    iop->resid = (int)iop->dxfer_len;
    return true;
  }

  size_t data_length = 0;
  if (!finish_uas_async_transfer(data_request, iop->dxferp, iop->dxfer_len,
      iop->dxfer_dir == DXFER_FROM_DEVICE, iop->timeout, data_length,
      error_message)) {
    error_number = EIO;
    return false;
  }
  iop->resid = (int)(iop->dxfer_len - data_length);
  return true;
}

static bool uas_execute_without_streams(darwin_usb_handle * handle,
  scsi_cmnd_io * iop, const uint8_t * command_iu, size_t command_iu_length,
  int & error_number, std::string & error_message)
{
  uint8_t first_iu[16 + 0xff] = {};
  uas_async_transfer * status_request = enqueue_uas_pipe_transfer(
    handle->uas_status, first_iu, sizeof(first_iu), true, iop->timeout,
    error_message);
  if (!status_request) {
    error_number = EIO;
    return false;
  }

  size_t command_length = 0;
  std::string command_error;
  if (!pipe_transfer(handle->uas_command, (void *)command_iu,
      command_iu_length, false, iop->timeout, command_length, command_error)
      || command_length != command_iu_length) {
    cancel_uas_async_transfer(status_request);
    error_number = EIO;
    error_message = std::string("UASP command IU transfer failed: ")
      + (command_error.empty() ? "short transfer" : command_error);
    return false;
  }

  size_t first_length = 0;
  if (!finish_uas_async_transfer(status_request, first_iu, sizeof(first_iu),
      true, iop->timeout, first_length, error_message)) {
    error_number = EIO;
    return false;
  }
  if (first_length < 4 || get_be16(first_iu + 2) != 1) {
    error_number = EIO;
    error_message = "UASP IU has an invalid length or tag";
    return false;
  }

  const uint8_t expected_ready = (iop->dxfer_dir == DXFER_FROM_DEVICE
    ? 0x06 : 0x07);
  if (!iop->dxfer_len || first_iu[0] != expected_ready) {
    if (iop->dxfer_len && first_iu[0] == 0x03) {
      iop->resid = (int)iop->dxfer_len;
      return parse_uas_status(first_iu, first_length, iop, error_number,
        error_message);
    }
    return parse_uas_status(first_iu, first_length, iop, error_number,
      error_message);
  }

  uint8_t final_iu[16 + 0xff] = {};
  uas_async_transfer * final_status_request = enqueue_uas_pipe_transfer(
    handle->uas_status, final_iu, sizeof(final_iu), true, iop->timeout,
    error_message);
  if (!final_status_request) {
    error_number = EIO;
    return false;
  }

  IOUSBHostPipe * data_pipe = (iop->dxfer_dir == DXFER_FROM_DEVICE
    ? handle->uas_data_in : handle->uas_data_out);
  size_t data_length = 0;
  std::string data_error;
  if (!pipe_transfer(data_pipe, iop->dxferp, iop->dxfer_len,
      iop->dxfer_dir == DXFER_FROM_DEVICE, iop->timeout, data_length,
      data_error)) {
    cancel_uas_async_transfer(final_status_request);
    error_number = EIO;
    error_message = std::string("UASP data transfer failed: ") + data_error;
    return false;
  }
  iop->resid = (int)(iop->dxfer_len - data_length);

  size_t final_length = 0;
  if (!finish_uas_async_transfer(final_status_request, final_iu,
      sizeof(final_iu), true, iop->timeout, final_length, error_message)) {
    error_number = EIO;
    return false;
  }
  return parse_uas_status(final_iu, final_length, iop, error_number,
    error_message);
}

static bool uas_execute(darwin_usb_handle * handle, scsi_cmnd_io * iop,
  int & error_number, std::string & error_message)
{
  if (iop->cmnd_len > 16) {
    error_number = EINVAL;
    error_message = "UASP PoC supports CDBs up to 16 bytes";
    return false;
  }

  uint8_t command_iu[32] = {};
  command_iu[0] = 0x01;
  put_be16(command_iu + 2, 1); // QD1: UAS tag and stream ID are both 1.
  command_iu[4] = 0;           // SIMPLE task attribute.
  command_iu[6] = 0;           // No additional CDB bytes.
  // command_iu[8..15] is the flat-space representation of LUN 0.
  memcpy(command_iu + 16, iop->cmnd, iop->cmnd_len);

  const bool ok = handle->uas_streams_enabled
    ? uas_execute_with_streams(handle, iop, command_iu, sizeof(command_iu),
        error_number, error_message)
    : uas_execute_without_streams(handle, iop, command_iu, sizeof(command_iu),
        error_number, error_message);
  if (!ok)
    abort_uas_transport(handle);
  return ok;
}

static bool ata_read_command_is_allowed(const scsi_cmnd_io * iop)
{
  const bool passthrough16 = (iop->cmnd[0] == SAT_ATA_PASSTHROUGH_16);
  if ((passthrough16 && iop->cmnd_len < 16)
      || (!passthrough16 && iop->cmnd_len < 12))
    return false;

  const uint8_t feature = iop->cmnd[passthrough16 ? 4 : 3];
  const uint8_t command = iop->cmnd[passthrough16 ? 14 : 9];
  switch (command) {
    case 0xec: // IDENTIFY DEVICE
    case 0xa1: // IDENTIFY PACKET DEVICE
    case 0x2f: // READ LOG EXT
    case 0x47: // READ LOG DMA EXT
      return iop->dxfer_dir == DXFER_FROM_DEVICE;
    case 0xe5: // CHECK POWER MODE
      return iop->dxfer_dir == DXFER_NONE;
    case 0xb0: // SMART
      if (feature == 0xda) // RETURN STATUS
        return iop->dxfer_dir == DXFER_NONE;
      return (feature == 0xd0 || feature == 0xd1 || feature == 0xd5)
        && iop->dxfer_dir == DXFER_FROM_DEVICE;
    default:
      return false;
  }
}

static bool jmicron_nvme_read_command_is_allowed(const scsi_cmnd_io * iop)
{
  if (iop->cmnd_len < 12)
    return false;
  const uint8_t protocol = iop->cmnd[1];
  if (protocol == 0x80) { // Submit the 512-byte NVMe command envelope.
    if (iop->dxfer_dir != DXFER_TO_DEVICE || iop->dxfer_len != 512
        || !iop->dxferp)
      return false;
    const uint8_t * command = iop->dxferp;
    if (memcmp(command, "NVME", 4))
      return false;
    const uint8_t opcode = command[8];
    return opcode == 0x02 || opcode == 0x06; // Get Log Page or Identify
  }
  if (protocol == 0x82 || protocol == 0x8f)
    return iop->dxfer_dir == DXFER_FROM_DEVICE;
  return false;
}

static bool vendor_nvme_read_command_is_allowed(const scsi_cmnd_io * iop)
{
  if (iop->cmnd[0] == 0xe6) { // ASMedia
    return iop->cmnd_len >= 16 && iop->dxfer_dir == DXFER_FROM_DEVICE
      && (iop->cmnd[1] == 0x02 || iop->cmnd[1] == 0x06);
  }
  if (iop->cmnd[0] == 0xe4) { // Realtek
    return iop->cmnd_len >= 16 && iop->dxfer_dir == DXFER_FROM_DEVICE
      && (iop->cmnd[3] == 0x02 || iop->cmnd[3] == 0x06);
  }
  return false;
}

static bool read_only_scsi_command_is_allowed(const scsi_cmnd_io * iop)
{
  if (!iop || !iop->cmnd || !iop->cmnd_len)
    return false;

  switch (iop->cmnd[0]) {
    case 0x00: // TEST UNIT READY
      return iop->dxfer_dir == DXFER_NONE;
    case 0x03: // REQUEST SENSE
    case 0x12: // INQUIRY
    case 0x1a: // MODE SENSE(6)
    case 0x25: // READ CAPACITY(10)
    case 0x4d: // LOG SENSE
    case 0x5a: // MODE SENSE(10)
    case 0x9e: // SERVICE ACTION IN(16), including READ CAPACITY(16)
    case 0xa0: // REPORT LUNS
    case 0xa3: // MAINTENANCE IN, including REPORT SUPPORTED OPERATION CODES
      return iop->dxfer_dir == DXFER_FROM_DEVICE;
    case SAT_ATA_PASSTHROUGH_12:
      if (iop->cmnd_len >= 2 && (iop->cmnd[1] & 0x80))
        return jmicron_nvme_read_command_is_allowed(iop);
      return ata_read_command_is_allowed(iop);
    case SAT_ATA_PASSTHROUGH_16:
      return ata_read_command_is_allowed(iop);
    case 0xe4:
    case 0xe6:
      return vendor_nvme_read_command_is_allowed(iop);
    default:
      return false;
  }
}

darwin_usb_handle * darwin_usb_open(const char * selector, int & error_number,
  std::string & error_message)
{
  error_number = 0;
  error_message.clear();

  if (!validate_selector_syntax(selector, error_message)) {
    error_number = EINVAL;
    return 0;
  }

  if (geteuid() != 0) {
    error_number = EPERM;
    error_message = "IOUSBHost DeviceCapture requires root privileges";
    return 0;
  }

  @autoreleasepool {
    io_service_t service = resolve_selector(selector, error_message);
    if (!service) {
      error_number = ENODEV;
      return 0;
    }

    uint64_t registry_id = 0;
    IORegistryEntryGetRegistryEntryID(service, &registry_id);

    NSError * ns_error = nil;
    IOUSBHostDevice * device = [[IOUSBHostDevice alloc]
      initWithIOService:service
      options:IOUSBHostObjectInitOptionsDeviceCapture
      queue:nil
      error:&ns_error
      interestHandler:nil];
    IOObjectRelease(service);
    if (!device) {
      error_number = EBUSY;
      error_message = std::string("unable to capture USB device: ")
        + ns_error_string(ns_error);
      return 0;
    }

    darwin_usb_transport transport = darwin_usb_transport_none;
    uint8_t interface_number = 0;
    IOUSBHostInterface * interface = find_mass_storage_interface(device,
      transport, interface_number, error_message);
    if (!interface) {
      [device destroy];
      [device release];
      error_number = ENODEV;
      return 0;
    }

    darwin_usb_handle * handle = new darwin_usb_handle;
    handle->device = device;
    handle->interface = interface;
    handle->bulk_in = nil;
    handle->bulk_out = nil;
    handle->uas_command = nil;
    handle->uas_status = nil;
    handle->uas_data_in = nil;
    handle->uas_data_out = nil;
    handle->uas_status_stream = nil;
    handle->uas_data_in_stream = nil;
    handle->uas_data_out_stream = nil;
    handle->uas_streams_enabled = false;
    handle->transport_failed = false;
    handle->transport = transport;
    handle->registry_id = registry_id;
    handle->interface_number = interface_number;
    handle->next_tag = 0;
    if (transport == darwin_usb_transport_bot
        && !copy_bot_pipes(interface, handle->bulk_in, handle->bulk_out,
          error_message)) {
      darwin_usb_close(handle);
      error_number = ENODEV;
      return 0;
    }
    if (transport == darwin_usb_transport_uasp) {
      if (!copy_uas_pipes(interface, handle->uas_command, handle->uas_status,
          handle->uas_data_in, handle->uas_data_out, error_message)) {
        darwin_usb_close(handle);
        error_number = ENODEV;
        return 0;
      }
      try_enable_uas_streams(handle);
    }
    return handle;
  }
}

void darwin_usb_close(darwin_usb_handle * handle)
{
  if (!handle)
    return;
  @autoreleasepool {
    disable_uas_streams(handle);
    release_uas_pipes(handle->uas_command, handle->uas_status,
      handle->uas_data_in, handle->uas_data_out);
    if (handle->bulk_in)
      [handle->bulk_in release];
    if (handle->bulk_out)
      [handle->bulk_out release];
    if (handle->interface) {
      [handle->interface destroy];
      [handle->interface release];
    }
    if (handle->device) {
      // For DeviceCapture, normal destroy resets the device and re-registers
      // the macOS drivers for matching.
      [handle->device destroy];
      [handle->device release];
    }
  }
  delete handle;
}

const char * darwin_usb_transport_name(const darwin_usb_handle * handle)
{
  if (!handle)
    return "unknown";
  return (handle->transport == darwin_usb_transport_uasp ? "UASP" : "BOT");
}

bool darwin_usb_scsi_pass_through(darwin_usb_handle * handle,
  scsi_cmnd_io * iop, int & error_number, std::string & error_message)
{
  if (!handle || !iop) {
    error_number = EINVAL;
    error_message = "invalid raw USB SCSI request";
    return false;
  }

  iop->resp_sense_len = 0;
  iop->scsi_status = 0;
  iop->resid = 0;

  if (handle->transport_failed) {
    error_number = EIO;
    error_message = "raw USB transport is failed; close it to reset the device";
    return false;
  }

  if (!read_only_scsi_command_is_allowed(iop)) {
    error_number = EPERM;
    error_message = "raw USB PoC rejected a command outside its read-only allowlist";
    return false;
  }

  if (iop->dxfer_len > INT_MAX) {
    error_number = EINVAL;
    error_message = "raw USB transfer is too large";
    return false;
  }
  if (iop->dxfer_len && !iop->dxferp) {
    error_number = EINVAL;
    error_message = "raw USB transfer has a null data buffer";
    return false;
  }

  if (handle->transport == darwin_usb_transport_bot) {
    bot_result result = {};
    if (!bot_execute(handle, iop->cmnd, iop->cmnd_len, iop->dxfer_dir,
        iop->dxferp, iop->dxfer_len, iop->timeout, result, error_number,
        error_message))
      return false;

    iop->resid = (int)result.residue;
    if (result.status == 0)
      return true;

    iop->scsi_status = SCSI_STATUS_CHECK_CONDITION;
    if (!iop->sensep || !iop->max_sense_len)
      return true;

    const size_t sense_length = std::min(iop->max_sense_len, (size_t)0xff);
    uint8_t request_sense[6] = { REQUEST_SENSE, 0, 0, 0,
      (uint8_t)sense_length, 0 };
    bot_result sense_result = {};
    if (!bot_execute(handle, request_sense, sizeof(request_sense),
        DXFER_FROM_DEVICE, iop->sensep, sense_length, iop->timeout,
        sense_result, error_number, error_message))
      return false;
    if (sense_result.status == 0)
      iop->resp_sense_len = sense_length - sense_result.residue;
    return true;
  }
  return uas_execute(handle, iop, error_number, error_message);
}

} // namespace os_darwin
} // namespace smartmon
