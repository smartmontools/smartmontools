//  $Id: ataprint.cpp,v 1.9 2002/10/14 15:26:10 ballen4705 Exp $
/*
 * ataprint.c
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

#include "ataprint.h"
#include "smartctl.h"
#include "extern.h"

void ataPrintDriveInfo (struct hd_driveid drive){
  int version;
  const char *description;
  char unknown[32];

  version=ataVersionInfo(&description,drive);

  // unrecognized minor revision code
  if (!description){
    sprintf(unknown,"Unrecognized. Minor revision code: 0x%02x",drive.minor_rev_num);
    description=unknown;
  }
  
  // print out information for user
  printf("Device Model:     %.40s\n",drive.model);
  printf("Serial Number:    %.20s\n",drive.serial_no);
  printf("Firmware Version: %.8s\n",drive.fw_rev);
  printf("ATA Version is:   %i\n",version);
  printf("ATA Standard is:  %s\n",description);

  if (version>=3)
    return;

  printf("SMART is only supported in ATA version 3 or greater\n");
  exit (0);
}


/* void PrintSmartOfflineStatus ( struct ata_smart_values data) 
   prints verbose value Off-line data collection status byte */

void PrintSmartOfflineStatus ( struct ata_smart_values data)
{
   printf ("Off-line data collection status: ");	
   
   switch (data.offline_data_collection_status)
   {
      case 0x0: case 0x80:
          printf ("(0x%02x)\tOffline data collection activity was\n\t\t\t\t\t",
                  data.offline_data_collection_status);
          printf("never started.\n");
          break;
      case 0x01: case 0x81:
          printf ("(0x%02x)\tReserved.\n",
                  data.offline_data_collection_status);
          break;
      case 0x02: case 0x82:
          printf ("(0x%02x)\tOffline data collection activity \n\t\t\t\t\t",
                  data.offline_data_collection_status);
          printf ("completed without error.\n");
          break;
      case 0x03: case 0x83:
          printf ("(0x%02x)\tReserved.\n",
                  data.offline_data_collection_status);
          break;
      case 0x04: case 0x84:
          printf ("(0x%02x)\tOffline data collection activity was \n\t\t\t\t\t",
                  data.offline_data_collection_status);
          printf ("suspended by an interrupting command.\n");
          break;
      case 0x05: case 0x85:
          printf ("(0x%02x)\tOffline data collection activity was \n\t\t\t\t\t",
                  data.offline_data_collection_status);
          printf ("aborted by an interrupting command.\n");
          break;
      case 0x06: case 0x86:
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
       printf ("automatic saving of SMART data"); 
       printf ("\t\t\t\t\tis not implemented.\n");
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



void PrintSmartAttribWithThres ( struct ata_smart_values data, 
                                 struct ata_smart_thresholds thresholds)
{
   int i,j;
   long long rawvalue; 
   printf ("Vendor Specific SMART Attributes with Thresholds:\n");
   printf ("Revision Number: %i\n", data.revnumber);
   printf ("Attribute                    Flag     Value Worst Threshold Raw Value\n");
	
   for ( i = 0 ; i < NUMBER_ATA_SMART_ATTRIBUTES ; i++ ){
     // step through all vendor attributes
     if (data.vendor_attributes[i].id && thresholds.thres_entries[i].id){
       ataPrintSmartAttribName(data.vendor_attributes[i].id);
       // printing if they contain something sensible
       printf(" 0x%04x   %.3i   %.3i   %.3i       ", 
	      data.vendor_attributes[i].status.all,
	      data.vendor_attributes[i].current,
	      data.vendor_attributes[i].worst,
	      thresholds.thres_entries[i].threshold);

       // convert the six individual bytes to a long long (8 byte) integer
       rawvalue = 0;
       for (j = 0 ; j < 6 ; j++)
	 rawvalue |= data.vendor_attributes[i].raw[j] << (8*j) ;

       // This switch statement is where we handle Raw attributes
       // that are stored in an unusual vendor-specific format,
       switch (data.vendor_attributes[i].id){

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
	 printf ("%u", data.vendor_attributes[i].raw[0]);
	 if (rawvalue==data.vendor_attributes[i].raw[0])
	   printf("\n");
	 else
	   // The other bytes are in use. Try IBM's model
	   printf(" (Lifetime Min/Max %u/%u)\n",data.vendor_attributes[i].raw[2],
		  data.vendor_attributes[i].raw[4]);
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
      printf ( "ATA Error Count: %u\n\n", data.ata_error_count);
  else
      printf ( "ATA Error Count: %u (only the most recent five errors are shown below)\n\n",
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

  printf("\nSMART Self-test log, version number %u\n",data.revnumber);
  if (data.revnumber!=0x01)
    printf("Warning - structure revision number does not match spec!\n");
  
  if (data.mostrecenttest==0){
    printf("No self-tests have been logged\n");
    return;
  }

  // print log      
  printf("\nNum  Test_Description    Status                  Remaining  LifeTime(hours)  LBA_of_first_error\n");
  for (i=20;i>=0;i--){

    struct ata_smart_selftestlog_struct *log;
    // log is a circular buffer
    j=(i+data.mostrecenttest)%21;
    log=&(data.selftest_struct[j]);

    if (nonempty((unsigned char*)log,sizeof(*log))){
      char *msgtest,*msgstat,percent[16],firstlba[16];

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

void ataPsuedoCheckSmart ( struct ata_smart_values data, 
                           struct ata_smart_thresholds thresholds) {
   int i;
   int failed = 0;
   for (i = 0 ; i < NUMBER_ATA_SMART_ATTRIBUTES ; i++ ) {
     if (data.vendor_attributes[i].id &&   
	  thresholds.thres_entries[i].id &&
	  data.vendor_attributes[i].status.flag.prefailure &&
	  (data.vendor_attributes[i].current < thresholds.thres_entries[i].threshold) &&
	  (thresholds.thres_entries[i].threshold != 0xFE)){
       printf("Attribute ID %i Failed\n", 
	      data.vendor_attributes[i].id);
       failed = 1;
     } 
   }   
   printf("%s\n", ( failed )?
	  "SMART overall-health self-assessment test result: FAILED!\n"
	  "Drive failure expected in less than 24 hours. SAVE ALL DATA\n":
	  "SMART overall-health self-assessment test result: PASSED\n");
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

void ataPrintMain ( int fd )
{
   struct hd_driveid drive;
   struct ata_smart_values smartval;
   struct ata_smart_thresholds smartthres;
   struct ata_smart_errorlog smarterror;
   struct ata_smart_selftestlog smartselftest;

   if ( driveinfo )
   {
      if (  ataReadHDIdentity ( fd, &drive) != 0 )
      {
         printf("Smartctl: Hard Drive Identity Failed\n");
         exit(0);	
      } 
		
      ataPrintDriveInfo(drive); 
		

      if (ataSmartSupport(drive))
      {
          printf ("SMART support is: ");
	  
          if ( ataSmartStatus(fd) != 0) 
          {
              printf( "Disabled\n");
              printf( "Use option -%c to enable\n\n", SMARTENABLE );
              exit(0);
          }
          else { 
              printf( "Enabled\n\n");
          } 
      }
      else {
           printf("SMART support is: Unavailable. Device lacks SMART capability.\n\n");
	   exit (0);
      }
  }

  if ( smartdisable )
  {
		
     if ( ataDisableSmart(fd) != 0) 
     {
	printf( "Smartctl: SMART Enable Failed\n");
	exit(-1);
     }
    
     printf("SMART Disabled\n");
     exit (0);
		
   }

   if ( smartenable )
   {
      if ( ataEnableSmart(fd) != 0) 
	{
          printf( "Smartctl: SMART Enable Failed\n");
          exit(-1);
	}

      if (ataSmartStatus(fd)==0)
	printf("SMART Enabled\n");
      else
	printf( "Smartctl: SMART Enable Failed for unknown reasons\n");
   }
   
   
   if ( smartautosavedisable ){
     if (ataDisableAutoSave(fd) != 0)
       {
	 printf( "Smartctl: SMART Disable Attribute Autosave Failed\n");
	 exit(-1);
       }     
     printf("SMART Attribute Autosave Disabled\n");
   }
	
   if ( smartautosaveenable ){
     if (ataEnableAutoSave(fd) != 0)
       {
	 printf( "Smartctl: SMART Enable Attribute Autosave Failed\n");
	 exit(-1);
       } 
     printf("SMART Attribute Autosave Enabled\n");
   }

   /* for everything else read values and thresholds 
   are needed */	

   if ( ataReadSmartValues ( fd, &smartval) != 0 )
   {
      printf("Smartctl: SMART Values Read Failed\n");
      exit (-1);
   }

   if ( ataReadSmartThresholds ( fd, &smartthres) != 0 )
   {
      printf("Smartctl: SMART Thresholds Read Failed\n");
      exit (-1); 
   }
	
   if ( checksmart )
   {
	/* pseudo is used because linux does not support access to
	   Task File registers */
	// I am very confused by this comment.  Does anyone get it?  Bruce
      ataPsuedoCheckSmart ( smartval , smartthres);

   }
	
   if (  generalsmartvalues )
   {
      ataPrintGeneralSmartValues( smartval ); 
   }

   if ( smartvendorattrib )
   {
      PrintSmartAttribWithThres( smartval, smartthres);
   }
	
   if ( smarterrorlog )
   {
      if ( isSmartErrorLogCapable(smartval) == 0)
      {
          printf("Device does not support Error Logging\n");
      } 
      else
      {
         if ( ataReadErrorLog ( fd, &smarterror) != 0 )
	 {
             printf("Smartctl: SMART Errorlog Read Failed\n");
         }
         else
         {
             ataPrintSmartErrorlog ( smarterror); 
         }
      }
    }
	

   if ( smartselftestlog )
   {
       if ( isSmartErrorLogCapable(smartval) == 0)
       {
           printf("Device does not support Self Test Logging\n");
       }
       else
       {
	    if (  ataReadSelfTestLog( fd, &smartselftest) != 0 )
            {
                printf("Smartctl: SMART Self Test Log Read Failed\n");
            }
            else
            {
                 ataPrintSmartSelfTestlog (smartselftest); 
            }
        } 
    }

    if (  smartautoofflineenable  )
    {
        if ( !isSupportAutomaticTimer (smartval))
        {
            printf("Device does not support SMART Automatic Timers\n");
            exit(-1);
        }

        if ( ataEnableAutoOffline (fd) != 0) 
        {
            printf( "Smartctl: SMART Enable Automatic Offline Failed\n");
            exit(-1);
        }
        
        printf ("SMART Automatic Offline Testing Enabled every four hours\n");
    }

    if (  smartautoofflinedisable  )
    {
        if ( !isSupportAutomaticTimer (smartval))
        {
            printf("Device does not support SMART Automatic Timers\n");
            exit(-1);
        }
			
        if ( ataDisableAutoOffline (fd) != 0) 
        {
            printf( "Smartctl: SMART Disable Automatic Offline Failed\n");
            exit(-1);
        }
         
        printf ("SMART Automatic Offline Testing Disabled\n");

     }


     if ( smartexeoffimmediate )
     {
        if ( ataSmartOfflineTest (fd) != 0) 
	{
            printf( "Smartctl: SMART Offline Failed\n");
            exit(-1);
        }

        printf ("Drive Command Successful offline test has begun\n");
        printf ("Please wait %d seconds for test to complete\n", 
                 isOfflineTestTime(smartval) );
        printf ("Use smartctl -%c to abort test\n", SMARTSELFTESTABORT);	
	exit (0);
     }

     if ( smartshortcapselftest )
     {
         if ( ! isSupportSelfTest(smartval) )
         {
             printf (" ERROR: device does not support Self-Test function\n");
             exit(-1);
         }
		
         if ( ataSmartShortCapSelfTest (fd) != 0) 
         {
              printf( "Smartctl: SMART Short Self Test Failed\n");
              exit(-1);
         }
     
         printf ("Drive Command Successful offline test has begun\n");
         printf ("Please wait %d minutes for test to complete\n", 
                  isShortSelfTestTime (smartval) );
	 printf ("Use smartctl -%c to abort test\n", SMARTSELFTESTABORT);	
	
         /* Make sure Offline testing is last thing done */
        exit (0);
     }

     if ( smartshortselftest )
     {
        if ( ! isSupportSelfTest(smartval) )
        {
            printf (" ERROR: device does not support Self-Test function\n");
            exit(-1);
        }
		
        if ( ataSmartShortSelfTest (fd) != 0) 
        {
            printf( "Smartctl: SMART Short Self Test Failed\n");
            exit(-1);
        }

        printf ("Drive Command Successful offline test has begun\n");
        printf ("Please wait %d minutes for test to complete\n", 
                 isShortSelfTestTime (smartval) );
        printf ("Use smartctl -%c to abort test\n", SMARTSELFTESTABORT);	
		
	/* Make sure Offline testing is last thing done */
	exit (0);
     }
 
	
     if ( smartextendselftest )
     {
        if ( ! isSupportSelfTest(smartval) )
        {
           printf (" ERROR: device does not support Self-Test function\n");
           exit(-1);
        }

        if ( ataSmartExtendSelfTest (fd) != 0) 
        {
           printf( "SMART Extended Self Test Failed\n");
           exit(-1);
        }
		
        printf ("Drive Command Successful self test has begun\n");
        printf ("Please wait %d minutes for test to complete\n", 
                 isExtendedSelfTestTime(smartval) );
        printf ("Use smartctl -%c to abort test\n", SMARTSELFTESTABORT);	
	
        exit (0);
     }

	
     if ( smartextendcapselftest )
     {
         if ( ! isSupportSelfTest(smartval) )
         {
            printf (" ERROR: device does not support self test function\n");
            exit(-1);
         }

         if ( ataSmartExtendCapSelfTest (fd) != 0) 
         {
            printf( "SMART Extended Self Test Failed\n");
            exit(-1);
         }
		
            printf ("Drive Command Successful captive extended self test has begun\n");
            printf ("Please wait %d minutes for test to complete\n", 
                    isExtendedSelfTestTime(smartval) );
            printf ("Use smartctl -%c to abort test\n", SMARTSELFTESTABORT);	
            exit (0);
     }

     if ( smartselftestabort )
     {
        if ( ! isSupportSelfTest(smartval) )
        {
            printf (" ERROR: device does not support Self-Test function\n");
            exit(-1);
        }

        if ( ataSmartSelfTestAbort (fd) != 0) 
        {
            printf( "SMART Self Test Abort Failed\n");
            exit(-1);
        }
		
        printf ("Drive Command Successful self test aborted\n");
    }		
	
}
