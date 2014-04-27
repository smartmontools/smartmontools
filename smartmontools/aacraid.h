/* aacraid.h
 * Copyright (C) 2014 Raghava Aditya <Raghava.Aditya@pmcs.com>
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

// Check windows
#if _WIN32 || _WIN64
#if _WIN64
  #define ENVIRONMENT64
#else
  #define ENVIRONMENT32
#endif
#endif

// Check GCC
#if __GNUC__
#if __x86_64__ || __ppc64__
  #define ENVIRONMENT64
#else
  #define ENVIRONMENT32
#endif
#endif

#define METHOD_BUFFERED 0
#define METHOD_NEITHER  3

#define CTL_CODE(function, method) ((4<< 16) | ((function) << 2) | (method) )

#define FSACTL_SEND_RAW_SRB  CTL_CODE(2067, METHOD_BUFFERED)

#define  SRB_FUNCTION_EXECUTE_SCSI 0X00

#define  SRB_DataIn      0x0040
#define  SRB_DataOut     0x0080
#define  SRB_NoDataXfer  0x0000

typedef struct {
  uint32_t lo32;
  uint32_t hi32;
  }  address64;

typedef struct {
  address64 addr64;
  uint32_t length;  /* Length. */
  }  user_sgentry64;

typedef struct {
  uint32_t addr32;
  uint32_t length;
  }  user_sgentry32;

typedef struct {
  uint32_t         count;
  user_sgentry64   sg64[1];
  }  user_sgmap64;

typedef struct {
  uint32_t         count;
  user_sgentry32   sg32[1];
  }  user_sgmap32;

typedef struct {
  uint32_t function;           //SRB_FUNCTION_EXECUTE_SCSI 0x00
  uint32_t channel;            //bus
  uint32_t id;                 //use the ID number this is wrong
  uint32_t lun;                //Logical unit number
  uint32_t timeout;
  uint32_t flags;              //Interesting stuff I must say
  uint32_t count;              // Data xfer size
  uint32_t retry_limit;        // We shall see
  uint32_t cdb_size;           // Length of CDB
  uint8_t  cdb[16];            // The actual cdb command
  user_sgmap64 sg64;           // pDatabuffer and address of Databuffer
  }  user_aac_srb64;

typedef struct {
  uint32_t function;           //SRB_FUNCTION_EXECUTE_SCSI 0x00
  uint32_t channel;            //bus
  uint32_t id;                 //use the ID number this is wrong
  uint32_t lun;                //Logical unit number
  uint32_t timeout;
  uint32_t flags;              //Interesting stuff I must say
  uint32_t count;              // Data xfer size
  uint32_t retry_limit;        // We shall see
  uint32_t cdb_size;           // Length of CDB
  uint8_t  cdb[16];            // The actual cdb command
  user_sgmap32 sg32;           // pDatabuffer and address of Databuffer
  }  user_aac_srb32;

typedef struct {
  uint32_t status;
  uint32_t srb_status;
  uint32_t scsi_status;
  uint32_t data_xfer_length;
  uint32_t sense_data_size;
  uint8_t  sense_data[30];
  }  user_aac_reply;
