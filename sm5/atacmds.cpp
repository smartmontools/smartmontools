//  $Id: atacmds.cpp,v 1.10 2002/10/20 19:40:23 ballen4705 Exp $
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
#include "atacmds.h"

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
  "ATA-3 X3T10 2008D revision 1",		/* 0x0006	*/
  "ATA-2 X3T10 948D revision 2k",		/* 0x0007	*/
  "ATA-3 X3T10 2008D revision 0",		/* 0x0008	*/ /* SMART NOT INCLUDED */
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

const int actual_ver[] = { 
  /* word 81 value: */
  0,		/* 0x0000	WARNING: 	*/
  1,		/* 0x0001	WARNING: 	*/
  1,		/* 0x0002	WARNING: 	*/
  1,		/* 0x0003	WARNING: 	*/
  2,		/* 0x0004	WARNING:   This array 		*/
  2,		/* 0x0005	WARNING:   corresponds 		*/
  3,		/* 0x0006	WARNING:   *exactly*		*/
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
  printf("Warning! %s error: invalid checksum.\n",string);
  fprintf(stderr,"Warning! %s error: invalid checksum.\n",string);
  syslog(LOG_INFO,"Warning! %s error: invalid checksum.\n",string);
  return;
}

// We no longer use this function, because the IOCTL appears to return
// only the drive identity at the time that the system was booted
// (perhaps from the BIOS.  It doesn't correctly reflect the current
// state information, and for example the checksum is usually
// wrong. The replacement function follows afterwards
#if (0)
int ataReadHDIdentity ( int device, struct hd_driveid *buf){
  if (ioctl(device, HDIO_GET_IDENTITY, buf)){ 
    perror ("ATA GET HD Failed");
    return -1; 
  }
  return 0;
}
#endif


// Reads current Device Identity info (512 bytes) into buf
int ataReadHDIdentity (int device, struct hd_driveid *buf){
  unsigned char parms[HDIO_DRIVE_CMD_HDR_SIZE+sizeof(*buf)]=
    {WIN_IDENTIFY, 0, 0, 1,};

  if (ioctl(device ,HDIO_DRIVE_CMD,parms)){ 
    perror ("ATA GET HD Identity Failed");
    return -1; 
  }
  
  // copy data into driveid structure
  memcpy(buf,parms+HDIO_DRIVE_CMD_HDR_SIZE,sizeof(*buf));
  
  // Note -- the declaration that appears in
  // /usr/include/linux/hdregs.h: short words160_255[95], is WRONG.
  // It should say: short words160_255[96]. I have written to Andre
  // Hedrick about this on Oct 17 2002.  Please remove this comment
  // once the fix has made it into the stock kernel tree.
  if ((buf->words160_255[95] & 0x00ff) == 0x00a5){
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
int ataVersionInfo (const char** description, struct hd_driveid drive){
  unsigned short major,minor;
  int i,atavalue=0;
  
  // get major and minor ATA revision numbers
#ifdef __NEW_HD_DRIVE_ID
  major=drive.major_rev_num;
  minor=drive.minor_rev_num;
#else
  major=drive.word80;
  minor=drive.word81;
#endif
  
  // First check if device has ANY ATA version information in it
  if (major==NOVAL_0 || major==NOVAL_1) {
    *description=NULL;
    return -1;
  }
  
  // The minor revision number has more information - try there first
  if (minor && (minor<=MINOR_MAX)){
    int std = actual_ver[minor];
    if (std) {
      *description=minor_str[minor];
      return std;
    }
  }
  
  // HDPARM has a very complicated algorithm from here on. Since SMART only
  // exists on ATA-3 and later standards, let's punt on this.  If you don't
  // like it, please fix it.  The code's in CVS.
  for (i=1; i<16;i++ )
    if (major & (0x1<<i))
      atavalue = i;
  *description=NULL;
  return atavalue;
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
    {WIN_SMART, 0, SMART_READ_VALUES, 1};
  
  if (ioctl(device,HDIO_DRIVE_CMD,buf)){
    perror ("SMART Values Read failed");
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
    perror ("SMART Error Log Read failed");
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
    perror ("SMART Error Log Read failed");
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


// This routine is marked as "Obsolete" in the ATA-5 spec, but it's
// very important for us.  Together with the SMART READ DATA command
// above, it's the only way for us to find out if the SMART status is
// good or not.  Hopefully this will get fixed -- I will find a way to
// get SMART Status directly.
int ataReadSmartThresholds ( int device, struct ata_smart_thresholds *data){
  int i;
  unsigned char chksum=0;	
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE] = 
    {WIN_SMART, 1, SMART_READ_THRESHOLDS, 1,};
  
  // get data from device
  if (ioctl(device ,HDIO_DRIVE_CMD, buf)){
    perror ("SMART Thresholds Read failed");
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
// alone and unused.
int ataSetSmartThresholds ( int device, struct ata_smart_thresholds *data){	
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE] = 
    {WIN_SMART, 1, 0xD7, 1,};
  
  memcpy(buf+HDIO_DRIVE_CMD_HDR_SIZE, data, ATA_SMART_SEC_SIZE);
  
  if (ioctl(device, HDIO_DRIVE_CMD, buf)){
    perror ("SMART Thresholds Read failed");
    return -1;
  }
  
  return 0;
}

int ataEnableSmart (int device ){	
  unsigned char parms[4] = {WIN_SMART, 1, SMART_ENABLE, 0};
  
  if (ioctl (device, HDIO_DRIVE_CMD, parms)){
    perror ("SMART Enable failed");
    return -1;
  }
  return 0;
}

int ataDisableSmart (int device ){	
  unsigned char parms[4] = {WIN_SMART, 1, SMART_DISABLE, 0};
  
  if (ioctl(device, HDIO_DRIVE_CMD, parms)){
    perror ("SMART Disable failed");
    return -1;
  }  
  return 0;
}

int ataEnableAutoSave(int device){
  unsigned char parms[4] = {WIN_SMART, 241, SMART_AUTOSAVE, 0};
  
  if (ioctl(device, HDIO_DRIVE_CMD, parms)){
    perror ("SMART Enable Auto-save failed");
    return -1;
  }
  return 0;
}

int ataDisableAutoSave(int device){
  unsigned char parms[4] = {WIN_SMART, 0, SMART_AUTOSAVE, 0};
  
  if (ioctl(device, HDIO_DRIVE_CMD, parms)){
    perror ("SMART Disable Auto-save failed");
    return -1;
  }
  return 0;
}

// Note that in the ATA-5 standard this command is marked "OBSOLETE"
int ataEnableAutoOffline (int device ){	
  
  /* timer hard coded to 4 hours */
  unsigned char parms[4] = {WIN_SMART, 248, SMART_AUTO_OFFLINE, 0};
  
  if (ioctl(device , HDIO_DRIVE_CMD, parms)){
    perror ("SMART Enable Automatic Offline failed");
    return -1;
  }
  return 0;
}

// Another Obsolete Command!
int ataDisableAutoOffline (int device ){	
  unsigned char parms[4] = {WIN_SMART, 0, SMART_AUTO_OFFLINE, 0};
  
  if (ioctl(device , HDIO_DRIVE_CMD, parms)){
    perror ("SMART Disable Automatic Offline failed");
    return -1;
  }
  return 0;
}


// Not being used correctly.  Must examine the CL and CH registers to
// see what the smart status was.  Look at ataSmartStatus2()
int ataSmartStatus (int device ){	
   unsigned char parms[4] = {WIN_SMART, 0, SMART_STATUS, 0};

   if (ioctl(device, HDIO_DRIVE_CMD, parms)){
     perror("Return SMART Status failed");
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


// This function needs to be properly tested and debugged.  I am not
// yet sure if this is right; have asked Andre for help.  May need to
// use IDE_DRIVE_TASK.  Does CONFIG_IDE_TASKFILE_IO need to be
// configured into the kernel?
int ataSmartStatus2(int device){
  unsigned char normal_cyl_lo=0x4f, normal_cyl_hi=0xc2;
  unsigned char failed_cyl_lo=0xf4, failed_cyl_hi=0x2c;

  unsigned char parms[HDIO_DRIVE_TASK_HDR_SIZE]=
    {WIN_SMART,    // CMD
     SMART_STATUS, // FR
     0,            // NS
     0,            // SC
     0,            // CL
     0,            // CH
     0             // SEL -- Andre, is this right?? Or should it be 1?
    };

  // load CL and CH values
  parms[4]=normal_cyl_lo;
  parms[5]=normal_cyl_hi;

  if (ioctl(device,HDIO_DRIVE_TASK,parms)){
    perror ("SMART Status command failed.");
    return -1;
  }
  
  // Cyl low and Cyl high unchanged means "Good SMART status"
  if (parms[4]==normal_cyl_lo && parms[5]==normal_cyl_hi)
    return 0;

  // These values mean "Bad SMART status"
  if (parms[4]==failed_cyl_lo && parms[5]==failed_cyl_hi)
    return 1;

  // We haven't gotten output that makes sense; print out some debugging info
  perror("SMART Status returned register values that don't make sense:\n");
  printf("CMD=0x%02x\n",parms[0]);
  printf("FR =0x%02x\n",parms[1]);
  printf("NS =0x%02x\n",parms[2]);
  printf("SC =0x%02x\n",parms[3]);
  printf("CL =0x%02x\n",parms[4]);
  printf("CH =0x%02x\n",parms[5]);
  printf("SEL=0x%02x\n",parms[6]);

  return -1;
}

// This is the way to execute ALL tests: offline, short self-test,
// extended self test, with and without captive mode, etc.
int ataSmartTest(int device, int testtype){	
  unsigned char parms[4] = {WIN_SMART, 0, SMART_IMMEDIATE_OFFLINE};
  char cmdmsg[128],*type,*captive;

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
  printf("Sending command: \"%s\".\n",cmdmsg);

  // Now send the command to test
  if (ioctl(device , HDIO_DRIVE_CMD, parms)){
    char errormsg[128];
    sprintf(errormsg,"Command \"%s\" failed.\n\n",cmdmsg); 
    perror (errormsg);
    return -1;
  }
  
  // Since the command succeeded, tell user
  if (testtype==ABORT_SELF_TEST)
    printf("Self-testing aborted!\n");
  else
    printf("Drive command \"%s\" successful.\nTesting has begun.\n",cmdmsg);
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
// and are below the threshold value, then return the index of the
// lowest failing attribute.  Return 0 if all prefailure attributes
// are in bounds.
int ataCheckSmart (struct ata_smart_values data, struct ata_smart_thresholds thresholds){
  int i;
  
  for (i = 0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++){
    if (data.vendor_attributes[i].id &&   
	thresholds.thres_entries[i].id &&
	data.vendor_attributes[i].status.flag.prefailure &&
	(data.vendor_attributes[i].current < thresholds.thres_entries[i].threshold) &&
	(thresholds.thres_entries[i].threshold != 0xFE))
      return i;
  }
  return 0;
}
