

// This is needed for the various HAVE_* macros and PROJECT_* macros.
#include "config.h"

// These are needed to define prototypes and structures for the
// functions defined below
#include "int64.h"
#include "atacmds.h"
#include "scsicmds.h"
#include "utility.h"

// This is to include whatever structures and prototypes you define in
// os_generic.h
#include "os_qnxnto.h"

// Needed by '-V' option (CVS versioning) of smartd/smartctl.  You
// should have one *_H_CVSID macro appearing below for each file
// appearing with #include "*.h" above.  Please list these (below) in
// alphabetic/dictionary order.
const char *os_XXXX_c_cvsid="$Id: os_qnxnto.cpp,v 1.3 2008/06/12 21:46:31 ballen4705 Exp $" \
ATACMDS_H_CVSID CONFIG_H_CVSID INT64_H_CVSID OS_QNXNTO_H_CVSID SCSICMDS_H_CVSID UTILITY_H_CVSID;


// This is here to prevent compiler warnings for unused arguments of
// functions.
#define ARGUSED(x) ((void)(x))

// Please eliminate the following block: both the #include and
// the 'unsupported()' function.  They are only here to warn
// unsuspecting users that their Operating System is not supported! If
// you wish, you can use a similar warning mechanism for any of the
// functions in this file that you can not (or choose not to)
// implement.


#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif
//----------------------------------------------------------------------------------------------
// private Functions
static int ata_sense_data(void *sdata,int *error,int *key,int *asc,int *ascq);
static int ata_interpret_sense(struct cam_pass_thru *cpt,void *sense,int *status,int rcount);
static int ata_pass_thru(int fd,struct cam_pass_thru *pcpt);
//----------------------------------------------------------------------------------------------
static void unsupported(){
  static int warninggiven;

  if (!warninggiven) {
    char *osname;
    extern unsigned char debugmode;
    unsigned char savedebugmode=debugmode;

#ifdef HAVE_UNAME
    struct utsname ostype;
    uname(&ostype);
    osname=ostype.sysname;
#else
    osname="host's";
#endif

    debugmode=1;
    pout("\n"
         "############################################################################\n"
         "WARNING: smartmontools has not been ported to the %s Operating System.\n"
         "Please see the files os_generic.cpp and os_generic.h for porting instructions.\n"
         "############################################################################\n\n",
         osname);
    debugmode=savedebugmode;
    warninggiven=1;
  }

  return;
}
// End of the 'unsupported()' block that you should eliminate.


// print examples for smartctl.  You should modify this function so
// that the device paths are sensible for your OS, and to eliminate
// unsupported commands (eg, 3ware controllers).
void print_smartctl_examples(){
  printf("=================================================== SMARTCTL EXAMPLES =====\n\n");
#ifdef HAVE_GETOPT_LONG
  printf(
         "  smartctl -a /dev/hd0                       (Prints all SMART information)\n\n"
         "  smartctl --smart=on --offlineauto=on --saveauto=on /dev/hd0\n"
         "                                              (Enables SMART on first disk)\n\n"
         "  smartctl -t long /dev/hd0              (Executes extended disk self-test)\n\n"
         "  smartctl --attributes --log=selftest --quietmode=errorsonly /dev/hd0\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         "  smartctl -a --device=3ware,2 /dev/sda\n"
         "          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
         );
#else
  printf(
         "  smartctl -a /dev/hd0                       (Prints all SMART information)\n"
         "  smartctl -s on -o on -S on /dev/hd0         (Enables SMART on first disk)\n"
         "  smartctl -t long /dev/hd0              (Executes extended disk self-test)\n"
         "  smartctl -A -l selftest -q errorsonly /dev/hd0\n"
         "                                      (Prints Self-Test & Attribute errors)\n"
         "  smartctl -a -d 3ware,2 /dev/sda\n"
         "          (Prints all SMART info for 3rd ATA disk on 3ware RAID controller)\n"
         );
#endif
  return;
}

// tries to guess device type given the name (a path).  See utility.h
// for return values.
static const char *net_dev_prefix = "/dev/";
static const char *net_dev_ata_disk = "hd";

int guess_device_type (const char* dev_name)
{
int len,dev_prefix_len;
  dev_prefix_len=strlen(net_dev_prefix);
  if(!dev_name||!(len=strlen(dev_name)))
    return(CONTROLLER_UNKNOWN);
  if (!strncmp(net_dev_prefix,dev_name,dev_prefix_len))
   {
    if(len<=dev_prefix_len)
      return(CONTROLLER_UNKNOWN);
    else
      dev_name += dev_prefix_len;
   }
  if(!strncmp(net_dev_ata_disk,dev_name,strlen(net_dev_ata_disk)))
    return(CONTROLLER_ATA);
  return(CONTROLLER_UNKNOWN);
}

// makes a list of ATA or SCSI devices for the DEVICESCAN directive of
// smartd.  Returns number N of devices, or -1 if out of
// memory. Allocates N+1 arrays: one of N pointers (devlist); the
// other N arrays each contain null-terminated character strings.  In
// the case N==0, no arrays are allocated because the array of 0
// pointers has zero length, equivalent to calling malloc(0).
int make_device_names (char*** devlist, const char* name) {
  ARGUSED(devlist);
  ARGUSED(name);
  unsupported();
  return 0;
}

// Like open().  Return non-negative integer handle, only used by the
// functions below.  type=="ATA" or "SCSI".  If you need to store
// extra information about your devices, create a private internal
// array within this file (see os_freebsd.cpp for an example).  If you
// can not open the device (permission denied, does not exist, etc)
// set errno as open() does and return <0.
int deviceopen(const char *pathname, char *type)
{
  if(!strcmp(type, "ATA"))
    return(open(pathname,O_RDWR|O_NONBLOCK));
  else
    return(-1);
}

// Like close().  Acts only on integer handles returned by
// deviceopen() above.
int deviceclose(int fd)
{
  return(close(fd));
}
//----------------------------------------------------------------------------------------------
// Interface to ATA devices.  See os_linux.cpp for the cannonical example.
// DETAILED DESCRIPTION OF ARGUMENTS
//   device: is the integer handle provided by deviceopen()
//   command: defines the different operations, see atacmds.h
//   select: additional input data IF NEEDED (which log, which type of
//           self-test).
//   data:   location to write output data, IF NEEDED (1 or 512 bytes).
//   Note: not all commands use all arguments.
// RETURN VALUES (for all commands BUT command==STATUS_CHECK)
//  -1 if the command failed
//   0 if the command succeeded,
// RETURN VALUES if command==STATUS_CHECK
//  -1 if the command failed OR the disk SMART status can't be determined
//   0 if the command succeeded and disk SMART status is "OK"
//   1 if the command succeeded and disk SMART status is "FAILING"
int ata_command_interface(int fd,smart_command_set command,int select,char *data)
{
struct cam_pass_thru cpt;
ATA_SENSE            sense;
CDB                  *cdb;
int                  status,rc;
  memset(&cpt,0x00,sizeof(struct cam_pass_thru));
  cdb=(CDB *)cpt.cam_cdb;
  rc=-1;
  switch(command)
   {
    case READ_VALUES:
         cpt.cam_flags                  = CAM_DIR_IN;
         cpt.cam_cdb_len                = 16;
         cpt.cam_dxfer_len              = 512;
         cpt.cam_data_ptr               = (uint32_t)data;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_PIO_DATA_IN;
         cdb->ata_pass_thru.flags       = ATA_FLG_T_DIR|ATA_FLG_TLEN_STPSIU;
         cdb->ata_pass_thru.command     = ATA_SMART_CMD;
         cdb->ata_pass_thru.features    = ATA_SMART_READ_VALUES;
         cdb->ata_pass_thru.lba_mid     = ATA_SMART_LBA_MID_SIG;
         cdb->ata_pass_thru.lba_high    = ATA_SMART_LBA_HI_SIG;
         break;
    case READ_THRESHOLDS:
         cpt.cam_flags                  = CAM_DIR_IN;
         cpt.cam_cdb_len                = 16;
         cpt.cam_dxfer_len              = 512;
         cpt.cam_data_ptr               = (uint32_t)data;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_PIO_DATA_IN;
         cdb->ata_pass_thru.flags       = ATA_FLG_T_DIR|ATA_FLG_TLEN_STPSIU;
         cdb->ata_pass_thru.command     = ATA_SMART_CMD;
         cdb->ata_pass_thru.features    = ATA_SMART_READ_THRESHOLDS;
         cdb->ata_pass_thru.lba_mid     = ATA_SMART_LBA_MID_SIG;
         cdb->ata_pass_thru.lba_high    = ATA_SMART_LBA_HI_SIG;
         break;
    case READ_LOG:
         cpt.cam_flags                  = CAM_DIR_IN;
         cpt.cam_cdb_len                = 16;
         cpt.cam_dxfer_len              = 512;
         cpt.cam_data_ptr               = (uint32_t)data;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_PIO_DATA_IN;
         cdb->ata_pass_thru.flags       = ATA_FLG_T_DIR|ATA_FLG_TLEN_STPSIU;
         cdb->ata_pass_thru.command     = ATA_SMART_CMD;
         cdb->ata_pass_thru.features    = ATA_SMART_READ_LOG_SECTOR;
         cdb->ata_pass_thru.sector_count= 1;
         cdb->ata_pass_thru.lba_low     = select;
         cdb->ata_pass_thru.lba_mid     = ATA_SMART_LBA_MID_SIG;
         cdb->ata_pass_thru.lba_high    = ATA_SMART_LBA_HI_SIG;
         break;
    case WRITE_LOG:
         return(-1);
         break;
    case IDENTIFY:
         cpt.cam_flags                  = CAM_DIR_IN;
         cpt.cam_cdb_len                = 16;
         cpt.cam_dxfer_len              = 512;
         cpt.cam_data_ptr               = (uint32_t)data;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_PIO_DATA_IN;
         cdb->ata_pass_thru.flags       = ATA_FLG_T_DIR|ATA_FLG_TLEN_STPSIU;
         cdb->ata_pass_thru.command     = ATA_IDENTIFY_DEVICE;
         break;
    case PIDENTIFY:
         cpt.cam_flags                  = CAM_DIR_IN;
         cpt.cam_cdb_len                = 16;
         cpt.cam_dxfer_len              = 512;
         cpt.cam_data_ptr               = (uint32_t)data;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_PIO_DATA_IN;
         cdb->ata_pass_thru.flags       = ATA_FLG_T_DIR|ATA_FLG_TLEN_STPSIU;
         cdb->ata_pass_thru.command     = ATA_IDENTIFY_PACKET_DEVICE;
         break;
    case ENABLE:
         cpt.cam_flags                  = CAM_DIR_NONE;
         cpt.cam_cdb_len                = 16;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_DATA_NONE;
         cdb->ata_pass_thru.command     = ATA_SMART_CMD;
         cdb->ata_pass_thru.features    = ATA_SMART_ENABLE;
         cdb->ata_pass_thru.lba_mid     = ATA_SMART_LBA_MID_SIG;
         cdb->ata_pass_thru.lba_high    = ATA_SMART_LBA_HI_SIG;
         break;
    case DISABLE:
         cpt.cam_flags                  = CAM_DIR_NONE;
         cpt.cam_cdb_len                = 16;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_DATA_NONE;
         cdb->ata_pass_thru.command     = ATA_SMART_CMD;
         cdb->ata_pass_thru.features    = ATA_SMART_DISABLE;
         cdb->ata_pass_thru.lba_mid     = ATA_SMART_LBA_MID_SIG;
         cdb->ata_pass_thru.lba_high    = ATA_SMART_LBA_HI_SIG;
         break;
    case AUTO_OFFLINE:
    // NOTE: According to ATAPI 4 and UP, this command is obsolete 
         cpt.cam_flags                  = CAM_DIR_NONE;
         cpt.cam_cdb_len                = 16;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_DATA_NONE;
         cdb->ata_pass_thru.command     = ATA_SMART_CMD;
         cdb->ata_pass_thru.features    = ATA_SMART_AUTO_OFFLINE;
         cdb->ata_pass_thru.lba_low     = select;
         cdb->ata_pass_thru.lba_mid     = ATA_SMART_LBA_MID_SIG;
         cdb->ata_pass_thru.lba_high    = ATA_SMART_LBA_HI_SIG;
         break;
    case AUTOSAVE:
         cpt.cam_flags                  = CAM_DIR_NONE;
         cpt.cam_cdb_len                = 16;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_DATA_NONE;
         cdb->ata_pass_thru.command     = ATA_SMART_CMD;
         cdb->ata_pass_thru.features    = ATA_SMART_AUTOSAVE;
         cdb->ata_pass_thru.sector_count= select;
         cdb->ata_pass_thru.lba_mid     = ATA_SMART_LBA_MID_SIG;
         cdb->ata_pass_thru.lba_high    = ATA_SMART_LBA_HI_SIG;
         break;
    case IMMEDIATE_OFFLINE:
    // NOTE: According to ATAPI 4 and UP, this command is obsolete 
         cpt.cam_flags                  = CAM_DIR_NONE;
         cpt.cam_cdb_len                = 16;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_DATA_NONE;
         cdb->ata_pass_thru.command     = ATA_SMART_CMD;
         cdb->ata_pass_thru.features    = ATA_SMART_IMMEDIATE_OFFLINE;
         cdb->ata_pass_thru.lba_low     = select;
         cdb->ata_pass_thru.lba_mid     = ATA_SMART_LBA_MID_SIG;
         cdb->ata_pass_thru.lba_high    = ATA_SMART_LBA_HI_SIG;
         break;
    case STATUS_CHECK:
    // same command, no HDIO in NetBSD 
    case STATUS:
         cpt.cam_flags                  = CAM_DIR_NONE;
         cpt.cam_cdb_len                = 16;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_DATA_NONE;
         cdb->ata_pass_thru.flags       = ATA_FLG_CK_COND;
         cdb->ata_pass_thru.command     = ATA_SMART_CMD;
         cdb->ata_pass_thru.features    = ATA_SMART_STATUS;
         cdb->ata_pass_thru.lba_mid     = ATA_SMART_LBA_MID_SIG;
         cdb->ata_pass_thru.lba_high    = ATA_SMART_LBA_HI_SIG;
         break;
    case CHECK_POWER_MODE:
         cpt.cam_flags                  = CAM_DIR_NONE;
         cpt.cam_cdb_len                = 16;
         cpt.cam_sense_len              = sizeof(sense);
         cpt.cam_sense_ptr              = (uint32_t)&sense;
         cdb->ata_pass_thru.opcode      = SC_ATA_PT16;
         cdb->ata_pass_thru.protocol    = ATA_PROTO_DATA_NONE;
         cdb->ata_pass_thru.flags       = ATA_FLG_CK_COND;
         cdb->ata_pass_thru.command     = ATA_CHECK_POWER_MODE;
         break;
    default:
         pout("Unrecognized command %d in ata_command_interface()\n", command);
         errno=ENOSYS;
         return(-1);
   }
// execute now
  if((status=ata_pass_thru(fd,&cpt))==EOK)
   {
    rc=status==EOK?0:-1;
    if(cpt.cam_status!=CAM_REQ_CMP)
     {
      ata_interpret_sense(&cpt,&sense,&status,0);
      if(command==STATUS||command==STATUS_CHECK)
        rc=((sense.desc.lba_high<<8)|sense.desc.lba_mid)==ATA_SMART_SIG?0:1;
     }
   }
  if(command==CHECK_POWER_MODE)
    data[0]=cdb->ata_pass_thru.sector_count;
// finish
  return(rc);
}
//----------------------------------------------------------------------------------------------
int marvell_command_interface(int fd, smart_command_set command, int select, char *data)
{
  ARGUSED(fd);
  ARGUSED(command);
  ARGUSED(select);
  ARGUSED(data);
  unsupported();
  return -1;
}
//----------------------------------------------------------------------------------------------
int highpoint_command_interface(int fd, smart_command_set command, int select, char *data)
{
  ARGUSED(fd);
  ARGUSED(command);
  ARGUSED(select);
  ARGUSED(data);
  unsupported();
  return -1;
}
//----------------------------------------------------------------------------------------------
// Interface to ATA devices behind 3ware escalade/apache RAID
// controller cards.  Same description as ata_command_interface()
// above except that 0 <= disknum <= 15 specifies the ATA disk
// attached to the controller, and controller_type specifies the
// precise type of 3ware controller.  See os_linux.c
int escalade_command_interface(int fd,int disknum,int controller_type,smart_command_set command,int select,char *data)
{
  ARGUSED(fd);
  ARGUSED(disknum);
  ARGUSED(controller_type);
  ARGUSED(command);
  ARGUSED(select);
  ARGUSED(data);

  unsupported();
  return -1;
}

int areca_command_interface(int fd,int disknum,smart_command_set command,int select,char *data)
{
  ARGUSED(fd);
  ARGUSED(disknum);
  ARGUSED(command);
  ARGUSED(select);
  ARGUSED(data);

  unsupported();
  return -1;
}
//----------------------------------------------------------------------------------------------
#include <errno.h>
// Interface to SCSI devices.  See os_linux.c
int do_scsi_cmnd_io(int fd,struct scsi_cmnd_io * iop,int report)
{
  ARGUSED(fd);
  ARGUSED(iop);
  ARGUSED(report);
  unsupported();
  return -ENOSYS;
}
//----------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------
static int ata_sense_data(void *sdata,int *error,int *key,int *asc,int *ascq)
{
SCSI_SENSE	*sf;
SCSI_SENSE_DESCRIPTOR	*sd;
  sf=(SCSI_SENSE *)sdata;
  sd=(SCSI_SENSE_DESCRIPTOR *)sdata;
  *error=sf->error;
  if(*error & SENSE_DATA_FMT_DESCRIPTOR)
   {
    *key=sd->sense & SK_MSK;
    *asc=sd->asc;
    *ascq=sd->ascq;
   }
  else
   {
    *key=sf->sense & SK_MSK;
    *asc=sf->asc;
    *ascq=sf->ascq;
   }
  return(CAM_SUCCESS);
}
//----------------------------------------------------------------------------------------------
static int ata_interpret_sense(struct cam_pass_thru *cpt,void *sense,int *status,int rcount)
{
int retry;
int key;
int asc;
int ascq;
int error;
  *status=EIO;
  retry=CAM_TRUE;
  if(cpt->cam_status&CAM_AUTOSNS_VALID)
   {
    ata_sense_data(sense,&error,&key,&asc,&ascq);
    switch(key)
     {
      case SK_NO_SENSE:					// No sense data (no error)
           retry=CAM_FALSE;
           *status=EOK;
           break;
      case SK_RECOVERED:					// Recovered error
	   switch(asc)
            {
             case ASC_ATA_PASS_THRU:
                  switch(ascq)
                   {
                    case ASCQ_ATA_PASS_THRU_INFO_AVAIL:
                         break;
                    default:
                         break;
                   }
                  break;
             default:
                  break;
            }
           retry=CAM_FALSE;
           *status=EOK;
           break;
      case SK_NOT_RDY:					// Device not ready
           *status=EAGAIN;
           switch(asc)
            {
             case ASC_NOT_READY:
                  switch(ascq)
                   {
                    case ASCQ_BECOMING_READY:
                    case ASCQ_CAUSE_NOT_REPORTABLE:
                    default:
                    retry=CAM_FALSE;
                    break;
                   }
                  break;
             case ASC_MEDIA_NOT_PRESENT:
                  *status=ENXIO;
                  retry=CAM_FALSE;
                  break;
            }
           break;
      case SK_MEDIUM:						// Medium error
      case SK_HARDWARE:					// Hardware error
           retry=CAM_FALSE;
           *status=EIO;
           break;
      case SK_ILLEGAL:					// Illegal Request (bad command)
           retry=CAM_FALSE;
           *status=EINVAL;
           break;
      case SK_UNIT_ATN:					// Unit Attention
           switch(asc)
            {
             case ASC_MEDIUM_CHANGED:
                  *status=ESTALE;
                  retry=CAM_FALSE;
                  break;
             case ASC_BUS_RESET:
                  break;
            }
           break;
      case SK_DATA_PROT:					// Data Protect
           retry=CAM_FALSE;
           *status=EROFS;
           break;
      case SK_VENDOR:						// Vendor Specific
      case SK_CPY_ABORT:					// Copy Aborted
           retry=CAM_FALSE;
           *status=EIO;
           break;
      case SK_CMD_ABORT:					// Aborted Command
           retry=CAM_FALSE;
           *status=ECANCELED;
           break;
      case SK_EQUAL:						// Equal
      case SK_VOL_OFL:					// Volume Overflow
      case SK_MISCMP:						// Miscompare
      case SK_RESERVED:					// Reserved
           break; 
     }
    if(*status==EOK)
     {
      switch(cpt->cam_status&CAM_STATUS_MASK) 
       {
        case CAM_REQ_CMP_ERR:			// CCB request completed with an err
             retry=CAM_FALSE;
             *status=EIO;
             break;
        case CAM_BUSY:					// CAM subsystem is busy
             *status=EAGAIN;
             break;
        case CAM_REQ_INVALID:			// CCB request is invalid
        case CAM_PATH_INVALID:			// Path ID supplied is invalid
        case CAM_DEV_NOT_THERE:			// SCSI device not installed/there
        case CAM_SEL_TIMEOUT:			// Target selection timeout
        case CAM_LUN_INVALID:			// LUN supplied is invalid
        case CAM_TID_INVALID:			// Target ID supplied is invalid
             retry=CAM_FALSE;
             *status=ENXIO;
             break;
        case CAM_CMD_TIMEOUT:			// Command timeout
             *status=rcount?EAGAIN:EIO;
             break;
        case CAM_MSG_REJECT_REC:		// Message reject received
        case CAM_SCSI_BUS_RESET:		// SCSI bus reset sent/received
        case CAM_UNCOR_PARITY:			// Uncorrectable parity err occurred
        case CAM_AUTOSENSE_FAIL:		// Autosense: Request sense cmd fail
        case CAM_NO_HBA:				// No HBA detected Error
        case CAM_DATA_RUN_ERR:			// Data overrun/underrun error
             retry=CAM_FALSE;
             *status=EIO;
             break;
        case CAM_UNEXP_BUSFREE:			// Unexpected BUS free
        case CAM_SEQUENCE_FAIL:			// Target bus phase sequence failure
             *status=EIO;
             break;
        case CAM_PROVIDE_FAIL:			// Unable to provide requ. capability
             retry=CAM_FALSE;
             *status=ENOTTY;
             break;
        case CAM_CCB_LEN_ERR:			// CCB length supplied is inadequate
        case CAM_BDR_SENT:				// A SCSI BDR msg was sent to target
        case CAM_REQ_TERMIO:			// CCB request terminated by the host
        case CAM_FUNC_NOTAVAIL:			// The requ. func is not available
        case CAM_NO_NEXUS:				// Nexus is not established
        case CAM_IID_INVALID:			// The initiator ID is invalid
        case CAM_CDB_RECVD:				// The SCSI CDB has been received
             retry=CAM_FALSE;
             *status=EIO;
             break;
        case CAM_SCSI_BUSY:				// SCSI bus busy
             *status=EAGAIN;
             break;
       }
     }
   }
  return(retry);
}
//----------------------------------------------------------------------------------------------
static int ata_pass_thru(int fd,struct cam_pass_thru *pcpt)
{
int    icnt;
int    status;
iov_t  iov[3];
struct cam_pass_thru	cpt;
  cpt=*pcpt;
  icnt=1;
  SETIOV(&iov[0],&cpt,sizeof(cpt));
  cpt.cam_timeout=cpt.cam_timeout?cpt.cam_timeout:CAM_TIME_DEFAULT;
  if(cpt.cam_sense_len)
   {
    SETIOV(&iov[1],cpt.cam_sense_ptr,cpt.cam_sense_len);
    cpt.cam_sense_ptr=sizeof(cpt);
    icnt++;
   }
  if(cpt.cam_dxfer_len)
   {
    SETIOV(&iov[2],(void *)cpt.cam_data_ptr,cpt.cam_dxfer_len);
    cpt.cam_data_ptr=(paddr_t)sizeof(cpt)+cpt.cam_sense_len;
    icnt++;
   }
  if((status=devctlv(fd,DCMD_CAM_PASS_THRU,icnt,icnt,iov,iov,NULL)))
    pout("ata_pass_thru devctl:  %s\n",strerror(status));
  pcpt->cam_status=cpt.cam_status;
  pcpt->cam_scsi_status=cpt.cam_scsi_status;
  return(status);
}
//----------------------------------------------------------------------------------------------
