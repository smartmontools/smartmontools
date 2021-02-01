/*
 * dev_jmb39x_raid.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2019-21 Christian Franke
 *
 * Based on JMraidcon (same license):
 *   Copyright (C) 2010 Werner Johansson
 *   http://git.xnk.nu/?p=JMraidcon.git
 *   https://github.com/Vlad1mir-D/JMraidcon
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include "dev_interface.h"
#include "dev_tunnelled.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "sg_unaligned.h"
#include "static_assert.h"
#include "utility.h"

#include <errno.h>

const char * dev_jmb39x_raid_cpp_svnid = "$Id$";

static void jmbassert_failed(int line, const char * expr)
{
  char msg[128];
  // Avoid __FILE__ as it may break reproducible builds
  snprintf(msg, sizeof(msg), "dev_jmb39x_raid.cpp(%d): Assertion failed: %s", line, expr);
  throw std::logic_error(msg);
}

#define jmbassert(expr) (!(expr) ? jmbassert_failed(__LINE__, #expr) : (void)0)

static void jmb_xor(uint8_t (& data)[512])
{
  static const uint8_t xor_table[] = {
    0x08, 0xc1, 0x67, 0x44, 0x04, 0x91, 0x0d, 0x3d, 0x9c, 0x44, 0xdb, 0x61, 0xba, 0x63, 0x00, 0x5c,
    0x48, 0x78, 0xc4, 0x19, 0x9f, 0xc8, 0x8a, 0x1f, 0x8f, 0xa3, 0x7f, 0x83, 0x08, 0xcf, 0x7a, 0x71,
    0x89, 0xa4, 0x1d, 0xcd, 0xe7, 0xd2, 0x32, 0xe1, 0x27, 0xad, 0xd4, 0xfa, 0x0e, 0x03, 0x99, 0xeb,
    0xf7, 0x83, 0x50, 0x50, 0x11, 0x2d, 0x79, 0xbe, 0x3c, 0xb4, 0xf1, 0xe3, 0x8f, 0xd9, 0x3b, 0x9f,
    0xd9, 0xb0, 0xf3, 0x67, 0x87, 0x90, 0xe0, 0x5d, 0xff, 0xf9, 0xf0, 0x60, 0x61, 0x55, 0x1a, 0x2e,
    0x81, 0x52, 0xaf, 0x73, 0xee, 0x25, 0xad, 0xc7, 0x01, 0x6e, 0xce, 0x6b, 0x01, 0x8d, 0x49, 0x74,
    0x9c, 0x9e, 0xed, 0x7e, 0xe9, 0x3b, 0xf3, 0xa2, 0x8e, 0x45, 0xa0, 0x39, 0x0f, 0xcd, 0x96, 0x6b,
    0x90, 0x3c, 0xa7, 0xb4, 0x5a, 0x6f, 0x72, 0xba, 0x08, 0x6b, 0x58, 0x1f, 0x35, 0x42, 0x2a, 0xc6,
    0x4f, 0xf4, 0x51, 0xa2, 0xa1, 0x48, 0x6e, 0x89, 0xe9, 0x36, 0x6d, 0xc8, 0x3b, 0x12, 0xec, 0x3a,
    0xad, 0x89, 0x2f, 0x37, 0xab, 0x1a, 0xde, 0x63, 0x2f, 0xef, 0x74, 0xee, 0xc7, 0xa9, 0x51, 0xd1,
    0xae, 0x63, 0xad, 0x92, 0x1b, 0x78, 0x98, 0xf1, 0xb6, 0x40, 0xbb, 0xfa, 0x22, 0x07, 0xf3, 0x22,
    0x95, 0xb7, 0x46, 0xa3, 0xca, 0x2b, 0x16, 0x85, 0x40, 0x41, 0x0a, 0xc5, 0xf3, 0x61, 0xc7, 0xad,
    0x53, 0xfb, 0x1b, 0x65, 0xac, 0xc9, 0x55, 0xee, 0x73, 0xc1, 0x02, 0xa0, 0x29, 0xfe, 0x53, 0x15,
    0x8f, 0x1f, 0xad, 0x8d, 0x77, 0xde, 0x15, 0xef, 0x6b, 0xf3, 0x1b, 0xd8, 0x44, 0x96, 0xe3, 0xaa,
    0x5a, 0x2a, 0xdc, 0x10, 0x7b, 0x96, 0xda, 0x3c, 0x8b, 0xf2, 0x3d, 0x38, 0xa4, 0x81, 0xf3, 0x2c,
    0x58, 0x41, 0xf5, 0x54, 0x73, 0x45, 0x9d, 0x73, 0xc5, 0xfd, 0xe8, 0x2a, 0xbe, 0xc6, 0x30, 0x50,
    0x9e, 0x4f, 0x8f, 0xa0, 0x29, 0xed, 0x4a, 0xe9, 0x2f, 0x32, 0x03, 0xca, 0x13, 0xd8, 0x5b, 0x7a,
    0xae, 0x9d, 0x58, 0xe6, 0x88, 0x73, 0x22, 0x90, 0x0a, 0x43, 0x6c, 0x41, 0x5b, 0x17, 0xc4, 0x1a,
    0x27, 0x5e, 0xf9, 0xef, 0x63, 0x9f, 0x57, 0x23, 0x6c, 0x27, 0x97, 0x70, 0xf5, 0xa8, 0x5b, 0x7b,
    0x5d, 0xa9, 0x0f, 0x37, 0xae, 0xff, 0x8b, 0xb2, 0xc8, 0xca, 0xd9, 0x28, 0x8e, 0x5b, 0xb2, 0x46,
    0xbe, 0x80, 0x40, 0x38, 0xe4, 0xee, 0xbb, 0x2c, 0xd2, 0x82, 0xc1, 0x72, 0x5a, 0x11, 0x4f, 0x4b,
    0x54, 0xe2, 0xb9, 0xf1, 0x24, 0x96, 0x53, 0x3d, 0x33, 0x81, 0xf1, 0x50, 0x2e, 0x1a, 0x04, 0x71,
    0x80, 0xf9, 0xbf, 0x66, 0x69, 0x9c, 0x6f, 0x22, 0x44, 0xd0, 0x69, 0xbb, 0xad, 0x93, 0x84, 0x98,
    0x74, 0xaf, 0x67, 0x32, 0xb9, 0x8f, 0x65, 0xf3, 0x4b, 0x0f, 0xf4, 0x85, 0xef, 0xb5, 0xba, 0xff,
    0xe1, 0xda, 0x9e, 0x9e, 0x32, 0x96, 0xa9, 0x19, 0xb8, 0x4f, 0x43, 0xf7, 0xf6, 0x4c, 0x1c, 0x0f,
    0xce, 0xd2, 0x67, 0xb6, 0xe3, 0xe3, 0x8d, 0x27, 0x1e, 0x27, 0x98, 0x4c, 0x73, 0x37, 0x5c, 0xff,
    0xab, 0x16, 0xca, 0x64, 0x7d, 0x91, 0xc0, 0x6d, 0xae, 0x60, 0xf0, 0x1a, 0x43, 0x12, 0xe6, 0xf4,
    0xd6, 0xe8, 0xba, 0xc2, 0x9b, 0x2f, 0xe6, 0xce, 0x07, 0x08, 0x6a, 0x8d, 0x28, 0x62, 0xa7, 0x31,
    0xe9, 0x3d, 0x4b, 0x9b, 0x5b, 0x19, 0x18, 0x13, 0xd2, 0xa9, 0xc1, 0x08, 0xce, 0x62, 0x12, 0x8c,
    0x12, 0x64, 0xe3, 0x43, 0xbb, 0xe3, 0x59, 0x1c, 0x57, 0x7f, 0xcd, 0xb9, 0x72, 0x65, 0x47, 0xab,
    0xb8, 0xfe, 0x61, 0xc1, 0x08, 0xc2, 0xec, 0x25, 0x8e, 0xb9, 0x1c, 0x89, 0xdf, 0x6d, 0xd2, 0xa7,
    0x36, 0xa7, 0x10, 0x52, 0x2a, 0x21, 0x2d, 0xaa, 0x98, 0x31, 0xd1, 0x77, 0x35, 0xa8, 0x3b, 0x40,
  };
  STATIC_ASSERT(sizeof(xor_table) == sizeof(data));

  for (unsigned i = 0; i < sizeof(data); i++) {
    data[i] ^= xor_table[i];
  }
}

static uint32_t jmb_crc(const uint8_t (& data)[512])
{
  static const uint32_t crc_table[] = { // Polynomial 0x04c11db7
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
  };
  STATIC_ASSERT(sizeof(crc_table) == 256*sizeof(uint32_t));

  uint32_t crc = 0x52325032;
  for (unsigned i = 0; i < sizeof(data)/sizeof(uint32_t) - 1; i++) {
    uint32_t dw = sg_get_unaligned_be32(data + i*sizeof(uint32_t));
    crc = crc_table[( dw      & 0xff) ^ (crc >> 24)] ^ (crc << 8);
    crc = crc_table[((dw>> 8) & 0xff) ^ (crc >> 24)] ^ (crc << 8);
    crc = crc_table[((dw>>16) & 0xff) ^ (crc >> 24)] ^ (crc << 8);
    crc = crc_table[( dw>>24        ) ^ (crc >> 24)] ^ (crc << 8);
  }
  return crc;
}

static inline uint32_t jmb_get_crc(const uint8_t (& data)[512])
{
  return sg_get_unaligned_le32(data + sizeof(data) - 4);
}

static inline void jmb_put_crc(uint8_t (& data)[512], uint32_t crc)
{
  sg_put_unaligned_le32(crc, data + sizeof(data) - 4);
}

static inline bool jmb_check_crc(const uint8_t (& data)[512])
{
  return (jmb_get_crc(data) == jmb_crc(data));
}

static inline void jmb_put_le32(uint8_t (& data)[512], unsigned index, uint32_t val)
{
  jmbassert(index + 4 <= sizeof(data));
  sg_put_unaligned_le32(val, data + index);
}

static void jmb_set_wakeup_sector(uint8_t (& data)[512], int id)
{
  uint32_t code = 0, crc = 0;
  switch (id) {
    case 0: code = 0x3c75a80b; crc = 0x706d10d9; break;
    case 1: code = 0x0388e337; crc = 0x6958511e; break;
    case 2: code = 0x689705f3; crc = 0xfe234b07; break;
    case 3: code = 0xe00c523a; crc = 0x5be57adb; break;
    default: jmbassert(false);
  }
  jmb_put_le32(data, 0, 0x197b0325); // WAKEUP_CMD
  jmb_put_le32(data, 4, code);
  memset(data + 8, 0, 8);
  for (unsigned i = 16; i < sizeof(data) - 8; i++)
    data[i] = (uint8_t)i;
  jmb_put_le32(data, sizeof(data) - 8, 0x10eca1db);
  jmb_put_crc(data, crc);
}

static void jmb_set_request_sector(uint8_t (& data)[512], uint8_t version, uint32_t cmd_id,
  const uint8_t * cmd, unsigned cmdsize)
{
  jmbassert(4 <= cmdsize && cmdsize <= 24);
  memset(data, 0, sizeof(data));

  uint32_t scrambled_cmd_code;
  switch (version) {
    default:
    case 0: scrambled_cmd_code = 0x197b0322; break; // JMB39x: various devices
    case 1: scrambled_cmd_code = 0x197b0393; break; // JMB39x: QNAP TR-004 NAS
    case 2: scrambled_cmd_code = 0x197b0562; break; // JMS562
  }
  jmb_put_le32(data, 0, scrambled_cmd_code);

  jmb_put_le32(data, 4, cmd_id);
  memcpy(data + 8, cmd, cmdsize);
  jmb_put_crc(data, jmb_crc(data));
}

static int jmb_get_sector_type(const uint8_t (& data)[512])
{
  if (jmb_check_crc(data))
    return 1; // Plain (wakeup) sector
  uint8_t data2[512];
  memcpy(data2, data, sizeof(data2));
  jmb_xor(data2);
  if (jmb_check_crc(data2))
    return 2; // Obfuscated (request/response) sector
  return 0;
}

static void jmb_check_funcs()
{
  uint8_t data[512];
  jmb_set_wakeup_sector(data, 0);
  jmbassert(jmb_check_crc(data));
  jmbassert(jmb_get_sector_type(data) == 1);
  jmb_set_wakeup_sector(data, 1);
  jmbassert(jmb_check_crc(data));
  jmb_set_wakeup_sector(data, 2);
  jmbassert(jmb_check_crc(data));
  jmb_xor(data);
  jmbassert(jmb_crc(data) == 0x053ed64b);
  jmb_xor(data);
  jmbassert(jmb_check_crc(data));
  jmb_set_wakeup_sector(data, 3);
  jmbassert(jmb_check_crc(data));
  uint8_t cmd[] = {1, 2, 3, 4, 5, 6, 7};
  jmb_set_request_sector(data, 0, 42, cmd, sizeof(cmd));
  jmbassert(jmb_get_crc(data) == 0xb1f765d7);
  jmbassert(jmb_check_crc(data));
  jmb_set_request_sector(data, 1, 42, cmd, sizeof(cmd));
  jmbassert(jmb_get_crc(data) == 0x388b2759);
  jmbassert(jmb_check_crc(data));
  jmb_set_request_sector(data, 2, 42, cmd, sizeof(cmd));
  jmbassert(jmb_get_crc(data) == 0xde10952b);
  jmbassert(jmb_check_crc(data));
  jmb_xor(data);
  jmbassert(jmb_get_sector_type(data) == 2);
}

/////////////////////////////////////////////////////////////////////////////

static bool ata_read_lba8(ata_device * atadev, uint8_t lba8, uint8_t (& data)[512])
{
  ata_cmd_in in;
  in.in_regs.command = 0x20; // READ SECTORS, 28-bit PIO
  in.set_data_in(data, 1);
  in.in_regs.lba_low  = lba8;
  in.in_regs.lba_mid  = 0;
  in.in_regs.lba_high = 0;
  in.in_regs.device = 0x40; // LBA mode | LBA bits 24-27
  if (!atadev->ata_pass_through(in))
    return false;
  return true;
}

static bool ata_write_lba8(ata_device * atadev, uint8_t lba8, const uint8_t (& data)[512])
{
  ata_cmd_in in;
  in.in_regs.command = 0x30; // WRITE SECTORS, 28-bit PIO
  in.set_data_out(data, 1);
  in.in_regs.lba_low  = lba8;
  in.in_regs.lba_mid  = 0;
  in.in_regs.lba_high = 0;
  in.in_regs.device = 0x40; // LBA mode | LBA bits 24-27
  if (!atadev->ata_pass_through(in))
    return false;
  return true;
}

static int scsi_get_lba_size(scsi_device * scsidev)
{
  scsi_readcap_resp srr; memset(&srr, 0, sizeof(srr));
  if (!scsiGetSize(scsidev, false /*avoid_rcap16*/, &srr))
    return -1;
  return srr.lb_size;
}

static bool scsi_read_lba8(scsi_device * scsidev, uint8_t lba8, uint8_t (& data)[512])
{
  struct scsi_cmnd_io io_hdr; memset(&io_hdr, 0, sizeof(io_hdr));
  io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
  io_hdr.dxfer_len = 512;
  io_hdr.dxferp = data;
  uint8_t cdb[] = {0x28 /* READ(10) */, 0x00, 0x00, 0x00, 0x00, lba8, 0x00, 0x00, 0x01, 0x00};
  STATIC_ASSERT(sizeof(cdb) == 10);
  io_hdr.cmnd = cdb;
  io_hdr.cmnd_len = sizeof(cdb);
  io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

  if (!scsidev->scsi_pass_through_and_check(&io_hdr, "scsi_read_lba"))
    return false;
  return true;
}

static bool scsi_write_lba8(scsi_device * scsidev,  uint8_t lba8, const uint8_t (& data)[512])
{
  struct scsi_cmnd_io io_hdr; memset(&io_hdr, 0, sizeof(io_hdr));
  io_hdr.dxfer_dir = DXFER_TO_DEVICE;
  io_hdr.dxfer_len = 512;
  io_hdr.dxferp = const_cast<uint8_t *>(data);
  uint8_t cdb[] = {0x2a /* WRITE(10) */, 0x00, 0x00, 0x00, 0x00, lba8, 0x00, 0x00, 0x01, 0x00};
  STATIC_ASSERT(sizeof(cdb) == 10);
  io_hdr.cmnd = cdb;
  io_hdr.cmnd_len = sizeof(cdb);
  io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

  if (!scsidev->scsi_pass_through_and_check(&io_hdr, "scsi_write_lba"))
    return false;
  return true;
}

/////////////////////////////////////////////////////////////////////////////

namespace jmb39x {

class jmb39x_device
: public tunnelled_device<
  /*implements*/ ata_device,
  /*by tunnelling through a ATA or SCSI*/ smart_device
>
{
public:
  jmb39x_device(smart_interface * intf, smart_device * smartdev, const char * req_type,
    uint8_t version, uint8_t port, uint8_t lba, bool force);

  virtual ~jmb39x_device();

  virtual bool open() override;

  virtual bool close() override;

  virtual bool ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out) override;

private:
  uint8_t m_version;
  uint8_t m_port;
  uint8_t m_lba;
  bool m_force;

  bool m_blocked;
  bool m_orig_write_back;
  uint32_t m_cmd_id;
  uint8_t m_orig_data[512];

  bool raw_read(uint8_t (& data)[512]);
  bool raw_write(const uint8_t (& data)[512]);
  bool run_jmb_command(const uint8_t * cmd, unsigned cmdsize, uint8_t (& response)[512]);
  void report_orig_data_lost() const;
  bool restore_orig_data();
};

jmb39x_device::jmb39x_device(smart_interface * intf, smart_device * smartdev, const char * req_type,
  uint8_t version, uint8_t port, uint8_t lba, bool force)
: smart_device(intf, smartdev->get_dev_name(), req_type, req_type),
  tunnelled_device<ata_device, smart_device>(smartdev),
  m_version(version), m_port(port), m_lba(lba), m_force(force),
  m_blocked(false), m_orig_write_back(false), m_cmd_id(0)
{
  set_info().info_name = strprintf("%s [jmb39x_disk_%u]", smartdev->get_info_name(), port);
  memset(m_orig_data, 0, sizeof(m_orig_data));
}

jmb39x_device::~jmb39x_device()
{
  if (m_orig_write_back) try {
    jmb39x_device::restore_orig_data();
  } catch (...) {
    // ignore
  }
}

bool jmb39x_device::raw_read(uint8_t (& data)[512])
{
  memset(data, 0, sizeof(data));
  if (get_tunnel_dev()->is_scsi()) {
    if (!scsi_read_lba8(get_tunnel_dev()->to_scsi(), m_lba, data))
      return set_err(EIO, "SCSI READ LBA %d failed: %s", m_lba, get_tunnel_dev()->get_errmsg());
  }
  else if (get_tunnel_dev()->is_ata()) {
    if (!ata_read_lba8(get_tunnel_dev()->to_ata(), m_lba, data))
      return set_err(EIO, "ATA READ LBA %d failed: %s", m_lba, get_tunnel_dev()->get_errmsg());
  }
  else {
    jmbassert(false);
  }
  return true;
}

bool jmb39x_device::raw_write(const uint8_t (& data)[512])
{
  if (get_tunnel_dev()->is_scsi()) {
    if (!scsi_write_lba8(get_tunnel_dev()->to_scsi(), m_lba, data))
      return set_err(EIO, "SCSI WRITE LBA %d failed: %s", m_lba, get_tunnel_dev()->get_errmsg());
  }
  else if (get_tunnel_dev()->is_ata()) {
    if (!ata_write_lba8(get_tunnel_dev()->to_ata(), m_lba, data))
      return set_err(EIO, "ATA WRITE LBA %d failed: %s", m_lba, get_tunnel_dev()->get_errmsg());
  }
  else {
    jmbassert(false);
  }
  return true;
}

bool jmb39x_device::run_jmb_command(const uint8_t * cmd, unsigned cmdsize, uint8_t (& response)[512])
{
  // Set up request
  uint8_t request[512];
  jmb_set_request_sector(request, m_version, m_cmd_id, cmd, cmdsize);

  if (ata_debugmode) {
    pout("JMB39x: Write request sector #%d\n", m_cmd_id);
    if (ata_debugmode > 1)
      dStrHex(request, sizeof(request), 0);
  }

  // Write obfuscated request
  jmb_xor(request);
  if (!raw_write(request)) {
    m_blocked = true;
    return false;
  }
  jmb_xor(request);

  // Read obfuscated response
  memset(response, 0, sizeof(response));
  if (!raw_read(response)) {
    m_blocked = true;
    return false;
  }
  jmb_xor(response);

  if (ata_debugmode) {
    pout("JMB39x: Read response sector #%d\n", m_cmd_id);
    if (ata_debugmode > 1)
      dStrHex(response, sizeof(response), 0);
  }

  // Check result
  if (!memcmp(request, response, sizeof(request))) { // regular I/O?
    m_blocked = true;
    return set_err(EIO, "No JMB39x response detected");
  }
  if (!jmb_check_crc(response)) {
    m_blocked = true;
    jmb_xor(response);
    return set_err(EIO, "%s", (!jmb_check_crc(response)
      ? "CRC error in JMB39x response"
      : "JMB39x response contains a wakeup sector"));
  }
  if (memcmp(request, response, 8)) { // code + id identical?
    m_blocked = true;
    return set_err(EIO, "Invalid header in JMB39x response");
  }

  m_cmd_id++;
  return true;
}

void jmb39x_device::report_orig_data_lost() const
{
  bool zf = !nonempty(m_orig_data, sizeof(m_orig_data));
  pout("JMB39x: WARNING: Data (%szero filled) at LBA %d lost\n", (zf ? "" : "not "), m_lba);
  if (!zf) // Dump lost data
    dStrHex(m_orig_data, sizeof(m_orig_data), 0);
}

bool jmb39x_device::restore_orig_data()
{
  if (ata_debugmode)
    pout("JMB39x: Restore original sector (%szero filled)\n",
         (nonempty(m_orig_data, sizeof(m_orig_data)) ? "not " : ""));
  if (!raw_write(m_orig_data)) {
    report_orig_data_lost();
    m_blocked = true;
    return false;
  }
  return true;
}

bool jmb39x_device::open()
{
  m_orig_write_back = false;
  if (m_blocked)
    return set_err(EIO, "Device blocked due to previous errors");

  if (!tunnelled_device<ata_device, smart_device>::open())
    return false;

  // Check SCSI LBA size (assume 512 if ATA)
  if (get_tunnel_dev()->is_scsi()) {
    int lba_size = scsi_get_lba_size(get_tunnel_dev()->to_scsi());
    if (lba_size < 0) {
      error_info err = get_tunnel_dev()->get_err();
      tunnelled_device<ata_device, smart_device>::close();
      return set_err(err.no, "SCSI READ CAPACITY failed: %s", err.msg.c_str());
    }
    if (lba_size != 512) {
      tunnelled_device<ata_device, smart_device>::close();
      return set_err(EINVAL, "LBA size is %d but must be 512", lba_size);
    }
  }

  // Read original data
  if (ata_debugmode)
    pout("JMB39x: Read original data at LBA %d\n", m_lba);
  if (!raw_read(m_orig_data)) {
    error_info err = get_err();
    tunnelled_device<ata_device, smart_device>::close();
    return set_err(err);
  }

  // Check original data
  if (nonempty(m_orig_data, sizeof(m_orig_data))) {
    if (ata_debugmode > 1)
      dStrHex(m_orig_data, sizeof(m_orig_data), 0);
    int st = jmb_get_sector_type(m_orig_data);
    if (!m_force) {
      tunnelled_device<ata_device, smart_device>::close();
      m_blocked = true;
      return set_err(EINVAL, "Original sector at LBA %d %s", m_lba,
        (st == 0 ? "is not zero filled" :
         st == 1 ? "contains JMB39x wakeup data"
                 : "contains JMB39x protocol data"));
    }
    if (st) {
      // Zero fill to reset protocol state
      if (ata_debugmode)
        pout("JMB39x: Zero filling original data\n");
      memset(m_orig_data, 0, sizeof(m_orig_data));
    }
  }

  // TODO: Defer SIGINT,... until close()

  // Write 4 wakeup sectors
  uint8_t dataout[512];
  for (int id = 0; id < 4; id++) {
    jmb_set_wakeup_sector(dataout, id);
    if (ata_debugmode) {
      pout("JMB39x: Write wakeup sector #%d\n", id+1);
      if (ata_debugmode > 1)
        dStrHex(dataout, sizeof(dataout), 0);
    }
    if (!raw_write(dataout)) {
        error_info err = get_err();
        if (id > 0)
          report_orig_data_lost();
        tunnelled_device<ata_device, smart_device>::close();
        m_blocked = true;
        return set_err(err.no, "Write of JMB39x wakeup sector #%d: %s", id + 1, err.msg.c_str());
    }
  }
  m_orig_write_back = true;

  // start command sequence
  m_cmd_id = 1;

  // Run JMB identify disk command
  uint8_t b = (m_version != 1 ? 0x02 : 0x01);
  uint8_t cmd[24]= {
    0x00,
    b, b,
    0xff,
    m_port,
    0x00, 0x00, 0x00,
    m_port,
    0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
  };
  uint8_t (& response)[512] = dataout;
  if (!run_jmb_command(cmd, sizeof(cmd), response)) {
    error_info err = get_err();
    close();
    return set_err(err);
  }

  // Check for device model string
  if (response[16] < ' ') {
    close();
    return set_err(ENOENT, "No device connected to JMB39x port %d", m_port);
  }
  return true;
}

bool jmb39x_device::close()
{
  bool ok = true;
  if (m_orig_write_back) {
    ok = restore_orig_data();
    m_orig_write_back = false;
  }

  if (!tunnelled_device<ata_device, smart_device>::close())
    return false;
  return ok;
}

// Return: 0=unsupported, 1=supported, 2=supported and has checksum
static int is_supported_by_jmb(const ata_in_regs & r)
{
  switch (r.command) {
    case ATA_IDENTIFY_DEVICE:
      return 1; // Checksum is optional
    case ATA_SMART_CMD:
      switch (r.features) {
        case ATA_SMART_READ_VALUES:
        case ATA_SMART_READ_THRESHOLDS:
          return 2;
        case ATA_SMART_READ_LOG_SECTOR:
          switch (r.lba_low) {
            case 0x00: return 1; // Log directory
            case 0x01: return 2; // Summary Error log
            case 0xe0: return 1; // SCT Command/Status
          }
          break;
      }
      break;
  }
  return 0;
}

bool jmb39x_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & /* out */)
{
  jmbassert(is_open());
  if (m_blocked)
    return set_err(EIO, "Device blocked due to previous errors");
  if (in.direction == ata_cmd_in::no_data) // TODO: add to ata_cmd_is_supported() ?
    return set_err(ENOSYS, "NO DATA ATA commands not implemented [JMB39x]");
  if (!ata_cmd_is_supported(in, 0, "JMB39x"))
    return false;
  // Block all commands which require full sector data
  int supported = is_supported_by_jmb(in.in_regs);
  if (!supported)
    return set_err(ENOSYS, "ATA command not implemented due to truncated response [JMB39x]");
  jmbassert(in.direction == ata_cmd_in::data_in);

  // Run ATA pass-through command
  uint8_t cmd[24]= {
    0x00, 0x02, 0x03, 0xff,
    m_port,
    0x02, 0x00, 0xe0, 0x00, 0x00,
    // Registers
    in.in_regs.features,
    0x00,
    in.in_regs.sector_count,
    0x00,
    in.in_regs.lba_low,
    0x00,
    in.in_regs.lba_mid,
    0x00,
    in.in_regs.lba_high,
    0x00,
    0xa0, // in.in_regs.device ?
    0x00,
    in.in_regs.command,
    0x00 // status register returned here
  };
  uint8_t response[512];
  if (!run_jmb_command(cmd, sizeof(cmd), response))
    return false;

  // Check status register
  uint8_t status = response[31];
  if (status == 0x00) {
    m_blocked = true;
    return set_err(EIO, "No device connected to JMB39x port %d", m_port);
  }
  if ((status & 0xc1) != 0x40 /* !(!BSY && DRDY && !ERR) */)
    return set_err(EIO, "ATA command failed (status=0x%02x)", status);

  // Copy data
  jmbassert(in.size == sizeof(response));
  memset(in.buffer, 0, in.size);
  memcpy(in.buffer, response + 32, in.size - 32 - 16);

  // Prevent checksum warning
  if (supported > 1)
    ((uint8_t *)in.buffer)[512-1] -= checksum(in.buffer);

  return true;
}

} // namespace jmb39x

ata_device * smart_interface::get_jmb39x_device(const char * type, smart_device * smartdev)
{
  jmbassert(smartdev != 0);
  // Take temporary ownership of 'smartdev' to delete it on error
  smart_device_auto_ptr smartdev_holder(smartdev);
  jmb_check_funcs();

  // Base device must be ATA or SCSI
  if (!(smartdev->is_ata() || smartdev->is_scsi())) {
    set_err(EINVAL, "Type '%s+...': Device type '%s' is not ATA or SCSI", type, smartdev->get_req_type());
    return 0;
  }

  int n1 = -1;
  char prefix[15+1] = "";
  sscanf(type, "%15[^,],%n", prefix, &n1);
  uint8_t version;
  if (!strcmp(prefix, "jmb39x"))
    version = 0;
  else if (!strcmp(prefix, "jmb39x-q"))
    version = 1;
  else if (!strcmp(prefix, "jms56x"))
    version = 2;
  else
    n1 = -1;
  if (n1 < 0) {
    set_err(EINVAL, "Unknown JMicron type '%s'", type);
    return 0;
  }

  // Use default LBA 33, same as JMraidcon.
  // MBR disk: Zero filled if there is no boot code in boot area.
  // GPT disk: Zero filled if GPT entries 125-128 are empty.
  unsigned lba = 33;

  unsigned port = ~0;
  bool force = false;
  const char * args = type + n1;
  n1 = -1;
  sscanf(args, "%u%n", &port, &n1);
  int n2 = -1, len = strlen(args);
  if (0 < n1 && n1 < len && sscanf(args + n1, ",s%u%n", &lba, &n2) == 1 && n2 > 0)
    n1 += n2;
  n2 = -1;
  if (0 < n1 && n1 < len && (sscanf(args + n1, ",force%n",  &n2), n2) > 0) {
    force = true;
    n1 += n2;
  }
  if (!(n1 == len && port <= 4 && 33 <= lba && lba <= 62)) {
    set_err(EINVAL, "Option -d %s,N[,sLBA][,force] must have 0 <= N <= 4 [, 33 <= LBA <= 62]", prefix);
    return 0;
  }

  ata_device * jmbdev = new jmb39x::jmb39x_device(this, smartdev, type, version, port, lba, force);
  // 'smartdev' is now owned by 'jmbdev'
  smartdev_holder.release();
  return jmbdev;
}
