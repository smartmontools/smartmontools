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

const char* scsiprint_c_cvsid="$Id: scsiprint.c,v 1.62 2003/11/17 11:54:32 dpgilbert Exp $"
EXTERN_H_CVSID SCSICMDS_H_CVSID SCSIPRINT_H_CVSID SMARTCTL_H_CVSID UTILITY_H_CVSID;

// control block which points to external global control variables
extern smartmonctrl *con;

// to hold onto exit code for atexit routine
extern int exitstatus;

UINT8 gBuf[GBUF_SIZE];
#define LOG_RESP_LEN 252
#define LOG_RESP_TAPE_ALERT_LEN 0x144

/* Log pages supported */
static int gSmartLPage = 0;     /* Informational Exceptions log page */
static int gTempLPage = 0;
static int gSelfTestLPage = 0;
static int gStartStopLPage = 0;
static int gTapeAlertsLPage = 0;
static int gSeagateCacheLPage = 0;
static int gSeagateFactoryLPage = 0;

/* Mode pages supported */
static int gIecMPage = 1;     /* N.B. assume it until we know otherwise */

/* Remember last successful mode sense/select command */
static int modese_len = 0;


// Compares failure type to policy in effect, and either exits or
// simply returns to the calling routine.
static void failuretest(int type, int returnvalue)
{
    // If this is an error in an "optional" SMART command
    if (type == OPTIONAL_CMD) {
        if (con->conservative) {
            pout("An optional SMART command has failed: exiting.\n"
                 "To continue, set the tolerance level to something other "
                 "than 'conservative'\n");
            EXIT(returnvalue);
        }
        return;
    }
    // If this is an error in a "mandatory" SMART command
    if (type==MANDATORY_CMD) {
        if (con->permissive)
            return;
        pout("A mandatory SMART command has failed: exiting. To continue, "
             "use the -T option to set the tolerance level to 'permissive'\n");
        exit(returnvalue);
    }
    pout("Smartctl internal error in failuretest(type=%d). Please contact "
         "%s\n",type,PROJECTHOME);
    exit(returnvalue|FAILCMD);
}

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

void scsiGetSmartData(int device)
{
    UINT8 asc;
    UINT8 ascq;
    UINT8 currenttemp = 0;
    UINT8 triptemp = 0;
    const char * cp;
    int err;

    QUIETON(con);
    if ((err = scsiCheckIE(device, gSmartLPage, gTempLPage, &asc, 
                           &ascq, &currenttemp, &triptemp))) {
        /* error message already announced */
        QUIETOFF(con);
        return;
    }
    QUIETOFF(con);
    cp = scsiGetIEString(asc, ascq);
    if (cp) {
        QUIETON(con);
        pout("SMART Health Status: %s [asc=%x,ascq=%x]\n", cp, asc, ascq); 
        QUIETOFF(con);
    } else if (gIecMPage)
        pout("SMART Health Status: OK\n");

    if (currenttemp && !gTempLPage) {
        if (255 != currenttemp)
            pout("Current Drive Temperature:     %d C\n", currenttemp);
        else
            pout("Current Drive Temperature:     <not available>\n");
    }
    if (triptemp && !gTempLPage)
        pout("Drive Trip Temperature:        %d C\n", triptemp);
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

    QUIETON(con);
    if ((err = scsiLogSense(device, TAPE_ALERTS_LPAGE, gBuf, 
                        LOG_RESP_TAPE_ALERT_LEN, LOG_RESP_TAPE_ALERT_LEN))) {
        pout("scsiGetTapesAlertData Failed [%s]\n", scsiErrString(err));
        QUIETOFF(con);
        return -1;
    }
    if (gBuf[0] != 0x2e) {
        pout("TapeAlerts Log Sense Failed\n");
        QUIETOFF(con);
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
    QUIETOFF(con);

    if (! failures)
        pout("TapeAlert: OK\n");

    return failures;
}

void scsiGetStartStopData(int device)
{
    UINT32 currentStartStop;
    UINT32 recommendedStartStop; 
    int err, len, k;
    char str[6];

    if ((err = scsiLogSense(device, STARTSTOP_CYCLE_COUNTER_LPAGE, gBuf,
                            LOG_RESP_LEN, 0))) {
        QUIETON(con);
        pout("scsiGetStartStopData Failed [%s]\n", scsiErrString(err));
        QUIETOFF(con);
        return;
    }
    if (gBuf[0] != STARTSTOP_CYCLE_COUNTER_LPAGE) {
        QUIETON(con);
        pout("StartStop Log Sense Failed, page mismatch\n");
        QUIETOFF(con);
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
        pout("Recommended maximum start stop count:  %u times\n", 
             recommendedStartStop);
    }
} 

static void scsiPrintSeagateCacheLPage(int device)
{
    int k, j, num, pl, pc, pcb, err, len;
    unsigned char * ucp;
    unsigned char * xp;
    unsigned long long ull;

    pout("\nVendor (Seagate) cache information\n");
    if ((err = scsiLogSense(device, SEAGATE_CACHE_LPAGE, gBuf,
                            LOG_RESP_LEN, 0))) {
        QUIETON(con);
        pout("scsiPrintSeagateCacheLPage Failed [%s]\n", scsiErrString(err));
        QUIETOFF(con);
        return;
    }
    if (gBuf[0] != SEAGATE_CACHE_LPAGE) {
        QUIETON(con);
        pout("Seagate Cache Log Sense Failed, page mismatch\n");
        QUIETOFF(con);
        return;
    }
    len = ((gBuf[2] << 8) | gBuf[3]) + 4;
    num = len - 4;
    ucp = &gBuf[0] + 4;
    while (num > 3) {
        pc = (ucp[0] << 8) | ucp[1];
        pcb = ucp[2];
        pl = ucp[3] + 4;
        switch (pc) {
        case 0: pout("  Blocks sent to initiator"); break;
        case 1: pout("  Blocks received from initiator"); break;
        case 2: pout("  Blocks read from cache and sent to initiator"); break;
        case 3: pout("  Number of read and write commands whose size "
                       "<= segment size"); break;
        case 4: pout("  Number of read and write commands whose size "
                       "> segment size"); break;
        default: pout("  Unknown Seagate parameter code = 0x%x", pc); break;
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
        pout(" = %llu\n", ull);
        num -= pl;
        ucp += pl;
    }
}

static void scsiPrintSeagateFactoryLPage(int device)
{
    int k, j, num, pl, pc, pcb, len, err;
    unsigned char * ucp;
    unsigned char * xp;
    unsigned long long ull;

    pout("Vendor (Seagate) factory information\n");
    if ((err = scsiLogSense(device, SEAGATE_FACTORY_LPAGE, gBuf,
                            LOG_RESP_LEN, 0))) {
        QUIETON(con);
        pout("scsiPrintSeagateFactoryLPage Failed [%s]\n", scsiErrString(err));
        QUIETOFF(con);
        return;
    }
    if (gBuf[0] != SEAGATE_FACTORY_LPAGE) {
        QUIETON(con);
        pout("Seagate Factory Log Sense Failed, page mismatch\n");
        QUIETOFF(con);
        return;
    }
    len = ((gBuf[2] << 8) | gBuf[3]) + 4;
    num = len - 4;
    ucp = &gBuf[0] + 4;
    while (num > 3) {
        pc = (ucp[0] << 8) | ucp[1];
        pcb = ucp[2];
        pl = ucp[3] + 4;
        switch (pc) {
        case 0: pout("  number of hours powered up"); break;
        case 8: pout("  number of minutes until next internal SMART test");
            break;
        default: pout("  Unknown Seagate parameter code = 0x%x", pc); break;
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
        if (0 == pc)
            pout(" = %.2f\n", ((double)ull) / 60.0 );
        else
            pout(" = %llu\n", ull);
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

    if (0 == scsiLogSense(device, READ_ERROR_COUNTER_LPAGE, gBuf, 
                          LOG_RESP_LEN, 0)) {
        scsiDecodeErrCounterPage(gBuf, &errCounterArr[0]);
        found[0] = 1;
    }
    if (0 == scsiLogSense(device, WRITE_ERROR_COUNTER_LPAGE, gBuf, 
                          LOG_RESP_LEN, 0)) {
        scsiDecodeErrCounterPage(gBuf, &errCounterArr[1]);
        found[1] = 1;
    }
    if (0 == scsiLogSense(device, VERIFY_ERROR_COUNTER_LPAGE, gBuf, 
                          LOG_RESP_LEN, 0)) {
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
        pout("          Errors Corrected    Total      Total   "
             "Correction     Gigabytes    Total\n");
        pout("              delay:       [rereads/    errors   "
             "algorithm      processed    uncorrected\n");
        pout("            minor | major  rewrites]  corrected  "
             "invocations   [10^9 bytes]  errors\n");
        for (k = 0; k < 3; ++k) {
            if (! found[k])
                continue;
            ecp = &errCounterArr[k];
            pout("%s%8llu %8llu  %8llu  %8llu   %8llu", 
                 pageNames[k], ecp->counter[0], ecp->counter[1], 
                 ecp->counter[2], ecp->counter[3], ecp->counter[4]);
            processed_gb = ecp->counter[5] / 1000000000.0;
            pout("   %12.3f    %8llu\n", processed_gb, ecp->counter[6]);
        }
    }
    else 
        pout("\nDevice does not support Error Counter logging\n");
    if (0 == scsiLogSense(device, NON_MEDIUM_ERROR_LPAGE, gBuf, 
                          LOG_RESP_LEN, 0)) {
        scsiDecodeNonMediumErrPage(gBuf, &nme);
        if (nme.gotPC0)
            pout("\nNon-medium error count: %8llu\n", nme.counterPC0);
    }
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
static int scsiPrintSelfTest(int device)
{
    int num, k, n, res, err, durationSec;
    int noheader = 1;
    UINT8 * ucp;
    unsigned long long ull=0;

    if ((err = scsiLogSense(device, SELFTEST_RESULTS_LPAGE, gBuf, 
                            LOG_RESP_SELF_TEST_LEN, 0))) {
        QUIETON(con);
        pout("scsiPrintSelfTest Failed [%s]\n", scsiErrString(err));
        QUIETOFF(con);
        return 1;
    }
    if (gBuf[0] != SELFTEST_RESULTS_LPAGE) {
        QUIETON(con);
        pout("Self-test Log Sense Failed, page mismatch\n");
        QUIETOFF(con);
        return 1;
    }
    // compute page length
    num = (gBuf[2] << 8) + gBuf[3];
    // Log sense page length 0x190 bytes
    if (num != 0x190) {
        QUIETON(con);
        pout("Self-test Log Sense length is 0x%x not 0x190 bytes\n",num);
        QUIETOFF(con);
        return 1;
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
        pout("No self-tests have been logged\n");
    else
        pout("\n");
    if ((0 == scsiFetchExtendedSelfTestTime(device, &durationSec, 
                        modese_len)) && (durationSec > 0)) {
        pout("Long (extended) Self Test duration: %d seconds "
             "[%.1f minutes]\n", durationSec, durationSec / 60.0);
    }
    return 0;
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
 
/* Returns 0 on success */
static int scsiGetDriveInfo(int device, UINT8 * peripheral_type, int all)
{
    char manufacturer[9];
    char product[17];
    char revision[5];
    char timedatetz[64];
    struct scsi_iec_mode_page iec;
    int err, iec_err, len, val;
    int is_tape = 0;
    int peri_dt = 0;
        
    memset(gBuf, 0, 36);
    if ((err = scsiStdInquiry(device, gBuf, 36))) {
        QUIETON(con);
        pout("Standard Inquiry failed [%s]\n", scsiErrString(err));
        QUIETOFF(con);
        return 1;
    }
    len = gBuf[4] + 5;
    peri_dt = gBuf[0] & 0x1f;
    if (peripheral_type)
        *peripheral_type = peri_dt;
    if (! all)
	return 0;

    if (len < 36) {
        QUIETON(con);
        pout("Short INQUIRY response, skip product id\n");
        QUIETOFF(con);
        return 1;
    }
    memset(manufacturer, 0, sizeof(manufacturer));
    strncpy(manufacturer, (char *)&gBuf[8], 8);
 
    memset(product, 0, sizeof(product));
    strncpy(product, (char *)&gBuf[16], 16);
        
    memset(revision, 0, sizeof(revision));
    strncpy(revision, (char *)&gBuf[32], 4);
    pout("Device: %s %s Version: %s\n", manufacturer, product, revision);

    /* Do this here to try and detect badly conforming devices (some USB
       keys) that will lock up on a InquiryVpd or log sense or ... */
    if ((iec_err = scsiFetchIECmpage(device, &iec, modese_len))) {
        if (SIMPLE_ERR_BAD_RESP == iec_err) {
            pout(">> Terminate command early due to bad response to IEC "
                 "mode page\n");
            QUIETOFF(con);
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
        QUIETON(con);
        if (SIMPLE_ERR_BAD_RESP == err)
            pout("Vital Product Data (VPD) bit ignored in INQUIRY\n");
        else
            pout("Vital Product Data (VPD) INQUIRY failed [%d]\n", err);
        QUIETOFF(con);
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
            QUIETON(con);
            pout("device is NOT READY (media absent, spun down, etc)\n");
            QUIETOFF(con);
        } else {
            QUIETON(con);
            pout("device Test Unit Ready  [%s]\n", scsiErrString(err));
            QUIETOFF(con);
        }
	return 0;
    }
   
    if (iec_err) {
	if (!is_tape) {
            QUIETON(con);
	    pout("Device does not support SMART");
            if (con->reportscsiioctl > 0)
	        pout(" [%s]\n", scsiErrString(iec_err));
            else
	        pout("\n");
            QUIETOFF(con);
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
        QUIETON(con);
        pout("unable to fetch IEC (SMART) mode page [%s]\n", 
             scsiErrString(err));
        QUIETOFF(con);
        return 1;
    } else
        modese_len = iec.modese_len;

    if ((err = scsiSetExceptionControlAndWarning(device, 1, &iec))) {
        QUIETON(con);
        pout("unable to enable Exception control and warning [%s]\n",
             scsiErrString(err));
        QUIETOFF(con);
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
        QUIETON(con);
        pout("unable to fetch IEC (SMART) mode page [%s]\n", 
             scsiErrString(err));
        QUIETOFF(con);
        return 1;
    } else
        modese_len = iec.modese_len;

    if ((err = scsiSetExceptionControlAndWarning(device, 0, &iec))) {
        QUIETON(con);
        pout("unable to disable Exception control and warning [%s]\n",
             scsiErrString(err));
        QUIETOFF(con);
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

void scsiPrintTemp(int device)
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
    int returnval=0;
    int res, durationSec;

    if (scsiGetDriveInfo(fd, &peripheral_type, con->driveinfo)) {
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
            if (gTempLPage)
                scsiPrintTemp(fd);         
            if (gStartStopLPage)
                scsiGetStartStopData(fd);
        } else { /* disk, cd/dvd, enclosure, etc */
            scsiGetSmartData(fd);
            if (gTempLPage)
                scsiPrintTemp(fd);         
            if (gStartStopLPage)
                scsiGetStartStopData(fd);
        }
    }   
    if (con->smartvendorattrib) {
        if (gSeagateCacheLPage)
            scsiPrintSeagateCacheLPage(fd);
        if (gSeagateFactoryLPage)
            scsiPrintSeagateFactoryLPage(fd);
    }
    if (con->smarterrorlog) {
        scsiPrintErrorCounterLog(fd);
        if (1 == scsiFetchControlGLTSD(fd, modese_len))
            pout("Warning: log page contents are potentially being reset"
                 " at each power up\n         [Control mode page, GLTSD "
                 "(global logging target save disable) set]\n");
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
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
    }
    if (con->smartexeoffimmediate) {
        if (scsiSmartDefaultSelfTest(fd)) {
            pout( "Default Self Test Failed\n");
            return returnval;
        }
        pout("Default Self Test Successful\n");
    }
    if (con->smartshortcapselftest) {
        if (scsiSmartShortCapSelfTest(fd)) {
            pout("Short Foreground Self Test Failed\n");
            return returnval;
        }
        pout("Short Foreground Self Test Successful\n");
    }
    if (con->smartshortselftest ) { 
        if ( scsiSmartShortSelfTest(fd)) {
            pout("Short Background Self Test Failed\n");
            return returnval;
        }
        pout("Short Background Self Test has begun\n");
        pout("Use smartctl -X to abort test\n");
    }
    if (con->smartextendselftest) {
        if (scsiSmartExtendSelfTest(fd)) {
            pout("Extended Background Self Test Failed\n");
            return returnval;
        }
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
        if (scsiSmartExtendCapSelfTest(fd)) {
            pout("Extended Foreground Self Test Failed\n");
            return returnval;
        }
        pout("Extended Foreground Self Test Successful\n");
    }
    if (con->smartselftestabort) {
        if (scsiSmartSelfTestAbort(fd)) {
            pout("Self Test Abort Failed\n");
            return returnval;
        }
        pout("Self Test returned without error\n");
    }           
    return returnval;
}
