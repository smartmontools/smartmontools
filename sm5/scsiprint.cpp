/*
 * scsiprint.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-3 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * Additional SCSI work:
 * Copyright (C) 2003 Douglas Gilbert <dougg@torque.net>
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
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "extern.h"
#include "scsicmds.h"
#include "scsiprint.h"
#include "smartctl.h"
#include "utility.h"

#define GBUF_SIZE 65535

const char* scsiprint_c_cvsid="$Id: scsiprint.cpp,v 1.29 2003/04/07 10:57:02 dpgilbert Exp $"
EXTERN_H_CVSID SCSICMDS_H_CVSID SCSIPRINT_H_CVSID SMARTCTL_H_CVSID UTILITY_H_CVSID;

// control block which points to external global control variables
extern smartmonctrl *con;

UINT8 gBuf[GBUF_SIZE];
#define LOG_RESP_LEN 1024

UINT8 gSmartPage = 0;
UINT8 gTempPage = 0;
UINT8 gSelfTestPage = 0;
UINT8 gStartStopPage = 0;
UINT8 gTapeAlertsPage = 0;


static void scsiGetSupportedLogPages(int device)
{
    int i, err;

    if ((err = logsense(device, SUPPORTED_LOG_PAGES, gBuf, LOG_RESP_LEN))) {
        pout("Log Sense failed, err=%d\n", err); 
        return;
    } 

    for (i = 4; i < gBuf[3] + LOGPAGEHDRSIZE; i++) {
        switch (gBuf[i])
        {
            case TEMPERATURE_PAGE:
                gTempPage = 1;
                break;
            case STARTSTOP_CYCLE_COUNTER_PAGE:
                gStartStopPage = 1;
                break;
            case SELFTEST_RESULTS_PAGE:
                gSelfTestPage = 1;
                break;
            case IE_LOG_PAGE:
                gSmartPage = 1;
                break;
            case TAPE_ALERTS_PAGE:
                gTapeAlertsPage = 1;
                break;
            default:
                break;
        }
    }
}

void scsiGetSmartData(int device)
{
    UINT8 asc;
    UINT8 ascq;
    UINT8 currenttemp;
    const char * cp;
    int err;

    if ((err = scsiCheckIE(device, gSmartPage, &asc, &ascq, &currenttemp))) {
        pout("scsiGetSmart Failed, err=%d\n", err);
        return;
    }
    cp = scsiGetIEString(asc, ascq);
    if (cp)
        pout("SMART Sense: %s [asc=%x,ascq=%x]\n", cp, asc, ascq); 
    else
        pout("SMART Sense: Ok!\n");

    if (currenttemp && !gTempPage)
        pout("Current Drive Temperature:     %d C\n", currenttemp);
}


void scsiGetTapeAlertsData(int device)
{
    unsigned short pagelength;
    unsigned short parametercode;
    int i, err;
    int failure = 0;

    if ((err = logsense(device, TAPE_ALERTS_PAGE, gBuf, LOG_RESP_LEN))) {
        pout("scsiGetSmartData Failed, err=%d\n", err);
        return;
    }
    if (gBuf[0] != 0x2e) {
        pout("TapeAlerts Log Sense Failed\n");
        return;
    }
    pagelength = (unsigned short) gBuf[2] << 8 | gBuf[3];

    for (i = 4; i < pagelength; i += 5) {
        parametercode = (unsigned short) gBuf[i] << 8 | gBuf[i+1];

        if (gBuf[i + 4]) {
            pout("Tape Alerts Error!!!\n%s\n",
                   scsiTapeAlertsTapeDevice(parametercode));
            failure = 1; 
        }          
    }

    if (! failure)
        pout("No Tape Alerts Failure\n");
}

void scsiGetStartStopData(int device)
{
    UINT32 currentStartStop;
    UINT32 recommendedStartStop; 
    int err;

    if ((err = logsense(device, STARTSTOP_CYCLE_COUNTER_PAGE, gBuf,
                        LOG_RESP_LEN))) {
        pout("scsiGetStartStopData Failed, err=%d\n", err);
        return;
    }
    if (gBuf[0] != STARTSTOP_CYCLE_COUNTER_PAGE) {
         pout("StartStop Log Sense Failed\n");
         return;
    }
    recommendedStartStop= gBuf[28]<< 24 | gBuf[29] << 16 |
                                       gBuf[30] << 8 | gBuf[31];
    currentStartStop= gBuf[36]<< 24 | gBuf[37] << 16 |
                                       gBuf[38] << 8 | gBuf[39];

    pout("Current start stop count:      %u times\n", currentStartStop);
    pout("Recommended start stop count:  %u times\n", recommendedStartStop);
} 

const char * self_test_code[] = {
        "Default         ", 
        "Background short", 
        "Background long ", 
        "Reserved(3)     ",
        "Abort background", 
        "Foreground short", 
        "Foreground long ",
        "Reserved(7)     "
};

const char * self_test_result[] = {
        "Completed                ",
        "Interrupted ('-X' switch)",
        "Interrupted (bus reset ?)",
        "Unknown error, incomplete",
        "Completed, segment failed",
        "Failed in first segment  ",
        "Failed in second segment ",
        "Failed in segment -->    ",
        "Reserved(8)              ", 
        "Reserved(9)              ", 
        "Reserved(10)             ", 
        "Reserved(11)             ", 
        "Reserved(12)             ", 
        "Reserved(13)             ", 
        "Reserved(14)             ",
        "Self test in progress ..."
};

// See Working Draft SCSI Primary Commands - 3 (SPC-3) pages 231-233
// T10/1416-D Rev 10
void  scsiPrintSelfTest(int device)
{
    int num, k, n, res, err;
    int noheader = 1;
    UINT8 * ucp;
    unsigned long long ull=0;

    if ((err = logsense(device, SELFTEST_RESULTS_PAGE, gBuf, LOG_RESP_LEN))) {
        pout("scsiPrintSelfTest Failed, err=%d", err);
        return;
    }
    if (gBuf[0] != SELFTEST_RESULTS_PAGE) {
        pout("Self-test Log Sense Failed\n");
        return;
    }
    // compute page length
    num = (gBuf[2] << 8) + gBuf[3];
    // Log sense page length 0x190 bytes
    if (num != 0x190) {
        pout("Self-test Log Sense length is 0x%x not 0x190 bytes\n",num);
        return;
    }
    // loop through the twenty possible entries
    for (k = 0, ucp = gBuf + 4; k < 20; ++k, ucp += 20 ) {
        int i;

        // timestamp in power-on hours (or zero if test in progress)
        n = (ucp[6] << 8) | ucp[7];

        // The spec says "all 20 bytes will be zero if no test" but
        // DG has found otherwise.  So this is a heuristic.
        if ((0 == n) && (0 == ucp[4]))
            break;

        // only print header if needed
        if (noheader) {
            pout("\nSMART Self-test log\n");
            pout("Num  Test              Status                 segment  "
                   "LifeTime  LBA_first_err [SK ASC ASQ]\n");
            pout("     Description                              number   "
                   "(hours)\n");
            noheader=0;
        }

        // print parameter code (test number) & self-test code text
        pout("#%2d  %s", (ucp[0] << 8) | ucp[1], 
            self_test_code[(ucp[4] >> 5) & 0x7]);

        // self-test result
        res = ucp[4] & 0xf;
        pout("  %s", self_test_result[res]);

        // self-test number identifies test that failed and consists
        // of either the number of the segment that failed during
        // the test, or the number of the test that failed and the
        // number of the segment in which the test was run, using a
        // vendor-specific method of putting both numbers into a
        // single byte.
        if (ucp[5])
            pout(" %3d",  (int)ucp[5]);
        else
            pout("   -");

        // print time that the self-test was completed
        if (n==0 && res==0xf)
        // self-test in progress
            pout("   NOW");
        else   
            pout(" %5d", n);
          
        // construct 8-byte integer address of first failure
        for (i = 0; i < 8; i++) {
            ull <<= 8;
            ull |= ucp[i+8];
        }
        // print Address of First Failure, if sensible
        if ((0xffffffffffffffffULL != ull) && (res > 0) && ( res < 0xf))
            pout("  0x%16llx", ull);
        else
            pout("                   -");

        // if sense key nonzero, then print it, along with
        // additional sense code and additional sense code qualifier
        if (ucp[16] & 0xf)
            pout(" [0x%x 0x%x 0x%x]\n", ucp[16] & 0xf, ucp[17], ucp[18]);
        else
            pout(" [-   -    -]\n");
    }

    // if header never printed, then there was no output
    if (noheader)
        pout("No self-tests have been logged\n\n");
    else
        pout("\n");
    return;
}
 
void scsiGetDriveInfo(int device, UINT8 * peripheral_type)
{
    char manufacturer[9];
    char product[17];
    char revision[5];
    char timedatetz[64];
    struct scsi_iec_mode_page iec;
    int err, len;
        
    memset(gBuf, 0, 36);
    if ((err = stdinquiry(device, gBuf, 36))) {
        pout("Standard Inquiry failed, err=%d\n", err);
        return;
    }
    len = gBuf[4] + 5;
    if (peripheral_type)
        *peripheral_type = gBuf[0] & 0x1f;

    if (len >= 36) {
        memset(manufacturer, 0, sizeof(manufacturer));
        strncpy(manufacturer, &gBuf[8], 8);
     
        memset(product, 0, sizeof(product));
        strncpy(product, &gBuf[16], 16);
            
        memset(revision, 0, sizeof(revision));
        strncpy(revision, &gBuf[32], 4);
        pout("Device: %s %s Version: %s\n", manufacturer, product, revision);
        if (0 == inquiry_vpd(device, 0x80, gBuf, 64)) {
            /* should use VPD page 0x83 and fall back to this page (0x80)
             * if 0x83 not supported. NAA requires a lot of decoding code */
            len = gBuf[3];
            gBuf[4 + len] = '\0';
            pout("Serial number: %s\n", &gBuf[4]);
        }
    } else
        pout("Short INQUIRY response, skip product id\n");

    // print current time and date and timezone
    dateandtimezone(timedatetz);
    pout("Local Time is: %s\n", timedatetz);

    // See if unit accepts SCSI commmands from us
    if ((err = scsiTestUnitReady(device))) {
        if (1 == err)
            pout("device is NOT READY (start it?)\n");
        else
            pout("device Test Unit Ready: err=%d\n", err);
        return;
    }
   
    if ((err = scsiFetchIECmpage(device, &iec))) {
        pout("Device does not support %s [err=%d]\n", 
                ((1 == *peripheral_type) ? "TapeAlerts" : "SMART"), err);
        return;
    }
    pout("Device supports %s and is %s\n%s\n", 
            (1 == *peripheral_type) ? "TapeAlerts" : "SMART",
            (scsi_IsExceptionControlEnabled(&iec)) ? "Enabled" : "Disabled",
            (scsi_IsWarningEnabled(&iec)) ? "Temperature Warning Enabled" :
                "Temperature Warning Disabled or Not Supported");
}

void scsiSmartEnable(int device)
{
    struct scsi_iec_mode_page iec;
    int err;

    if ((err = scsiFetchIECmpage(device, &iec))) {
        pout("unable to fetch IEC (SMART) mode page [err=%d]\n", err);
        return;
    }
    /* Enable Exception Control */
    if ((err = scsiSetExceptionControl(device, 1, &iec))) {
        pout("unable to set Informational Exception (SMART) flag [err=%d]\n",
             err);
        return;
    }
    pout("Informational Exceptions (SMART) enabled\n");
    /* Enable Temperature Warning */
    if ((err = scsiSetWarning(device, 1, &iec))) {
        pout("unable to set Temperature Warning flag [err=%d]\n",
             err);
        return;
    }
    pout("Temperature Warning enabled\n");
}
        
void scsiSmartDisable(int device)
{
    struct scsi_iec_mode_page iec;
    int err;

    if ((err = scsiFetchIECmpage(device, &iec))) {
        pout("unable to fetch IEC (SMART) mode page [err=%d]\n", err);
        return;
    }
    /* Disable Exception Control */
    if ((err = scsiSetExceptionControl(device, 0, &iec))) {
        pout("unable to clear Informational Exception (SMART) flag [err=%d]\n",
             err);
        return;
    }
    pout("Informational Exceptions (SMART) disabled\n");
    /* Disable Temperature Warning */
    if ((err = scsiSetWarning(device, 0, &iec))) {
        pout("unable to clear Temperature Warning flag [err=%d]\n",
             err);
        return;
    }
    pout("Temperature Warning disabled\n");
}

void scsiPrintTemp(int device)
{
    UINT8 temp;
    UINT8 trip;

    if (scsiGetTemp(device, &temp, &trip))
        return;
  
    pout("Current Drive Temperature:     %d C\n", temp);
    pout("Drive Trip Temperature:        %d C\n", trip);
}

void scsiPrintStopStart(int device)
{
/**
    unsigned int css;

    if (scsiGetStartStop(device, unsigned int *css))
        return;
    pout("Start Stop Count: %d\n", css);
**/
}

void scsiPrintMain(const char *dev_name, int fd)
{
    int checkedSupportedLogPages = 0;
    UINT8 peripheral_type = 0;

    if (con->driveinfo)
        scsiGetDriveInfo(fd, &peripheral_type); 

    if (con->smartenable) 
        scsiSmartEnable(fd);

    if (con->smartdisable)
        scsiSmartDisable(fd);

    if (con->checksmart) {
        scsiGetSupportedLogPages(fd);
        checkedSupportedLogPages = 1;
        if(gTapeAlertsPage)
            scsiGetTapeAlertsData(fd);
        else {
            scsiGetSmartData(fd);
            if(gTempPage)
                scsiPrintTemp(fd);         
            if(gStartStopPage)
                scsiGetStartStopData(fd);
        }
    }   
    if (con->smartselftestlog) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(fd);
        if (gSelfTestPage)
            scsiPrintSelfTest(fd);
    }
    if (con->smartexeoffimmediate) {
        if (scsiSmartDefaultSelfTest(fd)) {
            pout( "Default Self Test Failed\n");
            return;
        }
        pout("Default Self Test Successful\n");
    }
    if (con->smartshortcapselftest) {
        if (scsiSmartShortCapSelfTest(fd)) {
            pout("Short Foreground Self Test Failed\n");
            return;
        }
        pout("Short Foreground Self Test Successful\n");
    }
    if (con->smartshortselftest ) { 
        if ( scsiSmartShortSelfTest(fd)) {
            pout("Short Background Self Test Failed\n");
            return;
        }
        pout("Short Background Self Test has begun\n");
        pout("Use smartctl -X to abort test\n");
    }
    if (con->smartextendselftest) {
        if (scsiSmartExtendSelfTest(fd)) {
            pout("Extended Background Self Test Failed\n");
            return;
        }
        pout("Extended Background Self Test has begun\n");
        pout("Use smartctl -X to abort test\n");        
    }
    if (con->smartextendcapselftest) {
        if (scsiSmartExtendCapSelfTest(fd)) {
            pout("Extended Foreground Self Test Failed\n");
            return;
        }
        pout("Extended Foreground Self Test Successful\n");
    }
    if (con->smartselftestabort) {
        if (scsiSmartSelfTestAbort(fd)) {
            pout("Self Test Abort Failed\n");
            return;
        }
        pout("Self Test returned without error\n");
    }           
}
