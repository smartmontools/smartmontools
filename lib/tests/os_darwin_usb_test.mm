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

#define CHECK(condition) do { if (!(condition)) { \
  std::fprintf(stderr, "line %d: %s\n", __LINE__, #condition); return 1; \
} } while (0)

int main()
{
  using namespace smartmon;
  using namespace smartmon::os_darwin;
  @autoreleasepool {
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
