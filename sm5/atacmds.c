/*
 * atacmds.c
 * 
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-3 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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
#include <errno.h>
#include <stdlib.h>
#include "atacmds.h"
#include "utility.h"

const char *atacmds_c_cvsid="$Id: atacmds.c,v 1.56 2003/03/06 07:27:15 ballen4705 Exp $" ATACMDS_H_CVSID UTILITY_H_CVSID;

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

const char *vendorattributeargs[] = {
  // 0
  "9,minutes",
  // 1
  "9,seconds",
  // 2
  "9,temp",
  // 3
  "220,temp",
  // NULL should always terminate the array
  NULL
};

// This is a utility function for parsing pairs like "9,minutes" or
// "220,temp", and putting the correct flag into the attributedefs
// array.  Returns 1 if problem, 0 if pair has been recongized.
int parse_attribute_def(char *pair, unsigned char *defs){
  int i;

  // look along list and see if we find the pair
  for (i=0; vendorattributeargs[i] && strcmp(pair, vendorattributeargs[i]); i++);

  switch (i) {
  case 0:
    // attribute 9 is power on time in minutes
    defs[9]=1;
    return 0;
  case 1:
    // attribute 9 is power-on-time in seconds
    defs[9]=3;
    return 0;
  case 2:
    // attribute 9 is temperature in celsius
    defs[9]=2;
    return 0;
  case 3:
    // attribute 220 is temperature in celsius
    defs[220]=1;
    return 0;
  default:
    // pair not found
    return 1;
  }
}

// Function to return a string containing a list of the arguments in 
// vendorattributeargs[] separated by commas.  The strings themselves
// contain commas, so surrounding double quotes are added for clarity.
// This function allocates the required memory for the string and the
// caller must use free() to free it.  Returns NULL if the required
// memory can't be allocated.
char *create_vendor_attribute_arg_list(void){
  const char **ps;
  char *s;
  int len;

  // Calculate the required number of characters
  len = 1;                // At least one char ('\0')
  for (ps = vendorattributeargs; *ps != NULL; ps++) {
    len += strlen(*ps);   // For the actual argument string
    len += 2;             // For the surrounding double quotes
    if (*(ps+1))
      len += 2;           // For the ", " delimiter if required
  }

  // Attempt to allocate memory for the string
  if (!(s = (char *)malloc(len)))
    return NULL;

  // Construct the string
  *s = '\0';
  for (ps = vendorattributeargs; *ps != NULL; ps++) {
    strcat(s, "\"");
    strcat(s, *ps);
    strcat(s, "\"");
    if (*(ps+1))
      strcat(s, ", ");
  }

  // Return a pointer to the string
  return s;
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


// This function computes the checksum of a single disk sector (512
// bytes).  Returns zero if checksum is OK, nonzero if the checksum is
// incorrect.  The size (512) is correct for all SMART structures.
unsigned char checksum(unsigned char *buffer){
  unsigned char sum=0;
  int i;
  
  for (i=0; i<512; i++)
    sum+=buffer[i];

  return sum;
}

// Reads current Device Identity info (512 bytes) into buf
int ataReadHDIdentity (int device, struct hd_driveid *buf){
  unsigned short driveidchecksum;
  unsigned char parms[HDIO_DRIVE_CMD_HDR_SIZE+sizeof(*buf)]=
  {WIN_IDENTIFY, 0, 0, 1,};
  
  if (ioctl(device ,HDIO_DRIVE_CMD,parms)){
    // See if device responds to packet command...
    parms[0]=WIN_PIDENTIFY;
    if (ioctl(device ,HDIO_DRIVE_CMD,parms)){
      syserror("Error ATA GET HD Identity Failed");
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
  
  if ((driveidchecksum & 0x00ff) == 0x00a5 && checksum((unsigned char *)buf))
    checksumwarning("Drive Identity Structure");
  
  return 0;
}

// Returns ATA version as an integer, and a pointer to a string
// describing which revision.  Note that Revision 0 of ATA-3 does NOT
// support SMART.  For this one case we return -3 rather than +3 as
// the version number.  See notes above.
int ataVersionInfo (const char** description, struct hd_driveid *drive, unsigned short *minor){
  unsigned short major;
  int i;
  
  // get major and minor ATA revision numbers
#ifdef __NEW_HD_DRIVE_ID
  major=drive->major_rev_num;
  *minor=drive->minor_rev_num;
#else
  major=drive->word80;
  *minor=drive->word81;
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
int ataSmartSupport(struct hd_driveid *drive){
  unsigned short word82,word83;

  // get correct bits of IDENTIFY DEVICE structure
#ifdef __NEW_HD_DRIVE_ID
  word82=drive->command_set_1;
  word83=drive->command_set_2;
#else
  word82=drive->command_sets;
  word83=drive->word83;
#endif

  // Note this does not work for ATA3 < Revision 6, when word82 and word83 were added
  // we should check for ATA3 Rev 0 in minor identity code...  
  return (word83 & 0x0001<<14) && !(word83 & 0x0001<<15) && (word82 & 0x0001);
}

// returns 1 if SMART enabled, 0 if SMART disabled, -1 if can't tell
int ataIsSmartEnabled(struct hd_driveid *drive){
    unsigned short word85,word87;

  // Get correct bits of IDENTIFY DRIVE structure
#ifdef __NEW_HD_DRIVE_ID
  word85=drive->cfs_enable_1;
  word87=drive->csf_default;
#else
  word85=drive->word85;
  word87=drive->word87;
#endif
  
  if ((word87 & 0x0001<<14) && !(word87 & 0x0001<<15))
    // word85 contains valid information, so
    return word85 & 0x0001;
  
  // Since we can't rely word85, we don't know if SMART is enabled.
  return -1;
}


// Reads SMART attributes into *data
int ataReadSmartValues(int device, struct ata_smart_values *data){	
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE]= 
    {WIN_SMART, 0, SMART_READ_VALUES, 1, };
  
  if (ioctl(device,HDIO_DRIVE_CMD,buf)){
    syserror("Error SMART Values Read failed");
    return -1;
  }

  // copy data
  memcpy(data,buf+HDIO_DRIVE_CMD_HDR_SIZE,ATA_SMART_SEC_SIZE);

  // compute checksum
  if (checksum((unsigned char *)data))
    checksumwarning("SMART Attribute Data Structure");
  
  return 0;
}


// Reads the Self Test Log (log #6)
int ataReadSelfTestLog (int device, struct ata_smart_selftestlog *data){	
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE] = 
    {WIN_SMART, 0x06, SMART_READ_LOG_SECTOR, 1,};
  
  // get data from device
  if (ioctl(device, HDIO_DRIVE_CMD, buf)){
    syserror("Error SMART Error Self-Test Log Read failed");
    return -1;
  }

  // copy data back to the user
  memcpy(data,buf+HDIO_DRIVE_CMD_HDR_SIZE, ATA_SMART_SEC_SIZE); 

  // compute its checksum, and issue a warning if needed
  if (checksum((unsigned char *)data))
    checksumwarning("SMART Self-Test Log Structure");
  
  return 0;
}

// Reads the Error Log (log #1)
int ataReadErrorLog (int device, struct ata_smart_errorlog *data){	
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE] = 
    {WIN_SMART, 0x01, SMART_READ_LOG_SECTOR, 1,};
  
  // get data from device
  if (ioctl(device,HDIO_DRIVE_CMD,buf)) {
    syserror("Error SMART Error Log Read failed");
    return -1;
  }
  
  //copy data back to user
  memcpy(data, buf+HDIO_DRIVE_CMD_HDR_SIZE, ATA_SMART_SEC_SIZE);
  
  // compute its checksum, and issue a warning if needed
  if (checksum((unsigned char *)data))
    checksumwarning("SMART ATA Error Log Structure");
  
  return 0;
}


int ataReadSmartThresholds (int device, struct ata_smart_thresholds *data){
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE] = 
    {WIN_SMART, 1, SMART_READ_THRESHOLDS, 1,};
  
  // get data from device
  if (ioctl(device ,HDIO_DRIVE_CMD, buf)){
    syserror("Error SMART Thresholds Read failed");
    return -1;
  }

  // copy data back to user
  memcpy(data,buf+HDIO_DRIVE_CMD_HDR_SIZE, ATA_SMART_SEC_SIZE);
  
  // compute its checksum, and issue a warning if needed
  if (checksum((unsigned char *)data))
    checksumwarning("SMART Attribute Thresholds Structure");
  
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
    syserror("Error SMART Thresholds Write failed");
    return -1;
  }
  return 0;
}

int ataEnableSmart (int device ){	
  unsigned char parms[4] = {WIN_SMART, 1, SMART_ENABLE, 0};
  
  if (ioctl (device, HDIO_DRIVE_CMD, parms)){
    syserror("Error SMART Enable failed");
    return -1;
  }
  return 0;
}

int ataDisableSmart (int device ){	
  unsigned char parms[4] = {WIN_SMART, 1, SMART_DISABLE, 0};
  
  if (ioctl(device, HDIO_DRIVE_CMD, parms)){
    syserror("Error SMART Disable failed");
    return -1;
  }  
  return 0;
}

int ataEnableAutoSave(int device){
  unsigned char parms[4] = {WIN_SMART, 241, SMART_AUTOSAVE, 0};
  
  if (ioctl(device, HDIO_DRIVE_CMD, parms)){
    syserror("Error SMART Enable Auto-save failed");
    return -1;
  }
  return 0;
}

int ataDisableAutoSave(int device){
  unsigned char parms[4] = {WIN_SMART, 0, SMART_AUTOSAVE, 0};
  
  if (ioctl(device, HDIO_DRIVE_CMD, parms)){
    syserror("Error SMART Disable Auto-save failed");
    return -1;
  }
  return 0;
}

// Note that in the ATA-5 standard the Enable/Disable AutoOffline
// command is marked "OBSOLETE".  Curiously, I could not find it
// documented in ANY of the ATA specifications.  In other words, it's
// been obsolete forever. However some vendors (eg, IBM) seem to be
// using this command anyway.  For example see the IBM Travelstar
// 40GNX hard disk drive specifications page 164 Revision 1.1 22 Apr
// 2002.  This gives a detailed description of the command, although
// the drive claims to comply with the ATA/ATAPI-5 Revision 3
// standard!  The latter document makes no mention of this command at
// all, other than to say that it is "obsolete".
int ataEnableAutoOffline (int device ){	
  
  /* timer hard coded to 4 hours */
  unsigned char parms[4] = {WIN_SMART, 248, SMART_AUTO_OFFLINE, 0};
  
  if (ioctl(device , HDIO_DRIVE_CMD, parms)){
    syserror("Error SMART Enable Automatic Offline failed");
    return -1;
  }
  return 0;
}

// Another Obsolete Command.  See comments directly above, associated
// with the corresponding Enable command.
int ataDisableAutoOffline (int device ){	
  unsigned char parms[4] = {WIN_SMART, 0, SMART_AUTO_OFFLINE, 0};
  
  if (ioctl(device , HDIO_DRIVE_CMD, parms)){
    syserror("Error SMART Disable Automatic Offline failed");
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
     syserror("Error Return SMART Status via HDIO_DRIVE_CMD failed");
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
    syserror("Error SMART Status command via HDIO_DRIVE_TASK failed");
    return -1;
  }
  
  // Cyl low and Cyl high unchanged means "Good SMART status"
  if (parms[4]==normal_cyl_lo && parms[5]==normal_cyl_hi)
    return 0;
  
  // These values mean "Bad SMART status"
  if (parms[4]==failed_cyl_lo && parms[5]==failed_cyl_hi)
    return 1;

  // We haven't gotten output that makes sense; print out some debugging info
  syserror("Error SMART Status command failed");
  pout("Please get assistance from %s\n",PROJECTHOME);
  pout("Register values returned from SMART Status command are:\n");
  pout("CMD=0x%02x\n",(int)parms[0]);
  pout("FR =0x%02x\n",(int)parms[1]);
  pout("NS =0x%02x\n",(int)parms[2]);
  pout("SC =0x%02x\n",(int)parms[3]);
  pout("CL =0x%02x\n",(int)parms[4]);
  pout("CH =0x%02x\n",(int)parms[5]);
  pout("SEL=0x%02x\n",(int)parms[6]);
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
    syserror(errormsg);
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
int TestTime(struct ata_smart_values *data,int testtype){
  switch (testtype){
  case OFFLINE_FULL_SCAN:
    return (int) data->total_time_to_complete_off_line;
  case SHORT_SELF_TEST:
  case SHORT_CAPTIVE_SELF_TEST:
    return (int) data->short_test_completion_time;
  case EXTEND_SELF_TEST:
  case EXTEND_CAPTIVE_SELF_TEST:
    return (int) data->extend_test_completion_time;
  default:
    return 0;
  }
}

// This function tells you both about the ATA error log and the
// self-test error log capability.  The bit is poorly documented in
// the ATA/ATAPI standard.
int isSmartErrorLogCapable ( struct ata_smart_values *data){
   return data->errorlog_capability & 0x01;
}
int isSupportExecuteOfflineImmediate ( struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x01;
}

// Note in the ATA-5 standard, the following bit is listed as "Vendor
// Specific".  So it may not be reliable. The only use of this that I
// have found is in IBM drives, where it is well-documented.  See for
// example page 170, section 13.32.1.18 of the IBM Travelstar 40GNX
// hard disk drive specifications page 164 Revision 1.1 22 Apr 2002.
int isSupportAutomaticTimer ( struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x02;
}
int isSupportOfflineAbort ( struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x04;
}
int isSupportOfflineSurfaceScan ( struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x08;
}
int isSupportSelfTest (struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x10;
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
int ataCheckSmart(struct ata_smart_values *data,
		  struct ata_smart_thresholds *thresholds,
		  int onlyfailed){
  int i;
  
  // loop over all attributes
  for (i=0; i<NUMBER_ATA_SMART_ATTRIBUTES; i++){

    // pointers to disk's values and vendor's thresholds
    struct ata_smart_attribute *disk=data->vendor_attributes+i;
    struct ata_smart_threshold_entry *thre=thresholds->thres_entries+i;
 
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


// This routine prints the raw value of an attribute as a text string
// into out. It also returns this 48-bit number as a long long.  The
// array defs[] contains non-zero values if particular attributes have
// non-default interpretations.

long long ataPrintSmartAttribRawValue(char *out, 
				      struct ata_smart_attribute *attribute,
				      unsigned char *defs){
  long long rawvalue;
  int j;
  
  // convert the six individual bytes to a long long (8 byte) integer.
  // This is the value that we'll eventually return.
  rawvalue = 0;
  for (j=0; j<6; j++) {
    // This looks a bit roundabout, but is necessary.  Don't
    // succumb to the temptation to use raw[j]<<(8*j) since under
    // the normal rules this will be promoted to the native type.
    // On a 32 bit machine this might then overflow.
    long long temp;
    temp = attribute->raw[j];
    temp <<= 8*j;
    rawvalue |= temp;
  }
  
  // This switch statement is where we handle Raw attributes
  // that are stored in an unusual vendor-specific format,
  switch (attribute->id){
    // Spin-up time
  case 3:
    {
      int i, spin[2];
      // construct two twy-byte quantities, print first
      for (i=0; i<2; i++){
	spin[i] = attribute->raw[2*i+1];
	spin[i] <<= 8;
	spin[i] |= attribute->raw[2*i];
      }
      out+=sprintf(out, "%d", spin[0]);
      
      // if second nonzero then it stores the average spin-up time
      if (spin[1])
	sprintf(out, " (Average %d)", spin[1]);
    }
    break;
    // Power on time
  case 9:
    if (defs[9]==1){
      // minutes
      long long tmp1=rawvalue/60;
      long long tmp2=rawvalue%60;
      sprintf(out, "%lluh+%02llum", tmp1, tmp2);
    }
    else if (defs[9]==3){
      // seconds
      long long hours=rawvalue/3600;
      long long minutes=(rawvalue-3600*hours)/60;
      long long seconds=rawvalue%60;
      sprintf(out, "%lluh+%02llum+%02llus", hours, minutes, seconds);
    }
    else
      // hours
      sprintf(out, "%llu", rawvalue);  //stored in hours
    break;
    // Temperature
  case 194:
    out+=sprintf(out, "%d", (int)attribute->raw[0]);
    if (!(rawvalue==attribute->raw[0]))
      // The other bytes are in use. Try IBM's model
      sprintf(out, " (Lifetime Min/Max %d/%d)",(int)attribute->raw[2],
	      (int)attribute->raw[4]);
    break;
  default:
    sprintf(out, "%llu", rawvalue);
  }

  // Return the full value
  return rawvalue;
}


// Note some attribute names appear redundant because different
// manufacturers use different attribute IDs for an attribute with the
// same name.  The array defs[] contains non-zero values if particular
// attributes have non-default interpretations.
void ataPrintSmartAttribName(char *out, unsigned char id, unsigned char *defs){
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
    switch (defs[id]) {
    case 1:
      name="Power_On_Minutes";
      break;
    case 2:
      name="Temperature_Celsius";
      break;
    case 3:
      name="Power_On_Seconds";
      break;
    default:
      name="Power_On_Hours";
      break;
    }
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
    name="Temperature_Celsius";
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
  case 200:
    // Western Digital
    name="Multi_Zone_Error_Rate";
    break;
  case 220:
    switch (defs[id]) {
    case 1:
      name="Temperature_Celsius";
      break;
    default:
      name="Disk_Shift";
      break;
    }
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
  case 230:
    // seen in IBM DTPA-353750
    name="Head Amplitude";
    break;
  case 231:
    name="Temperature_Celsius";
    break;
  case 240:
    name="Head flying hours";
    break;
  default:
    name="Unknown_Attribute";
    break;
  }
  sprintf(out,"%3hhu %s",id,name);
  return;
}
