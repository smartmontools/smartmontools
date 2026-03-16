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

#undef u32

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t


#define MPI3_POINTER    *

/*****************************************************************************
 *              Common structure for Simple, Chain, and Last Chain           *
 *              scatter gather elements                                      *
 ****************************************************************************/
typedef struct _MPI3_SGE_COMMON
{
    u64             Address;                           /* 0x00 */
    u32             Length;                            /* 0x08 */
    u8              Reserved0C[3];                     /* 0x0C */
    u8              Flags;                             /* 0x0F */
} __attribute__ ((packed)) MPI3_SGE_SIMPLE, MPI3_POINTER PTR_MPI3_SGE_SIMPLE,
  Mpi3SGESimple_t, MPI3_POINTER pMpi3SGESimple_t,
  MPI3_SGE_CHAIN, MPI3_POINTER PTR_MPI3_SGE_CHAIN,
  Mpi3SGEChain_t, MPI3_POINTER pMpi3SGEChain_t,
  MPI3_SGE_LAST_CHAIN, MPI3_POINTER PTR_MPI3_SGE_LAST_CHAIN,
  Mpi3SGELastChain_t, MPI3_POINTER pMpi3SGELastChain_t;

/*****************************************************************************
 *              Union of scatter gather elements                             *
 ****************************************************************************/
typedef union _MPI3_SGE_UNION
{
    MPI3_SGE_SIMPLE                 Simple;
    u32                             Words[4];
} __attribute__ ((packed)) MPI3_SGE_UNION, MPI3_POINTER PTR_MPI3_SGE_UNION,
  Mpi3SGEUnion_t, MPI3_POINTER pMpi3SGEUnion_t;

/**
 * @brief MPI reply buffer definition
 *
 */

typedef struct _SL8_MPI_REPLY_BUF{
    u8 mpiRepType;
    u8 reserved[3];
} __attribute__ ((packed)) SL8_MPI_REPLY_BUF;


/*****************************************************************************
 *              SCSI IO Error Reply Message                                  *
 ****************************************************************************/
typedef struct _MPI3_SCSI_IO_REPLY
{
    u16                     HostTag;                        /* 0x00 */
    u8                      IOCUseOnly02;                   /* 0x02 */
    u8                      Function;                       /* 0x03 */
    u16                     IOCUseOnly04;                   /* 0x04 */
    u8                      IOCUseOnly06;                   /* 0x06 */
    u8                      MsgFlags;                       /* 0x07 */
    u16                     IOCUseOnly08;                   /* 0x08 */
    u16                     IOCStatus;                      /* 0x0A */
    u32                     IOCLogInfo;                     /* 0x0C */
    u8                      SCSIStatus;                     /* 0x10 */
    u8                      SCSIState;                      /* 0x11 */
    u16                     DevHandle;                      /* 0x12 */
    u32                     TransferCount;                  /* 0x14 */
    u32                     SenseCount;                     /* 0x18 */
    u32                     ResponseData;                   /* 0x1C */
    u16                     TaskTag;                        /* 0x20 */
    u16                     SCSIStatusQualifier;            /* 0x22 */
    u32                     EEDPErrorOffset;                /* 0x24 */
    u16                     EEDPObservedAppTag;             /* 0x28 */
    u16                     EEDPObservedGuard;              /* 0x2A */
    u32                     EEDPObservedRefTag;             /* 0x2C */
    u64                     SenseDataBufferAddress;         /* 0x30 */
} __attribute__ ((packed)) MPI3_SCSI_IO_REPLY, MPI3_POINTER PTR_MPI3_SCSI_IO_REPLY,
  Mpi3SCSIIOReply_t, MPI3_POINTER pMpi3SCSIIOReply_t;

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
    u8    mrioc_id;
    u8    opcode;
    u16   rsvd1;
    u32   rsvd2[4];
};

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
    u8    mpi_reply_type;
    u8    rsvd1;
    u16   rsvd2;
    u8    reply_buf[];
};

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
    u8    buf_type;
    u8    rsvd1;
    u16   rsvd2;
    u32   buf_len;
};

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
    u8    num_of_entries;
    u8    rsvd1;
    u16   rsvd2;
    u32   rsvd3;
    struct mpi3mr_buf_entry buf_entry[1];
};

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
    u8    mrioc_id;
    u8    rsvd1;
    u16   timeout;
    u32   rsvd2;
    struct mpi3mr_buf_entry_list buf_entry_list;
};

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
    u8    cmd_type;
    u8    rsvd1;
    u16   rsvd2;
    u32   rsvd3;
    union {
        struct mpi3mr_bsg_drv_cmd drvrcmd;
        struct mpi3mr_bsg_mptcmd mptcmd;
    } cmd;
};

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
    u16   handle;
    u16   perst_id;
    u32   target_id;
    u8    bus_id;
    u8    rsvd1;
    u16   rsvd2;
};

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
    u16   num_devices;
    u16   rsvd1;
    u32   rsvd2;
    struct mpi3mr_device_map_info dmi[1];
};

struct mpi3_scsi_io_cdb_eedp32 {
    u8                 cdb[20];
    u32             primary_reference_tag;
    u16             primary_application_tag;
    u16             primary_application_tag_mask;
    u32             transfer_length;
} __attribute__ ((packed));

struct mpi3_sge_common {
    u64             address;
    u32             length;
    u8                 reserved0c[3];
    u8                 flags;
} __attribute__ ((packed));

union mpi3_scsi_io_cdb_union {
    u8                         cdb32[32];
    struct mpi3_scsi_io_cdb_eedp32 eedp32;
    struct mpi3_sge_common         sge;
} __attribute__ ((packed));

struct mpi3_scsi_io_request {
    u16                     host_tag;
    u8                         ioc_use_only02;
    u8                         function;
    u16                     ioc_use_only04;
    u8                         ioc_use_only06;
    u8                         msg_flags;
    u16                     change_count;
    u16                     dev_handle;
    u32                     flags;
    u32                     skip_count;
    u32                     data_length;
    u8                         lun[8];
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
    u16                     host_tag;
    u8                         ioc_use_only02;
    u8                         function;
    u16                     ioc_use_only04;
    u8                         ioc_use_only06;
    u8                         msg_flags;
    u16                     ioc_use_only08;
    u16                     ioc_status;
    u32                     ioc_log_info;
    u8                         scsi_status;
    u8                         scsi_state;
    u16                     dev_handle;
    u32                     transfer_count;
    u32                     sense_count;
    u32                     response_data;
    u16                     task_tag;
    u16                     scsi_status_qualifier;
    u32                     eedp_error_offset;
    u16                     eedp_observed_app_tag;
    u16                     eedp_observed_guard;
    u32                     eedp_observed_ref_tag;
    u64                     sense_data_buffer_address;
} __attribute__ ((packed));


/* Definitions for BSG commands */
#define MPI3MR_IOCTL_VERSION            0x06

#define MPI3MR_APP_DEFAULT_TIMEOUT      (60) /*seconds*/

#define MPI3MR_BSG_ADPTYPE_UNKNOWN      0
#define MPI3MR_BSG_ADPTYPE_AVGFAMILY        1

#define MPI3MR_BSG_ADPSTATE_UNKNOWN     0
#define MPI3MR_BSG_ADPSTATE_OPERATIONAL     1
#define MPI3MR_BSG_ADPSTATE_FAULT       2
#define MPI3MR_BSG_ADPSTATE_IN_RESET        3
#define MPI3MR_BSG_ADPSTATE_UNRECOVERABLE   4

#define MPI3MR_BSG_ADPRESET_UNKNOWN     0
#define MPI3MR_BSG_ADPRESET_SOFT        1
#define MPI3MR_BSG_ADPRESET_DIAG_FAULT      2

#define MPI3MR_BSG_LOGDATA_MAX_ENTRIES      400
#define MPI3MR_BSG_LOGDATA_ENTRY_HEADER_SZ  4

#define MPI3MR_DRVBSG_OPCODE_UNKNOWN        0
#define MPI3MR_DRVBSG_OPCODE_ADPINFO        1
#define MPI3MR_DRVBSG_OPCODE_ADPRESET       2
#define MPI3MR_DRVBSG_OPCODE_ALLTGTDEVINFO  4
#define MPI3MR_DRVBSG_OPCODE_GETCHGCNT      5
#define MPI3MR_DRVBSG_OPCODE_LOGDATAENABLE  6
#define MPI3MR_DRVBSG_OPCODE_PELENABLE      7
#define MPI3MR_DRVBSG_OPCODE_GETLOGDATA     8
#define MPI3MR_DRVBSG_OPCODE_QUERY_HDB      9
#define MPI3MR_DRVBSG_OPCODE_REPOST_HDB     10
#define MPI3MR_DRVBSG_OPCODE_UPLOAD_HDB     11
#define MPI3MR_DRVBSG_OPCODE_REFRESH_HDB_TRIGGERS   12


#define MPI3MR_BSG_BUFTYPE_UNKNOWN      0
#define MPI3MR_BSG_BUFTYPE_RAIDMGMT_CMD     1
#define MPI3MR_BSG_BUFTYPE_RAIDMGMT_RESP    2
#define MPI3MR_BSG_BUFTYPE_DATA_IN      3
#define MPI3MR_BSG_BUFTYPE_DATA_OUT     4
#define MPI3MR_BSG_BUFTYPE_MPI_REPLY        5
#define MPI3MR_BSG_BUFTYPE_ERR_RESPONSE     6
#define MPI3MR_BSG_BUFTYPE_MPI_REQUEST      0xFE

#define MPI3MR_BSG_MPI_REPLY_BUFTYPE_UNKNOWN    0
#define MPI3MR_BSG_MPI_REPLY_BUFTYPE_STATUS 1
#define MPI3MR_BSG_MPI_REPLY_BUFTYPE_ADDRESS    2

#define MPI3MR_HDB_BUFTYPE_UNKNOWN      0
#define MPI3MR_HDB_BUFTYPE_TRACE        1
#define MPI3MR_HDB_BUFTYPE_FIRMWARE     2
#define MPI3MR_HDB_BUFTYPE_RESERVED     3

#define MPI3MR_HDB_BUFSTATUS_UNKNOWN        0
#define MPI3MR_HDB_BUFSTATUS_NOT_ALLOCATED  1
#define MPI3MR_HDB_BUFSTATUS_POSTED_UNPAUSED    2
#define MPI3MR_HDB_BUFSTATUS_POSTED_PAUSED  3
#define MPI3MR_HDB_BUFSTATUS_RELEASED       4

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


#undef u8
#undef u16
#undef u32
#undef u64
