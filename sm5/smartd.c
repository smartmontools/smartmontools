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

#include <errno.h>
#include <stdlib.h>
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
const char *CVSid3="$Id: smartd.c,v 1.31 2002/10/25 14:54:14 ballen4705 Exp $" 
CVSID1 CVSID4 CVSID7;

int daemon_init(void){
  pid_t pid;
  int i;  

  if ( (pid = fork()) < 0)
    // unable to fork!
    exit(1);
  else if (pid != 0)
    // we are the parent process -- exit cleanly
    exit (0);

  // from here on, we are the child process
  setsid();

  // close any open file descriptors
  for (i=getdtablesize();i>=0;--i)
    close(i);

  // redirect any IO attempts to /dev/null
  // open stdin
  i=open("/dev/null",O_RDWR);
  // stdout
  dup(i);
  // stderr
  dup(i);
  umask(0);
  chdir("/");
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

// Printing function for atacmds
void pout(char *fmt, ...){
  va_list ap;
  // initialize variable argument list 
  va_start(ap,fmt);
  va_end(ap);
  return;
#if (0)
  // print out
  vprintf(fmt,ap);
  va_end(ap);
  return;
#endif
}


void printhead(){
  printout(LOG_INFO,"smartd version %d.%d-%d - S.M.A.R.T. Daemon.\n",
           RELEASE_MAJOR, RELEASE_MINOR, SMARTMONTOOLS_VERSION);
  printout(LOG_INFO,"Home page is %s\n\n",PROJECTHOME);
  return;
}

/* prints help information for command syntax */
void Usage (void){
  printout(LOG_INFO,"usage: smartd -[opts] \n\n");
  printout(LOG_INFO,"Read Only Options:\n");
  printout(LOG_INFO,"   %c  Start smartd in debug Mode\n",DEBUGMODE);
  printout(LOG_INFO,"   %c  Print License, Copyright, and version information\n\n",PRINTCOPYLEFT);
  printout(LOG_INFO,"Configuration file: /etc/smartd.conf\n");
}
	
// scan to see what ata devices there are, and if they support SMART
int atadevicescan (atadevices_t *devices, char *device){
  int fd;
  struct hd_driveid drive;
  
  printout(LOG_INFO,"Opening device %s\n", device);
  fd = open(device, O_RDONLY);
  if (fd < 0) {
    if (errno<sys_nerr)
      printout(LOG_INFO,"%s: Device: %s, Opening device failed\n",sys_errlist[errno],device);
    else
      printout(LOG_INFO,"Device: %s, Opening device failed\n",device);
    return 1;
  }
  
  if (ataReadHDIdentity (fd,&drive) || !ataSmartSupport(drive) || ataEnableSmart(fd)){
    // device exists, but not able to do SMART
    close(fd);
    printout(LOG_INFO,"Device: %s, Found but not SMART capable, or couldn't enable SMART\n",device);
    return 2;
  }
  
  // Does device support read values and read thresholds?  We should
  // modify this next block for devices that do support SMART status
  // but don't support read values and read thresholds.
  if (ataReadSmartValues (fd,&devices[numatadevices].smartval)){
    close(fd);
    printout(LOG_INFO,"Device: %s, Read SMART Values Failed\n",device);
    return 3;
  }
  else if (ataReadSmartThresholds (fd,&devices[numatadevices].smartthres)){
    close(fd);
    printout(LOG_INFO,"Device: %s, Read SMART Thresholds Failed\n",device);
    return 4;
  }
  
  // device exists, and does SMART.  Add to list
  printout(LOG_INFO,"%s Found and is SMART capable. Adding to \"monitor\" list.\n",device);
  devices[numatadevices].fd = fd;
  strcpy(devices[numatadevices].devicename, device);
  devices[numatadevices].drive = drive;
  
  // This makes NO sense.  We may want to know if the drive supports
  // Offline Surface Scan, for example.  But checking if it supports
  // self-tests seems useless. In any case, smartd NEVER uses this
  // field anywhere...
  devices[numatadevices].selftest = 
    isSupportSelfTest(devices[numatadevices].smartval);
  
  numatadevices++;
  return 0;
}

// This function is hard to read and ought to be rewritten. Why in the
// world is the four-byte integer cast to a pointer to an eight-byte
// object??
int scsidevicescan (scsidevices_t *devices, char *device){
  int i, fd, smartsupport;
  unsigned char  tBuf[4096];
  
  
  // open device
  printout(LOG_INFO,"Opening device %s\n", device);
  fd=open(device, O_RDWR);
  if (fd<0) {
    if (errno<sys_nerr)
      printout(LOG_INFO,"%s: Device: %s, Opening device failed\n",sys_errlist[errno],device);
    else
      printout(LOG_INFO,"Device: %s, Opening device failed\n", device);
    return 1;
  }

  // check that it's ready for commands
  if (!testunitready(fd)){
    printout(LOG_INFO,"Device: %s, Failed Test Unit Ready\n", device);
    close(fd);
    return 2;
  }
  
  // make sure that we can read mode page
  if (modesense(fd, 0x1c, (UINT8 *) &tBuf)){
    printout(LOG_INFO,"Device: %s, Failed read of ModePage 0x1c\n", device);
    close(fd);
    return 3;
  }
  
  // see if SMART is supported and enabled
  if (scsiSmartSupport(fd, (UINT8 *) &smartsupport) ||
      (smartsupport & DEXCPT_ENABLE)){
    printout(LOG_INFO,"Device: %s, SMART not supported or not enabled\n", device);
    close(fd);
    return 4;
  }

  // now we can proceed to register the device
  printout(LOG_INFO, "Device: %s, Found and is SMART capable. Adding to \"monitor\" list.\n",device);
  devices[numscsidevices].fd = fd;
  strcpy(devices[numscsidevices].devicename,device);

  // register the supported functionality.  The smartd code does not
  // seem to make any further use of this information.
  if (logsense(fd, SUPPORT_LOG_PAGES, (UINT8 *) &tBuf) == 0){
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
  return 0;
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
	if (oldval!=newval){
	  // values have changed; print them
	  char *loc,attributename[64];
	  loc=attributename;
	  ataPrintSmartAttribName(attributename,idnew);
	  // skip blank space in name
	  while (*loc && *loc==' ')
	    loc++;
	  printout(LOG_INFO, "Device: %s, SMART Attribute: %s changed from %i to %i\n",
		   device->devicename,loc,oldval,newval);
	}
      }
    }
}


int ataCheckDevice( atadevices_t *drive){
  struct ata_smart_values tempsmartval;
  struct ata_smart_thresholds tempsmartthres;
  int failed;
  char *loc,attributename[64];

  // Coming into this function, *drive contains the last values measured,
  // and we read the NEW values into tempsmartval
  if (ataReadSmartValues(drive->fd,&tempsmartval))
    printout(LOG_INFO, "%s:Failed to read SMART values\n", drive->devicename);
  
  // and we read the new thresholds into tempsmartthres
  if (ataReadSmartThresholds (drive->fd, &tempsmartthres))
    printout(LOG_INFO, "%s:Failed to read SMART thresholds\n",drive->devicename);
  
  // See if any vendor attributes are below minimum, and print them out
  if ((failed=ataCheckSmart(tempsmartval,tempsmartthres,1))){
    ataPrintSmartAttribName(attributename,failed);
    // skip blank space in name
    loc=attributename;
    while (*loc && *loc==' ')
      loc++;
    printout(LOG_CRIT,"Device: %s, Failed SMART attribute: %s. Use smartctl -a %s.\n",
	     drive->devicename,loc,drive->devicename);
  }

  // WHEN IT WORKS, we should here add a call to ataSmartStatus2()
  // either in addition to or instead of the ataCheckSmart command
  // above. This is the "right" long-term solution.
  
  // see if any values have changed.  Second argument is new values
  ataCompareSmartValues (drive , tempsmartval);
  
  // Save the new values into *drive for the next time around
  drive->smartval = tempsmartval;
  drive->smartthres = tempsmartthres;
  
  return 0;
}



int scsiCheckDevice( scsidevices_t *drive){
  UINT8 returnvalue;
  UINT8 currenttemp;
  UINT8 triptemp;
  
  currenttemp = triptemp = 0;
  
  if (scsiCheckSmart( drive->fd, drive->SmartPageSupported, &returnvalue, &currenttemp, &triptemp ) != 0)
    printout(LOG_INFO, "%s:Failed to read SMART values\n", drive->devicename);
  
  if (returnvalue)
    printout(LOG_CRIT, "Device: %s, SMART Failure: (%02x) %s\n", drive->devicename, 
	     returnvalue, scsiSmartGetSenseCode( returnvalue) );
  else
    printout(LOG_INFO,"Device: %s, Acceptable attribute: %d\n", drive->devicename, returnvalue);  
  
  // Seems to completely ignore what capabilities were found on the
  // device when scanned
  if (currenttemp){
    if ( (currenttemp != drive->Temperature) && ( drive->Temperature) )
      printout(LOG_INFO, "Device: %s, Temperature changed %d degrees to %d degrees since last reading\n", 
	       drive->devicename, (int) (currenttemp - drive->Temperature), (unsigned int) currenttemp );
    
    drive->Temperature = currenttemp;
  } 
  return 0;
}

void CheckDevices (  atadevices_t *atadevices, scsidevices_t *scsidevices){
  int i;
  
  if (!numatadevices && !numscsidevices){
    printout(LOG_INFO,"Unable to monitor any SMART enabled ATA or SCSI devices.\n");
    return;
  }

  printout(LOG_INFO,"Started monitoring %d ATA and %d SCSI devices\n",numatadevices,numscsidevices);
  while (1){
    for (i = 0; i < numatadevices;i++)
      ataCheckDevice (  &atadevices[i]);
    
    for (i = 0; i < numscsidevices;i++)
      scsiCheckDevice (  &scsidevices[i]);
    
    sleep ( checktime );
  }
}


int massagecvs(char *out,const char *in){
  char filename[128], version[128], date[128];
  int i=0;
  const char *savein=in;

  // skip to I of $Id:
  while (*in !='\0' && *in!='I')
    in++;
  
  // skip to start of filename
  if (!*in)
    return 0;
  in+=4;

  // copy filename
  i=0;
  while (i<100 && *in!=',' && *in)
    filename[i++]=*in++;
  filename[i]='\0';
  if (!*in)
    return 0;

  // skip ,v and space
  in+=3;

  i=0;
  // copy version number
  while (i<100 && *in!=' ' && *in)
    version[i++]=*in++;
  version[i]='\0';
  if (!*in)
    return 0;

  // skip space
  in++;
  // copy date
  i=0;
  while (i<100 && *in!=' ' && *in)
    date[i++]=*in++;
  date[i]='\0';

  sprintf(out,"%-13s revision: %-6s date: %-15s", filename, version, date);
  return in-savein;
}

// prints a single set of CVS ids
void printone(const char *cvsid){
  char strings[512];
  const char *here;
  int len,line=1;
  here=cvsid;
  while ((len=massagecvs(strings,here))){
    switch (line++){
    case 1:
      printout(LOG_INFO,"Module:");
      break;
    default:
      printout(LOG_INFO,"  uses:");
    } 
    printout(LOG_INFO," %s\n",strings);
    here+=len;
  }
  return;
}

char copyleftstring[]=
"smartd comes with ABSOLUTELY NO WARRANTY. This\n"
"is free software, and you are welcome to redistribute it\n"
"under the terms of the GNU General Public License Version 2.\n"
"See http://www.gnu.org for further details.\n\n";

cfgfile config[MAXENTRIES];


// returns number of entries in config file, or 0 if no config file exists
int parseconfigfile(){
  FILE *fp;
  int entry=0,lineno=0;
  char line[MAXLINELEN+2];
  
  // Open config file, if it exists
  fp=fopen(CONFIGFILE,"r");
  if (fp==NULL && errno!=ENOENT){
    // file exists but we can't read it
    if (errno<sys_nerr)
      printout(LOG_INFO,"%s: Unable to open configuration file %s\n",
	       sys_errlist[errno],CONFIGFILE);
    else
      printout(LOG_INFO,"Unable to open configuration file %s\n",CONFIGFILE);
    exit(1);
  }
  
  // No config file
  if (fp==NULL)
    return 0;
  
  // configuration file exists.  Read it and search for devices
  printout(LOG_INFO,"Using configuration file %s\n",CONFIGFILE);
  while (fgets(line,MAXLINELEN+2,fp)){
    int len;
    char *dev;
    
    // track linenumber for error messages
    lineno++;
    
    // See if line is too long
    len=strlen(line);
    if (len>MAXLINELEN){
      printout(LOG_INFO,"Error: line %d of file %s is more than than %d characters long.\n",
	       lineno,CONFIGFILE,MAXLINELEN);
      exit(1); 
    }
    
    // eliminate any terminating newline
    if (line[len-1]=='\n'){
      len--;
      line[len]='\0';
    }
    
    // Skip white space
    dev=line;
    while (*dev && (*dev==' ' || *dev=='\t'))
      dev++;
    len=strlen(dev);
    
    // If line is blank, or a comment, skip it
    if (!len || *dev=='#')
      continue;
    
    // We've got a legit entry
    if (entry>=MAXENTRIES){
      printout(LOG_INFO,"Error: configuration file %s can have no more than %d entries\n",
	       CONFIGFILE,MAXENTRIES);
      exit(1);
    }
    
    // Copy information into data structure for after forking
    strcpy(config[entry].name,dev);
    config[entry].lineno=lineno;
    config[entry].tryscsi=config[entry].tryata=1;
    
    // Try and recognize if a IDE or SCSI device
    if (len>5 && !strncmp("/dev/h",dev, 6))
      config[entry].tryscsi=0;
    
    if (len>5 && !strncmp("/dev/s",dev, 6))
      config[entry].tryata=0;
    
    entry++;
  }
  fclose(fp);
  if (entry)
    return entry;
  
  printout(LOG_INFO,"Configuration file %s contained no devices (like /dev/hda)\n",CONFIGFILE);
  exit(1);
}

const char opts[] = { DEBUGMODE, EMAILNOTIFICATION, PRINTCOPYLEFT,'h','?','\0' };

/* Main Program */
int main (int argc, char **argv){
  atadevices_t atadevices[MAXATADEVICES], *atadevicesptr;
  scsidevices_t scsidevices[MAXSCSIDEVICES], *scsidevicesptr;
  int optchar,i;
  extern char *optarg;
  extern int  optopt, optind, opterr;
  int entries;
  
  numatadevices=0;
  numscsidevices=0;
  scsidevicesptr = scsidevices;
  atadevicesptr = atadevices;
  opterr=optopt=0;

  // Parse input options:
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
    case 'h':
    default:
      debugmode=1;
      if (optopt) {
	printhead();
	printout(LOG_INFO,"=======> UNRECOGNIZED OPTION: %c <======= \n\n",optopt);
	Usage();
	exit(-1);
      }
      printhead();
      Usage();
      exit(0);
    }
  }
  
  // If needed print copyright, license and version information
  if (printcopyleft){
    debugmode=1;
    printhead();
    printout(LOG_INFO,copyleftstring);
    printout(LOG_INFO,"CVS version IDs of files used to build this code are:\n");
    printone(CVSid3);
    printone(CVSid1);
    printone(CVSid2);
    exit(0);
  }
  
  // print header
  printhead();
 
  // look in configuration file CONFIGFILE (normally /etc/smartd.conf)
  entries=parseconfigfile();

  // If in background as a daemon, fork and close file descriptors
  if (!debugmode){
    daemon_init();
  }
  
  // If we found a config file, look at its entries
  if (entries)
    for (i=0;i<entries;i++){
      if (config[i].tryata && atadevicescan(atadevicesptr, config[i].name))
	printout(LOG_INFO,"Unable to register ATA device %s at line %d of file %s\n",
		 config[i].name, config[i].lineno, CONFIGFILE);
      
      if (config[i].tryscsi && scsidevicescan(scsidevicesptr, config[i].name))
	printout(LOG_INFO,"Unable to register SCSI device %s at line %d of file %s\n",
		 config[i].name, config[i].lineno, CONFIGFILE);
    }
  else {
    char deviceata[] = "/dev/hda";
    char devicescsi[]= "/dev/sda";
    printout(LOG_INFO,"No configuration file %s found. Searching for devices.\n",CONFIGFILE);
    for(i=0;i<MAXATADEVICES;i++,deviceata[7]++)
      atadevicescan(atadevicesptr, deviceata);    
    for(i=0;i<MAXSCSIDEVICES;i++,devicescsi[7]++)
      scsidevicescan(scsidevicesptr, devicescsi);
  }
  
  CheckDevices(atadevicesptr, scsidevicesptr); 
  return 0;
}

