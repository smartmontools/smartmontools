/*
 * mpi3mr.h
 *
 *
 * Original Mpi3mr code:
 *  Copyright (C) 2025 Alexandra Löber <a.loeber@de.leaseweb.com>
 *  Copyright (C) 2025 Andreas Pelger <a.pelger@de.leaseweb.com>
 *  Copyright (C) 2025 Tranquillity Codes <tranquillitycodes@proton.me>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

/*****************************************************************************
 *              Common structure for Simple, Chain, and Last Chain           *
 *              scatter gather elements                                      *
 ****************************************************************************/
typedef struct _MPI3_SGE_COMMON
{
    uint64_t             Address;                           /* 0x00 */
    uint32_t             Length;                            /* 0x08 */
    uint8_t              Reserved0C[3];                     /* 0x0C */
    uint8_t              Flags;                             /* 0x0F */
} __attribute__ ((packed)) MPI3_SGE_COMMON, * PTR_MPI3_SGE_COMMON,
  MPI3_SGE_SIMPLE, * PTR_MPI3_SGE_SIMPLE,
  Mpi3SGESimple_t, * pMpi3SGESimple_t,
  MPI3_SGE_CHAIN, * PTR_MPI3_SGE_CHAIN,
  Mpi3SGEChain_t, * pMpi3SGEChain_t,
  MPI3_SGE_LAST_CHAIN, * PTR_MPI3_SGE_LAST_CHAIN,
  Mpi3SGELastChain_t, * pMpi3SGELastChain_t;
SMARTMON_ASSERT_SIZEOF(MPI3_SGE_COMMON, 16);

/*****************************************************************************
 *              Union of scatter gather elements                             *
 ****************************************************************************/
typedef union _MPI3_SGE_UNION
{
    MPI3_SGE_SIMPLE                 Simple;
    uint32_t                             Words[4];
} __attribute__ ((packed)) MPI3_SGE_UNION, * PTR_MPI3_SGE_UNION,
  Mpi3SGEUnion_t, * pMpi3SGEUnion_t;
SMARTMON_ASSERT_SIZEOF(MPI3_SGE_UNION, 16);

/**
 * @brief MPI reply buffer definition
 *
 */

typedef struct _SL8_MPI_REPLY_BUF{
    uint8_t mpiRepType;
    uint8_t reserved[3];
} __attribute__ ((packed)) SL8_MPI_REPLY_BUF;
SMARTMON_ASSERT_SIZEOF(SL8_MPI_REPLY_BUF, 4);

/*****************************************************************************
 *              SCSI IO Error Reply Message                                  *
 ****************************************************************************/
typedef struct _MPI3_SCSI_IO_REPLY
{
    uint16_t                     HostTag;                        /* 0x00 */
    uint8_t                      IOCUseOnly02;                   /* 0x02 */
    uint8_t                      Function;                       /* 0x03 */
    uint16_t                     IOCUseOnly04;                   /* 0x04 */
    uint8_t                      IOCUseOnly06;                   /* 0x06 */
    uint8_t                      MsgFlags;                       /* 0x07 */
    uint16_t                     IOCUseOnly08;                   /* 0x08 */
    uint16_t                     IOCStatus;                      /* 0x0A */
    uint32_t                     IOCLogInfo;                     /* 0x0C */
    uint8_t                      SCSIStatus;                     /* 0x10 */
    uint8_t                      SCSIState;                      /* 0x11 */
    uint16_t                     DevHandle;                      /* 0x12 */
    uint32_t                     TransferCount;                  /* 0x14 */
    uint32_t                     SenseCount;                     /* 0x18 */
    uint32_t                     ResponseData;                   /* 0x1C */
    uint16_t                     TaskTag;                        /* 0x20 */
    uint16_t                     SCSIStatusQualifier;            /* 0x22 */
    uint32_t                     EEDPErrorOffset;                /* 0x24 */
    uint16_t                     EEDPObservedAppTag;             /* 0x28 */
    uint16_t                     EEDPObservedGuard;              /* 0x2A */
    uint32_t                     EEDPObservedRefTag;             /* 0x2C */
    uint64_t                     SenseDataBufferAddress;         /* 0x30 */
} __attribute__ ((packed)) MPI3_SCSI_IO_REPLY, * PTR_MPI3_SCSI_IO_REPLY,
  Mpi3SCSIIOReply_t, * pMpi3SCSIIOReply_t;
SMARTMON_ASSERT_SIZEOF(MPI3_SCSI_IO_REPLY, 56);

/**
 * struct mpi3mr_bsg_drv_cmd -  Generic bsg data
 * structure for all driver specific requests.
 *
 * @mrioc_id: Controller ID
 * @opcode: Driver specific opcode
 * @rsvd1: Reserved
 * @rsvd2: Reserved
 */
struct mpi3mr_bsg_drv_cmd {
    uint8_t    mrioc_id;
    uint8_t    opcode;
    uint16_t   rsvd1;
    uint32_t   rsvd2[4];
};
SMARTMON_ASSERT_SIZEOF(mpi3mr_bsg_drv_cmd, 20);

/**
 * struct mpi3mr_bsg_in_reply_buf - MPI reply buffer returned
 * for MPI Passthrough request .
 *
 * @mpi_reply_type: Type of MPI reply
 * @rsvd1: Reserved
 * @rsvd2: Reserved
 * @reply_buf: Variable Length buffer based on mpirep type
 */
struct mpi3mr_bsg_in_reply_buf {
    uint8_t    mpi_reply_type;
    uint8_t    rsvd1;
    uint16_t   rsvd2;
    uint8_t    reply_buf[];
};
SMARTMON_ASSERT_SIZEOF(mpi3mr_bsg_in_reply_buf, 4);

/**
 * struct mpi3mr_buf_entry - User buffer descriptor for MPI
 * Passthrough requests.
 *
 * @buf_type: Buffer type
 * @rsvd1: Reserved
 * @rsvd2: Reserved
 * @buf_len: Buffer length
 */
struct mpi3mr_buf_entry {
    uint8_t    buf_type;
    uint8_t    rsvd1;
    uint16_t   rsvd2;
    uint32_t   buf_len;
};
SMARTMON_ASSERT_SIZEOF(mpi3mr_buf_entry, 8);

/**
 * struct mpi3mr_bsg_buf_entry_list - list of user buffer
 * descriptor for MPI Passthrough requests.
 *
 * @num_of_entries: Number of buffer descriptors
 * @rsvd1: Reserved
 * @rsvd2: Reserved
 * @rsvd3: Reserved
 * @buf_entry: Variable length array of buffer descriptors
 */
struct mpi3mr_buf_entry_list {
    uint8_t    num_of_entries;
    uint8_t    rsvd1;
    uint16_t   rsvd2;
    uint32_t   rsvd3;
    struct mpi3mr_buf_entry buf_entry[1];
};
SMARTMON_ASSERT_SIZEOF(mpi3mr_buf_entry_list, 16);

/**
 * struct mpi3mr_bsg_mptcmd -  Generic bsg data
 * structure for all MPI Passthrough requests .
 *
 * @mrioc_id: Controller ID
 * @rsvd1: Reserved
 * @timeout: MPI request timeout
 * @buf_entry_list: Buffer descriptor list
 */
struct mpi3mr_bsg_mptcmd {
    uint8_t    mrioc_id;
    uint8_t    rsvd1;
    uint16_t   timeout;
    uint32_t   rsvd2;
    struct mpi3mr_buf_entry_list buf_entry_list;
};
SMARTMON_ASSERT_SIZEOF(mpi3mr_bsg_mptcmd, 24);

/**
 * struct mpi3mr_bsg_packet -  Generic bsg data
 * structure for all supported requests .
 *
 * @cmd_type: represents drvrcmd or mptcmd
 * @rsvd1: Reserved
 * @rsvd2: Reserved
 * @drvrcmd: driver request structure
 * @mptcmd: mpt request structure
 */
struct mpi3mr_bsg_packet {
    uint8_t    cmd_type;
    uint8_t    rsvd1;
    uint16_t   rsvd2;
    uint32_t   rsvd3;
    union {
        struct mpi3mr_bsg_drv_cmd drvrcmd;
        struct mpi3mr_bsg_mptcmd mptcmd;
    } cmd;
};
SMARTMON_ASSERT_SIZEOF(mpi3mr_bsg_packet, 32);

/**
 * struct mpi3mr_device_map_info - Target device mapping
 * information
 *
 * @handle: Firmware device handle
 * @perst_id: Persistent ID assigned by the firmware
 * @target_id: Target ID assigned by the driver
 * @bus_id: Bus ID assigned by the driver
 * @rsvd1: Reserved
 * @rsvd2: Reserved
 */
struct mpi3mr_device_map_info {
    uint16_t   handle;
    uint16_t   perst_id;
    uint32_t   target_id;
    uint8_t    bus_id;
    uint8_t    rsvd1;
    uint16_t   rsvd2;
};
SMARTMON_ASSERT_SIZEOF(mpi3mr_device_map_info, 12);

/**
 * struct mpi3mr_all_tgt_info - Target device mapping
 * information returned by the driver
 *
 * @num_devices: The number of devices in driver's inventory
 * @rsvd1: Reserved
 * @rsvd2: Reserved
 * @dmi: Variable length array of mapping information of targets
 */
struct mpi3mr_all_tgt_info {
    uint16_t   num_devices;
    uint16_t   rsvd1;
    uint32_t   rsvd2;
    struct mpi3mr_device_map_info dmi[1];
};
SMARTMON_ASSERT_SIZEOF(mpi3mr_all_tgt_info, 20);

struct mpi3_scsi_io_cdb_eedp32 {
    uint8_t                 cdb[20];
    uint32_t             primary_reference_tag;
    uint16_t             primary_application_tag;
    uint16_t             primary_application_tag_mask;
    uint32_t             transfer_length;
} __attribute__ ((packed));
SMARTMON_ASSERT_SIZEOF(mpi3_scsi_io_cdb_eedp32, 32);

struct mpi3_sge_common {
    uint64_t             address;
    uint32_t             length;
    uint8_t                 reserved0c[3];
    uint8_t                 flags;
} __attribute__ ((packed));
SMARTMON_ASSERT_SIZEOF(mpi3_sge_common, 16);

union mpi3_scsi_io_cdb_union {
    uint8_t                         cdb32[32];
    struct mpi3_scsi_io_cdb_eedp32 eedp32;
    struct mpi3_sge_common         sge;
} __attribute__ ((packed));
SMARTMON_ASSERT_SIZEOF(mpi3_scsi_io_cdb_union, 32);

struct mpi3_scsi_io_request {
    uint16_t                     host_tag;
    uint8_t                         ioc_use_only02;
    uint8_t                         function;
    uint16_t                     ioc_use_only04;
    uint8_t                         ioc_use_only06;
    uint8_t                         msg_flags;
    uint16_t                     change_count;
    uint16_t                     dev_handle;
    uint32_t                     flags;
    uint32_t                     skip_count;
    uint32_t                     data_length;
    uint8_t                         lun[8];
    union mpi3_scsi_io_cdb_union  cdb;
    MPI3_SGE_UNION          sgl[4];
} __attribute__ ((packed));

#define MPI3_SCSIIO_MSGFLAGS_METASGL_VALID                  (0x80)
#define MPI3_SCSIIO_MSGFLAGS_DIVERT_TO_FIRMWARE             (0x40)

#define MPI3_SCSIIO_FLAGS_LARGE_CDB                         (0x60000000)
#define MPI3_SCSIIO_FLAGS_CDB_16_OR_LESS                    (0x00000000)
#define MPI3_SCSIIO_FLAGS_CDB_GREATER_THAN_16               (0x20000000)
#define MPI3_SCSIIO_FLAGS_CDB_IN_SEPARATE_BUFFER            (0x40000000)
#define MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_MASK                (0x07000000)
#define MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_SIMPLEQ             (0x00000000)
#define MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_HEADOFQ             (0x01000000)
#define MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_ORDEREDQ            (0x02000000)
#define MPI3_SCSIIO_FLAGS_TASKATTRIBUTE_ACAQ                (0x04000000)
#define MPI3_SCSIIO_FLAGS_CMDPRI_MASK                       (0x00f00000)
#define MPI3_SCSIIO_FLAGS_CMDPRI_SHIFT                      (20)
#define MPI3_SCSIIO_FLAGS_DATADIRECTION_MASK                (0x000c0000)
#define MPI3_SCSIIO_FLAGS_DATADIRECTION_NO_DATA_TRANSFER    (0x00000000)
#define MPI3_SCSIIO_FLAGS_DATADIRECTION_WRITE               (0x00040000)
#define MPI3_SCSIIO_FLAGS_DATADIRECTION_READ                (0x00080000)
#define MPI3_SCSIIO_FLAGS_DMAOPERATION_MASK                 (0x00030000)
#define MPI3_SCSIIO_FLAGS_DMAOPERATION_HOST_PI              (0x00010000)
#define MPI3_SCSIIO_FLAGS_DIVERT_REASON_MASK                (0x000000f0)
#define MPI3_SCSIIO_FLAGS_DIVERT_REASON_IO_THROTTLING       (0x00000010)
#define MPI3_SCSIIO_FLAGS_DIVERT_REASON_WRITE_SAME_TOO_LARGE (0x00000020)
#define MPI3_SCSIIO_FLAGS_DIVERT_REASON_PROD_SPECIFIC       (0x00000080)

struct mpi3_scsi_io_reply {
    uint16_t                     host_tag;
    uint8_t                         ioc_use_only02;
    uint8_t                         function;
    uint16_t                     ioc_use_only04;
    uint8_t                         ioc_use_only06;
    uint8_t                         msg_flags;
    uint16_t                     ioc_use_only08;
    uint16_t                     ioc_status;
    uint32_t                     ioc_log_info;
    uint8_t                         scsi_status;
    uint8_t                         scsi_state;
    uint16_t                     dev_handle;
    uint32_t                     transfer_count;
    uint32_t                     sense_count;
    uint32_t                     response_data;
    uint16_t                     task_tag;
    uint16_t                     scsi_status_qualifier;
    uint32_t                     eedp_error_offset;
    uint16_t                     eedp_observed_app_tag;
    uint16_t                     eedp_observed_guard;
    uint32_t                     eedp_observed_ref_tag;
    uint64_t                     sense_data_buffer_address;
} __attribute__ ((packed));
SMARTMON_ASSERT_SIZEOF(mpi3_scsi_io_reply, 56);


/* Definitions for BSG commands */
#define MPI3MR_IOCTL_VERSION            0x06

#define MPI3MR_APP_DEFAULT_TIMEOUT      (60) /*seconds*/

#define MPI3MR_BSG_ADPTYPE_UNKNOWN          0
#define MPI3MR_BSG_ADPTYPE_AVGFAMILY        1

#define MPI3MR_BSG_ADPSTATE_UNKNOWN         0
#define MPI3MR_BSG_ADPSTATE_OPERATIONAL     1
#define MPI3MR_BSG_ADPSTATE_FAULT           2
#define MPI3MR_BSG_ADPSTATE_IN_RESET        3
#define MPI3MR_BSG_ADPSTATE_UNRECOVERABLE   4

#define MPI3MR_BSG_ADPRESET_UNKNOWN         0
#define MPI3MR_BSG_ADPRESET_SOFT            1
#define MPI3MR_BSG_ADPRESET_DIAG_FAULT      2

#define MPI3MR_BSG_LOGDATA_MAX_ENTRIES      400
#define MPI3MR_BSG_LOGDATA_ENTRY_HEADER_SZ  4

#define MPI3MR_DRVBSG_OPCODE_UNKNOWN                0
#define MPI3MR_DRVBSG_OPCODE_ADPINFO                1
#define MPI3MR_DRVBSG_OPCODE_ADPRESET               2
#define MPI3MR_DRVBSG_OPCODE_ALLTGTDEVINFO          4
#define MPI3MR_DRVBSG_OPCODE_GETCHGCNT              5
#define MPI3MR_DRVBSG_OPCODE_LOGDATAENABLE          6
#define MPI3MR_DRVBSG_OPCODE_PELENABLE              7
#define MPI3MR_DRVBSG_OPCODE_GETLOGDATA             8
#define MPI3MR_DRVBSG_OPCODE_QUERY_HDB              9
#define MPI3MR_DRVBSG_OPCODE_REPOST_HDB             10
#define MPI3MR_DRVBSG_OPCODE_UPLOAD_HDB             11
#define MPI3MR_DRVBSG_OPCODE_REFRESH_HDB_TRIGGERS   12


#define MPI3MR_BSG_BUFTYPE_UNKNOWN          0
#define MPI3MR_BSG_BUFTYPE_RAIDMGMT_CMD     1
#define MPI3MR_BSG_BUFTYPE_RAIDMGMT_RESP    2
#define MPI3MR_BSG_BUFTYPE_DATA_IN          3
#define MPI3MR_BSG_BUFTYPE_DATA_OUT         4
#define MPI3MR_BSG_BUFTYPE_MPI_REPLY        5
#define MPI3MR_BSG_BUFTYPE_ERR_RESPONSE     6
#define MPI3MR_BSG_BUFTYPE_MPI_REQUEST      0xFE

#define MPI3MR_BSG_MPI_REPLY_BUFTYPE_UNKNOWN    0
#define MPI3MR_BSG_MPI_REPLY_BUFTYPE_STATUS     1
#define MPI3MR_BSG_MPI_REPLY_BUFTYPE_ADDRESS    2

#define MPI3MR_HDB_BUFTYPE_UNKNOWN      0
#define MPI3MR_HDB_BUFTYPE_TRACE        1
#define MPI3MR_HDB_BUFTYPE_FIRMWARE     2
#define MPI3MR_HDB_BUFTYPE_RESERVED     3

#define MPI3MR_HDB_BUFSTATUS_UNKNOWN            0
#define MPI3MR_HDB_BUFSTATUS_NOT_ALLOCATED      1
#define MPI3MR_HDB_BUFSTATUS_POSTED_UNPAUSED    2
#define MPI3MR_HDB_BUFSTATUS_POSTED_PAUSED      3
#define MPI3MR_HDB_BUFSTATUS_RELEASED           4

#define MPI3MR_HDB_TRIGGER_TYPE_UNKNOWN     0
#define MPI3MR_HDB_TRIGGER_TYPE_DIAGFAULT   1
#define MPI3MR_HDB_TRIGGER_TYPE_ELEMENT     2
#define MPI3MR_HDB_TRIGGER_TYPE_MASTER      3

#define MPI3_FUNCTION_MGMT_PASSTHROUGH              (0x0a)
#define MPI3_FUNCTION_SCSI_IO                       (0x20)

/* Supported BSG commands */
enum command {
    MPI3MR_DRV_CMD = 1,
    MPI3MR_MPT_CMD = 2,
};
