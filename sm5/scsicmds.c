//  $Id: scsicmds.c,v 1.8 2002/10/22 08:43:22 ballen4705 Exp $

/*
 * scsicmds.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
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
#include <getopt.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>
// We do NOT want to include the kernel SCSI header file, just user space one
#define  _LINUX_SCSI_H
#include <scsi/scsi.h>
#include "scsicmds.h"




UINT8 logsense (int device, UINT8 pagenum, UINT8 *pBuf)
{
  struct cdb10hdr *ioctlhdr;
  UINT8 tBuf[1024 + CDB_12_HDR_SIZE];
  UINT8 status;

  
  memset ( &tBuf, 0, 255);
    
  ioctlhdr = (struct cdb10hdr *) &tBuf;
  
  ioctlhdr->inbufsize = 0;
  ioctlhdr->outbufsize = 1024;
   
  ioctlhdr->cdb[0] = LOG_SENSE;
  ioctlhdr->cdb[1] = 0x00;
  ioctlhdr->cdb[2] = 0x40 | pagenum;
  ioctlhdr->cdb[3] = 0x00;
  ioctlhdr->cdb[4] = 0x00;
  ioctlhdr->cdb[5] = 0x00;
  ioctlhdr->cdb[6] = 0x00;
  ioctlhdr->cdb[7] = 0x04;
  ioctlhdr->cdb[8] = 0x00;
  ioctlhdr->cdb[9] = 0x00;

  status =  ioctl( device, 1 , &tBuf);

	
  memcpy ( pBuf, &tBuf[8], 1024); 

  return status;
  
}




UINT8 modesense (int device,  UINT8 pagenum, UINT8 *pBuf)
{
  
  UINT8 tBuf[CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE ];
 
  struct cdb6hdr *ioctlhdr;
  	
  UINT8 status;

  memset ( &tBuf, 0, CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE );
  
  ioctlhdr = (struct cdb6hdr *) &tBuf;
  
  ioctlhdr->inbufsize = 0;
  ioctlhdr->outbufsize = 0xff;
   
  ioctlhdr->cdb[0] = MODE_SENSE;
  ioctlhdr->cdb[1] = 0x00;
  ioctlhdr->cdb[2] = pagenum;
  ioctlhdr->cdb[3] = 0x00;
  ioctlhdr->cdb[4] = CDB_6_MAX_DATA_SIZE;
  ioctlhdr->cdb[5] = 0x00;
  
  
  status =  ioctl( device, 1 , &tBuf);
  
  memcpy ( pBuf, &tBuf[8], 256); 

  return status;

}




UINT8 modeselect (int device,  UINT8 pagenum, UINT8 *pBuf)
{
  struct cdb6hdr *ioctlhdr;
  UINT8 tBuf[CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE ];
  UINT8 status;

  memset ( &tBuf, 0, CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE );

  ioctlhdr = (struct cdb6hdr *) &tBuf;
  
  ioctlhdr->inbufsize = pBuf[0] + 1;
  ioctlhdr->outbufsize = 0;
  
  
  ioctlhdr->cdb[0] = MODE_SELECT;
  ioctlhdr->cdb[1] = 0x11;
  ioctlhdr->cdb[2] = 0x00;
  ioctlhdr->cdb[3] = 0x00;
  ioctlhdr->cdb[4] = pBuf[0] + 1;
  ioctlhdr->cdb[5] = 0x00;
  
  tBuf[CDB_6_HDR_SIZE + 3]  = 0x08;
  tBuf[CDB_6_HDR_SIZE + 10] = 0x02;
  
    
  memcpy ( &tBuf[ CDB_6_HDR_SIZE + MODE_DATA_HDR_SIZE],
			 pBuf +  MODE_DATA_HDR_SIZE,
			pBuf[0] - MODE_DATA_HDR_SIZE + 1);

  tBuf[26] &= 0x3f;		
 
  status = ioctl( device, 1 , &tBuf);

  return status;

}




UINT8 modesense10 (int device, UINT8 pagenum, UINT8 *pBuf)
{
  
    struct cdb10hdr *ioctlhdr;
  UINT8 tBuf[1024];
  UINT8 status;
	
  memset ( &tBuf, 0, 1024);
    
  ioctlhdr = (struct cdb10hdr *) &tBuf;
  
  ioctlhdr->inbufsize = 0;
  ioctlhdr->outbufsize = 0xff;
   
  ioctlhdr->cdb[0] = MODE_SELECT_10;
  ioctlhdr->cdb[1] = 0x00;
  ioctlhdr->cdb[2] = 0x11;
  ioctlhdr->cdb[3] = 0x00;
  ioctlhdr->cdb[4] = 0x00;
  ioctlhdr->cdb[5] = 0x00;
  ioctlhdr->cdb[6] = 0x00;
  ioctlhdr->cdb[7] = 0x00;
  ioctlhdr->cdb[8] = 0xff;
  ioctlhdr->cdb[9] = 0x00;

  status =  ioctl( device, 1 , &tBuf);
 
  memcpy ( pBuf, &tBuf[8], 0xff); 
  
  return status;

}




UINT8 modeselect10 (int device,  UINT8 pagenum, UINT8 *pBuf)
{
  struct cdb10hdr *ioctlhdr;
  UINT8 tBuf[CDB_10_MAX_DATA_SIZE + CDB_10_HDR_SIZE ];
  UINT8 status;

  memset ( &tBuf, 0, CDB_10_MAX_DATA_SIZE + CDB_10_HDR_SIZE );

  ioctlhdr = (struct cdb10hdr *) &tBuf;
  
  ioctlhdr->inbufsize = pBuf[0] + 1;
  ioctlhdr->outbufsize = 0;
  
  ioctlhdr->cdb[0] = MODE_SELECT_10;
  ioctlhdr->cdb[1] = 0x00;
  ioctlhdr->cdb[2] = pagenum;
  ioctlhdr->cdb[3] = 0x00;
  ioctlhdr->cdb[4] = 0x00;
  ioctlhdr->cdb[5] = 0x00;
  ioctlhdr->cdb[6] = 0x00;
  ioctlhdr->cdb[7] = 0x00;
  ioctlhdr->cdb[8] = pBuf[0] + 1;
  ioctlhdr->cdb[9] = 0x00;
  
  tBuf[CDB_10_HDR_SIZE + 3]  = 0x08;
  tBuf[CDB_10_HDR_SIZE + 10] = 0x02;
  
    
  memcpy ( &tBuf[ CDB_10_HDR_SIZE + MODE_DATA_HDR_SIZE],
			 pBuf +  MODE_DATA_HDR_SIZE,
			pBuf[0] - MODE_DATA_HDR_SIZE + 1);

  tBuf[26] &= 0x3f;		
 
  status = ioctl( device, 1 , &tBuf);

  return status;

}




UINT8 stdinquiry ( int device, UINT8 *pBuf)
{
 
  UINT8 tBuf[CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE ];
 
  struct cdb6hdr *ioctlhdr;
  	
  UINT8 status;

  memset ( &tBuf, 0, CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE );
  
  ioctlhdr = (struct cdb6hdr *) &tBuf;
  
  ioctlhdr->inbufsize = 0;
  ioctlhdr->outbufsize = CDB_6_MAX_DATA_SIZE;
   
  ioctlhdr->cdb[0] = INQUIRY;
  ioctlhdr->cdb[1] = 0x00;
  ioctlhdr->cdb[2] = 0x00;
  ioctlhdr->cdb[3] = 0x00;
  ioctlhdr->cdb[4] = CDB_6_MAX_DATA_SIZE;
  ioctlhdr->cdb[5] = 0x00;
  
  
  status =  ioctl( device, 1, &tBuf );
  
  memcpy ( pBuf, &tBuf[8], 255); 

  return status;

}




UINT8 inquiry ( int device, UINT8 pagenum, UINT8 *pBuf)
{
 
  UINT8 tBuf[CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE ];
 
  struct cdb6hdr *ioctlhdr;
  	
  UINT8 status;

  memset ( &tBuf, 0, CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE );
  
  ioctlhdr = (struct cdb6hdr *) &tBuf;
  
  ioctlhdr->inbufsize = 0;
  ioctlhdr->outbufsize = 0xff;
   
  ioctlhdr->cdb[0] = INQUIRY;
  ioctlhdr->cdb[1] = 0x01;
  ioctlhdr->cdb[2] = 0x00;
  ioctlhdr->cdb[3] = pagenum;
  ioctlhdr->cdb[4] = CDB_6_MAX_DATA_SIZE;
  ioctlhdr->cdb[5] = 0x00;
  
  
  status =  ioctl( device, 6 , &tBuf);
  /*status =  ioctl( device, 1 , &tBuf);*/
  
  memcpy ( pBuf, &tBuf[8], 255); 

  return status;

}




UINT8 requestsense (int device, UINT8 *pBuf)
{
    
  UINT8 tBuf[CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE ];
 
  struct cdb6hdr *ioctlhdr;
  	
  UINT8 status;

  memset ( &tBuf, 0, CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE );
  
  ioctlhdr = (struct cdb6hdr *) &tBuf;
  
  ioctlhdr->inbufsize = 0;
  ioctlhdr->outbufsize = 0xff;
   
  ioctlhdr->cdb[0] = REQUEST_SENSE;
  ioctlhdr->cdb[1] = 0x00;
  ioctlhdr->cdb[2] = 0x00;
  ioctlhdr->cdb[3] = 0x00;
  ioctlhdr->cdb[4] = CDB_6_MAX_DATA_SIZE;
  ioctlhdr->cdb[5] = 0x00;
  
  
  status =  ioctl( device, 1 , &tBuf);
  
  memcpy ( pBuf, &tBuf[8], 255); 

  return status;
}


UINT8 senddiagnostic (int device, UINT8 functioncode,  UINT8 *pBuf)
{
  UINT8 tBuf[CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE ];
 
  struct cdb6hdr *ioctlhdr;
  	
  UINT8 status;

  memset ( &tBuf, 0, CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE );
  
  ioctlhdr = (struct cdb6hdr *) &tBuf;
  
  ioctlhdr->inbufsize = 0;
  ioctlhdr->outbufsize = 0xff;
   
  ioctlhdr->cdb[0] = SEND_DIAGNOSTIC;
  
  if (functioncode != SCSI_DIAG_SELF_TEST)
	ioctlhdr->cdb[1] = ( functioncode <<5 ) | 0x10;
  
  ioctlhdr->cdb[2] = 0x00;
  ioctlhdr->cdb[3] = 0x00;
  ioctlhdr->cdb[4] = 0x00;
  ioctlhdr->cdb[5] = 0x00;
  
  if (pBuf != NULL)
  {
	  ioctlhdr->inbufsize = pBuf[0];	
      ioctlhdr->cdb[4] = pBuf[0];
	  memcpy ( &tBuf[CDB_6_HDR_SIZE],
				pBuf + 1,
				pBuf[0]);
  }

  status =  ioctl( device, 1 , &tBuf);
  
  if (pBuf != NULL)
  	memcpy ( pBuf, &tBuf[8], 256); 

  return status;

}



UINT8 receivediagnostic (int device, UINT8 pagenum,  UINT8 *pBuf)
{
  UINT8 tBuf[CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE ];
 
  struct cdb6hdr *ioctlhdr;
  	
  UINT8 status;

  memset ( &tBuf, 0, CDB_6_MAX_DATA_SIZE + CDB_6_HDR_SIZE );
  
  ioctlhdr = (struct cdb6hdr *) &tBuf;
  
  ioctlhdr->inbufsize = 0;
  ioctlhdr->outbufsize = 0xff;
   
  ioctlhdr->cdb[0] = RECEIVE_DIAGNOSTIC;
  
  ioctlhdr->cdb[1] = 0x01;
  
  ioctlhdr->cdb[2] = pagenum;
  ioctlhdr->cdb[3] = 0x00;
  ioctlhdr->cdb[4] = 0x00;
  ioctlhdr->cdb[5] = 0x00;
  

  status =  ioctl( device, 1 , &tBuf);
  
  memcpy ( pBuf, &tBuf[8], 256); 

  return status;

}


UINT8 testunitready (int device)
{
  return ioctl( device, 2 , NULL);

}




/* ModePage1C Handler */

#define SMART_SUPPORT	0x00	

UINT8 scsiSmartModePage1CHandler (int device, UINT8 setting, UINT8 *retval)
{
	char tBuf[CDB_6_MAX_DATA_SIZE];
	
	if (modesense ( device, 0x1c, (UINT8 *) &tBuf) != 0)
	{
		return 1;
	}
	
	switch (setting)
	{
		case DEXCPT_DISABLE:
 			tBuf[14] &= 0xf7;
			tBuf[15] = 0x04;
			break;
		case DEXCPT_ENABLE:
			tBuf[14] |= 0x08;
			break;
		case EWASC_ENABLE:
			tBuf[14] |= 0x10;
			break;
		case EWASC_DISABLE:
			tBuf[14] &= 0xef;
			break;
		case SMART_SUPPORT:
			*retval = tBuf[14] & 0x08;
			return 0;
			break;
		default:
			return 1;
	}
			
	if (modeselect ( device, 0x1c, (UINT8 *) &tBuf ) != 0)
	{
		return 1;
	}
	
	return 0;
}




UINT8 scsiSmartSupport (int device, UINT8 *retval)
{
	return scsiSmartModePage1CHandler( device, SMART_SUPPORT, retval);
}




UINT8 scsiSmartEWASCEnable (int device)
{
	return scsiSmartModePage1CHandler( device, EWASC_ENABLE, NULL);
}




UINT8 scsiSmartEWASCDisable (int device)
{
	return scsiSmartModePage1CHandler( device, EWASC_DISABLE, NULL);
}




UINT8 scsiSmartDEXCPTEnable (int device)
{
	return scsiSmartModePage1CHandler( device, DEXCPT_ENABLE, NULL);
}




UINT8 scsiSmartDEXCPTDisable (int device)
{
	return scsiSmartModePage1CHandler( device, DEXCPT_DISABLE, NULL);
}

UINT8 scsiGetTemp ( int device, UINT8 *currenttemp, UINT8 *triptemp)
{
    
   UINT8 tBuf[1024];

  if (logsense ( device , TEMPERATURE_PAGE, (UINT8 *) &tBuf) != 0)
  {
     perror ( "Log Sense failed");
     exit (1);
  }
  *currenttemp = tBuf[9];
  *triptemp = tBuf[15];
  return 0;
}

UINT8 scsiCheckSmart(int device, UINT8 method, UINT8 *retval,
                     UINT8 *currenttemp, UINT8 *triptemp)
{
   UINT8 tBuf[1024];
   UINT8 asc;
   UINT8 ascq;
   unsigned short pagesize;
 
   *currenttemp = *triptemp = 0;
  
   if ( method == CHECK_SMART_BY_LGPG_2F)
   {
      if (logsense ( device , SMART_PAGE, (UINT8 *) &tBuf) != 0)
      {
	perror ( "Log Sense failed");
	exit (1);
      }

      pagesize = (unsigned short) (tBuf[2] << 8) | tBuf[3];

      if ( !pagesize )
      {
	/* failed read of page 2F\n */
	return 1;
      } 

      asc  = tBuf[8]; 
      ascq = tBuf[9];

      if ( pagesize == 8 && (currenttemp != NULL) && (triptemp != NULL) )
      {
	*currenttemp = tBuf[10];
	*triptemp =  tBuf[11];
      }	

   }
   else
   {
      if (requestsense ( device , (UINT8 *) &tBuf) != 0)
      {
	perror ( "Request Sense failed");
	exit (1);
      }
      
      asc = tBuf[12]; 
      ascq = tBuf[13];
	
   }

   if ( asc == 0x5d )
	*retval = ascq;
   else
         *retval = 0;

   return 0;
}


char* scsiTapeAlertsTapeDevice ( unsigned short code)
{

#define NUMENTRIESINTAPEALERTSTABLE 54

    char *TapeAlertsMessageTable[]=
    {  " ",
   "The tape drive is having problems reading data. No data has been lost, but there has been a reduction in the performance of the tape.",
   "The tape drive is having problems writing data. No data has been lost, but there has been a reduction in the performance of the tape.",
   "The operation has stopped because an error has occurred while reading or writing data which the drive cannot correct.",
   "Your data is at risk:\n1. Copy any data you require from this tape. \n2. Do not use this tape again.\n3. Restart the operation with a different tape.",
   "The tape is damaged or the drive is faulty. Call the tape drive supplier helpline.",
   "The tape is from a faulty batch or the tape drive is faulty:\n1. Use a good tape to test the drive.\n2. If problem persists, call the tape drive supplier helpline.",
   "The tape cartridge has reached the end of its calculated useful life: \n1. Copy data you need to another tape.\n2. Discard the old tape.",
   "The tape cartridge is not data-grade. Any data you back up to the tape is at risk. Replace the cartridge with a data-grade tape.",
   "You are trying to write to a write-protected cartridge. Remove the write-protection or use another tape.",
   "You cannot eject the cartridge because the tape drive is in use. Wait until the operation is complete before ejecting the cartridge.",
   "The tape in the drive is a cleaning cartridge.",
   "You have tried to load a cartridge of a type which is not supported by this drive.",
   "The operation has failed because the tape in the drive has snapped:\n1. Discard the old tape.\n2. Restart the operation with a different tape.",
   "The operation has failed because the tape in the drive has snapped:\n1. Do not attempt to extract the tape cartridge\n2. Call the tape drive supplier helpline.",
   "The memory in the tape cartridge has failed, which reduces performance. Do not use the cartridge for further backup operations.",
   "The operation has failed because the tape cartridge was manually ejected while the tape drive was actively writing or reading.",
   "You have loaded of a type that is read-only in this drive. The cartridge will appear as write-protected.",
   "The directory on the tape cartridge has been corrupted. File search performance will be degraded. The tape directory can be rebuilt by reading all the data on the cartridge.",
   "The tape cartridge is nearing the end of its calculated life. It is recommended that you:\n1. Use another tape cartridge for your next backup.\n2.Store this tape in a safe place in case you need to restore data from it.",
   "The tape drive needs cleaning:\n1. If the operation has stopped, eject the tape and clean the drive.\n2. If the operation has not stopped, wait for it ti finish and then clean the drive. Check the tape drive users manual for device specific cleaning instructions.",
   "The tape drive is due for routine cleaning:\n1. Wait for the current operation to finish.\n2. The use a cleaning cartridge. Check the tape drive users manual for device specific cleaning instructions.",
   "The last cleaning cartridge used in the tape drive has worn out:\n1. Discard the worn out cleaning cartridge.\n2. Wait for the current operation to finish.\n 3.Then use a new cleaning cartridge.",
   "The last cleaning cartridge used in the tape drive was an invalid type:\n1. Do not use this cleaning cartridge in this drive.\n2. Wait for the current operation to finish.\n 3.Then use a new cleaning cartridge.",
   "The tape drive has requested a retention operation",
   "A redundant interface port on the tape drive has failed",
   "A tape drive cooling fan has failed",
   "A redundant power supply has failed inside the tape drive enclosure. Check the enclosure users manual for instructions on replacing the failed power supply.",
   "The tape drive power consumption is outside the specified range.",
   "Preventive maintenance of the tape drive is required. Check the tape drive users manual for device specific preventive maintenance tasks or call the tape drive supplier helpline.",
   "The tape drive has a hardware fault:\n1. Eject the tape or magazine.\n2. Reset the drive.\n3. Restart the operation.",
   "The tape drive has a hardware fault:\n1. Turn the tape drive off and then on again.\n2. Restart the operation.\n3. If the problem persists, call the tape drive supplier helpline.\n Check the tape drive users manual for device specific instructions on turning the device power in and off.",
   "The tape drive has a problem with the host interface:\n1. Check the cables and cable connections.\n2. Restart the operation.",
   "The operation has failed:\n1. Eject the tape or magazine.\n2. Insert the tape or magazine again.\n3. Restart the operation.",
   "The firmware download has failed because you have tried to use the incorrect firmware for this tape drive. Obtain the correct firmware and try again.",
   "Environmental conditions inside the tape drive are outside the specified humidity range.",
   "Environmental conditions inside the tape drive are outside the specified temperature range.",
   "The voltage supply to the tape drive is outside the specified range.",
   "A hardware failure of the tape drive is predicted. Call the tape drive supplier helpline.",
   "The tape drive may have a fault. Check for availability of diagnostic information and run extended diagnostics if applicable. Check the tape drive users manual for instruction on running extended diagnostic tests and retrieving diagnostic data",
   "The changer mechanism is having difficulty communicating with the tape drive:\n1. Turn the autoloader off then on.\n2. Restart the operation.\n3. If problem persists, call the tape drive supplier helpline.",
   "A tape has been left in the autoloader by a previous hardware fault:\n1. Insert an empty magazine to clear the fault.\n2. If the fault does not clear, turn the autoloader off and then on again.\n3. If the problem persists, call the tape drive supplier helpline.",
   "There is a problem with the autoloader mechanism.",
   "The operation has failed because the autoloader door is open:\n1. Clear any obstructions from the autoloader door.\n2. Eject the magazine and then insert it again.\n3. If the fault does not clear, turn the autoloader off and then on again.\n4. If the problem persists, call the tape drive supplier helpline.",
   "The autoloader has a hardware fault:\n1. Turn the autoloader off and then on again.\n2. Restart the operation.\n3. If the problem persists, call the tape drive supplier helpline.\n Check the autoloader users manual for device specific instructions on turning the device power on and off.",
   "The autoloader cannot operate without the magazine,\n1. Insert the magazine into the autoloader.\n 2. Restart the operation.",
   "A hardware failure of the changer mechanism is predicted. Call the tape drive supplier helpline.",
   " ",
   " ",
   " ",
   "Media statistics have been lost at some time in the past",
   "The tape directory on the tape cartridge just unloaded has been corrupted. File search performance will be degraded. The tape directory can be rebuilt by reading all the data.",
   "The tape just unloaded could not write its system area successfully:\n1. Copy data to another tape cartridge.\n2. Discard the old cartridge.",
   "The tape system are could not be read successfully at load time:\n1. Copy data to another tape cartridge.\n2. Discard the old cartridge.",
   "The start or data could not be found on the tape:\n1. Check you are using the correct format tape.\n2. Discard the tape or return the tape to you supplier",
    };
            
    return ( code > NUMENTRIESINTAPEALERTSTABLE)? "Unknown Alert" : TapeAlertsMessageTable[code];
}


char* scsiSmartGetSenseCode ( UINT8 ascq)
{

 char *smartsensetable [] =   {
	"FAILURE PREDICTION THRESHOLD EXCEEDED",
	"MEDIA FAILURE PREDICTION THRESHOLD EXCEEDED",
	"LOGICAL UNIT FAILURE PREDICTION THRESHOLD EXCEEDED",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"HARDWARE IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
	"HARDWARE IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
	"HARDWARE IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
	"HARDWARE IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
	"HARDWARE IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
	"HARDWARE IMPENDING FAILURE ACCESS TIMES TOO HIGH",
	"HARDWARE IMPENDING FAILURE START UNIT TIMES TOO HIGH",
	"HARDWARE IMPENDING FAILURE CHANNEL PARAMETRICS",
	"HARDWARE IMPENDING FAILURE CONTROLLER DETECTED",
	"HARDWARE IMPENDING FAILURE THROUGHPUT PERFORMANCE",
	"HARDWARE IMPENDING FAILURE SEEK TIME PERFORMANCE",
	"HARDWARE IMPENDING FAILURE SPIN-UP RETRY COUNT",
	"HARDWARE IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"CONTROLLER IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
	"CONTROLLER IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
	"CONTROLLER IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
	"CONTROLLER IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
	"CONTROLLER IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
	"CONTROLLER IMPENDING FAILURE ACCESS TIMES TOO HIGH",
	"CONTROLLER IMPENDING FAILURE START UNIT TIMES TOO HIGH",
	"CONTROLLER IMPENDING FAILURE CHANNEL PARAMETRICS",
	"CONTROLLER IMPENDING FAILURE CONTROLLER DETECTED",
	"CONTROLLER IMPENDING FAILURE THROUGHPUT PERFORMANCE",
	"CONTROLLER IMPENDING FAILURE SEEK TIME PERFORMANCE",
	"CONTROLLER IMPENDING FAILURE SPIN-UP RETRY COUNT",
	"CONTROLLER IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"DATA CHANNEL IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
	"DATA CHANNEL IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
	"DATA CHANNEL IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
	"DATA CHANNEL IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
	"DATA CHANNEL IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
	"DATA CHANNEL IMPENDING FAILURE ACCESS TIMES TOO HIGH",
	"DATA CHANNEL IMPENDING FAILURE START UNIT TIMES TOO HIGH",
	"DATA CHANNEL IMPENDING FAILURE CHANNEL PARAMETRICS",
	"DATA CHANNEL IMPENDING FAILURE CONTROLLER DETECTED",
	"DATA CHANNEL IMPENDING FAILURE THROUGHPUT PERFORMANCE",
	"DATA CHANNEL IMPENDING FAILURE SEEK TIME PERFORMANCE",
	"DATA CHANNEL IMPENDING FAILURE SPIN-UP RETRY COUNT",
	"DATA CHANNEL IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"SERVO IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
	"SERVO IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
	"SERVO IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
	"SERVO IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
	"SERVO IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
	"SERVO IMPENDING FAILURE ACCESS TIMES TOO HIGH",
	"SERVO IMPENDING FAILURE START UNIT TIMES TOO HIGH",
	"SERVO IMPENDING FAILURE CHANNEL PARAMETRICS",
	"SERVO IMPENDING FAILURE CONTROLLER DETECTED",
	"SERVO IMPENDING FAILURE THROUGHPUT PERFORMANCE",
	"SERVO IMPENDING FAILURE SEEK TIME PERFORMANCE",
	"SERVO IMPENDING FAILURE SPIN-UP RETRY COUNT",
	"SERVO IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"SPINDLE IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
	"SPINDLE IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
	"SPINDLE IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
	"SPINDLE IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
	"SPINDLE IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
	"SPINDLE IMPENDING FAILURE ACCESS TIMES TOO HIGH",
	"SPINDLE IMPENDING FAILURE START UNIT TIMES TOO HIGH",
	"SPINDLE IMPENDING FAILURE CHANNEL PARAMETRICS",
	"SPINDLE IMPENDING FAILURE CONTROLLER DETECTED",
	"SPINDLE IMPENDING FAILURE THROUGHPUT PERFORMANCE",
	"SPINDLE IMPENDING FAILURE SEEK TIME PERFORMANCE",
	"SPINDLE IMPENDING FAILURE SPIN-UP RETRY COUNT",
	"SPINDLE IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT",
	"Unknown Failure",
	"Unknown Failure",
	"Unknown Failure",
	"FIRMWARE IMPENDING FAILURE GENERAL HARD DRIVE FAILURE",
	"FIRMWARE IMPENDING FAILURE DRIVE ERROR RATE TOO HIGH",
	"FIRMWARE IMPENDING FAILURE DATA ERROR RATE TOO HIGH",
	"FIRMWARE IMPENDING FAILURE SEEK ERROR RATE TOO HIGH",
	"FIRMWARE IMPENDING FAILURE TOO MANY BLOCK REASSIGNS",
	"FIRMWARE IMPENDING FAILURE ACCESS TIMES TOO HIGH",
	"FIRMWARE IMPENDING FAILURE START UNIT TIMES TOO HIGH",
	"FIRMWARE IMPENDING FAILURE CHANNEL PARAMETRICS",
	"FIRMWARE IMPENDING FAILURE CONTROLLER DETECTED",
	"FIRMWARE IMPENDING FAILURE THROUGHPUT PERFORMANCE",
	"FIRMWARE IMPENDING FAILURE SEEK TIME PERFORMANCE",
	"FIRMWARE IMPENDING FAILURE SPIN-UP RETRY COUNT",
	"FIRMWARE IMPENDING FAILURE DRIVE CALIBRATION RETRY COUNT"};

	if ( ascq == 0xff)
		return "SMART Sense: False Alarm";
	else if ( ascq <= SMART_SENSE_MAX_ENTRY)
		return smartsensetable[ascq];
	else
		return "Unknown Failure";

}




UINT8 scsiSmartOfflineTest (int device)
{	
	UINT8 tBuf[256];
	
	memset ( &tBuf, 0, 256);

	/* Build SMART Off-line Immediate Diag Header */
	tBuf[0] = 8;    /* Buffer Length n-1 */
	tBuf[1] = 0x80; /* Page Code */
	tBuf[2] = 0x00; /* Reserved */
	tBuf[3] = 0x00; /* Page Length MSB */
	tBuf[4] = 0x04; /* Page Length LSB */
	tBuf[5] = 0x03; /* SMART Revision */
	tBuf[6] = 0x00; /* Reserved */
	tBuf[7] = 0x00; /* Off-line Immediate Time MSB */
	tBuf[8] = 0x00; /* Off-line Immediate Time LSB */

	return senddiagnostic (device, SCSI_DIAG_NO_SELF_TEST, (UINT8 *) &tBuf);
}


UINT8 scsiSmartShortSelfTest (int device)
{	
	return senddiagnostic (device, SCSI_DIAG_BG_SHORT_SELF_TEST, NULL);
}


UINT8 scsiSmartExtendSelfTest (int device)
{	
	return senddiagnostic (device, SCSI_DIAG_BG_EXTENDED_SELF_TEST, NULL);
}


UINT8 scsiSmartShortCapSelfTest (int device)
{	
	return senddiagnostic (device, SCSI_DIAG_FG_SHORT_SELF_TEST, NULL);
}


UINT8 scsiSmartExtendCapSelfTest (int device)
{
	return senddiagnostic (device, SCSI_DIAG_FG_EXTENDED_SELF_TEST, NULL);
}


UINT8 scsiSmartSelfTestAbort (int device)
{
	return senddiagnostic (device, SCSI_DIAG_ABORT_SELF_TEST, NULL);
}
