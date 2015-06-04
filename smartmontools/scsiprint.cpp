/*
 * scsiprint.cpp
 *
 * Home page of code is: http://smartmontools.sourceforge.net
 *
 * Copyright (C) 2002-11 Bruce Allen <smartmontools-support@lists.sourceforge.net>
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 *
 * Additional SCSI work:
 * Copyright (C) 2003-13 Douglas Gilbert <dgilbert@interlog.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * (for example COPYING); if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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
#include "scsicmds.h"
#include "atacmds.h" // smart_command_set
#include "dev_interface.h"
#include "scsiprint.h"
#include "smartctl.h"
#include "utility.h"

#define GBUF_SIZE 65535

const char * scsiprint_c_cvsid = "$Id$"
                                 SCSIPRINT_H_CVSID;


UINT8 gBuf[GBUF_SIZE];
#define LOG_RESP_LEN 252
#define LOG_RESP_LONG_LEN ((62 * 256) + 252)
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
static int gBackgroundResultsLPage = 0;
static int gProtocolSpecificLPage = 0;
static int gTapeAlertsLPage = 0;
static int gSSMediaLPage = 0;

/* Vendor specific log pages */
static int gSeagateCacheLPage = 0;
static int gSeagateFactoryLPage = 0;

/* Mode pages supported */
static int gIecMPage = 1;     /* N.B. assume it until we know otherwise */

/* Remember last successful mode sense/select command */
static int modese_len = 0;


static void
scsiGetSupportedLogPages(scsi_device * device)
{
    int i, err;

    if ((err = scsiLogSense(device, SUPPORTED_LPAGES, 0, gBuf,
                            LOG_RESP_LEN, 0))) {
        if (scsi_debugmode > 0)
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
            case BACKGROUND_RESULTS_LPAGE:
                gBackgroundResultsLPage = 1;
                break;
            case PROTOCOL_SPECIFIC_LPAGE:
                gProtocolSpecificLPage = 1;
                break;
            case TAPE_ALERTS_LPAGE:
                gTapeAlertsLPage = 1;
                break;
            case SS_MEDIA_LPAGE:
                gSSMediaLPage = 1;
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
static int
scsiGetSmartData(scsi_device * device, bool attribs)
{
    UINT8 asc;
    UINT8 ascq;
    UINT8 currenttemp = 0;
    UINT8 triptemp = 0;
    const char * cp;
    int err = 0;
    print_on();
    if (scsiCheckIE(device, gSmartLPage, gTempLPage, &asc, &ascq,
                    &currenttemp, &triptemp)) {
        /* error message already announced */
        print_off();
        return -1;
    }
    print_off();
    cp = scsiGetIEString(asc, ascq);
    if (cp) {
        err = -2;
        print_on();
        pout("SMART Health Status: %s [asc=%x, ascq=%x]\n", cp, asc, ascq);
        print_off();
    } else if (gIecMPage)
        pout("SMART Health Status: OK\n");

    if (attribs && !gTempLPage) {
        if (currenttemp) {
            if (255 != currenttemp)
                pout("Current Drive Temperature:     %d C\n", currenttemp);
            else
                pout("Current Drive Temperature:     <not available>\n");
        }
        if (triptemp)
            pout("Drive Trip Temperature:        %d C\n", triptemp);
    }
    pout("\n");
    return err;
}


// Returns number of logged errors or zero if none or -1 if fetching
// TapeAlerts fails
static const char * const severities = "CWI";

static int
scsiGetTapeAlertsData(scsi_device * device, int peripheral_type)
{
    unsigned short pagelength;
    unsigned short parametercode;
    int i, err;
    const char *s;
    const char *ts;
    int failures = 0;

    print_on();
    if ((err = scsiLogSense(device, TAPE_ALERTS_LPAGE, 0, gBuf,
                        LOG_RESP_TAPE_ALERT_LEN, LOG_RESP_TAPE_ALERT_LEN))) {
        pout("scsiGetTapesAlertData Failed [%s]\n", scsiErrString(err));
        print_off();
        return -1;
    }
    if (gBuf[0] != 0x2e) {
        pout("TapeAlerts Log Sense Failed\n");
        print_off();
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
                        pout("TapeAlert Errors (C=Critical, W=Warning, "
                             "I=Informational):\n");
                    pout("[0x%02x] %s\n", parametercode, ts);
                    failures += 1;
                }
            }
        }
    }
    print_off();

    if (! failures)
        pout("TapeAlert: OK\n");

    return failures;
}

static void
scsiGetStartStopData(scsi_device * device)
{
    UINT32 u;
    int err, len, k, extra, pc;
    unsigned char * ucp;

    if ((err = scsiLogSense(device, STARTSTOP_CYCLE_COUNTER_LPAGE, 0, gBuf,
                            LOG_RESP_LEN, 0))) {
        print_on();
        pout("scsiGetStartStopData Failed [%s]\n", scsiErrString(err));
        print_off();
        return;
    }
    if ((gBuf[0] & 0x3f) != STARTSTOP_CYCLE_COUNTER_LPAGE) {
        print_on();
        pout("StartStop Log Sense Failed, page mismatch\n");
        print_off();
        return;
    }
    len = ((gBuf[2] << 8) | gBuf[3]);
    ucp = gBuf + 4;
    for (k = len; k > 0; k -= extra, ucp += extra) {
        if (k < 3) {
            print_on();
            pout("StartStop Log Sense Failed: short\n");
            print_off();
            return;
        }
        extra = ucp[3] + 4;
        pc = (ucp[0] << 8) + ucp[1];
        switch (pc) {
        case 1:
            if (10 == extra)
                pout("Manufactured in week %.2s of year %.4s\n", ucp + 8,
                     ucp + 4);
            break;
        case 2:
            /* ignore Accounting date */
            break;
        case 3:
            if (extra > 7) {
                u = (ucp[4] << 24) | (ucp[5] << 16) | (ucp[6] << 8) | ucp[7];
                if (0xffffffff != u)
                    pout("Specified cycle count over device lifetime:  %u\n",
                         u);
            }
            break;
        case 4:
            if (extra > 7) {
                u = (ucp[4] << 24) | (ucp[5] << 16) | (ucp[6] << 8) | ucp[7];
                if (0xffffffff != u)
                    pout("Accumulated start-stop cycles:  %u\n", u);
            }
            break;
        case 5:
            if (extra > 7) {
                u = (ucp[4] << 24) | (ucp[5] << 16) | (ucp[6] << 8) | ucp[7];
                if (0xffffffff != u)
                    pout("Specified load-unload count over device "
                         "lifetime:  %u\n", u);
            }
            break;
        case 6:
            if (extra > 7) {
                u = (ucp[4] << 24) | (ucp[5] << 16) | (ucp[6] << 8) | ucp[7];
                if (0xffffffff != u)
                    pout("Accumulated load-unload cycles:  %u\n", u);
            }
            break;
        default:
            /* ignore */
            break;
        }
    }
}

static void
scsiPrintGrownDefectListLen(scsi_device * device)
{
    int err, dl_format, got_rd12, generation;
    unsigned int dl_len, div;

    memset(gBuf, 0, 8);
    if ((err = scsiReadDefect12(device, 0 /* req_plist */, 1 /* req_glist */,
                                4 /* format: bytes from index */,
                                0 /* addr desc index */, gBuf, 8))) {
        if (2 == err) { /* command not supported */
            if ((err = scsiReadDefect10(device, 0 /* req_plist */, 1 /* req_glist */,
                                        4 /* format: bytes from index */, gBuf, 4))) {
                if (scsi_debugmode > 0) {
                    print_on();
                    pout("Read defect list (10) Failed: %s\n", scsiErrString(err));
                    print_off();
                }
                return;
            } else
                got_rd12 = 0;
        } else if (101 == err)    /* Defect list not found, leave quietly */
            return;
        else {
            if (scsi_debugmode > 0) {
                print_on();
                pout("Read defect list (12) Failed: %s\n", scsiErrString(err));
                print_off();
            }
            return;
        }
    } else
        got_rd12 = 1;

    if (got_rd12) {
        generation = (gBuf[2] << 8) + gBuf[3];
        if ((generation > 1) && (scsi_debugmode > 0)) {
            print_on();
            pout("Read defect list (12): generation=%d\n", generation);
            print_off();
        }
        dl_len = (gBuf[4] << 24) + (gBuf[5] << 16) + (gBuf[6] << 8) + gBuf[7];
    } else {
        dl_len = (gBuf[2] << 8) + gBuf[3];
    }
    if (0x8 != (gBuf[1] & 0x18)) {
        print_on();
        pout("Read defect list: asked for grown list but didn't get it\n");
        print_off();
        return;
    }
    div = 0;
    dl_format = (gBuf[1] & 0x7);
    switch (dl_format) {
        case 0:     /* short block */
            div = 4;
            break;
        case 1:     /* extended bytes from index */
        case 2:     /* extended physical sector */
            /* extended = 1; # might use in future */
            div = 8;
            break;
        case 3:     /* long block */
        case 4:     /* bytes from index */
        case 5:     /* physical sector */
            div = 8;
            break;
        default:
            print_on();
            pout("defect list format %d unknown\n", dl_format);
            print_off();
            break;
    }
    if (0 == dl_len)
        pout("Elements in grown defect list: 0\n\n");
    else {
        if (0 == div)
            pout("Grown defect list length=%u bytes [unknown "
                 "number of elements]\n\n", dl_len);
        else
            pout("Elements in grown defect list: %u\n\n", dl_len / div);
    }
}

static void
scsiPrintSeagateCacheLPage(scsi_device * device)
{
    int k, j, num, pl, pc, err, len;
    unsigned char * ucp;
    unsigned char * xp;
    uint64_t ull;

    if ((err = scsiLogSense(device, SEAGATE_CACHE_LPAGE, 0, gBuf,
                            LOG_RESP_LEN, 0))) {
        print_on();
        pout("Seagate Cache Log Sense Failed: %s\n", scsiErrString(err));
        print_off();
        return;
    }
    if ((gBuf[0] & 0x3f) != SEAGATE_CACHE_LPAGE) {
        print_on();
        pout("Seagate Cache Log Sense Failed, page mismatch\n");
        print_off();
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
            if (scsi_debugmode > 0) {
                print_on();
                pout("Vendor (Seagate) cache lpage has unexpected parameter"
                     ", skip\n");
                print_off();
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
        pout(" = %" PRIu64 "\n", ull);
        num -= pl;
        ucp += pl;
    }
    pout("\n");
}

static void
scsiPrintSeagateFactoryLPage(scsi_device * device)
{
    int k, j, num, pl, pc, len, err, good, bad;
    unsigned char * ucp;
    unsigned char * xp;
    uint64_t ull;

    if ((err = scsiLogSense(device, SEAGATE_FACTORY_LPAGE, 0, gBuf,
                            LOG_RESP_LEN, 0))) {
        print_on();
        pout("scsiPrintSeagateFactoryLPage Failed [%s]\n", scsiErrString(err));
        print_off();
        return;
    }
    if ((gBuf[0] & 0x3f) != SEAGATE_FACTORY_LPAGE) {
        print_on();
        pout("Seagate/Hitachi Factory Log Sense Failed, page mismatch\n");
        print_off();
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
        if (scsi_debugmode > 0) {
            print_on();
            pout("\nVendor (Seagate/Hitachi) factory lpage has too many "
                 "unexpected parameters, skip\n");
            print_off();
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
            if (scsi_debugmode > 0) {
                print_on();
                pout("Vendor (Seagate/Hitachi) factory lpage: "
                     "unknown parameter code [0x%x]\n", pc);
                print_off();
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
                pout(" = %.2f\n", ull / 60.0 );
            else
                pout(" = %" PRIu64 "\n", ull);
        }
        num -= pl;
        ucp += pl;
    }
    pout("\n");
}

static void
scsiPrintErrorCounterLog(scsi_device * device)
{
    struct scsiErrorCounter errCounterArr[3];
    struct scsiErrorCounter * ecp;
    struct scsiNonMediumError nme;
    int found[3] = {0, 0, 0};
    const char * pageNames[3] = {"read:   ", "write:  ", "verify: "};
    double processed_gb;

    if (gReadECounterLPage && (0 == scsiLogSense(device,
                READ_ERROR_COUNTER_LPAGE, 0, gBuf, LOG_RESP_LEN, 0))) {
        scsiDecodeErrCounterPage(gBuf, &errCounterArr[0]);
        found[0] = 1;
    }
    if (gWriteECounterLPage && (0 == scsiLogSense(device,
                WRITE_ERROR_COUNTER_LPAGE, 0, gBuf, LOG_RESP_LEN, 0))) {
        scsiDecodeErrCounterPage(gBuf, &errCounterArr[1]);
        found[1] = 1;
    }
    if (gVerifyECounterLPage && (0 == scsiLogSense(device,
                VERIFY_ERROR_COUNTER_LPAGE, 0, gBuf, LOG_RESP_LEN, 0))) {
        scsiDecodeErrCounterPage(gBuf, &errCounterArr[2]);
        ecp = &errCounterArr[2];
        for (int k = 0; k < 7; ++k) {
            if (ecp->gotPC[k] && ecp->counter[k]) {
                found[2] = 1;
                break;
            }
        }
    }
    if (found[0] || found[1] || found[2]) {
        pout("Error counter log:\n");
        pout("           Errors Corrected by           Total   "
             "Correction     Gigabytes    Total\n");
        pout("               ECC          rereads/    errors   "
             "algorithm      processed    uncorrected\n");
        pout("           fast | delayed   rewrites  corrected  "
             "invocations   [10^9 bytes]  errors\n");
        for (int k = 0; k < 3; ++k) {
            if (! found[k])
                continue;
            ecp = &errCounterArr[k];
            pout("%s%8" PRIu64 " %8" PRIu64 "  %8" PRIu64 "  %8" PRIu64 "   %8" PRIu64,
                 pageNames[k], ecp->counter[0], ecp->counter[1],
                 ecp->counter[2], ecp->counter[3], ecp->counter[4]);
            processed_gb = ecp->counter[5] / 1000000000.0;
            pout("   %12.3f    %8" PRIu64 "\n", processed_gb, ecp->counter[6]);
        }
    }
    else
        pout("Error Counter logging not supported\n");
    if (gNonMediumELPage && (0 == scsiLogSense(device,
                NON_MEDIUM_ERROR_LPAGE, 0, gBuf, LOG_RESP_LEN, 0))) {
        scsiDecodeNonMediumErrPage(gBuf, &nme);
        if (nme.gotPC0)
            pout("\nNon-medium error count: %8" PRIu64 "\n", nme.counterPC0);
        if (nme.gotTFE_H)
            pout("Track following error count [Hitachi]: %8" PRIu64 "\n",
                 nme.counterTFE_H);
        if (nme.gotPE_H)
            pout("Positioning error count [Hitachi]: %8" PRIu64 "\n",
                 nme.counterPE_H);
    }
    if (gLastNErrorLPage && (0 == scsiLogSense(device,
                LAST_N_ERROR_LPAGE, 0, gBuf, LOG_RESP_LONG_LEN, 0))) {
        int num = (gBuf[2] << 8) + gBuf[3] + 4;
        int truncated = (num > LOG_RESP_LONG_LEN) ? num : 0;
        if (truncated)
            num = LOG_RESP_LONG_LEN;
        unsigned char * ucp = gBuf + 4;
        num -= 4;
        if (num < 4)
            pout("\nNo error events logged\n");
        else {
            pout("\nLast n error events log page\n");
            for (int k = num, pl; k > 0; k -= pl, ucp += pl) {
                if (k < 3) {
                    pout("  <<short Last n error events log page>>\n");
                    break;
                }
                pl = ucp[3] + 4;
                int pc = (ucp[0] << 8) + ucp[1];
                if (pl > 4) {
                    if ((ucp[2] & 0x1) && (ucp[2] & 0x2)) {
                        pout("  Error event %d:\n", pc);
                        pout("    [binary]:\n");
                        dStrHex((const char *)ucp + 4, pl - 4, 1);
                    } else if (ucp[2] & 0x1) {
                        pout("  Error event %d:\n", pc);
                        pout("    %.*s\n", pl - 4, (const char *)(ucp + 4));
                    } else {
                        if (scsi_debugmode > 0) {
                            pout("  Error event %d:\n", pc);
                            pout("    [data counter??]:\n");
                            dStrHex((const char *)ucp + 4, pl - 4, 1);
                        }
                    }
                }
            }
            if (truncated)
                pout(" >>>> log truncated, fetched %d of %d available "
                     "bytes\n", LOG_RESP_LONG_LEN, truncated);
        }
    }
    pout("\n");
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
        "Aborted (by user command)",
        "Aborted (device reset ?) ",
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
static int
scsiPrintSelfTest(scsi_device * device)
{
    int num, k, n, res, err, durationSec;
    int noheader = 1;
    int retval = 0;
    UINT8 * ucp;
    uint64_t ull=0;
    struct scsi_sense_disect sense_info;

    // check if test is running
    if (!scsiRequestSense(device, &sense_info) &&
                        (sense_info.asc == 0x04 && sense_info.ascq == 0x09 &&
                        sense_info.progress != -1)) {
        pout("Self-test execution status:\t\t%d%% of test remaining\n",
             100 - ((sense_info.progress * 100) / 65535));
    }

    if ((err = scsiLogSense(device, SELFTEST_RESULTS_LPAGE, 0, gBuf,
                            LOG_RESP_SELF_TEST_LEN, 0))) {
        print_on();
        pout("scsiPrintSelfTest Failed [%s]\n", scsiErrString(err));
        print_off();
        return FAILSMART;
    }
    if ((gBuf[0] & 0x3f) != SELFTEST_RESULTS_LPAGE) {
        print_on();
        pout("Self-test Log Sense Failed, page mismatch\n");
        print_off();
        return FAILSMART;
    }
    // compute page length
    num = (gBuf[2] << 8) + gBuf[3];
    // Log sense page length 0x190 bytes
    if (num != 0x190) {
        print_on();
        pout("Self-test Log Sense length is 0x%x not 0x190 bytes\n",num);
        print_off();
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
            pout("SMART Self-test log\n");
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
            snprintf(buff, sizeof(buff), "%" PRIu64, ull);
            // snprintf(buff, sizeof(buff), "0x%" PRIx64, ull);
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
    if ((0 == scsiFetchExtendedSelfTestTime(device, &durationSec,
                        modese_len)) && (durationSec > 0)) {
        pout("\nLong (extended) Self Test duration: %d seconds "
             "[%.1f minutes]\n", durationSec, durationSec / 60.0);
    }
    pout("\n");
    return retval;
}

static const char * bms_status[] = {
    "no scans active",
    "scan is active",
    "pre-scan is active",
    "halted due to fatal error",
    "halted due to a vendor specific pattern of error",
    "halted due to medium formatted without P-List",
    "halted - vendor specific cause",
    "halted due to temperature out of range",
    "waiting until BMS interval timer expires", /* 8 */
};

static const char * reassign_status[] = {
    "Reserved [0x0]",
    "Require Write or Reassign Blocks command",
    "Successfully reassigned",
    "Reserved [0x3]",
    "Reassignment by disk failed",
    "Recovered via rewrite in-place",
    "Reassigned by app, has valid data",
    "Reassigned by app, has no valid data",
    "Unsuccessfully reassigned by app", /* 8 */
};

// See SCSI Block Commands - 3 (SBC-3) rev 6 (draft) section 6.2.2 .
// Returns 0 if ok else FAIL* bitmask. Note can have a status entry
// and up to 2048 events (although would hope to have less). May set
// FAILLOG if serious errors detected (in the future).
static int
scsiPrintBackgroundResults(scsi_device * device)
{
    int num, j, m, err, pc, pl, truncated;
    int noheader = 1;
    int firstresult = 1;
    int retval = 0;
    UINT8 * ucp;

    if ((err = scsiLogSense(device, BACKGROUND_RESULTS_LPAGE, 0, gBuf,
                            LOG_RESP_LONG_LEN, 0))) {
        print_on();
        pout("scsiPrintBackgroundResults Failed [%s]\n", scsiErrString(err));
        print_off();
        return FAILSMART;
    }
    if ((gBuf[0] & 0x3f) != BACKGROUND_RESULTS_LPAGE) {
        print_on();
        pout("Background scan results Log Sense Failed, page mismatch\n");
        print_off();
        return FAILSMART;
    }
    // compute page length
    num = (gBuf[2] << 8) + gBuf[3] + 4;
    if (num < 20) {
        print_on();
        pout("Background scan results Log Sense length is %d, no scan "
             "status\n", num);
        print_off();
        return FAILSMART;
    }
    truncated = (num > LOG_RESP_LONG_LEN) ? num : 0;
    if (truncated)
        num = LOG_RESP_LONG_LEN;
    ucp = gBuf + 4;
    num -= 4;
    while (num > 3) {
        pc = (ucp[0] << 8) | ucp[1];
        // pcb = ucp[2];
        pl = ucp[3] + 4;
        switch (pc) {
        case 0:
            if (noheader) {
                noheader = 0;
                pout("Background scan results log\n");
            }
            pout("  Status: ");
            if ((pl < 16) || (num < 16)) {
                pout("\n");
                break;
            }
            j = ucp[9];
            if (j < (int)(sizeof(bms_status) / sizeof(bms_status[0])))
                pout("%s\n", bms_status[j]);
            else
                pout("unknown [0x%x] background scan status value\n", j);
            j = (ucp[4] << 24) + (ucp[5] << 16) + (ucp[6] << 8) + ucp[7];
            pout("    Accumulated power on time, hours:minutes %d:%02d "
                 "[%d minutes]\n", (j / 60), (j % 60), j);
            pout("    Number of background scans performed: %d,  ",
                 (ucp[10] << 8) + ucp[11]);
            pout("scan progress: %.2f%%\n",
                 (double)((ucp[12] << 8) + ucp[13]) * 100.0 / 65536.0);
            pout("    Number of background medium scans performed: %d\n",
                 (ucp[14] << 8) + ucp[15]);
            break;
        default:
            if (noheader) {
                noheader = 0;
                pout("\nBackground scan results log\n");
            }
            if (firstresult) {
                firstresult = 0;
                pout("\n   #  when        lba(hex)    [sk,asc,ascq]    "
                     "reassign_status\n");
            }
            pout(" %3d ", pc);
            if ((pl < 24) || (num < 24)) {
                if (pl < 24)
                    pout("parameter length >= 24 expected, got %d\n", pl);
                break;
            }
            j = (ucp[4] << 24) + (ucp[5] << 16) + (ucp[6] << 8) + ucp[7];
            pout("%4d:%02d  ", (j / 60), (j % 60));
            for (m = 0; m < 8; ++m)
                pout("%02x", ucp[16 + m]);
            pout("  [%x,%x,%x]   ", ucp[8] & 0xf, ucp[9], ucp[10]);
            j = (ucp[8] >> 4) & 0xf;
            if (j <
                (int)(sizeof(reassign_status) / sizeof(reassign_status[0])))
                pout("%s\n", reassign_status[j]);
            else
                pout("Reassign status: reserved [0x%x]\n", j);
            break;
        }
        num -= pl;
        ucp += pl;
    }
    if (truncated)
        pout(" >>>> log truncated, fetched %d of %d available "
             "bytes\n", LOG_RESP_LONG_LEN, truncated);
    pout("\n");
    return retval;
}

// See SCSI Block Commands - 3 (SBC-3) rev 27 (draft) section 6.3.6 .
// Returns 0 if ok else FAIL* bitmask. Note can have a status entry
// and up to 2048 events (although would hope to have less). May set
// FAILLOG if serious errors detected (in the future).
static int
scsiPrintSSMedia(scsi_device * device)
{
    int num, err, pc, pl, truncated;
    int retval = 0;
    UINT8 * ucp;

    if ((err = scsiLogSense(device, SS_MEDIA_LPAGE, 0, gBuf,
                            LOG_RESP_LONG_LEN, 0))) {
        print_on();
        pout("scsiPrintSSMedia Failed [%s]\n", scsiErrString(err));
        print_off();
        return FAILSMART;
    }
    if ((gBuf[0] & 0x3f) != SS_MEDIA_LPAGE) {
        print_on();
        pout("Solid state media Log Sense Failed, page mismatch\n");
        print_off();
        return FAILSMART;
    }
    // compute page length
    num = (gBuf[2] << 8) + gBuf[3] + 4;
    if (num < 12) {
        print_on();
        pout("Solid state media Log Sense length is %d, too short\n", num);
        print_off();
        return FAILSMART;
    }
    truncated = (num > LOG_RESP_LONG_LEN) ? num : 0;
    if (truncated)
        num = LOG_RESP_LONG_LEN;
    ucp = gBuf + 4;
    num -= 4;
    while (num > 3) {
        pc = (ucp[0] << 8) | ucp[1];
        // pcb = ucp[2];
        pl = ucp[3] + 4;
        switch (pc) {
        case 1:
            if (pl < 8) {
                print_on();
                pout("SS Media Percentage used endurance indicator parameter "
                     "too short (pl=%d)\n", pl);
                print_off();
                return FAILSMART;
            }
            pout("Percentage used endurance indicator: %d%%\n", ucp[7]);
        default:        /* ignore other parameter codes */
            break;
        }
        num -= pl;
        ucp += pl;
    }
    return retval;
}

static void
show_sas_phy_event_info(int peis, unsigned int val, unsigned thresh_val)
{
    unsigned int u;

    switch (peis) {
    case 0:
        pout("     No event\n");
        break;
    case 0x1:
        pout("     Invalid word count: %u\n", val);
        break;
    case 0x2:
        pout("     Running disparity error count: %u\n", val);
        break;
    case 0x3:
        pout("     Loss of dword synchronization count: %u\n", val);
        break;
    case 0x4:
        pout("     Phy reset problem count: %u\n", val);
        break;
    case 0x5:
        pout("     Elasticity buffer overflow count: %u\n", val);
        break;
    case 0x6:
        pout("     Received ERROR  count: %u\n", val);
        break;
    case 0x20:
        pout("     Received address frame error count: %u\n", val);
        break;
    case 0x21:
        pout("     Transmitted abandon-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x22:
        pout("     Received abandon-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x23:
        pout("     Transmitted retry-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x24:
        pout("     Received retry-class OPEN_REJECT count: %u\n", val);
        break;
    case 0x25:
        pout("     Received AIP (WATING ON PARTIAL) count: %u\n", val);
        break;
    case 0x26:
        pout("     Received AIP (WAITING ON CONNECTION) count: %u\n", val);
        break;
    case 0x27:
        pout("     Transmitted BREAK count: %u\n", val);
        break;
    case 0x28:
        pout("     Received BREAK count: %u\n", val);
        break;
    case 0x29:
        pout("     Break timeout count: %u\n", val);
        break;
    case 0x2a:
        pout("     Connection count: %u\n", val);
        break;
    case 0x2b:
        pout("     Peak transmitted pathway blocked count: %u\n",
               val & 0xff);
        pout("         Peak value detector threshold: %u\n",
               thresh_val & 0xff);
        break;
    case 0x2c:
        u = val & 0xffff;
        if (u < 0x8000)
            pout("     Peak transmitted arbitration wait time (us): "
                   "%u\n", u);
        else
            pout("     Peak transmitted arbitration wait time (ms): "
                   "%u\n", 33 + (u - 0x8000));
        u = thresh_val & 0xffff;
        if (u < 0x8000)
            pout("         Peak value detector threshold (us): %u\n",
                   u);
        else
            pout("         Peak value detector threshold (ms): %u\n",
                   33 + (u - 0x8000));
        break;
    case 0x2d:
        pout("     Peak arbitration time (us): %u\n", val);
        pout("         Peak value detector threshold: %u\n", thresh_val);
        break;
    case 0x2e:
        pout("     Peak connection time (us): %u\n", val);
        pout("         Peak value detector threshold: %u\n", thresh_val);
        break;
    case 0x40:
        pout("     Transmitted SSP frame count: %u\n", val);
        break;
    case 0x41:
        pout("     Received SSP frame count: %u\n", val);
        break;
    case 0x42:
        pout("     Transmitted SSP frame error count: %u\n", val);
        break;
    case 0x43:
        pout("     Received SSP frame error count: %u\n", val);
        break;
    case 0x44:
        pout("     Transmitted CREDIT_BLOCKED count: %u\n", val);
        break;
    case 0x45:
        pout("     Received CREDIT_BLOCKED count: %u\n", val);
        break;
    case 0x50:
        pout("     Transmitted SATA frame count: %u\n", val);
        break;
    case 0x51:
        pout("     Received SATA frame count: %u\n", val);
        break;
    case 0x52:
        pout("     SATA flow control buffer overflow count: %u\n", val);
        break;
    case 0x60:
        pout("     Transmitted SMP frame count: %u\n", val);
        break;
    case 0x61:
        pout("     Received SMP frame count: %u\n", val);
        break;
    case 0x63:
        pout("     Received SMP frame error count: %u\n", val);
        break;
    default:
        break;
    }
}

static void
show_sas_port_param(unsigned char * ucp, int param_len)
{
    int j, m, n, nphys, t, sz, spld_len;
    unsigned char * vcp;
    uint64_t ull;
    unsigned int ui;
    char s[64];

    sz = sizeof(s);
    // pcb = ucp[2];
    t = (ucp[0] << 8) | ucp[1];
    pout("relative target port id = %d\n", t);
    pout("  generation code = %d\n", ucp[6]);
    nphys = ucp[7];
    pout("  number of phys = %d\n", nphys);

    for (j = 0, vcp = ucp + 8; j < (param_len - 8);
         vcp += spld_len, j += spld_len) {
        pout("  phy identifier = %d\n", vcp[1]);
        spld_len = vcp[3];
        if (spld_len < 44)
            spld_len = 48;      /* in SAS-1 and SAS-1.1 vcp[3]==0 */
        else
            spld_len += 4;
        t = ((0x70 & vcp[4]) >> 4);
        switch (t) {
        case 0: snprintf(s, sz, "no device attached"); break;
        case 1: snprintf(s, sz, "SAS or SATA device"); break;
        case 2: snprintf(s, sz, "expander device"); break;
        case 3: snprintf(s, sz, "expander device (fanout)"); break;
        default: snprintf(s, sz, "reserved [%d]", t); break;
        }
        pout("    attached device type: %s\n", s);
        t = 0xf & vcp[4];
        switch (t) {
        case 0: snprintf(s, sz, "unknown"); break;
        case 1: snprintf(s, sz, "power on"); break;
        case 2: snprintf(s, sz, "hard reset"); break;
        case 3: snprintf(s, sz, "SMP phy control function"); break;
        case 4: snprintf(s, sz, "loss of dword synchronization"); break;
        case 5: snprintf(s, sz, "mux mix up"); break;
        case 6: snprintf(s, sz, "I_T nexus loss timeout for STP/SATA");
            break;
        case 7: snprintf(s, sz, "break timeout timer expired"); break;
        case 8: snprintf(s, sz, "phy test function stopped"); break;
        case 9: snprintf(s, sz, "expander device reduced functionality");
             break;
        default: snprintf(s, sz, "reserved [0x%x]", t); break;
        }
        pout("    attached reason: %s\n", s);
        t = (vcp[5] & 0xf0) >> 4;
        switch (t) {
        case 0: snprintf(s, sz, "unknown"); break;
        case 1: snprintf(s, sz, "power on"); break;
        case 2: snprintf(s, sz, "hard reset"); break;
        case 3: snprintf(s, sz, "SMP phy control function"); break;
        case 4: snprintf(s, sz, "loss of dword synchronization"); break;
        case 5: snprintf(s, sz, "mux mix up"); break;
        case 6: snprintf(s, sz, "I_T nexus loss timeout for STP/SATA");
            break;
        case 7: snprintf(s, sz, "break timeout timer expired"); break;
        case 8: snprintf(s, sz, "phy test function stopped"); break;
        case 9: snprintf(s, sz, "expander device reduced functionality");
             break;
        default: snprintf(s, sz, "reserved [0x%x]", t); break;
        }
        pout("    reason: %s\n", s);
        t = (0xf & vcp[5]);
        switch (t) {
        case 0: snprintf(s, sz, "phy enabled; unknown");
                     break;
        case 1: snprintf(s, sz, "phy disabled"); break;
        case 2: snprintf(s, sz, "phy enabled; speed negotiation failed");
                     break;
        case 3: snprintf(s, sz, "phy enabled; SATA spinup hold state");
                     break;
        case 4: snprintf(s, sz, "phy enabled; port selector");
                     break;
        case 5: snprintf(s, sz, "phy enabled; reset in progress");
                     break;
        case 6: snprintf(s, sz, "phy enabled; unsupported phy attached");
                     break;
        case 8: snprintf(s, sz, "phy enabled; 1.5 Gbps"); break;
        case 9: snprintf(s, sz, "phy enabled; 3 Gbps"); break;
        case 0xa: snprintf(s, sz, "phy enabled; 6 Gbps"); break;
        case 0xb: snprintf(s, sz, "phy enabled; 12 Gbps"); break;
        default: snprintf(s, sz, "reserved [%d]", t); break;
        }
        pout("    negotiated logical link rate: %s\n", s);
        pout("    attached initiator port: ssp=%d stp=%d smp=%d\n",
               !! (vcp[6] & 8), !! (vcp[6] & 4), !! (vcp[6] & 2));
        pout("    attached target port: ssp=%d stp=%d smp=%d\n",
               !! (vcp[7] & 8), !! (vcp[7] & 4), !! (vcp[7] & 2));
        for (n = 0, ull = vcp[8]; n < 8; ++n) {
            ull <<= 8; ull |= vcp[8 + n];
        }
        pout("    SAS address = 0x%" PRIx64 "\n", ull);
        for (n = 0, ull = vcp[16]; n < 8; ++n) {
            ull <<= 8; ull |= vcp[16 + n];
        }
        pout("    attached SAS address = 0x%" PRIx64 "\n", ull);
        pout("    attached phy identifier = %d\n", vcp[24]);
        ui = (vcp[32] << 24) | (vcp[33] << 16) | (vcp[34] << 8) | vcp[35];
        pout("    Invalid DWORD count = %u\n", ui);
        ui = (vcp[36] << 24) | (vcp[37] << 16) | (vcp[38] << 8) | vcp[39];
        pout("    Running disparity error count = %u\n", ui);
        ui = (vcp[40] << 24) | (vcp[41] << 16) | (vcp[42] << 8) | vcp[43];
        pout("    Loss of DWORD synchronization = %u\n", ui);
        ui = (vcp[44] << 24) | (vcp[45] << 16) | (vcp[46] << 8) | vcp[47];
        pout("    Phy reset problem = %u\n", ui);
        if (spld_len > 51) {
            int num_ped, peis;
            unsigned char * xcp;
            unsigned int pvdt;

            num_ped = vcp[51];
            if (num_ped > 0)
               pout("    Phy event descriptors:\n");
            xcp = vcp + 52;
            for (m = 0; m < (num_ped * 12); m += 12, xcp += 12) {
                peis = xcp[3];
                ui = (xcp[4] << 24) | (xcp[5] << 16) | (xcp[6] << 8) |
                     xcp[7];
                pvdt = (xcp[8] << 24) | (xcp[9] << 16) | (xcp[10] << 8) |
                       xcp[11];
                show_sas_phy_event_info(peis, ui, pvdt);
            }
        }
    }
}

// Returns 1 if okay, 0 if non SAS descriptors
static int
show_protocol_specific_page(unsigned char * resp, int len)
{
    int k, num, param_len;
    unsigned char * ucp;

    num = len - 4;
    for (k = 0, ucp = resp + 4; k < num; ) {
        param_len = ucp[3] + 4;
        if (6 != (0xf & ucp[4]))
            return 0;   /* only decode SAS log page */
        if (0 == k)
            pout("Protocol Specific port log page for SAS SSP\n");
        show_sas_port_param(ucp, param_len);
        k += param_len;
        ucp += param_len;
    }
    pout("\n");
    return 1;
}


// See Serial Attached SCSI (SPL-3) (e.g. revision 6g) the Protocol Specific
// log page [0x18]. Returns 0 if ok else FAIL* bitmask.
static int
scsiPrintSasPhy(scsi_device * device, int reset)
{
    int num, err;

    if ((err = scsiLogSense(device, PROTOCOL_SPECIFIC_LPAGE, 0, gBuf,
                            LOG_RESP_LONG_LEN, 0))) {
        print_on();
        pout("scsiPrintSasPhy Log Sense Failed [%s]\n\n", scsiErrString(err));
        print_off();
        return FAILSMART;
    }
    if ((gBuf[0] & 0x3f) != PROTOCOL_SPECIFIC_LPAGE) {
        print_on();
        pout("Protocol specific Log Sense Failed, page mismatch\n\n");
        print_off();
        return FAILSMART;
    }
    // compute page length
    num = (gBuf[2] << 8) + gBuf[3];
    if (1 != show_protocol_specific_page(gBuf, num + 4)) {
        print_on();
        pout("Only support protocol specific log page on SAS devices\n\n");
        print_off();
        return FAILSMART;
    }
    if (reset) {
        if ((err = scsiLogSelect(device, 1 /* pcr */, 0 /* sp */, 0 /* pc */,
                                 PROTOCOL_SPECIFIC_LPAGE, 0, NULL, 0))) {
            print_on();
            pout("scsiPrintSasPhy Log Select (reset) Failed [%s]\n\n",
                 scsiErrString(err));
            print_off();
            return FAILSMART;
        }
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
        "reserved [0x10]"
        "object based storage"
        "automation/driver interface"
        "security manager device"
        "host managed zoned block device"
        "reserved [0x15]"
        "reserved [0x16]"
        "reserved [0x17]"
        "reserved [0x18]"
        "reserved [0x19]"
        "reserved [0x1a]"
        "reserved [0x1b]"
        "reserved [0x1c]"
        "reserved [0x1d]"
        "well known logical unit"
        "unknown or no device type"
};

static const char * transport_proto_arr[] = {
        "Fibre channel (FCP-2)",
        "Parallel SCSI (SPI-4)",
        "SSA",
        "IEEE 1394 (SBP-2)",
        "RDMA (SRP)",
        "iSCSI",
        "SAS (SPL-3)",
        "ADT",
        "ATA (ACS-2)",
        "UAS",
        "SOP",
        "0xb",
        "0xc",
        "0xd",
        "0xe",
        "0xf"
};

/* Returns 0 on success, 1 on general error and 2 for early, clean exit */
static int
scsiGetDriveInfo(scsi_device * device, UINT8 * peripheral_type, bool all)
{
    char timedatetz[DATEANDEPOCHLEN];
    struct scsi_iec_mode_page iec;
    int err, iec_err, len, req_len, avail_len, n, scsi_version;
    int is_tape = 0;
    int peri_dt = 0;
    int returnval = 0;
    int transport = -1;
    int form_factor = 0;
    int haw_zbc = 0;
    int protect = 0;

    memset(gBuf, 0, 96);
    req_len = 36;
    if ((err = scsiStdInquiry(device, gBuf, req_len))) {
        print_on();
        pout("Standard Inquiry (36 bytes) failed [%s]\n", scsiErrString(err));
        pout("Retrying with a 64 byte Standard Inquiry\n");
        print_off();
        /* Marvell controllers fail on a 36 bytes StdInquiry, but 64 suffices */
        req_len = 64;
        if ((err = scsiStdInquiry(device, gBuf, req_len))) {
            print_on();
            pout("Standard Inquiry (64 bytes) failed [%s]\n",
                 scsiErrString(err));
            print_off();
            return 1;
        }
    }
    avail_len = gBuf[4] + 5;
    len = (avail_len < req_len) ? avail_len : req_len;
    peri_dt = gBuf[0] & 0x1f;
    if (peripheral_type)
        *peripheral_type = peri_dt;

    if (len < 36) {
        print_on();
        pout("Short INQUIRY response, skip product id\n");
        print_off();
        return 1;
    }
    // Upper bits of version bytes were used in older standards
    // Only interested in SPC-4 (0x6) and SPC-5 (assumed to be 0x7)
    scsi_version = gBuf[2] & 0x7;

    if (all && (0 != strncmp((char *)&gBuf[8], "ATA", 3))) {
        char vendor[8+1], product[16+1], revision[4+1];
        scsi_format_id_string(vendor, (const unsigned char *)&gBuf[8], 8);
        scsi_format_id_string(product, (const unsigned char *)&gBuf[16], 16);
        scsi_format_id_string(revision, (const unsigned char *)&gBuf[32], 4);

        pout("=== START OF INFORMATION SECTION ===\n");
        pout("Vendor:               %.8s\n", vendor);
        pout("Product:              %.16s\n", product);
        if (gBuf[32] >= ' ')
            pout("Revision:             %.4s\n", revision);
        if (scsi_version == 0x6)
            pout("Compliance:           SPC-4\n");
        else if (scsi_version == 0x7)
            pout("Compliance:           SPC-5\n");
    }

    if (!*device->get_req_type()/*no type requested*/ &&
               (0 == strncmp((char *)&gBuf[8], "ATA", 3))) {
        pout("\nProbable ATA device behind a SAT layer\n"
             "Try an additional '-d ata' or '-d sat' argument.\n");
        return 2;
    }
    if (! all)
        return 0;

    protect = gBuf[5] & 0x1;    /* from and including SPC-3 */

    if (! is_tape) {    /* only do this for disks */
        unsigned int lb_size = 0;
        unsigned char lb_prov_resp[8];
        char cap_str[64];
        char si_str[64];
        char lb_str[16];
        int lb_per_pb_exp = 0;
        uint64_t capacity = scsiGetSize(device, &lb_size, &lb_per_pb_exp);

        if (capacity) {
            format_with_thousands_sep(cap_str, sizeof(cap_str), capacity);
            format_capacity(si_str, sizeof(si_str), capacity);
            pout("User Capacity:        %s bytes [%s]\n", cap_str, si_str);
            snprintf(lb_str, sizeof(lb_str) - 1, "%u", lb_size);
            pout("Logical block size:   %s bytes\n", lb_str);
        }
        int lbpme = -1;
        int lbprz = -1;
        if (protect || lb_per_pb_exp) {
            unsigned char rc16_12[20] = {0, };

            if (0 == scsiGetProtPBInfo(device, rc16_12)) {
                lb_per_pb_exp = rc16_12[1] & 0xf;       /* just in case */
                if (lb_per_pb_exp > 0) {
                    snprintf(lb_str, sizeof(lb_str) - 1, "%u",
                             (lb_size * (1 << lb_per_pb_exp)));
                    pout("Physical block size:  %s bytes\n", lb_str);
                    n = ((rc16_12[2] & 0x3f) << 8) + rc16_12[3];
                    if (n > 0)  // not common so cut the clutter
                        pout("Lowest aligned LBA:   %d\n", n);
                }
                if (rc16_12[0] & 0x1) { /* PROT_EN set */
                    int p_type = ((rc16_12[0] >> 1) & 0x7);

                    switch (p_type) {
                    case 0 :
                        pout("Formatted with type 1 protection\n");
                        break;
                    case 1 :
                        pout("Formatted with type 2 protection\n");
                        break;
                    case 2 :
                        pout("Formatted with type 3 protection\n");
                        break;
                    default:
                        pout("Formatted with unknown protection type [%d]\n",
                             p_type);
                        break;
                    }
                    int p_i_exp = ((rc16_12[1] >> 4) & 0xf);

                    if (p_i_exp > 0)
                        pout("%d protection information intervals per "
                             "logical block\n", (1 << p_i_exp));
                }
                /* Pick up some LB provisioning info since its available */
                lbpme = !! (rc16_12[2] & 0x80);
                lbprz = !! (rc16_12[2] & 0x40);
            }
        }
        if (0 == scsiInquiryVpd(device, SCSI_VPD_LOGICAL_BLOCK_PROVISIONING,
                                lb_prov_resp, sizeof(lb_prov_resp))) {
            int prov_type = lb_prov_resp[6] & 0x7;

            if (-1 == lbprz)
                lbprz = !! (lb_prov_resp[5] & 0x4);
            switch (prov_type) {
            case 0:
                pout("LB provisioning type: unreported, LBPME=%d, LBPRZ=%d\n",
                     lbpme, lbprz);
                break;
            case 1:
                pout("LU is resource provisioned, LBPRZ=%d\n", lbprz);
                break;
            case 2:
                pout("LU is thin provisioned, LBPRZ=%d\n", lbprz);
                break;
            default:
                pout("LU provisioning type reserved [%d], LBPRZ=%d\n",
                     prov_type, lbprz);
                break;
            }
        } else if (1 == lbpme)
            pout("Logical block provisioning enabled, LBPRZ=%d\n", lbprz);

        int rpm = scsiGetRPM(device, modese_len, &form_factor, &haw_zbc);
        if (rpm >= 0) {
            if (0 == rpm)
                ;       // Not reported
            else if (1 == rpm)
                pout("Rotation Rate:        Solid State Device\n");
            else if ((rpm <= 0x400) || (0xffff == rpm))
                ;       // Reserved
            else
                pout("Rotation Rate:        %d rpm\n", rpm);
        }
        if (form_factor > 0) {
            const char * cp = NULL;

            switch (form_factor) {
            case 1:
                cp = "5.25";
                break;
            case 2:
                cp = "3.5";
                break;
            case 3:
                cp = "2.5";
                break;
            case 4:
                cp = "1.8";
                break;
            case 5:
                cp = "< 1.8";
                break;
            }
            if (cp)
                pout("Form Factor:          %s inches\n", cp);
        }
        if (haw_zbc > 0)
            pout("Host aware zoned block capable\n");
    }

    /* Do this here to try and detect badly conforming devices (some USB
       keys) that will lock up on a InquiryVpd or log sense or ... */
    if ((iec_err = scsiFetchIECmpage(device, &iec, modese_len))) {
        if (SIMPLE_ERR_BAD_RESP == iec_err) {
            pout(">> Terminate command early due to bad response to IEC "
                 "mode page\n");
            print_off();
            gIecMPage = 0;
            return 1;
        }
    } else
        modese_len = iec.modese_len;

    if (! dont_print_serial_number) {
        if (0 == (err = scsiInquiryVpd(device, SCSI_VPD_DEVICE_IDENTIFICATION,
                                       gBuf, 252))) {
            char s[256];

            len = gBuf[3];
            scsi_decode_lu_dev_id(gBuf + 4, len, s, sizeof(s), &transport);
            if (strlen(s) > 0)
                pout("Logical Unit id:      %s\n", s);
        } else if (scsi_debugmode > 0) {
            print_on();
            if (SIMPLE_ERR_BAD_RESP == err)
                pout("Vital Product Data (VPD) bit ignored in INQUIRY\n");
            else
                pout("Vital Product Data (VPD) INQUIRY failed [%d]\n", err);
            print_off();
        }
        if (0 == (err = scsiInquiryVpd(device, SCSI_VPD_UNIT_SERIAL_NUMBER,
                                       gBuf, 252))) {
            char serial[256];
            len = gBuf[3];

            gBuf[4 + len] = '\0';
            scsi_format_id_string(serial, &gBuf[4], len);
            pout("Serial number:        %s\n", serial);
        } else if (scsi_debugmode > 0) {
            print_on();
            if (SIMPLE_ERR_BAD_RESP == err)
                pout("Vital Product Data (VPD) bit ignored in INQUIRY\n");
            else
                pout("Vital Product Data (VPD) INQUIRY failed [%d]\n", err);
            print_off();
        }
    }

    // print SCSI peripheral device type
    if (peri_dt < (int)(sizeof(peripheral_dt_arr) /
                        sizeof(peripheral_dt_arr[0])))
        pout("Device type:          %s\n", peripheral_dt_arr[peri_dt]);
    else
        pout("Device type:          <%d>\n", peri_dt);

    // See if transport protocol is known
    if (transport < 0)
        transport = scsiFetchTransportProtocol(device, modese_len);
    if ((transport >= 0) && (transport <= 0xf))
        pout("Transport protocol:   %s\n", transport_proto_arr[transport]);

    // print current time and date and timezone
    dateandtimezone(timedatetz);
    pout("Local Time is:        %s\n", timedatetz);

    if ((SCSI_PT_SEQUENTIAL_ACCESS == *peripheral_type) ||
        (SCSI_PT_MEDIUM_CHANGER == *peripheral_type))
        is_tape = 1;
    // See if unit accepts SCSI commmands from us
    if ((err = scsiTestUnitReady(device))) {
        if (SIMPLE_ERR_NOT_READY == err) {
            print_on();
            if (!is_tape)
                pout("device is NOT READY (e.g. spun down, busy)\n");
            else
                pout("device is NOT READY (e.g. no tape)\n");
            print_off();
         } else if (SIMPLE_ERR_NO_MEDIUM == err) {
            print_on();
            pout("NO MEDIUM present on device\n");
            print_off();
         } else if (SIMPLE_ERR_BECOMING_READY == err) {
            print_on();
            pout("device becoming ready (wait)\n");
            print_off();
        } else {
            print_on();
            pout("device Test Unit Ready  [%s]\n", scsiErrString(err));
            print_off();
        }
        failuretest(MANDATORY_CMD, returnval|=FAILID);
    }

    if (iec_err) {
        if (!is_tape) {
            print_on();
            pout("SMART support is:     Unavailable - device lacks SMART capability.\n");
            if (scsi_debugmode > 0)
                pout(" [%s]\n", scsiErrString(iec_err));
            print_off();
        }
        gIecMPage = 0;
        return 0;
    }

    if (!is_tape)
        pout("SMART support is:     Available - device has SMART capability.\n"
             "SMART support is:     %s\n",
             (scsi_IsExceptionControlEnabled(&iec)) ? "Enabled" : "Disabled");
    pout("%s\n", (scsi_IsWarningEnabled(&iec)) ?
                  "Temperature Warning:  Enabled" :
                  "Temperature Warning:  Disabled or Not Supported");
    return 0;
}

static int
scsiSmartEnable(scsi_device * device)
{
    struct scsi_iec_mode_page iec;
    int err;

    if ((err = scsiFetchIECmpage(device, &iec, modese_len))) {
        print_on();
        pout("unable to fetch IEC (SMART) mode page [%s]\n",
             scsiErrString(err));
        print_off();
        return 1;
    } else
        modese_len = iec.modese_len;

    if ((err = scsiSetExceptionControlAndWarning(device, 1, &iec))) {
        print_on();
        pout("unable to enable Exception control and warning [%s]\n",
             scsiErrString(err));
        print_off();
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

static int
scsiSmartDisable(scsi_device * device)
{
    struct scsi_iec_mode_page iec;
    int err;

    if ((err = scsiFetchIECmpage(device, &iec, modese_len))) {
        print_on();
        pout("unable to fetch IEC (SMART) mode page [%s]\n",
             scsiErrString(err));
        print_off();
        return 1;
    } else
        modese_len = iec.modese_len;

    if ((err = scsiSetExceptionControlAndWarning(device, 0, &iec))) {
        print_on();
        pout("unable to disable Exception control and warning [%s]\n",
             scsiErrString(err));
        print_off();
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

static void
scsiPrintTemp(scsi_device * device)
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
    if (temp || trip)
        pout("\n");
}

/* Main entry point used by smartctl command. Return 0 for success */
int
scsiPrintMain(scsi_device * device, const scsi_print_options & options)
{
    int checkedSupportedLogPages = 0;
    UINT8 peripheral_type = 0;
    int returnval = 0;
    int res, durationSec;
    struct scsi_sense_disect sense_info;

    bool any_output = options.drive_info;

    if (supported_vpd_pages_p) {
        delete supported_vpd_pages_p;
        supported_vpd_pages_p = NULL;
    }
    supported_vpd_pages_p = new supported_vpd_pages(device);

    res = scsiGetDriveInfo(device, &peripheral_type, options.drive_info);
    if (res) {
        if (2 == res)
            return 0;
        else
            failuretest(MANDATORY_CMD, returnval |= FAILID);
        any_output = true;
    }

  // Print read look-ahead status for disks
  short int wce = -1, rcd = -1;
  if (options.get_rcd || options.get_wce) {
    if (SCSI_PT_DIRECT_ACCESS == peripheral_type)
       res = scsiGetSetCache(device, modese_len, &wce, &rcd);
    else
       res = -1; // fetch for disks only
    any_output = true;
  }

  if (options.get_rcd) {
    pout("Read Cache is:        %s\n",
      res ? "Unavailable" : // error
      rcd ? "Disabled" : "Enabled");
   }

  if (options.get_wce) {
    pout("Writeback Cache is:   %s\n",
      res ? "Unavailable" : // error
      !wce ? "Disabled" : "Enabled");
   }
   if (options.drive_info)
     pout("\n");

  // START OF THE ENABLE/DISABLE SECTION OF THE CODE
  if (   options.smart_disable           || options.smart_enable
      || options.smart_auto_save_disable || options.smart_auto_save_enable)
    pout("=== START OF ENABLE/DISABLE COMMANDS SECTION ===\n");

    if (options.smart_enable) {
        if (scsiSmartEnable(device))
            failuretest(MANDATORY_CMD, returnval |= FAILSMART);
        any_output = true;
    }

    if (options.smart_disable) {
        if (scsiSmartDisable(device))
            failuretest(MANDATORY_CMD,returnval |= FAILSMART);
        any_output = true;
    }

    if (options.smart_auto_save_enable) {
      if (scsiSetControlGLTSD(device, 0, modese_len)) {
        pout("Enable autosave (clear GLTSD bit) failed\n");
        failuretest(OPTIONAL_CMD,returnval |= FAILSMART);
      }
      else {
         pout("Autosave enabled (GLTSD bit set).\n");
      }
      any_output = true;
    }

    // Enable/Disable write cache
    if (options.set_wce && SCSI_PT_DIRECT_ACCESS == peripheral_type) {
      short int enable = wce = (options.set_wce > 0);
      rcd = -1;
      if (scsiGetSetCache(device, modese_len, &wce, &rcd)) {
          pout("Write cache %sable failed: %s\n", (enable ? "en" : "dis"),
               device->get_errmsg());
          failuretest(OPTIONAL_CMD,returnval |= FAILSMART);
      }
      else
        pout("Write cache %sabled\n", (enable ? "en" : "dis"));
      any_output = true;
    }

    // Enable/Disable read cache
    if (options.set_rcd && SCSI_PT_DIRECT_ACCESS == peripheral_type) {
      short int enable =  (options.set_rcd > 0);
      rcd = !enable;
      wce = -1;
      if (scsiGetSetCache(device, modese_len, &wce, &rcd)) {
          pout("Read cache %sable failed: %s\n", (enable ? "en" : "dis"),
                device->get_errmsg());
          failuretest(OPTIONAL_CMD,returnval |= FAILSMART);
      }
      else
        pout("Read cache %sabled\n", (enable ? "en" : "dis"));
      any_output = true;
    }

    if (options.smart_auto_save_disable) {
      if (scsiSetControlGLTSD(device, 1, modese_len)) {
        pout("Disable autosave (set GLTSD bit) failed\n");
        failuretest(OPTIONAL_CMD,returnval |= FAILSMART);
      }
      else {
         pout("Autosave disabled (GLTSD bit cleared).\n");
      }
      any_output = true;
    }
  if (   options.smart_disable           || options.smart_enable
      || options.smart_auto_save_disable || options.smart_auto_save_enable)
    pout("\n"); // END OF THE ENABLE/DISABLE SECTION OF THE CODE

    // START OF READ-ONLY OPTIONS APART FROM -V and -i
    if (    options.smart_check_status  || options.smart_ss_media_log
           || options.smart_vendor_attrib || options.smart_error_log
           || options.smart_selftest_log  || options.smart_vendor_attrib
           || options.smart_background_log || options.sasphy
         )
    pout("=== START OF READ SMART DATA SECTION ===\n");

    if (options.smart_check_status) {
        scsiGetSupportedLogPages(device);
        checkedSupportedLogPages = 1;
        if ((SCSI_PT_SEQUENTIAL_ACCESS == peripheral_type) ||
            (SCSI_PT_MEDIUM_CHANGER == peripheral_type)) { /* tape device */
            if (gTapeAlertsLPage) {
                if (options.drive_info)
                    pout("TapeAlert Supported\n");
                if (-1 == scsiGetTapeAlertsData(device, peripheral_type))
                    failuretest(OPTIONAL_CMD, returnval |= FAILSMART);
            }
            else
                pout("TapeAlert Not Supported\n");
        } else { /* disk, cd/dvd, enclosure, etc */
            if ((res = scsiGetSmartData(device, options.smart_vendor_attrib))) {
                if (-2 == res)
                    returnval |= FAILSTATUS;
                else
                    returnval |= FAILSMART;
            }
        }
        any_output = true;
    }

    if (options.smart_ss_media_log) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(device);
        res = 0;
        if (gSSMediaLPage)
            res = scsiPrintSSMedia(device);
        if (0 != res)
            failuretest(OPTIONAL_CMD, returnval|=res);
        any_output = true;
    }
    if (options.smart_vendor_attrib) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(device);
        if (gTempLPage) {
            scsiPrintTemp(device);
        }
        if (gStartStopLPage)
            scsiGetStartStopData(device);
        if (SCSI_PT_DIRECT_ACCESS == peripheral_type) {
            scsiPrintGrownDefectListLen(device);
            if (gSeagateCacheLPage)
                scsiPrintSeagateCacheLPage(device);
            if (gSeagateFactoryLPage)
                scsiPrintSeagateFactoryLPage(device);
        }
        any_output = true;
    }
    if (options.smart_error_log) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(device);
        scsiPrintErrorCounterLog(device);
        if (1 == scsiFetchControlGLTSD(device, modese_len, 1))
            pout("\n[GLTSD (Global Logging Target Save Disable) set. "
                 "Enable Save with '-S on']\n");
        any_output = true;
    }
    if (options.smart_selftest_log) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(device);
        res = 0;
        if (gSelfTestLPage)
            res = scsiPrintSelfTest(device);
        else {
            pout("Device does not support Self Test logging\n");
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        }
        if (0 != res)
            failuretest(OPTIONAL_CMD, returnval|=res);
        any_output = true;
    }
    if (options.smart_background_log) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(device);
        res = 0;
        if (gBackgroundResultsLPage)
            res = scsiPrintBackgroundResults(device);
        else {
            pout("Device does not support Background scan results logging\n");
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        }
        if (0 != res)
            failuretest(OPTIONAL_CMD, returnval|=res);
        any_output = true;
    }
    if (options.smart_default_selftest) {
        if (scsiSmartDefaultSelfTest(device))
            return returnval | FAILSMART;
        pout("Default Self Test Successful\n");
        any_output = true;
    }
    if (options.smart_short_cap_selftest) {
        if (scsiSmartShortCapSelfTest(device))
            return returnval | FAILSMART;
        pout("Short Foreground Self Test Successful\n");
        any_output = true;
    }
    // check if another test is running
    if (options.smart_short_selftest || options.smart_extend_selftest) {
      if (!scsiRequestSense(device, &sense_info) &&
            (sense_info.asc == 0x04 && sense_info.ascq == 0x09)) {
         if (!options.smart_selftest_force) {
           pout("Can't start self-test without aborting current test");
           if (sense_info.progress != -1) {
             pout(" (%d%% remaining)",
                  100 - sense_info.progress * 100 / 65535);
           }
           pout(",\nadd '-t force' option to override, or run 'smartctl -X' "
                "to abort test.\n");
            return -1;
         }
         else
            scsiSmartSelfTestAbort(device);
       }
    }
    if (options.smart_short_selftest) {
        if (scsiSmartShortSelfTest(device))
            return returnval | FAILSMART;
        pout("Short Background Self Test has begun\n");
        pout("Use smartctl -X to abort test\n");
        any_output = true;
    }
    if (options.smart_extend_selftest) {
        if (scsiSmartExtendSelfTest(device))
            return returnval | FAILSMART;
        pout("Extended Background Self Test has begun\n");
        if ((0 == scsiFetchExtendedSelfTestTime(device, &durationSec,
                        modese_len)) && (durationSec > 0)) {
            time_t t = time(NULL);

            t += durationSec;
            pout("Please wait %d minutes for test to complete.\n",
                 durationSec / 60);
            pout("Estimated completion time: %s\n", ctime(&t));
        }
        pout("Use smartctl -X to abort test\n");
        any_output = true;
    }
    if (options.smart_extend_cap_selftest) {
        if (scsiSmartExtendCapSelfTest(device))
            return returnval | FAILSMART;
        pout("Extended Foreground Self Test Successful\n");
    }
    if (options.smart_selftest_abort) {
        if (scsiSmartSelfTestAbort(device))
            return returnval | FAILSMART;
        pout("Self Test returned without error\n");
        any_output = true;
    }
    if (options.sasphy && gProtocolSpecificLPage) {
        if (scsiPrintSasPhy(device, options.sasphy_reset))
            return returnval | FAILSMART;
        any_output = true;
    }

    if (!any_output)
      pout("SCSI device successfully opened\n\n"
           "Use 'smartctl -a' (or '-x') to print SMART (and more) information\n\n");

    return returnval;
}
