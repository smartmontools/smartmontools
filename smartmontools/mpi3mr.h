#ifndef _MPI3MR_H_
#define _MPI3MR_H_
#include <stdint.h>
#include <stdio.h>

//TODO: Disk enumeration, controller enumeration

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
    uint8_t disk_selector_high;
    uint8_t disk_selector_low;
    uint8_t unsure[20];
    scsi_io_cmnd scsi_cmd;
};


struct entries_ {
    uint32_t type;
    uint32_t size;
};

struct bsg_ioctl {
    uint8_t cmd;
    uint8_t padding1[7];
    uint8_t ctrl_id;
    uint8_t padding2[1];
    uint16_t timeout;
    uint8_t padding3[4];
    uint8_t size;
    uint8_t padding4[7];

    entries_ entries[4];

    uint8_t padding5[8];
} __attribute__((packed));

#endif