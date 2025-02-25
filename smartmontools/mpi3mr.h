/*
 * mpi3mr.h
 *
 * Home page of code is: http://www.smartmontools.org
 *
 * Copyright (C) 2025 Alexandra Löber <a.loeber@de.leaseweb.com>
 * Copyright (C) 2025 Andreas Pelger <a.pelger@de.leaseweb.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _MPI3MR_H_
#define _MPI3MR_H_
#include <stdint.h>
#include <stdio.h>

#define MPI3_DRV_CMD 0x01
#define MPI3_DRVBSG_OPCODE_ALLTGTDEVINFO 0x04

#define MPI3_MPT_CMD 0x02
#define MPI3_DEFAULT_SIZE 0x04
#define MPI3_SCSI_CDB_SENSE_MAX_SIZE 96
#define MPI3_SCSI_CDB_SENSE_MAX_LEN 96
#define MPI3_DEFAULT_FUNCTION 0x20
#define MPI3_DISK_SELECTOR_HIGH 0x03
#define MPI3_DISK_SELECTOR_LOW 0x01

#define MPI3_DEFAULT_CMD_TIMEOUT 0xb4
#define MPI3_BSG_APPEND_TIMEOUT_MS 600000

struct scsi_io_cmnd {
    uint8_t cmnd[16];
    size_t cmnd_len;
    int dxfer_dir;
    void *dxferp;
    size_t dxfer_len;
    uint8_t *sensep;
    size_t max_sense_len;
    unsigned timeout;
    size_t resp_sense_len;
    uint8_t scsi_status;
    int resid;
};

struct xfer_out {
    uint16_t host_tag ;
    uint8_t ioc_use_only02;
    uint8_t function;
    uint16_t ioc_use_only04;
    uint8_t ioc_use_only06;
    uint8_t msg_flags;
    uint16_t change_count;
    uint16_t disk_selector;
    uint8_t unsure[20];
    scsi_io_cmnd scsi_cmd;
};

struct entries_ {
    uint32_t type;
    uint32_t size;
};

struct mpi3mr_mpt_bsg_ioctl {
    uint8_t cmd;
    uint8_t rsvd[7];
    uint8_t ctrl_id;
    uint8_t padding1[1];
    uint16_t timeout;
    uint8_t padding2[4];
    uint8_t size;
    uint8_t padding3[7];
    entries_ entries[4];
    uint8_t padding4[8];
};

struct mpi3mr_device_map_info {
    uint16_t handle;
    uint16_t perst_id;
    uint32_t target_id;
    uint8_t bus_id;
    uint8_t rsvd1;
    uint16_t rsvd2;
};

#define MPI3_MAX_PD_NUM

struct mpi3mr_all_tgt_info {
    uint16_t    num_devices;
    uint16_t    rsvd1;
    uint32_t    rsvd2;
    struct mpi3mr_device_map_info dmi[32];
};

struct mpi3mr_drv_bsg_ioctl {
    uint8_t cmd;
    uint8_t rsvd[7];
    uint8_t ctrl_id;
    uint8_t opcode;
    uint16_t rsvd1;
    uint32_t rsvd2[4];
};

#endif