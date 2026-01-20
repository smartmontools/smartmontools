/*
 * ps3stor.cpp
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2024 Hong Xu
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dev_ps3stor.h"
#include <memory>

namespace smartmon {

ps3stor_channel *ps3stor_channel::s_channel;

ps3stor_errno ps3stor_channel::get_enclcount(unsigned hostid, uint8_t &enclcount)
{
  ps3stor_msg_info reqinfo{};
  reqinfo.length = sizeof(reqinfo);
  reqinfo.opcode = PS3STOR_CMD_ENCL_GET_COUNT;

  size_t outsize = sizeof(ps3stor_msg_info) + sizeof(enclcount);
  std::unique_ptr<uint8_t[]> rspinfo_buf(new uint8_t[outsize]{});
  ps3stor_msg_info *rspinfo = reinterpret_cast<ps3stor_msg_info *>(rspinfo_buf.get());

  if (PS3STOR_ERRNO_SUCCESS != firecmd(hostid, &reqinfo, rspinfo, outsize))
    return -1;

  memcpy(&enclcount, rspinfo->body, sizeof(enclcount));
  return PS3STOR_ERRNO_SUCCESS;
}
ps3stor_errno ps3stor_channel::get_encllist(unsigned hostid, ps3stor_encl_list *&encllist, size_t listsize)
{
  ps3stor_msg_info reqinfo{};
  reqinfo.length = sizeof(reqinfo);
  reqinfo.opcode = PS3STOR_CMD_ENCL_GET_LIST;

  size_t outsize = sizeof(ps3stor_msg_info) + listsize;
  std::unique_ptr<uint8_t[]> rspinfo_buf(new uint8_t[outsize]{});
  ps3stor_msg_info *rspinfo = reinterpret_cast<ps3stor_msg_info *>(rspinfo_buf.get());

  if (PS3STOR_ERRNO_SUCCESS != firecmd(hostid, &reqinfo, rspinfo, outsize))
    return -1;

  memcpy(encllist, rspinfo->body, listsize);
  return PS3STOR_ERRNO_SUCCESS;
}

ps3stor_errno ps3stor_channel::pd_get_devcount_by_encl(unsigned hostid, uint8_t enclid, uint16_t &devcount)
{
  ps3stor_msg_info reqinfo{};
  reqinfo.length = sizeof(ps3stor_msg_info);
  reqinfo.opcode = PS3STOR_CMD_PD_GET_COUNT_IN_ENCL;
  reqinfo.id.type = PS3STOR_ID_GROUP_TYPE_PD_POSITION;
  reqinfo.id.pd_position.enclid = enclid;

  size_t outsize = sizeof(ps3stor_msg_info) + sizeof(devcount);
  std::unique_ptr<uint8_t[]> rspinfo_buf(new uint8_t[outsize]{});
  ps3stor_msg_info *rspinfo = reinterpret_cast<ps3stor_msg_info *>(rspinfo_buf.get());

  if (PS3STOR_ERRNO_SUCCESS != firecmd(hostid, &reqinfo, rspinfo, outsize))
    return -1;

  memcpy(&devcount, rspinfo->body, sizeof(devcount));
  return PS3STOR_ERRNO_SUCCESS;
}

ps3stor_errno ps3stor_channel::pd_get_devlist_by_encl(unsigned hostid, uint8_t enclid, uint16_t *&devlist, size_t listsize)
{
  ps3stor_msg_info reqinfo{};
  reqinfo.length = sizeof(reqinfo);
  reqinfo.opcode = PS3STOR_CMD_PD_GET_DEV_LIST_IN_ENCL;
  reqinfo.id.type = PS3STOR_ID_GROUP_TYPE_PD_POSITION;
  reqinfo.id.pd_position.enclid = enclid;

  size_t outsize = sizeof(ps3stor_msg_info) + listsize;
  std::unique_ptr<uint8_t[]> rspinfo_buf(new uint8_t[outsize]{});
  ps3stor_msg_info *rspinfo = reinterpret_cast<ps3stor_msg_info *>(rspinfo_buf.get());

  if (PS3STOR_ERRNO_SUCCESS != firecmd(hostid, &reqinfo, rspinfo, outsize))
    return -1;

  memcpy(devlist, rspinfo->body, listsize);
  return PS3STOR_ERRNO_SUCCESS;
}

ps3stor_errno ps3stor_channel::pd_get_baseinfo_by_devid(unsigned hostid, unsigned devid, ps3stor_pd_baseinfo_t &baseinfo)
{
  baseinfo.enclid = 0;
  baseinfo.slotid = 0;
  ps3stor_tlv *req_tlv = nullptr;
  uint32_t id_count = 1;
  uint32_t data_size = sizeof(ps3stor_pd_baseinfo_t);

  size_t reqsize = sizeof(ps3stor_batch_req_t) + id_count * sizeof(ps3stor_id_group);
  std::unique_ptr<uint8_t[]> batchreq_buf(new uint8_t[reqsize]{});
  ps3stor_batch_req_t *batchreq = reinterpret_cast<ps3stor_batch_req_t *>(batchreq_buf.get());

  batchreq->idcount = id_count;
  batchreq->datasize = data_size;
  batchreq->idgroup[0].type = PS3STOR_ID_GROUP_TYPE_DEVICE_ID;
  batchreq->idgroup[0].deviceid = (uint16_t)devid;

  // packet batchreq
  req_tlv = add_tlv_data(req_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_BATCH_TLV_CODE_MASK, ps3stor_batch_req_t, idcount),
                         &batchreq->idcount, (uint16_t)sizeof(batchreq->idcount));
  req_tlv = add_tlv_data(req_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_BATCH_TLV_CODE_MASK, ps3stor_batch_req_t, datasize),
                         &batchreq->datasize, (uint16_t)sizeof(batchreq->datasize));
  req_tlv = add_tlv_data(req_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_BATCH_TLV_CODE_MASK, ps3stor_batch_req_t, idgroup),
                         &batchreq->idgroup, (uint16_t)(id_count * sizeof(ps3stor_id_group)));
  if (req_tlv == nullptr)
    return -1;

  size_t rspsize = sizeof(ps3stor_batch_rsp_t) + id_count * (sizeof(ps3stor_rsp_entry_t) + sizeof(ps3stor_pd_baseinfo_t));
  std::unique_ptr<uint8_t[]> batchrsp_buf(new uint8_t[rspsize]{});
  ps3stor_batch_rsp_t *batchrsp = reinterpret_cast<ps3stor_batch_rsp_t *>(batchrsp_buf.get());

  // call ioc
  size_t insize = sizeof(ps3stor_msg_info) + req_tlv->size;
  std::unique_ptr<uint8_t[]> reqinfo_buf(new uint8_t[insize]{});
  ps3stor_msg_info *reqinfo = reinterpret_cast<ps3stor_msg_info *>(reqinfo_buf.get());
  reqinfo->length = insize;
  reqinfo->opcode = PS3STOR_CMD_PD_BASE_INFO;
  memcpy(reqinfo->body, req_tlv->buff, req_tlv->size);

  size_t outsize = sizeof(ps3stor_msg_info) + rspsize;
  std::unique_ptr<uint8_t[]> rspinfo_buf(new uint8_t[outsize]{});
  ps3stor_msg_info *rspinfo = reinterpret_cast<ps3stor_msg_info *>(rspinfo_buf.get());

  if (PS3STOR_ERRNO_SUCCESS != firecmd(hostid, reqinfo, rspinfo, outsize)) {
    free(req_tlv);
    return -1;
  }

  memcpy(batchrsp, rspinfo->body, rspsize);
  ps3stor_rsp_entry_t *entry = (ps3stor_rsp_entry_t *)batchrsp->rsp_entry;
  if (entry == nullptr || entry->result != PS3STOR_ERRNO_SUCCESS) {
    free(req_tlv);
    return -1;
  }

  memcpy(&baseinfo, entry->data, PS3STOR_MIN(entry->size, sizeof(ps3stor_pd_baseinfo_t)));

  free(req_tlv);
  return PS3STOR_ERRNO_SUCCESS;
}

ps3stor_errno ps3stor_channel::pd_scsi_passthrough(unsigned hostid, uint8_t eid, uint16_t sid,
                                                   ps3stor_scsi_req_t &scsireq, ps3stor_scsi_rsp &scsirsp,
                                                   uint8_t *&scsidata, const size_t scsilen)
{
  // 1. scsi data align
  unsigned scsicount = scsilen / PS3STOR_SGL_SIZE;
  scsicount = (scsilen % PS3STOR_SGL_SIZE == 0) ? (scsicount) : ((scsicount + 1));
  ps3stor_data scsiblks[PS3STOR_SCSI_MAX_BLK_CNT] = {};
  if (scsicount > PS3STOR_SCSI_MAX_BLK_CNT) {
    set_err(EIO, "linux_ps3stor_device::scsi_cmd: request for scsi data is too large.");
    return -1;
  }

  std::vector<std::unique_ptr<uint8_t[]>> scsi_buffers;
  scsi_buffers.reserve(scsicount);

  for (unsigned i = 0; i < scsicount; i++) {
    unsigned size = PS3STOR_SGL_SIZE;
    if (i == scsicount - 1) {
      size = scsilen - i * PS3STOR_SGL_SIZE;
    }

    size = (size % PS3STOR_SCSI_ALIGN_SIZE == 0) ? (size) : ((size / PS3STOR_SCSI_ALIGN_SIZE + 1) * PS3STOR_SCSI_ALIGN_SIZE);
    scsi_buffers.emplace_back(new uint8_t[size]{});
    scsiblks[i].pdata = scsi_buffers.back().get();
    scsiblks[i].size = size;
    scsireq.req.datalen += size;
  }

  scsireq.req.sgecount = (scsireq.req.datalen + PS3STOR_SCSI_BYTE_PER_CMD - 1) / PS3STOR_SCSI_BYTE_PER_CMD;
  scsireq.req.sgeindex = PS3STOR_SCSI_SGE_INDEX_BASE;

  scsireq.req.id.type = PS3STOR_ID_GROUP_TYPE_PD_POSITION;
  scsireq.req.id.pd_position.enclid = eid;
  scsireq.req.id.pd_position.slotid = sid;

  // 2. packet scsireq
  ps3stor_tlv *scsireq_tlv = nullptr;
  scsireq_tlv = add_tlv_data(scsireq_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_SCSI_TLV_CODE_MASK, ps3stor_scsi_req_t, count),
                             &scsireq.count, (uint16_t)sizeof(scsireq.count));
  scsireq_tlv = add_tlv_data(scsireq_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_SCSI_TLV_CODE_MASK, ps3stor_scsi_req_t, cmddir),
                             &scsireq.cmddir, (uint16_t)sizeof(scsireq.cmddir));
  scsireq_tlv = add_tlv_data(scsireq_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_SCSI_TLV_CODE_MASK, ps3stor_scsi_req_t, checklen),
                             &scsireq.checklen, (uint16_t)sizeof(scsireq.checklen));
  scsireq_tlv = add_tlv_data(scsireq_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_SCSI_TLV_CODE_MASK, ps3stor_scsi_req_t, cdb),
                             &scsireq.cdb, (uint16_t)PS3STOR_SCSI_CDB_LEN);
  scsireq_tlv = add_tlv_data(scsireq_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_SCSI_TLV_CODE_MASK, ps3stor_scsi_req_t, req),
                             &scsireq.req, (uint16_t)sizeof(scsireq.req));
  if (scsireq_tlv == nullptr)
    return -1;

  // 3. call scsi passthrough
  size_t insize = sizeof(ps3stor_msg_info) + scsireq_tlv->size;
  std::unique_ptr<uint8_t[]> reqinfo_buf(new uint8_t[insize]{});
  ps3stor_msg_info *reqinfo = reinterpret_cast<ps3stor_msg_info *>(reqinfo_buf.get());
  reqinfo->length = insize;
  reqinfo->opcode = PS3STOR_CMD_PD_SCSI_PASSTHROUGH;
  memcpy(&reqinfo->id, &scsireq.req.id, sizeof(ps3stor_id_group));
  memcpy(reqinfo->body, scsireq_tlv->buff, scsireq_tlv->size);

  size_t outsize = sizeof(ps3stor_msg_info) + sizeof(ps3stor_scsi_rsp);
  std::unique_ptr<uint8_t[]> rspinfo_buf(new uint8_t[outsize]{});
  ps3stor_msg_info *rspinfo = reinterpret_cast<ps3stor_msg_info *>(rspinfo_buf.get());

  if (PS3STOR_ERRNO_SUCCESS != firecmd_scsi(hostid, reqinfo, rspinfo, outsize, scsiblks, scsicount))  {
    free(scsireq_tlv);
    return -1;
  }

  // 4. copy and free memory
  memcpy(&scsirsp, rspinfo->body, sizeof(scsirsp));
  for (unsigned i = 0; i < scsicount; i++) {
    unsigned size = PS3STOR_SGL_SIZE;
    if (i == scsicount - 1) {
      size = scsilen - i * PS3STOR_SGL_SIZE;
    }
    memcpy(scsidata + i * PS3STOR_SGL_SIZE, scsiblks[i].pdata, size);
  }

  free(scsireq_tlv);
  return PS3STOR_ERRNO_SUCCESS;
}

} // namespace smartmon
