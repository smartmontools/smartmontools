//  $Id: atacmds.cpp,v 1.2 2002/10/11 09:20:32 ballen4705 Exp $
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
 */


#include <stdio.h>
#include <string.h>
#include "atacmds.h"

int ataReadHDIdentity ( int device, struct hd_driveid *buf)
{
   if (ioctl ( device , HDIO_GET_IDENTITY, buf ) != 0)
   { 
       perror ("ATA GET HD Failed");
       return -1; 
   }

   return 0;
}


int ataVersionInfo ( struct hd_driveid drive)
{
   int i;
   int atavalue = 0; 
	
#ifdef DEBUG
#ifdef __NEW_HD_DRIVE_ID
	printf ("Debug: Ata Version:	 %04x\n", drive.major_rev_num);
#else
	printf ("Debug: Ata Version:	 %04x\n", drive.word80);
#endif
#endif
 
#ifdef __NEW_HD_DRIVE_ID
	if ( (drive.major_rev_num == 0xffff) || (drive.major_rev_num == 0x000) )
#else
	if ( (drive.word80 == 0xffff) || (drive.word80 == 0x000) )
#endif
	{
		
		return -1;

	}

	for ( i = 1; i < 16; i++ )
	{
#ifdef __NEW_HD_DRIVE_ID
		if ( drive.major_rev_num &  ( 0x1 << i) )
#else
		
#endif
		{
			atavalue = i;
		}
	}
	return atavalue;
}




int ataSmartSupport ( struct hd_driveid drive)
{
#ifdef __NEW_HD_DRIVE_ID
	if ( drive.command_set_1 & 0x0001 ){
#else
	if ( drive.command_sets & 0x0001 ){
#endif
		return (1); /* drive supports S.M.A.R.T. and is disabled */
	}
			
	return (0);

}




int ataReadSmartValues (int device, struct ata_smart_values *data)
{	
   int i;
   unsigned char chksum;
   unsigned char buf[ HDIO_DRIVE_CMD_HDR_SIZE + 
                      ATA_SMART_SEC_SIZE] = 
                      { WIN_SMART, 0, SMART_READ_VALUES, 1};
	
   if (ioctl ( device , HDIO_DRIVE_CMD,  (unsigned char *) &buf ) != 0)
   {
       perror ("Smart Values Read failed");
       return -1;
   }
   chksum = 0;

   for ( i =  HDIO_DRIVE_CMD_HDR_SIZE;  
         i < ATA_SMART_SEC_SIZE + HDIO_DRIVE_CMD_HDR_SIZE; 
         i++ )
	chksum +=  buf[i];

   if (  (unsigned char) chksum != 0)
   {
       perror ("Smart Read Failed, Chksum error");
       return -1;
   }	

   memcpy( data, 
           &buf[HDIO_DRIVE_CMD_HDR_SIZE] ,
           ATA_SMART_SEC_SIZE );

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


int ataSmartStatus (int device )
{	
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
