/*
 * scsiprint.c
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-6 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * Additional SCSI work:
 * Copyright (C) 2003-6 Douglas Gilbert <dougg@torque.net>
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
#include <fcntl.h>
#include <errno.h>

#include "config.h"
#include "int64.h"
#include "extern.h"
#include "scsicmds.h"
#include "scsiprint.h"
#include "smartctl.h"
#include "utility.h"
#include "scsiata.h"

#define GBUF_SIZE 65535

const char* scsiprint_c_cvsid="$Id: scsiprint.cpp,v 1.109 2006/07/01 21:32:20 dpgilbert Exp $"
CONFIG_H_CVSID EXTERN_H_CVSID INT64_H_CVSID SCSICMDS_H_CVSID SCSIPRINT_H_CVSID SMARTCTL_H_CVSID UTILITY_H_CVSID;

// control block which points to external global control variables
extern smartmonctrl *con;

// to hold onto exit code for atexit routine
extern int exitstatus;

UINT8 gBuf[GBUF_SIZE];
#define LOG_RESP_LEN 252
#define LOG_RESP_LONG_LEN 8192
#define LOG_RESP_TAPE_ALERT_LEN 0x144

/* Log pages supported */
static int gSmartLPage = 0;     /* Informational Exceptions log page */
static int gTempLPage = 0;
static int gSelfTestLPage = 0;
static int gStartStopLPage = 0;
static int gReadECounterLPage = 0;
static int gWriteECounterLPage = 0;
static int gVerifyECounterLPage = 0;
static int gNonMediumELPage = 0;
static int gLastNErrorLPage = 0;
static int gTapeAlertsLPage = 0;
static int gSeagateCacheLPage = 0;
static int gSeagateFactoryLPage = 0;

/* Mode pages supported */
static int gIecMPage = 1;     /* N.B. assume it until we know otherwise */

/* Remember last successful mode sense/select command */
static int modese_len = 0;

// Compares failure type to policy in effect, and either exits or
// simply returns to the calling routine.
extern void failuretest(int type, int returnvalue);

static void scsiGetSupportedLogPages(int device)
{
    int i, err;

    if ((err = scsiLogSense(device, SUPPORTED_LPAGES, gBuf, 
                            LOG_RESP_LEN, 0))) {
        if (con->reportscsiioctl > 0)
            pout("Log Sense for supported pages failed [%s]\n", 
                 scsiErrString(err)); 
        return;
    } 

    for (i = 4; i < gBuf[3] + LOGPAGEHDRSIZE; i++) {
        switch (gBuf[i])
        {
            case READ_ERROR_COUNTER_LPAGE:
                gReadECounterLPage = 1;
                break;
            case WRITE_ERROR_COUNTER_LPAGE:
                gWriteECounterLPage = 1;
                break;
            case VERIFY_ERROR_COUNTER_LPAGE:
                gVerifyECounterLPage = 1;
                break;
            case LAST_N_ERROR_LPAGE:
                gLastNErrorLPage = 1;
                break;
            case NON_MEDIUM_ERROR_LPAGE:
                gNonMediumELPage = 1;
                break;
            case TEMPERATURE_LPAGE:
                gTempLPage = 1;
                break;
            case STARTSTOP_CYCLE_COUNTER_LPAGE:
                gStartStopLPage = 1;
                break;
            case SELFTEST_RESULTS_LPAGE:
                gSelfTestLPage = 1;
                break;
            case IE_LPAGE:
                gSmartLPage = 1;
                break;
            case TAPE_ALERTS_LPAGE:
                gTapeAlertsLPage = 1;
                break;
            case SEAGATE_CACHE_LPAGE:
                gSeagateCacheLPage = 1;
                break;
            case SEAGATE_FACTORY_LPAGE:
                gSeagateFactoryLPage = 1;
                break;
            default:
                break;
        }
    }
}

/* Returns 0 if ok, -1 if can't check IE, -2 if can check and bad
   (or at least something to report). */
static int scsiGetSmartData(int device, int attribs)
{
    UINT8 asc;
    UINT8 ascq;
    UINT8 currenttemp = 0;
    UINT8 triptemp = 0;
    const char * cp;
    int err = 0;

    PRINT_ON(con);
    if (scsiCheckIE(device, gSmartLPage, gTempLPage, &asc, &ascq,
                    &currenttemp, &triptemp)) {
        /* error message already announced */
        PRINT_OFF(con);
        return -1;
    }
    PRINT_OFF(con);
    cp = scsiGetIEString(asc, ascq);
    if (cp) {
        err = -2;
        PRINT_ON(con);
        pout("SMART Health Status: %s [asc=%x, ascq=%x]\n", cp, asc, ascq); 
        PRINT_OFF(con);
    } else if (gIecMPage)
        pout("SMART Health Status: OK\n");

    if (attribs && !gTempLPage) {
        if (currenttemp || triptemp)
            pout("\n");
        if (currenttemp) {
            if (255 != currenttemp)
                pout("Current Drive Temperature:     %d C\n", currenttemp);
            else
                pout("Current Drive Temperature:     <not available>\n");
        }
        if (triptemp)
            pout("Drive Trip Temperature:        %d C\n", triptemp);
    }
    return err;
}


// Returns number of logged errors or zero if none or -1 if fetching
// TapeAlerts fails
static char *severities = "CWI";

static int scsiGetTapeAlertsData(int device, int peripheral_type)
{
    unsigned short pagelength;
    unsigned short parametercode;
    int i, err;
    char *s;
    const char *ts;
    int failures = 0;

    PRINT_ON(con);
    if ((err = scsiLogSense(device, TAPE_ALERTS_LPAGE, gBuf, 
                        LOG_RESP_TAPE_ALERT_LEN, LOG_RESP_TAPE_ALERT_LEN))) {
        pout("scsiGetTapesAlertData Failed [%s]\n", scsiErrString(err));
        PRINT_OFF(con);
        return -1;
    }
    if (gBuf[0] != 0x2e) {
        pout("TapeAlerts Log Sense Failed\n");
        PRINT_OFF(con);
        return -1;
    }
    pagelength = (unsigned short) gBuf[2] << 8 | gBuf[3];

    for (s=severities; *s; s++) {
        for (i = 4; i < pagelength; i += 5) {
            parametercode = (unsigned short) gBuf[i] << 8 | gBuf[i+1];

            if (gBuf[i + 4]) {
                ts = SCSI_PT_MEDIUM_CHANGER == peripheral_type ?
                    scsiTapeAlertsChangerDevice(parametercode) :
                    scsiTapeAlertsTapeDevice(parametercode);
                if (*ts == *s) {
                    if (!failures)
                        pout("TapeAlert Errors (C=Critical, W=Warning, I=Informational):\n");
                    pout("[0x%02x] %s\n", parametercode, ts);
                    failures += 1; 
                }
            }
        }
    }
    PRINT_OFF(con);

    if (! failures)
        pout("TapeAlert: OK\n");

    return failures;
}

static void scsiGetStartStopData(int device)
{
    UINT32 currentStartStop;
    UINT32 recommendedStartStop; 
    int err, len, k;
    char str[6];

    if ((err = scsiLogSense(device, STARTSTOP_CYCLE_COUNTER_LPAGE, gBuf,
                            LOG_RESP_LEN, 0))) {
        PRINT_ON(con);
        pout("scsiGetStartStopData Failed [%s]\n", scsiErrString(err));
        PRINT_OFF(con);
        return;
    }
    if (gBuf[0] != STARTSTOP_CYCLE_COUNTER_LPAGE) {
        PRINT_ON(con);
        pout("StartStop Log Sense Failed, page mismatch\n");
        PRINT_OFF(con);
        return;
    }
    len = ((gBuf[2] << 8) | gBuf[3]) + 4;
    if (len > 13) {
        for (k = 0; k < 2; ++k)
            str[k] = gBuf[12 + k];
        str[k] = '\0';
        pout("Manufactured in week %s of year ", str);
        for (k = 0; k < 4; ++k)
            str[k] = gBuf[8 + k];
        str[k] = '\0';
        pout("%s\n", str);
    }
    if (len > 39) {
        recommendedStartStop = (gBuf[28] << 24) | (gBuf[29] << 16) |
                               (gBuf[30] << 8) | gBuf[31];
        currentStartStop = (gBuf[36] << 24) | (gBuf[37] << 16) |
                           (gBuf[38] << 8) | gBuf[39];
        pout("Current start stop count:      %u times\n", currentStartStop);
	if (0xffffffff != recommendedStartStop)
            pout("Recommended maximum start stop count:  %u times\n", 
                 recommendedStartStop);
    }
} 

static void scsiPrintGrownDefectListLen(int device)
{
    int err, dl_format, dl_len, div;

    memset(gBuf, 0, 4);
    if ((err = scsiReadDefect10(device, 0 /* req_plist */, 1 /* req_glist */,
                                4 /* bytes from index */, gBuf, 4))) {
        if (con->reportscsiioctl > 0) {
            PRINT_ON(con);
            pout("Read defect list (10) Failed: %s\n", scsiErrString(err));
            PRINT_OFF(con);
        }
        return;
    }
    if (0x8 != (gBuf[1] & 0x18)) {
        PRINT_ON(con);
        pout("Read defect list: asked for grown list but didn't get it\n");
        PRINT_OFF(con);
        return;
    }
    div = 0;
    dl_format = (gBuf[1] & 0x7);
    switch (dl_format) {
        case 0:     /* short block */
            div = 4;
            break;
        case 3:     /* long block */
        case 4:     /* bytes from index */
        case 5:     /* physical sector */
            div = 8;
            break;
        default:
            PRINT_ON(con);
            pout("defect list format %d unknown\n", dl_format);
            PRINT_OFF(con);
            break;
    }
    dl_len = (gBuf[2] << 8) + gBuf[3];
    if (0 == dl_len)
        pout("Elements in grown defect list: 0\n");
    else {
        if (0 == div)
            pout("Grown defect list length=%d bytes [unknown "
                 "number of elements]\n", dl_len);
        else
            pout("Elements in grown defect list: %d\n", dl_len / div);
    }
}

static void scsiPrintSeagateCacheLPage(int device)
{
    int k, j, num, pl, pc, err, len;
    unsigned char * ucp;
    unsigned char * xp;
    uint64_t ull;

    if ((err = scsiLogSense(device, SEAGATE_CACHE_LPAGE, gBuf,
                            LOG_RESP_LEN, 0))) {
        PRINT_ON(con);
        pout("Seagate Cache Log Sense Failed: %s\n", scsiErrString(err));
        PRINT_OFF(con);
        return;
    }
    if (gBuf[0] != SEAGATE_CACHE_LPAGE) {
        PRINT_ON(con);
        pout("Seagate Cache Log Sense Failed, page mismatch\n");
        PRINT_OFF(con);
        return;
    }
    len = ((gBuf[2] << 8) | gBuf[3]) + 4;
    num = len - 4;
    ucp = &gBuf[0] + 4;
    while (num > 3) {
        pc = (ucp[0] << 8) | ucp[1];
        pl = ucp[3] + 4;
        switch (pc) {
        case 0: case 1: case 2: case 3: case 4:
            break;
        default: 
            if (con->reportscsiioctl > 0) {
                PRINT_ON(con);
                pout("Vendor (Seagate) cache lpage has unexpected parameter"
                     ", skip\n");
                PRINT_OFF(con);
            }
            return;
        }
        num -= pl;
        ucp += pl;
    }
    pout("Vendor (Seagate) cache information\n");
    num = len - 4;
    ucp = &gBuf[0] + 4;
    while (num > 3) {
        pc = (ucp[0] << 8) | ucp[1];
        pl = ucp[3] + 4;
        switch (pc) {
        case 0: pout("  Blocks sent to initiator"); break;
        case 1: pout("  Blocks received from initiator"); break;
        case 2: pout("  Blocks read from cache and sent to initiator"); break;
        case 3: pout("  Number of read and write commands whose size "
                       "<= segment size"); break;
        case 4: pout("  Number of read and write commands whose size "
                       "> segment size"); break;
        default: pout("  Unknown Seagate parameter code [0x%x]", pc); break;
        }
        k = pl - 4;
        xp = ucp + 4;
        if (k > (int)sizeof(ull)) {
            xp += (k - (int)sizeof(ull));
            k = (int)sizeof(ull);
        }
        ull = 0;
        for (j = 0; j < k; ++j) {
            if (j > 0)
                ull <<= 8;
            ull |= xp[j];
        }
        pout(" = %"PRIu64"\n", ull);
        num -= pl;
        ucp += pl;
    }
}

static void scsiPrintSeagateFactoryLPage(int device)
{
    int k, j, num, pl, pc, len, err, good, bad;
    unsigned char * ucp;
    unsigned char * xp;
    uint64_t ull;

    if ((err = scsiLogSense(device, SEAGATE_FACTORY_LPAGE, gBuf,
                            LOG_RESP_LEN, 0))) {
        PRINT_ON(con);
        pout("scsiPrintSeagateFactoryLPage Failed [%s]\n", scsiErrString(err));
        PRINT_OFF(con);
        return;
    }
    if (gBuf[0] != SEAGATE_FACTORY_LPAGE) {
        PRINT_ON(con);
        pout("Seagate/Hitachi Factory Log Sense Failed, page mismatch\n");
        PRINT_OFF(con);
        return;
    }
    len = ((gBuf[2] << 8) | gBuf[3]) + 4;
    num = len - 4;
    ucp = &gBuf[0] + 4;
    good = 0;
    bad = 0;
    while (num > 3) {
        pc = (ucp[0] << 8) | ucp[1];
        pl = ucp[3] + 4;
        switch (pc) {
        case 0: case 8:
            ++good;
            break;
        default: 
            ++bad;
            break;
        }
        num -= pl;
        ucp += pl;
    }
    if ((good < 2) || (bad > 4)) {  /* heuristic */
        if (con->reportscsiioctl > 0) {
            PRINT_ON(con);
            pout("\nVendor (Seagate/Hitachi) factory lpage has too many "
                 "unexpected parameters, skip\n");
            PRINT_OFF(con);
        }
        return;
    }
    pout("Vendor (Seagate/Hitachi) factory information\n");
    num = len - 4;
    ucp = &gBuf[0] + 4;
    while (num > 3) {
        pc = (ucp[0] << 8) | ucp[1];
        pl = ucp[3] + 4;
        good = 0;
        switch (pc) {
        case 0: pout("  number of hours powered up");
            good = 1;
            break;
        case 8: pout("  number of minutes until next internal SMART test");
            good = 1;
            break;
        default:
            if (con->reportscsiioctl > 0) {
                PRINT_ON(con);
                pout("Vendor (Seagate/Hitachi) factory lpage: "
                     "unknown parameter code [0x%x]\n", pc);
                PRINT_OFF(con);
            }
            break;
        }
        if (good) {
            k = pl - 4;
            xp = ucp + 4;
            if (k > (int)sizeof(ull)) {
                xp += (k - (int)sizeof(ull));
                k = (int)sizeof(ull);
            }
            ull = 0;
            for (j = 0; j < k; ++j) {
                if (j > 0)
                    ull <<= 8;
                ull |= xp[j];
            }
            if (0 == pc)
                pout(" = %.2f\n", uint64_to_double(ull) / 60.0 );
            else
                pout(" = %"PRIu64"\n", ull);
        }
        num -= pl;
        ucp += pl;
    }
}

static void scsiPrintErrorCounterLog(int device)
{
    struct scsiErrorCounter errCounterArr[3];
    struct scsiErrorCounter * ecp;
    struct scsiNonMediumError nme;
    int found[3] = {0, 0, 0};
    const char * pageNames[3] = {"read:   ", "write:  ", "verify: "};
    int k;
    double processed_gb;

    if (gReadECounterLPage && (0 == scsiLogSense(device,
                READ_ERROR_COUNTER_LPAGE, gBuf, LOG_RESP_LEN, 0))) {
        scsiDecodeErrCounterPage(gBuf, &errCounterArr[0]);
        found[0] = 1;
    }
    if (gWriteECounterLPage && (0 == scsiLogSense(device,
                WRITE_ERROR_COUNTER_LPAGE, gBuf, LOG_RESP_LEN, 0))) {
        scsiDecodeErrCounterPage(gBuf, &errCounterArr[1]);
        found[1] = 1;
    }
    if (gVerifyECounterLPage && (0 == scsiLogSense(device,
                VERIFY_ERROR_COUNTER_LPAGE, gBuf, LOG_RESP_LEN, 0))) {
        scsiDecodeErrCounterPage(gBuf, &errCounterArr[2]);
        ecp = &errCounterArr[2];
        for (k = 0; k < 7; ++k) {
            if (ecp->gotPC[k] && ecp->counter[k]) {
                found[2] = 1;
                break;
            }
        }
    }
    if (found[0] || found[1] || found[2]) {
        pout("\nError counter log:\n");
        pout("           Errors Corrected by           Total   "
             "Correction     Gigabytes    Total\n");
        pout("               ECC          rereads/    errors   "
             "algorithm      processed    uncorrected\n");
        pout("           fast | delayed   rewrites  corrected  "
             "invocations   [10^9 bytes]  errors\n");
        for (k = 0; k < 3; ++k) {
            if (! found[k])
                continue;
            ecp = &errCounterArr[k];
            pout("%s%8"PRIu64" %8"PRIu64"  %8"PRIu64"  %8"PRIu64"   %8"PRIu64, 
                 pageNames[k], ecp->counter[0], ecp->counter[1], 
                 ecp->counter[2], ecp->counter[3], ecp->counter[4]);
            processed_gb = uint64_to_double(ecp->counter[5]) / 1000000000.0;
            pout("   %12.3f    %8"PRIu64"\n", processed_gb, ecp->counter[6]);
        }
    }
    else 
        pout("\nError Counter logging not supported\n");
    if (gNonMediumELPage && (0 == scsiLogSense(device,
                NON_MEDIUM_ERROR_LPAGE, gBuf, LOG_RESP_LEN, 0))) {
        scsiDecodeNonMediumErrPage(gBuf, &nme);
        if (nme.gotPC0)
            pout("\nNon-medium error count: %8"PRIu64"\n", nme.counterPC0);
        if (nme.gotTFE_H)
            pout("Track following error count [Hitachi]: %8"PRIu64"\n",
                 nme.counterTFE_H);
        if (nme.gotPE_H)
            pout("Positioning error count [Hitachi]: %8"PRIu64"\n",
                 nme.counterPE_H);
    }
    if (gLastNErrorLPage && (0 == scsiLogSense(device,
                LAST_N_ERROR_LPAGE, gBuf, LOG_RESP_LONG_LEN, 0))) {
        unsigned char * ucp;
        int num, k, pc, pl;

        num = (gBuf[2] << 8) + gBuf[3] + 4;
        num = (num < LOG_RESP_LONG_LEN) ? num : LOG_RESP_LONG_LEN;
        ucp = gBuf + 4;
        num -= 4;
        if (num < 4)
            pout("\nNo error events logged\n");
        else {
            pout("\nLast n error events log page\n");
            for (k = num; k > 0; k -= pl, ucp += pl) {
                if (k < 3) {
                    pout("  <<short Last n error events log page>>\n");
                    break;
                }
                pl = ucp[3] + 4;
                pc = (ucp[0] << 8) + ucp[1];
                if (pl > 4) {
                    if ((ucp[2] & 0x1) && (ucp[2] & 0x2)) {
                        pout("  Error event %d:\n", pc);
                        pout("    [binary]:\n");
                        dStrHex((const char *)ucp + 4, pl - 4, 1);
                    } else if (ucp[2] & 0x1) {
                        pout("  Error event %d:\n", pc);
                        pout("    %.*s\n", pl - 4, (const char *)(ucp + 4));
                    } else {
                        if (con->reportscsiioctl > 0) {
                            pout("  Error event %d:\n", pc);
                            pout("    [data counter??]:\n");
                            dStrHex((const char *)ucp + 4, pl - 4, 1);
			}
                    }
                }
            }
        }
    }
}

static const char * self_test_code[] = {
        "Default         ", 
        "Background short", 
        "Background long ", 
        "Reserved(3)     ",
        "Abort background", 
        "Foreground short", 
        "Foreground long ",
        "Reserved(7)     "
};

static const char * self_test_result[] = {
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

// See SCSI Primary Commands - 3 (SPC-3) rev 23 (draft) section 7.2.10 .
// Returns 0 if ok else FAIL* bitmask. Note that if any of the most recent
// 20 self tests fail (result code 3 to 7 inclusive) then FAILLOG and/or
// FAILSMART is returned.
static int scsiPrintSelfTest(int device)
{
    int num, k, n, res, err, durationSec;
    int noheader = 1;
    int retval = 0;
    UINT8 * ucp;
    uint64_t ull=0;

    if ((err = scsiLogSense(device, SELFTEST_RESULTS_LPAGE, gBuf, 
                            LOG_RESP_SELF_TEST_LEN, 0))) {
        PRINT_ON(con);
        pout("scsiPrintSelfTest Failed [%s]\n", scsiErrString(err));
        PRINT_OFF(con);
        return FAILSMART;
    }
    if (gBuf[0] != SELFTEST_RESULTS_LPAGE) {
        PRINT_ON(con);
        pout("Self-test Log Sense Failed, page mismatch\n");
        PRINT_OFF(con);
        return FAILSMART;
    }
    // compute page length
    num = (gBuf[2] << 8) + gBuf[3];
    // Log sense page length 0x190 bytes
    if (num != 0x190) {
        PRINT_ON(con);
        pout("Self-test Log Sense length is 0x%x not 0x190 bytes\n",num);
        PRINT_OFF(con);
        return FAILSMART;
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

        // check the self-test result nibble, using the self-test results
        // field table from T10/1416-D (SPC-3) Rev. 23, section 7.2.10:
        switch ((res = ucp[4] & 0xf)) {
        case 0x3:
            // an unknown error occurred while the device server
            // was processing the self-test and the device server
            // was unable to complete the self-test
            retval|=FAILSMART;
            break;
        case 0x4:
            // the self-test completed with a failure in a test
            // segment, and the test segment that failed is not
            // known
            retval|=FAILLOG;
            break;
        case 0x5:
            // the first segment of the self-test failed
            retval|=FAILLOG;
            break;
        case 0x6:
            // the second segment of the self-test failed
            retval|=FAILLOG;
            break;
        case 0x7:
            // another segment of the self-test failed and which
            // test is indicated by the contents of the SELF-TEST
            // NUMBER field
            retval|=FAILLOG;
            break;
        default:
            break;
        }
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
            pout("     NOW");
        else   
            pout("   %5d", n);
          
        // construct 8-byte integer address of first failure
        for (i = 0; i < 8; i++) {
            ull <<= 8;
            ull |= ucp[i+8];
        }
        // print Address of First Failure, if sensible
        if ((~(uint64_t)0 != ull) && (res > 0) && (res < 0xf)) {
            char buff[32];

            // was hex but change to decimal to conform with ATA
            snprintf(buff, sizeof(buff), "%"PRIu64, ull);
            // snprintf(buff, sizeof(buff), "0x%"PRIx64, ull);
            pout("%18s", buff);
        } else
            pout("                 -");

        // if sense key nonzero, then print it, along with
        // additional sense code and additional sense code qualifier
        if (ucp[16] & 0xf)
            pout(" [0x%x 0x%x 0x%x]\n", ucp[16] & 0xf, ucp[17], ucp[18]);
        else
            pout(" [-   -    -]\n");
    }

    // if header never printed, then there was no output
    if (noheader)
        pout("No self-tests have been logged\n");
    else
        pout("\n");
    if ((0 == scsiFetchExtendedSelfTestTime(device, &durationSec, 
                        modese_len)) && (durationSec > 0)) {
        pout("Long (extended) Self Test duration: %d seconds "
             "[%.1f minutes]\n", durationSec, durationSec / 60.0);
    }
    return retval;
}

static const char * peripheral_dt_arr[] = {
        "disk",
        "tape",
        "printer",
        "processor",
        "optical disk(4)",
        "CD/DVD",
        "scanner",
        "optical disk(7)",
        "medium changer",
        "communications",
        "graphics(10)",
        "graphics(11)",
        "storage array",
        "enclosure",
        "simplified disk",
        "optical card reader"
};

static const char * transport_proto_arr[] = {
        "Fibre channel (FCP-2)",
        "Parallel SCSI (SPI-4)",
        "SSA",
        "IEEE 1394 (SBP-2)",
        "RDMA (SRP)",
        "iSCSI",
        "SAS",
        "ADT",
        "0x8",
        "0x9",
        "0xa",
        "0xb",
        "0xc",
        "0xd",
        "0xe",
        "0xf"
};

/* Returns 0 on success, 1 on general error and 2 for early, clean exit */
static int scsiGetDriveInfo(int device, UINT8 * peripheral_type, int all)
{
    char manufacturer[9];
    char product[17];
    char revision[5];
    char timedatetz[DATEANDEPOCHLEN];
    struct scsi_iec_mode_page iec;
    int err, iec_err, len, req_len, avail_len, val;
    int is_tape = 0;
    int peri_dt = 0;
    int returnval=0;
        
    memset(gBuf, 0, 96);
    req_len = 36;
    if ((err = scsiStdInquiry(device, gBuf, req_len))) {
        PRINT_ON(con);
        pout("Standard Inquiry (36 bytes) failed [%s]\n", scsiErrString(err));
        pout("Retrying with a 64 byte Standard Inquiry\n");
        PRINT_OFF(con);
        /* Marvell controllers fail on a 36 bytes StdInquiry, but 64 suffices */
        req_len = 64;
        if ((err = scsiStdInquiry(device, gBuf, req_len))) {
            PRINT_ON(con);
            pout("Standard Inquiry (64 bytes) failed [%s]\n",
                 scsiErrString(err));
            PRINT_OFF(con);
            return 1;
        }
    }
    avail_len = gBuf[4] + 5;
    len = (avail_len < req_len) ? avail_len : req_len;
    peri_dt = gBuf[0] & 0x1f;
    if (peripheral_type)
        *peripheral_type = peri_dt;

    if (len < 36) {
        PRINT_ON(con);
        pout("Short INQUIRY response, skip product id\n");
        PRINT_OFF(con);
        return 1;
    }
    memset(manufacturer, 0, sizeof(manufacturer));
    strncpy(manufacturer, (char *)&gBuf[8], 8);
 
    memset(product, 0, sizeof(product));
    strncpy(product, (char *)&gBuf[16], 16);
        
    memset(revision, 0, sizeof(revision));
    strncpy(revision, (char *)&gBuf[32], 4);
    if (all && (0 != strncmp(manufacturer, "ATA", 3)))
        pout("Device: %s %s Version: %s\n", manufacturer, product, revision);

    if (0 == strncmp(manufacturer, "3ware", 5) || 0 == strncmp(manufacturer, "AMCC", 4) ) {
        pout("please try adding '-d 3ware,N'\n");
        pout("you may also need to change device to /dev/twaN or /dev/tweN\n");
        return 2;
    } else if ((len >= 42) &&
	       (0 == strncmp((const char *)(gBuf + 36), "MVSATA", 6))) {
        pout("please try '-d marvell'\n");
        return 2;
    } else if ((0 == con->controller_explicit) &&
	       (0 == strncmp(manufacturer, "ATA     ", 8)) &&
               has_sat_pass_through(device, 0)) {
        con->controller_type = CONTROLLER_SAT;
        if (con->reportscsiioctl > 0) {
            PRINT_ON(con);
            pout("Detected SAT interface, switch to device type 'sat'\n");
            PRINT_OFF(con);
        }
	return 2;
    } else if ((avail_len >= 96) && (0 == strncmp(manufacturer, "ATA", 3))) {
        /* <<<< This is Linux specific code to detect SATA disks using a
                SCSI-ATA command translation layer. This may be generalized
                later when the t10.org SAT project matures. >>>> */
        pout("Device: %s %s Version: %s\n", manufacturer, product, revision);
        req_len = 252;
        memset(gBuf, 0, req_len);
        if ((err = scsiInquiryVpd(device, 0x83, gBuf, req_len))) {
            PRINT_ON(con);
            pout("Inquiry for VPD page 0x83 [device id] failed [%s]\n",
                  scsiErrString(err));
            PRINT_OFF(con);
            return 1;
        }
        avail_len = ((gBuf[2] << 8) + gBuf[3]) + 4;
        len = (avail_len < req_len) ? avail_len : req_len;
        if (isLinuxLibAta(gBuf, len)) {
            pout("\nIn Linux, SATA disks accessed via libata are "
                 "only supported by smartmontools\n"
                 "for kernel versions 2.6.15 and above.\n  Try "
                 "an additional '-d ata' or '-d sat' argument.\n");
            return 2;
        }
    }
    if (! all)
        return 0;

    /* Do this here to try and detect badly conforming devices (some USB
       keys) that will lock up on a InquiryVpd or log sense or ... */
    if ((iec_err = scsiFetchIECmpage(device, &iec, modese_len))) {
        if (SIMPLE_ERR_BAD_RESP == iec_err) {
            pout(">> Terminate command early due to bad response to IEC "
                 "mode page\n");
            PRINT_OFF(con);
            gIecMPage = 0;
            return 1;
        }
    } else
        modese_len = iec.modese_len;

    if (0 == (err = scsiInquiryVpd(device, 0x80, gBuf, 64))) {
        /* should use VPD page 0x83 and fall back to this page (0x80)
         * if 0x83 not supported. NAA requires a lot of decoding code */
        len = gBuf[3];
        gBuf[4 + len] = '\0';
        pout("Serial number: %s\n", &gBuf[4]);
    }
    else if (con->reportscsiioctl > 0) {
        PRINT_ON(con);
        if (SIMPLE_ERR_BAD_RESP == err)
            pout("Vital Product Data (VPD) bit ignored in INQUIRY\n");
        else
            pout("Vital Product Data (VPD) INQUIRY failed [%d]\n", err);
        PRINT_OFF(con);
    }

    // print SCSI peripheral device type
    if (peri_dt < (int)(sizeof(peripheral_dt_arr) / 
                        sizeof(peripheral_dt_arr[0])))
        pout("Device type: %s\n", peripheral_dt_arr[peri_dt]);
    else
        pout("Device type: <%d>\n", peri_dt);

    // See if transport protocol is known
    val = scsiFetchTransportProtocol(device, modese_len);
    if ((val >= 0) && (val <= 0xf))
        pout("Transport protocol: %s\n", transport_proto_arr[val]);

    // print current time and date and timezone
    dateandtimezone(timedatetz);
    pout("Local Time is: %s\n", timedatetz);

    if ((SCSI_PT_SEQUENTIAL_ACCESS == *peripheral_type) ||
        (SCSI_PT_MEDIUM_CHANGER == *peripheral_type))
        is_tape = 1;
    // See if unit accepts SCSI commmands from us
    if ((err = scsiTestUnitReady(device))) {
        if (SIMPLE_ERR_NOT_READY == err) {
            PRINT_ON(con);
            if (!is_tape)
                pout("device is NOT READY (e.g. spun down, busy)\n");
            else
                pout("device is NOT READY (e.g. no tape)\n");
            PRINT_OFF(con);
         } else if (SIMPLE_ERR_NO_MEDIUM == err) {
            PRINT_ON(con);
            pout("NO MEDIUM present on device\n");
            PRINT_OFF(con);
         } else if (SIMPLE_ERR_BECOMING_READY == err) {
            PRINT_ON(con);
            pout("device becoming ready (wait)\n");
            PRINT_OFF(con);
        } else {
            PRINT_ON(con);
            pout("device Test Unit Ready  [%s]\n", scsiErrString(err));
            PRINT_OFF(con);
        }
        failuretest(MANDATORY_CMD, returnval|=FAILID);
    }
   
    if (iec_err) {
        if (!is_tape) {
            PRINT_ON(con);
            pout("Device does not support SMART");
            if (con->reportscsiioctl > 0)
                pout(" [%s]\n", scsiErrString(iec_err));
            else
                pout("\n");
            PRINT_OFF(con);
        }
        gIecMPage = 0;
        return 0;
    }

    if (!is_tape)
        pout("Device supports SMART and is %s\n",
             (scsi_IsExceptionControlEnabled(&iec)) ? "Enabled" : "Disabled");
    pout("%s\n", (scsi_IsWarningEnabled(&iec)) ? 
                  "Temperature Warning Enabled" :
                  "Temperature Warning Disabled or Not Supported");
    return 0;
}

static int scsiSmartEnable(int device)
{
    struct scsi_iec_mode_page iec;
    int err;

    if ((err = scsiFetchIECmpage(device, &iec, modese_len))) {
        PRINT_ON(con);
        pout("unable to fetch IEC (SMART) mode page [%s]\n", 
             scsiErrString(err));
        PRINT_OFF(con);
        return 1;
    } else
        modese_len = iec.modese_len;

    if ((err = scsiSetExceptionControlAndWarning(device, 1, &iec))) {
        PRINT_ON(con);
        pout("unable to enable Exception control and warning [%s]\n",
             scsiErrString(err));
        PRINT_OFF(con);
        return 1;
    }
    /* Need to refetch 'iec' since could be modified by previous call */
    if ((err = scsiFetchIECmpage(device, &iec, modese_len))) {
        pout("unable to fetch IEC (SMART) mode page [%s]\n", 
             scsiErrString(err));
        return 1;
    } else
        modese_len = iec.modese_len;

    pout("Informational Exceptions (SMART) %s\n",
         scsi_IsExceptionControlEnabled(&iec) ? "enabled" : "disabled");
    pout("Temperature warning %s\n",
         scsi_IsWarningEnabled(&iec) ? "enabled" : "disabled");
    return 0;
}
        
static int scsiSmartDisable(int device)
{
    struct scsi_iec_mode_page iec;
    int err;

    if ((err = scsiFetchIECmpage(device, &iec, modese_len))) {
        PRINT_ON(con);
        pout("unable to fetch IEC (SMART) mode page [%s]\n", 
             scsiErrString(err));
        PRINT_OFF(con);
        return 1;
    } else
        modese_len = iec.modese_len;

    if ((err = scsiSetExceptionControlAndWarning(device, 0, &iec))) {
        PRINT_ON(con);
        pout("unable to disable Exception control and warning [%s]\n",
             scsiErrString(err));
        PRINT_OFF(con);
        return 1;
    }
    /* Need to refetch 'iec' since could be modified by previous call */
    if ((err = scsiFetchIECmpage(device, &iec, modese_len))) {
        pout("unable to fetch IEC (SMART) mode page [%s]\n", 
             scsiErrString(err));
        return 1;
    } else
        modese_len = iec.modese_len;

    pout("Informational Exceptions (SMART) %s\n",
         scsi_IsExceptionControlEnabled(&iec) ? "enabled" : "disabled");
    pout("Temperature warning %s\n",
         scsi_IsWarningEnabled(&iec) ? "enabled" : "disabled");
    return 0;
}

static void scsiPrintTemp(int device)
{
    UINT8 temp = 0;
    UINT8 trip = 0;

    if (scsiGetTemp(device, &temp, &trip))
        return;
  
    if (temp) {
        if (255 != temp)
            pout("Current Drive Temperature:     %d C\n", temp);
        else
            pout("Current Drive Temperature:     <not available>\n");
    }
    if (trip)
        pout("Drive Trip Temperature:        %d C\n", trip);
}

/* Main entry point used by smartctl command. Return 0 for success */
int scsiPrintMain(int fd)
{
    int checkedSupportedLogPages = 0;
    UINT8 peripheral_type = 0;
    int returnval = 0;
    int res, durationSec;

    res = scsiGetDriveInfo(fd, &peripheral_type, con->driveinfo);
    if (res) {
        if (2 == res)
            return 0;
        else
            failuretest(MANDATORY_CMD, returnval |= FAILID);
    }

    if (con->smartenable) {
        if (scsiSmartEnable(fd))
            failuretest(MANDATORY_CMD, returnval |= FAILSMART);
    }

    if (con->smartdisable) {
        if (scsiSmartDisable(fd))
            failuretest(MANDATORY_CMD,returnval |= FAILSMART);
    }
    
    if (con->smartautosaveenable) {
      if (scsiSetControlGLTSD(fd, 0, modese_len)) {
        pout("Enable autosave (clear GLTSD bit) failed\n");
        failuretest(OPTIONAL_CMD,returnval |= FAILSMART);
      }
    }
    
    if (con->smartautosavedisable) {
      if (scsiSetControlGLTSD(fd, 1, modese_len)) {
        pout("Disable autosave (set GLTSD bit) failed\n");
        failuretest(OPTIONAL_CMD,returnval |= FAILSMART);
      }
    }
    
    if (con->checksmart) {
        scsiGetSupportedLogPages(fd);
        checkedSupportedLogPages = 1;
        if ((SCSI_PT_SEQUENTIAL_ACCESS == peripheral_type) ||
            (SCSI_PT_MEDIUM_CHANGER == peripheral_type)) { /* tape device */
            if (gTapeAlertsLPage) {
                if (con->driveinfo)
                    pout("TapeAlert Supported\n");
                if (-1 == scsiGetTapeAlertsData(fd, peripheral_type))
                    failuretest(OPTIONAL_CMD, returnval |= FAILSMART);
            }
            else
                pout("TapeAlert Not Supported\n");
        } else { /* disk, cd/dvd, enclosure, etc */
            if ((res = scsiGetSmartData(fd, con->smartvendorattrib))) {
                if (-2 == res)
                    returnval |= FAILSTATUS;
                else
                    returnval |= FAILSMART;
            }
        }
    }   
    if (con->smartvendorattrib) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(fd);
        if (gTempLPage) {
            if (con->checksmart)
                pout("\n");
            scsiPrintTemp(fd);         
        }
        if (gStartStopLPage)
            scsiGetStartStopData(fd);
        if (SCSI_PT_DIRECT_ACCESS == peripheral_type) {
            scsiPrintGrownDefectListLen(fd);
            if (gSeagateCacheLPage)
                scsiPrintSeagateCacheLPage(fd);
            if (gSeagateFactoryLPage)
                scsiPrintSeagateFactoryLPage(fd);
        }
    }
    if (con->smarterrorlog) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(fd);
        scsiPrintErrorCounterLog(fd);
        if (1 == scsiFetchControlGLTSD(fd, modese_len, 1))
            pout("\n[GLTSD (Global Logging Target Save Disable) set. "
                 "Enable Save with '-S on']\n");
    }
    if (con->smartselftestlog) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(fd);
        res = 0;
        if (gSelfTestLPage)
            res = scsiPrintSelfTest(fd);
        else {
            pout("Device does not support Self Test logging\n");
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        }
        if (0 != res)
            failuretest(OPTIONAL_CMD, returnval|=res);
    }
    if (con->smartexeoffimmediate) {
        if (scsiSmartDefaultSelfTest(fd))
            return returnval | FAILSMART;
        pout("Default Self Test Successful\n");
    }
    if (con->smartshortcapselftest) {
        if (scsiSmartShortCapSelfTest(fd))
            return returnval | FAILSMART;
        pout("Short Foreground Self Test Successful\n");
    }
    if (con->smartshortselftest ) { 
        if (scsiSmartShortSelfTest(fd))
            return returnval | FAILSMART;
        pout("Short Background Self Test has begun\n");
        pout("Use smartctl -X to abort test\n");
    }
    if (con->smartextendselftest) {
        if (scsiSmartExtendSelfTest(fd))
            return returnval | FAILSMART;
        pout("Extended Background Self Test has begun\n");
        if ((0 == scsiFetchExtendedSelfTestTime(fd, &durationSec, 
                        modese_len)) && (durationSec > 0)) {
            time_t t = time(NULL);

            t += durationSec;
            pout("Please wait %d minutes for test to complete.\n", 
                 durationSec / 60);
            pout("Estimated completion time: %s\n", ctime(&t));
        }
        pout("Use smartctl -X to abort test\n");        
    }
    if (con->smartextendcapselftest) {
        if (scsiSmartExtendCapSelfTest(fd))
            return returnval | FAILSMART;
        pout("Extended Foreground Self Test Successful\n");
    }
    if (con->smartselftestabort) {
        if (scsiSmartSelfTestAbort(fd))
            return returnval | FAILSMART;
        pout("Self Test returned without error\n");
    }           
    return returnval;
}
