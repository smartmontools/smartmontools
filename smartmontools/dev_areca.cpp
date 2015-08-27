/*
 * dev_areca.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2012 Hank Wu <hank@areca.com.tw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "int64.h"

#include "dev_interface.h"
#include "dev_areca.h"

const char * dev_areca_cpp_cvsid = "$Id$"
  DEV_ARECA_H_CVSID;

#include "atacmds.h"
#include "scsicmds.h"

#include <errno.h>

#if 0 // For debugging areca code
static void dumpdata(unsigned char *block, int len)
{
  int ln = (len / 16) + 1;   // total line#
  unsigned char c;
  int pos = 0;

  printf(" Address = %p, Length = (0x%x)%d\n", block, len, len);
  printf("      0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F      ASCII      \n");
  printf("=====================================================================\n");

  for ( int l = 0; l < ln && len; l++ )
  {
    // printf the line# and the HEX data
    // if a line data length < 16 then append the space to the tail of line to reach 16 chars
    printf("%02X | ", l);
    for ( pos = 0; pos < 16 && len; pos++, len-- )
    {
      c = block[l*16+pos];
      printf("%02X ", c);
    }

    if ( pos < 16 )
    {
      for ( int loop = pos; loop < 16; loop++ )
      {
        printf("   ");
      }
    }

    // print ASCII char
    for ( int loop = 0; loop < pos; loop++ )
    {
      c = block[l*16+loop];
      if ( c >= 0x20 && c <= 0x7F )
      {
        printf("%c", c);
      }
      else
      {
        printf(".");
      }
    }
    printf("\n");
  }
  printf("=====================================================================\n");
}
#endif

generic_areca_device::generic_areca_device(smart_interface * intf, const char * dev_name, int disknum, int encnum)
: smart_device(intf, dev_name, "areca", "areca"),
  m_disknum(disknum),
  m_encnum(encnum)
{
  set_info().info_name = strprintf("%s [areca_disk#%02d_enc#%02d]", dev_name, disknum, encnum);
}

generic_areca_device::~generic_areca_device() throw()
{

}

// PURPOSE
//   This is an interface routine meant to isolate the OS dependent
//   parts of the code, and to provide a debugging interface.  Each
//   different port and OS needs to provide it's own interface.  This
//   is the Windows interface to the Areca "arcmsr" driver.  It allows ATA
//   commands to be passed through the SCSI driver.
// DETAILED DESCRIPTION OF ARGUMENTS
//   fd: is the file descriptor provided by open()
//   disknum is the disk number (0 to 127) in the RAID array
//   command: defines the different operations.
//   select: additional input data if needed (which log, which type of
//           self-test).
//   data:   location to write output data, if needed (512 bytes).
//   Note: not all commands use all arguments.
// RETURN VALUES
//  -1 if the command failed
//   0 if the command succeeded,
//   STATUS_CHECK routine:
//  -1 if the command failed
//   0 if the command succeeded and disk SMART status is "OK"
//   1 if the command succeeded and disk SMART status is "FAILING"
int generic_areca_device::arcmsr_command_handler(unsigned long arcmsr_cmd, unsigned char *data, int data_len)
{
  if (arcmsr_cmd >= ARCMSR_CMD_TOTAL)
    return -1;

  static const unsigned int cmds[ARCMSR_CMD_TOTAL] =
  {
    ARCMSR_IOCTL_READ_RQBUFFER,
    ARCMSR_IOCTL_WRITE_WQBUFFER,
    ARCMSR_IOCTL_CLEAR_RQBUFFER,
    ARCMSR_IOCTL_CLEAR_WQBUFFER,
    ARCMSR_IOCTL_RETURN_CODE_3F
  };

  int ioctlreturn = 0;
  sSRB_BUFFER sBuf;
  struct scsi_cmnd_io iop;
  int dir = DXFER_TO_DEVICE;

  UINT8 cdb[10]={0};
  UINT8 sense[32]={0};

  unsigned char *areca_return_packet;
  int total = 0;
  int expected = -1;
  unsigned char return_buff[2048]={0};
  unsigned char *ptr = &return_buff[0];

  memset((unsigned char *)&sBuf, 0, sizeof(sBuf));
  memset(&iop, 0, sizeof(iop));

  sBuf.srbioctl.HeaderLength = sizeof(sARCMSR_IO_HDR);
  memcpy(sBuf.srbioctl.Signature, ARECA_SIG_STR, strlen(ARECA_SIG_STR));
  sBuf.srbioctl.Timeout = 10000;
  sBuf.srbioctl.ControlCode = cmds[arcmsr_cmd];

  switch ( arcmsr_cmd )
  {
  // command for writing data to driver
  case ARCMSR_WRITE_WQBUFFER:
    if ( data && data_len )
    {
      sBuf.srbioctl.Length = data_len;
      memcpy((unsigned char *)sBuf.ioctldatabuffer, (unsigned char *)data, data_len);
    }
    // commands for clearing related buffer of driver
  case ARCMSR_CLEAR_RQBUFFER:
  case ARCMSR_CLEAR_WQBUFFER:
    cdb[0] = 0x3B; //SCSI_WRITE_BUF command;
    break;
  // command for reading data from driver
  case ARCMSR_READ_RQBUFFER:
  // command for identifying driver
  case ARCMSR_RETURN_CODE_3F:
    cdb[0] = 0x3C; //SCSI_READ_BUF command;
    dir = DXFER_FROM_DEVICE;
    break;
  default:
    // unknown arcmsr commands
    return -1;
  }

  cdb[1] = 0x01;
  cdb[2] = 0xf0;
  cdb[5] = cmds[arcmsr_cmd] >> 24;
  cdb[6] = cmds[arcmsr_cmd] >> 16;
  cdb[7] = cmds[arcmsr_cmd] >> 8;
  cdb[8] = cmds[arcmsr_cmd] & 0x0F;

  iop.dxfer_dir = dir;
  iop.dxfer_len = sizeof(sBuf);
  iop.dxferp = (unsigned char *)&sBuf;
  iop.cmnd = cdb;
  iop.cmnd_len = sizeof(cdb);
  iop.sensep = sense;
  iop.max_sense_len = sizeof(sense);
  iop.timeout = SCSI_TIMEOUT_DEFAULT;

  while ( 1 )
  {
    ioctlreturn = arcmsr_do_scsi_io(&iop);
    if(ioctlreturn || iop.scsi_status)
    {
      break;
    }

    if ( arcmsr_cmd != ARCMSR_READ_RQBUFFER )
    {
      // if succeeded, just returns the length of outgoing data
      return data_len;
    }

    if ( sBuf.srbioctl.Length )
    {
      memcpy(ptr, &sBuf.ioctldatabuffer[0], sBuf.srbioctl.Length);
      ptr += sBuf.srbioctl.Length;
      total += sBuf.srbioctl.Length;
      // the returned bytes enough to compute payload length ?
      if ( expected < 0 && total >= 5 )
      {
        areca_return_packet = (unsigned char *)&return_buff[0];
        if ( areca_return_packet[0] == 0x5E &&
           areca_return_packet[1] == 0x01 &&
           areca_return_packet[2] == 0x61 )
        {
          // valid header, let's compute the returned payload length,
          // we expected the total length is
          // payload + 3 bytes header + 2 bytes length + 1 byte checksum
          expected = areca_return_packet[4] * 256 + areca_return_packet[3] + 6;
        }
      }

      if ( total >= 7 && total >= expected )
      {
        //printf("total bytes received = %d, expected length = %d\n", total, expected);

        // ------ Okay! we received enough --------
        break;
      }
    }
  }

  // Deal with the different error cases
  if ( arcmsr_cmd == ARCMSR_RETURN_CODE_3F )
  {
    // Silence the ARCMSR_IOCTL_RETURN_CODE_3F's error, no pout(...)
    return -4;
  }

  if ( ioctlreturn )
  {
    pout("do_scsi_cmnd_io with write buffer failed code = %x\n", ioctlreturn);
    return -2;
  }

  if ( iop.scsi_status )
  {
    pout("io_hdr.scsi_status with write buffer failed code = %x\n", iop.scsi_status);
    return -3;
  }

  if ( data )
  {
    memcpy(data, return_buff, total);
  }

  return total;
}

bool generic_areca_device::arcmsr_probe()
{
  if(!is_open())
  {
    open();
  }

  if(arcmsr_command_handler(ARCMSR_RETURN_CODE_3F, NULL, 0) != 0)
  {
    return false;
  }
  return true;
}

int generic_areca_device::arcmsr_ui_handler(unsigned char *areca_packet, int areca_packet_len, unsigned char *result)
{
  int expected = 0;
  unsigned char return_buff[2048];
  unsigned char cs = 0;
  int cs_pos = 0;

  // ----- ADD CHECKSUM -----
  cs_pos = areca_packet_len - 1;
  for(int i = 3; i < cs_pos; i++)
  {
      areca_packet[cs_pos] += areca_packet[i];
  }

  if(!arcmsr_lock())
  {
    return -1;
  }
  expected = arcmsr_command_handler(ARCMSR_CLEAR_RQBUFFER, NULL, 0);
  if (expected==-3) {
    return set_err(EIO);
  }
  expected = arcmsr_command_handler(ARCMSR_CLEAR_WQBUFFER, NULL, 0);
  expected = arcmsr_command_handler(ARCMSR_WRITE_WQBUFFER, areca_packet, areca_packet_len);
  if ( expected > 0 )
  {
    expected = arcmsr_command_handler(ARCMSR_READ_RQBUFFER, return_buff, sizeof(return_buff));
  }

  if ( expected < 0 )
  {
    return -1;
  }

  if(!arcmsr_unlock())
  {
    return -1;
  }

  // ----- VERIFY THE CHECKSUM -----
  cs = 0;
  for ( int loop = 3; loop < expected - 1; loop++ )
  {
      cs += return_buff[loop];
  }

  if ( return_buff[expected - 1] != cs )
  {
    return -1;
  }

  memcpy(result, return_buff, expected);

  return expected;
}

int generic_areca_device::arcmsr_get_controller_type()
{
  int expected = 0;
  unsigned char return_buff[2048];
  unsigned char areca_packet[] = {0x5E, 0x01, 0x61, 0x01, 0x00, 0x23, 0x00};

  memset(return_buff, 0, sizeof(return_buff));
  expected = arcmsr_ui_handler(areca_packet, sizeof(areca_packet), return_buff);
  if ( expected < 0 )
  {
    return -1;
  }

  return return_buff[0xc2];
}

int generic_areca_device::arcmsr_get_dev_type()
{
  int expected = 0;
  unsigned char return_buff[2048];
  int ctlr_type = -1;
  int encnum = get_encnum();
  int disknum = get_disknum();
  unsigned char areca_packet[] = {0x5E, 0x01, 0x61, 0x03, 0x00, 0x22,
    (unsigned char)(disknum - 1), (unsigned char)(encnum - 1), 0x00};

  memset(return_buff, 0, sizeof(return_buff));
  expected = arcmsr_ui_handler(areca_packet, sizeof(areca_packet), return_buff);
  if ( expected < 0 )
  {
    return -1;
  }

  ctlr_type = arcmsr_get_controller_type();

  if( ctlr_type < 0 )
  {
    return ctlr_type;
  }

  if( ctlr_type == 0x02/* SATA Controllers */ ||
     (ctlr_type == 0x03 /* SAS Controllers */ && return_buff[0x52] & 0x01 /* SATA devices behind SAS Controller */) )
  {
    // SATA device
    return 1;
  }

  // SAS device
  return 0;
}

bool generic_areca_device::arcmsr_ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  // ATA input registers
  typedef struct _ATA_INPUT_REGISTERS
  {
    unsigned char features;
    unsigned char sector_count;
    unsigned char sector_number;
    unsigned char cylinder_low;
    unsigned char cylinder_high;
    unsigned char device_head;
    unsigned char command;
    unsigned char reserved[8];
    unsigned char data[512]; // [in/out] buffer for outgoing/incoming data
  } sATA_INPUT_REGISTERS;

  // ATA output registers
  // Note: The output registers is re-sorted for areca internal use only
  typedef struct _ATA_OUTPUT_REGISTERS
  {
    unsigned char error;
    unsigned char status;
    unsigned char sector_count;
    unsigned char sector_number;
    unsigned char cylinder_low;
    unsigned char cylinder_high;
  } sATA_OUTPUT_REGISTERS;

  // Areca packet format for outgoing:
  // B[0~2] : 3 bytes header, fixed value 0x5E, 0x01, 0x61
  // B[3~4] : 2 bytes command length + variant data length, little endian
  // B[5]   : 1 bytes areca defined command code, ATA passthrough command code is 0x1c
  // B[6~last-1] : variant bytes payload data
  // B[last] : 1 byte checksum, simply sum(B[3] ~ B[last -1])
  //
  //
  //   header 3 bytes  length 2 bytes   cmd 1 byte    payload data x bytes  cs 1 byte
  // +--------------------------------------------------------------------------------+
  // + 0x5E 0x01 0x61 |   0x00 0x00   |     0x1c   | .................... |   0x00    |
  // +--------------------------------------------------------------------------------+
  //

  //Areca packet format for incoming:
  // B[0~2] : 3 bytes header, fixed value 0x5E, 0x01, 0x61
  // B[3~4] : 2 bytes payload length, little endian
  // B[5~last-1] : variant bytes returned payload data
  // B[last] : 1 byte checksum, simply sum(B[3] ~ B[last -1])
  //
  //
  //   header 3 bytes  length 2 bytes   payload data x bytes  cs 1 byte
  // +-------------------------------------------------------------------+
  // + 0x5E 0x01 0x61 |   0x00 0x00   | .................... |   0x00    |
  // +-------------------------------------------------------------------+
  unsigned char    areca_packet[640];
  int areca_packet_len = sizeof(areca_packet);
  unsigned char return_buff[2048];
  int expected = 0;

  sATA_INPUT_REGISTERS *ata_cmd;

  // For debugging
#if 0
  memset(sInq, 0, sizeof(sInq));
  scsiStdInquiry(fd, (unsigned char *)sInq, (int)sizeof(sInq));
  dumpdata((unsigned char *)sInq, sizeof(sInq));
#endif
  memset(areca_packet, 0, areca_packet_len);

  // ----- BEGIN TO SETUP HEADERS -------
  areca_packet[0] = 0x5E;
  areca_packet[1] = 0x01;
  areca_packet[2] = 0x61;
  areca_packet[3] = (unsigned char)((areca_packet_len - 6) & 0xff);
  areca_packet[4] = (unsigned char)(((areca_packet_len - 6) >> 8) & 0xff);
  areca_packet[5] = 0x1c;  // areca defined code for ATA passthrough command

  // ----- BEGIN TO SETUP PAYLOAD DATA -----
  memcpy(&areca_packet[7], "SmrT", 4);  // areca defined password
  ata_cmd = (sATA_INPUT_REGISTERS *)&areca_packet[12];

  // Set registers
  {
    const ata_in_regs & r = in.in_regs;
    ata_cmd->features      = r.features;
    ata_cmd->sector_count  = r.sector_count;
    ata_cmd->sector_number = r.lba_low;
    ata_cmd->cylinder_low  = r.lba_mid;
    ata_cmd->cylinder_high = r.lba_high;
    ata_cmd->device_head   = r.device;
    ata_cmd->command       = r.command;
  }
  bool readdata = false;
  if (in.direction == ata_cmd_in::data_in) {
      readdata = true;
      // the command will read data
      areca_packet[6] = 0x13;
  }
  else if ( in.direction == ata_cmd_in::no_data )
  {
    // the commands will return no data
    areca_packet[6] = 0x15;
  }
  else if (in.direction == ata_cmd_in::data_out)
  {
    // the commands will write data
    memcpy(ata_cmd->data, in.buffer, in.size);
    areca_packet[6] = 0x14;
  }
  else {
      // COMMAND NOT SUPPORTED VIA ARECA IOCTL INTERFACE
      return set_err(ENOSYS);
  }

  areca_packet[11] = get_disknum() - 1;  // disk#
  areca_packet[19] = get_encnum() - 1;   // enc#

  // ----- BEGIN TO SEND TO ARECA DRIVER ------
  expected = arcmsr_ui_handler(areca_packet, areca_packet_len, return_buff);
  if ( expected < 0 )
  {
    return set_err(EIO);
  }

  sATA_OUTPUT_REGISTERS *ata_out = (sATA_OUTPUT_REGISTERS *)&return_buff[5] ;
  if ( ata_out->status )
  {
    if ( in.in_regs.command == ATA_IDENTIFY_DEVICE
     && !nonempty((unsigned char *)in.buffer, in.size))
     {
        return set_err(ENODEV, "No drive on port %d", get_disknum());
     }
  }

  // returns with data
  if (readdata)
  {
    memcpy(in.buffer, &return_buff[7], in.size);
  }

  // Return register values
  {
    ata_out_regs & r = out.out_regs;
    r.error          = ata_out->error;
    r.sector_count   = ata_out->sector_count;
    r.lba_low        = ata_out->sector_number;
    r.lba_mid        = ata_out->cylinder_low;
    r.lba_high       = ata_out->cylinder_high;
    r.status         = ata_out->status;
  }
  return true;
}

bool generic_areca_device::arcmsr_scsi_pass_through(struct scsi_cmnd_io * iop)
{
  // Areca packet format for outgoing:
  // B[0~2] : 3 bytes header, fixed value 0x5E, 0x01, 0x61
  // B[3~4] : 2 bytes command length + variant data length, little endian
  // B[5]   : 1 bytes areca defined command code
  // B[6~last-1] : variant bytes payload data
  // B[last] : 1 byte checksum, simply sum(B[3] ~ B[last -1])
  //
  //
  //   header 3 bytes  length 2 bytes   cmd 1 byte    payload data x bytes  cs 1 byte
  // +--------------------------------------------------------------------------------+
  // + 0x5E 0x01 0x61 |   0x00 0x00   |     0x1c   | .................... |   0x00    |
  // +--------------------------------------------------------------------------------+
  //

  //Areca packet format for incoming:
  // B[0~2] : 3 bytes header, fixed value 0x5E, 0x01, 0x61
  // B[3~4] : 2 bytes payload length, little endian
  // B[5~last-1] : variant bytes returned payload data
  // B[last] : 1 byte checksum, simply sum(B[3] ~ B[last -1])
  //
  //
  //   header 3 bytes  length 2 bytes   payload data x bytes  cs 1 byte
  // +-------------------------------------------------------------------+
  // + 0x5E 0x01 0x61 |   0x00 0x00   | .................... |   0x00    |
  // +-------------------------------------------------------------------+
  unsigned char    areca_packet[640];
  int areca_packet_len = sizeof(areca_packet);
  unsigned char return_buff[2048];
  int expected = 0;

  if (iop->cmnd_len > 16) {
    set_err(EINVAL, "cmnd_len too large");
    return false;
  }

  memset(areca_packet, 0, areca_packet_len);

  // ----- BEGIN TO SETUP HEADERS -------
  areca_packet[0] = 0x5E;
  areca_packet[1] = 0x01;
  areca_packet[2] = 0x61;
  areca_packet[3] = (unsigned char)((areca_packet_len - 6) & 0xff);
  areca_packet[4] = (unsigned char)(((areca_packet_len - 6) >> 8) & 0xff);
  areca_packet[5] = 0x1c;

  // ----- BEGIN TO SETUP PAYLOAD DATA -----
  areca_packet[6] = 0x16; // scsi pass through
  memcpy(&areca_packet[7], "SmrT", 4);  // areca defined password
  areca_packet[12] = iop->cmnd_len; // cdb length
  memcpy( &areca_packet[35], iop->cmnd, iop->cmnd_len); // cdb
  areca_packet[15] = (unsigned char)iop->dxfer_len; // 15(LSB) ~ 18(MSB): data length ( max=512 bytes)
  areca_packet[16] = (unsigned char)(iop->dxfer_len >> 8);
  areca_packet[17] = (unsigned char)(iop->dxfer_len >> 16);
  areca_packet[18] = (unsigned char)(iop->dxfer_len >> 24);
  if(iop->dxfer_dir == DXFER_TO_DEVICE)
  {
    areca_packet[13] |= 0x01;
    memcpy(&areca_packet[67], iop->dxferp, iop->dxfer_len);
  }
  else if (iop->dxfer_dir == DXFER_FROM_DEVICE)
  {
  }
  else if( iop->dxfer_dir == DXFER_NONE)
  {
  }
  else {
    // COMMAND NOT SUPPORTED VIA ARECA IOCTL INTERFACE
    return set_err(ENOSYS);
  }

  areca_packet[11] = get_disknum() - 1;  // disk#
  areca_packet[19] = get_encnum() - 1;   // enc#

  // ----- BEGIN TO SEND TO ARECA DRIVER ------
  expected = arcmsr_ui_handler(areca_packet, areca_packet_len, return_buff);

  if (expected < 0)
    return set_err(EIO, "arcmsr_scsi_pass_through: I/O error");
  if (expected < 15) // 7 bytes if port is empty
    return set_err(EIO, "arcmsr_scsi_pass_through: missing data (%d bytes, expected %d)", expected, 15);

  int scsi_status = return_buff[5];
  int in_data_len = return_buff[11] | return_buff[12] << 8 | return_buff[13] << 16 | return_buff[14] << 24;

  if (iop->dxfer_dir == DXFER_FROM_DEVICE)
  {
    memset(iop->dxferp, 0, iop->dxfer_len); // need?
    memcpy(iop->dxferp, &return_buff[15], in_data_len);
  }

  if(scsi_status == 0xE1 /* Underrun, actual data length < requested data length */)
  {
      // don't care, just ignore
      scsi_status = 0x0;
  }

  if(scsi_status != 0x00 && scsi_status != SCSI_STATUS_CHECK_CONDITION)
  {
    return set_err(EIO);
  }

  if(scsi_status == SCSI_STATUS_CHECK_CONDITION)
  {
    // check condition
    iop->scsi_status = SCSI_STATUS_CHECK_CONDITION;
    iop->resp_sense_len = 4;
    iop->sensep[0] = return_buff[7];
    iop->sensep[1] = return_buff[8];
    iop->sensep[2] = return_buff[9];
    iop->sensep[3] = return_buff[10];
  }

  return true;
}

/////////////////////////////////////////////////////////////
areca_ata_device::areca_ata_device(smart_interface * intf, const char * dev_name, int disknum, int encnum)
: smart_device(intf, dev_name, "areca", "areca")
{
  set_encnum(encnum);
  set_disknum(disknum);
  set_info().info_name = strprintf("%s [areca_disk#%02d_enc#%02d]", dev_name, disknum, encnum);
}

areca_ata_device::~areca_ata_device() throw()
{

}

bool areca_ata_device::ata_pass_through(const ata_cmd_in & in, ata_cmd_out & out)
{
  if (!ata_cmd_is_supported(in,
    ata_device::supports_data_out |
    ata_device::supports_output_regs |
  //ata_device::supports_multi_sector | // TODO
    ata_device::supports_48bit_hi_null,
    "Areca")
  )
    return false;

  return arcmsr_ata_pass_through(in, out);
}

/////////////////////////////////////////////////////////////
areca_scsi_device::areca_scsi_device(smart_interface * intf, const char * dev_name, int disknum, int encnum)
: smart_device(intf, dev_name, "areca", "areca")
{
  set_encnum(encnum);
  set_disknum(disknum);
  set_info().info_name = strprintf("%s [areca_disk#%02d_enc#%02d]", dev_name, disknum, encnum);
}

areca_scsi_device::~areca_scsi_device() throw()
{

}

bool areca_scsi_device::scsi_pass_through(struct scsi_cmnd_io * iop)
{
  return arcmsr_scsi_pass_through(iop);
}




