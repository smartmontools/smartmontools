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
#include "ataprint.h"
#include "smartctl.h"
#include "extern.h"

const char *CVSid4="$Id: ataprint.cpp,v 1.19 2002/10/22 11:47:59 ballen4705 Exp $\n"
	           "\t" CVSID2 "\t" CVSID3 "\t" CVSID6 ;

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
    printf("%.*s\n",n-i,out+i);
  else
    printf("[No Information Found]\n");

  return;
}


void ataPrintDriveInfo (struct hd_driveid drive){
  int version;
  const char *description;
  char unknown[64];

  // print out model, serial # and firmware versions  (byte-swap ASCI strings)
  printf("Device Model:     ");
  printswap(drive.model,40);

  printf("Serial Number:    ");
  printswap(drive.serial_no,20);

  printf("Firmware Version: ");
  printswap(drive.fw_rev,8);

  // now get ATA version info
  version=ataVersionInfo(&description,drive);

  // unrecognized minor revision code
  if (!description){
    sprintf(unknown,"Unrecognized. Minor revision code: 0x%02x",drive.minor_rev_num);
    description=unknown;
  }
  
  
  // SMART Support was first added into the ATA/ATAPI-3 Standard with
  // Revision 3 of the document, July 25, 1995.  Look at the "Document
  // Status" revision commands at the beginning of
  // http://www.t13.org/project/d2008r6.pdf to see this.  So it's not
  // enough to check if we are ATA-3.  Version=-3 indicates ATA-3
  // BEFORE Revision 3.
  printf("ATA Version is:   %i\n",version>0?version:-1*version);
  printf("ATA Standard is:  %s\n",description);
  
  if (version>=3)
    return;
  
  printf("SMART is only available in ATA Version 3 Revision 3 or greater.\n\n");
  exit (0);
}


/* void PrintSmartOfflineStatus ( struct ata_smart_values data) 
   prints verbose value Off-line data collection status byte */

void PrintSmartOfflineStatus ( struct ata_smart_values data)
{
   printf ("Off-line data collection status: ");	
   
   switch (data.offline_data_collection_status){
   case 0x00:
   case 0x80:
     printf ("(0x%02x)\tOffline data collection activity was\n\t\t\t\t\t",
	     data.offline_data_collection_status);
     printf("never started.\n");
     break;
   case 0x01:
   case 0x81:
     printf ("(0x%02x)\tReserved.\n",
	     data.offline_data_collection_status);
     break;
   case 0x02:
   case 0x82:
     printf ("(0x%02x)\tOffline data collection activity \n\t\t\t\t\t",
	     data.offline_data_collection_status);
     printf ("completed without error.\n");
     break;
   case 0x03:
   case 0x83:
     printf ("(0x%02x)\tReserved.\n",
	     data.offline_data_collection_status);
     break;
   case 0x04:
   case 0x84:
     printf ("(0x%02x)\tOffline data collection activity was \n\t\t\t\t\t",
	     data.offline_data_collection_status);
     printf ("suspended by an interrupting command from host.\n");
     break;
   case 0x05:
   case 0x85:
     printf ("(0x%02x)\tOffline data collection activity was \n\t\t\t\t\t",
	     data.offline_data_collection_status);
     printf ("aborted by an interrupting command from host.\n");
     break;
   case 0x06:
   case 0x86:
     printf ("(0x%02x)\tOffline data collection activity was \n\t\t\t\t\t",
	     data.offline_data_collection_status);
     printf ("aborted by the device with a fatal error.\n");
     break;
   default:
     if ( ((data.offline_data_collection_status >= 0x07) &&
	   (data.offline_data_collection_status <= 0x3f)) ||
	  ((data.offline_data_collection_status >= 0xc0) &&
	   (data.offline_data_collection_status <= 0xff)) )
       {
	 printf ("(0x%02x)\tVendor Specific.\n",
		 data.offline_data_collection_status);
       } 
     else 
       {
	 printf ("(0x%02x)\tReserved.\n",
		 data.offline_data_collection_status);
       }
   }
}



void PrintSmartSelfExecStatus ( struct ata_smart_values data)
{
   printf ("Self-test execution status:      ");
   
   switch (data.self_test_exec_status >> 4)
   {
      case 0:
        printf ("(%4d)\tThe previous self-test routine completed\n\t\t\t\t\t",
                data.self_test_exec_status);
        printf ("without error or no self-test has ever \n\t\t\t\t\tbeen run.\n");
        break;
       case 1:
         printf ("(%4d)\tThe self-test routine was aborted by\n\t\t\t\t\t",
                 data.self_test_exec_status);
         printf ("the host.\n");
         break;
       case 2:
         printf ("(%4d)\tThe self-test routine was interrupted\n\t\t\t\t\t",
                 data.self_test_exec_status);
         printf ("by the host with a hard or soft reset.\n");
         break;
       case 3:
          printf ("(%4d)\tA fatal error or unknown test error\n\t\t\t\t\t",
                  data.self_test_exec_status);
          printf ("occurred while the device was executing\n\t\t\t\t\t");
          printf ("its self-test routine and the device \n\t\t\t\t\t");
          printf ("was unable to complete the self-test \n\t\t\t\t\t");
          printf ("routine.\n");
          break;
       case 4:
          printf ("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  data.self_test_exec_status);
          printf ("a test element that failed and the test\n\t\t\t\t\t");
          printf ("element that failed is not known.\n");
          break;
       case 5:
          printf ("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  data.self_test_exec_status);
          printf ("the electrical element of the test\n\t\t\t\t\t");
          printf ("failed.\n");
          break;
       case 6:
          printf ("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  data.self_test_exec_status);
          printf ("the servo (and/or seek) element of the \n\t\t\t\t\t");
          printf ("test failed.\n");
          break;
       case 7:
          printf ("(%4d)\tThe previous self-test completed having\n\t\t\t\t\t",
                  data.self_test_exec_status);
          printf ("the read element of the test failed.\n");
          break;
       case 15:
          printf ("(%4d)\tSelf-test routine in progress...\n\t\t\t\t\t",
                  data.self_test_exec_status);
          printf ("%1d0%% of test remaining.\n", 
                  data.self_test_exec_status & 0x0f);
          break;
       default:
          printf ("(%4d)\tReserved.\n",
                  data.self_test_exec_status);
          break;
   }
	
}



void PrintSmartTotalTimeCompleteOffline ( struct ata_smart_values data)
{
   printf ("Total time to complete off-line \n");
   printf ("data collection: \t\t (%4d) seconds.\n", 
           data.total_time_to_complete_off_line);
}



void PrintSmartOfflineCollectCap ( struct ata_smart_values data)
{
   printf ("Offline data collection\n");
   printf ("capabilities: \t\t\t (0x%02x) ",
            data.offline_data_collection_capability);

   if (data.offline_data_collection_capability == 0x00)
   {
      printf ("\tOff-line data collection not supported.\n");
   } 
   else 
   {
      printf( "%s\n", isSupportExecuteOfflineImmediate(data)?
              "SMART execute Offline immediate." :
              "No SMART execute Offline immediate.");

      printf( "\t\t\t\t\t%s\n", isSupportAutomaticTimer(data)? 
              "Automatic timer ON/OFF support.":
              "No Automatic timer ON/OFF support.");
		
      printf( "\t\t\t\t\t%s\n", isSupportOfflineAbort(data)? 
              "Abort Offline collection upon new\n\t\t\t\t\tcommand.":
              "Suspend Offline collection upon new\n\t\t\t\t\tcommand.");

      printf( "\t\t\t\t\t%s\n", isSupportOfflineSurfaceScan(data)? 
              "Offline surface scan supported.":
              "No Offline surface scan supported.");

      printf( "\t\t\t\t\t%s\n", isSupportSelfTest(data)? 
              "Self-test supported.":
              "No Self-test supported.");
    }
}



void PrintSmartCapability ( struct ata_smart_values data)
{
   printf ("SMART capabilities:            ");
   printf ("(0x%04x)\t", data.smart_capability);
   
   if (data.smart_capability == 0x00)
   {
       printf ("Automatic saving of SMART data\t\t\t\t\tis not implemented.\n");
   } 
   else 
   {
	
      printf( "%s\n", (data.smart_capability & 0x01)? 
              "Saves SMART data before entering\n\t\t\t\t\tpower-saving mode.":
              "Does not save SMART data before\n\t\t\t\t\tentering power-saving mode.");
		
      if ( data.smart_capability & 0x02 )
      {
          printf ("\t\t\t\t\tSupports SMART auto save timer.\n");
      }
   }
}



void PrintSmartErrorLogCapability ( struct ata_smart_values data)
{

   printf ("Error logging capability:       ");
    
   if ( isSmartErrorLogCapable(data) )
   {
      printf (" (0x%02x)\tError logging supported.\n",
               data.errorlog_capability);
   }
   else {
       printf (" (0x%02x)\tError logging NOT supported.\n",
                data.errorlog_capability);
   }
}



void PrintSmartShortSelfTestPollingTime ( struct ata_smart_values data)
{
   if ( isSupportSelfTest(data) )
   {
      printf ("Short self-test routine \n");
      printf ("recommended polling time: \t (%4d) minutes.\n", 
               data.short_test_completion_time);

   }
   else
   {
      printf ("Short self-test routine \n");
      printf ("recommended polling time: \t        Not Supported.\n");
   }
}


void PrintSmartExtendedSelfTestPollingTime ( struct ata_smart_values data)
{
   if ( isSupportSelfTest(data) )
   {
      printf ("Extended self-test routine \n");
      printf ("recommended polling time: \t (%4d) minutes.\n", 
               data.extend_test_completion_time);
   }
   else
   {
      printf ("Extended self-test routine \n");
      printf ("recommended polling time: \t        Not Supported.\n");
   }
}


// onlyfailing is a bitflag
// onlyfailing=0 : all values
// onlyfailing=1:  just ones that are currently failed and have prefailure bit set
// onlyfailing=2:  ones that are failed, or have failed with or without prefailure bit set
void PrintSmartAttribWithThres (struct ata_smart_values data, 
				struct ata_smart_thresholds thresholds,
				int onlyfailed){
  int i,j;
  long long rawvalue;
  int needheader=1;
    
  // step through all vendor attributes
  for (i=0; i<NUMBER_ATA_SMART_ATTRIBUTES; i++){
    char *status;
    struct ata_smart_attribute *disk=data.vendor_attributes+i;
    struct ata_smart_threshold_entry *thre=thresholds.thres_entries+i;
    
    // consider only valid attributes
    if (disk->id && thre->id){
      char *type;
      int failednow,failedever;

      failednow =disk->current <= thre->threshold;
      failedever=disk->worst   <= thre->threshold;
      
      // These break out of the loop if we are only printing certain entries...
      if (onlyfailed==1 && (!disk->status.flag.prefailure || !failednow))
	break;
      
      if (onlyfailed==2 && !failedever)
	break;
      
      // print header only if needed
      if (needheader){
	printf ("SMART Data Structure revision number: %i\n",data.revnumber);
	printf ("Vendor Specific SMART Attributes with Thresholds:\n"
		"Attribute                    Flag     Value Worst Threshold Type     Ever_Failed? Raw_Value\n");
	needheader=0;
      }
      
      // is this currently failed, or has it ever failed?
      if (failednow)
	status="FAILED NOW!";
      else if (failedever)
	status="In the past";
      else
	status="    -";
      
      // Print name of attribute
      ataPrintSmartAttribName(disk->id);
      
      // printing line for each valid attribute
      type=disk->status.flag.prefailure?"Pre-fail":"Old age";
      printf(" 0x%04x   %.3i   %.3i   %.3i       %-9s%-13s", 
	     disk->status.all, disk->current, disk->worst,
	     thre->threshold, type, status);
      
      // convert the six individual bytes to a long long (8 byte) integer
      rawvalue = 0;
      for (j = 0 ; j < 6 ; j++)
	rawvalue |= disk->raw[j] << (8*j) ;
      
      // This switch statement is where we handle Raw attributes
      // that are stored in an unusual vendor-specific format,
      switch (disk->id){
	// Power on time
      case 9:
	if (smart009minutes)
	  // minutes
	  printf ("%llu h + %2llu m\n",rawvalue/60,rawvalue%60);
	else
	   // hours
	  printf ("%llu\n", rawvalue);  //stored in hours
	break;
	
	// Temperature
      case 194:
	printf ("%u", disk->raw[0]);
	if (rawvalue==disk->raw[0])
	  printf("\n");
	else
	  // The other bytes are in use. Try IBM's model
	  printf(" (Lifetime Min/Max %u/%u)\n",disk->raw[2],
		 disk->raw[4]);
	break;
      default:
	printf("%llu\n", rawvalue);
      }	    
    }
  }
}


void ataPrintGeneralSmartValues  ( struct ata_smart_values data)
{
   printf ("\nGeneral SMART Values: \n");

   PrintSmartOfflineStatus (data); 
   printf("\n");
	
   if (isSupportSelfTest(data))
   {
       PrintSmartSelfExecStatus (data);
       printf("\n");
   }
	
   PrintSmartTotalTimeCompleteOffline (data);
   printf("\n");
	
   PrintSmartOfflineCollectCap (data);
   printf("\n");
	
   PrintSmartCapability ( data);
   printf("\n");

   PrintSmartErrorLogCapability (data);
   printf ("\n");
	
   if (isSupportSelfTest(data))
   {
      PrintSmartShortSelfTestPollingTime (data);
      printf ("\n");

      PrintSmartExtendedSelfTestPollingTime (data);
      printf ("\n");
   }

}


void ataPrintSmartThresholds (struct ata_smart_thresholds data)
{
   int i;

   printf ("SMART Thresholds\n");
   printf ("SMART Threshold Revision Number: %i\n", data.revnumber);
	
   for ( i = 0 ; i < NUMBER_ATA_SMART_ATTRIBUTES ; i++) {
      if (data.thres_entries[i].id)	
          printf ("Attribute %3i threshold: %02x (%2i)\n", 
                   data.thres_entries[i].id, 
                   data.thres_entries[i].threshold, 
                   data.thres_entries[i].threshold);
   }
}


// Returns nonzero if region of memory contains non-zero entries
int nonempty(unsigned char *testarea,int n){
  int i;
  for (i=0;i<n;i++)
    if (testarea[i])
      return 1;
  return 0;
}
  
void ataPrintSmartErrorlog (struct ata_smart_errorlog data)
{
  int i,j,k;
  
  printf ("\nSMART Error Log\n");
  printf ( "SMART Error Logging Version: %i\n", data.revnumber);
  
  // if no errors logged, return
  if ( ! data.error_log_pointer)
    {
      printf ("No Errors Logged\n");
      return;
    }
  
  // if log pointer out of range, return
  if ( data.error_log_pointer>5 ){
    printf("Invalid Error log index = %02x (T13/1321D rev 1c"
	   "Section 8.41.6.8.2.2 gives valid range from 1 to 5)\n",
	   data.error_log_pointer);
    return;
  }
  
  // starting printing error log info
  if (data.ata_error_count<=5)
      printf ( "ATA Error Count: %u\n", data.ata_error_count);
  else
      printf ( "ATA Error Count: %u (only the most recent five errors are shown below)\n",
	       data.ata_error_count);

  printf(  "Acronyms used below:\n");
  printf(  "DCR = Device Control Register\n");
  printf(  "FR  = Features Register\n");
  printf(  "SC  = Sector Count Register\n");
  printf(  "SN  = Sector Number Register\n");
  printf(  "CL  = Cylinder Low Register\n");
  printf(  "CH  = Cylinder High Register\n");
  printf(  "D/H = Device/Head Register\n");
  printf(  "CR  = Content written to Command Register\n");
  printf(  "ER  = Error register\n");
  printf(  "STA = Status register\n\n");
  printf(  "Timestamp is time (in seconds) since the command that caused an error was accepted,\n");
  printf(  "measured from the time the disk was powered-on, during the session when the error occurred.\n");
  printf(  "Note: timestamp \"wraps\" after 1193.046 hours = 49.710 days = 2^32 seconds.\n");
  
  // now step through the five error log data structures (table 39 of spec)
  for (k = 4; k >= 0; k-- ) {
    
    // The error log data structure entries are a circular buffer
    i=(data.error_log_pointer+k)%5;
    
    // Spec says: unused error log structures shall be zero filled
    if (nonempty((unsigned char*)&(data.errorlog_struct[i]),sizeof(data.errorlog_struct[i]))){
      char *msgstate;
      switch (data.errorlog_struct[i].error_struct.state){
      case 0x00: msgstate="in an unknown state";break;
      case 0x01: msgstate="sleeping"; break;
      case 0x02: msgstate="in standby mode"; break;
      case 0x03: msgstate="active or idle"; break;
      case 0x04: msgstate="doing SMART off-line or self test"; break;
      default:   msgstate="in a vendor specific or reserved state";
      }
      printf("\nError Log Structure %i:\n",5-k);
      // See table 42 of ATA5 spec
      printf("Error occurred at disk power-on lifetime: %u hours\n",
	     data.errorlog_struct[i].error_struct.timestamp);
      printf("When the command that caused the error occurred, the device was %s.\n",msgstate);
      printf("After command completion occurred, registers were:\n");
      printf("ER:%02x SC:%02x SN:%02x CL:%02x CH:%02x D/H:%02x ST:%02x\n",
	     data.errorlog_struct[i].error_struct.error_register,
	     data.errorlog_struct[i].error_struct.sector_count,
	     data.errorlog_struct[i].error_struct.sector_number,
	     data.errorlog_struct[i].error_struct.cylinder_low,
	     data.errorlog_struct[i].error_struct.cylinder_high,
	     data.errorlog_struct[i].error_struct.drive_head,
	     data.errorlog_struct[i].error_struct.status);
      printf("Sequence of commands leading to the command that caused the error were:\n");
      printf("DCR   FR   SC   SN   CL   CH   D/H   CR   Timestamp\n");
      for ( j = 4; j >= 0; j--){
	struct ata_smart_errorlog_command_struct *thiscommand=&(data.errorlog_struct[i].commands[j]);
	
	// Spec says: unused data command structures shall be zero filled
	if (nonempty((unsigned char*)thiscommand,sizeof(*thiscommand)))
	  printf ( " %02x   %02x   %02x   %02x   %02x   %02x    %02x   %02x     %u.%03u\n", 
		   thiscommand->devicecontrolreg,
		   thiscommand->featuresreg,
		   thiscommand->sector_count,
		   thiscommand->sector_number,
		   thiscommand->cylinder_low,
		   thiscommand->cylinder_high,
		   thiscommand->drive_head,
		   thiscommand->commandreg,
		   (unsigned int)(thiscommand->timestamp / 1000),
		   (unsigned int)(thiscommand->timestamp % 1000)); 
      } 
    }
  }  
  return;  
}


void ataPrintSmartSelfTestlog (struct ata_smart_selftestlog data){
  int i,j;

  printf("SMART Self-test log, version number %u\n",data.revnumber);
  if (data.revnumber!=0x01)
    printf("Warning - structure revision number does not match spec!\n");
  
  if (data.mostrecenttest==0){
    printf("No self-tests have been logged\n");
    return;
  }

  // print log      
  printf("Num  Test_Description    Status                  Remaining  LifeTime(hours)  LBA_of_first_error\n");
  for (i=20;i>=0;i--){

    struct ata_smart_selftestlog_struct *log;
    // log is a circular buffer
    j=(i+data.mostrecenttest)%21;
    log=&(data.selftest_struct[j]);

    if (nonempty((unsigned char*)log,sizeof(*log))){
      char *msgtest,*msgstat,percent[64],firstlba[64];

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
      case  3:msgstat="Fatal or unknown error       "; break;
      case  4:msgstat="Completed: unknown failure   "; break;
      case  5:msgstat="Completed: electrical failure"; break;
      case  6:msgstat="Completed: servo/seek failure"; break;
      case  7:msgstat="Completed: read failure      "; break;
      case 15:msgstat="Test in progress             "; break;
      default:msgstat="Unknown test status          ";
      }
      
      sprintf(percent,"%1d0%%",(log->selfteststatus)&0xf);
      if (log->lbafirstfailure==0xffffffff || log->lbafirstfailure==0x00000000)
	sprintf(firstlba,"%s","");
      else	
	sprintf(firstlba,"0x%08x",log->lbafirstfailure);
      printf("#%2d  %s %s %s  %8u         %s\n",
	     21-i,
	     msgtest,
	     msgstat,
	     percent,
	     log->timestamp,
	     firstlba);
    }
    else
      return;
  }
  return;
}

void ataPseudoCheckSmart ( struct ata_smart_values data, 
                           struct ata_smart_thresholds thresholds) {
  int i;
  int failed = 0;
  for (i = 0 ; i < NUMBER_ATA_SMART_ATTRIBUTES ; i++) {
    if (data.vendor_attributes[i].id &&   
	thresholds.thres_entries[i].id &&
	data.vendor_attributes[i].status.flag.prefailure &&
	(data.vendor_attributes[i].current <= thresholds.thres_entries[i].threshold) &&
	(thresholds.thres_entries[i].threshold != 0xFE)){
      printf("Attribute ID %i Failed\n",data.vendor_attributes[i].id);
      failed = 1;
    } 
  }   
  printf("%s\n", ( failed )?
	 "SMART overall-health self-assessment test result: FAILED!\n"
	 "Drive failure expected in less than 24 hours. SAVE ALL DATA":
	 "SMART overall-health self-assessment test result: PASSED");
}

void ataPrintSmartAttribName ( unsigned char id ){
  switch (id){
    
  case 1:
    printf("(  1)Raw Read Error Rate    ");
    break;
  case 2:
    printf("(  2)Throughput Performance ");
    break;
  case 3:
    printf("(  3)Spin Up Time           ");
    break;
  case 4:
    printf("(  4)Start Stop Count       ");
    break;
  case 5:
    printf("(  5)Reallocated Sector Ct  ");
    break;
  case 6:
    printf("(  6)Read Channel Margin    ");
    break;
  case 7:
    printf("(  7)Seek Error Rate        ");
    break;
  case 8:
    printf("(  8)Seek Time Performance  ");
    break;
  case 9:
    printf("(  9)Power On Hours         ");
    break;
  case 10:
    printf("( 10)Spin Retry Count       ");
    break;
  case 11:
    printf("( 11)Calibration Retry Count");
    break;
  case 12:
    printf("( 12)Power Cycle Count      ");
    break;
  case 13:
    printf("( 13)Read Soft Error Rate   ");
    break;
  case 191:
    printf("(191)G-Sense Error Rate     ");
    break;
  case 192:
    printf("(192)Power-Off Retract Count");
    break;
  case 193:
    printf("(193)Load Cycle Count       ");
    break;
  case 194:
    printf("(194)Temperature            ");
    break;
  case 195:
    printf("(195)Hardware ECC Recovered ");
    break;
  case 196:
    printf("(196)Reallocated Event Count");
    break;
  case 197:
    printf("(197)Current Pending Sector ");
    break;
  case 198:
    printf("(198)Offline Uncorrectable  ");
    break;
  case 199:
    printf("(199)UDMA CRC Error Count   ");
    break;
  case 220:
    printf("(220)Disk Shift             ");
    break;
  case 221:
    printf("(221)G-Sense Error Rate     ");
    break;
  case 222:
    printf("(222)Loaded Hours           ");
    break;
  case 223:
    printf("(223)Load Retry Count       ");
    break;
  case 224:
    printf("(224)Load Friction          ");
    break;
  case 225:
    printf("(225)Load Cycle Count       ");
    break;
  case 226:
    printf("(226)Load-in Time           ");
    break;
  case 227:
    printf("(227)Torq-amp Count         ");
    break;
  case 228:
    printf("(228)Power-off Retract Count");
    break;
  default:
    printf("(%3d)Unknown Attribute      ", id);
    break;
  }
}	

/****
 Called by smartctl to access ataprint  
**/

void ataPrintMain (int fd){
  struct hd_driveid drive;
  struct ata_smart_values smartval;
  struct ata_smart_thresholds smartthres;
  struct ata_smart_errorlog smarterror;
  struct ata_smart_selftestlog smartselftest;
  int timewait;
  
  // Start by getting Drive ID information.  We need this, to know if SMART is supported.
  if (ataReadHDIdentity(fd,&drive)){
    printf("Smartctl: Hard Drive Read Identity Failed\n\n");
    exit(-1);	
  }
  
  // Print most drive identity information if requested
  if (driveinfo){
    printf("\n=== START OF INFORMATION SECTION ===\n");
    ataPrintDriveInfo(drive);
  }
  
  // now check if drive supports SMART; otherwise time to exit
  if (!ataSmartSupport(drive)){
    printf("SMART support is: Unavailable - device lacks SMART capability.\n");
    exit (0);
  }
  
  // Now print remaining drive info: is SMART enabled?    
  if (driveinfo){
    printf("SMART support is: Available - device has SMART capability.\n");
    if (ataDoesSmartWork(fd))
      printf("SMART support is: Enabled\n");
    else
      printf("SMART support is: Disabled\n");
  }
  
  // START OF THE ENABLE/DISABLE SECTION OF THE CODE
  if (smartenable || smartdisable || 
      smartautosaveenable || smartautosavedisable || 
      smartautoofflineenable || smartautoofflinedisable)
    printf("\n=== START OF ENABLE/DISABLE COMMANDS SECTION ===\n");

  // Enable/Disable SMART commands
  if (smartenable){
    if (ataEnableSmart(fd)) {
      printf("Smartctl: SMART Enable Failed.\n\n");
      exit(-1);
    }
    else
      printf("SMART Enabled.\n");
  }
  
  // From here on, every command requires that SMART be enabled...
  if (!ataDoesSmartWork(fd)) {
    printf("SMART Disabled. Use option -%c to enable it.\n", SMARTENABLE );
    exit(0);
  }
  
  // Turn off SMART on device
  if (smartdisable){    
    if (ataDisableSmart(fd)) {
      printf( "Smartctl: SMART Disable Failed.\n\n");
      exit(-1);
    }
    printf("SMART Disabled. Use option -%c to enable it.\n",SMARTENABLE);
    exit (0);		
  }
  
  // Enable/Disable Auto-save attributes
  if (smartautosaveenable){
    if (ataEnableAutoSave(fd)){
      printf( "Smartctl: SMART Enable Attribute Autosave Failed.\n\n");
      exit(-1);
    } 
    printf("SMART Attribute Autosave Enabled\n");
  }
  if (smartautosavedisable){
    if (ataDisableAutoSave(fd)){
      printf( "Smartctl: SMART Disable Attribute Autosave Failed.\n\n");
      exit(-1);
    }     
    printf("SMART Attribute Autosave Disabled\n");
  }
  
  // for everything else read values and thresholds are needed
  if (ataReadSmartValues(fd, &smartval)){
    printf("Smartctl: SMART Values Read Failed.\n\n");
    exit (-1);
  }
  if (ataReadSmartThresholds(fd, &smartthres)){
    printf("Smartctl: SMART Thresholds Read Failed.\n\n");
    exit (-1); 
  }

  // Enable/Disable Off-line testing
  if (smartautoofflineenable){
    if (!isSupportAutomaticTimer (smartval)){
      printf("Device does not support SMART Automatic Timers.\n\n");
      exit(-1);
    }
    if (ataEnableAutoOffline (fd)){
      printf( "Smartctl: SMART Enable Automatic Offline Failed.\n\n");
      exit(-1);
    }    
    printf ("SMART Automatic Offline Testing Enabled every four hours.\n");
  }
  if (smartautoofflinedisable){
    if (!isSupportAutomaticTimer (smartval)){
      printf("Device does not support SMART Automatic Timers.\n\n");
      exit(-1);
    }		
    if ( ataDisableAutoOffline (fd)){
      printf( "Smartctl: SMART Disable Automatic Offline Failed.\n\n");
      exit(-1);
    }
    printf ("SMART Automatic Offline Testing Disabled.\n");
  }

  // START OF READ-ONLY OPTIONS APART FROM -p and -i
  if (checksmart || generalsmartvalues || smartvendorattrib || smarterrorlog || smartselftestlog)
    printf("\n=== START OF READ SMART DATA SECTION ===\n");

  // Check SMART status
  if (checksmart)
    // Eventually when working use the ataSmartStatus2 function here.
    // This will then not require read values & thresholds above, and
    // shouldl be moved in the code to just before the read values &
    // thresholds statements.
    ataPseudoCheckSmart(smartval, smartthres);
  
  // Print general SMART values
  if (generalsmartvalues)
    ataPrintGeneralSmartValues(smartval); 
  
  // Print vendor-specific attributes
  if (smartvendorattrib)
    PrintSmartAttribWithThres(smartval, smartthres,0);
	
  // Print SMART error log
  if (smarterrorlog){
    if (!isSmartErrorLogCapable(smartval))
      printf("Device does not support Error Logging\n");
    else {
      if (ataReadErrorLog(fd, &smarterror))
	printf("Smartctl: SMART Errorlog Read Failed\n");
      else
	ataPrintSmartErrorlog(smarterror);
    }
  }

  // Print SMART self-test log
  if (smartselftestlog){
    if (!isSmartErrorLogCapable(smartval))
      printf("Device does not support Self Test Logging\n");
    else {
      if(ataReadSelfTestLog(fd, &smartselftest))
	printf("Smartctl: SMART Self Test Log Read Failed\n");
      else
	ataPrintSmartSelfTestlog(smartselftest); 
    } 
  }
  
  // START OF THE TESTING SECTION OF THE CODE.  IF NO TESTING, RETURN
  if (testcase==-1)
    return;

  printf("\n=== START OF OFFLINE IMMEDIATE AND SELF-TEST SECTION ===\n");

  
  // if doing a self-test, be sure it's supported by the hardware
  if (testcase==OFFLINE_FULL_SCAN &&  !isSupportExecuteOfflineImmediate(smartval)){
    printf("ERROR: device does not support Execute Off-Line Immediate function.\n\n");
    exit(-1);
  }
  else if (!isSupportSelfTest(smartval)){
    printf ("ERROR: device does not support Self-Test functions.\n\n");
    exit(-1);
  }
  
  // Now do the test
  if (ataSmartTest(fd, testcase))
    exit(-1);
  
  // Tell user how long test will take to complete  
  if ((timewait=TestTime(smartval,testcase))){ 
    printf ("Please wait %d %s for test to complete.\n",
	    timewait, testcase==OFFLINE_FULL_SCAN?"seconds":"minutes");
    
    if (testcase!=SHORT_CAPTIVE_SELF_TEST && testcase!=EXTEND_CAPTIVE_SELF_TEST)
      printf ("Use smartctl -%c to abort test.\n", SMARTSELFTESTABORT);	
  }    
  return;
}
