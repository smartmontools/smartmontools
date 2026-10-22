/*
 * test_scsi_trace.cpp - unit tests for the SCSI trace replay device
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2026 smartmontools contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <smartmon/atacmds.h>
#include <smartmon/dev_interface.h>
#include <smartmon/knowndrives.h>
#include <smartmon/scsicmds.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

using namespace smartmon;

static int g_checks = 0, g_failed = 0;

#define CHECK(cond) do { ++g_checks; if (!(cond)) { ++g_failed; \
    std::printf("%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define CHECK_EQ(a, b) do { ++g_checks; long long va_ = (long long)(a), vb_ = (long long)(b); \
    if (va_ != vb_) { ++g_failed; std::printf("%s:%d: CHECK_EQ failed: %s == %s (%lld != %lld)\n", \
      __FILE__, __LINE__, #a, #b, va_, vb_); } } while (0)

static const char mini_trace[] =
  "# minimal trace\n"
  "CDB 12 00 00 00 24 00\n"
  "ALLOC 36\n"
  "STATUS 0\n"
  "SENSE\n"
  "DATA 00 00 06 12 5b 00 00 02 53 61 6e 44 69 73 6b 20 45 78 74 72 65 6d 65 20"
  " 35 35 44 44 20 20 20 20 30 30 31 30\n"
  "END\n"
  "\n"
  "# LOG SENSE page 0x32\n"
  "CDB 4d 00 72 00 00 00 00 00 fc 00\n"
  "ALLOC 252\n"
  "STATUS 0\n"
  "SENSE\n"
  "DATA b2 00 00 05 01 ff 11 24 21\n"
  "END\n"
  "\n"
  "# LOG SENSE page 0x02 rejected by device\n"
  "CDB 4d 00 42 00 00 00 00 00 fc 00\n"
  "ALLOC 252\n"
  "STATUS 2\n"
  "SENSE 70 00 05 00 00 00 00 0a 00 00 00 00 24 00 00 00 00 00\n"
  "DATA\n"
  "END\n"
  "\n"
  "# MODE SENSE(6) page 0x1c: pass-through fails\n"
  "CDB 1a 00 1c 00 40 00\n"
  "ALLOC 64\n"
  "STATUS -1 ERROR 1117\n"
  "SENSE\n"
  "DATA\n"
  "END\n";

typedef std::unique_ptr<scsi_device> scsi_ptr;

static scsi_ptr open_trace(const char * text)
{
  scsi_ptr dev(get_trace_scsi_device(smi(), "-", text));
  if (dev && !dev->open())
    std::printf("open(): %s\n", dev->get_errmsg());
  return dev;
}

// Issue a raw command, return pass-through result
static bool raw_cmd(scsi_device * dev, const uint8_t * cdb, unsigned cdb_len,
                    int dir, uint8_t * buf, unsigned len, scsi_sense_disect & sinfo,
                    scsi_cmnd_io & io)
{
  static uint8_t sense[32];
  memset(&io, 0, sizeof(io));
  memset(sense, 0, sizeof(sense));
  io.cmnd = const_cast<uint8_t *>(cdb);
  io.cmnd_len = cdb_len;
  io.dxfer_dir = dir;
  io.dxferp = buf;
  io.dxfer_len = len;
  io.sensep = sense;
  io.max_sense_len = sizeof(sense);
  if (!dev->scsi_pass_through(&io))
    return false;
  scsi_do_sense_disect(&io, &sinfo);
  return true;
}

static void test_open()
{
  scsi_ptr dev = open_trace(mini_trace);
  CHECK(dev);
  CHECK(dev->is_open());
  CHECK(dev->is_scsi());
}

static void test_inquiry_any_allocation_length()
{
  scsi_ptr dev = open_trace(mini_trace);
  uint8_t buf[96];
  memset(buf, 0xee, sizeof(buf));
  CHECK_EQ(scsiStdInquiry(dev.get(), buf, 36), 0);
  CHECK(!memcmp(buf + 8, "SanDisk Extreme 55DD    0010", 28));

  // Longer request than recorded: recorded bytes, rest reported as residual
  scsi_sense_disect sinfo;
  scsi_cmnd_io io;
  const uint8_t cdb[6] = {0x12, 0, 0, 0, 96, 0};
  memset(buf, 0, sizeof(buf));
  CHECK(raw_cmd(dev.get(), cdb, sizeof(cdb), DXFER_FROM_DEVICE, buf, 96, sinfo, io));
  CHECK_EQ(io.scsi_status, 0);
  CHECK_EQ(io.resid, 96 - 36);
  CHECK(!memcmp(buf + 16, "Extreme 55DD", 12));

  // Shorter request: truncated
  const uint8_t cdb5[6] = {0x12, 0, 0, 0, 5, 0};
  memset(buf, 0, sizeof(buf));
  CHECK(raw_cmd(dev.get(), cdb5, sizeof(cdb5), DXFER_FROM_DEVICE, buf, 5, sinfo, io));
  CHECK_EQ(io.resid, 0);
  CHECK_EQ(buf[4], 0x5b);
  CHECK_EQ(buf[5], 0);
}

static void test_log_sense_twin_fetch()
{
  scsi_ptr dev = open_trace(mini_trace);
  uint8_t buf[252];
  memset(buf, 0, sizeof(buf));
  // known_resp_len == 0: header fetch (4 bytes), then page_len + 4
  CHECK_EQ(scsiLogSense(dev.get(), 0x32, 0, buf, sizeof(buf), 0), 0);
  CHECK_EQ(buf[0], 0xb2);
  CHECK_EQ(buf[3], 0x05);
  CHECK_EQ(buf[7], 0x24);
  CHECK_EQ(buf[8], 0x21);
}

static void test_recorded_check_condition()
{
  scsi_ptr dev = open_trace(mini_trace);
  uint8_t buf[252];
  CHECK_EQ(scsiLogSense(dev.get(), 0x02, 0, buf, sizeof(buf), 0), SIMPLE_ERR_BAD_FIELD);
}

static void test_unrecorded_page_of_known_opcode()
{
  scsi_ptr dev = open_trace(mini_trace);
  uint8_t buf[252];
  // LOG SENSE is known, page 0x0d was never recorded -> INVALID FIELD IN CDB
  CHECK_EQ(scsiLogSense(dev.get(), 0x0d, 0, buf, sizeof(buf), 0), SIMPLE_ERR_BAD_FIELD);
}

static void test_unknown_opcode()
{
  scsi_ptr dev = open_trace(mini_trace);
  unsigned last_lba = 0, lb_size = 0;
  // READ CAPACITY(10) never recorded -> INVALID COMMAND OPERATION CODE
  CHECK_EQ(scsiReadCapacity10(dev.get(), &last_lba, &lb_size), SIMPLE_ERR_BAD_OPCODE);
}

static void test_transport_failure()
{
  scsi_ptr dev = open_trace(mini_trace);
  uint8_t buf[64];
  scsi_sense_disect sinfo;
  scsi_cmnd_io io;
  const uint8_t cdb[6] = {0x1a, 0, 0x1c, 0, 64, 0};
  CHECK(!raw_cmd(dev.get(), cdb, sizeof(cdb), DXFER_FROM_DEVICE, buf, 64, sinfo, io));
  CHECK_EQ(dev->get_errno(), EIO);
}

static void test_data_out_is_never_accepted()
{
  scsi_ptr dev = open_trace(mini_trace);
  uint8_t buf[36] = {0, };
  scsi_sense_disect sinfo;
  scsi_cmnd_io io;
  // Same CDB as a recorded data-in command, but data-out direction
  const uint8_t cdb[6] = {0x12, 0, 0, 0, 36, 0};
  CHECK(raw_cmd(dev.get(), cdb, sizeof(cdb), DXFER_TO_DEVICE, buf, 36, sinfo, io));
  CHECK_EQ(io.scsi_status, SCSI_STATUS_CHECK_CONDITION);
  CHECK_EQ(sinfo.sense_key, SCSI_SK_ILLEGAL_REQUEST);
}

// A recorded data-out command (e.g. SMART WRITE LOG of an SCT command through
// ATA PASS-THROUGH) is replayed only if CDB and data match exactly
static void test_recorded_data_out()
{
  static const char trace[] =
    "# ATA PASS-THROUGH(16) SMART WRITE LOG 0xe0, 1 sector (shortened to 4 bytes)\n"
    "CDB 85 0b 06 00 d6 00 01 00 e0 00 4f 00 c2 00 b0 00\n"
    "ALLOC 4\n"
    "STATUS 0\n"
    "SENSE\n"
    "DATAOUT 05 00 01 00\n"
    "DATA\n"
    "END\n";
  scsi_ptr dev = open_trace(trace);
  CHECK(dev->is_open());
  scsi_sense_disect sinfo;
  scsi_cmnd_io io;
  const uint8_t cdb[16] = {0x85, 0x0b, 0x06, 0, 0xd6, 0, 1, 0, 0xe0, 0, 0x4f, 0, 0xc2, 0, 0xb0, 0};

  uint8_t same[4] = {0x05, 0x00, 0x01, 0x00};
  CHECK(raw_cmd(dev.get(), cdb, sizeof(cdb), DXFER_TO_DEVICE, same, sizeof(same), sinfo, io));
  CHECK_EQ(io.scsi_status, 0);

  uint8_t other[4] = {0x05, 0x00, 0x02, 0x00};
  CHECK(raw_cmd(dev.get(), cdb, sizeof(cdb), DXFER_TO_DEVICE, other, sizeof(other), sinfo, io));
  CHECK_EQ(io.scsi_status, SCSI_STATUS_CHECK_CONDITION);
  CHECK_EQ(sinfo.sense_key, SCSI_SK_ILLEGAL_REQUEST);

  // The data-out record never answers a data-in request
  uint8_t in[4] = {0xee, 0xee, 0xee, 0xee};
  CHECK(raw_cmd(dev.get(), cdb, sizeof(cdb), DXFER_FROM_DEVICE, in, sizeof(in), sinfo, io));
  CHECK_EQ(io.scsi_status, SCSI_STATUS_CHECK_CONDITION);
}

// Identical commands are answered in recorded order, the last record is
// repeated (e.g. SCT status before and after an SCT command)
static void test_identical_commands_in_recorded_order()
{
  static const char trace[] =
    "CDB 12 01 80 00 08 00\nALLOC 8\nSTATUS 0\nSENSE\nDATA 00 80 00 04 41 41 41 41\nEND\n"
    "CDB 12 01 80 00 08 00\nALLOC 8\nSTATUS 0\nSENSE\nDATA 00 80 00 04 42 42 42 42\nEND\n";
  scsi_ptr dev = open_trace(trace);
  scsi_sense_disect sinfo;
  scsi_cmnd_io io;
  const uint8_t cdb[6] = {0x12, 0x01, 0x80, 0, 8, 0};
  const char expect[] = "ABB";
  for (int k = 0; k < 3; k++) {
    uint8_t buf[8] = {0, };
    CHECK(raw_cmd(dev.get(), cdb, sizeof(cdb), DXFER_FROM_DEVICE, buf, sizeof(buf), sinfo, io));
    CHECK_EQ(buf[4], expect[k]);
  }
}

static void test_parse_errors()
{
  static const char * const bad[] = {
    "CDB 12 00 00 00 24 00\nALLOC 36\nSTATUS 0\nSENSE\nDATA 00\n", // missing END
    "CDB 12 00 zz 00 24 00\nALLOC 36\nSTATUS 0\nSENSE\nDATA\nEND\n", // bad hex
    "BOGUS line\n",
    "", // empty trace
    "CDB 12 00 00 00 24 00\nSTATUS 0\nSENSE\nDATA\nEND\n", // missing ALLOC
    "CDB 12 00 00 00 24 00\nALLOC 36\nSENSE\nDATA\nEND\n", // missing STATUS
    "CDB 12 00 00 00 24 00\nALLOC -1\nSTATUS 0\nSENSE\nDATA\nEND\n", // negative ALLOC
    "CDB 12 00 00 00 24 00\nALLOC 36x\nSTATUS 0\nSENSE\nDATA\nEND\n", // trailing input
    "CDB 12 00 00 00 24 00\nALLOC 36\nSTATUS 256\nSENSE\nDATA\nEND\n", // status > 255
    "CDB 12 00 00 00 24 00\nALLOC 36\nSTATUS -1 ERROR\nSENSE\nDATA\nEND\n", // ERROR number missing
    "CDB 12 00 00 00 24 00\nALLOC 36\nSTATUS 0 ERROR 5\nSENSE\nDATA\nEND\n", // ERROR without -1
  };
  for (const char * text : bad) {
    scsi_ptr dev(get_trace_scsi_device(smi(), "-", text));
    CHECK(dev);
    CHECK(!dev->open());
    CHECK(!dev->is_open());
    CHECK_EQ(dev->get_errno(), EINVAL);
  }
}

static void test_sandisk_55dd_trace(const std::string & datadir)
{
  std::string path = datadir + "/sandisk-extreme-55dd-1tb.trace";
  scsi_ptr dev(get_trace_scsi_device(smi(), path.c_str()));
  CHECK(dev->open());
  if (!dev->is_open()) {
    std::printf("%s: %s\n", path.c_str(), dev->get_errmsg());
    return;
  }

  uint8_t buf[252];
  memset(buf, 0, sizeof(buf));
  CHECK_EQ(scsiStdInquiry(dev.get(), buf, 36), 0);
  CHECK(!memcmp(buf + 8, "SanDisk Extreme 55DD    0010", 28));

  // Supported log pages: 0x00 + vendor specific 0x30, 0x31, 0x32
  memset(buf, 0, sizeof(buf));
  CHECK_EQ(scsiLogSense(dev.get(), 0x00, 0, buf, sizeof(buf), 0), 0);
  CHECK_EQ(buf[3], 4);
  CHECK(!memcmp(buf + 4, "\x00\x30\x31\x32", 4));

  // Vendor page 0x32: controller temperature at capture time
  memset(buf, 0, sizeof(buf));
  CHECK_EQ(scsiLogSense(dev.get(), 0x32, 0, buf, sizeof(buf), 0), 0);
  CHECK_EQ(buf[0], 0xb2);
  CHECK_EQ(buf[3], 0x2d);

  // NVMe pass-through through the ASMedia vendor CDB is rejected
  scsi_sense_disect sinfo;
  scsi_cmnd_io io;
  static uint8_t ident[4096];
  const uint8_t cdb[16] = {0xe6, 0x06, 0, 0x01, };
  CHECK(raw_cmd(dev.get(), cdb, sizeof(cdb), DXFER_FROM_DEVICE, ident, sizeof(ident), sinfo, io));
  CHECK_EQ(sinfo.sense_key, SCSI_SK_ILLEGAL_REQUEST);
  CHECK_EQ(sinfo.asc, 0x20);
}

// The traced device is selected as plain SCSI device from its USB ID
static void test_drivedb_usb_sandisk_55dd(const char * drivedb)
{
  CHECK(read_drive_database(drivedb));
  usb_dev_info info, info2;
  CHECK_EQ(lookup_usb_device(0x0781, 0x55dd, 0x0010, info, info2), 1);
  CHECK(info.usb_type == "scsi");
  CHECK(info.usb_device.find("SanDisk Extreme") != std::string::npos);

  // 0x55ae (NVMe behind ASMedia bridge) is unchanged
  CHECK_EQ(lookup_usb_device(0x0781, 0x55ae, 0x3008, info, info2), 1);
  CHECK(info.usb_type == "sntasmedia");
}

int main(int argc, char ** argv)
{
  smart_interface::init();
  std::string datadir = (argc > 1 ? argv[1] : "tests/data");
  const char * drivedb = (argc > 2 ? argv[2] : "../lib/drivedb.h");

  test_open();
  test_inquiry_any_allocation_length();
  test_log_sense_twin_fetch();
  test_recorded_check_condition();
  test_unrecorded_page_of_known_opcode();
  test_unknown_opcode();
  test_transport_failure();
  test_data_out_is_never_accepted();
  test_recorded_data_out();
  test_identical_commands_in_recorded_order();
  test_parse_errors();
  test_sandisk_55dd_trace(datadir);
  test_drivedb_usb_sandisk_55dd(drivedb);

  std::printf("%d checks, %d failed\n", g_checks, g_failed);
  return (g_failed ? 1 : 0);
}
