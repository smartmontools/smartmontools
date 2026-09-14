/*
 * Exercise the Darwin transport's protocol and completion handling without
 * capturing a USB device or issuing Disk Arbitration operations.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <chrono>
#include "../os_darwin_usb.mm"

@interface TestUSBPipe : NSObject {
@public
  IOUSBHostCompletionHandler completion;
  NSMutableData * pendingData;
  BOOL enqueueSucceeds;
  unsigned abortCalls;
}
- (void)completeWithStatus:(IOReturn)status count:(NSUInteger)count;
@end

@implementation TestUSBPipe
- (instancetype)init
{
  self = [super init];
  if (self)
    enqueueSucceeds = YES;
  return self;
}
- (BOOL)enqueueIORequestWithData:(NSMutableData *)data
             completionTimeout:(NSTimeInterval)timeout
                         error:(NSError **)error
             completionHandler:(IOUSBHostCompletionHandler)handler
{
  (void)timeout;
  (void)error;
  if (!enqueueSucceeds)
    return NO;
  completion = [handler copy];
  pendingData = [data retain];
  return YES;
}
- (BOOL)abortWithOption:(IOUSBHostAbortOption)option error:(NSError **)error
{
  (void)error;
  if (option != IOUSBHostAbortOptionAsynchronous)
    std::abort();
  ++abortCalls;
  // Deliberately fail cancellation; a later callback still owns the request.
  return NO;
}
- (void)completeWithStatus:(IOReturn)status count:(NSUInteger)count
{
  IOUSBHostCompletionHandler handler = [completion copy];
  [completion release];
  completion = nil;
  [pendingData release];
  pendingData = nil;
  handler(status, count);
  [handler release];
}
- (void)dealloc
{
  [completion release];
  [pendingData release];
  [super dealloc];
}
@end

struct TestBOTState {
  uint32_t tag = 0;
  uint32_t residue = 0;
  uint8_t status = 0;
  size_t dataCount = 0;
  bool dataSucceeds = true;
  unsigned resets = 0;
  unsigned commands = 0;
  unsigned dataCalls = 0;
  unsigned cswCalls = 0;
  unsigned cswFailures = 0;
  unsigned clearCalls = 0;
  bool clearSucceeds = true;
  IOReturn cswError = kUSBHostReturnPipeStalled;
  size_t cswLength = 13;
  bool wrongTag = false;
  bool resetSucceeds = true;
};

@interface TestBOTPipe : NSObject {
@public
  TestBOTState * state;
  bool input;
}
@end

@implementation TestBOTPipe
- (BOOL)sendIORequestWithData:(NSMutableData *)data
            bytesTransferred:(NSUInteger *)count
           completionTimeout:(NSTimeInterval)timeout
                       error:(NSError **)error
{
  (void)timeout;
  (void)error;
  using namespace smartmon::os_darwin;
  uint8_t * bytes = (uint8_t *)[data mutableBytes];
  if (!input && [data length] == 31) {
    ++state->commands;
    state->tag = get_le32(bytes + 4);
    *count = 31;
    return YES;
  }
  if (input && [data length] == 13) {
    if (++state->cswCalls <= state->cswFailures) {
      *count = 0;
      *error = [NSError errorWithDomain:@"test" code:state->cswError userInfo:nil];
      return NO;
    }
    memset(bytes, 0, 13);
    put_le32(bytes, 0x53425355);
    put_le32(bytes + 4, state->tag + (state->wrongTag ? 1 : 0));
    put_le32(bytes + 8, state->residue);
    bytes[12] = state->status;
    *count = state->cswLength;
    return YES;
  }
  ++state->dataCalls;
  if (input)
    memset(bytes, 0x5a, std::min(state->dataCount, (size_t)[data length]));
  *count = state->dataCount;
  return state->dataSucceeds;
}
- (BOOL)clearStallWithError:(NSError **)error
{
  ++state->clearCalls;
  if (!state->clearSucceeds && error)
    *error = [NSError errorWithDomain:@"test" code:kIOReturnError userInfo:nil];
  return state->clearSucceeds;
}
@end

@interface TestBOTDevice : NSObject {
@public
  TestBOTState * state;
}
@end

@implementation TestBOTDevice
- (BOOL)sendDeviceRequest:(IOUSBDeviceRequest)request
                    data:(NSMutableData *)data
        bytesTransferred:(NSUInteger *)count
       completionTimeout:(NSTimeInterval)timeout
                   error:(NSError **)error
{
  (void)request;
  (void)data;
  (void)count;
  (void)timeout;
  (void)error;
  ++state->resets;
  if (!state->resetSucceeds && error)
    *error = [NSError errorWithDomain:@"test" code:kIOReturnError userInfo:nil];
  return state->resetSucceeds;
}
@end

@interface TestUSBInterface : NSObject {
@public
  std::vector<uint8_t> descriptors;
  std::vector<uint8_t> addresses;
  std::vector<NSUInteger> selectedAlternates;
  bool failSelect;
  size_t interfaceOffset;
}
@end
@implementation TestUSBInterface
- (const IOUSBConfigurationDescriptor *)configurationDescriptor
{ return (const IOUSBConfigurationDescriptor *)descriptors.data(); }
- (const IOUSBInterfaceDescriptor *)interfaceDescriptor
{ return (const IOUSBInterfaceDescriptor *)(descriptors.data() + interfaceOffset); }
- (BOOL)selectAlternateSetting:(NSUInteger)value error:(NSError **)error
{
  selectedAlternates.push_back(value);
  if (failSelect) {
    *error = [NSError errorWithDomain:@"test" code:kIOReturnError userInfo:nil];
    return NO;
  }
  const auto * config = [self configurationDescriptor];
  const IOUSBDescriptorHeader * descriptor = nullptr;
  while ((descriptor = IOUSBGetNextDescriptor(config, descriptor))) {
    if (descriptor->bDescriptorType == kUSBInterfaceDesc
        && descriptor->bLength >= sizeof(IOUSBInterfaceDescriptor)
        && ((const IOUSBInterfaceDescriptor *)descriptor)->bAlternateSetting == value) {
      interfaceOffset = (const uint8_t *)descriptor - descriptors.data();
      return YES;
    }
  }
  return NO;
}
- (IOUSBHostPipe *)copyPipeWithAddress:(NSUInteger)address error:(NSError **)error
{
  (void)error;
  addresses.push_back(address);
  return (IOUSBHostPipe *)[[NSObject alloc] init];
}
@end

#define CHECK(condition) do { if (!(condition)) { \
  std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition); return 1; \
} } while (0)

int main()
{
  using namespace smartmon;
  using namespace smartmon::os_darwin;
  @autoreleasepool {
    // Exercise restoration with independent Disk Arbitration observations.
    // Existing auto-mounts must never trigger another mount, and callback
    // status alone is insufficient evidence that the original path returned.
    darwin_mounted_volume restoreVolume;
    restoreVolume.volume_uuid = "test-volume";
    restoreVolume.mount_path = "/Volumes/original";
    struct RestoreCase {
      bool appeared;
      const char * beforePath;
      const char * afterPath; // nullptr means the volume disappeared.
      const char * mountError;
      bool expected;
      unsigned mounts;
    };
    const RestoreCase restoreCases[] = {
      { true, "/Volumes/original", nullptr, "", true, 0 },
      { true, "/Volumes/other", nullptr, "", false, 0 },
      { false, "", nullptr, "", false, 0 },
      { true, "", "/Volumes/original", "", true, 1 },
      { true, "", "/Volumes/original", "volume remount failed (0xf8da0002)", true, 1 },
      { true, "", "", "volume remount failed (0xf8da0003)", false, 1 },
      { true, "", "", "", false, 1 },
      { true, "", "/Volumes/other", "", false, 1 },
      { true, "", nullptr, "volume remount failed (0xf8da0006)", false, 1 },
    };
    for (const auto & test : restoreCases) {
      unsigned observations = 0, mounts = 0;
      std::string restoreError;
      bool restored = restore_volume(restoreVolume,
        [&](std::string & path) -> DADiskRef {
          const char * observed = observations++ ? test.afterPath : test.beforePath;
          path = observed ? observed : "";
          return test.appeared && observed
            ? (DADiskRef)CFRetain(CFSTR("mock disk")) : nullptr;
        },
        [&](DADiskRef, std::string & error) {
          ++mounts;
          error = test.mountError;
          return error.empty();
        }, restoreError);
      CHECK(restored == test.expected && mounts == test.mounts);
      CHECK(observations == 1 + test.mounts);
      CHECK(restoreError.empty() == test.expected);
      if (!test.expected && *test.mountError)
        CHECK(restoreError.find(test.mountError) != std::string::npos);
      if (!test.expected && test.appeared)
        CHECK(restoreError.find(restoreVolume.mount_path) != std::string::npos);
    }

    // Volume UUID is authoritative even when a cloned media UUID matches.
    CFUUIDRef volumeUUID = CFUUIDCreate(kCFAllocatorDefault);
    CFUUIDRef mediaUUID = CFUUIDCreate(kCFAllocatorDefault);
    CFMutableDictionaryRef volumeDescription = CFDictionaryCreateMutable(
      kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
      &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(volumeDescription, kDADiskDescriptionVolumeUUIDKey, volumeUUID);
    CFDictionarySetValue(volumeDescription, kDADiskDescriptionMediaUUIDKey, mediaUUID);
    restoreVolume.volume_uuid = description_uuid(volumeDescription, kDADiskDescriptionVolumeUUIDKey);
    restoreVolume.media_uuid = description_uuid(volumeDescription, kDADiskDescriptionMediaUUIDKey);
    CHECK(volume_matches(volumeDescription, restoreVolume));
    CFDictionarySetValue(volumeDescription, kDADiskDescriptionVolumeUUIDKey, mediaUUID);
    CHECK(!volume_matches(volumeDescription, restoreVolume));
    restoreVolume.volume_uuid.clear();
    CHECK(volume_matches(volumeDescription, restoreVolume));
    CFRelease(volumeDescription);
    CFRelease(volumeUUID);
    CFRelease(mediaUUID);

    // Actual JMS583 SuperSpeed descriptors: pipe usages follow companions.
    TestUSBInterface * descriptorInterface = [[TestUSBInterface alloc] init];
    descriptorInterface->descriptors = {
      9,2,121,0,1,1,0,0x80,0x70,
      9,4,0,0,2,8,6,0x50,0,
      7,5,0x81,2,0,4,0, 6,0x30,15,0,0,0,
      7,5,2,2,0,4,0, 6,0x30,15,0,0,0,
      9,4,0,1,4,8,6,0x62,10,
      7,5,1,2,0,4,0, 6,0x30,0,0,0,0, 4,0x24,1,0,
      7,5,0x82,2,0,4,0, 6,0x30,0,5,0,0, 4,0x24,2,0,
      7,5,0x83,2,0,4,0, 6,0x30,15,5,0,0, 4,0x24,3,0,
      7,5,4,2,0,4,0, 6,0x30,15,5,0,0, 4,0x24,4,0
    };
    descriptorInterface->interfaceOffset = 9; // Capture has reverted to BOT.
    darwin_usb_transport selected = darwin_usb_transport_none;
    std::string selectionError;
    CHECK(select_protocol((IOUSBHostInterface *)descriptorInterface, 0,
      darwin_usb_protocol::uasp, selected, selectionError));
    CHECK(selected == darwin_usb_transport_uasp && descriptorInterface->interfaceOffset == 44);
    CHECK(descriptorInterface->selectedAlternates == std::vector<NSUInteger>{1});
    CHECK(select_protocol((IOUSBHostInterface *)descriptorInterface, 0,
      darwin_usb_protocol::uasp, selected, selectionError));
    // A fresh capture can retain the previous alternate without a usable
    // transport. It still needs SET_INTERFACE before pipes are opened.
    CHECK(descriptorInterface->selectedAlternates == (std::vector<NSUInteger>{1, 1}));
    descriptorInterface->failSelect = true;
    CHECK(!select_protocol((IOUSBHostInterface *)descriptorInterface, 0,
      darwin_usb_protocol::bot, selected, selectionError));
    CHECK(descriptorInterface->selectedAlternates == (std::vector<NSUInteger>{1, 1, 0}));
    descriptorInterface->failSelect = false;
    CHECK(!select_protocol((IOUSBHostInterface *)descriptorInterface, 1,
      darwin_usb_protocol::bot, selected, selectionError)); // Never another interface.
    CHECK(descriptorInterface->selectedAlternates.size() == 3);
    descriptorInterface->interfaceOffset = 44;
    IOUSBHostPipe * commandPipe, * statusPipe, * dataInPipe, * dataOutPipe;
    std::string descriptorError;
    auto copyTestPipes = [&]() {
      return copy_uas_pipes((IOUSBHostInterface *)descriptorInterface,
        commandPipe,statusPipe,dataInPipe,dataOutPipe,descriptorError);
    };
    CHECK(copyTestPipes());
    CHECK(descriptorInterface->addresses == (std::vector<uint8_t>{1,0x82,0x83,4}));
    release_uas_pipes(commandPipe,statusPipe,dataInPipe,dataOutPipe);
    descriptorInterface->interfaceOffset = 9;
    descriptorInterface->addresses.clear();
    CHECK(!copyTestPipes()); // Must not borrow usages from the next alternate.
    CHECK(descriptorInterface->addresses.empty());
    descriptorInterface->interfaceOffset = 44;
    descriptorInterface->descriptors[119] = 3; // Duplicate data-in usage.
    descriptorInterface->descriptors[106] = 0x84;
    CHECK(!copyTestPipes() && !commandPipe && !statusPipe && !dataInPipe && !dataOutPipe);
    descriptorInterface->descriptors[119] = 4;
    CHECK(!copyTestPipes()); // Data-out must not name an input endpoint.
    descriptorInterface->descriptors[106] = 4;
    descriptorInterface->descriptors[104] = 2; // Truncated endpoint.
    CHECK(!copyTestPipes() && descriptorError == "UASP endpoint descriptor is truncated");

    // ASM2362 lists data pipes before status and command, and reuses BOT
    // endpoint addresses. Resolve roles by pipe usage, not descriptor order.
    descriptorInterface->descriptors = {
      9,2,121,0,1,1,0,0xc0,0,
      9,4,0,0,2,8,6,0x50,0,
      7,5,0x81,2,0,4,0, 6,0x30,15,0,0,0,
      7,5,2,2,0,4,0, 6,0x30,15,0,0,0,
      9,4,0,1,4,8,6,0x62,0,
      7,5,0x81,2,0,4,0, 6,0x30,15,5,0,0, 4,0x24,3,0,
      7,5,2,2,0,4,0, 6,0x30,15,5,0,0, 4,0x24,4,0,
      7,5,0x83,2,0,4,0, 6,0x30,15,5,0,0, 4,0x24,2,0,
      7,5,4,2,0,4,0, 6,0x30,0,0,0,0, 4,0x24,1,0
    };
    descriptorInterface->selectedAlternates.clear();
    descriptorInterface->addresses.clear();
    CHECK(select_protocol((IOUSBHostInterface *)descriptorInterface, 0,
      darwin_usb_protocol::uasp, selected, selectionError));
    CHECK(descriptorInterface->selectedAlternates == std::vector<NSUInteger>{1});
    CHECK(copyTestPipes());
    CHECK(descriptorInterface->addresses == (std::vector<uint8_t>{0x81,2,0x83,4}));
    release_uas_pipes(commandPipe,statusPipe,dataInPipe,dataOutPipe);
    CHECK(select_protocol((IOUSBHostInterface *)descriptorInterface, 0,
      darwin_usb_protocol::bot, selected, selectionError));
    CHECK(select_protocol((IOUSBHostInterface *)descriptorInterface, 0,
      darwin_usb_protocol::bot, selected, selectionError));
    CHECK(descriptorInterface->selectedAlternates == (std::vector<NSUInteger>{1,0,0}));
    CHECK(selected == darwin_usb_transport_bot);
    [descriptorInterface release];

    uint8_t cdb[16] = {};
    scsi_cmnd_io io = {};
    io.cmnd = cdb;
    io.cmnd_len = 10;
    io.dxfer_dir = DXFER_FROM_DEVICE;
    cdb[0] = 0x4d;
    CHECK(read_only_scsi_command_is_allowed(&io));
    for (unsigned flags = 1; flags < 256; ++flags) {
      cdb[1] = flags;
      CHECK(!read_only_scsi_command_is_allowed(&io));
    }
    cdb[0] = SAT_ATA_PASSTHROUGH_16;
    cdb[1] = 0;
    cdb[14] = 0xec; // IDENTIFY
    io.cmnd_len = 16;
    CHECK(read_only_scsi_command_is_allowed(&io));
    cdb[14] = 0xb0;
    cdb[4] = 0xd8; // SMART ENABLE
    CHECK(!read_only_scsi_command_is_allowed(&io));
    io.cmnd_len = 2;
    CHECK(!read_only_scsi_command_is_allowed(&io));

    uint8_t startCdb[6] = {0x1b, 0, 0, 0, 1, 0};
    scsi_cmnd_io startIo = {};
    startIo.cmnd = startCdb;
    startIo.cmnd_len = 6;
    startIo.dxfer_dir = DXFER_NONE;
    CHECK(read_only_scsi_command_is_allowed(&startIo));
    for (unsigned byte = 1; byte < 6; ++byte) {
      const uint8_t original = startCdb[byte];
      for (unsigned value = 0; value < 256; ++value) {
        if (value == original)
          continue;
        startCdb[byte] = value;
        CHECK(!read_only_scsi_command_is_allowed(&startIo));
      }
      startCdb[byte] = original;
    }
    startIo.cmnd_len = 5;
    CHECK(!read_only_scsi_command_is_allowed(&startIo));
    startIo.cmnd_len = 6;
    startIo.dxfer_len = 1;
    CHECK(!read_only_scsi_command_is_allowed(&startIo));
    startIo.dxfer_len = 0;
    startIo.dxfer_dir = DXFER_FROM_DEVICE;
    CHECK(!read_only_scsi_command_is_allowed(&startIo));

    darwin_usb_device_info oldIdentity = {};
    oldIdentity.registry_id = 100;
    oldIdentity.location_id = 123;
    oldIdentity.vendor_id = 0x152d;
    oldIdentity.product_id = 0x0583;
    oldIdentity.device_version = 0x0209;
    oldIdentity.serial_number = "bridge-serial";
    auto newIdentity = oldIdentity;
    newIdentity.registry_id = 200;
    CHECK(same_usb_identity(oldIdentity, newIdentity));
    newIdentity.serial_number = "other";
    CHECK(!same_usb_identity(oldIdentity, newIdentity));
    newIdentity = oldIdentity;
    newIdentity.location_id = 124;
    CHECK(!same_usb_identity(oldIdentity, newIdentity));
    newIdentity = oldIdentity;
    newIdentity.product_id++;
    CHECK(!same_usb_identity(oldIdentity, newIdentity));

    TestBOTState bot;
    TestBOTPipe * botIn = [[TestBOTPipe alloc] init];
    TestBOTPipe * botOut = [[TestBOTPipe alloc] init];
    TestBOTDevice * botDevice = [[TestBOTDevice alloc] init];
    botIn->state = botOut->state = botDevice->state = &bot;
    botIn->input = true;
    botOut->input = false;
    darwin_usb_handle botHandle = {};
    botHandle.bulk_in = (IOUSBHostPipe *)botIn;
    botHandle.bulk_out = (IOUSBHostPipe *)botOut;
    botHandle.device = (IOUSBHostDevice *)botDevice;
    uint8_t botCdb[12] = { 0xa1, 0x82, 0, 0, 0x10, 0 };
    std::vector<uint8_t> botData(4096);
    bot_result botResult = {};
    int botError = 0;
    std::string botMessage;
    auto executeBot = [&](int direction = DXFER_FROM_DEVICE) {
      botError = 0;
      botMessage.clear();
      return bot_execute(&botHandle, botCdb, sizeof(botCdb), direction,
        botData.data(), botData.size(), 1, botResult, botError, botMessage);
    };

    // Captured JMS583 Identify response: a full payload and a full residue.
    bot.dataCount = bot.residue = 4096;
    CHECK(executeBot() && botResult.residue == 4096); // No blanket quirk.
    botHandle.jms583 = true;
    CHECK(executeBot() && botResult.residue == 0 && bot.resets == 0);
    CHECK(botData[0] == 0x5a && botData.back() == 0x5a);
    botCdb[1] = 0x8f;
    botData.resize(512);
    botCdb[4] = 2;
    bot.dataCount = bot.residue = 512;
    CHECK(executeBot() && botResult.residue == 512);

    // Neither unrelated CDBs nor malformed envelopes get the correction.
    botCdb[1] = 0x80;
    CHECK(executeBot() && botResult.residue == 512);
    botCdb[1] = 0x82;
    botCdb[0] = INQUIRY;
    CHECK(executeBot() && botResult.residue == 512);
    botCdb[0] = 0xa1;
    botCdb[4] = 1;
    CHECK(executeBot() && botResult.residue == 512);
    botCdb[4] = 2;
    CHECK(executeBot(DXFER_TO_DEVICE) && botResult.residue == 512);

    // Padding/discarded data is legal; residue need not equal wire shortfall.
    bot.residue = 128;
    CHECK(executeBot() && botResult.residue == 128);
    CHECK(executeBot(DXFER_TO_DEVICE) && botResult.residue == 128);
    // Short or failed data phases and failed commands never become full reads.
    bot.dataCount = 256;
    bot.residue = 512;
    CHECK(executeBot() && botResult.residue == 512);
    bot.dataCount = 512;
    bot.status = 1;
    CHECK(executeBot() && botResult.status == 1 && botResult.residue == 512);
    bot.status = 0;
    bot.dataSucceeds = false;
    CHECK(!executeBot() && bot.resets == 1);
    bot.dataSucceeds = true;
    // A device cannot claim more relevant bytes than were actually received.
    bot.dataCount = 256;
    bot.residue = 0;
    CHECK(!executeBot() && botError == EIO && bot.resets == 2);
    bot.residue = 513;
    CHECK(!executeBot() && bot.resets == 3);
    // A phase error always requires reset, even with a meaningless residue.
    bot.status = 2;
    CHECK(!executeBot() && botMessage == "BOT phase error" && bot.resets == 4);

    // A short data packet can be followed by a status STALL. Clear it and
    // read the CSW once more without replaying CBW or data (BOT 5.3.3).
    bot = TestBOTState();
    bot.dataCount = bot.residue = 256;
    bot.cswFailures = 1;
    CHECK(executeBot() && botResult.residue == 256);
    CHECK(bot.commands == 1 && bot.dataCalls == 1 && bot.cswCalls == 2);
    CHECK(bot.clearCalls == 1 && bot.resets == 0);
    // A second STALL, failed clear-halt, or invalid CSW still requires reset.
    bot = TestBOTState();
    bot.dataCount = 512;
    bot.cswFailures = 2;
    CHECK(!executeBot() && botError == EIO && bot.resets == 1);
    CHECK(bot.commands == 1 && bot.dataCalls == 1 && bot.cswCalls == 2);
    bot = TestBOTState();
    bot.dataCount = 512;
    bot.cswFailures = 1;
    bot.clearSucceeds = false;
    CHECK(!executeBot() && bot.resets == 1 && bot.cswCalls == 1);
    CHECK(botMessage.find("unable to clear status STALL") != std::string::npos);
    for (bool wrongTag : { false, true }) {
      bot = TestBOTState();
      bot.dataCount = 512;
      bot.cswFailures = 1;
      bot.wrongTag = wrongTag;
      bot.cswLength = (wrongTag ? 13 : 12);
      CHECK(!executeBot() && bot.resets == 1 && bot.cswCalls == 2);
    }
    // Other transport errors must not be mistaken for a STALL.
    bot = TestBOTState();
    bot.dataCount = 512;
    bot.cswFailures = 1;
    bot.cswError = kIOReturnNotResponding;
    CHECK(!executeBot() && bot.resets == 1 && bot.cswCalls == 1);
    bot = TestBOTState();
    CHECK(bot_reset_recovery(&botHandle, &botMessage));
    CHECK(bot.resets == 1 && bot.clearCalls == 2 && bot.commands == 0);
    bot.resetSucceeds = false;
    CHECK(!bot_reset_recovery(&botHandle, &botMessage));
    CHECK(bot.resets == 2 && bot.clearCalls == 4);
    bot.resetSucceeds = true;
    bot.clearSucceeds = false;
    CHECK(!bot_reset_recovery(&botHandle, &botMessage));
    CHECK(botMessage.find("BOT reset recovery failed") != std::string::npos);
    [botIn release];
    [botOut release];
    [botDevice release];

    uint8_t status[20] = { 3, 0, 0, 1 };
    uint8_t sense[2] = {};
    io.sensep = sense;
    io.max_sense_len = sizeof(sense);
    status[6] = SCSI_STATUS_CHECK_CONDITION;
    status[15] = 4;
    status[16] = 0x72;
    status[17] = 5;
    int error = 0;
    std::string message;
    CHECK(parse_uas_status(status, sizeof(status), &io, error, message));
    CHECK(io.resp_sense_len == 2 && sense[0] == 0x72 && sense[1] == 5);
    CHECK(!parse_uas_status(status, 19, &io, error, message));
    status[3] = 2;
    CHECK(!parse_uas_status(status, sizeof(status), &io, error, message));

    TestUSBPipe * pipe = [[TestUSBPipe alloc] init];
    uint8_t buffer[4] = {};
    size_t transferred = 0;
    uas_transfer_ptr request = enqueue_uas_pipe_transfer((IOUSBHostPipe *)pipe,
      buffer, sizeof(buffer), true, 1, message);
    CHECK(request);
    memset([pipe->pendingData mutableBytes], 0x5a, sizeof(buffer));
    [pipe completeWithStatus:kIOReturnSuccess count:sizeof(buffer)];
    CHECK(finish_uas_async_transfer(request, buffer, sizeof(buffer), true, 1,
      transferred, message));
    CHECK(transferred == sizeof(buffer) && buffer[0] == 0x5a);
    request.reset();

    request = enqueue_uas_pipe_transfer((IOUSBHostPipe *)pipe,
      buffer, sizeof(buffer), true, 1, message);
    std::weak_ptr<uas_async_transfer> pending = request;
    const auto started = std::chrono::steady_clock::now();
    CHECK(!finish_uas_async_transfer(request, buffer, sizeof(buffer), true, 1,
      transferred, message));
    CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(3));
    CHECK(message.find("cancellation did not complete") != std::string::npos);
    request.reset();
    CHECK(!pending.expired() && pipe->abortCalls == 1);
    [pipe completeWithStatus:kIOReturnAborted count:0];
    CHECK(pending.expired());

    pipe->enqueueSucceeds = NO;
    CHECK(!enqueue_uas_pipe_transfer((IOUSBHostPipe *)pipe, buffer,
      sizeof(buffer), true, 1, message));
    [pipe release];

    da_operation_ptr operation = std::make_shared<da_operation>(disk_session_ptr());
    std::weak_ptr<da_operation> late = operation;
    da_operation_ptr * callback_context = new da_operation_ptr(operation);
    operation.reset(); // Simulate a timed-out waiter returning to its caller.
    CHECK(!late.expired());
    da_operation_callback(nullptr, nullptr, callback_context);
    CHECK(late.expired());
  }
  return 0;
}
