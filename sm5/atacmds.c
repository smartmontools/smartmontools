//  $Id: atacmds.c,v 1.5 2002/10/14 15:26:05 ballen4705 Exp $
/*
 * atacmds.c
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
#include "atacmds.h"


// These Drive Identity tables are taken from hdparm 5.2. That's the
// "Gold Standard"
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
  "ATA-3 X3T10 2008D revision 0",		/* 0x0008	*/
  "ATA-2 X3T10 948D revision 3",		/* 0x0009	*/
  "ATA-3 published, ANSI X3.298-199x",		/* 0x000a	*/
  "ATA-3 X3T10 2008D revision 6",		/* 0x000b	*/
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

const char actual_ver[] = { 
  /* word 81 value: */
  0,		/* 0x0000	WARNING: 	*/
  1,		/* 0x0001	WARNING: 	*/
  1,		/* 0x0002	WARNING: 	*/
  1,		/* 0x0003	WARNING: 	*/
  2,		/* 0x0004	WARNING:   This array 		*/
  2,		/* 0x0005	WARNING:   corresponds 		*/
  3,		/* 0x0006	WARNING:   *exactly*		*/
  2,		/* 0x0007	WARNING:   to the ATA/		*/
  3,		/* 0x0008	WARNING:   ATAPI version	*/
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



int ataReadHDIdentity ( int device, struct hd_driveid *buf)
{
   if (ioctl ( device , HDIO_GET_IDENTITY, buf ) != 0)
   { 
       perror ("ATA GET HD Failed");
       return -1; 
   }

   return 0;
}


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


int ataSmartSupport ( struct hd_driveid drive){
#ifdef __NEW_HD_DRIVE_ID
  if (drive.command_set_1 & 0x0001)
#else
  if (drive.command_sets & 0x0001)
#endif
    return 1; /* drive supports S.M.A.R.T. and is disabled */
  return 0;
}

int ataReadSmartValues (int device, struct ata_smart_values *data){	
  int i;
  unsigned char chksum=0;
  unsigned char buf[HDIO_DRIVE_CMD_HDR_SIZE+ATA_SMART_SEC_SIZE]= 
    {WIN_SMART, 0, SMART_READ_VALUES, 1};
  
  if (ioctl(device,HDIO_DRIVE_CMD,buf)){
    perror ("Smart Values Read failed");
    return -1;
  }
  
  // compute checksum
  for (i=0;i<ATA_SMART_SEC_SIZE;i++)
    chksum+=buf[i+HDIO_DRIVE_CMD_HDR_SIZE];
  
  // verify that checksum vanishes
  if (chksum){
    perror ("Smart Read Failed, Checksum error!");
    return -1;
  }	
  
  // copy data and return
  memcpy(data,buf+HDIO_DRIVE_CMD_HDR_SIZE,ATA_SMART_SEC_SIZE);
  return 0;
}


int ataReadSelfTestLog (int device, struct ata_smart_selftestlog *data)
{	
   int i;
   unsigned char chksum=0;	
   unsigned char buf[ HDIO_DRIVE_CMD_HDR_SIZE + 
                      ATA_SMART_SEC_SIZE] = 
                      { WIN_SMART, 0x06, SMART_READ_LOG_SECTOR, 1};

   if (ioctl ( device , HDIO_DRIVE_CMD, (unsigned char *) &buf ) != 0)
   {
       perror ("Smart Error Log Read failed");
       return -1;
   }
   
   // compute checksum
   for (i=0;i<ATA_SMART_SEC_SIZE;i++)
     chksum+=buf[HDIO_DRIVE_CMD_HDR_SIZE+i];

   if (chksum){
     fprintf(stderr,"Smart Self Test Log Checksum Incorrect!\n");
     return -1;
   }

   memcpy( data, &buf[HDIO_DRIVE_CMD_HDR_SIZE] , ATA_SMART_SEC_SIZE);

   return 0;
}


int ataReadErrorLog (int device, struct ata_smart_errorlog *data)
{	
   int i;
   unsigned char chksum=0;	
   unsigned char buf[ HDIO_DRIVE_CMD_HDR_SIZE + 
                      ATA_SMART_SEC_SIZE] = 
                      { WIN_SMART, 0x01, SMART_READ_LOG_SECTOR, 1};

   if (ioctl ( device , HDIO_DRIVE_CMD, (unsigned char *) &buf ) != 0)
   {
       perror ("Smart Error Log Read failed");
       return -1;
   }

   // compute checksum
   for (i=0;i<ATA_SMART_SEC_SIZE;i++)
     chksum+=buf[HDIO_DRIVE_CMD_HDR_SIZE+i];

   if (chksum){
     fprintf(stderr,"Smart Error Log Checksum Incorrect!\n");
     return -1;
   }
   memcpy( data, &buf[HDIO_DRIVE_CMD_HDR_SIZE] , ATA_SMART_SEC_SIZE);

   return 0;
}


int ataReadSmartThresholds ( int device, struct ata_smart_thresholds *data)
{	
   unsigned char buf[ HDIO_DRIVE_CMD_HDR_SIZE + 
                      ATA_SMART_SEC_SIZE] = 
                      { WIN_SMART, 1, SMART_READ_THRESHOLDS, 1};
	
   if (ioctl ( device , HDIO_DRIVE_CMD, (unsigned char *) &buf ) != 0)
   {
       perror ("Smart Thresholds Read failed");
       return -1;
   }

   memcpy( data, &buf[HDIO_DRIVE_CMD_HDR_SIZE] , ATA_SMART_SEC_SIZE);

   return 0;
}

int ataSetSmartThresholds ( int device, struct ata_smart_thresholds *data)
{	
   unsigned char buf[ HDIO_DRIVE_CMD_HDR_SIZE + 
		           ATA_SMART_SEC_SIZE] = 
                           { WIN_SMART, 1, 0xD7, 1};
	
   memcpy( &buf[HDIO_DRIVE_CMD_HDR_SIZE],  data , ATA_SMART_SEC_SIZE);

   if (ioctl ( device , HDIO_DRIVE_CMD, (unsigned char *)  &buf ) != 0)
   {
       perror ("Smart Thresholds Read failed");
       return -1;
   }

   return 0;
}


int ataEnableSmart (int device )
{	

   unsigned char parms[4] = { WIN_SMART, 1, SMART_ENABLE, 0};
	
   if (ioctl ( device , HDIO_DRIVE_CMD,  &parms ) != 0)
   {
      perror ("Smart Enable failed");
      return -1;
   }
	
   return 0;
 }


int ataDisableSmart (int device )
{	
	
   unsigned char parms[4] = { WIN_SMART, 1, SMART_DISABLE, 0};
	
   if (ioctl ( device , HDIO_DRIVE_CMD,  &parms ) != 0)
   {
      perror ("Smart Disable failed");
      return -1;
   }

   return 0;
}

int ataEnableAutoSave(int device){
   unsigned char parms[4] = { WIN_SMART, 241, SMART_AUTOSAVE, 0};
	
   if (ioctl ( device , HDIO_DRIVE_CMD,  &parms ) != 0)
   {
      perror ("Smart Enable Auto-save failed");
      return -1;
   }
	
   return 0;
};

int ataDisableAutoSave(int device){
   unsigned char parms[4] = { WIN_SMART, 0, SMART_AUTOSAVE, 0};
	
   if (ioctl ( device , HDIO_DRIVE_CMD,  &parms ) != 0)
   {
      perror ("Smart Disable Auto-save failed");
      return -1;
   }
	
   return 0;
};

int ataEnableAutoOffline (int device )
{	

   /* timer hard coded to 4 hours */
   unsigned char parms[4] = { WIN_SMART, 248, SMART_AUTO_OFFLINE, 0};
	
   if (ioctl ( device , HDIO_DRIVE_CMD,  &parms ) != 0)
   {
       perror ("Smart Enable Automatic Offline failed");
       return -1;
   }

   return 0;
}


int ataDisableAutoOffline (int device )
{	
   unsigned char parms[4] = { WIN_SMART, 0, SMART_AUTO_OFFLINE, 0};
	
   if (ioctl ( device , HDIO_DRIVE_CMD,  &parms ) != 0)
   {
      perror ("Smart Disable Automatic Offline failed");
      return -1;
   }

   return 0;
}


// Not being used correctly.  Must examine the CL and CH registers to
// see what the smart status was.  How to fix this?  I don't know...
int ataSmartStatus (int device ){	
   unsigned char parms[4] = { WIN_SMART, 0, SMART_STATUS, 0};

   if (ioctl ( device , HDIO_DRIVE_CMD,  &parms) != 0)
   {
      return -1;
   }
   return 0;
}



int ataSmartTest (int device, int testtype)
{	
   unsigned char parms[4] = { WIN_SMART, testtype, 
                    SMART_IMMEDIATE_OFFLINE, 0};
   
   if (ioctl ( device , HDIO_DRIVE_CMD, &parms) != 0)
   {
       perror ("Smart Offline failed");
       return -1;
   }

   printf("Completed Off-line command\n");
	
   return 0;
}


int ataSmartOfflineTest (int device)
{	
   return ataSmartTest( device, OFFLINE_FULL_SCAN  );
}

int ataSmartShortSelfTest (int device)
{	
   return ataSmartTest( device, SHORT_SELF_TEST  );
}

int ataSmartExtendSelfTest (int device)
{	
   return ataSmartTest( device, EXTEND_SELF_TEST  );
}

int ataSmartShortCapSelfTest (int device)
{	
   return ataSmartTest( device, SHORT_CAPTIVE_SELF_TEST );
}

int ataSmartExtendCapSelfTest (int device)
{
   return ataSmartTest( device, EXTEND_CAPTIVE_SELF_TEST );
}


int ataSmartSelfTestAbort (int device)
{
   return ataSmartTest( device, 127 );
}

/* Test Time Functions */

int isOfflineTestTime ( struct ata_smart_values data)
{
   return (int) data.total_time_to_complete_off_line;
}


int isShortSelfTestTime ( struct ata_smart_values data)
{
   return (int) data.short_test_completion_time;
}


int isExtendedSelfTestTime ( struct ata_smart_values data)
{
   return (int) data.extend_test_completion_time;
}


int isSmartErrorLogCapable ( struct ata_smart_values data)
{
   return data.errorlog_capability & 0x01;
}


int isSupportExecuteOfflineImmediate ( struct ata_smart_values data)
{
   return data.offline_data_collection_capability & 0x01;
}


int isSupportAutomaticTimer ( struct ata_smart_values data)
{
   return data.offline_data_collection_capability & 0x02;
}


int isSupportOfflineAbort ( struct ata_smart_values data)
{
   return data.offline_data_collection_capability & 0x04;
}


int isSupportOfflineSurfaceScan ( struct ata_smart_values data)
{
   return data.offline_data_collection_capability & 0x08;
}


int isSupportSelfTest (struct ata_smart_values data)
{
   return data.offline_data_collection_capability & 0x10;
}


int ataCheckSmart ( struct ata_smart_values data, struct ata_smart_thresholds thresholds)
{
   int i;
  
   for ( i = 0 ; i < 30 ; i++ )
   {
      if ( (data.vendor_attributes[i].id !=0) &&   
           (thresholds.thres_entries[i].id != 0) &&
           (data.vendor_attributes[i].status.flag.prefailure) &&
           (data.vendor_attributes[i].current <
             thresholds.thres_entries[i].threshold) &&
           (thresholds.thres_entries[i].threshold != 0xFE) )
      {
         return i;
      }
   }

   return 0;
}
