/*
 * ataprint.c
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

#include <ctype.h>
#include <stdio.h>
#include <syslog.h>
#include "atacmds.h"
#include "ataprint.h"
#include "smartctl.h"
#include "extern.h"

const char *CVSid2="$Id: ataprint.cpp,v 1.42 2002/11/07 11:00:55 ballen4705 Exp $"
CVSID1 CVSID2 CVSID3 CVSID6;

// for passing global control variables
extern atamainctrl *con;

// Function for printing ASCII byte-swapped strings, skipping white
// space. This is needed on little-endian architectures, eg Intel,
// Alpha. If someone wants to run this on SPARC they'll need to test
// for the Endian-ness and skip the byte swapping if it's big-endian.
void printswap(char *in, unsigned int n){
  unsigned int i;
  char out[64];

  // swap bytes
  for (i=0;i<n;i+=2){
    unsigned int j=i+1;
    out[i]=in[j];
    out[j]=in[i];
  }

  // find the end of the white space
  for (i=0;i<n && isspace(out[i]);i++);

  // and do the printing starting from first non-white space
  if (n-i)
    pout("%.*s\n",(int)(n-i),out+i);
  else
    pout("[No Information Found]\n");

  return;
}


void ataPrintDriveInfo (struct hd_driveid *drive){
  int version;
  const char *description;
  char unknown[64];
  unsigned short minorrev;

  // print out model, serial # and firmware versions  (byte-swap ASCI strings)
  pout("Device Model:     ");
  printswap(drive->model,40);

  pout("Serial Number:    ");
  printswap(drive->serial_no,20);

  pout("Firmware Version: ");
  printswap(drive->fw_rev,8);

  // now get ATA version info
  version=ataVersionInfo(&description,drive, &minorrev);

  // unrecognized minor revision code
  if (!description){
    sprintf(unknown,"Unrecognized. Minor revision code: 0x%02hx",minorrev);
    description=unknown;
  }
  
  
  // SMART Support was first added into the ATA/ATAPI-3 Standard with
  // Revision 3 of the document, July 25, 1995.  Look at the "Document
  // Status" revision commands at the beginning of
  // http://www.t13.org/project/d2008r6.pdf to see this.  So it's not
  // enough to check if we are ATA-3.  Version=-3 indicates ATA-3
  // BEFORE Revision 3.
  pout("ATA Version is:   %d\n",(int)abs(version));
  pout("ATA Standard is:  %s\n",description);
  
  if (version>=3)
    return;
  
  pout("SMART is only available in ATA Version 3 Revision 3 or greater.\n");
  pout("We will try to proceed in spite of this.\n");
  return;
}


/*  prints verbose value Off-line data collection status byte */
void PrintSmartOfflineStatus(struct ata_smart_values *data){
  pout("Off-line data collection status: ");	
  
  switch(data->offline_data_collection_status){
  case 0x00:
  case 0x80:
    pout("(0x%02x)\tOffline data collection activity was\n\t\t\t\t\t",
	 (int)data->offline_data_collection_status);
    pout("never started.\n");
    break;
  case 0x01:
  case 0x81:
    pout("(0x%02x)\tReserved.\n",
	 (int)data->offline_data_collection_status);
    break;
  case 0x02:
  case 0x82:
    pout("(0x%02x)\tOffline data collection activity \n\t\t\t\t\t",
	 (int)data->offline_data_collection_status);
    pout("completed without error.\n");
    break;
  case 0x03:
  case 0x83:
    pout("(0x%02x)\tReserved.\n",
	 (int)data->offline_data_collection_status);
    break;
  case 0x04:
  case 0x84:
    pout("(0x%02x)\tOffline data collection activity was \n\t\t\t\t\t",
	 (int)data->offline_data_collection_status);
    pout("suspended by an interrupting command from host.\n");
    break;
  case 0x05:
  case 0x85:
    pout("(0x%02x)\tOffline data collection activity was \n\t\t\t\t\t",
	 (int)data->offline_data_collection_status);
    pout("aborted by an interrupting command from host.\n");
    break;
  case 0x06:
  case 0x86:
    pout("(0x%02x)\tOffline data collection activity was \n\t\t\t\t\t",
	 (int)data->offline_data_collection_status);
    pout("aborted by the device with a fatal error.\n");
    break;
  default:
    if ( ((data->offline_data_collection_status >= 0x07) &&
	  (data->offline_data_collection_status <= 0x3f)) ||
	 ((data->offline_data_collection_status >= 0xc0) &&
	  (data->offline_data_collection_status <= 0xff)) )
      pout("(0x%02x)\tVendor Specific.\n",(int)data->offline_data_collection_status);
    else
      pout("(0x%02x)\tReserved.\n",(int)data->offline_data_collection_status);
  }
}



void PrintSmartSelfExecStatus(struct ata_smart_values *data)
{
   pout("Self-test execution status:      ");
   
   switch (data->self_test_exec_status >> 4)
   {
      case 0:
        pout("(%4d)\tThe previous self-test routine completed\n\t\t\t\t\t",
                (int)data->self_test_exec_status);
        pout("without error or no self-test has ever \n\t\t\t\t\tbeen run.\n");
        break;
       case 1:
         pout("(%4d)\tThe self-test routine was aborted by\n\t\t\t\t\t",
                 (int)data->self_test_exec_status);
         pout("the host.\n");
         break;
       case 2:
         pout("(%4d)\tThe self-test routine was interrupted\n\t\t\t\t\t",
                 (int)data->self_test_exec_status);
         pout("by the host with a hard or soft reset.\n");
         break;
       case 3:
          pout("(%4d)\tA fatal error or unknown test error\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("occurred while the device was executing\n\t\t\t\t\t");
          pout("its self-test routine and the device \n\t\t\t\t\t");
          pout("was unable to complete the self-test \n\t\t\t\t\t");
          pout("routine.\n");
          break;
       case 4:
          pout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("a test element that failed and the test\n\t\t\t\t\t");
          pout("element that failed is not known.\n");
          break;
       case 5:
          pout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("the electrical element of the test\n\t\t\t\t\t");
          pout("failed.\n");
          break;
       case 6:
          pout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("the servo (and/or seek) element of the \n\t\t\t\t\t");
          pout("test failed.\n");
          break;
       case 7:
          pout("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("the read element of the test failed.\n");
          break;
       case 15:
          pout("(%4d)\tSelf-test routine in progress...\n\t\t\t\t\t",
                  (int)data->self_test_exec_status);
          pout("%1d0%% of test remaining.\n", 
                  (int)(data->self_test_exec_status & 0x0f));
          break;
       default:
          pout("(%4d)\tReserved.\n",
                  (int)data->self_test_exec_status);
          break;
   }
	
}



void PrintSmartTotalTimeCompleteOffline ( struct ata_smart_values *data){
  pout("Total time to complete off-line \n");
  pout("data collection: \t\t (%4d) seconds.\n", 
       (int)data->total_time_to_complete_off_line);
}



void PrintSmartOfflineCollectCap(struct ata_smart_values *data)
{
   pout("Offline data collection\n");
   pout("capabilities: \t\t\t (0x%02x) ",
            (int)data->offline_data_collection_capability);

   if (data->offline_data_collection_capability == 0x00)
   {
      pout("\tOff-line data collection not supported.\n");
   } 
   else 
   {
      pout( "%s\n", isSupportExecuteOfflineImmediate(data)?
              "SMART execute Offline immediate." :
              "No SMART execute Offline immediate.");

      pout( "\t\t\t\t\t%s\n", isSupportAutomaticTimer(data)? 
              "Automatic timer ON/OFF support.":
              "No Automatic timer ON/OFF support.");
		
      pout( "\t\t\t\t\t%s\n", isSupportOfflineAbort(data)? 
              "Abort Offline collection upon new\n\t\t\t\t\tcommand.":
              "Suspend Offline collection upon new\n\t\t\t\t\tcommand.");

      pout( "\t\t\t\t\t%s\n", isSupportOfflineSurfaceScan(data)? 
              "Offline surface scan supported.":
              "No Offline surface scan supported.");

      pout( "\t\t\t\t\t%s\n", isSupportSelfTest(data)? 
              "Self-test supported.":
              "No Self-test supported.");
    }
}



void PrintSmartCapability ( struct ata_smart_values *data)
{
   pout("SMART capabilities:            ");
   pout("(0x%04x)\t", (int)data->smart_capability);
   
   if (data->smart_capability == 0x00)
   {
       pout("Automatic saving of SMART data\t\t\t\t\tis not implemented.\n");
   } 
   else 
   {
	
      pout( "%s\n", (data->smart_capability & 0x01)? 
              "Saves SMART data before entering\n\t\t\t\t\tpower-saving mode.":
              "Does not save SMART data before\n\t\t\t\t\tentering power-saving mode.");
		
      if ( data->smart_capability & 0x02 )
      {
          pout("\t\t\t\t\tSupports SMART auto save timer.\n");
      }
   }
}



void PrintSmartErrorLogCapability ( struct ata_smart_values *data)
{

   pout("Error logging capability:       ");
    
   if ( isSmartErrorLogCapable(data) )
   {
      pout(" (0x%02x)\tError logging supported.\n",
               (int)data->errorlog_capability);
   }
   else {
       pout(" (0x%02x)\tError logging NOT supported.\n",
                (int)data->errorlog_capability);
   }
}



void PrintSmartShortSelfTestPollingTime (struct ata_smart_values *data)
{
   if ( isSupportSelfTest(data) )
   {
      pout("Short self-test routine \n");
      pout("recommended polling time: \t (%4d) minutes.\n", 
               (int)data->short_test_completion_time);

   }
   else
   {
      pout("Short self-test routine \n");
      pout("recommended polling time: \t        Not Supported.\n");
   }
}


void PrintSmartExtendedSelfTestPollingTime ( struct ata_smart_values *data)
{
   if ( isSupportSelfTest(data) )
   {
      pout("Extended self-test routine \n");
      pout("recommended polling time: \t (%4d) minutes.\n", 
               (int)data->extend_test_completion_time);
   }
   else
   {
      pout("Extended self-test routine \n");
      pout("recommended polling time: \t        Not Supported.\n");
   }
}


// onlyfailed=0 : print all attribute values
// onlyfailed=1:  just ones that are currently failed and have prefailure bit set
// onlyfailed=2:  ones that are failed, or have failed with or without prefailure bit set
void PrintSmartAttribWithThres (struct ata_smart_values *data, 
				struct ata_smart_thresholds *thresholds,
				int onlyfailed){
  int i,j;
  long long rawvalue;
  int needheader=1;
    
  // step through all vendor attributes
  for (i=0; i<NUMBER_ATA_SMART_ATTRIBUTES; i++){
    char *status;
    struct ata_smart_attribute *disk=data->vendor_attributes+i;
    struct ata_smart_threshold_entry *thre=thresholds->thres_entries+i;
    
    // consider only valid attributes
    if (disk->id && thre->id){
      char *type;
      int failednow,failedever;
      char attributename[64];

      failednow = (disk->current <= thre->threshold);
      failedever= (disk->worst   <= thre->threshold);
      
      // These break out of the loop if we are only printing certain entries...
      if (onlyfailed==1 && (!disk->status.flag.prefailure || !failednow))
	continue;
      
      if (onlyfailed==2 && !failedever)
	continue;
      
      // print header only if needed
      if (needheader){
	if (!onlyfailed){
	  pout("SMART Attributes Data Structure revision number: %d\n",(int)data->revnumber);
	  pout("Vendor Specific SMART Attributes with Thresholds:\n");
	}
	pout("ID# ATTRIBUTE_NAME          FLAG     VALUE WORST THRESH TYPE     WHEN_FAILED RAW_VALUE\n");
	needheader=0;
      }
      
      // is this currently failed, or has it ever failed?
      if (failednow)
	status="FAILING_NOW";
      else if (failedever)
	status="In_the_past";
      else
	status="    -";
      
      // Print name of attribute
      ataPrintSmartAttribName(attributename,disk->id);
      pout("%-28s",attributename);

      // printing line for each valid attribute
      type=disk->status.flag.prefailure?"Pre-fail":"Old_age";
      pout("0x%04x   %.3d   %.3d   %.3d    %-9s%-12s", 
	     (int)disk->status.all, (int)disk->current, (int)disk->worst,
	     (int)thre->threshold, type, status);
      
      // convert the six individual bytes to a long long (8 byte) integer
      rawvalue = 0;
      for (j = 0 ; j < 6 ; j++)
	rawvalue |= disk->raw[j] << (8*j) ;
      
      // This switch statement is where we handle Raw attributes
      // that are stored in an unusual vendor-specific format,
      switch (disk->id){
	// Power on time
      case 9:
	if (con->smart009minutes){
	  // minutes
	  long long tmp1=rawvalue/60;
	  long long tmp2=rawvalue%60;
	  pout("%llu h + %2llu m\n", tmp1, tmp2);
	}
	else
	  // hours
	  pout("%llu\n", rawvalue);  //stored in hours
	break;
	
	// Temperature
      case 194:
	pout("%d", (int)disk->raw[0]);
	if (rawvalue==disk->raw[0])
	  pout("\n");
	else
	  // The other bytes are in use. Try IBM's model
	  pout(" (Lifetime Min/Max %d/%d)\n",(int)disk->raw[2],
		 (int)disk->raw[4]);
	break;
      default:
	pout("%llu\n", rawvalue);
      }
      
      // print a warning if there is inconsistency here!
      if (disk->id != thre->id){
	char atdat[64],atthr[64];
	ataPrintSmartAttribName(atdat,disk->id);
	ataPrintSmartAttribName(atthr,thre->id);
	pout("%-28s<== Data Page      |  WARNING: PREVIOUS ATTRIBUTE HAS TWO\n",atdat);
	pout("%-28s<== Threshold Page |  INCONSISTENT IDENTITIES IN THE DATA\n",atthr);
      }
    }
  }
  if (!needheader) pout("\n");
}


void ataPrintGeneralSmartValues(struct ata_smart_values *data){
  pout("General SMART Values:\n");
  
  PrintSmartOfflineStatus(data); 
  
  if (isSupportSelfTest(data)){
    PrintSmartSelfExecStatus (data);
  }
  
  PrintSmartTotalTimeCompleteOffline(data);
  PrintSmartOfflineCollectCap(data);
  PrintSmartCapability(data);
  
  PrintSmartErrorLogCapability(data);
  if (isSupportSelfTest(data)){
    PrintSmartShortSelfTestPollingTime (data);
    PrintSmartExtendedSelfTestPollingTime (data);
  }
  pout("\n");
}

// Returns nonzero if region of memory contains non-zero entries
int nonempty(unsigned char *testarea,int n){
  int i;
  for (i=0;i<n;i++)
    if (testarea[i])
      return 1;
  return 0;
}

// returns number of errors
void ataPrintSmartErrorlog (struct ata_smart_errorlog *data){
  int i,j,k;
  
  pout("SMART Error Log Version: %d\n", (int)data->revnumber);
  
  // if no errors logged, return
  if (!data->error_log_pointer){
    pout("No Errors Logged\n\n");
    return;
  }
  QUIETON(con);
  // if log pointer out of range, return
  if ( data->error_log_pointer>5 ){
    pout("Invalid Error Log index = %02x (T13/1321D rev 1c"
	 "Section 8.41.6.8.2.2 gives valid range from 1 to 5)\n\n",
	 (int)data->error_log_pointer);
    return;
  }
  
  // starting printing error log info
  if (data->ata_error_count<=5)
    pout( "ATA Error Count: %d\n", (int)data->ata_error_count);
  else
    pout( "ATA Error Count: %d (device log contains only the most recent five errors)\n",
	   (int)data->ata_error_count);
  QUIETOFF(con);
  pout("\tDCR = Device Control Register\n");
  pout("\tFR  = Features Register\n");
  pout("\tSC  = Sector Count Register\n");
  pout("\tSN  = Sector Number Register\n");
  pout("\tCL  = Cylinder Low Register\n");
  pout("\tCH  = Cylinder High Register\n");
  pout("\tD/H = Device/Head Register\n");
  pout("\tCR  = Content written to Command Register\n");
  pout("\tER  = Error register\n");
  pout("\tSTA = Status register\n");
  pout("Timestamp is seconds since the previous disk power-on.\n");
  pout("Note: timestamp \"wraps\" after 2^32 msec = 49.710 days.\n\n");
  
  // now step through the five error log data structures (table 39 of spec)
  for (k = 4; k >= 0; k-- ) {
    
    // The error log data structure entries are a circular buffer
    i=(data->error_log_pointer+k)%5;
    
    // Spec says: unused error log structures shall be zero filled
    if (nonempty((unsigned char*)&(data->errorlog_struct[i]),sizeof(data->errorlog_struct[i]))){
      char *msgstate;
      switch (data->errorlog_struct[i].error_struct.state){
      case 0x00: msgstate="in an unknown state";break;
      case 0x01: msgstate="sleeping"; break;
      case 0x02: msgstate="in standby mode"; break;
      case 0x03: msgstate="active or idle"; break;
      case 0x04: msgstate="doing SMART off-line or self test"; break;
      default:   msgstate="in a vendor specific or reserved state";
      }
      // See table 42 of ATA5 spec
      QUIETON(con);
      pout("Error %d occurred at disk power-on lifetime: %d hours\n",
	     (int)(5-k), (int)data->errorlog_struct[i].error_struct.timestamp);
      QUIETOFF(con);
      pout("When the command that caused the error occurred, the device was %s.\n",msgstate);
      pout("After command completion occurred, registers were:\n");
      pout("ER:%02x SC:%02x SN:%02x CL:%02x CH:%02x D/H:%02x ST:%02x\n",
	   (int)data->errorlog_struct[i].error_struct.error_register,
	   (int)data->errorlog_struct[i].error_struct.sector_count,
	   (int)data->errorlog_struct[i].error_struct.sector_number,
	   (int)data->errorlog_struct[i].error_struct.cylinder_low,
	   (int)data->errorlog_struct[i].error_struct.cylinder_high,
	   (int)data->errorlog_struct[i].error_struct.drive_head,
	   (int)data->errorlog_struct[i].error_struct.status);
      pout("Sequence of commands leading to the command that caused the error were:\n");
      pout("DCR   FR   SC   SN   CL   CH   D/H   CR   Timestamp\n");
      for ( j = 4; j >= 0; j--){
	struct ata_smart_errorlog_command_struct *thiscommand=&(data->errorlog_struct[i].commands[j]);
	
	// Spec says: unused data command structures shall be zero filled
	if (nonempty((unsigned char*)thiscommand,sizeof(*thiscommand)))
	  pout(" %02x   %02x   %02x   %02x   %02x   %02x    %02x   %02x     %d.%03d\n", 
	       (int)thiscommand->devicecontrolreg,
	       (int)thiscommand->featuresreg,
	       (int)thiscommand->sector_count,
	       (int)thiscommand->sector_number,
	       (int)thiscommand->cylinder_low,
	       (int)thiscommand->cylinder_high,
	       (int)thiscommand->drive_head,
	       (int)thiscommand->commandreg,
	       (unsigned int)(thiscommand->timestamp / 1000U),
	       (unsigned int)(thiscommand->timestamp % 1000U)); 
      }
      pout("\n");
    }
  }
  QUIETON(con);
  if (con->quietmode)
    pout("\n");
  QUIETOFF(con);
  return;  
}

// return value is number of entries found where the self-test showed an error
int ataPrintSmartSelfTestlog(struct ata_smart_selftestlog *data,int allentries){
  int i,j,noheaderprinted=1;
  int retval=0;

  if (allentries)
    pout("SMART Self-test log, version number %d\n",(int)data->revnumber);
  if ((data->revnumber!=0x0001) && allentries)
    pout("Warning - structure revision number does not match spec!\n");
  if (data->mostrecenttest==0){
    if (allentries)
      pout("No self-tests have been logged\n\n");
    return 0;
  }

  // print log      
  for (i=20;i>=0;i--){    
    struct ata_smart_selftestlog_struct *log;
    // log is a circular buffer
    j=(i+data->mostrecenttest)%21;
    log=data->selftest_struct+j;

    if (nonempty((unsigned char*)log,sizeof(*log))){
      char *msgtest,*msgstat,percent[64],firstlba[64];
      int errorfound=0;

      // test name
      switch(log->selftestnumber){
      case   0: msgtest="Off-line           "; break;
      case   1: msgtest="Short off-line     "; break;
      case   2: msgtest="Extended off-line  "; break;
      case 127: msgtest="Abort off-line test"; break;
      case 129: msgtest="Short captive      "; break;
      case 130: msgtest="Extended captive   "; break;
      default:  msgtest="Unknown test       ";
      }
      
      // test status
      switch((log->selfteststatus)>>4){
      case  0:msgstat="Completed                    "; break;
      case  1:msgstat="Aborted by host              "; break;
      case  2:msgstat="Interrupted (host reset)     "; break;
      case  3:msgstat="Fatal or unknown error       "; errorfound=1; break;
      case  4:msgstat="Completed: unknown failure   "; errorfound=1; break;
      case  5:msgstat="Completed: electrical failure"; errorfound=1; break;
      case  6:msgstat="Completed: servo/seek failure"; errorfound=1; break;
      case  7:msgstat="Completed: read failure      "; errorfound=1; break;
      case 15:msgstat="Test in progress             "; break;
      default:msgstat="Unknown test status          ";
      }

      retval+=errorfound;
      sprintf(percent,"%1d0%%",(log->selfteststatus)&0xf);

      // T13/1321D revision 1c: (Data structure Rev #1)

      //The failing LBA shall be the LBA of the uncorrectable sector
      //that caused the test to fail. If the device encountered more
      //than one uncorrectable sector during the test, this field
      //shall indicate the LBA of the first uncorrectable sector
      //encountered. If the test passed or the test failed for some
      //reason other than an uncorrectable sector, the value of this
      //field is undefined.

      // This is true in ALL ATA-5 specs
      
      if (!errorfound || log->lbafirstfailure==0xffffffff || log->lbafirstfailure==0x00000000)
	sprintf(firstlba,"%s","");
      else	
	sprintf(firstlba,"0x%08x",log->lbafirstfailure);

      if (noheaderprinted && (allentries || errorfound)){
	pout("Num  Test_Description    Status                  Remaining  LifeTime(hours)  LBA_of_first_error\n");
	noheaderprinted=0;
      }
      
      if (allentries || errorfound)
	pout("#%2d  %s %s %s  %8d         %s\n",21-i,msgtest,msgstat,
	     percent,(int)log->timestamp,firstlba);
    }
  }
  if (!allentries && retval)
    pout("\n");
  return retval;
}

void ataPseudoCheckSmart ( struct ata_smart_values *data, 
                           struct ata_smart_thresholds *thresholds) {
  int i;
  int failed = 0;
  for (i = 0 ; i < NUMBER_ATA_SMART_ATTRIBUTES ; i++) {
    if (data->vendor_attributes[i].id &&   
	thresholds->thres_entries[i].id &&
	data->vendor_attributes[i].status.flag.prefailure &&
	(data->vendor_attributes[i].current <= thresholds->thres_entries[i].threshold) &&
	(thresholds->thres_entries[i].threshold != 0xFE)){
      pout("Attribute ID %d Failed\n",(int)data->vendor_attributes[i].id);
      failed = 1;
    } 
  }   
  pout("%s\n", ( failed )?
	 "SMART overall-health self-assessment test result: FAILED!\n"
	 "Drive failure expected in less than 24 hours. SAVE ALL DATA":
	 "SMART overall-health self-assessment test result: PASSED");
}


// Compares failure type to policy in effect, and either exits or
// simply returns to the calling routine.
void failuretest(type, returnvalue){

  // If this is an error in an "optional" SMART command
  if (type==OPTIONAL_CMD){
    if (con->conservative){
      pout("An optional SMART command has failed: exiting.  To continue, turn off the -%c option\n",
	   ULTRACONSERVATIVE);
      exit(returnvalue);
    }
    return;
  }

  // If this is an error in a "mandatory" SMART command
  if (type==MANDATORY_CMD){
    if (con->permissive)
      return;
    pout("A mandatory SMART command has failed: exiting. To continue, turn on the -%c option\n",
	 PERMISSIVE);
    exit(returnvalue);
  }

  fprintf(stderr,"Smartctl internal error in failuretest(type=%d). Please contact %s\n",type,PROJECTHOME);
  exit(returnvalue|FAILCMD);
}

// Used to warn users about invalid checksums.  However we will not
// abort on invalid checksums.
void checksumwarning(const char *string){
  pout("Warning! %s error: invalid SMART checksum.\n",string);
  fprintf(stderr,"Warning! %s error: invalid SMART checksum.\n",string);
  syslog(LOG_INFO,"Warning! %s error: invalid SMART checksum.\n",string);

  if (con->checksumfail)
    exit(FAILSMART);

  return;
}

// Initialize to zero just in case some SMART routines don't work
struct hd_driveid drive;
struct ata_smart_values smartval;
struct ata_smart_thresholds smartthres;
struct ata_smart_errorlog smarterror;
struct ata_smart_selftestlog smartselftest;

int ataPrintMain (int fd){
  int timewait,code;
  int returnval=0;

  // Start by getting Drive ID information.  We need this, to know if SMART is supported.
  if (ataReadHDIdentity(fd,&drive)){
    pout("Smartctl: Hard Drive Read Identity Failed\n\n");
    failuretest(MANDATORY_CMD, returnval|=FAILID);
  }
  
  // Print most drive identity information if requested
  if (con->driveinfo){
    pout("=== START OF INFORMATION SECTION ===\n");
    ataPrintDriveInfo(&drive);
  }
  
  // now check if drive supports SMART; otherwise time to exit
  if (!ataSmartSupport(&drive)){
    pout("SMART support is: Unavailable - device lacks SMART capability.\n");
    failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    pout("                  Checking to be sure by trying SMART ENABLE command.\n");
    if (ataEnableSmart(fd)){
      pout("                  No SMART functionality found. Sorry.\n");
      failuretest(MANDATORY_CMD,returnval|=FAILSMART);
    }
    else
      pout("                  SMART appears to work.  Continuing.\n"); 
    if (!con->driveinfo) pout("\n");
  }
  
  // Now print remaining drive info: is SMART enabled?    
  if (con->driveinfo){
    pout("SMART support is: Available - device has SMART capability.\n");
    if (ataDoesSmartWork(fd))
      pout("SMART support is: Enabled\n");
    else
      pout("SMART support is: Disabled\n");
    pout("\n");
  }

  
  // START OF THE ENABLE/DISABLE SECTION OF THE CODE
  if (con->smartenable || con->smartdisable || 
      con->smartautosaveenable || con->smartautosavedisable || 
      con->smartautoofflineenable || con->smartautoofflinedisable)
    pout("=== START OF ENABLE/DISABLE COMMANDS SECTION ===\n");
  
  // Enable/Disable SMART commands
  if (con->smartenable){
    if (ataEnableSmart(fd)) {
      pout("Smartctl: SMART Enable Failed.\n\n");
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Enabled.\n");
  }
  
  // From here on, every command requires that SMART be enabled...
  if (!ataDoesSmartWork(fd)) {
    pout("SMART Disabled. Use option -%c to enable it.\n", (int)SMARTENABLE );
    return returnval;
  }
  
  // Turn off SMART on device
  if (con->smartdisable){    
    if (ataDisableSmart(fd)) {
      pout( "Smartctl: SMART Disable Failed.\n\n");
      failuretest(MANDATORY_CMD,returnval|=FAILSMART);
    }
    pout("SMART Disabled. Use option -%c to enable it.\n",(int)SMARTENABLE);
    return returnval;		
  }
  
  // Let's ALWAYS issue this command to get the SMART status
  code=ataSmartStatus2(fd);
  if (code==-1)
    failuretest(MANDATORY_CMD, returnval|=FAILSMART);

  // Enable/Disable Auto-save attributes
  if (con->smartautosaveenable){
    if (ataEnableAutoSave(fd)){
      pout( "Smartctl: SMART Enable Attribute Autosave Failed.\n\n");
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Attribute Autosave Enabled.\n");
  }
  if (con->smartautosavedisable){
    if (ataDisableAutoSave(fd)){
      pout( "Smartctl: SMART Disable Attribute Autosave Failed.\n\n");
      failuretest(MANDATORY_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Attribute Autosave Disabled.\n");
  }
  
  // for everything else read values and thresholds are needed
  if (ataReadSmartValues(fd, &smartval)){
    pout("Smartctl: SMART Read Values failed.\n\n");
    failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  }
  if (ataReadSmartThresholds(fd, &smartthres)){
    pout("Smartctl: SMART Read Thresholds failed.\n\n");
    failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  }

  // Enable/Disable Off-line testing
  if (con->smartautoofflineenable){
    if (!isSupportAutomaticTimer(&smartval)){
      pout("Warning: device does not support SMART Automatic Timers.\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    if (ataEnableAutoOffline(fd)){
      pout( "Smartctl: SMART Enable Automatic Offline Failed.\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Automatic Offline Testing Enabled every four hours.\n");
  }
  if (con->smartautoofflinedisable){
    if (!isSupportAutomaticTimer(&smartval)){
      pout("Warning: device does not support SMART Automatic Timers.\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    if (ataDisableAutoOffline(fd)){
      pout("Smartctl: SMART Disable Automatic Offline Failed.\n\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    else
      pout("SMART Automatic Offline Testing Disabled.\n");
  }

  // all this for a newline!
  if (con->smartenable || con->smartdisable || 
      con->smartautosaveenable || con->smartautosavedisable || 
      con->smartautoofflineenable || con->smartautoofflinedisable)
    pout("\n");

  // START OF READ-ONLY OPTIONS APART FROM -V and -i
  if (con->checksmart || con->generalsmartvalues || con->smartvendorattrib || con->smarterrorlog || con->smartselftestlog)
    pout("=== START OF READ SMART DATA SECTION ===\n");
  
  // Check SMART status (use previously returned value)
  if (con->checksmart){
    if (code) {
      QUIETON(con);
      pout("SMART overall-health self-assessment test result: FAILED!\n"
	     "Drive failure expected in less than 24 hours. SAVE ALL DATA.\n");
      QUIETOFF(con);
      if (ataCheckSmart(&smartval, &smartthres,1)){
	returnval|=FAILATTR;
	if (con->smartvendorattrib)
	  pout("See vendor-specific Attribute list for failed Attributes.\n\n");
	else {
	  QUIETON(con);
	  pout("Failed Attributes:\n");
	  PrintSmartAttribWithThres(&smartval, &smartthres,1);
	}
      }
      else
	pout("No failed Attributes found.\n\n");   
      returnval|=FAILSTATUS;
      QUIETOFF(con);
    }
    else {
      pout("SMART overall-health self-assessment test result: PASSED\n");
      if (ataCheckSmart(&smartval, &smartthres,0)){
	if (con->smartvendorattrib)
	  pout("See vendor-specific Attribute list for marginal Attributes.\n\n");
	else {
	  QUIETON(con);
	  pout("Please note the following marginal Attributes:\n");
	  PrintSmartAttribWithThres(&smartval, &smartthres,2);
	} 
	returnval|=FAILAGE;
      }
      else
	pout("\n");
    }
    QUIETOFF(con);
  }
  
  // Print general SMART values
  if (con->generalsmartvalues)
    ataPrintGeneralSmartValues(&smartval); 
  
  // Print vendor-specific attributes
  if (con->smartvendorattrib){
    QUIETON(con);
    PrintSmartAttribWithThres(&smartval, &smartthres,con->quietmode?2:0);
    QUIETOFF(con);
  }
  
  // Print SMART error log
  if (con->smarterrorlog){
    if (!isSmartErrorLogCapable(&smartval)){
      pout("Warning: device does not support Error Logging\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    if (ataReadErrorLog(fd, &smarterror)){
      pout("Smartctl: SMART Errorlog Read Failed\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);

    }
    else {
      // quiet mode is turned on inside ataPrintSmartErrorLog()
      ataPrintSmartErrorlog(&smarterror);
      QUIETOFF(con);
    }
  }
  
  // Print SMART self-test log
  if (con->smartselftestlog){
    if (!isSmartErrorLogCapable(&smartval)){
      pout("Warning: device does not support Self Test Logging\n");
      failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }    
    else {
      if(ataReadSelfTestLog(fd, &smartselftest)){
	pout("Smartctl: SMART Self Test Log Read Failed\n");
	failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
      }
      else {
	QUIETON(con);
	if (ataPrintSmartSelfTestlog(&smartselftest,!con->quietmode))
	  returnval|=FAILLOG;
	QUIETOFF(con);
	pout("\n");
      }
    } 
  }
  
  // START OF THE TESTING SECTION OF THE CODE.  IF NO TESTING, RETURN
  if (con->testcase==-1)
    return returnval;
  
  pout("=== START OF OFFLINE IMMEDIATE AND SELF-TEST SECTION ===\n");
  // if doing a self-test, be sure it's supported by the hardware
  if (con->testcase==OFFLINE_FULL_SCAN &&  !isSupportExecuteOfflineImmediate(&smartval)){
    pout("Warning: device does not support Execute Off-Line Immediate function.\n\n");
    failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  }
  else if (!isSupportSelfTest(&smartval)){
    pout("Warning: device does not support Self-Test functions.\n\n");
    failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  }
  // Now do the test
  if (ataSmartTest(fd, con->testcase))
    failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
  
  // Tell user how long test will take to complete  
  if ((timewait=TestTime(&smartval,con->testcase))){ 
    pout("Please wait %d %s for test to complete.\n",
	    (int)timewait, con->testcase==OFFLINE_FULL_SCAN?"seconds":"minutes");
    
    if (con->testcase!=SHORT_CAPTIVE_SELF_TEST && con->testcase!=EXTEND_CAPTIVE_SELF_TEST)
      pout("Use smartctl -%c to abort test.\n", (int)SMARTSELFTESTABORT);	
  }    
  return returnval;
}
