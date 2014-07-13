#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#include "config.h"

#if defined(linux) || defined(__linux__)
#  include <sys/ioctl.h>
#  ifdef HAVE_LINUX_COMPILER_H
#    include <linux/compiler.h>
#  endif
#  if defined(HAVE_LINUX_CCISS_IOCTL_H)
#    include <linux/cciss_ioctl.h>
#    define _HAVE_CCISS
#  endif
#  include <asm/byteorder.h>
#  ifndef be32toh
#    define be32toh __be32_to_cpu
#  endif
#elif defined(__FreeBSD__)
#  include <sys/endian.h>
#  include CISS_LOCATION
#  define _HAVE_CCISS
#elif defined(__FreeBSD_kernel__)
#  include <endian.h>
#  ifdef __GLIBC__
#  include <bsd/sys/cdefs.h>
#  include <stdint.h>
#  endif
#  include CISS_LOCATION
#  define _HAVE_CCISS
#endif

#ifdef _HAVE_CCISS
#include "cciss.h"
#include "int64.h"
#include "scsicmds.h"
#include "utility.h"

const char * cciss_cpp_cvsid = "$Id$"
  CCISS_H_CVSID;

typedef struct _ReportLUNdata_struct
{
  uint32_t LUNListLength;	/* always big-endian */
  uint32_t reserved;
  uint8_t LUN[CISS_MAX_LUN][8];
} ReportLunData_struct;

/* Structure/defines of Report Physical LUNS of drive */
#ifndef CISS_MAX_LUN
#define CISS_MAX_LUN        16
#endif
#define CISS_MAX_PHYS_LUN   1024
#define CISS_REPORT_PHYS    0xc3

#define LSCSI_DRIVER_SENSE  0x8		/* alternate CHECK CONDITION indication */
#define SEND_IOCTL_RESP_SENSE_LEN 16    /* ioctl limitation */

static int cciss_getlun(int device, int target, unsigned char *physlun, int report);
static int cciss_sendpassthru(unsigned int cmdtype, unsigned char *CDB,
    			unsigned int CDBlen, char *buff,
    			unsigned int size, unsigned int LunID,
    			unsigned char *scsi3addr, int fd);

/* 
   This is an interface that uses the cciss passthrough to talk to the SMART controller on
   the HP system. The cciss driver provides a way to send SCSI cmds through the CCISS passthrough.
*/
int cciss_io_interface(int device, int target, struct scsi_cmnd_io * iop, int report)
{
     unsigned char pBuf[512] = {0};
     unsigned char phylun[8] = {0};
     int iBufLen = 512;
     int status = -1;
     int len = 0; // used later in the code.
 
     status = cciss_getlun(device, target, phylun, report);
     if (report > 0)
         printf("  cciss_getlun(%d, %d) = 0x%x; scsi3addr: %02x %02x %02x %02x %02x %02x %02x %02x\n", 
	     device, target, status, 
	     phylun[0], phylun[1], phylun[2], phylun[3], phylun[4], phylun[5], phylun[6], phylun[7]);
     if (status) {
         return -ENXIO;      /* give up, assume no device there */
     }

     status = cciss_sendpassthru( 2, iop->cmnd, iop->cmnd_len, (char*) pBuf, iBufLen, 1, phylun, device);
 
     if (0 == status)
     {
         if (report > 0)
             printf("  status=0\n");
         if (DXFER_FROM_DEVICE == iop->dxfer_dir)
         {
             memcpy(iop->dxferp, pBuf, iop->dxfer_len);
             if (report > 1)
             {
                 int trunc = (iop->dxfer_len > 256) ? 1 : 0;
                 printf("  Incoming data, len=%d%s:\n", (int)iop->dxfer_len,
                      (trunc ? " [only first 256 bytes shown]" : ""));
                 dStrHex((const char*)iop->dxferp, (trunc ? 256 : iop->dxfer_len) , 1);
             }
         }
         return 0;
     }
     iop->scsi_status = status & 0x7e; /* bits 0 and 7 used to be for vendors */
     if (LSCSI_DRIVER_SENSE == ((status >> 24) & 0xf))
         iop->scsi_status = SCSI_STATUS_CHECK_CONDITION;
     len = (SEND_IOCTL_RESP_SENSE_LEN < iop->max_sense_len) ?
                SEND_IOCTL_RESP_SENSE_LEN : iop->max_sense_len;
     if ((SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) &&
         iop->sensep && (len > 0))
     {
         memcpy(iop->sensep, pBuf, len);
         iop->resp_sense_len = iBufLen;
         if (report > 1)
         {
             printf("  >>> Sense buffer, len=%d:\n", (int)len);
             dStrHex((const char *)pBuf, len , 1);
         }
     }
     if (report)
     {
         if (SCSI_STATUS_CHECK_CONDITION == iop->scsi_status) {
             printf("  status=%x: sense_key=%x asc=%x ascq=%x\n", status & 0xff,
                  pBuf[2] & 0xf, pBuf[12], pBuf[13]);
         }
         else
             printf("  status=0x%x\n", status);
     }
     if (iop->scsi_status > 0)
         return 0;
     else
     {
         if (report > 0)
             printf("  ioctl status=0x%x but scsi status=0, fail with ENXIO\n", status);
         return -ENXIO;      /* give up, assume no device there */
     }
} 

static int cciss_sendpassthru(unsigned int cmdtype, unsigned char *CDB,
    			unsigned int CDBlen, char *buff,
    			unsigned int size, unsigned int LunID,
    			unsigned char *scsi3addr, int fd)
{
    int err ;
    IOCTL_Command_struct iocommand;

    memset(&iocommand, 0, sizeof(iocommand));

    if (cmdtype == 0) 
    {
        // To controller; nothing to do
    }
    else if (cmdtype == 1) 
    {
        iocommand.LUN_info.LogDev.VolId = LunID;
        iocommand.LUN_info.LogDev.Mode = 1;
    }
    else if (cmdtype == 2) 
    {
        memcpy(&iocommand.LUN_info.LunAddrBytes,scsi3addr,8);
        iocommand.LUN_info.LogDev.Mode = 0;
    }
    else 
    {
        fprintf(stderr, "cciss_sendpassthru: bad cmdtype\n");
        return 1;
    }

    memcpy(&iocommand.Request.CDB[0], CDB, CDBlen);
    iocommand.Request.CDBLen = CDBlen;
    iocommand.Request.Type.Type = TYPE_CMD;
    iocommand.Request.Type.Attribute = ATTR_SIMPLE;
    iocommand.Request.Type.Direction = XFER_READ;
    iocommand.Request.Timeout = 0;

    iocommand.buf_size = size;
    iocommand.buf = (unsigned char *)buff;

    if ((err = ioctl(fd, CCISS_PASSTHRU, &iocommand))) 
    {
        fprintf(stderr, "CCISS ioctl error %d (fd %d CDBLen %d buf_size %d)\n",
	    fd, err, CDBlen, size);
    }
    return err;
}

static int cciss_getlun(int device, int target, unsigned char *physlun, int report)
{
    unsigned char CDB[16]= {0};
    ReportLunData_struct *luns;
    int reportlunsize = sizeof(*luns) + CISS_MAX_PHYS_LUN * 8;
    int ret;

    luns = (ReportLunData_struct *)malloc(reportlunsize);

    memset(luns, 0, reportlunsize);

    /* Get Physical LUN Info (for physical device) */
    CDB[0] = CISS_REPORT_PHYS;
    CDB[6] = (reportlunsize >> 24) & 0xFF;  /* MSB */
    CDB[7] = (reportlunsize >> 16) & 0xFF;
    CDB[8] = (reportlunsize >> 8) & 0xFF;
    CDB[9] = reportlunsize & 0xFF;

    if ((ret = cciss_sendpassthru(0, CDB, 12, (char *)luns, reportlunsize, 0, NULL, device)))
    {
        free(luns);
        return ret;
    }

    if (report > 1)
    {
      unsigned int i,j;
      unsigned char *stuff = (unsigned char *)luns;

      pout("\n===== [%s] DATA START (BASE-16) =====\n", "LUN DATA");
      for (i=0; i<(sizeof(_ReportLUNdata_struct)+15)/16; i++){
	pout("%03d-%03d: ", 16*i, 16*(i+1)-1);
	for (j=0; j<15; j++)
	  pout("%02x ",*stuff++);
	pout("%02x\n",*stuff++);
      }
      pout("===== [%s] DATA END (%u Bytes) =====\n\n", "LUN DATA", (unsigned)sizeof(_ReportLUNdata_struct));
    }

    if (target >= 0 && target < (int) be32toh(luns->LUNListLength) / 8)
    {
	memcpy(physlun, luns->LUN[target], 8);
	free(luns);
	return 0;
    }

    free(luns);
    return 1;
}
#endif 
