/*
 * atacmds.c
 * 
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-3 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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
#include <errno.h>
#include <stdlib.h>
#include <ctype.h>
#include "atacmds.h"
#include "extern.h"
#include "utility.h"

const char *atacmds_c_cvsid="$Id: atacmds.cpp,v 1.123 2003/10/08 01:24:58 ballen4705 Exp $" ATACMDS_H_CVSID EXTERN_H_CVSID UTILITY_H_CVSID;

// for passing global control variables
extern smartmonctrl *con;

// These Drive Identity tables are taken from hdparm 5.2, and are also
// given in the ATA/ATAPI specs for the IDENTIFY DEVICE command.  Note
// that SMART was first added into the ATA/ATAPI-3 Standard with
// Revision 3 of the document, July 25, 1995.  Look at the "Document
// Status" revision commands at the beginning of
// http://www.t13.org/project/d2008r6.pdf to see this.
#define NOVAL_0			0x0000
#define NOVAL_1			0xffff
/* word 81: minor version number */
#define MINOR_MAX 0x1e
const char *minor_str[] = {			/* word 81 value: */
  "Device does not report version",		/* 0x0000	*/
  "ATA-1 X3T9.2 781D prior to revision 4",	/* 0x0001	*/
  "ATA-1 published, ANSI X3.221-1994",		/* 0x0002	*/
  "ATA-1 X3T9.2 781D revision 4",		/* 0x0003	*/
  "ATA-2 published, ANSI X3.279-1996",		/* 0x0004	*/
  "ATA-2 X3T10 948D prior to revision 2k",	/* 0x0005	*/
  "ATA-3 X3T10 2008D revision 1",		/* 0x0006	*/ /* SMART NOT INCLUDED */
  "ATA-2 X3T10 948D revision 2k",		/* 0x0007	*/
  "ATA-3 X3T10 2008D revision 0",		/* 0x0008	*/ 
  "ATA-2 X3T10 948D revision 3",		/* 0x0009	*/
  "ATA-3 published, ANSI X3.298-199x",		/* 0x000a	*/
  "ATA-3 X3T10 2008D revision 6",		/* 0x000b	*/ /* 1st VERSION WITH SMART */
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
  "ATA/ATAPI-7 T13 1532D revision 1",		/* 0x001a	*/
  "ATA/ATAPI-6 T13 1410D revision 2",		/* 0x001b	*/
  "ATA/ATAPI-6 T13 1410D revision 1",		/* 0x001c	*/
  "reserved",					/* 0x001d	*/
  "ATA/ATAPI-7 T13 1532D revision 0"		/* 0x001e	*/
};

// NOTE ATA/ATAPI-4 REV 4 was the LAST revision where the device
// attribute structures were NOT completely vendor specific.  So any
// disk that is ATA/ATAPI-4 or above can not be trusted to show the
// vendor values in sensible format.

// Negative values below are because it doesn't support SMART
const int actual_ver[] = { 
  /* word 81 value: */
  0,		/* 0x0000	WARNING: 	*/
  1,		/* 0x0001	WARNING: 	*/
  1,		/* 0x0002	WARNING: 	*/
  1,		/* 0x0003	WARNING: 	*/
  2,		/* 0x0004	WARNING:   This array 		*/
  2,		/* 0x0005	WARNING:   corresponds 		*/
  -3, /*<== */	/* 0x0006	WARNING:   *exactly*		*/
  2,		/* 0x0007	WARNING:   to the ATA/		*/
  -3, /*<== */	/* 0x0008	WARNING:   ATAPI version	*/
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
  7,		/* 0x001a	WARNING:	*/
  6,		/* 0x001b	WARNING:	*/
  6,		/* 0x001c	WARNING:	*/
  0,		/* 0x001d	WARNING:	*/
  7		/* 0x001e	WARNING:	*/
};

// When you add additional items to this list, you should then:
// 0 -- update this list
// 1 -- modify the following function parse_attribute_def()
// 2 -- if needed, modify ataPrintSmartAttribRawValue()
// 3 -  if needed, modify ataPrintSmartAttribName()
// 4 -- add #define PRESET_N_DESCRIPTION at top of knowndrives.c
// 5 -- add drive in question into knowndrives[] table in knowndrives.c
// 6 -- update smartctl.8
// 7 -- update smartd.8
// 8 -- do "make smartd.conf.5" to update smartd.conf.5
// 9 -- update CHANGELOG file
const char *vendorattributeargs[] = {
  // 0  defs[9]=1
  "9,minutes",
  // 1  defs[9]=3
  "9,seconds",
  // 2  defs[9]=2
  "9,temp",
  // 3  defs[220]=1
  "220,temp",
  // 4  defs[*]=253
  "N,raw8",
  // 5  defs[*]=254
  "N,raw16",
  // 6  defs[*]=255
  "N,raw48",
  // 7  defs[200]=1
  "200,writeerrorcount",
  // 8  defs[9]=4
  "9,halfminutes",
  // 9  defs[194]=1
  "194,10xCelsius",
  // 10 defs[194]=2
  "194,unknown",
  // 11 defs[193]=1
  "193,loadunload",
  // 12 defs[201]=1
  "201,detectedtacount",
  // 13 defs[192]=1
  "192,emergencyretractcyclect",
  // 14 defs[198]=1
  "198,offlinescanuncsectorct",
  // NULL should always terminate the array
  NULL
};

// This is a utility function for parsing pairs like "9,minutes" or
// "220,temp", and putting the correct flag into the attributedefs
// array.  Returns 1 if problem, 0 if pair has been recongized.
int parse_attribute_def(char *pair, unsigned char **defsptr){
  int i,j;
  char temp[32];
  unsigned char *defs;

  // If array does not exist, allocate it
  if (!*defsptr && !(*defsptr=(unsigned char *)calloc(MAX_ATTRIBUTE_NUM, 1))){
    pout("Out of memory in parse_attribute_def\n");
    exit(1);
  }

  defs=*defsptr;

  // look along list and see if we find the pair
  for (i=0; vendorattributeargs[i] && strcmp(pair, vendorattributeargs[i]); i++);

  switch (i) {
  case 0:
    // attribute 9 is power on time in minutes
    defs[9]=1;
    return 0;
  case 1:
    // attribute 9 is power-on-time in seconds
    defs[9]=3;
    return 0;
  case 2:
    // attribute 9 is temperature in celsius
    defs[9]=2;
    return 0;
  case 3:
    // attribute 220 is temperature in celsius
    defs[220]=1;
    return 0;
  case 4:
    // print all attributes in raw 8-bit form
    for (j=0; j<MAX_ATTRIBUTE_NUM; j++)
      defs[j]=253;
    return 0;
  case 5:
    // print all attributes in raw 16-bit form
    for (j=0; j<MAX_ATTRIBUTE_NUM; j++)
      defs[j]=254;
    return 0;
  case 6:
    // print all attributes in raw 48-bit form
    for (j=0; j<MAX_ATTRIBUTE_NUM; j++)
      defs[j]=255;
    return 0;
  case 7:
    // attribute 200 is write error count
    defs[200]=1;
    return 0;
  case 8:
    // attribute 9 increments once every 30 seconds (power on time
    // measure)
    defs[9]=4;
    return 0;
  case 9:
    // attribute 194 is ten times disk temp in Celsius
    defs[194]=1;
    return 0;
  case 10:
    // attribute 194 is unknown
    defs[194]=2;
    return 0;
  case 11:
    // Hitachi : Attributes 193 has 2 values : 1 load, 1 normal unload
    defs[193]=1;
    return 0;
  case 12:
    // Fujitsu
    defs[201]=1;
    return 0;
  case 13:
    // Fujitsu
    defs[192]=1;
    return 0;
  case 14:
    // Fujitsu
    defs[198]=1;
    return 0;
  default:
    // pair not found
    break;
  }
  // At this point, either the pair was not found, or it is of the
  // form N,uninterpreted, in which case we need to parse N
  j=sscanf(pair,"%d,%14s", &i, temp);
 
  // if no match to pattern, unrecognized
  if (j!=2 || i<0 || i >255)
    return 1;

  // check for recognized strings
  if (!strcmp(temp, "raw8")) {
    defs[i]=253;
    return 0;
  }
  
  // check for recognized strings
  if (!strcmp(temp, "raw16")) {
    defs[i]=254;
    return 0;
  }
  
  // check for recognized strings
  if (!strcmp(temp, "raw48")) {
    defs[i]=255;
    return 0;
  }
 
  // didn't recognize the string
  return 1;
}

// Structure used in sorting the array vendorattributeargs[].
typedef struct vaa_pair_s {
  const char *vaa;
  const char *padded_vaa;
} vaa_pair;

// Returns a copy of s with all numbers of less than three digits padded with
// leading zeros.  Returns NULL if there isn't enough memory available.  The
// memory for the string is dynamically allocated and should be freed by the
// caller.
char *pad_numbers(const char *s)
{
  char c, *t, *u;
  const char *r;
  int i, len, ndigits = 0;

  // Allocate the maximum possible amount of memory needed.
  if (!(t = (char *)malloc(strlen(s)*2+2)))
    return NULL;

  // Copy the string s to t, padding any numbers of less than three digits
  // with leading zeros.  The string is copied backwards to simplify the code.
  r = s + strlen(s);
  u = t;
  while (( r-- >= s)) {
    if (isdigit(*r))
      ndigits++;
    else if (ndigits > 0) {
      while (ndigits++ < 3)
        *u++ = '0';
      ndigits = 0;
    }
    *u++ = *r;
  }
  *u = '\0';

  // Reverse the string in t.
  len = strlen(t);
  for (i = 0; i < len/2; i++) {
    c          = t[i];
    t[i]       = t[len-1-i];
    t[len-1-i] = c;
  }

  return t;
}

// Comparison function for qsort().  Used by sort_vendorattributeargs().
int compare_vaa_pairs(const void *a, const void *b)
{
  vaa_pair *p = (vaa_pair *)a;
  vaa_pair *q = (vaa_pair *)b;

  return strcmp(p->padded_vaa, q->padded_vaa);
}

// Returns a sorted list of vendorattributeargs or NULL if there is not enough
// memory available.  The memory for the list is allocated dynamically and
// should be freed by the caller.
// To perform the sort, any numbers in the strings are padded out to three
// digits by adding leading zeros.  For example,
//
//    "9,minutes"  becomes  "009,minutes"
//    "N,raw16"    becomes  "N,raw016"
//
// and the original strings are paired with the padded strings.  The list of
// pairs is then sorted by comparing the padded strings (using strcmp) and the
// result is then the list of unpadded strings.
//
const char **sort_vendorattributeargs(void) {
  const char **ps, **sorted_list = NULL;
  vaa_pair *pairs, *pp;
  int count, i;

  // Figure out how many strings are in vendorattributeargs[] (not including
  // the terminating NULL).
  count = (sizeof vendorattributeargs) / sizeof(char *) - 1;

  // Construct a list of pairs of strings from vendorattributeargs[] and their
  // padded equivalents.
  if (!(pairs = (vaa_pair *)malloc(sizeof(vaa_pair) * count)))
    goto END;
  for (ps = vendorattributeargs, pp = pairs; *ps; ps++, pp++) {
    pp->vaa = *ps;
    if (!(pp->padded_vaa = pad_numbers(*ps)))
      goto END;
  }

  // Sort the array of vaa_pair structures by comparing the padded strings
  // using strcmp().
  qsort(pairs, count, sizeof(vaa_pair), compare_vaa_pairs);

  // Construct the sorted list of strings.
  if (!(sorted_list = (const char **)malloc(sizeof vendorattributeargs)))
    goto END;
  for (ps = sorted_list, pp = pairs, i = 0; i < count; ps++, pp++, i++)
    *ps = pp->vaa;
  *ps = NULL;

END:
  if (pairs) {
    for (i = 0; i < count; i++)
      if (pairs[i].padded_vaa)
        free((void *)pairs[i].padded_vaa);
    free((void *)pairs);
  }

  // If there was a problem creating the list then sorted_list should now
  // contain NULL.
  return sorted_list;
}

// Function to return a multiline string containing a list of the arguments in 
// vendorattributeargs[].  The strings are preceeded by tabs and followed
// (except for the last) by newlines.
// This function allocates the required memory for the string and the caller
// must use free() to free it.  It returns NULL if the required memory can't
// be allocated.
char *create_vendor_attribute_arg_list(void){
  const char **ps, **sorted;
  char *s;
  int len;

  // Get a sorted list of vendor attribute arguments.  If the sort fails
  // (which should only happen if the system is really low on memory) then just
  // use the unordered list.
  if (!(sorted = (const char **) sort_vendorattributeargs()))
    sorted = vendorattributeargs;

  // Calculate the required number of characters
  len = 1;                // At least one char ('\0')
  for (ps = sorted; *ps != NULL; ps++) {
    len += 1;             // For the tab
    len += strlen(*ps);   // For the actual argument string
    if (*(ps+1))
      len++;              // For the newline if required
  }

  // Attempt to allocate memory for the string
  if (!(s = (char *)malloc(len)))
    return NULL;

  // Construct the string
  *s = '\0';
  for (ps = sorted; *ps != NULL; ps++) {
    strcat(s, "\t");
    strcat(s, *ps);
    if (*(ps+1))
      strcat(s, "\n");
  }

  free(sorted);

  // Return a pointer to the string
  return s;
}

// swap two bytes.  Point to low address
void swap2(char *location){
  char tmp=*location;
  *location=*(location+1);
  *(location+1)=tmp;
  return;
}

// swap four bytes.  Point to low address
void swap4(char *location){
  char tmp=*location;
  *location=*(location+3);
  *(location+3)=tmp;
  swap2(location+1);
  return;
}

// swap eight bytes.  Points to low address
void swap8(char *location){
  char tmp=*location;
  *location=*(location+7);
  *(location+7)=tmp;
  tmp=*(location+1);
  *(location+1)=*(location+6);
  *(location+6)=tmp;
  swap4(location+2);
  return;
}

static char *commandstrings[]={
  [ENABLE]=           "SMART ENABLE",
  [DISABLE]=          "SMART DISABLE",
  [AUTOSAVE]=         "SMART AUTOMATIC ATTRIBUTE SAVE",
  [IMMEDIATE_OFFLINE]="SMART IMMEDIATE OFFLINE",
  [AUTO_OFFLINE]=     "SMART AUTO OFFLINE",
  [STATUS]=           "SMART STATUS",
  [STATUS_CHECK]=     "SMART STATUS CHECK",
  [READ_VALUES]=      "SMART READ ATTRIBUTE VALUES",
  [READ_THRESHOLDS]=  "SMART READ ATTRIBUTE THRESHOLDS",
  [READ_LOG]=         "SMART READ LOG",
  [IDENTIFY]=         "IDENTIFY DEVICE",
  [PIDENTIFY]=        "IDENTIFY PACKET DEVICE"
};

void prettyprint(unsigned char *stuff, char *name){
  int i,j;
  pout("\n===== [%s] DATA START (BASE-16) =====\n", name);
  for (i=0; i<32; i++){
    pout("%03d-%03d: ", 16*i, 16*(i+1)-1);
    for (j=0; j<15; j++)
      pout("%02x ",*stuff++);
    pout("%02x\n",*stuff++);
  }
  pout("===== [%s] DATA END (512 Bytes) =====\n\n", name);
}

// This function provides the pretty-print reporting for SMART
// commands: it implements the various -r "reporting" options for ATA
// ioctls.
int smartcommandhandler(int device, smart_command_set command, int select, char *data){
  int retval;

  // This conditional is true for commands that return data
  int getsdata=(command==PIDENTIFY || 
	        command==IDENTIFY || 
	        command==READ_LOG || 
	        command==READ_THRESHOLDS || 
	        command==READ_VALUES);
  
  // If reporting is enabled, say what the command will be before it's executed
  if (con->reportataioctl){
	  // conditional is true for commands that use parameters
	  int usesparam=(command==READ_LOG || 
			 command==AUTO_OFFLINE || 
			 command==AUTOSAVE || 
			 command==IMMEDIATE_OFFLINE);
		  
    pout("\nREPORT-IOCTL: DeviceFD=%d Command=%s", device, commandstrings[command]);
    if (usesparam)
      pout(" InputParameter=%d\n", select);
    else
      pout("\n");
  }
  
  if (getsdata && !data){
    pout("REPORT-IOCTL: Unable to execute command %s : data destination address is NULL\n", commandstrings[command]);
    return -1;
  }
  
  // In case the command produces an error, we'll want to know what it is:
  errno=0;
  
  // now execute the command
  if (con->escalade)
    retval=escalade_command_interface(device, con->escalade-1, command, select, data);
  else
    retval=ata_command_interface(device, command, select, data);
  
  // If reporting is enabled, say what output was produced by the command
  if (con->reportataioctl){
    if (errno)
      pout("REPORT-IOCTL: DeviceFD=%d Command=%s returned %d errno=%d [%s]\n", 
	   device, commandstrings[command], retval, errno, strerror(errno));
    else
      pout("REPORT-IOCTL: DeviceFD=%d Command=%s returned %d\n",
	   device, commandstrings[command], retval);
    
    // if requested, pretty-print the output data structure
    if (con->reportataioctl>1 && getsdata)
      prettyprint((unsigned char *)data, commandstrings[command]);
  }
  return retval;
}


// This function computes the checksum of a single disk sector (512
// bytes).  Returns zero if checksum is OK, nonzero if the checksum is
// incorrect.  The size (512) is correct for all SMART structures.
unsigned char checksum(unsigned char *buffer){
  unsigned char sum=0;
  int i;
  
  for (i=0; i<512; i++)
    sum+=buffer[i];

  return sum;
}

// Reads current Device Identity info (512 bytes) into buf.  Returns 0
// if all OK.  Returns -1 if no ATA Device identity can be
// established.  Returns 1 if Device is ATA Packet Device (not SMART
// capable).
int ataReadHDIdentity (int device, struct ata_identify_device *buf){
  unsigned short *rawstructure=(unsigned short *)buf;
  unsigned char *rawbyte=(unsigned char *)buf;
  int retval=0;

  if ((retval=smartcommandhandler(device, IDENTIFY, 0, (char *)buf))){
    // See if device responds to packet command...
    if (smartcommandhandler(device, PIDENTIFY, 0, (char *)buf)){
      syserror("Error ATA GET HD Identity Failed");
      return -1; 
    }
  }

  if (isbigendian()){
    unsigned short *alias=(unsigned short*)buf;
    int i;
    
    // swap various capability words that are needed
    swap2((char *)(alias+255)); 
    for (i=80; i<=87; i++)
      swap2((char *)(alias+i));
  }
  
  // If there is a checksum there, validate it
  if ((rawstructure[255] & 0x00ff) == 0x00a5 && checksum((unsigned char *)buf))
    checksumwarning("Drive Identity Structure");
  
  if (retval) {
    // This is a packet device.  Determine device type using bits
    // 14-15 and 8-12 of response
    return 1+(rawbyte[1] & 0x1f);
  }
  
  // IDENTIFY PACKET DEVICE was only introduced in ATA-4 revision
  // 2.  Before that, a packet device responds to IDENTIFY DEVICE.
  if (rawbyte[1] & (0x01<<7))
    // early ATAPI device -- let's guess a CDROM
    return 6;
  
  return 0;
}

// Returns ATA version as an integer, and a pointer to a string
// describing which revision.  Note that Revision 0 of ATA-3 does NOT
// support SMART.  For this one case we return -3 rather than +3 as
// the version number.  See notes above.
int ataVersionInfo (const char** description, struct ata_identify_device *drive, unsigned short *minor){
  unsigned short major;
  int i;

  // check that arrays at the top of this file are defined
  // consistently
  if (sizeof(minor_str) != sizeof(char *)*(1+MINOR_MAX)){
    pout("Internal error in ataVersionInfo().  minor_str[] size %d\n"
	 "is not consistent with value of MINOR_MAX+1 = %d\n", 
	 (int)(sizeof(minor_str)/sizeof(char *)), MINOR_MAX+1);
    fflush(NULL);
    abort();
  }
  if (sizeof(actual_ver) != sizeof(int)*(1+MINOR_MAX)){
    pout("Internal error in ataVersionInfo().  actual_ver[] size %d\n"
	 "is not consistent with value of MINOR_MAX = %d\n",
	 (int)(sizeof(actual_ver)/sizeof(int)), MINOR_MAX+1);
    fflush(NULL);
    abort();
  }

  // get major and minor ATA revision numbers
  major=drive->major_rev_num;
  *minor=drive->minor_rev_num;
  
  // First check if device has ANY ATA version information in it
  if (major==NOVAL_0 || major==NOVAL_1) {
    *description=NULL;
    return -1;
  }
  
  // The minor revision number has more information - try there first
  if (*minor && (*minor<=MINOR_MAX)){
    int std = actual_ver[*minor];
    if (std) {
      *description=minor_str[*minor];
      return std;
    }
  }
  
  // HDPARM has a very complicated algorithm from here on. Since SMART only
  // exists on ATA-3 and later standards, let's punt on this.  If you don't
  // like it, please fix it.  The code's in CVS.
  for (i=15; i>0; i--)
    if (major & (0x1<<i))
      break;
  
  *description=NULL; 
  if (i==0)
    return 1;
  else
    return i;;
}

// returns 1 if SMART supported, 0 if SMART unsupported, -1 if can't tell
int ataSmartSupport(struct ata_identify_device *drive){
  unsigned short word82=drive->command_set_1;
  unsigned short word83=drive->command_set_2;
  
  // check if words 82/83 contain valid info
  if ((word83>>14) == 0x01)
    // return value of SMART support bit 
    return word82 & 0x0001;
  
  // since we can're rely on word 82, we don't know if SMART supported
  return -1;
}

// returns 1 if SMART enabled, 0 if SMART disabled, -1 if can't tell
int ataIsSmartEnabled(struct ata_identify_device *drive){
  unsigned short word85=drive->cfs_enable_1;
  unsigned short word87=drive->csf_default;
  
  // check if words 85/86/87 contain valid info
  if ((word87>>14) == 0x01)
    // return value of SMART enabled bit
    return word85 & 0x0001;
  
  // Since we can't rely word85, we don't know if SMART is enabled.
  return -1;
}


// Reads SMART attributes into *data
int ataReadSmartValues(int device, struct ata_smart_values *data){	
  
  if (smartcommandhandler(device, READ_VALUES, 0, (char *)data)){
    syserror("Error SMART Values Read failed");
    return -1;
  }

  // compute checksum
  if (checksum((unsigned char *)data))
    checksumwarning("SMART Attribute Data Structure");
  
  // byte swap if needed
  if (isbigendian()){
    int i;
    swap2((char *)&(data->revnumber));
    swap2((char *)&(data->total_time_to_complete_off_line));
    swap2((char *)&(data->smart_capability));
    for (i=0; i<NUMBER_ATA_SMART_ATTRIBUTES; i++){
      struct ata_smart_attribute *x=data->vendor_attributes+i;
      swap2((char *)&(x->flags));
    }
  }

  return 0;
}


// This corrects some quantities that are byte reversed in the SMART
// SELF TEST LOG
void fixsamsungselftestlog(struct ata_smart_selftestlog *data){
  int i;
  
  // swap with one byte of reserved
  swap2((char *)&(data->mostrecenttest));

  // LBA low register (here called 'selftestnumber") is byte swapped
  // with Self-test execution status byte.
  for (i=0; i<21; i++)
    swap2((char *)&(data->selftest_struct[i].selftestnumber));

  return;
}

// Reads the Self Test Log (log #6)
int ataReadSelfTestLog (int device, struct ata_smart_selftestlog *data){	
  
  // get data from device
  if (smartcommandhandler(device, READ_LOG, 0x06, (char *)data)){
    syserror("Error SMART Error Self-Test Log Read failed");
    return -1;
  }

  // compute its checksum, and issue a warning if needed
  if (checksum((unsigned char *)data))
    checksumwarning("SMART Self-Test Log Structure");
  
  // fix firmware bugs in self-test log
  if (con->fixfirmwarebug == FIX_SAMSUNG)
    fixsamsungselftestlog(data);

  // fix endian order, if needed
  if (isbigendian()){
    int i;
    swap2((char*)&(data->revnumber));
    for (i=0; i<21; i++){
      struct ata_smart_selftestlog_struct *x=data->selftest_struct+i;
      swap2((char *)&(x->timestamp));
      swap4((char *)&(x->lbafirstfailure));
    }
  }

  return 0;
}


// Reads the Log Directory (log #0).  Note: NO CHECKSUM!!
int ataReadLogDirectory (int device, struct ata_smart_log_directory *data){	
  
  // get data from device
  if (smartcommandhandler(device, READ_LOG, 0x00, (char *)data)){
    return -1;
  }

  // swap endian order if needed
  if (isbigendian()){
    swap2((char *)&(data->logversion));
  }
  
  return 0;
}


// Reads the selective self-test log (log #9)
int ataReadSelectiveSelfTestLog(int device, struct ata_selective_self_test_log *data){	
  
  // get data from device
  if (smartcommandhandler(device, READ_LOG, 0x09, (char *)data)){
    return -1;
  }

  // swap endian order if needed
  if (isbigendian()){
    int i;
    swap2((char *)&(data->logversion));
    for (i=0;i<5;i++){
      swap8((char *)&(data->span[i].start));
      swap8((char *)&(data->span[i].end));
    }
    swap8((char *)&(data->currentlba));
    swap2((char *)&(data->currentspan));
    swap2((char *)&(data->flags));
    swap2((char *)&(data->pendingtime));
  }
  return 0;
}

#if DEVELOP_SELECTIVE_SELF_TEST
// Writes the selective self-test log (log #9)
int ataWriteSelectiveSelfTestLog(int device){	
  int i;
  struct ata_selective_self_test_log sstlog, *data=&sstlog;
  unsigned char cksum=0;
  unsigned char *ptr=(unsigned char *)data;

  // Read log
  ataReadSelectiveSelfTestLog(device, data);

  // Clear spans
  for (i=0; i<5; i++)
    memset(data->span+i, 0, sizeof(struct test_span));

  // Set spans for testing 
  for (i=0; i<con->smartselectivenumspans; i++){
    data->span[i].start = con->smartselectivespan[i][0];
    data->span[i].end   = con->smartselectivespan[i][1];
  }

  // Do NOT perform off-line scan after selective 
  data->flags=0;
  data->undefined=data->checksum=0;

  // Put in correct checksum
  for (i=0; i<512; i++)
    cksum+=ptr[i];
  cksum=~cksum;
  cksum+=1;
  data->checksum=cksum;

    // swap endian order if needed
  if (isbigendian()){
    int i;
    swap2((char *)&(data->logversion));
    for (i=0;i<5;i++){
      swap8((char *)&(data->span[i].start));
      swap8((char *)&(data->span[i].end));
    }
    swap8((char *)&(data->currentlba));
    swap2((char *)&(data->currentspan));
    swap2((char *)&(data->flags));
    swap2((char *)&(data->pendingtime));
  }

  // send data to device -- ioctl NOT implemented yet!
  if (smartcommandhandler(device, WRITE_LOG, 0x09, (char *)data)){
    return -1;
  }

  return 0;
}
#endif


// This corrects some quantities that are byte reversed in the SMART
// ATA ERROR LOG
void fixsamsungerrorlog(struct ata_smart_errorlog *data){
  int i,j;
  
  // Device error count in bytes 452-3
  swap2((char *)&(data->ata_error_count));
  
  // step through 5 error log data structures
  for (i=0; i<5; i++){
    // step through 5 command data structures
    for (j=0; j<5; j++)
      // Command data structure 4-byte millisec timestamp
      swap4((char *)&(data->errorlog_struct[i].commands[j].timestamp));
    // Error data structure life timestamp
    swap2((char *)&(data->errorlog_struct[i].error_struct.timestamp));
  }
  return;
}

void fixsamsungerrorlog2(struct ata_smart_errorlog *data){
  // Device error count in bytes 452-3
  swap2((char *)&(data->ata_error_count));
  return;
}

// Reads the Summary SMART Error Log (log #1). The Comprehensive SMART
// Error Log is #2, and the Extended Comprehensive SMART Error log is
// #3
int ataReadErrorLog (int device, struct ata_smart_errorlog *data){	
  
  // get data from device
  if (smartcommandhandler(device, READ_LOG, 0x01, (char *)data)){
    syserror("Error SMART Error Log Read failed");
    return -1;
  }
  
  // compute its checksum, and issue a warning if needed
  if (checksum((unsigned char *)data))
    checksumwarning("SMART ATA Error Log Structure");
  
  // Some disks have the byte order reversed in some SMART Summary
  // Error log entries
  if (con->fixfirmwarebug == FIX_SAMSUNG)
    fixsamsungerrorlog(data);
  else if (con->fixfirmwarebug == FIX_SAMSUNG2)
    fixsamsungerrorlog2(data);

  // Correct endian order if necessary
  if (isbigendian()){
    int i,j;
    
    // Device error count in bytes 452-3
    swap2((char *)&(data->ata_error_count));
    
    // step through 5 error log data structures
    for (i=0; i<5; i++){
      // step through 5 command data structures
      for (j=0; j<5; j++)
	// Command data structure 4-byte millisec timestamp
	swap4((char *)&(data->errorlog_struct[i].commands[j].timestamp));
      // Error data structure life timestamp
      swap2((char *)&(data->errorlog_struct[i].error_struct.timestamp));
    }
  }
  
  return 0;
}

int ataReadSmartThresholds (int device, struct ata_smart_thresholds *data){
  
  // get data from device
  if (smartcommandhandler(device, READ_THRESHOLDS, 0, (char *)data)){
    syserror("Error SMART Thresholds Read failed");
    return -1;
  }
  
  // compute its checksum, and issue a warning if needed
  if (checksum((unsigned char *)data))
    checksumwarning("SMART Attribute Thresholds Structure");
  
  // byte swap if needed
  if (isbigendian())
    swap2((char *)&(data->revnumber));

  return 0;
}

int ataEnableSmart (int device ){	
  if (smartcommandhandler(device, ENABLE, 0, NULL)){
    syserror("Error SMART Enable failed");
    return -1;
  }
  return 0;
}

int ataDisableSmart (int device ){	
  
  if (smartcommandhandler(device, DISABLE, 0, NULL)){
    syserror("Error SMART Disable failed");
    return -1;
  }  
  return 0;
}

int ataEnableAutoSave(int device){  
  if (smartcommandhandler(device, AUTOSAVE, 241, NULL)){
    syserror("Error SMART Enable Auto-save failed");
    return -1;
  }
  return 0;
}

int ataDisableAutoSave(int device){
  
  if (smartcommandhandler(device, AUTOSAVE, 0, NULL)){
    syserror("Error SMART Disable Auto-save failed");
    return -1;
  }
  return 0;
}

// In *ALL* ATA standards the Enable/Disable AutoOffline command is
// marked "OBSOLETE". It is defined in SFF-8035i Revision 2, and most
// vendors still support it for backwards compatibility. IBM documents
// it for some drives.
int ataEnableAutoOffline (int device ){	
  
  /* timer hard coded to 4 hours */  
  if (smartcommandhandler(device, AUTO_OFFLINE, 248, NULL)){
    syserror("Error SMART Enable Automatic Offline failed");
    return -1;
  }
  return 0;
}

// Another Obsolete Command.  See comments directly above, associated
// with the corresponding Enable command.
int ataDisableAutoOffline (int device ){	
  
  if (smartcommandhandler(device, AUTO_OFFLINE, 0, NULL)){
    syserror("Error SMART Disable Automatic Offline failed");
    return -1;
  }
  return 0;
}

// If SMART is enabled, supported, and working, then this call is
// guaranteed to return 1, else zero.  Silent inverse of
// ataSmartStatus()
int ataDoesSmartWork(int device){	
  return !smartcommandhandler(device, STATUS, 0, NULL);
}

// This function uses a different interface (DRIVE_TASK) than the
// other commands in this file.
int ataSmartStatus2(int device){

  int returnval=smartcommandhandler(device, STATUS_CHECK, 0, NULL);
  
  if (returnval==-1){
    syserror("Error SMART Status command via HDIO_DRIVE_TASK failed");
    pout("Rebuild older linux 2.2 kernels with HDIO_DRIVE_TASK support enabled\n");
  }
  
  return returnval;
}

// This is the way to execute ALL tests: offline, short self-test,
// extended self test, with and without captive mode, etc.
int ataSmartTest(int device, int testtype){	
  char cmdmsg[128],*type,*captive;
  int errornum;
  int cap,select=0;

  // Boolean, if set, says test is captive
  cap=testtype & CAPTIVE_MASK;
  
  // Boolean, if set, then test is selective
  select=(testtype==SELECTIVE_SELF_TEST || testtype==SELECTIVE_CAPTIVE_SELF_TEST);

  // Set up strings that describe the type of test
  if (cap)
    captive="captive";
  else
    captive="off-line";
  
  if (testtype==OFFLINE_FULL_SCAN)
    type="off-line";
  else  if (testtype==SHORT_SELF_TEST || testtype==SHORT_CAPTIVE_SELF_TEST)
    type="Short self-test";
  else if (testtype==EXTEND_SELF_TEST || testtype==EXTEND_CAPTIVE_SELF_TEST)
    type="Extended self-test";
  else if (testtype==CONVEYANCE_SELF_TEST || testtype==CONVEYANCE_CAPTIVE_SELF_TEST)
    type="Conveyance self-test";
#if DEVELOP_SELECTIVE_SELF_TEST
  else if (select){
    int i;
    type="Selective self-test";
    pout("Using test spans:\n");
    for (i = 0; i < con->smartselectivenumspans; i++)
      pout("%lld - %lld\n",
	   con->smartselectivespan[i][0],
	   con->smartselectivespan[i][1]);
  }
#endif
  else
    type="[Unrecognized] self-test";
  
  //  Print ouf message that we are sending the command to test
  if (testtype==ABORT_SELF_TEST)
    sprintf(cmdmsg,"Abort SMART off-line mode self-test routine");
  else
    sprintf(cmdmsg,"Execute SMART %s routine immediately in %s mode",type,captive);
  pout("Sending command: \"%s\".\n",cmdmsg);

#if DEVELOP_SELECTIVE_SELF_TEST
  // If doing a selective self-test, issue WRITE_LOG command
  if (select && ataWriteSelectiveSelfTestLog(device)){
    char errormsg[256];
    sprintf(errormsg,"Command \"%s\" failed to WRITE Selective Self-Test Log ",cmdmsg); 
    syserror(errormsg);
    pout("\n");
    return -1;
  }
#endif

  // Now send the command to test
  errornum=smartcommandhandler(device, IMMEDIATE_OFFLINE, testtype, NULL);

  if (errornum && !(cap && errno==EIO)){
    char errormsg[128];
    sprintf(errormsg,"Command \"%s\" failed",cmdmsg); 
    syserror(errormsg);
    pout("\n");
    return -1;
  }
  
  // Since the command succeeded, tell user
  if (testtype==ABORT_SELF_TEST)
    pout("Self-testing aborted!\n");
  else
    pout("Drive command \"%s\" successful.\nTesting has begun.\n",cmdmsg);
  return 0;
}

/* Test Time Functions */
int TestTime(struct ata_smart_values *data,int testtype){
  switch (testtype){
  case OFFLINE_FULL_SCAN:
    return (int) data->total_time_to_complete_off_line;
  case SHORT_SELF_TEST:
  case SHORT_CAPTIVE_SELF_TEST:
    return (int) data->short_test_completion_time;
  case EXTEND_SELF_TEST:
  case EXTEND_CAPTIVE_SELF_TEST:
    return (int) data->extend_test_completion_time;
  case CONVEYANCE_SELF_TEST:
  case CONVEYANCE_CAPTIVE_SELF_TEST:
    return (int) data->conveyance_test_completion_time;
  default:
    return 0;
  }
}

// This function tells you both about the ATA error log and the
// self-test error log capability.  The bit is poorly documented in
// the ATA/ATAPI standard.  SMART error logging is also indicated in
// bit 0 of DEVICE IDENTIFY word 87 (if top two bits of word 87 match
// pattern 01).  However this was only introduced in ATA-6 (but error
// logging was in ATA-5).
int isSmartErrorLogCapable ( struct ata_smart_values *data){
   return data->errorlog_capability & 0x01;
}

int isGeneralPurposeLoggingCapable(struct ata_identify_device *identity){
  unsigned short word84=identity->command_set_extension;
  unsigned short word87=identity->csf_default;

  // If bit 14 of word 84 is set to one and bit 15 of word 84 is
  // cleared to zero, the contents of word 84 contains valid support
  // information. If not, support information is not valid in this
  // word.
  if ((word84>>14) == 0x01)
    // If bit 5 of word 84 is set to one, the device supports the
    // General Purpose Logging feature set.
    return (word84 & (0x01 << 5));
  
  // If bit 14 of word 87 is set to one and bit 15 of word 87 is
  // cleared to zero, the contents of words (87:85) contain valid
  // information. If not, information is not valid in these words.  
  if ((word87>>14) == 0x01)
    // If bit 5 of word 87 is set to one, the device supports
    // the General Purpose Logging feature set.
    return (word87 & (0x01 << 5));

  // not capable
  return 0;
}


// SMART self-test capability is also indicated in bit 1 of DEVICE
// IDENTIFY word 87 (if top two bits of word 87 match pattern 01).
// However this was only introduced in ATA-6 (but self-test log was in
// ATA-5).
int isSupportExecuteOfflineImmediate(struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x01;
}
// Note in the ATA-5 standard, the following bit is listed as "Vendor
// Specific".  So it may not be reliable. The only use of this that I
// have found is in IBM drives, where it is well-documented.  See for
// example page 170, section 13.32.1.18 of the IBM Travelstar 40GNX
// hard disk drive specifications page 164 Revision 1.1 22 Apr 2002.
int isSupportAutomaticTimer(struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x02;
}
int isSupportOfflineAbort(struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x04;
}
int isSupportOfflineSurfaceScan(struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x08;
}
int isSupportSelfTest (struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x10;
}
int isSupportConveyanceSelfTest(struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x20;
}
int isSupportSelectiveSelfTest(struct ata_smart_values *data){
   return data->offline_data_collection_capability & 0x40;
}



// Loop over all valid attributes.  If they are prefailure attributes
// and are at or below the threshold value, then return the ID of the
// first failing attribute found.  Return 0 if all prefailure
// attributes are in bounds.  The spec says "Bit 0
// -Pre-failure/advisory - If the value of this bit equals zero, an
// attribute value less than or equal to its corresponding attribute
// threshold indicates an advisory condition where the usage or age of
// the device has exceeded its intended design life period. If the
// value of this bit equals one, an atribute value less than or equal
// to its corresponding attribute threshold indicates a pre-failure
// condition where imminent loss of data is being predicted."


// onlyfailed=0 : are or were any age or prefailure attributes <= threshold
// onlyfailed=1:  are any prefailure attributes <= threshold now
int ataCheckSmart(struct ata_smart_values *data,
		  struct ata_smart_thresholds *thresholds,
		  int onlyfailed){
  int i;
  
  // loop over all attributes
  for (i=0; i<NUMBER_ATA_SMART_ATTRIBUTES; i++){

    // pointers to disk's values and vendor's thresholds
    struct ata_smart_attribute *disk=data->vendor_attributes+i;
    struct ata_smart_threshold_entry *thre=thresholds->thres_entries+i;
 
    // consider only valid attributes
    if (disk->id && thre->id){
      int failednow,failedever;
      
      failednow =disk->current <= thre->threshold;
      failedever=disk->worst   <= thre->threshold;
      
      if (!onlyfailed && failedever)
	return disk->id;
      
      if (onlyfailed && failednow && ATTRIBUTE_FLAGS_PREFAILURE(disk->flags))
	return disk->id;      
    }
  }
  return 0;
}



// This checks the n'th attribute in the attribute list, NOT the
// attribute with id==n.  If the attribute does not exist, or the
// attribute is > threshold, then returns zero.  If the attribute is
// <= threshold (failing) then we the attribute number if it is a
// prefail attribute.  Else we return minus the attribute number if it
// is a usage attribute.
int ataCheckAttribute(struct ata_smart_values *data,
		      struct ata_smart_thresholds *thresholds,
		      int n){
  struct ata_smart_attribute *disk;
  struct ata_smart_threshold_entry *thre;
  
  if (n<0 || n>=NUMBER_ATA_SMART_ATTRIBUTES || !data || !thresholds)
    return 0;
  
  // pointers to disk's values and vendor's thresholds
  disk=data->vendor_attributes+n;
  thre=thresholds->thres_entries+n;

  if (!disk || !thre)
    return 0;
  
  // consider only valid attributes, check for failure
  if (!disk->id || !thre->id || (disk->id != thre->id) || disk->current> thre->threshold)
    return 0;
  
  // We have found a failed attribute.  Return positive or negative? 
  if (ATTRIBUTE_FLAGS_PREFAILURE(disk->flags))
    return disk->id;
  else
    return -1*(disk->id);
}


// This routine prints the raw value of an attribute as a text string
// into out. It also returns this 48-bit number as a long long.  The
// array defs[] contains non-zero values if particular attributes have
// non-default interpretations.

long long ataPrintSmartAttribRawValue(char *out, 
				      struct ata_smart_attribute *attribute,
				      unsigned char *defs){
  long long rawvalue;
  unsigned word[3];
  int j;
  unsigned char select;
  
  // convert the six individual bytes to a long long (8 byte) integer.
  // This is the value that we'll eventually return.
  rawvalue = 0;
  for (j=0; j<6; j++) {
    // This looks a bit roundabout, but is necessary.  Don't
    // succumb to the temptation to use raw[j]<<(8*j) since under
    // the normal rules this will be promoted to the native type.
    // On a 32 bit machine this might then overflow.
    long long temp;
    temp = attribute->raw[j];
    temp <<= 8*j;
    rawvalue |= temp;
  }

  // convert quantities to three two-byte words
  for (j=0; j<3; j++){
    word[j] = attribute->raw[2*j+1];
    word[j] <<= 8;
    word[j] |= attribute->raw[2*j];
  }
  
  // if no data array, Attributes have default interpretations
  if (defs)
    select=defs[attribute->id];
  else
    select=0;

  // Print six one-byte quantities.
  if (select==253){
    for (j=0; j<5; j++)
      out+=sprintf(out, "%d ", attribute->raw[5-j]);
    out+=sprintf(out, "%d ", attribute->raw[0]);
    return rawvalue;
  } 
  
  // Print three two-byte quantities
  if (select==254){
    out+=sprintf(out, "%d %d %d", word[2], word[1], word[0]); 
    return rawvalue;
  } 
  
  // Print one six-byte quantity
  if (select==255){
    out+=sprintf(out, "%llu", rawvalue);
    return rawvalue;
  }

  // This switch statement is where we handle Raw attributes
  // that are stored in an unusual vendor-specific format,
  switch (attribute->id){
    // Spin-up time
  case 3:
    out+=sprintf(out, "%d", word[0]);
    // if second nonzero then it stores the average spin-up time
    if (word[1])
      out+=sprintf(out, " (Average %d)", word[1]);
    break;
    // Power on time
  case 9:
    if (select==1){
      // minutes
      long long tmp1=rawvalue/60;
      long long tmp2=rawvalue%60;
      out+=sprintf(out, "%lluh+%02llum", tmp1, tmp2);
    }
    else if (select==3){
      // seconds
      long long hours=rawvalue/3600;
      long long minutes=(rawvalue-3600*hours)/60;
      long long seconds=rawvalue%60;
      out+=sprintf(out, "%lluh+%02llum+%02llus", hours, minutes, seconds);
    }
    else if (select==4){
      // 30-second counter
      long long tmp1=rawvalue/120;
      long long tmp2=(rawvalue-120*tmp1)/2;
      out+=sprintf(out, "%lluh+%02llum", tmp1, tmp2);
    }
    else
      // hours
      out+=sprintf(out, "%llu", rawvalue);  //stored in hours
    break;
   // Load unload cycles
  case 193:
    if (select==1){
      // loadunload
      long load  =attribute->raw[0] + (attribute->raw[1]<<8) + (attribute->raw[2]<<16);
      long unload=attribute->raw[3] + (attribute->raw[4]<<8) + (attribute->raw[5]<<16);
      out+=sprintf(out, "%lu/%lu", load, unload);
    }
    else
      // associated
      out+=sprintf(out, "%llu", rawvalue);
    break;
    // Temperature
  case 194:
    if (select==1){
      // ten times temperature in Celsius
      int deg=word[0]/10;
      int tenths=word[0]%10;
      out+=sprintf(out, "%d.%d", deg, tenths);
    }
    else if (select==2)
      // unknown attribute
      out+=sprintf(out, "%llu", rawvalue);
    else {
      out+=sprintf(out, "%d", word[0]);
      if (!(rawvalue==word[0]))
	// The other bytes are in use. Try IBM's model
	out+=sprintf(out, " (Lifetime Min/Max %d/%d)", word[1], word[2]);
    }
    break;
  default:
    out+=sprintf(out, "%llu", rawvalue);
  }
  
  // Return the full value
  return rawvalue;
}


// Note some attribute names appear redundant because different
// manufacturers use different attribute IDs for an attribute with the
// same name.  The variable val should contain a non-zero value if a particular
// attributes has a non-default interpretation.
void ataPrintSmartAttribName(char *out, unsigned char id, unsigned char *definitions){
  char *name;
  unsigned char val;

  // If no data array, use default interpretations
  if (definitions)
    val=definitions[id];
  else
    val=0;

  switch (id){
    
  case 1:
    name="Raw_Read_Error_Rate";
    break;
  case 2:
    name="Throughput_Performance";
    break;
  case 3:
    name="Spin_Up_Time";
    break;
  case 4:
    name="Start_Stop_Count";
    break;
  case 5:
    name="Reallocated_Sector_Ct";
    break;
  case 6:
    name="Read_Channel_Margin";
    break;
  case 7:
    name="Seek_Error_Rate";
    break;
  case 8:
    name="Seek_Time_Performance";
    break;
  case 9:
    switch (val) {
    case 1:
      name="Power_On_Minutes";
      break;
    case 2:
      name="Temperature_Celsius";
      break;
    case 3:
      name="Power_On_Seconds";
      break;
    case 4:
      name="Power_On_Half_Minutes";
      break;
    default:
      name="Power_On_Hours";
      break;
    }
    break;
  case 10:
    name="Spin_Retry_Count";
    break;
  case 11:
    name="Calibration_Retry_Count";
    break;
  case 12:
    name="Power_Cycle_Count";
    break;
  case 13:
    name="Read_Soft_Error_Rate";
    break;
  case 191:
    name="G-Sense_Error_Rate";
    break;
  case 192:
    switch (val) {
    case 1:
      // Fujitsu
      name="Emergency_Retract_Cycle_Ct";
      break;
    default:
      name="Power-Off_Retract_Count";
      break;
    }
    break;
  case 193:
    name="Load_Cycle_Count";
    break;
  case 194:
    switch (val){
    case 1:
      // Samsung SV1204H with RK100-13 firmware
      name="Temperature_Celsius_x10";
      break;
    case 2:
      // for disks with no temperature Attribute
      name="Unknown_Attribute";
      break;
    default:
      name="Temperature_Celsius";
      break;
    }
    break;
  case 195:
    // Fujitsu name="ECC_On_The_Fly_Count";
    name="Hardware_ECC_Recovered";
    break;
  case 196:
    name="Reallocated_Event_Count";
    break;
  case 197:
    name="Current_Pending_Sector";
    break;
  case 198:
    switch (val){
    case 1:
      // Fujitsu
      name="Off-line_Scan_UNC_Sector_Ct";
      break;
    default:
      name="Offline_Uncorrectable";
      break;
    }
    break;
  case 199:
    name="UDMA_CRC_Error_Count";
    break;
  case 200:
    switch (val) {
    case 1:
      // Fujitsu MHS2020AT
      name="Write_Error_Count";
      break;
    default:
      // Western Digital
      name="Multi_Zone_Error_Rate";
      break;
    }
    break;
  case 201:
    switch (val) {
    case 1:
      // Fujitsu
      name="Detected_TA_Count";
      break;
    default:
      name="Soft_Read_Error_Rate";
      break;
    }
    break;
  case 202:
    // Fujitsu
    name="TA_Increase_Count";
    // Maxtor: Data Address Mark Errors
    break;
  case 203:
    // Fujitsu
    name="Run_Out_Cancel";
    // Maxtor: ECC Errors
    break;
  case 204:
    // Fujitsu
    name="Shock_Count_Write_Opern";
    // Maxtor: Soft ECC Correction
    break;
  case 205:
    // Fujitsu
    name="Shock_Rate_Write_Opern";
    // Maxtor: Thermal Aspirates
    break;
  case 206:
    // Fujitsu
    name="Flying_Height_Measurement";
    break;
  case 207:
    // Maxtor
    name="Spin_High_Current";
    break;
  case 208:
    // Maxtor
    name="Spin_Buzz";
    break;
  case 209:
    // Maxtor
    name="Offline_Seek_Performnce";
    break;
  case 220:
    switch (val) {
    case 1:
      name="Temperature_Celsius";
      break;
    default:
      name="Disk_Shift";
      break;
    }
    break;
  case 221:
    name="G-Sense_Error_Rate";
    break;
  case 222:
    name="Loaded_Hours";
    break;
  case 223:
    name="Load_Retry_Count";
    break;
  case 224:
    name="Load_Friction";
    break;
  case 225:
    name="Load_Cycle_Count";
    break;
  case 226:
    name="Load-in_Time";
    break;
  case 227:
    name="Torq-amp_Count";
    break;
  case 228:
    name="Power-off_Retract_Count";
    break;
  case 230:
    // seen in IBM DTPA-353750
    name="Head_Amplitude";
    break;
  case 231:
    name="Temperature_Celsius";
    break;
  case 240:
    name="Head_Flying_Hours";
    break;
  case 250:
    name="Read_Error_Retry_Rate";
    break;
  default:
    name="Unknown_Attribute";
    break;
  }
  sprintf(out,"%3hhu %s",id,name);
  return;
}
