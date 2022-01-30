/*
 * scsiprint.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2002-11 Bruce Allen
 * Copyright (C) 2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2003-20 Douglas Gilbert <dgilbert@interlog.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "config.h"
#define __STDC_FORMAT_MACROS 1 // enable PRI* for C++

#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "scsicmds.h"
#include "atacmds.h" // dont_print_serial_number
#include "dev_interface.h"
#include "scsiprint.h"
#include "smartctl.h"
#include "utility.h"
#include "sg_unaligned.h"

#define GBUF_SIZE 65532

const char * scsiprint_c_cvsid = "$Id$"
                                 SCSIPRINT_H_CVSID;

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

uint8_t gBuf[GBUF_SIZE];
#define LOG_RESP_LEN 252
#define LOG_RESP_LONG_LEN ((62 * 256) + 252)
#define LOG_RESP_TAPE_ALERT_LEN 0x144

/* Supported log pages + Supported log pages and subpages maximum count */
#define SCSI_SUPP_LOG_PAGES_MAX_COUNT (252 + (62 * 128) + 126)

/* Log pages supported */
static bool gSmartLPage = false;     /* Informational Exceptions log page */
static bool gTempLPage = false;
static bool gSelfTestLPage = false;
static bool gStartStopLPage = false;
static bool gReadECounterLPage = false;
static bool gWriteECounterLPage = false;
static bool gVerifyECounterLPage = false;
static bool gNonMediumELPage = false;
static bool gLastNErrorEvLPage = false;
static bool gBackgroundResultsLPage = false;
static bool gProtocolSpecificLPage = false;
static bool gTapeAlertsLPage = false;
static bool gSSMediaLPage = false;
static bool gFormatStatusLPage = false;
static bool gEnviroReportingLPage = false;
static bool gEnviroLimitsLPage = false;
static bool gUtilizationLPage = false;
static bool gPendDefectsLPage = false;
static bool gBackgroundOpLPage = false;
static bool gLPSMisalignLPage = false;
static bool gTapeDeviceStatsLPage = false;
static bool gZBDeviceStatsLPage = false;

/* Vendor specific log pages */
static bool gSeagateCacheLPage = false;
static bool gSeagateFactoryLPage = false;

/* Mode pages supported */
static bool gIecMPage = true;    /* N.B. assume it until we know otherwise */

/* Remember last successful mode sense/select command */
static int modese_len = 0;

/* Remember this value from the most recent INQUIRY */
static int scsi_version;
#define SCSI_VERSION_SPC_4 0x6
#define SCSI_VERSION_SPC_5 0x7
#define SCSI_VERSION_HIGHEST SCSI_VERSION_SPC_5

/* T10 vendor identification. Should match entry in last Annex of SPC
 * drafts and standards (e.g. SPC-4). */
static char scsi_vendor[8+1];
#define T10_VENDOR_SEAGATE "SEAGATE"
#define T10_VENDOR_HITACHI_1 "HITACHI"
#define T10_VENDOR_HITACHI_2 "HL-DT-ST"
#define T10_VENDOR_HITACHI_3 "HGST"

static const char * logSenStr = "Log Sense";
static const char * logSenRspStr = "Log Sense response";


static bool
seagate_or_hitachi(void)
{
    return ((0 == memcmp(scsi_vendor, T10_VENDOR_SEAGATE,
                         strlen(T10_VENDOR_SEAGATE))) ||
            (0 == memcmp(scsi_vendor, T10_VENDOR_HITACHI_1,
                         strlen(T10_VENDOR_HITACHI_1))) ||
            (0 == memcmp(scsi_vendor, T10_VENDOR_HITACHI_2,
                         strlen(T10_VENDOR_HITACHI_2))) ||
            (0 == memcmp(scsi_vendor, T10_VENDOR_HITACHI_3,
                         strlen(T10_VENDOR_HITACHI_3))));
}

static bool
all_ffs(const uint8_t * bp, int b_len)
{
    if ((NULL == bp) || (b_len <= 0))
        return false;
    for (--b_len; b_len >= 0; --b_len) {
        if (0xff != bp[b_len])
            return false;
    }
    return true;
}

// trim from right. By default trims whitespace.
static std::string rtrim(const std::string& s, const char* t = " \t\n\r\f\v")
{
    std::string r(s);

    r.erase(r.find_last_not_of(t) + 1);
    return r;
}

static void
scsiGetSupportedLogPages(scsi_device * device)
{
    bool got_subpages = false;
    int k, err, resp_len, num_unreported, num_unreported_spg;
    int supp_lpg_and_spg_count = 0;

    const uint8_t * up;
    uint8_t sup_lpgs[LOG_RESP_LEN];
    struct scsi_supp_log_pages supp_lpg_and_spg[SCSI_SUPP_LOG_PAGES_MAX_COUNT];

    memset(gBuf, 0, LOG_RESP_LEN);
    memset(supp_lpg_and_spg, 0, sizeof(supp_lpg_and_spg));

    /* Get supported log pages */
    if ((err = scsiLogSense(device, SUPPORTED_LPAGES, 0, gBuf,
                            LOG_RESP_LEN, 0 /* do double fetch */))) {
        if (scsi_debugmode > 0)
            pout("%s for supported pages failed [%s]\n", logSenStr,
                 scsiErrString(err));
        /* try one more time with defined length, workaround for the bug #678
        found with ST8000NM0075/E001 */
        err = scsiLogSense(device, SUPPORTED_LPAGES, 0, gBuf,
                            LOG_RESP_LEN, 68); /* 64 max pages + 4b header */
        if (scsi_debugmode > 0)
            pout("%s for supported pages failed (second attempt) [%s]\n",
                 logSenStr, scsiErrString(err));
        if (err)
            return;
    }

    memcpy(sup_lpgs, gBuf, LOG_RESP_LEN);
    resp_len = gBuf[3];
    up = gBuf + LOGPAGEHDRSIZE;

    for (k = 0; k < resp_len; k += 1) {
        uint8_t page_code = 0x3f & up[k];
        supp_lpg_and_spg[supp_lpg_and_spg_count++] = {page_code, 0};
    }

    /* Get supported log pages and subpages. Most drives seems to include the
    supported log pages here as well, but some drives such as the Samsung
    PM1643a will only report the additional log pages with subpages here */
    if ((scsi_version >= SCSI_VERSION_SPC_4) &&
            (scsi_version <= SCSI_VERSION_HIGHEST)) {
        /* unclear what code T10 will choose for SPC-6 */
        if ((err = scsiLogSense(device, SUPPORTED_LPAGES, SUPP_SPAGE_L_SPAGE,
                                gBuf, LOG_RESP_LONG_LEN,
                                -1 /* just single not double fetch */))) {
            if (scsi_debugmode > 0)
                pout("%s for supported pages and subpages failed [%s]\n",
                     logSenStr, scsiErrString(err));
        } else {
            /* Ensure we didn't get the same answer than without the subpages */
            if (0 == memcmp(gBuf, sup_lpgs, LOG_RESP_LEN)) {
                if (scsi_debugmode > 0)
                    pout("%s: %s ignored subpage field, bad\n",
                         __func__, logSenRspStr);
            } else if (! ((0x40 & gBuf[0]) &&
                          (SUPP_SPAGE_L_SPAGE == gBuf[1]))) {
                if (scsi_debugmode > 0)
                    pout("%s supported subpages is bad SPF=%u SUBPG=%u\n",
                         logSenRspStr, !! (0x40 & gBuf[0]), gBuf[2]);
            } else {
                got_subpages = true;
            }
        }
    }

    if (got_subpages) {
        resp_len = sg_get_unaligned_be16(gBuf + 2);
        up = gBuf + LOGPAGEHDRSIZE;
        for (k = 0; k < resp_len; k += 2) {
            uint8_t page_code = 0x3f & up[k];
            uint8_t subpage_code = up[k+1];
            supp_lpg_and_spg[supp_lpg_and_spg_count++] = {page_code, subpage_code};
        }
    }

    num_unreported = 0;
    num_unreported_spg = 0;
    for (k = 0; k < supp_lpg_and_spg_count; k += 1) {
        struct scsi_supp_log_pages supp_lpg = supp_lpg_and_spg[k];

        switch (supp_lpg.page_code)
        {
            case SUPPORTED_LPAGES:
                if (! ((NO_SUBPAGE_L_SPAGE == supp_lpg.subpage_code) ||
                       (SUPP_SPAGE_L_SPAGE == supp_lpg.subpage_code))) {
                    if (scsi_debugmode > 1)
                        pout("%s: Strange Log page number: 0x0,0x%x\n",
                             __func__, supp_lpg.subpage_code);
                }
                break;
            case READ_ERROR_COUNTER_LPAGE:
                gReadECounterLPage = true;
                break;
            case WRITE_ERROR_COUNTER_LPAGE:
                gWriteECounterLPage = true;
                break;
            case VERIFY_ERROR_COUNTER_LPAGE:
                gVerifyECounterLPage = true;
                break;
            case LAST_N_ERROR_EVENTS_LPAGE:
                gLastNErrorEvLPage = true;
                break;
            case NON_MEDIUM_ERROR_LPAGE:
                gNonMediumELPage = true;
                break;
            case TEMPERATURE_LPAGE:
                if (NO_SUBPAGE_L_SPAGE == supp_lpg.subpage_code)
                    gTempLPage = true;
                else if (ENVIRO_REP_L_SPAGE == supp_lpg.subpage_code)
                    gEnviroReportingLPage = true;
                else if (ENVIRO_LIMITS_L_SPAGE == supp_lpg.subpage_code)
                    gEnviroLimitsLPage = true;
                else if (SUPP_SPAGE_L_SPAGE != supp_lpg.subpage_code) {
                    ++num_unreported;
                    ++num_unreported_spg;
                }
                /* WDC/HGST report <lpage>,0xff tuples for all supported
                   lpages; Seagate doesn't. T10 does not exclude the
                   reporting of <lpage>,0xff so it is not an error. */
                break;
            case STARTSTOP_CYCLE_COUNTER_LPAGE:
                if (NO_SUBPAGE_L_SPAGE == supp_lpg.subpage_code)
                    gStartStopLPage = true;
                else if (UTILIZATION_L_SPAGE == supp_lpg.subpage_code)
                    gUtilizationLPage = true;
                else if (SUPP_SPAGE_L_SPAGE != supp_lpg.subpage_code) {
                    ++num_unreported;
                    ++num_unreported_spg;
                }
                break;
            case SELFTEST_RESULTS_LPAGE:
                gSelfTestLPage = true;
                break;
            case IE_LPAGE:
                gSmartLPage = true;
                break;
            case DEVICE_STATS_LPAGE:
                if (NO_SUBPAGE_L_SPAGE == supp_lpg.subpage_code)
                    gTapeDeviceStatsLPage = true;
                else if (ZB_DEV_STATS_L_SPAGE == supp_lpg.subpage_code)
                    gZBDeviceStatsLPage = true;
                break;
            case BACKGROUND_RESULTS_LPAGE:
                if (NO_SUBPAGE_L_SPAGE == supp_lpg.subpage_code)
                    gBackgroundResultsLPage = true;
                else if (PEND_DEFECTS_L_SPAGE == supp_lpg.subpage_code)
                    gPendDefectsLPage = true;
                else if (BACKGROUND_OP_L_SPAGE == supp_lpg.subpage_code)
                    gBackgroundOpLPage = true;
                else if (LPS_MISALIGN_L_SPAGE == supp_lpg.subpage_code)
                    gLPSMisalignLPage = true;
                else if (SUPP_SPAGE_L_SPAGE != supp_lpg.subpage_code) {
                    ++num_unreported;
                    ++num_unreported_spg;
                }
                break;
            case PROTOCOL_SPECIFIC_LPAGE:
                gProtocolSpecificLPage = true;
                break;
            case TAPE_ALERTS_LPAGE:
                gTapeAlertsLPage = true;
                break;
            case SS_MEDIA_LPAGE:
                gSSMediaLPage = true;
                break;
            case FORMAT_STATUS_LPAGE:
                gFormatStatusLPage = true;
                break;
            case SEAGATE_CACHE_LPAGE:
                if (failuretest_permissive) {
                    gSeagateCacheLPage = true;
                    break;
                }
                if (seagate_or_hitachi())
                    gSeagateCacheLPage = true;
                break;
            case SEAGATE_FACTORY_LPAGE:
                if (failuretest_permissive) {
                    gSeagateFactoryLPage = true;
                    break;
                }
                if (seagate_or_hitachi())
                    gSeagateFactoryLPage = true;
                break;
            default:
                if (supp_lpg.page_code < 0x30) {     /* don't count VS pages */
                    ++num_unreported;
                    if ((supp_lpg.subpage_code > 0) &&
                            (SUPP_SPAGE_L_SPAGE != supp_lpg.subpage_code))
                        ++num_unreported_spg;
                }
                break;
        }
    }
    if (scsi_debugmode > 1)
        pout("%s: number of unreported (standard) log pages: %d (sub-pages: "
             "%d)\n", __func__, num_unreported, num_unreported_spg);
}

/* Returns 0 if ok, -1 if can't check IE, -2 if can check and bad
   (or at least something to report). */
static int
scsiGetSmartData(scsi_device * device, bool attribs)
{
    uint8_t asc;
    uint8_t ascq;
    uint8_t currenttemp = 255;
    uint8_t triptemp = 255;
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
        jout("SMART Health Status: %s [asc=%x, ascq=%x]\n", cp, asc, ascq);
        print_off();
        jglb["smart_status"]["passed"] = false;
        jglb["smart_status"]["scsi"]["asc"] = asc;
        jglb["smart_status"]["scsi"]["ascq"] = ascq;
        jglb["smart_status"]["scsi"]["ie_string"] = cp;
    }
    else if (gIecMPage) {
        jout("SMART Health Status: OK\n");
        jglb["smart_status"]["passed"] = true;
    }

    if (attribs && !gTempLPage) {
        if (255 == currenttemp)
            pout("Current Drive Temperature:     <not available>\n");
        else {
            jout("Current Drive Temperature:     %d C\n", currenttemp);
            jglb["temperature"]["current"] = currenttemp;
        }
        if (255 == triptemp)
            pout("Drive Trip Temperature:        <not available>\n");
        else {
            jout("Drive Trip Temperature:        %d C\n", triptemp);
            jglb["scsi_temperature"]["drive_trip"] = triptemp;
        }
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
    int i, k, j, m, err;
    const char *s;
    const char *ts;
    int failures = 0;

    print_on();
    if ((err = scsiLogSense(device, TAPE_ALERTS_LPAGE, 0, gBuf,
                        LOG_RESP_TAPE_ALERT_LEN, LOG_RESP_TAPE_ALERT_LEN))) {
        pout("%s Failed [%s]\n", __func__, scsiErrString(err));
        print_off();
        return -1;
    }
    if (gBuf[0] != 0x2e) {
        pout("TapeAlerts %s Failed\n", logSenStr);
        print_off();
        return -1;
    }
    pagelength = sg_get_unaligned_be16(gBuf + 2);

    json::ref jref = jglb["scsi_tapealert"]["status"];
    for (s=severities, k = 0, j = 0; *s; s++, ++k) {
        for (i = 4, m = 0; i < pagelength; i += 5, ++k, ++m) {
            parametercode = sg_get_unaligned_be16(gBuf + i);

            if (gBuf[i + 4]) {
                ts = SCSI_PT_MEDIUM_CHANGER == peripheral_type ?
                    scsiTapeAlertsChangerDevice(parametercode) :
                    scsiTapeAlertsTapeDevice(parametercode);
                if (*ts == *s) {
                    if (!failures)
                        pout("TapeAlert Errors (C=Critical, W=Warning, "
                             "I=Informational):\n");
                    jout("[0x%02x] %s\n", parametercode, ts);
                    jref[j]["descriptor_idx"] = m + 1;
                    jref[j]["parameter_code"] = parametercode;
                    jref[j]["string"] = ts;
                    ++j;
                    failures += 1;
                }
            }
        }
    }
    print_off();

    if (! failures) {
        jout("TapeAlert: OK\n");
        jglb["scsi_tapealert"]["status"] = "Good";
    }

    return failures;
}

static void
scsiGetStartStopData(scsi_device * device)
{
    int err, len, k, extra;
    unsigned char * ucp;
    char b[32];
    const char * q;
    static const char * jname = "scsi_start_stop_cycle_counter";

    if ((err = scsiLogSense(device, STARTSTOP_CYCLE_COUNTER_LPAGE, 0, gBuf,
                            LOG_RESP_LEN, 0))) {
        print_on();
        pout("%s Failed [%s]\n", __func__, scsiErrString(err));
        print_off();
        return;
    }
    if ((gBuf[0] & 0x3f) != STARTSTOP_CYCLE_COUNTER_LPAGE) {
        print_on();
        pout("StartStop %s Failed, page mismatch\n", logSenStr);
        print_off();
        return;
    }
    len = sg_get_unaligned_be16(gBuf + 2);
    ucp = gBuf + 4;
    for (k = len; k > 0; k -= extra, ucp += extra) {
        if (k < 3) {
            print_on();
            pout("StartStop %s: short\n", logSenRspStr);
            print_off();
            return;
        }
        extra = ucp[3] + 4;
        int pc = sg_get_unaligned_be16(ucp + 0);
        uint32_t u = (extra > 7) ? sg_get_unaligned_be32(ucp + 4) : 0;
        bool is_all_ffs = (extra > 7) ? all_ffs(ucp + 4, 4) : false;
        switch (pc) {
        case 1:
            if (10 == extra) {
                jout("Manufactured in week %.2s of year %.4s\n", ucp + 8,
                     ucp + 4);
                snprintf(b, sizeof(b), "%.4s", ucp + 4);
                jglb[jname]["year_of_manufacture"] = b;
                snprintf(b, sizeof(b), "%.2s", ucp + 8);
                jglb[jname]["week_of_manufacture"] = b;
            }
            break;
        case 2:
            /* ignore Accounting date */
            break;
        case 3:
            if ((extra > 7) && (! is_all_ffs)) {
                q = "Specified cycle count over device lifetime";
                jout("%s:  %u\n", q, u);
                jglb[jname][q] = u;
            }
            break;
        case 4:
            if ((extra > 7) && (! is_all_ffs)) {
                q = "Accumulated start-stop cycles";
                jout("%s:  %u\n", q, u);
                jglb[jname][q] = u;
            }
            break;
        case 5:
            if ((extra > 7) && (! is_all_ffs)) {
                q = "Specified load-unload count over device lifetime";
                jout("%s:  %u\n", q, u);
                jglb[jname][q] = u;
            }
            break;
        case 6:
            if ((extra > 7) && (! is_all_ffs)) {
                q = "Accumulated load-unload cycles";
                jout("%s:  %u\n", q, u);
                jglb[jname][q] = u;
            }
            break;
        default:
            /* ignore */
            break;
        }
    }
}
/* PENDING_DEFECTS_SUBPG [0x15,0x1]  introduced: SBC-4 */
static void
scsiPrintPendingDefectsLPage(scsi_device * device)
{
    static const char * pDefStr = "Pending Defects";
    static const char * jname = "scsi_pending_defects";

    int err;
    if ((err = scsiLogSense(device, BACKGROUND_RESULTS_LPAGE,
                            PEND_DEFECTS_L_SPAGE, gBuf, LOG_RESP_LONG_LEN,
                            0))) {
        print_on();
        pout("%s Failed [%s]\n", __func__, scsiErrString(err));
        print_off();
        return;
    }
    if (((gBuf[0] & 0x3f) != BACKGROUND_RESULTS_LPAGE) &&
        (gBuf[1] != PEND_DEFECTS_L_SPAGE)) {
        print_on();
        pout("%s %s, page mismatch\n", pDefStr, logSenRspStr);
        print_off();
        return;
    }
    int num = sg_get_unaligned_be16(gBuf + 2);
    if (num > LOG_RESP_LONG_LEN) {
        print_on();
        pout("%s %s too long\n", pDefStr, logSenRspStr);
        print_off();
        return;
    }
    const uint8_t * bp = gBuf + 4;
    while (num > 3) {
        int pc = sg_get_unaligned_be16(bp + 0);
        int pl = bp[3] + 4;
        uint32_t count;
        switch (pc) {
        case 0x0:
            jout("  Pending defect count:");
            if ((pl < 8) || (num < 8)) {
                print_on();
                pout("%s truncated descriptor\n", pDefStr);
                print_off();
                return;
            }
            count = sg_get_unaligned_be32(bp + 4);
            jglb[jname]["count"] = count;
            if (0 == count)
                jout("0 %s\n", pDefStr);
            else if (1 == count)
                jout("1 Pending Defect, LBA and accumulated_power_on_hours "
                     "follow\n");
            else
                jout("%u %s: index, LBA and accumulated_power_on_hours "
                     "follow\n", count, pDefStr);
            break;
        default:
            if ((pl < 16) || (num < 16)) {
                print_on();
                pout("%s truncated descriptor\n", pDefStr);
                print_off();
                return;
            }
            jout("  %4d:  0x%-16" PRIx64 ",  %5u\n", pc,
                 sg_get_unaligned_be64(bp + 8), sg_get_unaligned_be32(bp + 4));
            jglb[jname][pc]["LBA"] = sg_get_unaligned_be64(bp + 8);
            jglb[jname][pc]["accum_power_on_hours"] =
                   sg_get_unaligned_be32(bp + 4);
            break;
        }
        num -= pl;
        bp += pl;
    }
}

static void
scsiPrintGrownDefectListLen(scsi_device * device)
{
    bool got_rd12;
    int err, dl_format;
    unsigned int dl_len, div;
    static const char * hname = "Read defect list";

    memset(gBuf, 0, 8);
    if ((err = scsiReadDefect12(device, 0 /* req_plist */, 1 /* req_glist */,
                                4 /* format: bytes from index */,
                                0 /* addr desc index */, gBuf, 8))) {
        if (2 == err) { /* command not supported */
            err = scsiReadDefect10(device, 0 /* req_plist */,
                                   1 /* req_glist */,
                                   4 /* format: bytes from index */, gBuf, 4);
            if (err) {
                if (scsi_debugmode > 0) {
                    print_on();
                    pout("%s (10) Failed: %s\n", hname, scsiErrString(err));
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
                pout("%s (12) Failed: %s\n", hname, scsiErrString(err));
                print_off();
            }
            return;
        }
    } else
        got_rd12 = true;

    if (got_rd12) {
        int generation = sg_get_unaligned_be16(gBuf + 2);
        if ((generation > 1) && (scsi_debugmode > 0)) {
            print_on();
            pout("%s (12): generation=%d\n", hname, generation);
            print_off();
        }
        dl_len = sg_get_unaligned_be32(gBuf + 4);
    } else
        dl_len = sg_get_unaligned_be16(gBuf + 2);
    if (0x8 != (gBuf[1] & 0x18)) {
        print_on();
        pout("%s: asked for grown list but didn't get it\n", hname);
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
        case 6:     /* vendor specific */
            break;
        default:
            print_on();
            pout("defect list format %d unknown\n", dl_format);
            print_off();
            break;
    }
    if (0 == dl_len) {
        jout("Elements in grown defect list: 0\n\n");
        jglb["scsi_grown_defect_list"] = 0;
    }
    else {
        if (0 == div)
            pout("Grown defect list length=%u bytes [unknown "
                 "number of elements]\n\n", dl_len);
        else {
            jout("Elements in grown defect list: %u\n\n", dl_len / div);
            jglb["scsi_grown_defect_list"] = dl_len / div;
        }
    }
}

static uint64_t
variableLengthIntegerParam(const unsigned char * ucp)
{
    static const size_t sz_u64 = (int)sizeof(uint64_t);
    unsigned int u = ucp[3];
    const unsigned char * xp = ucp + 4;

    if (u > sz_u64) {
        xp += (u - sz_u64);
        u = sz_u64;
    }
    return sg_get_unaligned_be(u, xp + 0);
}

static void
scsiPrintSeagateCacheLPage(scsi_device * device)
{
    int num, pl, pc, err, len;
    unsigned char * ucp;
    static const char * seaCacStr = "Seagate Cache";

    if ((err = scsiLogSense(device, SEAGATE_CACHE_LPAGE, 0, gBuf,
                            LOG_RESP_LEN, 0))) {
        if (scsi_debugmode > 0) {
            print_on();
            pout("%s %s Failed: %s\n", seaCacStr, logSenStr,
                 scsiErrString(err));
            print_off();
        }
        return;
    }
    if ((gBuf[0] & 0x3f) != SEAGATE_CACHE_LPAGE) {
        if (scsi_debugmode > 0) {
            print_on();
            pout("%s %s, page mismatch\n", seaCacStr, logSenRspStr);
            print_off();
        }
        return;
    }
    len = sg_get_unaligned_be16(gBuf + 2) + 4;
    num = len - 4;
    ucp = &gBuf[0] + 4;
    while (num > 3) {
        pc = sg_get_unaligned_be16(ucp + 0);
        pl = ucp[3] + 4;
        switch (pc) {
        case 0: case 1: case 2: case 3: case 4:
            break;
        default:
            if (scsi_debugmode > 0) {
                print_on();
                pout("Vendor (%s) lpage has unexpected parameter, skip\n",
                     seaCacStr);
                print_off();
            }
            return;
        }
        num -= pl;
        ucp += pl;
    }
    pout("Vendor (%s) information\n", seaCacStr);
    num = len - 4;
    ucp = &gBuf[0] + 4;
    while (num > 3) {
        pc = sg_get_unaligned_be16(ucp + 0);
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
        pout(" = %" PRIu64 "\n", variableLengthIntegerParam(ucp));
        num -= pl;
        ucp += pl;
    }
    pout("\n");
}

static void
scsiPrintSeagateFactoryLPage(scsi_device * device)
{
    int num, pl, pc, len, err, good, bad;
    unsigned char * ucp;
    uint64_t ull;

    if ((err = scsiLogSense(device, SEAGATE_FACTORY_LPAGE, 0, gBuf,
                            LOG_RESP_LEN, 0))) {
        if (scsi_debugmode > 0) {
            print_on();
            pout("%s Failed [%s]\n", __func__, scsiErrString(err));
            print_off();
        }
        return;
    }
    if ((gBuf[0] & 0x3f) != SEAGATE_FACTORY_LPAGE) {
        if (scsi_debugmode > 0) {
            print_on();
            pout("Seagate/Hitachi Factory %s, page mismatch\n", logSenRspStr);
            print_off();
        }
        return;
    }
    len = sg_get_unaligned_be16(gBuf + 2) + 4;
    num = len - 4;
    ucp = &gBuf[0] + 4;
    good = 0;
    bad = 0;
    while (num > 3) {
        pc = sg_get_unaligned_be16(ucp + 0);
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
        pc = sg_get_unaligned_be16(ucp + 0);
        pl = ucp[3] + 4;
        good = 0;
        switch (pc) {
        case 0: jout("  number of hours powered up");
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
            ull = variableLengthIntegerParam(ucp);
            if (0 == pc) {
                jout(" = %.2f\n", ull / 60.0 );
                jglb["power_on_time"]["hours"] = ull / 60;
                jglb["power_on_time"]["minutes"] = ull % 60;
            }
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
    int found[3] = {0, 0, 0};

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

        json::ref jref = jglb["scsi_error_counter_log"];
        for (int k = 0; k < 3; ++k) {
            if (! found[k])
                continue;
            ecp = &errCounterArr[k];
            static const char * const pageNames[3] =
                                 {"read:   ", "write:  ", "verify: "};
            static const char * jpageNames[3] =
                                 {"read", "write", "verify"};
            jout("%s%8" PRIu64 " %8" PRIu64 "  %8" PRIu64 "  %8" PRIu64
                 "   %8" PRIu64, pageNames[k], ecp->counter[0],
                 ecp->counter[1], ecp->counter[2], ecp->counter[3],
                 ecp->counter[4]);
            double processed_gb = ecp->counter[5] / 1000000000.0;
            jout("   %12.3f    %8" PRIu64 "\n", processed_gb,
                 ecp->counter[6]);
            // Error counter log info
            jref[jpageNames[k]]["errors_corrected_by_eccfast"] = ecp->counter[0];
            jref[jpageNames[k]]["errors_corrected_by_eccdelayed"] = ecp->counter[1];
            jref[jpageNames[k]]["errors_corrected_by_rereads_rewrites"] = ecp->counter[2];
            jref[jpageNames[k]]["total_errors_corrected"] = ecp->counter[3];
            jref[jpageNames[k]]["correction_algorithm_invocations"] = ecp->counter[4];
            jref[jpageNames[k]]["gigabytes_processed"] = strprintf("%.3f", processed_gb);
            jref[jpageNames[k]]["total_uncorrected_errors"] = ecp->counter[6];
        }
    }
    else
        pout("Error Counter logging not supported\n");
    if (gNonMediumELPage && (0 == scsiLogSense(device,
                NON_MEDIUM_ERROR_LPAGE, 0, gBuf, LOG_RESP_LEN, 0))) {
        struct scsiNonMediumError nme;
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
    if (gLastNErrorEvLPage &&
        (0 == scsiLogSense(device, LAST_N_ERROR_EVENTS_LPAGE, 0, gBuf,
                           LOG_RESP_LONG_LEN, 0))) {
        int num = sg_get_unaligned_be16(gBuf + 2) + 4;
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
                int pc = sg_get_unaligned_be16(ucp + 0);
                if (pl > 4) {
                    if ((ucp[2] & 0x1) && (ucp[2] & 0x2)) {
                        pout("  Error event %d:\n", pc);
                        pout("    [binary]:\n");
                        dStrHex((const uint8_t *)ucp + 4, pl - 4, 1);
                    } else if (ucp[2] & 0x1) {
                        pout("  Error event %d:\n", pc);
                        pout("    %.*s\n", pl - 4, (const char *)(ucp + 4));
                    } else {
                        if (scsi_debugmode > 0) {
                            pout("  Error event %d:\n", pc);
                            pout("    [data counter??]:\n");
                            dStrHex((const uint8_t *)ucp + 4, pl - 4, 1);
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
        "Failed in segment",            /* special handling for result 7 */
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
    bool noheader = true;
    int num, k, err, durationSec;
    int retval = 0;
    uint8_t * ucp;
    struct scsi_sense_disect sense_info;
    static const char * hname = "Self-test";
    static const char * fixup_stres7 = " -->    ";  /* only for non-json */

    // check if test is running
    if (!scsiRequestSense(device, &sense_info) &&
                        (sense_info.asc == 0x04 && sense_info.ascq == 0x09 &&
                        sense_info.progress != -1)) {
        pout("%s execution status:\t\t%d%% of test remaining\n", hname,
             100 - ((sense_info.progress * 100) / 65535));
    }

    if ((err = scsiLogSense(device, SELFTEST_RESULTS_LPAGE, 0, gBuf,
                            LOG_RESP_SELF_TEST_LEN, 0))) {
        print_on();
        pout("%s: Failed [%s]\n", __func__, scsiErrString(err));
        print_off();
        return FAILSMART;
    }
    if ((gBuf[0] & 0x3f) != SELFTEST_RESULTS_LPAGE) {
        print_on();
        pout("%s %s, page mismatch\n", hname, logSenRspStr);
        print_off();
        return FAILSMART;
    }
    // compute page length
    num = sg_get_unaligned_be16(gBuf + 2);
    // Log sense page length 0x190 bytes
    if (num != 0x190) {
        print_on();
        pout("%s %s length is 0x%x not 0x190 bytes\n", hname, logSenStr, num);
        print_off();
        return FAILSMART;
    }
    // loop through the twenty possible entries
    for (k = 0, ucp = gBuf + 4; k < 20; ++k, ucp += 20 ) {
        // timestamp in power-on hours (or zero if test in progress)
        unsigned int poh = sg_get_unaligned_be16(ucp + 6);
        unsigned int u, tr;
        char st[32];

        snprintf(st, sizeof(st), "scsi_self_test_%d", k);
        // The spec says "all 20 bytes will be zero if no test" but
        // DG has found otherwise.  So this is a heuristic.
        if ((0 == poh) && (0 == ucp[4]))
            break;

        // only print header if needed
        if (noheader) {
            jout("SMART %s log\n", hname);
            jout("Num  Test              Status                 segment  "
                   "LifeTime  LBA_first_err [SK ASC ASQ]\n");
            jout("     Description                              number   "
                   "(hours)\n");
            noheader = false;
        }

        // print parameter code (test number) & self-test code text
        u = (ucp[4] >> 5) & 0x7;
        jout("#%2d  %s", sg_get_unaligned_be16(ucp + 0), self_test_code[u]);
        jglb[st]["code"]["value"] = u;
        jglb[st]["code"]["string"] = rtrim(self_test_code[u]);

        // check the self-test result nibble, using the self-test results
        // field table from T10/1416-D (SPC-3) Rev. 23, section 7.2.10:
        tr = ucp[4] & 0xf;
        switch (tr) {
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
        jout("  %s%s", self_test_result[tr], (tr == 7 ? fixup_stres7 : ""));
        jglb[st]["result"]["value"] = tr;
        jglb[st]["result"]["string"] = rtrim(self_test_result[tr]);

        // self-test number identifies test that failed and consists
        // of either the number of the segment that failed during
        // the test, or the number of the test that failed and the
        // number of the segment in which the test was run, using a
        // vendor-specific method of putting both numbers into a
        // single byte.
        u = ucp[5];
        if (u > 0) {
            jout(" %3u",  u);
            jglb[st]["failed_segment"]["value"] = u;
            jglb[st]["failed_segment"]["aka"] = "self_test_number";
        } else
            jout("   -");

        // print time that the self-test was completed
        if (poh==0 && tr==0xf) {
        // self-test in progress
            jout("     NOW");
            jglb[st]["self_test_in_progress"] = true;
        } else {
            jout("   %5d", poh);
            jglb[st]["power_on_time"]["hours"] = poh;
            jglb[st]["power_on_time"]["aka"] = "accumulated_power_on_hours";
        }

        // construct 8-byte integer address of first failure
        uint64_t ull = sg_get_unaligned_be64(ucp + 8);
        bool is_all_ffs = all_ffs(ucp + 8, 8);
        // print Address of First Failure, if sensible
        if ((! is_all_ffs) && (tr > 0) && (tr < 0xf)) {
            char buff[32];

            // was hex but change to decimal to conform with ATA
            snprintf(buff, sizeof(buff), "%" PRIu64, ull);
            // snprintf(buff, sizeof(buff), "0x%" PRIx64, ull);
            jout("%18s", buff);
            jglb[st]["lba_first_failure"]["value"] = ull;
            jglb[st]["lba_first_failure"]["aka"] = "address_of_first_failure";
        } else
            jout("                 -");

        // if sense key nonzero, then print it, along with
        // additional sense code and additional sense code qualifier
        if (ucp[16] & 0xf) {
            char b[48];

            jout(" [0x%x 0x%x 0x%x]\n", ucp[16] & 0xf, ucp[17], ucp[18]);
            u = ucp[16] & 0xf;
            jglb[st]["sense_key"]["value"] = u;
            jglb[st]["sense_key"]["string"] = 
                        scsi_get_sense_key_str(u, sizeof(b), b);
            jglb[st]["asc"] = ucp[17];
            jglb[st]["ascq"] = ucp[18];
            jglb[st]["vendor_specific"] = ucp[19];
        } else
            pout(" [-   -    -]\n");
    }

    // if header never printed, then there was no output
    if (noheader)
        jout("No %ss have been logged\n", hname);
    else if ((0 == scsiFetchExtendedSelfTestTime(device, &durationSec,
                        modese_len)) && (durationSec > 0)) {
        if (durationSec > 14400)
            jout("\nLong (extended) %s duration: %d seconds "
                 "[%.1f hours]\n", hname, durationSec, durationSec / 3600.0);
        else
            jout("\nLong (extended) %s duration: %d seconds "
                 "[%.1f minutes]\n", hname, durationSec, durationSec / 60.0);
        jglb["scsi_extended_self_test_seconds"] = durationSec;
    }
    jout("\n");
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
// When only_pow_time is true only print "Accumulated power on time"
// data, if available.
static int
scsiPrintBackgroundResults(scsi_device * device, bool only_pow_time)
{
    bool noheader = true;
    bool firstresult = true;
    int num, j, m, err, truncated;
    int retval = 0;
    unsigned int u;
    uint64_t lba;
    uint8_t * ucp;
    char b[48];
    char res_s[32];
    static const char * hname = "Background scan results";
    static const char * jname = "scsi_background_scan";

    if ((err = scsiLogSense(device, BACKGROUND_RESULTS_LPAGE, 0, gBuf,
                            LOG_RESP_LONG_LEN, 0))) {
        print_on();
        pout("%s Failed [%s]\n", __func__, scsiErrString(err));
        print_off();
        return FAILSMART;
    }
    if ((gBuf[0] & 0x3f) != BACKGROUND_RESULTS_LPAGE) {
        print_on();
        pout("%s %s, page mismatch\n", hname, logSenRspStr);
        print_off();
        return FAILSMART;
    }
    // compute page length
    num = sg_get_unaligned_be16(gBuf + 2) + 4;
    if (num < 20) {
        if (! only_pow_time) {
            print_on();
            pout("%s %s length is %d, no scan status\n", hname, logSenStr,
                 num);
            print_off();
        }
        return FAILSMART;
    }
    truncated = (num > LOG_RESP_LONG_LEN) ? num : 0;
    if (truncated)
        num = LOG_RESP_LONG_LEN;
    ucp = gBuf + 4;
    num -= 4;
    while (num > 3) {
        int pc = sg_get_unaligned_be16(ucp + 0);
        // pcb = ucp[2];
        int pl = ucp[3] + 4;
        switch (pc) {
        case 0:
            if (noheader) {
                noheader = false;
                if (! only_pow_time)
                    jout("%s log\n", hname);
            }
            if (! only_pow_time)
                jout("  Status: ");
            if ((pl < 16) || (num < 16)) {
                if (! only_pow_time)
                    jout("\n");
                break;
            }
            j = ucp[9];
            if (! only_pow_time) {
                if (j < (int)ARRAY_SIZE(bms_status)) {
                    jout("%s\n", bms_status[j]);
                    jglb[jname]["status"]["value"] = j;
                    jglb[jname]["status"]["string"] = bms_status[j];
                } else {
                    jout("unknown [0x%x] background scan status value\n", j);
                    jglb[jname]["status"]["value"] = j;
                }
            }
            j = sg_get_unaligned_be32(ucp + 4);
            jout("%sAccumulated power on time, hours:minutes %d:%02d",
                 (only_pow_time ? "" : "    "), (j / 60), (j % 60));
            if (only_pow_time)
                jout("\n");
            else
                jout(" [%d minutes]\n", j);
            jglb["power_on_time"]["hours"] = j / 60;
            jglb["power_on_time"]["minutes"] = j % 60;
            if (only_pow_time)
                break;
            u = sg_get_unaligned_be16(ucp + 10);
            jout("    Number of background scans performed: %u,  ", u);
            jglb[jname]["status"]["number_scans_performed"] = u;
            u = sg_get_unaligned_be16(ucp + 12);
            snprintf(b, sizeof(b), "%.2f%%", (double)u * 100.0 / 65536.0);
            jout("scan progress: %s\n", b);
            jglb[jname]["status"]["scan_progress"] = b;
            u = sg_get_unaligned_be16(ucp + 14);
            jout("    Number of background medium scans performed: %d\n", u);
            jglb[jname]["status"]["number_medium_scans_performed"] = u;
            break;
        default:
            if (noheader) {
                noheader = false;
                if (! only_pow_time)
                    jout("\n%s log\n", hname);
            }
            if (only_pow_time)
                break;
            if (firstresult) {
                firstresult = 0;
                jout("\n   #  when        lba(hex)    [sk,asc,ascq]    "
                     "reassign_status\n");
            }
            snprintf(res_s, sizeof(res_s), "result_%d", pc);
            jout(" %3d ", pc);
            jglb[jname][res_s]["parameter_code"] = pc;
            if ((pl < 24) || (num < 24)) {
                if (pl < 24)
                    jout("parameter length >= 24 expected, got %d\n", pl);
                break;
            }
            u = sg_get_unaligned_be32(ucp + 4);
            jout("%4u:%02u  ", (u / 60), (u % 60));
            jglb[jname][res_s]["accumulated_power_on"]["minutes"] = u;
            for (m = 0; m < 8; ++m)
                jout("%02x", ucp[16 + m]);
            lba = sg_get_unaligned_be64(ucp + 16);
            jglb[jname][res_s]["lba"] = lba;
            u = ucp[8] & 0xf;
            jout("  [%x,%x,%x]   ", u, ucp[9], ucp[10]);
            jglb[jname][res_s]["sense_key"]["value"] = u;
            jglb[jname][res_s]["sense_key"]["string"] =
                        scsi_get_sense_key_str(u, sizeof(b), b);
            jglb[jname][res_s]["asc"] = ucp[9];
            jglb[jname][res_s]["ascq"] = ucp[10];
            u = (ucp[8] >> 4) & 0xf;
            if (u < ARRAY_SIZE(reassign_status)) {
                jout("%s\n", reassign_status[u]);
                jglb[jname][res_s]["reassign_status"]["value"] = u;
                jglb[jname][res_s]["reassign_status"]["string"] =
                                                        reassign_status[u];
            } else {
                jout("Reassign status: reserved [0x%x]\n", u);
                jglb[jname][res_s]["reassign_status"]["value"] = u;
            }
            break;
        }
        num -= pl;
        ucp += pl;
    }
    if (truncated && (! only_pow_time))
        jout(" >>>> log truncated, fetched %d of %d available "
             "bytes\n", LOG_RESP_LONG_LEN, truncated);
    if (! only_pow_time)
        jout("\n");
    return retval;
}

// See SCSI Block Commands - 3 (SBC-3) rev 27 (draft) section 6.3.6 .
// Returns 0 if ok else FAIL* bitmask. Note can have a status entry
// and up to 2048 events (although would hope to have less). May set
// FAILLOG if serious errors detected (in the future).
static int
scsiPrintSSMedia(scsi_device * device)
{
    int num, err, truncated;
    int retval = 0;
    uint8_t * ucp;
    const char * q;
    static const char * hname = "Solid state media";

    if ((err = scsiLogSense(device, SS_MEDIA_LPAGE, 0, gBuf,
                            LOG_RESP_LONG_LEN, 0))) {
        print_on();
        pout("%s: Failed [%s]\n", __func__, scsiErrString(err));
        print_off();
        return FAILSMART;
    }
    if ((gBuf[0] & 0x3f) != SS_MEDIA_LPAGE) {
        print_on();
        pout("%s %s, page mismatch\n", hname, logSenStr);
        print_off();
        return FAILSMART;
    }
    // compute page length
    num = sg_get_unaligned_be16(gBuf + 2) + 4;
    if (num < 12) {
        print_on();
        pout("%s %s length is %d, too short\n", hname, logSenStr, num);
        print_off();
        return FAILSMART;
    }
    truncated = (num > LOG_RESP_LONG_LEN) ? num : 0;
    if (truncated)
        num = LOG_RESP_LONG_LEN;
    ucp = gBuf + 4;
    num -= 4;
    while (num > 3) {
        int pc = sg_get_unaligned_be16(ucp + 0);
        // pcb = ucp[2];
        int pl = ucp[3] + 4;
        switch (pc) {
        case 1:
            if (pl < 8) {
                print_on();
                pout("%s Percentage used endurance indicator parameter "
                     "too short (pl=%d)\n", hname, pl);
                print_off();
                return FAILSMART;
            }
            q = "Percentage used endurance indicator";
            jout("%s: %d%%\n", q, ucp[7]);
            jglb[std::string("scsi_") + q] = ucp[7];
        default:        /* ignore other parameter codes */
            break;
        }
        num -= pl;
        ucp += pl;
    }
    return retval;
}

static int
scsiPrintZBDeviceStats(scsi_device * device)
{
    int num, err, truncated;
    int retval = 0;
    uint32_t u;
    uint8_t * ucp;
    const char * q;
    static const char * hname = "Zoned block device statistics";
    static const char * jname = "scsi_zoned_block_device_statistics";

    jout("%s:\n", hname);
    if ((err = scsiLogSense(device, DEVICE_STATS_LPAGE, ZB_DEV_STATS_L_SPAGE,
                            gBuf, LOG_RESP_LONG_LEN, 0))) {
        print_on();
        pout("%s: Failed [%s]\n", __func__, scsiErrString(err));
        print_off();
        return FAILSMART;
    }
    if (((gBuf[0] & 0x3f) != DEVICE_STATS_LPAGE) &&
        (gBuf[1] == ZB_DEV_STATS_L_SPAGE)) {
        print_on();
        pout("%s %s, page mismatch\n", hname, logSenStr);
        print_off();
        return FAILSMART;
    }
    // compute page length
    num = sg_get_unaligned_be16(gBuf + 2) + 4;
    if (num < 12) {
        print_on();
        pout("%s %s length is %d, too short\n", hname, logSenStr, num);
        print_off();
        return FAILSMART;
    }
    truncated = (num > LOG_RESP_LONG_LEN) ? num : 0;
    if (truncated)
        num = LOG_RESP_LONG_LEN;
    ucp = gBuf + 4;
    num -= 4;
    while (num > 3) {
        int pc = sg_get_unaligned_be16(ucp + 0);
        // pcb = ucp[2];
        int pl = ucp[3] + 4;

        if (pl < 12)
            goto skip;  /* DC HC650 has non-compliant 4 byte parameters */
        switch (pc) {
        case 0:
            q = "Maximum open zones";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        case 1:
            q = "Maximum explicitly open zones";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        case 2:
            q = "Maximum implicitly open zones";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        case 3:
            q = "Minimum empty zones";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        case 4:
            q = "Maximum nonseq zones";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        case 5:
            q = "Zones emptied";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        case 6:
            q = "Suboptimal write commands";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        case 7:
            q = "Commands exceeding optinmal limit";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        case 8:
            q = "Failed explicit opens";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        case 9:
            q = "Read rule violations";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        case 0xa:
            q = "Write rule violations";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        case 0xb:
            q = "Maximum implicitly open sequential or before required zones";
            u = sg_get_unaligned_be32(ucp + 8);
            jout("    %s: %u\n", q, u);
            jglb[jname][q] = u;
            break;
        default:        /* ignore other parameter codes */
            break;
        }
skip:
        num -= pl;
        ucp += pl;
    }
    return retval;
}

static int
scsiPrintTapeDeviceStats(scsi_device * device)
{
    int num, err, truncated;
    int retval = 0;
    uint32_t k, n, u;
    uint64_t ull;
    uint8_t * ucp;
    const char * q;
    static const char * hname = "Device statistics (SSC, tape)";
    static const char * jname = "scsi_device_statistics";

    if ((err = scsiLogSense(device, DEVICE_STATS_LPAGE, 0,
                            gBuf, LOG_RESP_LONG_LEN, 0))) {
        print_on();
        pout("%s: Failed [%s]\n", __func__, scsiErrString(err));
        print_off();
        return FAILSMART;
    }
    if (((gBuf[0] & 0x3f) != DEVICE_STATS_LPAGE) &&
        (gBuf[1] != 0)) {
        print_on();
        pout("%s %s, page mismatch\n", hname, logSenStr);
        print_off();
        return FAILSMART;
    }
    // compute page length
    num = sg_get_unaligned_be16(gBuf + 2) + 4;
    if (num < 12) {
        print_on();
        pout("%s %s length is %d, too short\n", hname, logSenStr, num);
        print_off();
        return FAILSMART;
    }
    truncated = (num > LOG_RESP_LONG_LEN) ? num : 0;
    if (truncated)
        num = LOG_RESP_LONG_LEN;
    ucp = gBuf + 4;
    num -= 4;
    while (num > 3) {
        int pc = sg_get_unaligned_be16(ucp + 0);
        // pcb = ucp[2];
        int pl = ucp[3] + 4;
        switch (pc) {
        case 0:
            q = "Lifetime volume loads";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 1:
            q = "Lifetime cleaning operations";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 2:
            q = "Lifetime power on hours";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 3:
            q = "Lifetime medium motion hours";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 4:
            q = "Lifetime meters of tape processed";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 5:
            q = "Lifetime medium motion hours at last incompatible volume "
                "load";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 6:
            q = "Lifetime power on hours at last temperature condition "
                "occurrence";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 7:
            q = "Lifetime power on hours at last power consumption condition "
                "occurrence";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 8:
            q = "Medium motion hours since last successful cleaning "
                "operation";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 9:
            q = "Medium motion hours since second to last successful "
                "cleaning operation";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0xa:
            q = "Medium motion hours since third to last successful "
                "cleaning operation";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0xb:
            q = "Lifetime power on hours at last operator initiated forced "
                "reset and/or emergency eject occurrence";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0xc:
            q = "Lifetime power cycles";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0xd:
            q = "Volume loads since last parameter reset";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0xe:
            q = "Hard write errors";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0xf:
            q = "Hard read errors";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0x10:
            q = "Duty cycle sample time";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0x11:
            q = "Read duty cycle";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0x12:
            q = "Write duty cycle";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0x13:
            q = "Activity duty cycle";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0x14:
            q = "Volume not present duty cycle";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0x15:
            q = "Ready duty cycle";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0x16:
            q = "Megabytes transferred from application client in duty cycle"
                "sample time";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0x17:
            q = "Megabytes transferred to application client in duty cycle"
                "sample time";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0x40:
            {
                std::string s((const char *)(ucp + 4), ucp[3]);
                q = "Drive manufacturer's serial number";
                jout("    %s: %s\n", q, s.c_str());
                jglb[jname][q] = s;
            }
            break;
        case 0x41:
            {
                std::string s((const char *)(ucp + 4), ucp[3]);
                q = "Drive serial number";
                jout("    %s: %s\n", q, s.c_str());
                jglb[jname][q] = s;
            }
            break;
        case 0x42:
            {
                std::string s((const char *)(ucp + 4), ucp[3]);
                q = "Manufacturing date year,month,day";
                jout("    %s: %s\n", q, s.c_str());
                jglb[jname][q] = s;
            }
            break;
        case 0x43:
            {
                std::string s((const char *)(ucp + 4), ucp[3]);
                q = "Manufacturing date year,week";
                jout("    %s: %s\n", q, s.c_str());
                jglb[jname][q] = s;
            }
            break;
        case 0x44:
            {
                std::string s((const char *)(ucp + 4), ucp[3]);
                q = "Manufacturing date year,week";
                jout("    %s: %s\n", q, s.c_str());
                jglb[jname][q] = s;
            }
            break;
        case 0x80:
            q = "Medium removal prevented";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0x81:
            q = "Maximum recommeded mechanism temperature exceeded";
            ull = variableLengthIntegerParam(ucp);
            jout("    %s: %" PRIu64 "\n", q, ull);
            jglb[jname][q] = ull;
            break;
        case 0x1000:
            q = "Medium motion hours for each medium type";
            n = ucp[3] / 8;
            jout("    %s, number of element: %u\n", q, n);
            for (k = 0; k < n; ++k, ucp += 8) {
                u = sg_get_unaligned_be32(ucp + 8);
                jout("      [%d] density code: %u, density code: %u, hours: "
                     "%u\n", k + 1, ucp[6], ucp[7], u);
                jglb[jname][q][k]["density code"] = ucp[6];
                jglb[jname][q][k]["medium type"] = ucp[7];
                jglb[jname][q][k]["medium motion hours"] = u;
            }
            break;
        default:        /* ignore other parameter codes */
            break;
        }
        num -= pl;
        ucp += pl;
    }
    return retval;
}

static int
scsiPrintFormatStatus(scsi_device * device)
{
    int num, err, truncated;
    int retval = 0;
    uint64_t ull;
    uint8_t * ucp;
    static const char * hname = "Format Status";
    static const char * jname = "scsi_format_status";

    if ((err = scsiLogSense(device, FORMAT_STATUS_LPAGE, 0, gBuf,
                            LOG_RESP_LONG_LEN, 0))) {
        print_on();
        jout("%s: Failed [%s]\n", __func__, scsiErrString(err));
        print_off();
        return FAILSMART;
    }
    if ((gBuf[0] & 0x3f) != FORMAT_STATUS_LPAGE) {
        print_on();
        jout("%s %s, page mismatch\n", hname, logSenRspStr);
        print_off();
        return FAILSMART;
    }
    // compute page length
    num = sg_get_unaligned_be16(gBuf + 2) + 4;
    if (num < 12) {
        print_on();
        jout("%s %s length is %d, too short\n", hname, logSenStr, num);
        print_off();
        return FAILSMART;
    }
    truncated = (num > LOG_RESP_LONG_LEN) ? num : 0;
    if (truncated)
        num = LOG_RESP_LONG_LEN;
    ucp = gBuf + 4;
    num -= 4;
    while (num > 3) {
        int pc = sg_get_unaligned_be16(ucp + 0);
        // pcb = ucp[2];
        int pl = ucp[3] + 4;

        bool is_count = true;
        const char * jout_str = "";
        const char * jglb_str = "x";
        switch (pc) {
        case 0:
            if (scsi_debugmode > 1) {
                if (pl < 5)
                    jout("Format data out: <empty>\n");
                else {
                    if (all_ffs(ucp + 4, pl - 4))
                        jout("Format data out: <not available>\n");
                    else {
                        jout("Format data out:\n");
                        dStrHex((const uint8_t *)ucp + 4, pl - 4, 0);
                    }
                }
            }
            is_count = false;
            break;
        case 1:
            jout_str = "Grown defects during certification";
            jglb_str = "grown_defects_during_cert";
            break;
        case 2:
            jout_str = "Total blocks reassigned during format";
            jglb_str = "blocks_reassigned_during_format";
            break;
        case 3:
            jout_str = "Total new blocks reassigned";
            jglb_str = "total_new_block_since_format";
            break;
        case 4:
            jout_str = "Power on minutes since format";
            jglb_str = "power_on_minutes_since_format";
            break;
        default:
            if (scsi_debugmode > 3) {
                pout("  Unknown Format parameter code = 0x%x\n", pc);
                dStrHex((const uint8_t *)ucp, pl, 0);
            }
            is_count = false;
            break;
        }
        if (is_count) {
            if (all_ffs(ucp + 4, ucp[3])) {
                pout("%s <not available>\n", jout_str);
            } else {
                ull = variableLengthIntegerParam(ucp);
                jout("%s = %" PRIu64 "\n", jout_str, ull);
                jglb[jname][jglb_str] = ull;
            }
        } else
        num -= pl;
        ucp += pl;
    }
    return retval;

}

static void
show_sas_phy_event_info(const json::ref & jref, int peis, unsigned int val,
                        unsigned thresh_val)
{
    unsigned int u;
    const char * q;
    static const char * pvd_th = "Peak value detector threshold";
    static const char * pvd_th_j = "pvd_threshold";

    switch (peis) {
    case 0:
        jout("     No event\n");
        break;
    case 0x1:   /* 0x1 to 0x4 will be duplicates so append "_2" to name */
        q = "Invalid dword count";
        jout("     %s: %u\n", q, val);
        jref[std::string(q) + "_2"] = val;
        break;
    case 0x2:
        q = "Running disparity error count";
        jout("     %s: %u\n", q, val);
        jref[std::string(q) + "_2"] = val;
        break;
    case 0x3:
        q = "Loss of dword synchronization count";
        jout("     %s: %u\n", q, val);
        jref[std::string(q) + "_2"] = val;
        break;
    case 0x4:
        q = "Phy reset problem count";
        jout("     %s: %u\n", q, val);
        jref[std::string(q) + "_2"] = val;
        break;
    case 0x5:
        q = "Elasticity buffer overflow count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x6:
        q = "Received ERROR count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x20:
        q = "Received address frame error count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x21:
        q = "Transmitted abandon-class OPEN_REJECT count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x22:
        q = "Received abandon-class OPEN_REJECT count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x23:
        q = "Transmitted retry-class OPEN_REJECT count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x24:
        q = "Received retry-class OPEN_REJECT count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x25:
        q = "Received AIP (WAITING ON PARTIAL) count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x26:
        q = "Received AIP (WAITING ON CONNECTION) count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x27:
        q = "Transmitted BREAK count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x28:
        q = "Received BREAK count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x29:
        q = "Break timeout count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x2a:
        q = "Connection count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x2b:
        q = "Peak transmitted pathway blocked";
        jout("     %s count: %u\n", q, val & 0xff);
        jout("         %s: %u\n", pvd_th, thresh_val & 0xff);
        jref[q]["count"] = val & 0xff;
        jref[q][pvd_th_j] = thresh_val & 0xff;
        break;
    case 0x2c:
        q = "Peak transmitted arbitration wait time";
        u = val & 0xffff;
        if (u < 0x8000) {
            jout("     %s (us): %u\n", q, u);
            jref[std::string(q) + "_us"]["event"] = u;
        } else {
            jout("     %s (ms): %u\n", q, 33 + (u - 0x8000));
            jref[std::string(q) + "_ms"]["event"] = 33 + (u - 0x8000);
        }
        u = thresh_val & 0xffff;
        if (u < 0x8000) {
            jout("         %s (us): %u\n", pvd_th, u);
            jref[std::string(q) + "_us"][pvd_th_j] = u;
        } else {
            jout("         %s (ms): %u\n", pvd_th, 33 + (u - 0x8000));
            jref[std::string(q) + "_ms"][pvd_th_j] = 33 + (u - 0x8000);
        }
        break;
    case 0x2d:
        q = "Peak arbitration time";
        jout("     %s (us): %u\n", q, val);
        jref[std::string(q) + "_us"]["event"] = val;
        jout("         %s: %u\n", pvd_th, thresh_val);
        jref[std::string(q) + "_us"][pvd_th_j] = thresh_val;
        break;
    case 0x2e:
        q = "Peak connection time";
        jout("     %s (us): %u\n", q, val);
        jref[std::string(q) + "_us"]["event"] = val;
        jout("         %s: %u\n", pvd_th, thresh_val);
        jref[std::string(q) + "_us"][pvd_th_j] = thresh_val;
        break;
    case 0x40:
        q = "Transmitted SSP frame count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x41:
        q = "Received SSP frame count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x42:
        q = "Transmitted SSP frame error count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x43:
        q = "Received SSP frame error count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x44:
        q = "Transmitted CREDIT_BLOCKED count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x45:
        q = "Received CREDIT_BLOCKED count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x50:
        q = "Transmitted SATA frame count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x51:
        q = "Received SATA frame count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x52:
        q = "SATA flow control buffer overflow count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x60:
        q = "Transmitted SMP frame count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x61:
        q = "Received SMP frame count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    case 0x63:
        q = "Received SMP frame error count";
        jout("     %s: %u\n", q, val);
        jref[q] = val;
        break;
    default:
        break;
    }
}

static void
show_sas_port_param(int port_num, unsigned char * ucp, int param_len)
{
    int k, j, m, nphys, t, sz, spld_len;
    char pn[32];
    unsigned char * vcp;
    char s[64];
    const char * q;

    snprintf(pn, sizeof(pn), "scsi_sas_port_%d", port_num);
    sz = sizeof(s);
    // pcb = ucp[2];
    t = sg_get_unaligned_be16(ucp + 0);
    jout("relative target port id = %d\n", t);
    jglb[pn]["relative_target_port_id"] = t;
    jout("  generation code = %d\n", ucp[6]);
    jglb[pn]["generation_code"] = ucp[6];
    nphys = ucp[7];
    jout("  number of phys = %d\n", nphys);
    jglb[pn]["number_of_phys"] = nphys;

    for (j = 0, k = 0, vcp = ucp + 8; j < (param_len - 8);
         vcp += spld_len, j += spld_len, ++k) {
        char yn[32];

        snprintf(yn, sizeof(yn), "phy_%d", k);
        json::ref jref = jglb[pn][yn];
        jout("  phy identifier = %d\n", vcp[1]);
        jref["identifier"] = vcp[1];
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
        q = "attached device type";
        jout("    %s: %s\n", q, s);
        jref[q] = s;
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
        q = "attached reason";
        jout("    %s: %s\n", q, s);
        jref[q] = s;
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
        q = "reason";
        jout("    %s: %s\n", q, s);
        jref[q] = s;
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
        q = "negotiated logical link rate";
        jout("    %s: %s\n", q, s);
        jref[q] = s;
        q = "attached initiator port";
        jout("    %s: ssp=%d stp=%d smp=%d\n", q,
               !! (vcp[6] & 8), !! (vcp[6] & 4), !! (vcp[6] & 2));
        snprintf(s, sz, "%03d", ((vcp[6] & 8) ? 100 : 0) +
                 ((vcp[6] & 4) ? 10 : 0) + ((vcp[6] & 2) ? 1 : 0));
        jref[q]["ssp_stp_smp"] = s;
        q = "attached target port";
        jout("    %s: ssp=%d stp=%d smp=%d\n", q,
               !! (vcp[7] & 8), !! (vcp[7] & 4), !! (vcp[7] & 2));
        snprintf(s, sz, "%03d", ((vcp[7] & 8) ? 100 : 0) +
                 ((vcp[7] & 4) ? 10 : 0) + ((vcp[7] & 2) ? 1 : 0));
        jref[q]["ssp_stp_smp"] = s;
        if (!dont_print_serial_number) {
            uint64_t ull = sg_get_unaligned_be64(vcp + 8);
            char b[32];

            snprintf(b, sizeof(b), "0x%" PRIx64, ull);
            q = "SAS address";
            jout("    %s = %s\n", q, b);
            jref[q] = b;
            ull = sg_get_unaligned_be64(vcp + 16);
            snprintf(b, sizeof(b), "0x%" PRIx64, ull);
            q = "attached SAS address";
            jout("    %s = %s\n", q, b);
            jref[q] = b;
        }
        q = "attached phy identifier";
        jout("    %s = %d\n", q, vcp[24]);
        jref[q] = vcp[24];
        unsigned int ui = sg_get_unaligned_be32(vcp + 32);

        q = "Invalid DWORD count";
        jout("    %s = %u\n", q, ui);
        jref[q] = ui;
        ui = sg_get_unaligned_be32(vcp + 36);
        q = "Running disparity error count";
        jout("    %s = %u\n", q, ui);
        jref[q] = ui;
        ui = sg_get_unaligned_be32(vcp + 40);
        q = "Loss of DWORD synchronization count";
        jout("    %s = %u\n", q, ui);
        jref[q] = ui;
        ui = sg_get_unaligned_be32(vcp + 44);
        q = "Phy reset problem count";
        jout("    %s = %u\n", q, ui);
        jref[q] = ui;
        if (spld_len > 51) {
            bool header_given = false;
            bool allow_dupl = (scsi_debugmode > 0);
            int num_ped;
            unsigned char * xcp;

            num_ped = vcp[51];
            xcp = vcp + 52;
            for (m = 0; m < (num_ped * 12); m += 12, xcp += 12) {
                int peis;
                unsigned int pvdt;

                peis = xcp[3];
                ui = sg_get_unaligned_be32(xcp + 4);
                pvdt = sg_get_unaligned_be32(xcp + 8);
                if (allow_dupl || (peis > 0x4)) {
                    if (! header_given) {
                        header_given = true;
                        jout("    Phy event descriptors:\n");
                    }
                    show_sas_phy_event_info(jref, peis, ui, pvdt);
                }
            }
        }
    }
}

// Returns 1 if okay, 0 if non SAS descriptors
static int
show_protocol_specific_port_page(unsigned char * resp, int len)
{
    int k, j, num;
    unsigned char * ucp;

    num = len - 4;
    for (k = 0, j = 0, ucp = resp + 4; k < num; ++j) {
        int param_len = ucp[3] + 4;
        if (SCSI_TPROTO_SAS != (0xf & ucp[4]))
            return 0;   /* only decode SAS log page */
        if (0 == k)
            jout("Protocol Specific port log page for SAS SSP\n");
        show_sas_port_param(j, ucp, param_len);
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
    static const char * hname = "Protocol specific port";

    if ((err = scsiLogSense(device, PROTOCOL_SPECIFIC_LPAGE, 0, gBuf,
                            LOG_RESP_LONG_LEN, 0))) {
        print_on();
        pout("%s %s Failed [%s]\n\n", __func__, logSenStr,
             scsiErrString(err));
        print_off();
        return FAILSMART;
    }
    if ((gBuf[0] & 0x3f) != PROTOCOL_SPECIFIC_LPAGE) {
        print_on();
        pout("%s %s, page mismatch\n\n", hname, logSenRspStr);
        print_off();
        return FAILSMART;
    }
    // compute page length
    num = sg_get_unaligned_be16(gBuf + 2);
    if (1 != show_protocol_specific_port_page(gBuf, num + 4)) {
        print_on();
        pout("Only support %s log page on SAS devices\n\n", hname);
        print_off();
        return FAILSMART;
    }
    if (reset) {
        if ((err = scsiLogSelect(device, 1 /* pcr */, 0 /* sp */, 0 /* pc */,
                                 PROTOCOL_SPECIFIC_LPAGE, 0, NULL, 0))) {
            print_on();
            pout("%s Log Select (reset) Failed [%s]\n\n", __func__,
                 scsiErrString(err));
            print_off();
            return FAILSMART;
        }
    }
    return 0;
}


static const char * peripheral_dt_arr[32] = {
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
        "optical card reader",
        "reserved [0x10]",
        "object based storage",
        "automation/driver interface",
        "security manager device",
        "host managed zoned block device",
        "reserved [0x15]",
        "reserved [0x16]",
        "reserved [0x17]",
        "reserved [0x18]",
        "reserved [0x19]",
        "reserved [0x1a]",
        "reserved [0x1b]",
        "reserved [0x1c]",
        "reserved [0x1d]",
        "well known logical unit",
        "unknown or no device type",
};

/* Symbolic indexes to this array SCSI_TPROTO_* in scscmds.h */
static const char * transport_proto_arr[] = {
        "Fibre channel (FCP-4)",
        "Parallel SCSI (SPI-4)",        /* obsolete */
        "SSA",
        "IEEE 1394 (SBP-3)",
        "RDMA (SRP)",
        "iSCSI",
        "SAS (SPL-4)",
        "ADT",
        "ATA (ACS-2)",
        "UAS",
        "SOP",
        "PCIe",
        "0xc",
        "0xd",
        "0xe",
        "None given [0xf]"
};

/* Returns 0 on success, 1 on general error and 2 for early, clean exit */
static int
scsiGetDriveInfo(scsi_device * device, uint8_t * peripheral_type,
                 bool & have_zbc, bool all)
{
    struct scsi_iec_mode_page iec;
    int err, iec_err, len, req_len, avail_len;
    bool ok;
    bool is_tape = false;
    int peri_dt = 0;
    int transport = -1;
    int form_factor = 0;
    int haw_zbc = 0;
    int protect = 0;
    const char * q;

    memset(gBuf, 0, 96);
    have_zbc = false;
    req_len = 36;
    if ((err = scsiStdInquiry(device, gBuf, req_len))) {
        print_on();
        pout("Standard Inquiry (36 bytes) failed [%s]\n", scsiErrString(err));
        pout("Retrying with a 64 byte Standard Inquiry\n");
        print_off();
        /* Marvell controllers fail with 36 byte StdInquiry, but 64 is ok */
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
    *peripheral_type = peri_dt;
    if (SCSI_PT_HOST_MANAGED == peri_dt)
        have_zbc = true;
    if ((SCSI_PT_SEQUENTIAL_ACCESS == peri_dt) ||
        (SCSI_PT_MEDIUM_CHANGER == peri_dt))
        is_tape = true;

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
        char product[16+1], revision[4+1];
        scsi_format_id_string(scsi_vendor, &gBuf[8], 8);
        scsi_format_id_string(product, &gBuf[16], 16);
        scsi_format_id_string(revision, &gBuf[32], 4);

        pout("=== START OF INFORMATION SECTION ===\n");
        jout("Vendor:               %.8s\n", scsi_vendor);
        jglb["scsi_vendor"] = scsi_vendor;
        jout("Product:              %.16s\n", product);
        jglb["scsi_product"] = product;
        jglb["scsi_model_name"] = strprintf("%s%s%s",
          scsi_vendor, (*scsi_vendor && *product ? " " : ""), product);
        if (gBuf[32] >= ' ') {
            jout("Revision:             %.4s\n", revision);
            // jglb["firmware_version"] = revision;
            jglb["scsi_revision"] = revision;
        }
        if ((scsi_version > 0x3) && (scsi_version < 0x8)) {
            char sv_arr[8];

            snprintf(sv_arr, sizeof(sv_arr), "SPC-%d", scsi_version - 2);
            jout("Compliance:           %s\n", sv_arr);
            jglb["scsi_version"] = sv_arr;
        }
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

    if (! is_tape) {    /* assume disk if not tape drive (or tape changer) */
        struct scsi_readcap_resp srr;
        int lbpme = -1;
        int lbprz = -1;
        unsigned char lb_prov_resp[8];
        uint64_t capacity = scsiGetSize(device, false /*avoid_rcap16 */,
                                        &srr);
        static const char * lb_prov_j = "scsi_lb_provisioning";

        if (capacity) {
            char cap_str[64], si_str[64];
            format_with_thousands_sep(cap_str, sizeof(cap_str), capacity);
            format_capacity(si_str, sizeof(si_str), capacity);
            jout("User Capacity:        %s bytes [%s]\n", cap_str, si_str);
            if (srr.lb_size)
              jglb["user_capacity"]["blocks"].set_unsafe_uint64(capacity /
                                                                srr.lb_size);
            jglb["user_capacity"]["bytes"].set_unsafe_uint64(capacity);
            jout("Logical block size:   %u bytes\n", srr.lb_size);
            jglb["logical_block_size"] = srr.lb_size;
            if (protect || srr.lb_p_pb_exp) {
                if (srr.lb_p_pb_exp > 0) {
                    unsigned pb_size = srr.lb_size * (1 << srr.lb_p_pb_exp);
                    jout("Physical block size:  %u bytes\n", pb_size);
                    jglb["physical_block_size"] = pb_size;
                    if (srr.l_a_lba > 0)  // not common so cut the clutter
                        pout("Lowest aligned LBA:   %u\n", srr.l_a_lba);
                }
                if (srr.prot_type > 0) {
                    switch (srr.prot_type) {
                    case 1 :
                        pout("Formatted with type 1 protection\n");
                        break;
                    case 2 :
                        pout("Formatted with type 2 protection\n");
                        break;
                    case 3 :
                        pout("Formatted with type 3 protection\n");
                        break;
                    default:
                        pout("Formatted with unknown protection type [%d]\n",
                             srr.prot_type);
                        break;
                    }
                    jglb["scsi_protection_type"] = srr.prot_type;
                    unsigned p_i_per_lb = (1 << srr.p_i_exp);
                    const unsigned pi_sz = 8;   /* ref-tag(4 bytes),
                                                   app-tag(2), tag-mask(2) */

                    if (p_i_per_lb > 1) {
                        jout("%d protection information intervals per "
                             "logical block\n", p_i_per_lb);
                        jglb["scsi_protection_intervals_per_lb"] = srr.prot_type;
                    }
                    jout("%d bytes of protection information per logical "
                         "block\n", pi_sz * p_i_per_lb);
                    jglb["scsi_protection_interval_bytes_per_lb"] =
                                                        pi_sz * p_i_per_lb;
                }
                /* Pick up some LB provisioning info since its available */
                lbpme = (int)srr.lbpme;
                lbprz = (int)srr.lbprz;
            }
        }
        /* Thin Provisioning VPD page renamed Logical Block Provisioning VPD
         * page in sbc3r25; some fields changed their meaning so that the
         * new page covered both thin and resource provisioned LUs. */
        if (0 == scsiInquiryVpd(device, SCSI_VPD_LOGICAL_BLOCK_PROVISIONING,
                                lb_prov_resp, sizeof(lb_prov_resp))) {
            int prov_type = lb_prov_resp[6] & 0x7;      /* added sbc3r27 */
            int vpd_lbprz = ((lb_prov_resp[5]  >> 2) & 0x7);  /* sbc4r07 */

            if (-1 == lbprz)
                lbprz = vpd_lbprz;
            else if ((0 == vpd_lbprz) && (1 == lbprz))
                ;  /* vpd_lbprz introduced in sbc3r27, expanded in sbc4r07 */
            else
                lbprz = vpd_lbprz;
            switch (prov_type) {
            case 0:
                if (lbpme <= 0) {
                    jout("LU is fully provisioned");
                    jglb[lb_prov_j]["name"] = "fully provisioned";
                    if (lbprz)
                        jout(" [LBPRZ=%d]\n", lbprz);
                    else
                        jout("\n");
                } else {
                    jout("LB provisioning type: not reported [LBPME=1, "
                         "LBPRZ=%d]\n", lbprz);
                    jglb[lb_prov_j]["name"] = "not reported";
                }
                break;
            case 1:
                jout("LU is resource provisioned, LBPRZ=%d\n", lbprz);
                jglb[lb_prov_j]["name"] = "resource provisioned";
                break;
            case 2:
                jout("LU is thin provisioned, LBPRZ=%d\n", lbprz);
                jglb[lb_prov_j]["name"] = "thin provisioned";
                break;
            default:
                jout("LU provisioning type reserved [%d], LBPRZ=%d\n",
                     prov_type, lbprz);
                jglb[lb_prov_j]["name"] = "reserved";
                break;
            }
            jglb[lb_prov_j]["value"] = prov_type;
            jglb[lb_prov_j]["management_enabled"]["name"] = "LBPME";
            jglb[lb_prov_j]["management_enabled"]["value"] = lbpme;
            jglb[lb_prov_j]["read_zeros"]["name"] = "LBPRZ";
            jglb[lb_prov_j]["read_zeros"]["value"] = lbprz;
        } else if (1 == lbpme) {
            if (scsi_debugmode > 0)
                jout("rcap_16 sets LBPME but no LB provisioning VPD page\n");
            jout("Logical block provisioning enabled, LBPRZ=%d\n", lbprz);
        }

        int rpm = scsiGetRPM(device, modese_len, &form_factor, &haw_zbc);
        if (rpm >= 0) {
            if (0 == rpm)
                ;       // Not reported
            else if (1 == rpm)
                jout("Rotation Rate:        Solid State Device\n");
            else if ((rpm <= 0x400) || (0xffff == rpm))
                ;       // Reserved
            else
                jout("Rotation Rate:        %d rpm\n", rpm);
            jglb["rotation_rate"] = (rpm == 1 ? 0 : rpm);
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
            jglb["form_factor"]["scsi_value"] = form_factor;
            if (cp) {
                jout("Form Factor:          %s inches\n", cp);
                jglb["form_factor"]["name"] = strprintf("%s inches", cp);
            }
        }
        if (haw_zbc == 1) {
            have_zbc = true;
            q = "Host aware zoned block capable";
            jout("%s\n", q);
            jglb[std::string("scsi_") + q] = true;
        } else if (haw_zbc == 2) {
            have_zbc = true;
            q = "Device managed zoned block capable";
            jout("%s\n", q);
            jglb[std::string("scsi_") + q] = true;
        } else {
            supported_vpd_pages * s_vpd_pp = supported_vpd_pages_p;

            if (s_vpd_pp &&
                s_vpd_pp->is_supported(SCSI_VPD_ZONED_BLOCK_DEV_CHAR)) {
                // TODO: need to read that VPD page and look at the
                // 'Zoned block device extension' field

            }
        }
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
            if (strlen(s) > 0) {
                jout("Logical Unit id:      %s\n", s);
                jglb["logical_unit_id"] = s;
            }
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
            jout("Serial number:        %s\n", serial);
            jglb["serial_number"] = serial;
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
    jglb["device_type"]["scsi_terminology"] = "Peripheral Device Type [PDT]";
    jglb["device_type"]["scsi_value"] = peri_dt;
    if (peri_dt < (int)(ARRAY_SIZE(peripheral_dt_arr))) {
        jout("Device type:          %s\n", peripheral_dt_arr[peri_dt]);
        jglb["device_type"]["name"] = peripheral_dt_arr[peri_dt];
    }
    else
        jout("Device type:          <%d>\n", peri_dt);

    // See if transport protocol is known
    if (transport < 0)
        transport = scsiFetchTransportProtocol(device, modese_len);
    if ((transport >= 0) && (transport <= 0xf)) {
        jout("Transport protocol:   %s\n", transport_proto_arr[transport]);
        jglb["scsi_transport_protocol"]["name"] = transport_proto_arr[transport];
        jglb["scsi_transport_protocol"]["value"] = transport;
    }

    jout_startup_datetime("Local Time is:        ");

    // See if unit accepts SCSI commands from us
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
            if (is_tape)
                pout("NO tape present in drive\n");
            else
                pout("NO MEDIUM present in device\n");
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
        if (! is_tape) {
            int returnval = 0; // TODO: exit with FAILID if failuretest returns

            failuretest(MANDATORY_CMD, returnval|=FAILID);
        }
    }

    if (iec_err) {
        if (!is_tape) {
            print_on();
            jout("SMART support is:     Unavailable - device lacks SMART "
                 "capability.\n");
            jglb["smart_support"]["available"] = false;
            if (scsi_debugmode > 0)
                pout(" [%s]\n", scsiErrString(iec_err));
            print_off();
        }
        gIecMPage = 0;
        return 0;
    }

    if (!is_tape) {
        ok = scsi_IsExceptionControlEnabled(&iec);
        jout("SMART support is:     Available - device has SMART capability.\n"
             "SMART support is:     %s\n", ok ? "Enabled" : "Disabled");
        jglb["smart_support"]["available"] = true;
        jglb["smart_support"]["enabled"] = ok;
    }
    ok = scsi_IsWarningEnabled(&iec);
    jout("Temperature Warning:  %s\n",
         ok ? "Enabled" : "Disabled or Not Supported");
    jglb["temperature_warning"]["enabled"] = ok;
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
    uint8_t temp = 255;
    uint8_t trip = 255;

    if (scsiGetTemp(device, &temp, &trip))
        return;

    if (255 == temp)
        pout("Current Drive Temperature:     <not available>\n");
    else {
        jout("Current Drive Temperature:     %d C\n", temp);
        jglb["temperature"]["current"] = temp;
    }
    if (255 == trip)
        pout("Drive Trip Temperature:        <not available>\n");
    else {
        jout("Drive Trip Temperature:        %d C\n", trip);
        jglb["temperature"]["drive_trip"] = trip;
    }
    pout("\n");
}

static void
scsiPrintEnviroReporting(scsi_device * device)
{
    int len, num, err;
    int temp_num = 0;
    int humid_num = 0;
    unsigned char * ucp;
    const char * q;
    static const char * hname = "Environmental Reports";
    static const char * jname = "scsi_environmental_reports";
    static const char * rh_n = "relative humidity";
    static const char * temp_n = "temperature";
    static const char * sop_n = "since power on";
    static const char * unkn_n = "unknown";

   if ((err = scsiLogSense(device, TEMPERATURE_LPAGE, ENVIRO_REP_L_SPAGE,
                           gBuf, LOG_RESP_LEN, -1 /* single fetch */))) {
        print_on();
        pout("%s Failed [%s]\n", __func__, scsiErrString(err));
        print_off();
        return;
    }
    if (((gBuf[0] & 0x3f) != TEMPERATURE_LPAGE) ||
        (gBuf[1] != ENVIRO_REP_L_SPAGE)) {
        print_on();
        pout("%s %s Failed, page mismatch\n", hname, logSenStr);
        print_off();
        return;
    }
    if (! (gBuf[0] & 0x40)) {
        if (scsi_debugmode > 0) {
            print_on();
            pout("Another flaky device that doesn't set the SPF bit\n");
            print_off();
        }
    }
    len = sg_get_unaligned_be16(gBuf + 2);
    num = len - 4;
    ucp = &gBuf[0] + 4;

    while (num > 3) {
        int pc = sg_get_unaligned_be16(ucp + 0);
        int pl = ucp[3] + 4;
        char pc_s[32];

        if ((pc < 0x100) && (pl == 12)) {
            snprintf(pc_s, sizeof(pc_s), "temperature_%d", ++temp_num);
            /* temperature is two's complement 8 bit in centigrade */
            int temp = (int)(int8_t)ucp[5];

            jglb[jname][pc_s]["parameter_code"] = pc;
            q = "Current";
            if (ucp[5] == 0x80) {
                jout("%s %s = %s\n", q, temp_n, unkn_n);
                jglb[jname][pc_s][q] = unkn_n;
            } else {
                jout("%s %s = %d\n", q, temp_n, temp);
                jglb[jname][pc_s][q] = temp;
            }
            temp = (int)(int8_t)ucp[6];
            q = "Lifetime maximum";
            if (ucp[6] == 0x80) {
                jout("%s %s = %s\n", q, temp_n, unkn_n);
                jglb[jname][pc_s][q] = unkn_n;
            } else {
                jout("%s %s = %d\n", q, temp_n, temp);
                jglb[jname][pc_s][q] = temp;
            }
            temp = (int)(int8_t)ucp[7];
            q = "Lifetime minimum";
            if (ucp[7] == 0x80) {
                jout("%s %s = %s\n", q, temp_n, unkn_n);
                jglb[jname][pc_s][q] = unkn_n;
            } else {
                jout("%s %s = %d\n", q, temp_n, temp);
                jglb[jname][pc_s][q] = temp;
            }
            temp = (int)(int8_t)ucp[8];
            q = "Maximum since power on";
            if (ucp[8] == 0x80) {
                jout("Maximum %s %s = %s\n", temp_n, sop_n, unkn_n);
                jglb[jname][pc_s][q] = unkn_n;
            } else {
                jout("Maximum %s %s = %d\n", temp_n, sop_n, temp);
                jglb[jname][pc_s][q] = temp;
            }
            temp = (int)(int8_t)ucp[9];
            q = "Minimum since power on";
            if (ucp[9] == 0x80) {
                jout("Minimum %s %s = %s\n", temp_n, sop_n, unkn_n);
                jglb[jname][pc_s][q] = unkn_n;
            } else {
                jout("Minimum %s %s = %d\n", temp_n, sop_n, temp);
                jglb[jname][pc_s][q] = temp;
            }
            if ((ucp[4] & 0x3) == 1) {  /* OTV field set to 1 */
                temp = (int)(int8_t)ucp[10];
                q = "Maximum other";
                if (ucp[10] == 0x80) {
                    jout("%s %s = %s\n", q, temp_n, unkn_n);
                    jglb[jname][pc_s][q] = unkn_n;
                } else {
                    jout("%s %s = %d\n", q, temp_n, temp);
                    jglb[jname][pc_s][q] = temp;
                }
                temp = (int)(int8_t)ucp[11];
                q = "Minimum other";
                if (ucp[11] == 0x80) {
                    jout("%s %s = %s\n", q, temp_n, unkn_n);
                    jglb[jname][pc_s][q] = unkn_n;
                } else {
                    jout("%s %s = %d\n", q, temp_n, temp);
                    jglb[jname][pc_s][q] = temp;
                }
            }
        } else if ((pc < 0x200) && (pl == 12)) {
            snprintf(pc_s, sizeof(pc_s), "relative_humidity_%d", ++humid_num);
            jglb[jname][pc_s]["parameter_code"] = pc;
            jout("Relative humidity = %u\n", ucp[5]);
            jglb[jname][pc_s]["current"] = ucp[5];
            q = "Lifetime maximum";
            jout("%s %s = %d\n", q, rh_n, ucp[6]);
            jglb[jname][pc_s][q] = ucp[6];
            q = "Lifetime minimum";
            jout("%s %s = %d\n", q, rh_n, ucp[7]);
            jglb[jname][pc_s][q] = ucp[7];
            jout("Maximum %s %s = %d\n", rh_n, sop_n, ucp[8]);
            jglb[jname][pc_s]["maximum_since_power_on"] = ucp[8];
            jout("Minimum %s %s = %d\n", rh_n, sop_n, ucp[9]);
            jglb[jname][pc_s]["minimum_since_power_on"] = ucp[9];
            if ((ucp[4] & 0x3) == 1) {  /* ORHV field set to 1 */
                q = "Maximum other";
                jout("%s %s = %d\n", q, rh_n, ucp[10]);
                jglb[jname][pc_s][q] = ucp[10];
                q = "Minimum other";
                jout("%s %s = %d\n", q, rh_n, ucp[11]);
                jglb[jname][pc_s][q] = ucp[11];
            }
        } else {
            if (scsi_debugmode > 0) {
                print_on();
                if ((pc < 0x200) && (pl != 12))
                    pout("%s sub-lpage unexpected parameter length [%d], skip\n",
                         hname, pl);
                else
                    pout("%s sub-lpage has unexpected parameter [0x%x], skip\n",
                         hname, pc);
                print_off();
            }
            return;
        }
        num -= pl;
        ucp += pl;
    }
}

/* Main entry point used by smartctl command. Return 0 for success */
int
scsiPrintMain(scsi_device * device, const scsi_print_options & options)
{
    int checkedSupportedLogPages = 0;
    uint8_t peripheral_type = 0;
    int returnval = 0;
    int res, durationSec;
    struct scsi_sense_disect sense_info;
    bool is_disk;
    bool is_zbc;
    bool is_tape;
    bool any_output = options.drive_info;

// Enable -n option for SCSI Drives
    const char * powername = NULL;
    bool powerchg = false;

    if (options.powermode) {
        scsiRequestSense(device, &sense_info) ;
        if (sense_info.asc == 0x5E) {
            unsigned char powerlimit = 0xff;
            int     powermode = sense_info.ascq ;

            // 5Eh/00h  DZTPRO A  K    LOW POWER CONDITION ON
            // 5Eh/01h  DZTPRO A  K    IDLE CONDITION ACTIVATED BY TIMER
            // 5Eh/02h  DZTPRO A  K    STANDBY CONDITION ACTIVATED BY TIMER
            // 5Eh/03h  DZTPRO A  K    IDLE CONDITION ACTIVATED BY COMMAND
            // 5Eh/04h  DZTPRO A  K    STANDBY CONDITION ACTIVATED BY COMMAND
            // 5Eh/05h  DZTPRO A  K    IDLE_B CONDITION ACTIVATED BY TIMER
            // 5Eh/06h  DZTPRO A  K    IDLE_B CONDITION ACTIVATED BY COMMAND
            // 5Eh/07h  DZTPRO A  K    IDLE_C CONDITION ACTIVATED BY TIMER
            // 5Eh/08h  DZTPRO A  K    IDLE_C CONDITION ACTIVATED BY COMMAND
            // 5Eh/09h  DZTPRO A  K    STANDBY_Y CONDITION ACTIVATED BY TIMER
            // 5Eh/0Ah  DZTPRO A  K    STANDBY_Y CONDITION ACTIVATED BY COMMAND
            // 5Eh/41h           B     POWER STATE CHANGE TO ACTIVE
            // 5Eh/42h           B     POWER STATE CHANGE TO IDLE
            // 5Eh/43h           B     POWER STATE CHANGE TO STANDBY
            // 5Eh/45h           B     POWER STATE CHANGE TO SLEEP
            // 5Eh/47h           BK    POWER STATE CHANGE TO DEVICE CONTROL

            switch (powermode) {
            case -1:
               if (device->is_syscall_unsup()) {
                   pout("CHECK POWER MODE not implemented, ignoring -n option\n"); break;
                }
                powername = "SLEEP";   powerlimit = 2;
                break;

            case 0x00: // LOW POWER CONDITION ON
                powername = "LOW POWER"; powerlimit = 2; break;
            case 0x01: // IDLE CONDITION ACTIVATED BY TIMER
                powername = "IDLE BY TIMER"; powerlimit = 4; break;
            case 0x02: // STANDBY CONDITION ACTIVATED BY TIMER
                powername = "STANDBY BY TIMER";    powerlimit = 2; break;
            case 0x03: // IDLE CONDITION ACTIVATED BY COMMAND
                powername = "IDLE BY COMMAND";  powerlimit = 4; break;
            case 0x04: // STANDBY CONDITION ACTIVATED BY COMMAND
                powername = "STANDBY BY COMMAND";  powerlimit = 2; break;
            case 0x05: // IDLE_B CONDITION ACTIVATED BY TIMER
                powername = "IDLE BY TIMER";  powerlimit = 4; break;
            case 0x06: // IDLE_B CONDITION ACTIVATED BY COMMAND
                powername = "IDLE_ BY COMMAND";  powerlimit = 4; break;
            case 0x07: // IDLE_C CONDITION ACTIVATED BY TIMER
                powername = "IDLE_C BY TIMER";  powerlimit = 4; break;
            case 0x08: // IDLE_C CONDITION ACTIVATED BY COMMAND
                powername = "IDLE_C BY COMMAND";  powerlimit = 4; break;
            case 0x09: // STANDBY_Y CONDITION ACTIVATED BY TIMER
                powername = "STANDBY_Y BY TIMER";    powerlimit = 2; break;
            case 0x0A: // STANDBY_Y CONDITION ACTIVATED BY COMMAND
                powername = "STANDBY_Y BY COMMAND";  powerlimit = 2; break;

            default:
                pout("CHECK POWER MODE returned unknown value 0x%02x, "
                     "ignoring -n option\n", powermode);
                break;
            }
            if (powername) {
                if (options.powermode >= powerlimit) {
                    jinf("Device is in %s mode, exit(%d)\n", powername, options.powerexit);
                    return options.powerexit;
                }
                powerchg = (powermode != 0xff);
            }
        } else
            powername = "ACTIVE";
    }

    delete supported_vpd_pages_p;
    supported_vpd_pages_p = new supported_vpd_pages(device);

    res = scsiGetDriveInfo(device, &peripheral_type, is_zbc,
                           options.drive_info);
    if (res) {
        if (2 == res)
            return 0;
        else
            failuretest(MANDATORY_CMD, returnval |= FAILID);
        any_output = true;
    }
    is_disk = ((SCSI_PT_DIRECT_ACCESS == peripheral_type) ||
               (SCSI_PT_HOST_MANAGED == peripheral_type));
    is_tape = ((SCSI_PT_SEQUENTIAL_ACCESS == peripheral_type) ||
               (SCSI_PT_MEDIUM_CHANGER == peripheral_type));

    short int wce = -1, rcd = -1;
    // Print read look-ahead status for disks
    if (options.get_rcd || options.get_wce) {
        if (is_disk) {
            res = scsiGetSetCache(device, modese_len, &wce, &rcd);
            if (options.get_rcd)
                pout("Read Cache is:        %s\n",
                     res ? "Unavailable" : // error
                     rcd ? "Disabled" : "Enabled");
            if (options.get_wce)
                pout("Writeback Cache is:   %s\n",
                     res ? "Unavailable" : // error
                     !wce ? "Disabled" : "Enabled");
        }
        any_output = true;
    }

    if (options.drive_info) {
        if (powername)   // Print power condition if requested -n (nocheck)
            pout("Power mode %s       %s\n", (powerchg?"was:":"is: "), powername);
        pout("\n");
    }

    // START OF THE ENABLE/DISABLE SECTION OF THE CODE
    if (options.smart_disable           || options.smart_enable ||
        options.smart_auto_save_disable || options.smart_auto_save_enable)
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
        } else
            pout("Autosave enabled (GLTSD bit cleared).\n");
        any_output = true;
    }

    // Enable/Disable write cache
    if (options.set_wce && is_disk) {
        short int enable = wce = (options.set_wce > 0);

        rcd = -1;
        if (scsiGetSetCache(device, modese_len, &wce, &rcd)) {
            pout("Write cache %sable failed: %s\n", (enable ? "en" : "dis"),
                 device->get_errmsg());
            failuretest(OPTIONAL_CMD,returnval |= FAILSMART);
        } else
            pout("Write cache %sabled\n", (enable ? "en" : "dis"));
        any_output = true;
    }

    // Enable/Disable read cache
    if (options.set_rcd && is_disk) {
        short int enable =  (options.set_rcd > 0);

        rcd = !enable;
        wce = -1;
        if (scsiGetSetCache(device, modese_len, &wce, &rcd)) {
            pout("Read cache %sable failed: %s\n", (enable ? "en" : "dis"),
                device->get_errmsg());
            failuretest(OPTIONAL_CMD,returnval |= FAILSMART);
        } else
            pout("Read cache %sabled\n", (enable ? "en" : "dis"));
        any_output = true;
    }

    if (options.smart_auto_save_disable) {
        if (scsiSetControlGLTSD(device, 1, modese_len)) {
            pout("Disable autosave (set GLTSD bit) failed\n");
            failuretest(OPTIONAL_CMD,returnval |= FAILSMART);
        } else
            pout("Autosave disabled (GLTSD bit set).\n");
        any_output = true;
    }
    if (options.smart_disable           || options.smart_enable ||
        options.smart_auto_save_disable || options.smart_auto_save_enable)
        pout("\n"); // END OF THE ENABLE/DISABLE SECTION OF THE CODE

    // START OF READ-ONLY OPTIONS APART FROM -V and -i
    if (options.smart_check_status  || options.smart_ss_media_log ||
        options.smart_vendor_attrib || options.smart_error_log ||
        options.smart_selftest_log  || options.smart_background_log ||
        options.sasphy)
        pout("=== START OF READ SMART DATA SECTION ===\n");

    if (options.smart_check_status) {
        scsiGetSupportedLogPages(device);
        checkedSupportedLogPages = 1;
        if (is_tape) {
            if (gTapeAlertsLPage) {
                if (options.drive_info) {
                    jout("TapeAlert Supported\n");
                    jglb["tapealert"]["supported"] = true;
                }
                if (-1 == scsiGetTapeAlertsData(device, peripheral_type))
                    failuretest(OPTIONAL_CMD, returnval |= FAILSMART);
            } else {
                jout("TapeAlert Not Supported\n");
                jglb["tapealert"]["supported"] = false;
            }
        } else { /* disk, cd/dvd, enclosure, etc */
            if ((res = scsiGetSmartData(device,
                                        options.smart_vendor_attrib))) {
                if (-2 == res)
                    returnval |= FAILSTATUS;
                else
                    returnval |= FAILSMART;
            }
        }
        any_output = true;
    }

    if (is_disk && options.smart_ss_media_log) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(device);
        res = 0;
        if (gSSMediaLPage)
            res = scsiPrintSSMedia(device);
        if (0 != res)
            failuretest(OPTIONAL_CMD, returnval|=res);
        if (gFormatStatusLPage)
            res = scsiPrintFormatStatus(device);
        if (0 != res)
            failuretest(OPTIONAL_CMD, returnval|=res);
        any_output = true;
    }
    if (options.smart_vendor_attrib) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(device);
        if (gEnviroReportingLPage && options.smart_env_rep)
            scsiPrintEnviroReporting(device);
        else if (gTempLPage)
            scsiPrintTemp(device);
        // in the 'smartctl -A' case only want: "Accumulated power on time"
        if ((! options.smart_background_log) && is_disk) {
            res = 0;
            if (gBackgroundResultsLPage)
                res = scsiPrintBackgroundResults(device, true);
            (void)res; // not yet used below, suppress warning
        }
        if (gStartStopLPage)
            scsiGetStartStopData(device);
        if (is_disk) {
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
        if (gPendDefectsLPage)
            scsiPrintPendingDefectsLPage(device);
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
    if (options.smart_background_log && is_disk) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(device);
        res = 0;
        if (gBackgroundResultsLPage)
            res = scsiPrintBackgroundResults(device, false);
        else {
            pout("Device does not support Background scan results logging\n");
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        }
        if (0 != res)
            failuretest(OPTIONAL_CMD, returnval|=res);
        any_output = true;
    }
    if (options.smart_background_log && is_zbc) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(device);
        res = 0;
        if (gZBDeviceStatsLPage)
            res = scsiPrintZBDeviceStats(device);
        else {
            pout("Device does not support Zoned block device "
                 "characteristics logging\n");
            failuretest(OPTIONAL_CMD, returnval|=FAILSMART);
        }
        if (0 != res)
            failuretest(OPTIONAL_CMD, returnval|=res);
        any_output = true;

    }
    if (options.smart_background_log && is_tape) {
        if (! checkedSupportedLogPages)
            scsiGetSupportedLogPages(device);
        res = 0;
        if (gTapeDeviceStatsLPage)
            res = scsiPrintTapeDeviceStats(device);
        else {
            pout("Device does not support (tape) device characteristics "
                 "(SSC) logging\n");
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
                if (sense_info.progress != -1)
                    pout(" (%d%% remaining)",
                         100 - sense_info.progress * 100 / 65535);
                pout(",\nadd '-t force' option to override, or run "
                     "'smartctl -X' to abort test.\n");
                return -1;
            } else
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
            char comptime[DATEANDEPOCHLEN];
            dateandtimezoneepoch(comptime, t);
            pout("Estimated completion time: %s\n", comptime);
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

    if (options.set_standby == 1) {
        if (scsiSetPowerCondition(device, SCSI_POW_COND_ACTIVE)) {
            pout("SCSI SSU(ACTIVE) command failed: %s\n",
                 device->get_errmsg());
            returnval |= FAILSMART;
        } else
            pout("Device placed in ACTIVE mode\n");
    } else if (options.set_standby > 1) {
        pout("SCSI SSU(STANDBY) with timeout not supported yet\n");
        returnval |= FAILSMART;
    } else if (options.set_standby_now) {
        if (scsiSetPowerCondition(device, SCSI_POW_COND_STANDBY)) {
            pout("SCSI STANDBY command failed: %s\n", device->get_errmsg());
            returnval |= FAILSMART;
        } else
            pout("Device placed in STANDBY mode\n");
    }

    if (!any_output && powername) // Output power mode if -n (nocheck)
        pout("Device is in %s mode\n", powername);

    if (!any_output)
        pout("SCSI device successfully opened\n\nUse 'smartctl -a' (or '-x') "
             "to print SMART (and more) information\n\n");

    return returnval;
}
