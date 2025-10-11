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
ps3stor_channel * ps3stor_channel::s_channel;

ps3stor_errno ps3stor_channel::get_enclcount(unsigned hostid, uint8_t &enclcount)
{
    struct ps3stor_msg_info* reqinfo = NULL;
    size_t insize = sizeof(struct ps3stor_msg_info);
    reqinfo = (struct ps3stor_msg_info*)malloc(insize);
    memset(reqinfo, 0, insize); 
    reqinfo->length = insize;
    reqinfo->opcode = PS3STOR_CMD_ENCL_GET_COUNT;

    struct ps3stor_msg_info* rspinfo = NULL;
    size_t outsize = sizeof(struct ps3stor_msg_info) + sizeof(enclcount);
    rspinfo = (struct ps3stor_msg_info*)malloc(outsize);
    memset(rspinfo, 0, outsize);

    if (PS3STOR_ERRNO_SUCCESS != firecmd(hostid, reqinfo, rspinfo, outsize)) {
      free(reqinfo);
      free(rspinfo);
      return -1;
    }
    memcpy(&enclcount, rspinfo->body, sizeof(enclcount));
    free(reqinfo);
    free(rspinfo); 
    return 0;
}

ps3stor_errno ps3stor_channel::get_encllist(unsigned hostid, ps3stor_encl_list *&encllist, size_t listsize)
{
    struct ps3stor_msg_info* reqinfo = NULL;
    size_t insize = sizeof(struct ps3stor_msg_info);
    reqinfo = (struct ps3stor_msg_info*)malloc(insize);
    memset(reqinfo, 0, insize); 
    reqinfo->length = insize;
    reqinfo->opcode = PS3STOR_CMD_ENCL_GET_LIST;

    struct ps3stor_msg_info* rspinfo = NULL;
    size_t outsize = sizeof(struct ps3stor_msg_info) + listsize;
    rspinfo = (struct ps3stor_msg_info*)malloc(outsize);
    memset(rspinfo, 0, outsize);

    if (PS3STOR_ERRNO_SUCCESS != firecmd(hostid, reqinfo, rspinfo, outsize)) {
      free(reqinfo);
      free(rspinfo);
      return -1;
    }
    memcpy(encllist, rspinfo->body, listsize);
    free(reqinfo);
    free(rspinfo); 
    return 0;
}

ps3stor_errno ps3stor_channel::pd_get_devcount_by_encl(unsigned hostid, uint8_t enclid, uint16_t &devcount)
{
    struct ps3stor_msg_info* reqinfo = NULL;
    size_t insize = sizeof(struct ps3stor_msg_info);
    reqinfo = (struct ps3stor_msg_info*)malloc(insize);
    memset(reqinfo, 0, insize); 
    reqinfo->length = insize;
    reqinfo->opcode = PS3STOR_CMD_PD_GET_COUNT_IN_ENCL;
    reqinfo->id.type = PS3STOR_ID_GROUP_TYPE_PD_POSITION;
    reqinfo->id.pd_position.enclid = enclid;

    struct ps3stor_msg_info* rspinfo = NULL;
    size_t outsize = sizeof(struct ps3stor_msg_info) + sizeof(devcount);
    rspinfo = (struct ps3stor_msg_info*)malloc(outsize);
    memset(rspinfo, 0, outsize);

    if (PS3STOR_ERRNO_SUCCESS != firecmd(hostid, reqinfo, rspinfo, outsize)) {
        free(reqinfo);
        free(rspinfo);
        return -1;
    }
    memcpy(&devcount, rspinfo->body, sizeof(devcount));
    free(reqinfo);
    free(rspinfo); 
    return 0;
}

ps3stor_errno ps3stor_channel::pd_get_devlist_by_encl(unsigned hostid, uint8_t enclid, uint16_t *&devlist, size_t listsize)
{
  ps3stor_errno err = PS3STOR_ERRNO_SUCCESS;

  struct ps3stor_msg_info* reqinfo = NULL;
  size_t insize = sizeof(struct ps3stor_msg_info);
  reqinfo = (struct ps3stor_msg_info*)malloc(insize);
  memset(reqinfo, 0, insize); 
  reqinfo->length = insize;
  reqinfo->opcode = PS3STOR_CMD_PD_GET_DEV_LIST_IN_ENCL;

  struct ps3stor_msg_info* rspinfo = NULL;
  size_t outsize = sizeof(struct ps3stor_msg_info) + listsize;
  rspinfo = (struct ps3stor_msg_info*)malloc(outsize);
  memset(rspinfo, 0, outsize);
  reqinfo->id.type = PS3STOR_ID_GROUP_TYPE_PD_POSITION;
  reqinfo->id.pd_position.enclid = enclid;

  if (PS3STOR_ERRNO_SUCCESS != firecmd(hostid, reqinfo, rspinfo, outsize)) {
    free(reqinfo);
    free(rspinfo);
    return -1;
  }
  memcpy(devlist, rspinfo->body, listsize);
  free(reqinfo);
  free(rspinfo); 
  return err;
}

ps3stor_errno ps3stor_channel::pd_get_baseinfo_by_devid(unsigned hostid, unsigned devid, ps3stor_pd_baseinfo_t &baseinfo)
{
    baseinfo.enclid = 0;
    baseinfo.slotid = 0;
    ps3stor_batch_req_t *batchreq = NULL;
    struct ps3stor_tlv *req_tlv = NULL;
    uint32_t id_count = 1;
    uint32_t data_size = sizeof(ps3stor_pd_baseinfo_t);

    unsigned reqsize = sizeof(ps3stor_batch_req_t) + id_count * sizeof(struct ps3stor_id_group);
    batchreq = (ps3stor_batch_req_t*)malloc(reqsize);
    memset(batchreq, 0, reqsize);
    batchreq->idcount = id_count;
    batchreq->datasize = data_size;
    batchreq->idgroup[0].type = PS3STOR_ID_GROUP_TYPE_DEVICE_ID;
    batchreq->idgroup[0].deviceid = (uint16_t)devid;

    //packet batchreq

    req_tlv = add_tlv_data(req_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_BATCH_TLV_CODE_MASK, ps3stor_batch_req_t, idcount),
                                    &batchreq->idcount, (uint16_t)sizeof(batchreq->idcount));
    req_tlv = add_tlv_data(req_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_BATCH_TLV_CODE_MASK, ps3stor_batch_req_t, datasize),
                                    &batchreq->datasize, (uint16_t)sizeof(batchreq->datasize));
    req_tlv = add_tlv_data(req_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_BATCH_TLV_CODE_MASK, ps3stor_batch_req_t, idgroup),
                                    &batchreq->idgroup, (uint16_t)(id_count * sizeof(struct ps3stor_id_group)));
    if (req_tlv == NULL) {
        free(batchreq);
        return -1;
    }

    ps3stor_batch_rsp_t *batchrsp = NULL;
    unsigned rspsize = sizeof(ps3stor_batch_rsp_t) + id_count * (sizeof(ps3stor_rsp_entry_t) + sizeof(ps3stor_pd_baseinfo_t));                                                                  
    batchrsp = (ps3stor_batch_rsp_t*)malloc(rspsize);
    memset(batchrsp, 0, rspsize);

    // call ioc
    struct ps3stor_msg_info* reqinfo = NULL;
    size_t insize = sizeof(struct ps3stor_msg_info) + req_tlv->size;
    reqinfo = (struct ps3stor_msg_info*)malloc(insize);
    memset(reqinfo, 0, insize); 
    reqinfo->length = insize;
    reqinfo->opcode = PS3STOR_CMD_PD_BASE_INFO;
    memcpy(reqinfo->body, req_tlv->buff, req_tlv->size);

    struct ps3stor_msg_info* rspinfo = NULL;
    size_t outsize = sizeof(struct ps3stor_msg_info) + rspsize;
    rspinfo = (struct ps3stor_msg_info*)malloc(outsize);
    memset(rspinfo, 0, outsize);

    if (PS3STOR_ERRNO_SUCCESS != firecmd(hostid, reqinfo, rspinfo, outsize)) {
        free(batchreq);
        free(req_tlv);
        free(batchrsp);
        free(reqinfo);
        free(rspinfo);
        return -1;
    }
    memcpy(batchrsp, rspinfo->body, rspsize);
    ps3stor_rsp_entry_t* entry = (ps3stor_rsp_entry_t*)batchrsp->rsp_entry; 
    if (entry != NULL && entry->result == PS3STOR_ERRNO_SUCCESS) {
        memcpy(&baseinfo, entry->data, PS3STOR_MIN(entry->size, sizeof(ps3stor_pd_baseinfo_t)));
    } else {
        free(batchreq);
        free(req_tlv);
        free(batchrsp);
        free(reqinfo);
        free(rspinfo);
        return -1;
    }

    free(batchreq);
    free(req_tlv);
    free(batchrsp);
    free(reqinfo);
    free(rspinfo);
    return 0;
}


ps3stor_errno ps3stor_channel::pd_scsi_passthrough(unsigned hostid, uint8_t eid, uint16_t sid,
                                                   ps3stor_scsi_req_t &scsireq, ps3stor_scsi_rsp &scsirsp,
                                                   uint8_t *&scsidata, const size_t scsilen)
{
    // 1. scsi data align
    unsigned scsicount = scsilen / PS3STOR_SGL_SIZE;
    scsicount = (scsilen % PS3STOR_SGL_SIZE == 0) ? (scsicount) : ((scsicount + 1));
    struct ps3stor_data scsiblks[PS3STOR_SCSI_MAX_BLK_CNT] = {};
    if (scsicount > PS3STOR_SCSI_MAX_BLK_CNT) {
        set_err(EIO, "linux_ps3stor_device::scsi_cmd: request for scsi data is too large.");
        return -1;
    }

    for(unsigned i = 0; i < scsicount; i++) {
        unsigned size = PS3STOR_SGL_SIZE;
        if (i == scsicount - 1) {
            size = scsilen - i * PS3STOR_SGL_SIZE;
        }
        size = (size % PS3STOR_SCSI_ALIGN_SIZE == 0) ? (size) : ((size / PS3STOR_SCSI_ALIGN_SIZE + 1) * PS3STOR_SCSI_ALIGN_SIZE);
        scsiblks[i].pdata = malloc(size);
        memset(scsiblks[i].pdata, 0, size);
        scsiblks[i].size = size;
        scsireq.req.datalen += size;
    } 

    scsireq.req.sgecount = (scsireq.req.datalen + PS3STOR_SCSI_BYTE_PER_CMD - 1) / PS3STOR_SCSI_BYTE_PER_CMD;
    scsireq.req.sgeindex = PS3STOR_SCSI_SGE_INDEX_BASE;

    scsireq.req.id.type = PS3STOR_ID_GROUP_TYPE_PD_POSITION;
    scsireq.req.id.pd_position.enclid = eid;
    scsireq.req.id.pd_position.slotid = sid; 

    // 2. packet scsireq
    struct ps3stor_tlv *scsireq_tlv = NULL;
    scsireq_tlv = add_tlv_data(scsireq_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_SCSI_TLV_CODE_MASK, ps3stor_scsi_req_t, reserved), 
                                        &scsireq.reserved, (uint16_t)sizeof(scsireq.reserved));
    scsireq_tlv = add_tlv_data(scsireq_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_SCSI_TLV_CODE_MASK, ps3stor_scsi_req_t, cmddir), 
                                        &scsireq.cmddir, (uint16_t)sizeof(scsireq.cmddir));                                          
    scsireq_tlv = add_tlv_data(scsireq_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_SCSI_TLV_CODE_MASK, ps3stor_scsi_req_t, checklen), 
                                        &scsireq.checklen, (uint16_t)sizeof(scsireq.checklen));
    scsireq_tlv = add_tlv_data(scsireq_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_SCSI_TLV_CODE_MASK, ps3stor_scsi_req_t, cdb), 
                                        &scsireq.cdb, (uint16_t)PS3STOR_SCSI_CDB_LEN);
    scsireq_tlv = add_tlv_data(scsireq_tlv, PS3STOR_MK_TLV_CODE(PS3STOR_SCSI_TLV_CODE_MASK, ps3stor_scsi_req_t, req), 
                                        &scsireq.req, (uint16_t)sizeof(scsireq.req));
    if (scsireq_tlv == NULL) {
        for(unsigned i = 0; i < scsicount; i++) {
            free(scsiblks[i].pdata);
        }
        return -1;
    }

    // 3. call scsi passthrough 
    struct ps3stor_msg_info* reqinfo = NULL;
    size_t insize = sizeof(struct ps3stor_msg_info) + scsireq_tlv->size;
    reqinfo = (struct ps3stor_msg_info*)malloc(insize);
    memset(reqinfo, 0, insize); 
    reqinfo->length = insize;
    reqinfo->opcode = PS3STOR_CMD_PD_SCSI_PASSTHROUGH;
    memcpy(&reqinfo->id, &scsireq.req.id, sizeof(struct ps3stor_id_group));
    memcpy(reqinfo->body, scsireq_tlv->buff, scsireq_tlv->size);

    struct ps3stor_msg_info* rspinfo = NULL;
    size_t outsize = sizeof(struct ps3stor_msg_info) + sizeof( struct ps3stor_scsi_rsp);
    rspinfo = (struct ps3stor_msg_info*)malloc(outsize);
    memset(rspinfo, 0, outsize);

    if (PS3STOR_ERRNO_SUCCESS != firecmd_scsi(hostid, reqinfo, rspinfo, outsize, scsiblks, scsicount)) {
        for(unsigned i = 0; i < scsicount; i++) {
            free(scsiblks[i].pdata);
        }
        free(reqinfo);
        free(rspinfo); 
        free(scsireq_tlv);
        return -1;
    }

    // 4. copy and free memory
    memcpy(&scsirsp, rspinfo->body, sizeof(scsirsp));
    for(unsigned i = 0; i < scsicount; i++) {
        unsigned size = PS3STOR_SGL_SIZE;
        if (i == scsicount - 1) {
            size = scsilen - i * PS3STOR_SGL_SIZE;
        }
        memcpy(scsidata + i * PS3STOR_SGL_SIZE, scsiblks[i].pdata, size);
        free(scsiblks[i].pdata);
    }
    free(scsireq_tlv);
    free(reqinfo);
    free(rspinfo); 
    return 0;
}
