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

#define _GNU_SOURCE
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
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "atacmds.h"
#include "scsicmds.h"
#include "smartd.h"
#include "ataprint.h"
#include "extern.h"

// CVS ID strings
extern const char *CVSid1, *CVSid2;
const char *CVSid6="$Id: smartd.c,v 1.39 2002/10/29 10:06:20 ballen4705 Exp $" 
CVSID1 CVSID2 CVSID3 CVSID4 CVSID7;

// global variable used for control of printing, passing arguments, etc.
atamainctrl *con=NULL;

// This function prints either to stdout or to the syslog as needed
void printout(int priority,char *fmt, ...){
  va_list ap;
  // initialize variable argument list 
  va_start(ap,fmt);
  if (debugmode) 
    vprintf(fmt,ap);
  else
    vsyslog(priority,fmt,ap);
    closelog();
  va_end(ap);
  return;
}

// Printing function for debugging atacmds.
// in #if statement
void pout(char *fmt, ...){
  va_list ap;
  // initialize variable argument list 
  va_start(ap,fmt);
  // in debug mode we will print the output from the ataprint.o functions!
  if (debugmode)
    vprintf(fmt,ap);
  va_end(ap);
  return;
}

void goobye(){
  printout(LOG_CRIT,"smartd is exiting\n");
  return;
}

volatile sig_atomic_t fatal = 0;
// simple signal handler to print goodby message to syslog
void sighandler(int sig){
    printout(LOG_CRIT,"smartd received signal %d: %s\n",
	     sig,strsignal(sig));
    exit(1);
}


// Forks new process, closes all file descriptors, redirects stdin,
// stdout, stderr
int daemon_init(void){
  pid_t pid;
  int i;  

  if ((pid=fork()) < 0) {
    // unable to fork!
    printout(LOG_CRIT,"smartd unable to fork daemon process!\n");
    exit(1);
  }
  else if (pid)
    // we are the parent process -- exit cleanly
    exit(0);
  
  // from here on, we are the child process.
  setsid();


  // Fork one more time to avoid any possibility of having terminals
  if ((pid=fork()) < 0) {
    // unable to fork!
    printout(LOG_CRIT,"smartd unable to fork daemon process!\n");
    exit(1);
  }
  else if (pid)
    // we are the parent process -- exit cleanly
    exit(0);

  // Now we are the child's child...

  // close any open file descriptors
  for (i=getdtablesize();i>=0;--i)
    close(i);
  
  // redirect any IO attempts to /dev/null for stdin
  i=open("/dev/null",O_RDWR);
  // stdout
  dup(i);
  // stderr
  dup(i);
  umask(0);
  chdir("/");
  return(0);
}

// Prints header identifying version of code and home
void printhead(){
  printout(LOG_INFO,"smartd version %d.%d-%d - S.M.A.R.T. Daemon.\n",
           RELEASE_MAJOR, RELEASE_MINOR, SMARTMONTOOLS_VERSION);
  printout(LOG_INFO,"Home page is %s\n\n",PROJECTHOME);
  return;
}


// prints help info for configuration file directives
void Directives() {
  printout(LOG_INFO,"Configuration file Directives (following device name):\n");
  printout(LOG_INFO,"  -A    Device is an ATA device\n");
  printout(LOG_INFO,"  -S    Device is a SCSI device\n");
  printout(LOG_INFO,"  -c    Monitor SMART Health Status\n");
  printout(LOG_INFO,"  -l    Monitor SMART Error Log for changes\n");
  printout(LOG_INFO,"  -L    Monitor SMART Self-Test Log for new errors\n");
  printout(LOG_INFO,"  -f    Monitor for failure of any 'Usage' Attributes\n");
  printout(LOG_INFO,"  -p    Report changes in 'Prefailure' Attributes\n");
  printout(LOG_INFO,"  -u    Report changes in 'Usage' Attributes\n");
  printout(LOG_INFO,"  -t    Equivalent to -p and -u Directives\n");
  printout(LOG_INFO,"  -i ID Ignore Attribute ID for -f Directive\n");
  printout(LOG_INFO,"  -I ID Ignore Attribute ID for -p, -u or -t Directive\n");
  printout(LOG_INFO,"   #    Comment: text after a hash sign is ignored\n");
  printout(LOG_INFO,"   \\    Line continuation character\n");
  printout(LOG_INFO,"Attribute ID is a decimal integer 1 <= ID <= 255\n");
  printout(LOG_INFO,"All but -S directive are only implemented for ATA devices\n");
  printout(LOG_INFO,"Example: /dev/hda -a\n");
return;
}

/* prints help information for command syntax */
void Usage (void){
  printout(LOG_INFO,"usage: smartd -[opts] \n\n");
  printout(LOG_INFO,"Command Line Options:\n");
  printout(LOG_INFO,"  %c  Start smartd in debug Mode\n",DEBUGMODE);
  printout(LOG_INFO,"  %c  Print License, Copyright, and version information\n\n",PRINTCOPYLEFT);
  printout(LOG_INFO,"Optional configuration file: %s\n",CONFIGFILE);
  Directives();
}

// returns negative if problem, else fd>=0
int opendevice(char *device){
  int fd = open(device, O_RDONLY);
  if (fd<0) {
    if (errno<sys_nerr)
      printout(LOG_INFO,"Device: %s, %s, open() failed\n",device, sys_errlist[errno]);
    else
      printout(LOG_INFO,"Device: %s, open() failed\n",device);
    return -1;
  }
  // device opened sucessfully
  return fd;
}

// returns 1 if problem, else zero
int closedevice(int fd, char *name){
  if (close(fd)){
    if (errno<sys_nerr)
      printout(LOG_INFO,"Device: %s, %s, close(%d) failed\n", name, sys_errlist[errno], fd);
    else
      printout(LOG_INFO,"Device: %s, close(%d) failed\n",name,fd);
    return 1;
  }
  // device sucessfully closed
  return 0;
}

// returns <0 on failure
int ataerrorcount(int fd, char *name){
  struct ata_smart_errorlog log;
  
  if (-1==ataReadErrorLog(fd,&log)){
    printout(LOG_INFO,"Device: %s, Read SMART Error Log Failed\n",name);
    return -1;
  }
  
  // return current number of ATA errors
  return log.error_log_pointer?log.ata_error_count:0;
}

// returns <0 if problem
char selftesterrorcount(int fd, char *name){
  struct ata_smart_selftestlog log;

  if (-1==ataReadSelfTestLog(fd,&log)){
    printout(LOG_INFO,"Device: %s, Read SMART Self Test Log Failed\n",name);
    return -1;
  }
  
  // return current number of self-test errors
  return (char)ataPrintSmartSelfTestlog(log,0);
}



// scan to see what ata devices there are, and if they support SMART
int atadevicescan2(atadevices_t *devices, cfgfile *cfg){
  int fd;
  struct hd_driveid drive;
  char *device=cfg->name;
  
  // should we try to register this as an ATA device?
  if (!(cfg->tryata))
    return 1;
  
  // open the device
  if ((fd=opendevice(device))<0)
    // device open failed
    return 1;
  printout(LOG_INFO,"Device: %s, opened\n", device);
  
  // Get drive identity structure
  if (ataReadHDIdentity (fd,&drive) || !ataSmartSupport(drive) || ataEnableSmart(fd)){
    // device exists, but not able to do SMART
    printout(LOG_INFO,"Device: %s, not SMART capable, or couldn't enable SMART\n",device);
    close(fd);
    return 2; 
  }
  
  // capability check: SMART status
  if (cfg->smartcheck && ataSmartStatus2(fd)==-1){
    printout(LOG_INFO,"Device: %s, not capable of SMART Health Status check\n",device);
    cfg->smartcheck=0;
  }
  
  // capability check: Read smart values and thresholds
  if (cfg->usagefailed || cfg->prefail || cfg->usage) {
    devices->smartval=(struct ata_smart_values *)calloc(1,sizeof(struct ata_smart_values));
    devices->smartthres=(struct ata_smart_thresholds *)calloc(1,sizeof(struct ata_smart_thresholds));
    
    if (!devices->smartval || !devices->smartthres){
      printout(LOG_CRIT,"Not enough memory to obtain SMART data\n");
      exit(1);
    }
    
    if (ataReadSmartValues(fd,devices->smartval) ||
	ataReadSmartThresholds (fd,devices->smartthres)){
      printout(LOG_INFO,"Device: %s, Read SMART Values and/or Thresholds Failed\n",device);
      free(devices->smartval);
      free(devices->smartthres);
      cfg->usagefailed=cfg->prefail=cfg->usage=0;
    }
  }
  
  // capability check: self-test-log
  if (cfg->selftest){
    char val=selftesterrorcount(fd, device);
    if (val>=0)
      cfg->selflogcount=val;
    else
      cfg->selftest=0;
  }
  
  // capability check: ATA error log
  if (cfg->errorlog){
    int val=ataerrorcount(fd, device);
    if (val>=0)
      cfg->ataerrorcount=val;
    else
      cfg->errorlog=0;
  }
  
  // If not tests available or selected, return
  if (!(cfg->errorlog || cfg->selftest || cfg->smartcheck || 
	cfg->usagefailed || cfg->prefail || cfg->usage)) {
    close(fd);
    return 3;
  }
  
  // Do we still have entries available?
  if (numatadevices>=MAXATADEVICES){
    printout(LOG_CRIT,"smartd has found more than MAXATADEVICES=%d ATA devices.\n"
	     "Recompile code from " PROJECTHOME " with larger MAXATADEVICES\n",numatadevices);
    exit(1);
  }
  
  printout(LOG_INFO,"Device: %s, is SMART capable. Adding to \"monitor\" list.\n",device);
  // no need to try sending SCSI commands to this device!
  cfg->tryscsi=0;
  
  // we were called from a routine that has global storage for the name.  Keep pointer.
  devices->devicename=device;
  devices->cfg=cfg;
  
  
  numatadevices++;
  closedevice(fd, device);
  return 0;
}


// This function is hard to read and ought to be rewritten. Why in the
// world is the four-byte integer cast to a pointer to an eight-byte
// object?? Can anyone explain this obscurity?
int scsidevicescan(scsidevices_t *devices, char *device){
  int i, fd, smartsupport;
  unsigned char  tBuf[4096];
  
  printout(LOG_INFO,"Device: %s, opening\n", device);
  if ((fd=opendevice(device))<0)
    // device open failed
    return 1;
  
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

  // Device exists, and does SMART.  Add to list
  if (numscsidevices>=MAXSCSIDEVICES){
    printout(LOG_CRIT,"smartd has found more than MAXSCSIDEVICES=%d SCSI devices.\n"
	     "Recompile code from " PROJECTHOME " with larger MAXSCSIDEVICES\n",numscsidevices);
    exit(1);
  }

  // now we can proceed to register the device
  printout(LOG_INFO, "Device: %s, is SMART capable. Adding to \"monitor\" list.\n",device);

  // since device points to global memory, just keep that address
  devices[numscsidevices].devicename=device;

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
  closedevice(fd, device);
  return 0;
}

// We compare old and new values of the n'th attribute.  Note that n
// is NOT the attribute ID number.. If equal, return 0.  The thre
// structure is used to verify that the attributes are valid ones.  If
// the new value is lower than the old value, then we return both old
// and new values. new value=>lowest byte, old value=>next-to-lowest
// byte, id value=>next-to-next-to-lowest byte., and prefail flag x as
// bottom bit of highest byte.  See below (lsb on right)

//  [00000000x][attribute ID][old value][new value]
int  ataCompareSmartValues2(struct ata_smart_values *new,
			    struct ata_smart_values *old,
			    struct ata_smart_thresholds *thresholds,
			    int n){
  struct ata_smart_attribute *now,*was;
  struct ata_smart_threshold_entry *thre;
  unsigned char oldval,newval;
  int returnvalue;

  // check that attribute number in range, and no null pointers
  if (n<0 || n>=NUMBER_ATA_SMART_ATTRIBUTES || !new || !old || !thresholds)
    return 0;
  
  // pointers to disk's values and vendor's thresholds
  now=new->vendor_attributes+n;
  was=old->vendor_attributes+n;
  thre=thresholds->thres_entries+n;

  // consider only valid attributes, with same valid ID in all structures
  if (!now->id || !was->id || !thre->id || (now->id != was->id) || (now->id != thre->id))
    return 0;

  // if values have not changed, return
  newval=now->current;
  oldval=was->current;

  // if any values out of the allowed range, or the values haven't changed, return
  if (!newval || !oldval || newval>0xfe || oldval>0xfe || oldval==newval)
    return 0;
  
  // values have changed.  Construct output
  returnvalue=0;
  returnvalue |= newval;
  returnvalue |= oldval<<8;
  returnvalue |= now->id<<16;
  returnvalue |= (now->status.flag.prefailure)<<24;

  return returnvalue;
}

// This looks to see if the corresponding bit of the 32 bytes is set.
// This wastes a few bytes of storage but eliminates all searching and
// sorting functions! Entry is ZERO <==> the attribute ON. Calling
// with set=0 tells you if the attribute is being tracked or not.
// Calling with set=1 turns the attribute OFF.
int isattoff(unsigned char attr,unsigned char *data, int set){
  // locate correct attribute
  int loc=attr>>3;
  int bit=attr & 0x07;
  unsigned char mask=0x01<<bit;

  // attribute zero is always OFF
  if (!attr)
    return 1;

  if (!set)
    return (data[loc] & mask);
  
  data[loc]|=mask;
  // return value when setting makes no sense!
  return 0;
}


int ataCheckDevice(atadevices_t *drive){
  int fd,i;
  char *name=drive->devicename;
  cfgfile *cfg=drive->cfg;
  
  // if we can't open device, fail gracefully rather than hard --
  // perhaps the next time around we'll be able to open it
  if ((fd=opendevice(name))<0)
    return 1;
  
  // check smart status
  if (cfg->smartcheck){
    int status=ataSmartStatus2(fd);
    if (status==-1)
      printout(LOG_INFO,"Device: %s, not capable of SMART self-check\n",name);
    else if (status==1)
      printout(LOG_INFO,"Device: %s, FAILED SMART self-check. BACK UP DATA NOW!\n",name);
  }
  
  // Check everything that depends upon SMART Data (eg, Attribute values)
  if (cfg->usagefailed || cfg->prefail || cfg->usage){
    struct ata_smart_values     curval;
    struct ata_smart_thresholds *thresh=drive->smartthres;
    
    // Read current attribute values. *drive contains old values adn thresholds
    if (ataReadSmartValues(fd,&curval))
      printout(LOG_INFO, "Device: %s, failed to read SMART Attribute Data\n", name);
    else {  
      // look for failed usage attributes, or track usage or prefail attributes
      for (i=0; i<NUMBER_ATA_SMART_ATTRIBUTES; i++) {
	int att;
	
	// This block looks for usage attributes that have failed.
	// Prefail attributes that have failed are returned with a
	// positive sign. No failure returns 0. Usage attributes<0.
	if (cfg->usagefailed && ((att=ataCheckAttribute(&curval, thresh, i))<0)){
	  
	  // are we tracking this attribute?
	  att *= -1;
	  if (!isattoff(att, cfg->failatt, 0)){
	    char attname[64], *loc=attname;
	    
	    // get attribute name & skip white space
	    ataPrintSmartAttribName(loc, att);
	    while (*loc && *loc==' ') loc++;
	    
	    // warning message
	    printout(LOG_CRIT,"Device: %s, Failed SMART usage attribute: %s.\n", name, loc, name);
	  }
	}
	
	// This block tracks usage or prefailure attributes to see if they are changing
	if ((cfg->usage || cfg->prefail) && ((att=ataCompareSmartValues2(&curval, drive->smartval, thresh, i)))){
	  const int mask=0xff;
	  int prefail=(att>>24) & mask;
	  int id     =(att>>16) & mask;
	  int oldval =(att>>8)  & mask;
	  int newval =(att>>0)  & mask;
	  char attname[64],*loc=attname;
	  
	  // are we tracking this attribute?
	  if (!isattoff(id, cfg->trackatt, 0)){
	    
	    // get attribute name, skip spaces
	    ataPrintSmartAttribName(loc, id);
	    while (*loc && *loc==' ') loc++;
	    
	    // prefailure attribute
	    if (cfg->prefail && prefail)
	      printout(LOG_INFO, "Device: %s, SMART Prefailure Attribute: %s changed from %i to %i\n",
		       name, loc, oldval, newval);

	    // usage attribute
	    if (cfg->usage && !prefail)
	      printout(LOG_INFO, "Device: %s, SMART Usage Attribute: %s changed from %i to %i\n",
		       name, loc, oldval, newval);
	  }
	} // endof block tracking usage or prefailure
      } // end of loop over attributes
     
      // Save the new values into *drive for the next time around
      memcpy(drive->smartval,&curval,sizeof(curval));
    } 
  }
  
  // check if number of selftest errors has increased (note: may also DECREASE)
  if (cfg->selftest){
    char old=cfg->selflogcount;
    char new=selftesterrorcount(fd, name);
    if (new>old){
      printout(LOG_CRIT,"Device: %s, Self-Test Log error count increased from %d to %d\n",
	       name,old,new);
    }
    if (new>=0)
      // Needed suince self-test error count may  DECREASE
      cfg->selflogcount=new;
  }

  
  // check if number of ATA errors has increased
  if (cfg->errorlog){
    int old=cfg->ataerrorcount;
    int new=ataerrorcount(fd, name);
    if (new>old){
      printout(LOG_CRIT,"Device: %s, ATA error count increased from %d to %d\n",
	       name,old,new);
    }
    // this last line is probably not needed, count always increases
    if (new>=0)
      cfg->ataerrorcount=new;
  }
  closedevice(fd, name);
  return 0;
}



int scsiCheckDevice( scsidevices_t *drive){
  UINT8 returnvalue;
  UINT8 currenttemp;
  UINT8 triptemp;
  int fd;

  // if we can't open device, fail gracefully rather than hard --
  // perhaps the next time around we'll be able to open it
  if ((fd=opendevice(drive->devicename))<0)
    return 1;

  currenttemp = triptemp = 0;
  
  if (scsiCheckSmart(fd, drive->SmartPageSupported, &returnvalue, &currenttemp, &triptemp))
    printout(LOG_INFO, "Device: %s, failed to read SMART values\n", drive->devicename);
  
  if (returnvalue)
    printout(LOG_CRIT, "Device: %s, SMART Failure: (%02x) %s\n", drive->devicename, 
	     returnvalue, scsiSmartGetSenseCode( returnvalue) );
  else
    printout(LOG_INFO,"Device: %s, Acceptable attribute: %d\n", drive->devicename, returnvalue);  
  
  // Seems to completely ignore what capabilities were found on the
  // device when scanned
  if (currenttemp){
    if ((currenttemp != drive->Temperature) && (drive->Temperature))
      printout(LOG_INFO, "Device: %s, Temperature changed %d degrees to %d degrees since last reading\n", 
	       drive->devicename, (int) (currenttemp - drive->Temperature), (unsigned int) currenttemp );
    drive->Temperature = currenttemp;
  }
  closedevice(fd, drive->devicename);
  return 0;
}

void CheckDevices(atadevices_t *atadevices, scsidevices_t *scsidevices){
  int i;
  
  // If there are no devices to monitor, then exit
  if (!numatadevices && !numscsidevices){
    printout(LOG_INFO,"Unable to monitor any SMART enabled ATA or SCSI devices.\n");
    return;
  }

  // Infinite loop, which checkes devices
  printout(LOG_INFO,"Started monitoring %d ATA and %d SCSI devices\n",numatadevices,numscsidevices);
  while (1){
    for (i=0; i<numatadevices; i++) 
      ataCheckDevice(atadevices+i);
    
    for (i=0; i<numscsidevices; i++)
      scsiCheckDevice(scsidevices+i);
    
    sleep(checktime);
  }
}

char copyleftstring[]=
"smartd comes with ABSOLUTELY NO WARRANTY. This\n"
"is free software, and you are welcome to redistribute it\n"
"under the terms of the GNU General Public License Version 2.\n"
"See http://www.gnu.org for further details.\n\n";

cfgfile config[MAXENTRIES];


int parsetoken(char *token,cfgfile *cfg){
  char sym=token[1];
  char *name=cfg->name;
  int lineno=cfg->lineno;
  char *delim=" \n\t";

  // is the rest of the line a comment
  if (*token=='#')
    return 1;
  
  // is the token not recognized?
  if (*token!='-' || strlen(token)!=2) {
    printout(LOG_CRIT,"Drive: %s, unknown Directive: %s at line %d of file %s\n",
	     name,token,lineno,CONFIGFILE);
    Directives();
    exit(1);
  }
  
  // let's parse the token and swallow its argument
  switch (sym) {
    char *arg;
    char *endptr;
    int val;
    
  case 'A':
    // ATA device
    cfg->tryata=1;
    cfg->tryscsi=0;
    break;
  case 'S':
    //SCSI device
    cfg->tryscsi=1;
    cfg->tryata=0;
    break;
  case 'c':
    // check SMART status
    cfg->smartcheck=1;
    break;
  case 'f':
    // check for failure of usage attributes
    cfg->usagefailed=1;
    break;
  case 't':
    // track changes in all vendor attributes
    cfg->prefail=1;
    cfg->usage=1;
    break;
  case 'p':
    // track changes in prefail vendor attributes
    cfg->prefail=1;
    break;
  case 'u':
    //  track changes in usage vendor attributes
    cfg->usage=1;
    break;
  case 'L':
    // track changes in self-test log
    cfg->selftest=1;
    break;
  case 'l':
    // track changes ATA error log
    cfg->errorlog=1;
    break;
  case 'a':
    // monitor everything
    cfg->smartcheck=1;
    cfg->prefail=1;
    cfg->usagefailed=1;
    cfg->usage=1;
    cfg->selftest=1;
    cfg->errorlog=1;
    break;
  case 'i':
  case 'I':
    // ignore a particular vendor attribute for tracking (i) or
    // failure (I)
    arg=strtok(NULL,delim);
    // make sure argument is there
    if (!arg) {
      printout(LOG_CRIT,"Drive %s Directive: %s at line %d of file %s needs integer argument.\n",
	       name,token,lineno,CONFIGFILE);
      Directives();
      exit(1);
    }
    // get argument value, check that it's properly-formed, an
    // integer, and in-range
    val=strtol(arg,&endptr,10);
    if (*endptr!='\0' || val<=0 || val>255)  {
      printout(LOG_CRIT,"Drive %s Directive: %s at line %d of file %s has argument: %s, needs 0 < n < 256\n",
	       name,token,lineno,CONFIGFILE,arg);
      Directives();
      exit(1);
    }
    // put into correct list (bitmaps, access only with isattoff()
    // function. Turns OFF corresponding attribute.
    if (sym=='I')
      isattoff(val,cfg->trackatt,1);
    else
      isattoff(val,cfg->failatt,1);
    break;
  default:
    printout(LOG_CRIT,"Drive: %s, unknown option: %s at line %d of file %s\n",
	     name,token,lineno,CONFIGFILE);
    Directives();
    exit(1);
  }
  return 1;
}


int parseconfigline(int entry, int lineno,char *line){
  char *token,*copy;
  char *delim=" \n\t";
  char *name;
  int len;
  cfgfile *cfg;
  
  if (!(copy=strdup(line))){
    perror("no memory available to parse line");
    exit(1);
  }
  
  // get first token -- device name
  if (!(name=strtok(copy,delim)) || *name=='#'){
    free(copy);
    return 0;
  }

  // Is there space for another entry?
  if (entry>=MAXENTRIES){
    printout(LOG_CRIT,"Error: configuration file %s can have no more than %d entries\n",
	     CONFIGFILE,MAXENTRIES);
    exit(1);
  }

  // We've got a legit entry, clear structure
  cfg=config+entry;
  memset(cfg,0,sizeof(*config));

  // Save info to process memory for after forking 32 bytes contains 1
  // bit per possible attribute ID.  See isattoff()
  cfg->name=strdup(name);
  cfg->failatt=(unsigned char *)calloc(32,1);
  cfg->trackatt=(unsigned char *)calloc(32,1);
  
  if (!cfg->name || !cfg->failatt || !cfg->trackatt) {
    perror("no memory available to save name");
    exit(1);
  }

  cfg->lineno=lineno;
  cfg->tryscsi=cfg->tryata=1;
  
  // Try and recognize if a IDE or SCSI device.  These can be
  // overwritten by configuration file directives.
  len=strlen(name);
  if (len>5 && !strncmp("/dev/h",name, 6))
    cfg->tryscsi=0;
  
  if (len>5 && !strncmp("/dev/s",name, 6))
    cfg->tryata=0;

  // parse tokens one at a time from the file
  while ((token=strtok(NULL,delim)) && parsetoken(token,cfg)){
#if 0
  printout(LOG_INFO,"Parsed token %s\n",token);
#endif
  }

  // basic sanity check -- are any options turned on?
  if (!(cfg->smartcheck || cfg->usagefailed || cfg->prefail || cfg->usage || cfg->selftest || cfg->errorlog || cfg->tryscsi)){
    printout(LOG_CRIT,"Drive: %s, no monitoring Directives on line %d of file %s\n",
	     cfg->name, cfg->lineno, CONFIGFILE);
    Directives();
    exit(1);
  }

  entry++;
  free(copy);
  return 1;
}

// returns number of entries in config file, or 0 if no config file
// exists.  A config file with zero entries will cause an error
// message and an exit.
int parseconfigfile(){
  FILE *fp;
  int entry=0,lineno=1,cont=0,contlineno=0;
  char line[MAXLINELEN+2];
  char fullline[MAXCONTLINE+1];

  // Open config file, if it exists
  fp=fopen(CONFIGFILE,"r");
  if (fp==NULL && errno!=ENOENT){
    // file exists but we can't read it
    if (errno<sys_nerr)
      printout(LOG_CRIT,"%s: Unable to open configuration file %s\n",
	       sys_errlist[errno],CONFIGFILE);
    else
      printout(LOG_CRIT,"Unable to open configuration file %s\n",CONFIGFILE);
    exit(1);
  }
  
  // No config file
  if (fp==NULL)
    return 0;
  
  // configuration file exists
  printout(LOG_INFO,"Using configuration file %s\n",CONFIGFILE);

  // parse config file line by line
  while (1) {
    int len=0;
    char *lastslash;
    char *comment;
    char *code;

    // make debugging simpler
    memset(line,0,sizeof(line));

    // get a line
    code=fgets(line,MAXLINELEN+2,fp);
    
    // are we at the end of the file?
    if (!code){
      if (cont) {
	// the final line is part of a continuation line
	cont=0;
	entry+=parseconfigline(entry,lineno,fullline);
      }
      break;
    }

    // input file line number
    contlineno++;
    
    // See if line is too long
    len=strlen(line);
    if (len>MAXLINELEN){
      char *warn;
      if (line[len-1]=='\n')
	warn="(including newline!) ";
      else
	warn="";
      printout(LOG_CRIT,"Error: line %d of file %s %sis more than %d characters.\n",
	       contlineno,CONFIGFILE,warn,MAXLINELEN);
      exit(1); 
    }

    // Ignore anything after comment symbol
    if ((comment=index(line,'#'))){
      *comment='\0';
      len=strlen(line);
    }

    // is the total line (made of all continuation lines) too long?
    if (cont+len>MAXCONTLINE){
      printout(LOG_CRIT,"Error: continued line %d (actual line %d) of file %s is more than %d characters.\n",
	       lineno,contlineno,CONFIGFILE,MAXCONTLINE);
      exit(1);
    }
    
    // copy string so far into fullline, and increment length
    strcpy(fullline+cont,line);
    cont+=len;

    // is this a continuation line.  If so, replace \ by space and look at next line
    if ( (lastslash=rindex(line,'\\')) && !strtok(lastslash+1," \n\t")){
      *(fullline+(cont-len)+(lastslash-line))=' ';
      continue;
    }

    // Not a continuation line. Parse it
    entry+=parseconfigline(entry,lineno,fullline);
    lineno++;
    cont=0;
  }
  fclose(fp);
  if (entry)
    return entry;
  
  printout(LOG_CRIT,"Configuration file %s contains no devices (like /dev/hda)\n",CONFIGFILE);
  exit(1);
}

// const char opts[] = {DEBUGMODE, EMAILNOTIFICATION, PRINTCOPYLEFT,'h','?','\0' };
const char opts[] = {DEBUGMODE, PRINTCOPYLEFT,'h','?','\0' };


// Parses input line, prints usage message and
// version/license/copyright messages
void ParseOpts(int argc, char **argv){
  extern char *optarg;
  extern int  optopt, optind, opterr;
  int optchar;

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
	printout(LOG_CRIT,"=======> UNRECOGNIZED OPTION: %c <======= \n\n",optopt);
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
    char out[CVSMAXLEN];
    debugmode=1;
    printhead();
    printout(LOG_INFO,copyleftstring);
    printout(LOG_INFO,"CVS version IDs of files used to build this code are:\n");
    printone(out,CVSid1);
    printout(LOG_INFO,"%s",out);
    printone(out,CVSid2);
    printout(LOG_INFO,"%s",out);
    printone(out,CVSid6);
    printout(LOG_INFO,"%s",out);
    exit(0);
  }
  
  // print header
  printhead();
  return;
}

// Function we call if no configuration file was found.  It makes
// entries for /dev/hd[a-l] and /dev/sd[a-z].
int makeconfigentries(int num, char *name, int isata, int start){
  int i;
  
  if (MAXENTRIES<(start+num)){
    printout(LOG_CRIT,"Error: simulated config file can have no more than %d entries\n",MAXENTRIES);
    exit(1);
  }
  
  for(i=0; i<num; i++){
    cfgfile *cfg=config+start+i;
    
    // clear all fields of structure
    memset(cfg,0,sizeof(*cfg));
    
    // select if it's a SCSI or ATA device
    cfg->tryata=isata;
    cfg->tryscsi=!isata;
    
    // enable all possible tests
    cfg->smartcheck=1;
    cfg->prefail=1;
    cfg->usagefailed=1;
    cfg->usage=1;
    cfg->selftest=1;
    cfg->errorlog=1;
    
    // lineno==0 is our clue that the device was not found in a
    // config file!
    cfg->lineno=0;
    
    // put in the device name
    cfg->name=strdup(name);
    cfg->failatt=(unsigned char *)calloc(32,1);
    cfg->trackatt=(unsigned char *)calloc(32,1);
    if (!cfg->name || !cfg->failatt || !cfg->trackatt) {
      perror("no memory available to save name");
      exit(1);
    }
    // increment final character of the name
    cfg->name[strlen(name)-1]+=i;
  }
  return i;
}


void cantregister(char *name, char *type, int line){
  if (line)
    printout(LOG_INFO,"Unable to register %s device %s at line %d of file %s\n",
	     type, name, line, CONFIGFILE);
  else
    printout(LOG_INFO,"Unable to register %s device %s\n",
	     type, name);
  return;
}


/* Main Program */
int main (int argc, char **argv){
  atadevices_t atadevices[MAXATADEVICES], *atadevicesptr=atadevices;
  scsidevices_t scsidevices[MAXSCSIDEVICES], *scsidevicesptr=scsidevices;
  int i,entries;
  atamainctrl control;
  
  // initialize global communications variables
  con=&control;
  memset(con,0,sizeof(control));
  
  // initialize global counters
  numatadevices=numscsidevices=0;
  
  // Parse input and print header and usage info if needed
  ParseOpts(argc,argv);
  
  // Do we mute printing from ataprint commands?
  con->quietmode=0;
  con->veryquietmode=debugmode?0:1;
  
  // look in configuration file CONFIGFILE (normally /etc/smartd.conf)
  entries=parseconfigfile();

  // If in background as a daemon, fork and close file descriptors
  if (!debugmode){
    daemon_init();
  }

  // setup signal handler for shutdown
  if (signal(SIGINT, sighandler)==SIG_IGN)
    signal(SIGINT, SIG_IGN);
  if (signal(SIGTERM, sighandler)==SIG_IGN)
    signal(SIGTERM, SIG_IGN);
  if (signal(SIGQUIT, sighandler)==SIG_IGN)
    signal(SIGQUIT, SIG_IGN);
  
  // install goobye message
  atexit(goobye);
  
  // if there was no config file, create needed entries
  if (!entries){
    printout(LOG_INFO,"smartd: file %s not found. Searching for devices.\n",CONFIGFILE);
    entries+=makeconfigentries(MAXATADEVICES,"/dev/hda",1,entries);
    entries+=makeconfigentries(MAXSCSIDEVICES,"/dev/sda",0,entries);
  }
  
  // Register entries
  for (i=0;i<entries;i++){
    // register ATA devices
    if (config[i].tryata && atadevicescan2(atadevicesptr+numatadevices, config+i))
      cantregister(config[i].name, "ATA", config[i].lineno);
    
    // then register SCSI devices
    if (config[i].tryscsi && scsidevicescan(scsidevicesptr, config[i].name))
      cantregister(config[i].name, "SCSI", config[i].lineno);
  }
  
  
  // Now start an infinite loop that checks all devices
  CheckDevices(atadevicesptr, scsidevicesptr); 
  return 0;
}

