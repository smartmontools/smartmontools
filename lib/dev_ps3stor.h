/*
 * ps3stor.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2024 Hong Xu
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __PS3STOR_H__
#define __PS3STOR_H__

#include <smartmon/utility.h>

#include <stdint.h>
#include <string>
#include <vector>
#include <stddef.h>

namespace smartmon {

typedef uint8_t                    encl_id_t;
typedef uint16_t                   slot_id_t;
typedef int32_t                    ps3stor_errno;

#define PS3STOR_ERRNO_SUCCESS           (0)
#define PS3STOR_ID_LIST_MAX_COUNT       (128)
#define PS3STOR_SCSI_STATUS_UNDERRUN    (140)
#define PS3STOR_MSG_MAGIC_CODE          (0x12345678)
#define PS3STOR_SGL_SIZE                (4 * 1024)
#define PS3STOR_ITR_VER                 (0x2000000)
#define PS3STOR_SMARTCTL_SERVICE        (0)
#define PS3STOR_SCSI_CDB_LEN            (32)
#define PS3STOR_SCSI_MAX_CDB_LENGTH     (32)
#define PS3STOR_SCSI_SENSE_BUFFER_LEN   (96)
#define PS3STOR_SCSI_MAX_BLK_CNT        (14)
#define PS3STOR_SCSI_ALIGN_SIZE         (512)
#define PS3STOR_SCSI_BYTE_PER_CMD       (4 * 1024)
#define PS3STOR_SCSI_SGE_INDEX_BASE     (2)

#define  PS3STOR_DRIVE_DEVICE_ID                 "/proc/devices"
#define  PS3STOR_SYS_CLASS_SCSI_HOST_PATH        "/sys/class/scsi_host/"
#define  PS3STOR_SYS_BUS_PCI_DEVICE              "/sys/bus/pci/devices"

#define PS3STOR_CMD_ENCL_GET_COUNT              (0x3040101)
#define PS3STOR_CMD_ENCL_GET_LIST               (0x3040102)
#define PS3STOR_CMD_PD_GET_COUNT_IN_ENCL        (0x3050101)
#define PS3STOR_CMD_PD_GET_DEV_LIST_IN_ENCL     (0x3050115)
#define PS3STOR_CMD_PD_SCSI_PASSTHROUGH         (0x23051c01)
#define PS3STOR_CMD_PD_BASE_INFO                (0x3050104)

#define PS3STOR_SCSI_TLV_CODE_MASK              (0x41c01000)
#define PS3STOR_BATCH_TLV_CODE_MASK             (0x6000)

#define PS3STOR_MIN(x,y)                ((x) < (y) ? (x) : (y))
#define PS3STOR_MK_TLV_CODE(code, struct_name, struct_member) \
        ((uint32_t) (code | offsetof(struct_name, struct_member)))

#define PS3STOR_ID_GROUP_TYPE_UNKNOWN       (0)
#define PS3STOR_ID_GROUP_TYPE_DEVICE_ID     (1)
#define PS3STOR_ID_GROUP_TYPE_PD_POSITION   (2)
#define PS3STOR_SCSI_CMD_DIR_FROM_HOST      (1)
#define PS3STOR_SCSI_CMD_DIR_TO_HOST        (2)
#define PS3STOR_SCSI_CMD_DIR_NONE           (0)

#define PS3STOR_INVALID_U64         (0xFFFFFFFFFFFFFFFF)
#define PS3STOR_INVALID_U32         (0xFFFFFFFF)
#define PS3STOR_INVALID_U16         (0xFFFF)
#define PS3STOR_INVALID_U8          (0xFF)

#define PS3STOR_INVALID_ENCL_ID     (PS3STOR_INVALID_U8)
#define PS3STOR_INVALID_SLOT_ID     (PS3STOR_INVALID_U16)
struct ps3stor_pd_position
{
  uint8_t enclid;
  uint8_t pad;
  uint16_t slotid;
};

struct ps3stor_id_group
{
  uint8_t type;
  uint8_t pad[7];
  union
  {
    uint16_t deviceid;
    struct ps3stor_pd_position pd_position;
    uint64_t reserved; // align as 8 bytes
    uint8_t value[24]; // max for 24 bytes
  };
};

struct ps3stor_tlv
{
  int32_t size;
  uint8_t buff[0];
};

struct ps3stor_encl_list
{
  uint8_t count;
  uint8_t idlist[0];
};

struct ps3stor_msg_info
{
  uint32_t magic;
  uint32_t opcode;
  uint32_t error;
  uint32_t timeout;
  uint32_t start_time;
  uint32_t runver;
  uint32_t length;
  uint32_t ack_offset;
  uint32_t ack_length;
  uint32_t leftmsg_count;
  uint32_t msg_index;
  uint8_t reserved[4];
  uint64_t uuid;
  uint8_t service;
  struct
  {
    uint8_t ack : 4;
    uint8_t tlv : 4;
  } index;
  uint8_t funcid : 1;
  uint8_t pad : 7;
  uint8_t reserved2[5];
  struct ps3stor_id_group id;
  uint64_t traceid;
  uint8_t body[0];
};

#define PS3STOR_MSG_INFO_SIZE (sizeof(struct ps3stor_msg_info))

struct ps3stor_sge
{
  uint64_t addr;
  uint32_t length;
  uint32_t reserved1 : 30;
  uint32_t last_sge : 1;
  uint32_t ext : 1;
};

struct ps3stor_ioctl_sync_cmd
{
  uint16_t hostid;
  uint16_t sgl_offset;
  uint16_t sge_count;
  uint16_t reserved1;
  uint32_t result_code;
  uint8_t reserved2[100];
  uint64_t traceid;
  uint8_t reserved3[120];
  uint8_t reserved4[128];
  struct ps3stor_sge sgl[16];
};

#define PS3STOR_CMD_IOCTL_SYNC_CMD _IOWR('M', 1, struct ps3stor_ioctl_sync_cmd)

typedef struct
{
  uint32_t count;
  uint8_t pad[4];
  int64_t values[PS3STOR_ID_LIST_MAX_COUNT];
} ps3stor_id_list;

struct ps3stor_pci_info
{
  uint32_t domainid;
  uint8_t busid;
  uint8_t deviceid;
  uint8_t function;
  uint8_t devtype;
  char pci_addr[32];
};

struct ps3stor_data
{
  void *pdata;
  uint32_t size;
};

struct ps3stor_scsi_info
{
  uint32_t datalen;
  uint32_t sgecount;
  uint32_t sgeindex;
  uint8_t pad[4];
  struct ps3stor_id_group id;
};

typedef struct ps3stor_scsi_req
{
  uint32_t count;
  uint8_t cmddir;
  uint8_t checklen;
  uint8_t pad[2];
  uint8_t cdb[PS3STOR_SCSI_CDB_LEN];
  struct ps3stor_scsi_info req;
} ps3stor_scsi_req_t;

struct ps3stor_scsi_rsp_entry
{
  uint8_t sensebuf[PS3STOR_SCSI_SENSE_BUFFER_LEN];
  uint8_t status;
  uint8_t pad1[3];
  uint32_t xfercnt;
  uint8_t pad2[4];
  uint32_t result;
};

struct ps3stor_scsi_rsp
{
  uint32_t reserved;
  struct ps3stor_scsi_rsp_entry entry;
};

typedef struct ps3stor_pd_baseinfo
{
  uint16_t deviceid;
  uint8_t reserved1[2];
  uint8_t enclid;
  uint8_t reserved2;
  uint16_t slotid;
  uint64_t reserved[22];
} ps3stor_pd_baseinfo_t;

typedef struct ps3stor_batch_req
{
  uint32_t idcount;
  uint32_t datasize;
  struct ps3stor_id_group idgroup[0];
} ps3stor_batch_req_t;

typedef struct ps3stor_rsp_entry
{
  int32_t result;
  uint32_t size;
  uint8_t data[0];
} ps3stor_rsp_entry_t;

typedef struct ps3stor_batch_rsp
{
  uint32_t count;
  uint8_t rsp_entry[0]; // ps3stor_rsp_entry_t
} ps3stor_batch_rsp_t;

bool ps3stor_init();
/////////////////////////////////////////////////////////////////////////////
// ps3stor_channel

/// The platform ps3stor channel abstraction
class ps3stor_channel
{
public:
  /// Initialize platform channel and register with ps3chn().
  /// Must be implemented by platform module and register channel with set()
  static void init();

  ps3stor_channel()
  {
    m_init = false;
  }

  virtual ~ps3stor_channel()
  {
  }

  /// Error (number,message) pair
  struct error_info
  {
    explicit error_info(int n = 0)
        : no(n) {}
    error_info(int n, const char *m)
        : no(n), msg(m) {}
    void clear()
    {
      no = 0;
      msg.erase();
    }

    int no;          ///< Error number
    std::string msg; ///< Error message
  };

protected:
  /// Set channel to use, must be called from init().
  static void set(ps3stor_channel *channel)
  {
    s_channel = channel;
  }

public:
  virtual ps3stor_errno channel_init() = 0;

  virtual ps3stor_errno get_host_list(std::vector<unsigned> &hostlist) = 0;

  ps3stor_errno get_enclcount(unsigned hostid, uint8_t &enclcount);

  ps3stor_errno get_encllist(unsigned hostid, ps3stor_encl_list *&encllist, size_t listsize);

  ps3stor_errno pd_get_devcount_by_encl(unsigned hostid, uint8_t enclid, uint16_t &devcount);

  ps3stor_errno pd_get_devlist_by_encl(unsigned hostid, uint8_t enclid, uint16_t *&devlist, size_t listsize);

  ps3stor_errno pd_get_baseinfo_by_devid(unsigned hostid, unsigned devid, ps3stor_pd_baseinfo_t &basinfo);

  ps3stor_errno pd_scsi_passthrough(unsigned hostid, uint8_t eid, uint16_t sid,
                                    ps3stor_scsi_req_t &scsireq, ps3stor_scsi_rsp &scsirsp,
                                    uint8_t *&scsidata, const size_t scsilen);

protected:
  virtual ps3stor_errno firecmd(unsigned hostid, ps3stor_msg_info *info, ps3stor_msg_info *ackinfo, unsigned acksize) = 0;

  virtual ps3stor_errno firecmd_scsi(unsigned hostid, ps3stor_msg_info *reqinfo, ps3stor_msg_info *ackinfo,
                                     unsigned acksize, ps3stor_data *scsidata, unsigned scsicount) = 0;

  virtual ps3stor_tlv *add_tlv_data(ps3stor_tlv *tlv, unsigned type, const void *data, uint16_t size) = 0;
  ///////////////////////////////////////////////
  // Last error information
  /// Set last error number and message.
  /// Returns false always to allow use as a return expression.
  virtual bool set_err(int no, const char *msg) = 0;

  /// Set last error number and default message.
  /// Message is retrieved from interface's get_msg_for_errno(no).
  virtual bool set_err(int no) = 0;

  bool m_init;

private:
  friend ps3stor_channel *ps3chn();  // below
  static ps3stor_channel *s_channel; ///< Pointer to the channel object.

  // Prevent copy/assignment
  ps3stor_channel(const ps3stor_channel &);
  void operator=(const ps3stor_channel &);
};

/// Global access to the (usually singleton) ps3stor_channel
inline ps3stor_channel *ps3chn()
{
  return ps3stor_channel::s_channel;
}

} // namespace smartmon

#endif
