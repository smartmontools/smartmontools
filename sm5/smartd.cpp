/*
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
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
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <linux/hdreg.h>
#include <syslog.h>
#include <stdarg.h>

#include "atacmds.h"
#include "scsicmds.h"
#include "smartd.h"

extern const char *CVSid1, *CVSid2;
const char *CVSid3="$Id: smartd.cpp,v 1.13 2002/10/22 14:57:43 ballen4705 Exp $\n" 
"\t" CVSID1 "\t" CVSID4 "\t" CVSID7 ;

int daemon_init(void){
  pid_t pid;
  
  if ( (pid = fork()) < 0)
    // unable to fork!
    return -1;
  else if (pid != 0)
    // we are the parent process -- exit
    exit (0);

  // from here on, we are the child process
  setsid ();
  chdir("/");
  umask(0);
  return(0);
}


// This function prints either to stdout or to the syslog as needed
void printout(int priority,char *fmt, ...){
  va_list ap;
  // initialize variable argument list 
 va_start(ap,fmt);
  if (debugmode)
    vprintf(fmt,ap);
  else
    vsyslog(priority,fmt,ap);
  va_end(ap);
  return;
}

/* prints help information for command syntax */
void Usage ( void){
  printout(LOG_INFO,"smartd version %d.%d-%d - S.M.A.R.T. Daemon\n", 
           RELEASE_MAJOR, RELEASE_MINOR,SMARTMONTOOLS_VERSION);
  printout(LOG_INFO,"usage: smartd -[opts] \n");
  printout(LOG_INFO,"Read Only Commands:\n");
  printout(LOG_INFO,"\t\t%c\t\tStart smartd in debug Mode\n",DEBUGMODE);
  printout(LOG_INFO,"\t\t%c\t\tPrint License and Copyright information\n",PRINTCOPYLEFT);
}
	
// scan to see what ata devices there are, and if they support SMART
void atadevicescan ( atadevices_t *devices){
  int i;
  int fd;
  struct hd_driveid drive;
  char device[] = "/dev/hda";
  
  for(i=0;i<MAXATADEVICES;i++,device[7]++ ){
    
    printout(LOG_INFO,"Reading Device %s\n", device);
    
    fd = open ( device , O_RDWR );
    if ( fd < 0)
      // no such device
      continue;
    
    if (ataReadHDIdentity (fd,&drive) || !ataSmartSupport(drive) || ataEnableSmart(fd)){
      // device exists, but not able to do SMART
      close(fd);
      printout(LOG_INFO,"Device: %s, Found but not SMART capable, or couldn't enable SMART\n",device);
      continue;
    }
     
    // device exists, and does SMART.  Add to list
    devices[numatadevices].fd = fd;
    strcpy(devices[numatadevices].devicename, device);
    devices[numatadevices].drive = drive;
    if (ataReadSmartValues (fd,&devices[numatadevices].smartval)){
      printout(LOG_INFO,"Device: %s, Read Smart Values Failed\n",device);
    }
    
    if (ataReadSmartThresholds (fd,&devices[numatadevices].smartthres)){
      printout(LOG_INFO,"Device: %s, Read Smart Thresholds Failed\n",device);
    }
    
    printout(LOG_INFO,"%s Found and is SMART capable\n",device);
    
    // This makes NO sense.  We may want to know if the drive supports
    // Offline Surface Scan, for example.  But checking if it supports
    // self-tests seems useless. In any case, smartd NEVER uses this
    // field anywhere...
    devices[numatadevices].selftest = 
      isSupportSelfTest(devices[numatadevices].smartval);

    numatadevices++;
  }
}

// This function is hard to read and ought to be rewritten
// A couple of obvious questions -- why isn't fd always closed if not used?
// Why in the world is the four-byte integer cast to a pointer to an eight-byte object??
void scsidevicescan ( scsidevices_t *devices){
  int i, fd, smartsupport;
  unsigned char  tBuf[4096];
  char device[] = "/dev/sda";	
  
  for(i = 0; i < MAXSCSIDEVICES ; i++,device[7]++ ){
    
    printout(LOG_INFO,"Reading Device %s\n", device);
    
    fd = open ( device , O_RDWR );
    
    if ( fd >= 0 && !testunitready (fd)) {
      if (modesense ( fd, 0x1c, (UINT8 *) &tBuf) != 0){
	printout(LOG_INFO,"Device: %s, Failed read of ModePage 1C \n", device);
	close(fd);
      }
      else
	if ( scsiSmartSupport( fd, (UINT8 *) &smartsupport) == 0){			
	  if (!(smartsupport & DEXCPT_ENABLE)){
	    devices[numscsidevices].fd = fd;
	    strcpy(devices[numscsidevices].devicename,device);
	    
	    printout(LOG_INFO, "Device: %s, Found and is SMART capable\n",device);
	    
	    if (logsense ( fd , SUPPORT_LOG_PAGES, (UINT8 *) &tBuf) == 0){
	      for ( i = 4; i < tBuf[3] + LOGPAGEHDRSIZE ; i++){
		switch ( tBuf[i]){ 
		case TEMPERATURE_PAGE:
		  devices[numscsidevices].TempPageSupported = 1;
		  break;
		case SMART_PAGE:
		  devices[numscsidevices].SmartPageSupported = 1;
		  break;
		default:
		  break;
		}
	      }	
	    }
	    numscsidevices++;
	  }
	}
    }
  }
}


void ataCompareSmartValues (atadevices_t *device, struct ata_smart_values new ){
    int i;
    int oldval,newval,idold,idnew;
    
    for ( i =0; i < NUMBER_ATA_SMART_ATTRIBUTES; i++){
      // which device is it?
      idnew=new.vendor_attributes[i].id;
      idold=device->smartval.vendor_attributes[i].id;
      
      if (idold && idnew){
	// if it's a valid attribute, compare values
	newval=new.vendor_attributes[i].current;
	oldval=device->smartval.vendor_attributes[i].current;
	if (oldval!=newval)
	  // values have changed; print them
	  printout(LOG_INFO, "Device: %s, S.M.A.R.T. Attribute: %i Changed from %i to %i\n",
		   device->devicename,idnew,oldval,newval);
      }
    }
}


int ataCheckDevice( atadevices_t *drive){
  struct ata_smart_values tempsmartval;
  struct ata_smart_thresholds tempsmartthres;
  int failed;

  // Coming into this function, *drive contains the last values measured,
  // and we read the NEW values into tempsmartval
  if (ataReadSmartValues(drive->fd,&tempsmartval))
    printout(LOG_INFO, "%s:Failed to read smart values\n", drive->devicename);
  
  // and we read the new thresholds into tempsmartthres
  if (ataReadSmartThresholds (drive->fd, &tempsmartthres))
    printout(LOG_INFO, "%s:Failed to read smart thresholds\n",drive->devicename);
  
  // See if any vendor attributes are below minimum, and print them out
  if ((failed=ataCheckSmart(tempsmartval,tempsmartthres,1)))
    printout(LOG_CRIT,"Device: %s, Failed attribute: %i\n",drive->devicename,failed);
  
  // WHEN IT WORKS, we should here add a call to ataSmartStatus2()

  // see if any values have changed.  Second argument is new values
  ataCompareSmartValues (drive , tempsmartval);
  
  // Save the new values into *drive for the next time around
  drive->smartval = tempsmartval;
  drive->smartthres = tempsmartthres;
  
  return 0;
}


int scsiCheckDevice( scsidevices_t *drive)
{
  UINT8 returnvalue;
  UINT8 currenttemp;
  UINT8 triptemp;
  
  currenttemp = triptemp = 0;
  
  if (scsiCheckSmart( drive->fd, drive->SmartPageSupported, &returnvalue, &currenttemp, &triptemp ) != 0)
    printout(LOG_INFO, "%s:Failed to read smart values\n", drive->devicename);
  
  if (returnvalue)
    printout(LOG_CRIT, "Device: %s, S.M.A.R.T. Failure: (%02x) %s\n", drive->devicename, 
	     returnvalue, scsiSmartGetSenseCode( returnvalue) );
  else
    printout(LOG_INFO,"Device: %s, Acceptable attribute: %d\n", drive->devicename, returnvalue);  
  
  if (currenttemp){
    if ( (currenttemp != drive->Temperature) && ( drive->Temperature) )
      printout(LOG_INFO, "Device: %s, Temperature changed %d degrees to %d degrees since last reading\n", 
	       drive->devicename, (int) (currenttemp - drive->Temperature), (unsigned int) currenttemp );
    
    drive->Temperature = currenttemp;
  } 
  return 0;
}

void CheckDevices (  atadevices_t *atadevices, scsidevices_t *scsidevices)
{
	int i;
	
	while (1){
	  for (i = 0; i < numatadevices;i++)
	    ataCheckDevice (  &atadevices[i]);
	  
	  for (i = 0; i < numscsidevices;i++)
	    scsiCheckDevice (  &scsidevices[i]);
	    
	  sleep ( checktime );
	}
}

char copyleftstring[]=
"Home page of smartd is " PROJECTHOME "\n\n"
"smartd comes with ABSOLUTELY NO WARRANTY. This\n"
"is free software, and you are welcome to redistribute it\n"
"under the terms of the GNU General Public License Version 2.\n"
"See http://www.gnu.org for further details.\n\n";

const char opts[] = { DEBUGMODE, EMAILNOTIFICATION, PRINTCOPYLEFT,'\0' };

/* Main Program */
int main (int argc, char **argv){
  
  atadevices_t atadevices[MAXATADEVICES], *atadevicesptr;
  scsidevices_t scsidevices[MAXSCSIDEVICES], *scsidevicesptr;
  int optchar;
  extern char *optarg;
  extern int  optopt, optind, opterr;
  
  numatadevices=0;
  numscsidevices=0;
  scsidevicesptr = scsidevices;
  atadevicesptr = atadevices;
  opterr=1;

  while (-1 != (optchar = getopt(argc, argv, opts))){
    switch(optchar) {
    case PRINTCOPYLEFT:
      printcopyleft=TRUE;
      break;
    case DEBUGMODE :
      debugmode  = TRUE;
      break;
    case EMAILNOTIFICATION:
      emailnotification = TRUE;
      break;
    case '?':
    default:
      debugmode=1;
      Usage();
      exit(-1);	
    }
  }
  
  if (printcopyleft){
    debugmode=1;
    printout(LOG_INFO,"smartd version %d.%d-%d Copyright (C) Bruce Allen 2002\n",
		RELEASE_MAJOR,RELEASE_MINOR,SMARTMONTOOLS_VERSION);
    printout(LOG_INFO,copyleftstring);
    printout(LOG_INFO,"CVS version IDs of files used to build this code are:\n%s%s%s",CVSid1,CVSid2,CVSid3);
    exit(0);
  }
  
  printout(LOG_INFO,"smartd version %d.%d-%d Copyright (C) Bruce Allen 2002\n",
		  RELEASE_MAJOR,RELEASE_MINOR,SMARTMONTOOLS_VERSION);
  
  if (!debugmode){
    daemon_init();
  }
  
  /* fork into independent process */
  atadevicescan (atadevicesptr); 
  scsidevicescan (scsidevicesptr);
  
  CheckDevices ( atadevicesptr, scsidevicesptr); 
  return 0;
}

