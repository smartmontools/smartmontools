/*
 * atacmds.c
 * 
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2000 Andre Hedrick <andre@linux-ide.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * This code was originally developed as a Senior Thesis by Michael Cornwell
 * at the Concurrent Systems Laboratory (now part of the Storage Systems
 * Research Center), Jack Baskin School of Engineering, University of
 * California, Santa Cruz. http://ssrc.soe.ucsc.edu/
 * 
 */

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <stdlib.h>
#include "atacmds.h"

const char *CVSid1="$Id: atacmds.c,v 1.34 2002/10/30 00:56:19 ballen4705 Exp $" CVSID1;

// These Drive Identity tables are taken from hdparm 5.2, and are also
// given in the ATA/ATAPI specs for the IDENTIFY DEVICE command.  Note
// that SMART was first added into the ATA/ATAPI-3 Standard with
// Revision 3 of the document, July 25, 1995.  Look at the "Document
// Status" revision commands at the beginning of
// http://www.t13.org/project/d2008r6.pdf to see this.
#define NOVAL_0			0x0000
#define NOVAL_1			0xffff
/* word 81: minor version number */
#define MINOR_MAX 0x1C
const char *minor_str[] = {			/* word 81 value: */
  "Device does not report version",		/* 0x0000	*/
  "ATA-1 X3T9.2 781D prior to revision 4",	/* 0x0001	*/
  "ATA-1 published, ANSI X3.221-1994",		/* 0x0002	*/
  "ATA-1 X3T9.2 781D revision 4",		/* 0x0003	*/
  "ATA-2 published, ANSI X3.279-1996",		/* 0x0004	*/
  "ATA-2 X3T10 948D prior to revision 2k",	/* 0x0005	*/
  "ATA-3 X3T10 2008D revision 1",		/* 0x0006	*/ /* SMART NOT INCLUDED */
  "ATA-2 X3T10 948D revision 2k",		/* 0x0007	*/
  "ATA-3 X3T10 2008D revision 0",		/* 0x0008	*/ 
  "ATA-2 X3T10 948D revision 3",		/* 0x0009	*/
  "ATA-3 published, ANSI X3.298-199x",		/* 0x000a	*/
  "ATA-3 X3T10 2008D revision 6",		/* 0x000b	*/ /* 1st VERSION WITH SMART */
  "ATA-3 X3T13 2008D revision 7 and 7a",	/* 0x000c	*/
  "ATA/ATAPI-4 X3T13 1153D revision 6",		/* 0x000d	*/
  "ATA/ATAPI-4 T13 1153D revision 13",		/* 0x000e	*/
  "ATA/ATAPI-4 X3T13 1153D revision 7",		/* 0x000f	*/
  "ATA/ATAPI-4 T13 1153D revision 18",		/* 0x0010	*/
  "ATA/ATAPI-4 T13 1153D revision 15",		/* 0x0011	*/
  "ATA/ATAPI-4 published, ANSI NCITS 317-1998",	/* 0x0012	*/
  "ATA/ATAPI-5 T13 1321D revision 3",	        /* 0x0013	*/
  "ATA/ATAPI-4 T13 1153D revision 14",		/* 0x0014	*/
  "ATA/ATAPI-5 T13 1321D revision 1",		/* 0x0015	*/
  "ATA/ATAPI-5 published, ANSI NCITS 340-2000",	/* 0x0016	*/
  "ATA/ATAPI-4 T13 1153D revision 17",		/* 0x0017	*/
  "ATA/ATAPI-6 T13 1410D revision 0",		/* 0x0018	*/
  "ATA/ATAPI-6 T13 1410D revision 3a",		/* 0x0019	*/
  "Reserved",					/* 0x001a	*/
  "ATA/ATAPI-6 T13 1410D revision 2",		/* 0x001b	*/
  "ATA/ATAPI-6 T13 1410D revision 1",		/* 0x001c	*/
  "reserved"					/* 0x001d	*/
  "reserved"					/* 0x001e	*/
  "reserved"					/* 0x001f-0xfffe*/
};

// NOTE ATA/ATAPI-4 REV 4 was the LAST revision where the device
// attribute structures were NOT completely vendor specific.  So any
// disk that is ATA/ATAPI-4 or above can not be trusted to show the
// vendor values in sensible format.

// Negative values below are because it doesn't support SMART
const int actual_ver[] = { 
  /* word 81 value: */
  0,		/* 0x0000	WARNING: 	*/
  1,		/* 0x0001	WARNING: 	*/
  1,		/* 0x0002	WARNING: 	*/
  1,		/* 0x0003	WARNING: 	*/
  2,		/* 0x0004	WARNING:   This array 		*/
  2,		/* 0x0005	WARNING:   corresponds 		*/
  -3, /*<== */	/* 0x0006	WARNING:   *exactly*		*/
  2,		/* 0x0007	WARNING:   to the ATA/		*/
  -3, /*<== */	/* 0x0008	WARNING:   ATAPI version	*/
  2,		/* 0x0009	WARNING:   listed in	 	*/
  3,		/* 0x000a	WARNING:   the 		 	*/
  3,		/* 0x000b	WARNING:   minor_str 		*/
  3,		/* 0x000c	WARNING:   array		*/
  4,		/* 0x000d	WARNING:   above.		*/
  4,		/* 0x000e	WARNING:  			*/
  4,		/* 0x000f	WARNING:   If you change 	*/
  4,		/* 0x0010	WARNING:   that one,      	*/
  4,		/* 0x0011	WARNING:   change this one	*/
  4,		/* 0x0012	WARNING:   too!!!        	*/
  5,		/* 0x0013	WARNING:	*/
  4,		/* 0x0014	WARNING:	*/
  5,		/* 0x0015	WARNING:	*/
  5,		/* 0x0016	WARNING:	*/
  4,		/* 0x0017	WARNING:	*/
  6,		/* 0x0018	WARNING:	*/
  6,		/* 0x0019	WARNING:	*/
  0,		/* 0x001a	WARNING:	*/
  6,		/* 0x001b	WARNING:	*/
  6,		/* 0x001c	WARNING:	*/
  0		/* 0x001d-0xfffe    		*/
};

// Used to warn users about invalid checksums.  However we will not
// abort on invalid checksums.
void checksumwarning(const char *string){
  pout("Warning! %s error: invalid SMART checksum.\n",string);
  fprintf(stderr,"Warning! %s error: invalid SMART checksum.\n",string);
  syslog(LOG_INFO,"Warning! %s error: invalid SMART checksum.\n",string);
  return;
}

// We no longer use this function, because the IOCTL appears to return
// only the drive identity at the time that the system was booted
// (perhaps from the BIOS.  It doesn't correctly reflect the current
// state information, and for example the checksum is usually
// wrong. The replacement function follows afterwards
#if (0)
int ataReadHDIdentity (int device, struct hd_driveid *buf){
  if (ioctl(device, HDIO_GET_IDENTITY, buf)){ 
    perror ("Error ATA GET HD Identity Failed");
    return -1;
  }
  return 0;
}
#endif

// Reads current Device Identity info (512 bytes) into buf
int ataReadHDIdentity (int device, struct hd_driveid *buf){
  unsigned short driveidchecksum;
  unsigned char parms[HDIO_DRIVE_CMD_HDR_SIZE+sizeof(*buf)]=
  {WIN_IDENTIFY, 0, 0, 1,};
  
  if (ioctl(device ,HDIO_DRIVE_CMD,parms)){
    // See if device responds to packet command...
    parms[0]=WIN_PIDENTIFY;
    if (ioctl(device ,HDIO_DRIVE_CMD,parms)){
      perror ("Error ATA GET HD Identity Failed");
      return -1; 
    }
  }
  // copy data into driveid structure
  memcpy(buf,parms+HDIO_DRIVE_CMD_HDR_SIZE,sizeof(*buf));
  
#if 0
  // The following ifdef is a HACK to distinguish different versions
  // of the header file defining hd_driveid
#ifdef CFA_REQ_EXT_ERROR_CODE
  driveidchecksum=buf->integrity_word;
#else
  // Note -- the declaration that appears in
  // /usr/include/linux/hdreg.h: short words160_255[95], is WRONG.
  // It should say: short words160_255[96]. I have written to Andre
  // Hedrick about this on Oct 17 2002.  Please remove this comment
  // once the fix has made it into the stock kernel tree.
  driveidchecksum=buf->words160_255[95];
#endif
#else
  // This way is ugly and you may feel ill -- but it always works...
  {
    unsigned short *rawstructure=
      (unsigned short *)buf;
    driveidchecksum=rawstructure[255];
  }
#endif
  
  if ((driveidchecksum & 0x00ff) == 0x00a5){
    // Device identity structure contains a checksum
    unsigned char cksum=0;
    int i;
    
    for (i=0;i<sizeof(*buf);i++)
      cksum+=parms[i+HDIO_DRIVE_CMD_HDR_SIZE];
    
    if (cksum)
      checksumwarning("Drive Identity Structure");
  }
 
 return 0;
}

// Returns ATA version as an integer, and a pointer to a string
// describing which revision.  Note that Revision 0 of ATA-3 does NOT
// support SMART.  For this one case we return -3 rather than +3 as
// the version number.  See notes above.
int ataVersionInfo (const char** description, struct hd_driveid drive, unsigned short *minor){
  unsigned short major;
  int i;
  
  // get major and minor ATA revision numbers
#ifdef __NEW_HD_DRIVE_ID
  major=drive.major_rev_num;
  *minor=drive.minor_rev_num;
#else
  major=drive.word80;
  *minor=drive.word81;
#endif
  
  // First check if device has ANY ATA version information in it
  if (major==NOVAL_0 || major==NOVAL_1) {
    *description=NULL;
    return -1;
  }
  
  // The minor revision number has more information - try there first
  if (*minor && (*minor<=MINOR_MAX)){
    int std = actual_ver[*minor];
    if (std) {
      *description=minor_str[*minor];
      return std;
    }
  }
  
  // HDPARM has a very complicated algorithm from here on. Since SMART only
  // exists on ATA-3 and later standards, let's punt on this.  If you don't
  // like it, please fix it.  The code's in CVS.
  for (i=15; i>0; i--)
    if (major & (0x1<<i))
      break;
  
  *description=NULL; 
  if (i==0)
    return 1;
  else
    return i;;
}

// returns 1 if SMART supported, 0 if not supported or can't tell
int ataSmartSupport(struct hd_driveid drive){
  unsigned short word82,word83;

  // get correct bits of IDENTIFY DEVICE structure
#ifdef __NEW_HD_DRIVE_ID
  word82=drive.command_set_1;
  word83=drive.command_set_2;
#else
  word82=drive.command_sets;
  word83=drive.word83;
#endif

  // Note this does not work for ATA3 < Revision 6, when word82 and word83 were added
  // we should check for ATA3 Rev 0 in minor identity code...  
  return (word83 & 0x0001<<14) && !(word83 & 0x0001<<15) && (word82 & 0x0001);
}

// returns 1 if SMART enabled, 0 if SMART disabled, -1 if can't tell
int ataIsSmartEnabled(struct hd_driveid drive){
    unsigned short word85,word87;

  // Get correct bits of IDENTIFY DRIVE structure
#ifdef __NEW_HD_DRIVE_ID
  word85=drive.cfs_enable_1;
  word87=drive.csf_default;
#else
  word85=drive.word85;
  word87=drive.word87;
#endif
  
  if ((word87 & 0x0001<<14) && !(word87 & 0x0001<<15))
    // word85 contains valid information, so
    return word85 & 0x0001;
  
  // Since we can't rely word85, we don't know if SMART is enabled.
  return -1;
}


// Reads SMART attributes into *data
int ataReadSmartValues(int device, struct ata_smart_values *data){	
  int i;
  unsigned char chksum=0;
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE]= 
    {WIN_SMART, 0, SMART_READ_VALUES, 1, };
  
  if (ioctl(device,HDIO_DRIVE_CMD,buf)){
    perror("Error SMART Values Read failed");
    return -1;
  }
  
  // compute checksum
  for (i=0;i<ATA_SMART_SEC_SIZE;i++)
    chksum+=buf[i+HDIO_DRIVE_CMD_HDR_SIZE];
  
  // verify that checksum vanishes
  if (chksum)
    checksumwarning("SMART Data Structure");
  
  // copy data and return
  memcpy(data,buf+HDIO_DRIVE_CMD_HDR_SIZE,ATA_SMART_SEC_SIZE);
  return 0;
}


// Reads the Self Test Log (log #6)
int ataReadSelfTestLog (int device, struct ata_smart_selftestlog *data){	
  int i;
  unsigned char chksum=0;	
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE] = 
    {WIN_SMART, 0x06, SMART_READ_LOG_SECTOR, 1,};
  
  // get data from device
  if (ioctl(device, HDIO_DRIVE_CMD, buf)){
    perror("Error SMART Error Self-Test Log Read failed");
    return -1;
  }
  
  // compute its checksum, and issue a warning if needed
  for (i=0;i<ATA_SMART_SEC_SIZE;i++)
    chksum+=buf[HDIO_DRIVE_CMD_HDR_SIZE+i];
  if (chksum)
    checksumwarning("SMART Self-Test Log");
  
  // copy data back to the user and return
  memcpy(data,buf+HDIO_DRIVE_CMD_HDR_SIZE, ATA_SMART_SEC_SIZE); 
  return 0;
}

// Reads the Error Log (log #1)
int ataReadErrorLog (int device, struct ata_smart_errorlog *data){	
  int i;
  unsigned char chksum=0;	
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE] = 
    {WIN_SMART, 0x01, SMART_READ_LOG_SECTOR, 1,};
  
  // get data from device
  if (ioctl(device,HDIO_DRIVE_CMD,buf)) {
    perror("Error SMART Error Log Read failed");
    return -1;
  }
  
  // compute checksum and issue warning if needed
  for (i=0;i<ATA_SMART_SEC_SIZE;i++)
    chksum+=buf[HDIO_DRIVE_CMD_HDR_SIZE+i];
  if (chksum)
    checksumwarning("SMART Error Log");
  
  //copy data back to user and return
  memcpy(data, buf+HDIO_DRIVE_CMD_HDR_SIZE, ATA_SMART_SEC_SIZE);
  return 0;
}


int ataReadSmartThresholds (int device, struct ata_smart_thresholds *data){
  int i;
  unsigned char chksum=0;	
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE] = 
    {WIN_SMART, 1, SMART_READ_THRESHOLDS, 1,};
  
  // get data from device
  if (ioctl(device ,HDIO_DRIVE_CMD, buf)){
    perror("Error SMART Thresholds Read failed");
    return -1;
  }
  
  // compute checksum and issue warning if needed
  for (i=0;i<ATA_SMART_SEC_SIZE;i++)
    chksum+=buf[HDIO_DRIVE_CMD_HDR_SIZE+i];
  if (chksum)
    checksumwarning("SMART Attribute Thresholds");
  
  // copy data back to user and return
  memcpy(data,buf+HDIO_DRIVE_CMD_HDR_SIZE, ATA_SMART_SEC_SIZE);
  return 0;
}


// This routine is not currently in use, and it's been marked as
// "Obsolete" in the ANSI ATA-5 spec.  So it should probably be left
// alone and unused.  If you do modify the thresholds, be sure to set
// the checksum correctly before putting the structure back!
int ataSetSmartThresholds ( int device, struct ata_smart_thresholds *data){	
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE] = 
    {WIN_SMART, 1, 0xD7, 1,};
  
  memcpy(buf+HDIO_DRIVE_CMD_HDR_SIZE, data, ATA_SMART_SEC_SIZE);
  
  if (ioctl(device, HDIO_DRIVE_CMD, buf)){
    perror("Error SMART Thresholds Write failed");
    return -1;
  }
  return 0;
}

int ataEnableSmart (int device ){	
  unsigned char parms[4] = {WIN_SMART, 1, SMART_ENABLE, 0};
  
  if (ioctl (device, HDIO_DRIVE_CMD, parms)){
    perror("Error SMART Enable failed");
    return -1;
  }
  return 0;
}

int ataDisableSmart (int device ){	
  unsigned char parms[4] = {WIN_SMART, 1, SMART_DISABLE, 0};
  
  if (ioctl(device, HDIO_DRIVE_CMD, parms)){
    perror("Error SMART Disable failed");
    return -1;
  }  
  return 0;
}

int ataEnableAutoSave(int device){
  unsigned char parms[4] = {WIN_SMART, 241, SMART_AUTOSAVE, 0};
  
  if (ioctl(device, HDIO_DRIVE_CMD, parms)){
    perror("Error SMART Enable Auto-save failed");
    return -1;
  }
  return 0;
}

int ataDisableAutoSave(int device){
  unsigned char parms[4] = {WIN_SMART, 0, SMART_AUTOSAVE, 0};
  
  if (ioctl(device, HDIO_DRIVE_CMD, parms)){
    perror("Error SMART Disable Auto-save failed");
    return -1;
  }
  return 0;
}

// Note that in the ATA-5 standard this command is marked "OBSOLETE"
int ataEnableAutoOffline (int device ){	
  
  /* timer hard coded to 4 hours */
  unsigned char parms[4] = {WIN_SMART, 248, SMART_AUTO_OFFLINE, 0};
  
  if (ioctl(device , HDIO_DRIVE_CMD, parms)){
    perror("Error SMART Enable Automatic Offline failed");
    return -1;
  }
  return 0;
}

// Another Obsolete Command!
int ataDisableAutoOffline (int device ){	
  unsigned char parms[4] = {WIN_SMART, 0, SMART_AUTO_OFFLINE, 0};
  
  if (ioctl(device , HDIO_DRIVE_CMD, parms)){
    perror("Error SMART Disable Automatic Offline failed");
    return -1;
  }
  return 0;
}


// This function does NOTHING except tell us if SMART is working &
// enabled on the device.  See ataSmartStatus2() for one that actually
// returns SMART status.
int ataSmartStatus (int device ){	
   unsigned char parms[4] = {WIN_SMART, 0, SMART_STATUS, 0};

   if (ioctl(device, HDIO_DRIVE_CMD, parms)){
     perror("Error Return SMART Status via HDIO_DRIVE_CMD failed");
     return -1;
   }
   return 0;
}

// If SMART is enabled, supported, and working, then this call is
// guaranteed to return 1, else zero.  Silent inverse of
// ataSmartStatus()
int ataDoesSmartWork(int device){	
   unsigned char parms[4] = {WIN_SMART, 0, SMART_STATUS, 0};
   return !ioctl(device, HDIO_DRIVE_CMD, parms);
}


#ifdef HDIO_DRIVE_TASK
// This function uses a different interface (DRIVE_TASK) than the
// other commands in this file.
int ataSmartStatus2(int device){
  unsigned char normal_cyl_lo=0x4f, normal_cyl_hi=0xc2;
  unsigned char failed_cyl_lo=0xf4, failed_cyl_hi=0x2c;
  
  unsigned char parms[HDIO_DRIVE_TASK_HDR_SIZE]=
    {WIN_SMART, SMART_STATUS, 0, 0, 0, 0, 0};
  
  // load CL and CH values
  parms[4]=normal_cyl_lo;
  parms[5]=normal_cyl_hi;

  if (ioctl(device,HDIO_DRIVE_TASK,parms)){
    perror("Error SMART Status command via HDIO_DRIVE_TASK failed");
    return -1;
  }
  
  // Cyl low and Cyl high unchanged means "Good SMART status"
  if (parms[4]==normal_cyl_lo && parms[5]==normal_cyl_hi)
    return 0;
  
  // These values mean "Bad SMART status"
  if (parms[4]==failed_cyl_lo && parms[5]==failed_cyl_hi)
    return 1;

  // We haven't gotten output that makes sense; print out some debugging info
  perror("Error SMART Status command failed");
  pout("Please get assistance from %s\n",PROJECTHOME);
  pout("Register values returned from SMART Status command are:\n");
  pout("CMD=0x%02x\n",parms[0]);
  pout("FR =0x%02x\n",parms[1]);
  pout("NS =0x%02x\n",parms[2]);
  pout("SC =0x%02x\n",parms[3]);
  pout("CL =0x%02x\n",parms[4]);
  pout("CH =0x%02x\n",parms[5]);
  pout("SEL=0x%02x\n",parms[6]);
  return -1;
}
#else
// Just a hack so that the code compiles on 
// 2.2 kernels without HDIO_DRIVE TASK support.  
// Should be fixed by putting in a call to code 
// that compares smart data to thresholds.
int ataSmartStatus2(int device){
  return ataSmartStatus(device);
}
#endif


// This is the way to execute ALL tests: offline, short self-test,
// extended self test, with and without captive mode, etc.
int ataSmartTest(int device, int testtype){	
  unsigned char parms[4] = {WIN_SMART, 0, SMART_IMMEDIATE_OFFLINE};
  char cmdmsg[128],*type,*captive;
  int errornum;

  parms[1]=testtype;

  // Set up strings that describe the type of test
  if (testtype==SHORT_CAPTIVE_SELF_TEST || testtype==EXTEND_CAPTIVE_SELF_TEST)
    captive="captive";
  else
    captive="off-line";

  if (testtype==OFFLINE_FULL_SCAN)
    type="off-line";
  else  if (testtype==SHORT_SELF_TEST || testtype==SHORT_CAPTIVE_SELF_TEST)
    type="Short self-test";
  else 
    type="Extended self-test";

  //  Print ouf message that we are sending the command to test
  if (testtype==ABORT_SELF_TEST)
    sprintf(cmdmsg,"Abort SMART off-line mode self-test routine");
  else
    sprintf(cmdmsg,"Execute SMART %s routine immediately in %s mode",type,captive);
  pout("Sending command: \"%s\".\n",cmdmsg);

  // Now send the command to test
  errornum=ioctl(device, HDIO_DRIVE_CMD, parms);
  if (errornum && !((testtype=SHORT_CAPTIVE_SELF_TEST || testtype==EXTEND_CAPTIVE_SELF_TEST) && errno==EIO)){
    char errormsg[128];
    sprintf(errormsg,"Command \"%s\" failed",cmdmsg); 
    perror(errormsg);
    fprintf(stderr,"\n");
    return -1;
  }
  
  // Since the command succeeded, tell user
  if (testtype==ABORT_SELF_TEST)
    pout("Self-testing aborted!\n");
  else
    pout("Drive command \"%s\" successful.\nTesting has begun.\n",cmdmsg);
  return 0;
}

/* Test Time Functions */
int TestTime(struct ata_smart_values data,int testtype){
  switch (testtype){
  case OFFLINE_FULL_SCAN:
    return (int) data.total_time_to_complete_off_line;
  case SHORT_SELF_TEST:
  case SHORT_CAPTIVE_SELF_TEST:
    return (int) data.short_test_completion_time;
  case EXTEND_SELF_TEST:
  case EXTEND_CAPTIVE_SELF_TEST:
    return (int) data.extend_test_completion_time;
  default:
    return 0;
  }
}


int isSmartErrorLogCapable ( struct ata_smart_values data){
   return data.errorlog_capability & 0x01;
}
int isSupportExecuteOfflineImmediate ( struct ata_smart_values data){
   return data.offline_data_collection_capability & 0x01;
}
int isSupportAutomaticTimer ( struct ata_smart_values data){
   return data.offline_data_collection_capability & 0x02;
}
int isSupportOfflineAbort ( struct ata_smart_values data){
   return data.offline_data_collection_capability & 0x04;
}
int isSupportOfflineSurfaceScan ( struct ata_smart_values data){
   return data.offline_data_collection_capability & 0x08;
}
int isSupportSelfTest (struct ata_smart_values data){
   return data.offline_data_collection_capability & 0x10;
}


// Loop over all valid attributes.  If they are prefailure attributes
// and are at or below the threshold value, then return the ID of the
// first failing attribute found.  Return 0 if all prefailure
// attributes are in bounds.  The spec says "Bit 0
// -Pre-failure/advisory - If the value of this bit equals zero, an
// attribute value less than or equal to its corresponding attribute
// threshold indicates an advisory condition where the usage or age of
// the device has exceeded its intended design life period. If the
// value of this bit equals one, an atribute value less than or equal
// to its corresponding attribute threshold indicates a pre-failure
// condition where imminent loss of data is being predicted."


// onlyfailed=0 : are or were any age or prefailure attributes <= threshold
// onlyfailed=1:  are any prefailure attributes <= threshold now
int ataCheckSmart(struct ata_smart_values data,
		  struct ata_smart_thresholds thresholds,
		  int onlyfailed){
  int i;
  
  // loop over all attributes
  for (i=0; i<NUMBER_ATA_SMART_ATTRIBUTES; i++){

    // pointers to disk's values and vendor's thresholds
    struct ata_smart_attribute *disk=data.vendor_attributes+i;
    struct ata_smart_threshold_entry *thre=thresholds.thres_entries+i;
 
    // consider only valid attributes
    if (disk->id && thre->id){
      int failednow,failedever;
      
      failednow =disk->current <= thre->threshold;
      failedever=disk->worst   <= thre->threshold;
      
      if (!onlyfailed && failedever)
	return disk->id;
      
      if (onlyfailed && failednow && disk->status.flag.prefailure)
	return disk->id;      
    }
  }
  return 0;
}



// This checks the n'th attribute in the attribute list, NOT the
// attribute with id==n.  If the attribute does not exist, or the
// attribute is > threshold, then returns zero.  If the attribute is
// <= threshold (failing) then we the attribute number if it is a
// prefail attribute.  Else we return minus the attribute number if it
// is a usage attribute.
int ataCheckAttribute(struct ata_smart_values *data,
		      struct ata_smart_thresholds *thresholds,
		      int n){
  struct ata_smart_attribute *disk;
  struct ata_smart_threshold_entry *thre;
  
  if (n<0 || n>=NUMBER_ATA_SMART_ATTRIBUTES || !data || !thresholds)
    return 0;
  
  // pointers to disk's values and vendor's thresholds
  disk=data->vendor_attributes+n;
  thre=thresholds->thres_entries+n;

  if (!disk || !thre)
    return 0;
  
  // consider only valid attributes, check for failure
  if (!disk->id || !thre->id || (disk->id != thre->id) || disk->current> thre->threshold)
    return 0;
  
  // We have found a failed attribute.  Return positive or negative? 
  if (disk->status.flag.prefailure)
    return disk->id;
  else
    return -1*(disk->id);
}

// Note some attribute names appear redundant because different
// manufacturers use different attribute IDs for an attribute with the
// same name.
void ataPrintSmartAttribName(char *out, unsigned char id){
  char *name;
  switch (id){
    
  case 1:
    name="Raw_Read_Error_Rate";
    break;
  case 2:
    name="Throughput_Performance";
    break;
  case 3:
    name="Spin_Up_Time";
    break;
  case 4:
    name="Start_Stop_Count";
    break;
  case 5:
    name="Reallocated_Sector_Ct";
    break;
  case 6:
    name="Read_Channel_Margin";
    break;
  case 7:
    name="Seek_Error_Rate";
    break;
  case 8:
    name="Seek_Time_Performance";
    break;
  case 9:
    name="Power_On_Hours";
    break;
  case 10:
    name="Spin_Retry_Count";
    break;
  case 11:
    name="Calibration_Retry_Count";
    break;
  case 12:
    name="Power_Cycle_Count";
    break;
  case 13:
    name="Read_Soft_Error_Rate";
    break;
  case 191:
    name="G-Sense_Error_Rate";
    break;
  case 192:
    name="Power-Off_Retract_Count";
    break;
  case 193:
    name="Load_Cycle_Count";
    break;
  case 194:
    name="Temperature_Centigrade";
    break;
  case 195:
    name="Hardware_ECC_Recovered";
    break;
  case 196:
    name="Reallocated_Event_Count";
    break;
  case 197:
    name="Current_Pending_Sector";
    break;
  case 198:
    name="Offline_Uncorrectable";
    break;
  case 199:
    name="UDMA_CRC_Error_Count";
    break;
  case 220:
    // Note -- this is also apparently used for temperature.
    name="Disk_Shift";
    break;
  case 221:
    name="G-Sense_Error_Rate";
    break;
  case 222:
    name="Loaded_Hours";
    break;
  case 223:
    name="Load_Retry_Count";
    break;
  case 224:
    name="Load_Friction";
    break;
  case 225:
    name="Load_Cycle_Count";
    break;
  case 226:
    name="Load-in_Time";
    break;
  case 227:
    name="Torq-amp_Count";
    break;
  case 228:
    name="Power-off_Retract_Count";
    break;
  case 231:
    name="Temperature_Centigrade";
    break;
  default:
    name="Unknown_Attribute";
    break;
  }
  sprintf(out,"%3hhu %s",id,name);
  return;
}


// These are two utility functions for printing CVS IDs. They don't
// really belong here.  But it's the only common source file included
// in both smartd and smartctl.  returns distance that it has moved
// ahead in the input string
int massagecvs(char *out, const char *cvsid){
  char *copy,*filename,*date,*version;
  const char delimiters[] = " ,$";

  // make a copy on stack, go to first token,
  if (!(copy=strdup(cvsid)) || !(filename=strtok(copy, delimiters))) 
    return 0;

  // move to first instance of "Id:"
  while (strcmp(filename,"Id:"))
    if (!(filename=strtok(NULL, delimiters)))
      return 0;

  // get filename, skip "v", get version and date
  if (!(  filename=strtok(NULL, delimiters)  ) ||
      !(           strtok(NULL, delimiters)  ) ||
      !(   version=strtok(NULL, delimiters)  ) ||
      !(      date=strtok(NULL, delimiters)  ) )
    return 0;

   sprintf(out,"%-13s revision: %-6s date: %-15s", filename, version, date);
   free(copy);
   return  (date-copy)+strlen(date);
}

// prints a single set of CVS ids
void printone(char *block, const char *cvsid){
  char strings[CVSMAXLEN];
  const char *here=cvsid;
  int line=1,len=strlen(cvsid)+1;

  // check that the size of the output block is sufficient
  if (len>=CVSMAXLEN) {
    fprintf(stderr,"CVSMAXLEN=%d must be at least %d\n",CVSMAXLEN,len+1);
    exit(1);
  }

  // loop through the different strings
  while ((len=massagecvs(strings,here))){
    switch (line++){
    case 1:
      block+=snprintf(block,CVSMAXLEN,"Module:");
      break;
    default:
      block+=snprintf(block,CVSMAXLEN,"  uses:");
    } 
    block+=snprintf(block,CVSMAXLEN," %s\n",strings);
    here+=len;
  }
  return;
}
