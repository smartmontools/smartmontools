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
#include <sys/wait.h>
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
#include <time.h>
#include <limits.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include "atacmds.h"
#include "scsicmds.h"
#include "smartd.h"
#include "ataprint.h"
#include "extern.h"


// CVS ID strings
extern const char *CVSid1, *CVSid2;
const char *CVSid6="$Id: smartd.cpp,v 1.88 2002/12/16 12:27:00 ballen4705 Exp $" 
CVSID1 CVSID2 CVSID3 CVSID4 CVSID7;

// global variable used for control of printing, passing arguments, etc.
atamainctrl *con=NULL;

// Two other globals -- number of ATA and SCSI devices being monitored
int numatadevices=0;
int numscsidevices=0;

// How long to sleep between checks.  Handy as global variable for
// debugging
int checktime=CHECKTIME;

// Needed to interrupt sleep when catching SIGUSR1.  Unix Gurus: I
// know that this can be done better.  Please tell me how -- use email
// address for Bruce Allen at the top of this file.  Search for
// "sleeptime" to see what I am doing.
volatile int sleeptime=CHECKTIME;

// Interrupt sleep if we get a SIGUSR1.  Unix Gurus: I know that this
// can be done better.  Please tell me how -- use email address for
// Bruce Allen at the top of this file. Search for "sleeptime" to see
// what I am doing.
void sleephandler(int sig){
  int oldsleeptime=sleeptime;
  sleeptime=0;
  printout(LOG_CRIT,"Signal USR1 - checking devices now rather than in %d seconds.\n",oldsleeptime<0?0:oldsleeptime);
  return;
}

// Global Variables for command line options. These should go into a
// structure at some point.
unsigned char debugmode               = FALSE;

// This function prints either to stdout or to the syslog as needed

// [From GLIBC Manual: Since the prototype doesn't specify types for
// optional arguments, in a call to a variadic function the default
// argument promotions are performed on the optional argument
// values. This means the objects of type char or short int (whether
// signed or not) are promoted to either int or unsigned int, as
// appropriate.]
void printout(int priority,char *fmt, ...){
  va_list ap;
  // initialize variable argument list 
  va_start(ap,fmt);
  if (debugmode) 
    vprintf(fmt,ap);
  else {
    openlog("smartd",LOG_PID,LOG_DAEMON);
    vsyslog(priority,fmt,ap);
    closelog();
  }
  va_end(ap);
  return;
}

// If address is null, this just prints a warning message.  But if
// address is non-null then send and log a warning email.
void printandmail(cfgfile *cfg, int which, int priority, char *fmt, ...){
  char command[2048], message[256], hostname[256], additional[256];
  char original[256], further[256], domainname[256];
  int status;
  time_t epoch;
  va_list ap;
  const int day=24*3600;
  int days=0;
  char *address=cfg->address;
  mailinfo *mail=cfg->maildata+which;
  
  // See if user wants us to send mail
  if (!address)
    return;
  
  // check for sanity
  if (cfg->emailopt<0 || cfg->emailopt>3){
    printout(LOG_INFO,"internal error in printandmail(): cfg->emailopts=%d\n",cfg->emailopt);
    return;
  }
  
  // Return if a single warning mail has been sent.
  if ((cfg->emailopt==0 || cfg->emailopt==1) && mail->logged)
    return;
  
  // To decide if to send mail, we need to know what time it is.
  epoch=time(NULL);

  // Return if less than one day has gone by
  if (cfg->emailopt==2 && mail->logged && epoch<(mail->lastsent+day))
    return;

  // Return if less than 2^(logged-1) days have gone by
  if (cfg->emailopt==3 && mail->logged){
    days=0x01<<(mail->logged-1);
    days*=day;
    if  (epoch<(mail->lastsent+days))
      return;
  }

  // record the time of this mail message, and the first mail message
  if (!mail->logged)
    mail->firstsent=epoch;
  mail->lastsent=epoch;
  
  // get system host & domain names (not null terminated if length=MAX) 
  if (gethostname(hostname, 256))
    sprintf(hostname,"Unknown host");
  else
    hostname[255]='\0';
  if (getdomainname(domainname, 256))
    sprintf(hostname,"Unknown domain");
  else
    domainname[255]='\0';
  
  // print warning string into message
  va_start(ap, fmt);
  vsnprintf(message, 256, fmt, ap);
  va_end(ap);

  // appropriate message about further information
  additional[0]=original[0]=further[0]='\0';
  if (which) {
    sprintf(further,"You can also use the smartctl utility for further investigation.\n");

    switch (cfg->emailopt){
    case 0:
    case 1:
      sprintf(additional,"No additional email messages about this problem will be sent.\n");
      break;
    case 2:
      sprintf(additional,"Another email message will be sent in 24 hours if the problem persists.\n");
      break;
    case 3:
      sprintf(additional,"Another email message will be sent in %d days if the problem persists\n",
	      (0x01)<<mail->logged);
      break;
    }
    if (cfg->emailopt>1 && mail->logged)
      sprintf(original,"The original email about this issue was sent at %s\n",ctime(&(mail->firstsent)));
  }
  
  // now construct a command to send this as EMAIL
  snprintf(command, 2048, "mail -s 'SMART errors detected on host: %s' %s > /dev/null 2> /dev/null << \"ENDMAIL\"\n"
	   "This email was generated by the smartd daemon running on host:\n"
	   "%s in domain: %s\n\n"
	   "The following warning/error was logged by the smartd daemon:\n"
	   "%s\n"
	   "For further details see the syslog (/var/log/messages) on host:\n"
	   "%s\n\n"
	   "%s%s%s"
	   "ENDMAIL\n",
	   hostname, address, hostname, domainname, message, hostname, further, original, additional);

  // issue the command to send email
  status=system(command);
#ifdef WEXITSTATUS
  if (WEXITSTATUS(status))
#else
  if (status)
#endif
    printout(LOG_CRIT,"Email warning message to %s failed (32-bit exit status: %d)\n",address,status);
  else {
    if (which)
      printout(LOG_INFO,"Email warning message sent to %s\n",address);
    else
      printout(LOG_INFO,"Email test message sent to %s\n",address);
  }

  // increment mail sent counter
  mail->logged++;
 
  return;
}

// Printing function for watching ataprint commands, or losing them
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

// tell user that we ignore HUP signals
void huphandler(int sig){
  printout(LOG_CRIT,"HUP ignored: smartd does NOT re-read /etc/smartd.conf.\n");
  return;
}

// simple signal handler to print goodby message to syslog
void sighandler(int sig){
    printout(LOG_CRIT,"smartd received signal %d: %s\n",
	     sig, strsignal(sig));
    exit(1);
}

void goobye(){
  printout(LOG_CRIT,"smartd is exiting\n");
  return;
}

// Forks new process, closes all file descriptors, redirects stdin,
// stdout, stderr
int daemon_init(void){
  pid_t pid;
  int i;  

  // flush all buffered streams.  Else we might get two copies of open
  // streams since both parent and child get copies of the buffers.
  fflush(NULL);
  
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
  return 0;
}

// Prints header identifying version of code and home
void printhead(){
  printout(LOG_INFO,"smartd version %d.%d-%d: S.M.A.R.T. Monitoring Daemon\n",
           (int)RELEASE_MAJOR, (int)RELEASE_MINOR, (int)SMARTMONTOOLS_VERSION);
  printout(LOG_INFO,"Home page is %s\n\n",PROJECTHOME);
  return;
}


// prints help info for configuration file directives
void Directives() {
  printout(LOG_INFO,"Configuration file Directives (following device name):\n");
  printout(LOG_INFO,"  -A     Device is an ATA device\n");
  printout(LOG_INFO,"  -S     Device is a SCSI device\n");
  printout(LOG_INFO,"  -C N   Check disks once every N seconds, where N>=10.\n");
  printout(LOG_INFO,"  -P     Permissive, ignore apparent lack of SMART.\n");
  printout(LOG_INFO,"  -T N   Automatic offline tests. N=0: Disable, 1: Enable.\n");
  printout(LOG_INFO,"  -s N   Attribute Autosave. N=0: Disable, 1: Enable.\n");
  printout(LOG_INFO,"  -c     Monitor SMART Health Status, report if failed\n");
  printout(LOG_INFO,"  -l     Monitor SMART Error Log, report new errors\n");
  printout(LOG_INFO,"  -L     Monitor SMART Self-Test Log, report new errors\n");
  printout(LOG_INFO,"  -f     Monitor 'Usage' Attributes, report failures\n");
  printout(LOG_INFO,"  -M ADD Send email warning to address ADD\n");
  printout(LOG_INFO,"  -m N   Modify email warning behavior. -3 <= N <= 3\n");
  printout(LOG_INFO,"  -p     Report changes in 'Prefailure' Attributes\n");
  printout(LOG_INFO,"  -u     Report changes in 'Usage' Attributes\n");
  printout(LOG_INFO,"  -t     Equivalent to -p and -u Directives\n");
  printout(LOG_INFO,"  -a     Equivalent to -c -l -L -f -t Directives\n");
  printout(LOG_INFO,"  -i ID  Ignore Attribute ID for -f Directive\n");
  printout(LOG_INFO,"  -I ID  Ignore Attribute ID for -p, -u or -t Directive\n");
  printout(LOG_INFO,"   #     Comment: text after a hash sign is ignored\n");
  printout(LOG_INFO,"   \\    Line continuation character\n");
  printout(LOG_INFO,"Attribute ID is a decimal integer 1 <= ID <= 255\n");
  printout(LOG_INFO,"SCSI devices: only -S, -C, -M, and -m Directives allowed.\n");
  printout(LOG_INFO,"Example: /dev/hda -a\n");
return;
}

/* prints help information for command syntax */
void Usage (void){
#ifdef HAVE_GETOPT_LONG
  printout(LOG_INFO,"Usage: smartd [-XVh] [--debug] [--version] [--help]\n\n");
  printout(LOG_INFO,"Command Line Options:\n");
  printout(LOG_INFO,"  -X, --debug\n  Start smartd in debug mode\n\n");
  printout(LOG_INFO,"  -V, --version, --license, --copyright\n");
  printout(LOG_INFO,"  Print License, Copyright, and version information\n\n");
  printout(LOG_INFO,"  -h, -?, --help, --usage\n  Display this help and exit\n\n");
#else
  printout(LOG_INFO,"Usage: smartd [-XVh]\n\n");
  printout(LOG_INFO,"Command Line Options:\n");
  printout(LOG_INFO,"  -X     Start smartd in debug mode\n");
  printout(LOG_INFO,"  -V     Print License, Copyright, and version information\n");
  printout(LOG_INFO,"  -h     Display this help and exit\n");
  printout(LOG_INFO,"  -?     Same as -h\n");
#endif
  printout(LOG_INFO,"\n");
  printout(LOG_INFO,"Optional configuration file: %s\n",CONFIGFILE);
  Directives();
}

// returns negative if problem, else fd>=0
int opendevice(char *device){
  int fd = open(device, O_RDONLY);
  if (fd<0) {
    printout(LOG_INFO,"Device: %s, %s, open() failed\n",device, strerror(errno));
    return -1;
  }
  // device opened sucessfully
  return fd;
}

int closedevice(int fd, char *name){
  if (close(fd)){
    printout(LOG_INFO,"Device: %s, %s, close(%d) failed\n", name, strerror(errno), fd);
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
int selftesterrorcount(int fd, char *name){
  struct ata_smart_selftestlog log;

  if (-1==ataReadSelfTestLog(fd,&log)){
    printout(LOG_INFO,"Device: %s, Read SMART Self Test Log Failed\n",name);
    return -1;
  }
  
  // return current number of self-test errors
  return ataPrintSmartSelfTestlog(&log,0);
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
  if (ataReadHDIdentity (fd,&drive)){
    // Unable to read Identity structure
    printout(LOG_INFO,"Device: %s, unable to read Device Identity Structure\n",device);
    close(fd);
    return 2; 
  }
  
  if (!cfg->permissive && !ataSmartSupport(&drive)){
    // SMART not supported
    printout(LOG_INFO,"Device: %s, appears to lack SMART, use '-P' Directive to try anyway.\n",device);
    close(fd);
    return 2; 
  }
  
  if (ataEnableSmart(fd)){
    // Enable SMART command has failed
    printout(LOG_INFO,"Device: %s, could not enable SMART capability\n",device);
    close(fd);
    return 2; 
  }
  
  // disable device attribute autosave...
  if (cfg->autosave==1){
    if (ataDisableAutoSave(fd))
      printout(LOG_INFO,"Device: %s, could not disable SMART Attribute Autosave.\n",device);
    else
      printout(LOG_INFO,"Device: %s, disabled SMART Attribute Autosave.\n",device);
  }

  // or enable device attribute autosave
  if (cfg->autosave==2){
    if (ataEnableAutoSave(fd))
      printout(LOG_INFO,"Device: %s, could not enable SMART Attribute Autosave.\n",device);
    else
      printout(LOG_INFO,"Device: %s, enabled SMART Attribute Autosave.\n",device);
  }

  // capability check: SMART status
  if (cfg->smartcheck && ataSmartStatus2(fd)==-1){
    printout(LOG_INFO,"Device: %s, not capable of SMART Health Status check\n",device);
    cfg->smartcheck=0;
  }
  
  // capability check: Read smart values and thresholds
  if (cfg->usagefailed || cfg->prefail || cfg->usage || cfg->autoofflinetest) {
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

      // make it easy to recognize that we've deallocated
      devices->smartval=NULL;
      devices->smartthres=NULL;
      cfg->usagefailed=cfg->prefail=cfg->usage=0;
    }
  }

  // disable automatic on-line testing
  if (cfg->autoofflinetest==1){
    if (devices->smartval && isSupportAutomaticTimer(devices->smartval) && !ataDisableAutoOffline(fd))
      printout(LOG_INFO,"Device: %s, disabled SMART Automatic Offline Testing .\n",device);
    else
      printout(LOG_INFO,"Device: %s, could not disable SMART Automatic Offline Testing.\n",device);
  }

  // enable automatic on-line testing
  if (cfg->autoofflinetest==2){
    if (devices->smartval && isSupportAutomaticTimer(devices->smartval) && !ataDisableAutoOffline(fd))
      printout(LOG_INFO,"Device: %s, enabled SMART Automatic Offline Testing.\n",device);
    else
      printout(LOG_INFO,"Device: %s, could not enable SMART Automatic Offline Testing.\n",device);
  }

  // capability check: self-test-log
  if (cfg->selftest){
    int val=selftesterrorcount(fd, device);
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
	     "Recompile code from " PROJECTHOME " with larger MAXATADEVICES\n",(int)numatadevices);
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
int scsidevicescan(scsidevices_t *devices, cfgfile *cfg){
  int i, fd, smartsupport;
  char *device=cfg->name;
  unsigned char  tBuf[4096];

  // should we try to register this as a SCSI device?
  if (!(cfg->tryscsi))
    return 1;
  
  // open the device
  if ((fd=opendevice(device))<0)
    // device open failed
    return 1;
  printout(LOG_INFO,"Device: %s, opened\n", device);
  
  // check that it's ready for commands.  Is this really needed?  It's
  // not part of smartctl at all.
  if (testunitnotready(fd)){
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
	     "Recompile code from " PROJECTHOME " with larger MAXSCSIDEVICES\n",(int)numscsidevices);
    exit(1);
  }

  // now we can proceed to register the device
  printout(LOG_INFO, "Device: %s, is SMART capable. Adding to \"monitor\" list.\n",device);

  // since device points to global memory, just keep that address
  devices->devicename=device;
  devices->cfg=cfg;

  // register the supported functionality.  The smartd code does not
  // seem to make any further use of this information.
  if (logsense(fd, SUPPORT_LOG_PAGES, (UINT8 *) &tBuf) == 0){
    for ( i = 4; i < tBuf[3] + LOGPAGEHDRSIZE ; i++){
      switch ( tBuf[i]){ 
      case TEMPERATURE_PAGE:
	devices->TempPageSupported = 1;
	break;
      case SMART_PAGE:
	devices->SmartPageSupported = 1;
	break;
      default:
	break;
      }
    }	
  }

  // increment number of SCSI devices found
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
			    int n, char *name){
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

  // consider only valid attributes
  if (!now->id || !was->id || !thre->id)
    return 0;
  
  
  // issue warning if they don't have the same ID in all structures:
  if ( (now->id != was->id) || (now->id != thre->id) ){
    printout(LOG_INFO,"Device: %s, same Attribute has different ID numbers: %d = %d = %d\n",
	     name, (int)now->id, (int)was->id, (int)thre->id);
    return 0;
  }

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
  
  // If user has asked, test the email warning system
  if (cfg->emailopt<0){
    cfg->emailopt*=-1;
    printandmail(cfg, 0, LOG_CRIT, "TEST EMAIL from smartd for device: %s\n", drive->devicename);
  }

  // if we can't open device, fail gracefully rather than hard --
  // perhaps the next time around we'll be able to open it
  if ((fd=opendevice(name))<0)
    return 1;
  
  // check smart status
  if (cfg->smartcheck){
    int status=ataSmartStatus2(fd);
    if (status==-1)
      printout(LOG_INFO,"Device: %s, not capable of SMART self-check\n",name);
    else if (status==1){
      printout(LOG_CRIT, "Device: %s, FAILED SMART self-check. BACK UP DATA NOW!\n", name);
      printandmail(cfg, 1, LOG_CRIT, "Device: %s, FAILED SMART self-check. BACK UP DATA NOW!\n", name);
    }
  }
  
  // Check everything that depends upon SMART Data (eg, Attribute values)
  if (cfg->usagefailed || cfg->prefail || cfg->usage){
    struct ata_smart_values     curval;
    struct ata_smart_thresholds *thresh=drive->smartthres;
    
    // Read current attribute values. *drive contains old values and thresholds
    if (ataReadSmartValues(fd,&curval))
      printout(LOG_CRIT, "Device: %s, failed to read SMART Attribute Data\n", name);
    else {  
      // look for failed usage attributes, or track usage or prefail attributes
      for (i=0; i<NUMBER_ATA_SMART_ATTRIBUTES; i++){
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
	    printout(LOG_CRIT, "Device: %s, Failed SMART usage Attribute: %s.\n", name, loc);
	    printandmail(cfg, 2, LOG_CRIT, "Device: %s, Failed SMART usage Attribute: %s.\n", name, loc);
	  }
	}
	
	// This block tracks usage or prefailure attributes to see if they are changing
	if ((cfg->usage || cfg->prefail) && ((att=ataCompareSmartValues2(&curval, drive->smartval, thresh, i, name)))){

	  // I should probably clean this up by defining a union to
	  // with one int=four unsigned chars to do this.
	  const int mask=0xff;
	  int newval =(att>>0)  & mask;
	  int oldval =(att>>8)  & mask;
	  int id     =(att>>16) & mask;
	  int prefail=(att>>24) & mask;

	  // for printing attribute name
	  char attname[64],*loc=attname;
	  
	  // are we tracking this attribute?
	  if (!isattoff(id, cfg->trackatt, 0)){
	    
	    // get attribute name, skip spaces
	    ataPrintSmartAttribName(loc, id);
	    while (*loc && *loc==' ') loc++;
	    
	    // prefailure attribute
	    if (cfg->prefail && prefail)
	      printout(LOG_INFO, "Device: %s, SMART Prefailure Attribute: %s changed from %d to %d\n",
		       name, loc, (int)oldval, (int)newval);

	    // usage attribute
	    if (cfg->usage && !prefail)
	      printout(LOG_INFO, "Device: %s, SMART Usage Attribute: %s changed from %d to %d\n",
		       name, loc, (int)oldval, (int)newval);
	  }
	} // endof block tracking usage or prefailure
      } // end of loop over attributes
     
      // Save the new values into *drive for the next time around
      *drive->smartval=curval;
    } 
  }
  
  // check if number of selftest errors has increased (note: may also DECREASE)
  if (cfg->selftest){
    unsigned char old=cfg->selflogcount;
    int new=selftesterrorcount(fd, name);
    if (new>old){
      printout(LOG_CRIT, "Device: %s, Self-Test Log error count increased from %d to %d\n",
	       name, (int)old, new);
      printandmail(cfg, 3, LOG_CRIT, "Device: %s, Self-Test Log error count increased from %d to %d\n",
		   name, (int)old, new);
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
      printout(LOG_CRIT, "Device: %s, ATA error count increased from %d to %d\n",
	       name, old, new);
      printandmail(cfg, 4, LOG_CRIT, "Device: %s, ATA error count increased from %d to %d\n",
		   name, old, new);
    }
    // this last line is probably not needed, count always increases
    if (new>=0)
      cfg->ataerrorcount=new;
  }
  closedevice(fd, name);
  return 0;
}



int scsiCheckDevice(scsidevices_t *drive){
  UINT8 returnvalue;
  UINT8 currenttemp;
  UINT8 triptemp;
  int fd;
  cfgfile *cfg=drive->cfg;

  // If the user has asked for it, test the email warning system
  if (cfg->emailopt<0){
    cfg->emailopt*=-1;
    printandmail(cfg, 0, LOG_CRIT, "TEST EMAIL from smartd for device: %s\n", drive->devicename);
  }

  // if we can't open device, fail gracefully rather than hard --
  // perhaps the next time around we'll be able to open it
  if ((fd=opendevice(drive->devicename))<0)
    return 1;

  currenttemp = triptemp = 0;
  
  if (scsiCheckSmart(fd, drive->SmartPageSupported, &returnvalue, &currenttemp, &triptemp))
    printout(LOG_INFO, "Device: %s, failed to read SMART values\n", drive->devicename);
  
  if (returnvalue) {
    printout(LOG_CRIT, "Device: %s, SMART Failure: (%d) %s\n", drive->devicename, 
	     (int)returnvalue, scsiSmartGetSenseCode(returnvalue));
    printandmail(cfg, 1, LOG_CRIT, "Device: %s, SMART Failure: (%d) %s\n", drive->devicename, 
		 (int)returnvalue, scsiSmartGetSenseCode(returnvalue));
  }
  else if (debugmode)
    printout(LOG_INFO,"Device: %s, Acceptable Attribute: %d\n", drive->devicename, (int)returnvalue);  
  
  // Seems to completely ignore what capabilities were found on the
  // device when scanned
  if (currenttemp){
    if ((currenttemp != drive->Temperature) && (drive->Temperature))
      printout(LOG_INFO, "Device: %s, Temperature changed %d degrees to %d degrees since last reading\n", 
	       drive->devicename, (int) (currenttemp - drive->Temperature), (int)currenttemp );
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

    // Unix Gurus: I know that this can be done better.  Please tell
    // me how -- use email address for Bruce Allen at the top of this
    // file. Search for "sleeptime" to see what I am doing.  I think
    // that when done "right" I should not have to call sleep once per
    // second, but just set an alarm for checktime in the future, and
    // then have an additional alarm sent if the user does SIGUSR1,
    // which arrives first to cause another device check.  Please help
    // me out.
    
    // Sleep until next check. Note that since sleeptime can be set to
    // zero by an EXTERNAL signal SIGUSR1, it's possible for sleeptime
    // to be negative.  Don't use while (sleeptime)!
    sleeptime=checktime;
    while (sleeptime-->0)
      sleep(1); 
  }
}

char copyleftstring[]=
"smartd comes with ABSOLUTELY NO WARRANTY. This\n"
"is free software, and you are welcome to redistribute it\n"
"under the terms of the GNU General Public License Version 2.\n"
"See http://www.gnu.org for further details.\n\n";

cfgfile config[MAXENTRIES];


// exits with an error message, or returns integer value of token
int inttoken(char *arg, char *name, char *token, int lineno, char *configfile, int min, int max){
  char *endptr;
  int val;
  
  // make sure argument is there
  if (!arg) {
    printout(LOG_CRIT,"File %s line %d (drive %s): Directive: %s takes integer argument from %d to %d.\n",
	     configfile, lineno, name, token, min, max);
    Directives();
    exit(1);
  }
  
  // get argument value (base 10), check that it's integer, and in-range
  val=strtol(arg,&endptr,10);
  if (*endptr!='\0' || val<min || val>max )  {
    printout(LOG_CRIT,"File %s line %d (drive %s): Directive: %s has argument: %s; needs integer from %d to %d.\n",
	     configfile, lineno, name, token, arg, min, max);
    Directives();
    exit(1);
  }

  // all is well; return value
  return val;
}

// This function returns non-zero if it has correctly parsed a token,
// else zero if it has failed to parse a token.  Or it exits with a
// directive message if there is a token-parsing problem.
int parsetoken(char *token,cfgfile *cfg){
  char sym;
  char *name=cfg->name;
  int lineno=cfg->lineno;
  char *delim=" \n\t";

  // is the rest of the line a comment
  if (*token=='#')
    return 1;
  
  // is the token not recognized?
  if (*token!='-' || strlen(token)!=2) {
    printout(LOG_CRIT,"File %s line %d (drive %s): unknown Directive: %s\n",
	     CONFIGFILE, lineno, name, token);
    Directives();
    exit(1);
  }
  
  // let's parse the token and swallow its argument
  switch (sym=token[1]) {
    char *arg;
    int val;
    
  case 'P':
    // Permissive mode; ignore errors from Mandatory SMART commands
    cfg->permissive=1;
    break;
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
  case 'T':
    // Automatic offline testing on/off.  Note "1+" below!
    cfg->autoofflinetest=1+inttoken(arg=strtok(NULL,delim), name, token, lineno, CONFIGFILE, 0, 1);
    break;
  case 's':
    // Attribute autosave on/off.  Note "1+" below!
    cfg->autosave=1+inttoken(arg=strtok(NULL,delim), name, token, lineno, CONFIGFILE, 0, 1);
    break;
  case 'm':
    // email warning option
    cfg->emailopt=inttoken(arg=strtok(NULL,delim), name, token, lineno, CONFIGFILE, -3, 3);
    break;
  case 'i':
    // ignore failure of usage attribute
    val=inttoken(arg=strtok(NULL,delim), name, token, lineno, CONFIGFILE, 1, 255);
    isattoff(val,cfg->failatt,1);
    break;
  case 'I':
    // ignore attribute for tracking purposes
    val=inttoken(arg=strtok(NULL,delim), name, token, lineno, CONFIGFILE, 1, 255);
    isattoff(val,cfg->trackatt,1);
    break;
  case 'C': 
    // period (time interval) for checking
    checktime=inttoken(arg=strtok(NULL,delim), name, token, lineno, CONFIGFILE, 10, INT_MAX);
    break;
  case 'M':
    // send email to address that follows
    arg=strtok(NULL,delim);
    if (!arg) {
      printout(LOG_CRIT,"File %s line %d (drive %s): Directive: %s needs email address(es)\n",
	       CONFIGFILE, lineno, name, token);
      Directives();
      exit(1);
    }
    if (!(cfg->address=strdup(arg))){
      printout(LOG_CRIT,"File %s line %d (drive %s): Directive: %s: no free memory for email address(es) %s\n",
	       CONFIGFILE, lineno, name, token, arg);
      Directives();
      exit(1);
    }
    break;
  default:
    printout(LOG_CRIT,"File %s line %d (drive %s): unknown Directive: %s\n",
	     CONFIGFILE, lineno, name, token);
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
  static int numtokens=0;

  if (!(copy=strdup(line))){
    printout(LOG_INFO,"No memory to parse file: %s line %d, %s\n", CONFIGFILE, lineno, strerror(errno));
    exit(1);
  }
  
  // get first token -- device name
  if (!(name=strtok(copy,delim)) || *name=='#'){
    free(copy);
    return 0;
  }

  // Have we detected the DEVICESCAN directive?
  if (!strcmp(SCANDIRECTIVE,name)){
    if (numtokens) {
      printout(LOG_INFO,"Scan Directive %s must be the first entry in %s\n",name,CONFIGFILE);
      exit(1);
    }
    else
      printout(LOG_INFO,"Scan Directive %s found in %s. Scanning for devices.\n",name,CONFIGFILE);
    free(copy);
    return -1;
  }
  numtokens++;

  // Is there space for another entry?
  if (entry>=MAXENTRIES){
    printout(LOG_CRIT,"Error: configuration file %s can have no more than MAXENTRIES=%d entries\n",
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
    printout(LOG_INFO,"No memory to store file: %s line %d, %s\n", CONFIGFILE, lineno, strerror(errno));
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

  // basic sanity check -- are any directives enabled?
  if (!(cfg->smartcheck || cfg->usagefailed || cfg->prefail || cfg->usage || 
	cfg->selftest || cfg->errorlog || cfg->tryscsi)){
    printout(LOG_CRIT,"Drive: %s, no monitoring Directives on line %d of file %s\n",
	     cfg->name, cfg->lineno, CONFIGFILE);
    Directives();
    exit(1);
  }

  // additional sanity check. Has user set -m without -M?
  if (cfg->emailopt && !cfg->address){
    printout(LOG_CRIT,"Drive: %s, Directive -m useless without address Directive -M on line %d of file %s\n",
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
    int len=0,scandevice;
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
	scandevice=parseconfigline(entry,lineno,fullline);
	// See if we found a SCANDEVICE directive
	if (scandevice<0)
	  return -1;
	// the final line is part of a continuation line
	cont=0;
	entry+=scandevice;
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
	       (int)contlineno,CONFIGFILE,warn,(int)MAXLINELEN);
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
	       lineno, (int)contlineno, CONFIGFILE, (int)MAXCONTLINE);
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
    scandevice=parseconfigline(entry,lineno,fullline);

    // did we find a scandevice directive?
    if (scandevice<0)
      return -1;

    entry+=scandevice;
    lineno++;
    cont=0;
  }
  fclose(fp);
  if (entry)
    return entry;
  
  printout(LOG_CRIT,"Configuration file %s contains no devices (like /dev/hda)\n",CONFIGFILE);
  exit(1);
}

// Prints copyright, license and version information
void PrintCopyleft(void){
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
}

// Parses input line, prints usage message and
// version/license/copyright messages
void ParseOpts(int argc, char **argv){
  extern char *optarg;
  extern int  optopt, optind, opterr;
  int optchar;
#ifdef HAVE_GETOPT_LONG
  struct option long_options[] = {
    { "debug",     no_argument, 0, 'X'},
    { "version",   no_argument, 0, 'V'},
    { "license",   no_argument, 0, 'V'},
    { "copyright", no_argument, 0, 'V'},
    { "help",      no_argument, 0, 'h'},
    { "usage",     no_argument, 0, 'h'},
    { 0,           0,           0, 0  }
  };
#endif

  opterr=optopt=0;

  // Parse input options:
#ifdef HAVE_GETOPT_LONG
  while (-1 != (optchar = getopt_long(argc, argv, "XVh?", long_options, NULL))){
#else
  while (-1 != (optchar = getopt(argc, argv, "XVh?"))){
#endif
    switch(optchar) {
    case 'X':
      debugmode  = TRUE;
      break;
    case 'V':
      PrintCopyleft();
      exit(0);
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
#ifdef HAVE_GETOPT_LONG
      } else if (optchar == '?' && argv[optind-1][1] == '-') {
	printhead();
	printout(LOG_CRIT,"=======> UNRECOGNIZED OPTION: %s <======= \n\n",argv[optind-1]+2);
	Usage();
	exit(-1);
#endif
      }
      printhead();
      Usage();
      exit(0);
    }
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
    printout(LOG_CRIT,"Error: simulated config file can have no more than %d entries\n",(int)MAXENTRIES);
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
	printout(LOG_INFO,"No memory for %d'th device after %s, %s\n", i, name, strerror(errno));
      exit(1);
    }

    // increment final character of the name
    cfg->name[strlen(name)-1]+=i;
  }
  return i;
}


void cantregister(char *name, char *type, int line){
  if (line)
    printout(LOG_CRIT,"Unable to register %s device %s at line %d of file %s\n",
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
  
  // Parse input and print header and usage info if needed
  ParseOpts(argc,argv);
  
  // Do we mute printing from ataprint commands?
  con->quietmode=0;
  con->veryquietmode=debugmode?0:1;
  con->checksumfail=0;

  // look in configuration file CONFIGFILE (normally /etc/smartd.conf)
  entries=parseconfigfile();

  // If in background as a daemon, fork and close file descriptors
  if (!debugmode)
    daemon_init();
  
  // setup signal handler for shutdown
  if (signal(SIGINT, sighandler)==SIG_IGN)
    signal(SIGINT, SIG_IGN);
  if (signal(SIGTERM, sighandler)==SIG_IGN)
    signal(SIGTERM, SIG_IGN);
  if (signal(SIGQUIT, sighandler)==SIG_IGN)
    signal(SIGQUIT, SIG_IGN);
  if (signal(SIGHUP, huphandler)==SIG_IGN)
    signal(SIGHUP, SIG_IGN);
  if (signal(SIGUSR1, sleephandler)==SIG_IGN)
    signal(SIGUSR1, SIG_IGN);
  
  // install goobye message
  atexit(goobye);
  
  // if there was no config file, create needed entries
  if (entries<=0){
    if (entries)
      printout(LOG_INFO,"smartd: Scanning for devices.\n");
    else
      printout(LOG_INFO,"smartd: file %s not found. Searching for devices.\n",CONFIGFILE);
    entries=0;
    entries+=makeconfigentries(MAXATADEVICES,"/dev/hda",1,entries);
    entries+=makeconfigentries(MAXSCSIDEVICES,"/dev/sda",0,entries);
  }
  

  // Register entries
  for (i=0;i<entries;i++){
    // register ATA devices
    if (config[i].tryata && atadevicescan2(atadevicesptr+numatadevices, config+i))
      cantregister(config[i].name, "ATA", config[i].lineno);
    
    // then register SCSI devices
    if (config[i].tryscsi && scsidevicescan(scsidevicesptr+numscsidevices, config+i))
      cantregister(config[i].name, "SCSI", config[i].lineno);
  }
  
  
  // Now start an infinite loop that checks all devices
  CheckDevices(atadevicesptr, scsidevicesptr); 
  return 0;
}

