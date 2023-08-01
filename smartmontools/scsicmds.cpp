/*
 * scsicmds.cpp
 *
 * Home page of code is: https://www.smartmontools.org
 *
 * Copyright (C) 2002-8 Bruce Allen
 * Copyright (C) 1999-2000 Michael Cornwell <cornwell@acm.org>
 * Copyright (C) 2003-2023 Douglas Gilbert <dgilbert@interlog.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 *
 * In the SCSI world "SMART" is a dead or withdrawn standard. In recent
 * SCSI standards (since SCSI-3) it goes under the awkward name of
 * "Informational Exceptions" ["IE" or "IEC" (with the "C" for "control")].
 * The relevant information is spread around several SCSI draft
 * standards available at http://www.t10.org . Reference is made in the
 * code to the following acronyms:
 *      - SAM [SCSI Architectural model, versions 2 or 3]
 *      - SPC [SCSI Primary commands, versions 2 or 3]
 *      - SBC [SCSI Block commands, versions 2]
 *
 * Some SCSI disk vendors have snippets of "SMART" information in their
 * product manuals.
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#include "config.h"

#include "scsicmds.h"
#include "dev_interface.h"
#include "utility.h"
#include "sg_unaligned.h"

const char *scsicmds_c_cvsid="$Id$"
  SCSICMDS_H_CVSID;

static const char * logSenStr = "Log Sense";

// Print SCSI debug messages?
unsigned char scsi_debugmode = 0;

supported_vpd_pages * supported_vpd_pages_p = nullptr;

#define RSOC_RESP_SZ 4096
#define RSOC_ALL_CMDS_CTDP_0 8
#define RSOC_ALL_CMDS_CTDP_1 20
#define RSOC_1_CMD_CTDP_0 36

// Check if LOG SENSE cdb supports changing the Subpage Code field
static scsi_cmd_support
chk_lsense_spc(scsi_device * device)
{
    int r_len = 0;
    int err;
    uint8_t rsoc_1cmd_rsp[RSOC_1_CMD_CTDP_0] = {};
    uint8_t * rp = rsoc_1cmd_rsp;

    err = scsiRSOCcmd(device, false /*rctd */ , 1 /* '1 cmd' format */,
                      LOG_SENSE, 0, rp, RSOC_1_CMD_CTDP_0, r_len);
    if (err) {
        if (scsi_debugmode)
            pout("%s Failed [%s]\n", __func__, scsiErrString(err));
        return SC_NO_SUPPORT;
    }
    if (r_len < 8) {
        if (scsi_debugmode)
            pout("%s response to short [%d]\n", __func__, r_len);
        return SC_NO_SUPPORT;
    }
    /* check the "subpage code" field in LOG SENSE cdb usage data */
    return rp[7] ? SC_SUPPORT : SC_NO_SUPPORT; /* 4 + ls_cdb_byte3 */
}

bool
scsi_device::query_cmd_support()
{
    bool res = true;
    int k, err, cd_len, bump;
    int r_len = 0;
    uint8_t * rp = (uint8_t *)calloc(sizeof(uint8_t), RSOC_RESP_SZ);
    const uint8_t * last_rp;
    uint8_t * cmdp;
    static const int max_bytes_of_cmds = RSOC_RESP_SZ - 4;

    if (nullptr == rp)
        return false;
    rsoc_queried = true;
    /* request 'all commands' format: 4 bytes header, 20 bytes per command */
    err = scsiRSOCcmd(this, false /* rctd */, 0 /* 'all' format */, 0, 0,
                      rp, RSOC_RESP_SZ, r_len);
    if (err) {
        rsoc_sup = SC_NO_SUPPORT;
        if (scsi_debugmode)
            pout("%s Failed [%s]\n", __func__, scsiErrString(err));
        res = false;
        goto fini;
    }
    if (r_len < 4) {
        pout("%s response too short\n", __func__);
        res = false;
        goto fini;
    }
    rsoc_sup = SC_SUPPORT;
    cd_len = sg_get_unaligned_be32(rp + 0);
    if (cd_len > max_bytes_of_cmds) {
        if (scsi_debugmode)
            pout("%s: truncate %d byte response to %d bytes\n", __func__,
                 cd_len, max_bytes_of_cmds);
        cd_len = max_bytes_of_cmds;
    }
    last_rp = rp + cd_len;
    logsense_sup = SC_NO_SUPPORT;
    logsense_spc_sup = SC_NO_SUPPORT;
    rdefect10_sup = SC_NO_SUPPORT;
    rdefect12_sup = SC_NO_SUPPORT;
    rcap16_sup = SC_NO_SUPPORT;

    for (k = 0, cmdp = rp + 4; cmdp < last_rp; ++k, cmdp += bump) {
        bool sa_valid = !! (0x1 & cmdp[5]);
        bool ctdp = !! (0x2 & cmdp[5]);
        uint8_t opcode = cmdp[0];
        uint16_t sa;

        bump = ctdp ? RSOC_ALL_CMDS_CTDP_1 : RSOC_ALL_CMDS_CTDP_0;
        sa = sa_valid ? sg_get_unaligned_be16(cmdp + 2) : 0;

        switch (opcode) {
        case LOG_SENSE:
            logsense_sup = SC_SUPPORT;
            logsense_spc_sup = chk_lsense_spc(this);
            break;
        case READ_DEFECT_10:
            rdefect10_sup = SC_SUPPORT;
            break;
        case READ_DEFECT_12:
            rdefect12_sup = SC_SUPPORT;
            break;
        case SERVICE_ACTION_IN_16:
            if (sa_valid && (SAI_READ_CAPACITY_16 == sa))
                rcap16_sup = SC_SUPPORT;
            break;
        default:
            break;
        }
    }
    if (scsi_debugmode > 3) {
        pout("%s: decoded %d supported commands\n", __func__, k);
        pout("  LOG SENSE %ssupported\n",
             (SC_SUPPORT == logsense_sup) ? "" : "not ");
        pout("  LOG SENSE subpage code %ssupported\n",
             (SC_SUPPORT == logsense_spc_sup) ? "" : "not ");
        pout("  READ DEFECT 10 %ssupported\n",
             (SC_SUPPORT == rdefect10_sup) ? "" : "not ");
        pout("  READ DEFECT 12 %ssupported\n",
             (SC_SUPPORT == rdefect12_sup) ? "" : "not ");
        pout("  READ CAPACITY 16 %ssupported\n",
             (SC_SUPPORT == rcap16_sup) ? "" : "not ");
    }

fini:
    free(rp);
    return res;
}

/* May track more in the future */
enum scsi_cmd_support
scsi_device::cmd_support_level(uint8_t opcode, bool sa_valid,
                               uint16_t sa, bool for_lsense_spc) const
{
    enum scsi_cmd_support scs = SC_SUPPORT_UNKNOWN;

    switch (opcode) {
    case LOG_SENSE:     /* checking if LOG SENSE _subpages_ supported */
        scs = for_lsense_spc ? logsense_spc_sup : logsense_sup;
        break;
    case READ_DEFECT_10:
        scs = rdefect10_sup;
        break;
    case READ_DEFECT_12:
        scs = rdefect12_sup;
        break;
    case SERVICE_ACTION_IN_16:
        if (sa_valid && (SAI_READ_CAPACITY_16 == sa))
            scs = rcap16_sup;
        break;
    case MAINTENANCE_IN_12:
        if (sa_valid && (MI_REP_SUP_OPCODES == sa))
            scs = rsoc_sup;
        break;
    default:
        break;
    }
    return scs;
}

supported_vpd_pages::supported_vpd_pages(scsi_device * device) : num_valid(0)
{
    unsigned char b[0xfc] = {};   /* pre SPC-3 INQUIRY max response size */

    if (device && (0 == scsiInquiryVpd(device, SCSI_VPD_SUPPORTED_VPD_PAGES,
                   b, sizeof(b)))) {
        num_valid = sg_get_unaligned_be16(b + 2);
        int n = sizeof(pages);
        if (num_valid > n)
            num_valid = n;
        memcpy(pages, b + 4, num_valid);
    }
}

bool
supported_vpd_pages::is_supported(int vpd_page_num) const
{
    /* Supported VPD pages numbers start at offset 4 and should be in
     * ascending order but don't assume that. */
    for (int k = 0; k < num_valid; ++k) {
        if (vpd_page_num == pages[k])
            return true;
    }
    return false;
}

/* Simple ASCII printable (does not use locale), includes space and excludes
 * DEL (0x7f). Note all UTF-8 encoding apart from <= 0x7f have top bit set. */
static inline int
my_isprint(uint8_t ch)
{
    return ((ch >= ' ') && (ch < 0x7f));
}

static int
trimTrailingSpaces(char * b)
{
    int n = strlen(b);

    while ((n > 0) && (' ' == b[n - 1]))
        b[--n] = '\0';
    return n;
}

/* Read binary starting at 'up' for 'len' bytes and output as ASCII
 * hexadecimal to the passed function 'out'. See dStrHex() below for more. */
static void
dStrHexHelper(const uint8_t * up, int len, int no_ascii,
              void (*out)(const char * s, void * ctx), void * ctx = nullptr)
{
    static const int line_len = 80;
    static const int cpstart = 60;      // offset of start of ASCII rendering
    uint8_t c;
    char buff[line_len + 2];    // room for trailing null
    int a = 0;
    int bpstart = 5;
    int cpos = cpstart;
    int bpos = bpstart;
    int i, k, blen;
    char e[line_len + 4];
    static const int elen = sizeof(e);

    if (len <= 0)
        return;
    blen = (int)sizeof(buff);
    memset(buff, ' ', line_len);
    buff[line_len] = '\0';
    if (no_ascii < 0) {
        bpstart = 0;
        bpos = bpstart;
        for (k = 0; k < len; k++) {
            c = *up++;
            if (bpos == (bpstart + (8 * 3)))
                bpos++;
            snprintf(&buff[bpos], blen - bpos, "%.2x", (int)(uint8_t)c);
            buff[bpos + 2] = ' ';
            if ((k > 0) && (0 == ((k + 1) % 16))) {
                trimTrailingSpaces(buff);
                if (no_ascii)
                    snprintf(e, elen, "%s\n", buff);
                else
                    snprintf(e, elen, "%.76s\n", buff);
                out(e, ctx);
                bpos = bpstart;
                memset(buff, ' ', line_len);
            } else
                bpos += 3;
        }
        if (bpos > bpstart) {
            buff[bpos + 2] = '\0';
            trimTrailingSpaces(buff);
            snprintf(e, elen, "%s\n", buff);
            out(e, ctx);
        }
        return;
    }
    /* no_ascii>=0, start each line with address (offset) */
    k = snprintf(buff + 1, blen - 1, "%.2x", a);
    buff[k + 1] = ' ';

    for (i = 0; i < len; i++) {
        c = *up++;
        bpos += 3;
        if (bpos == (bpstart + (9 * 3)))
            bpos++;
        snprintf(&buff[bpos], blen - bpos, "%.2x", (int)(uint8_t)c);
        buff[bpos + 2] = ' ';
        if (no_ascii)
            buff[cpos++] = ' ';
        else {
            if (! my_isprint(c))
                c = '.';
            buff[cpos++] = c;
        }
        if (cpos > (cpstart + 15)) {
            if (no_ascii)
                trimTrailingSpaces(buff);
            if (no_ascii)
                snprintf(e, elen, "%s\n", buff);
            else
                snprintf(e, elen, "%.76s\n", buff);
            out(e, ctx);
            bpos = bpstart;
            cpos = cpstart;
            a += 16;
            memset(buff, ' ', line_len);
            k = snprintf(buff + 1, blen - 1, "%.2x", a);
            buff[k + 1] = ' ';
        }
    }
    if (cpos > cpstart) {
        buff[cpos] = '\0';
        if (no_ascii)
            trimTrailingSpaces(buff);
        snprintf(e, elen, "%s\n", buff);
        out(e, ctx);
    }
}

/* Read binary starting at 'up' for 'len' bytes and output as ASCII
 * hexadecimal into file pointer (fp). If fp is nullptr, then send to
 * pout(). See dStrHex() below for more. */
void
dStrHexFp(const uint8_t * up, int len, int no_ascii, FILE * fp)
{
    /* N.B. Use of lamba requires C++11 or later. */
    if ((nullptr == up) || (len < 1))
        return;
    else if (fp)
        dStrHexHelper(up, len, no_ascii,
                      [](const char * s, void * ctx)
                            { fputs(s, reinterpret_cast<FILE *>(ctx)); },
                      fp);
    else
        dStrHexHelper(up, len, no_ascii,
                      [](const char * s, void *){ pout("%s", s); });
}

/* Read binary starting at 'up' for 'len' bytes and output as ASCII
 * hexadecimal into pout(). 16 bytes per line are output with an
 * additional space between 8th and 9th byte on each line (for readability).
 * 'no_ascii' selects one of 3 output format types:
 *     > 0     each line has address then up to 16 ASCII-hex bytes
 *     = 0     in addition, the bytes are rendered in ASCII to the right
 *             of each line, non-printable characters shown as '.'
 *     < 0     only the ASCII-hex bytes are listed (i.e. without address) */
void
dStrHex(const uint8_t * up, int len, int no_ascii)
{
    /* N.B. Use of lamba requires C++11 or later. */
    dStrHexHelper(up, len, no_ascii,
                  [](const char * s, void *){ pout("%s", s); });
}

/* This is a heuristic that takes into account the command bytes and length
 * to decide whether the presented unstructured sequence of bytes could be
 * a SCSI command. If so it returns true otherwise false. Vendor specific
 * SCSI commands (i.e. opcodes from 0xc0 to 0xff), if presented, are assumed
 * to follow SCSI conventions (i.e. length of 6, 10, 12 or 16 bytes). The
 * only SCSI commands considered above 16 bytes of length are the Variable
 * Length Commands (opcode 0x7f) and the XCDB wrapped commands (opcode 0x7e).
 * Both have an inbuilt length field which can be cross checked with clen.
 * No NVMe commands (64 bytes long plus some extra added by some OSes) have
 * opcodes 0x7e or 0x7f yet. ATA is register based but SATA has FIS
 * structures that are sent across the wire. The FIS register structure is
 * used to move a command from a SATA host to device, but the ATA 'command'
 * is not the first byte. So it is harder to say what will happen if a
 * FIS structure is presented as a SCSI command, hopefully there is a low
 * probability this function will yield true in that case. */
bool
is_scsi_cdb(const uint8_t * cdbp, int clen)
{
    if (clen < 6)
        return false;
    uint8_t opcode = cdbp[0];
    uint8_t top3bits = opcode >> 5;
    if (0x3 == top3bits) {      /* Opcodes 0x60 to 0x7f */
        int ilen, sa;
        if ((clen < 12) || (clen % 4))
            return false;       /* must be modulo 4 and 12 or more bytes */
        switch (opcode) {
        case 0x7e:      /* Extended cdb (XCDB) */
            ilen = 4 + sg_get_unaligned_be16(cdbp + 2);
            return (ilen == clen);
        case 0x7f:      /* Variable Length cdb */
            ilen = 8 + cdbp[7];
            sa = sg_get_unaligned_be16(cdbp + 8);
            /* service action (sa) 0x0 is reserved */
            return ((ilen == clen) && sa);
        default:
            return false;
        }
    } else if (clen <= 16) {
        switch (clen) {
        case 6:
            if (top3bits > 0x5)         /* vendor */
                return true;
            return (0x0 == top3bits);   /* 6 byte cdb */
        case 10:
            if (top3bits > 0x5)         /* vendor */
                return true;
            return ((0x1 == top3bits) || (0x2 == top3bits)); /* 10 byte cdb */
        case 16:
            if (top3bits > 0x5)         /* vendor */
                return true;
            return (0x4 == top3bits);   /* 16 byte cdb */
        case 12:
            if (top3bits > 0x5)         /* vendor */
                return true;
            return (0x5 == top3bits);   /* 12 byte cdb */
        default:
            return false;
        }
    }
    /* NVMe probably falls out here, clen > 16 and (opcode < 0x60 or
     * opcode > 0x7f). */
    return false;
}

enum scsi_sa_t {
    scsi_sa_none = 0,
    scsi_sa_b1b4n5,     /* for cdb byte 1, bit 4, number 5 bits */
    scsi_sa_b8b7n16,
};

struct scsi_sa_var_map {
    uint8_t cdb0;
    enum scsi_sa_t sa_var;
};

static struct scsi_sa_var_map sa_var_a[] = {
    {0x3b, scsi_sa_b1b4n5},     /* Write buffer modes_s */
    {0x3c, scsi_sa_b1b4n5},     /* Read buffer(10) modes_s */
    {0x48, scsi_sa_b1b4n5},     /* Sanitize sa_s */
    {0x5e, scsi_sa_b1b4n5},     /* Persistent reserve in sa_s */
    {0x5f, scsi_sa_b1b4n5},     /* Persistent reserve out sa_s */
    {0x7f, scsi_sa_b8b7n16},    /* Variable length commands */
    {0x83, scsi_sa_b1b4n5},     /* Extended copy out/cmd sa_s */
    {0x84, scsi_sa_b1b4n5},     /* Extended copy in sa_s */
    {0x8c, scsi_sa_b1b4n5},     /* Read attribute sa_s */
    {0x9b, scsi_sa_b1b4n5},     /* Read buffer(16) modes_s */
    {0x9e, scsi_sa_b1b4n5},     /* Service action in (16) */
    {0x9f, scsi_sa_b1b4n5},     /* Service action out (16) */
    {0xa3, scsi_sa_b1b4n5},     /* Maintenance in */
    {0xa4, scsi_sa_b1b4n5},     /* Maintenance out */
    {0xa9, scsi_sa_b1b4n5},     /* Service action out (12) */
    {0xab, scsi_sa_b1b4n5},     /* Service action in (12) */
};

struct scsi_opcode_name {
    uint8_t opcode;
    bool sa_valid;      /* Service action (next field) valid */
    uint16_t sa;
    const char * name;
};

/* Array assumed to be sorted by opcode then service action (sa) */
static struct scsi_opcode_name opcode_name_arr[] = {
    /* in ascending opcode order */
    {TEST_UNIT_READY, false, 0, "test unit ready"},       /* 0x00 */
    {REQUEST_SENSE, false, 0, "request sense"},           /* 0x03 */
    {INQUIRY, false, 0, "inquiry"},                       /* 0x12 */
    {MODE_SELECT_6, false, 0, "mode select(6)"},          /* 0x15 */
    {MODE_SENSE_6, false, 0, "mode sense(6)"},            /* 0x1a */
    {START_STOP_UNIT, false, 0, "start stop unit"},       /* 0x1b */
    {RECEIVE_DIAGNOSTIC, false, 0, "receive diagnostic"}, /* 0x1c */
    {SEND_DIAGNOSTIC, false, 0, "send diagnostic"},       /* 0x1d */
    {READ_CAPACITY_10, false, 0, "read capacity(10)"},    /* 0x25 */
    {READ_DEFECT_10, false, 0, "read defect list(10)"},   /* 0x37 */
    {LOG_SELECT, false, 0, "log select"},                 /* 0x4c */
    {LOG_SENSE, false, 0, "log sense"},                   /* 0x4d */
    {MODE_SELECT_10, false, 0, "mode select(10)"},        /* 0x55 */
    {MODE_SENSE_10, false, 0, "mode sense(10)"},          /* 0x5a */
    {SAT_ATA_PASSTHROUGH_16, false, 0, "ata pass-through(16)"}, /* 0x85 */
    {SERVICE_ACTION_IN_16, true, SAI_READ_CAPACITY_16, "read capacity(16)"},
                                                          /* 0x9e,0x10 */
    {SERVICE_ACTION_IN_16, true, SAI_GET_PHY_ELEM_STATUS,
        "get physical element status"},                   /* 0x9e,0x17 */
    {REPORT_LUNS, false, 0, "report luns"},               /* 0xa0 */
    {SAT_ATA_PASSTHROUGH_12, false, 0, "ata pass-through(12)"}, /* 0xa1 */
    {MAINTENANCE_IN_12, true, MI_REP_SUP_OPCODES,
        "report supported operation codes"},              /* 0xa3,0xc */
    {READ_DEFECT_12, false, 0, "read defect list(12)"},   /* 0xb7 */
};

static const char * vendor_specific = "<vendor specific>";

/* Need to expand to take service action into account. For commands
 * of interest the service action is in the 2nd command byte */
const char *
scsi_get_opcode_name(const uint8_t * cdbp)
{
    uint8_t opcode = cdbp[0];
    uint8_t cdb0;
    enum scsi_sa_t sa_var = scsi_sa_none;
    bool sa_valid = false;
    uint16_t sa = 0;
    int k;
    static const int sa_var_len = sizeof(sa_var_a) /
                                  sizeof(sa_var_a[0]);
    static const int len = sizeof(opcode_name_arr) /
                           sizeof(opcode_name_arr[0]);

    if (opcode >= 0xc0)
        return vendor_specific;
    for (k = 0; k < sa_var_len; ++k) {
        cdb0 = sa_var_a[k].cdb0;
        if (opcode == cdb0) {
            sa_var = sa_var_a[k].sa_var;
            break;
        }
        if (opcode < cdb0)
            break;
    }
    switch (sa_var) {
    case scsi_sa_none:
        break;
    case scsi_sa_b1b4n5:
        sa_valid = true;
        sa = cdbp[1] & 0x1f;
        break;
    case scsi_sa_b8b7n16:
        sa_valid = true;
        sa = sg_get_unaligned_be16(cdbp + 8);
        break;
    }
    for (k = 0; k < len; ++k) {
        struct scsi_opcode_name * onp = &opcode_name_arr[k];

        if (opcode == onp->opcode) {
            if ((! sa_valid) && (! onp->sa_valid))
                return onp->name;
            if (sa_valid && onp->sa_valid) {
                if (sa == onp->sa)
                    return onp->name;
            }
            /* should not see sa_valid and ! onp->sa_valid (or vice versa) */
        } else if (opcode < onp->opcode)
            return nullptr;
    }
    return nullptr;
}

void
scsi_do_sense_disect(const struct scsi_cmnd_io * io_buf,
                     struct scsi_sense_disect * out)
{
    memset(out, 0, sizeof(struct scsi_sense_disect));
    if (SCSI_STATUS_CHECK_CONDITION == io_buf->scsi_status) {
        int resp_code = (io_buf->sensep[0] & 0x7f);
        out->resp_code = resp_code;
        if (resp_code >= 0x72) {
            out->sense_key = (io_buf->sensep[1] & 0xf);
            out->asc = io_buf->sensep[2];
            out->ascq = io_buf->sensep[3];
        } else if (resp_code >= 0x70) {
            out->sense_key = (io_buf->sensep[2] & 0xf);
            if (io_buf->resp_sense_len > 13) {
                out->asc = io_buf->sensep[12];
                out->ascq = io_buf->sensep[13];
            }
        }
    }
}

int
scsiSimpleSenseFilter(const struct scsi_sense_disect * sinfo)
{
    switch (sinfo->sense_key) {
    case SCSI_SK_NO_SENSE:
    case SCSI_SK_RECOVERED_ERR:
    case SCSI_SK_COMPLETED:
        return SIMPLE_NO_ERROR;
    case SCSI_SK_NOT_READY:
        if (SCSI_ASC_NO_MEDIUM == sinfo->asc)
            return SIMPLE_ERR_NO_MEDIUM;
        else if (SCSI_ASC_NOT_READY == sinfo->asc) {
            if (0x1 == sinfo->ascq)
                return SIMPLE_ERR_BECOMING_READY;
            else
                return SIMPLE_ERR_NOT_READY;
        } else
            return SIMPLE_ERR_NOT_READY;
    case SCSI_SK_MEDIUM_ERROR:
    case SCSI_SK_HARDWARE_ERROR:
        return SIMPLE_ERR_MEDIUM_HARDWARE;
    case SCSI_SK_ILLEGAL_REQUEST:
        if (SCSI_ASC_UNKNOWN_OPCODE == sinfo->asc)
            return SIMPLE_ERR_BAD_OPCODE;
        else if (SCSI_ASC_INVALID_FIELD == sinfo->asc)
            return SIMPLE_ERR_BAD_FIELD;
        else if (SCSI_ASC_UNKNOWN_PARAM == sinfo->asc)
            return SIMPLE_ERR_BAD_PARAM;
        else
            return SIMPLE_ERR_BAD_PARAM;    /* all other illegal request */
    case SCSI_SK_UNIT_ATTENTION:
        return SIMPLE_ERR_TRY_AGAIN;
    case SCSI_SK_ABORTED_COMMAND:
        return SIMPLE_ERR_ABORTED_COMMAND;
    case SCSI_SK_DATA_PROTECT:
        return SIMPLE_ERR_PROTECTION;
    case SCSI_SK_MISCOMPARE:
        return SIMPLE_ERR_MISCOMPARE;
    default:
        return SIMPLE_ERR_UNKNOWN;
    }
}

const char *
scsiErrString(int scsiErr)
{
    if (scsiErr < 0)
        return strerror(-scsiErr);
    switch (scsiErr) {
        case SIMPLE_NO_ERROR:
            return "no error";
        case SIMPLE_ERR_NOT_READY:
            return "device not ready";
        case SIMPLE_ERR_BAD_OPCODE:
            return "unsupported scsi opcode";
        case SIMPLE_ERR_BAD_FIELD:
            return "unsupported field in scsi command";
        case SIMPLE_ERR_BAD_PARAM:
            return "badly formed scsi parameters";
        case SIMPLE_ERR_BAD_RESP:
            return "scsi response fails sanity test";
        case SIMPLE_ERR_NO_MEDIUM:
            return "no medium present";
        case SIMPLE_ERR_BECOMING_READY:
            return "device will be ready soon";
        case SIMPLE_ERR_TRY_AGAIN:
            return "unit attention reported, try again";
        case SIMPLE_ERR_MEDIUM_HARDWARE:
            return "medium or hardware error (serious)";
        case SIMPLE_ERR_UNKNOWN:
            return "unknown error (unexpected sense key)";
        case SIMPLE_ERR_ABORTED_COMMAND:
            return "aborted command";
        case SIMPLE_ERR_PROTECTION:
            return "data protection error";
        case SIMPLE_ERR_MISCOMPARE:
            return "miscompare";
        default:
            return "unknown error";
    }
}

static const char * sense_key_desc[] = {
    "No Sense",                 /* Filemark, ILI and/or EOM; progress
                                   indication (during FORMAT); power
                                   condition sensing (REQUEST SENSE) */
    "Recovered Error",          /* The last command completed successfully
                                   but used error correction */
    "Not Ready",                /* The addressed target is not ready */
    "Medium Error",             /* Data error detected on the medium */
    "Hardware Error",           /* Controller or device failure */
    "Illegal Request",
    "Unit Attention",           /* Removable medium was changed, or
                                   the target has been reset */
    "Data Protect",             /* Access to the data is blocked */
    "Blank Check",              /* Reached unexpected written or unwritten
                                   region of the medium */
    "Vendor specific(9)",       /* Vendor specific */
    "Copy Aborted",             /* COPY or COMPARE was aborted */
    "Aborted Command",          /* The target aborted the command */
    "Equal",                    /* SEARCH DATA found data equal (obsolete) */
    "Volume Overflow",          /* Medium full with data to be written */
    "Miscompare",               /* Source data and data on the medium
                                   do not agree */
    "Completed"                 /* may occur for successful cmd (spc4r23) */
};

/* Yield string associated with sense_key value. Returns 'buff'. */
char *
scsi_get_sense_key_str(int sense_key, int buff_len, char * buff)
{
    if (1 == buff_len) {
        buff[0] = '\0';
        return buff;
    }
    if ((sense_key >= 0) && (sense_key < 16))
        snprintf(buff, buff_len, "%s", sense_key_desc[sense_key]);
    else
        snprintf(buff, buff_len, "invalid value: 0x%x", sense_key);
    return buff;
}

/* Iterates to next designation descriptor in the device identification
 * VPD page. The 'initial_desig_desc' should point to start of first
 * descriptor with 'page_len' being the number of valid bytes in that
 * and following descriptors. To start, 'off' should point to a negative
 * value, thereafter it should point to the value yielded by the previous
 * call. If 0 returned then 'initial_desig_desc + *off' should be a valid
 * descriptor; returns -1 if normal end condition and -2 for an abnormal
 * termination. Matches association, designator_type and/or code_set when
 * any of those values are greater than or equal to zero. */
int
scsi_vpd_dev_id_iter(const unsigned char * initial_desig_desc, int page_len,
                     int * off, int m_assoc, int m_desig_type, int m_code_set)
{
    const unsigned char * ucp;
    int k;

    for (k = *off, ucp = initial_desig_desc ; (k + 3) < page_len; ) {
        k = (k < 0) ? 0 : (k + ucp[k + 3] + 4);
        if ((k + 4) > page_len)
            break;
        int c_set = (ucp[k] & 0xf);
        if ((m_code_set >= 0) && (m_code_set != c_set))
            continue;
        int assoc = ((ucp[k + 1] >> 4) & 0x3);
        if ((m_assoc >= 0) && (m_assoc != assoc))
            continue;
        int desig_type = (ucp[k + 1] & 0xf);
        if ((m_desig_type >= 0) && (m_desig_type != desig_type))
            continue;
        *off = k;
        return 0;
    }
    return (k == page_len) ? -1 : -2;
}

/* Decode VPD page 0x83 logical unit designator into a string. If both
 * numeric address and SCSI name string present, prefer the former.
 * Returns 0 on success, -1 on error with error string in s. */
int
scsi_decode_lu_dev_id(const unsigned char * b, int blen, char * s, int slen,
                      int * transport)
{
    if (transport)
        *transport = -1;
    if (slen < 32) {
        if (slen > 0)
            s[0] = '\0';
        return -1;
    }

    s[0] = '\0';
    int si = 0;
    int have_scsi_ns = 0;
    int off = -1;
    int u;
    while ((u = scsi_vpd_dev_id_iter(b, blen, &off, -1, -1, -1)) == 0) {
        const unsigned char * ucp = b + off;
        int i_len = ucp[3];
        if ((off + i_len + 4) > blen) {
            snprintf(s+si, slen-si, "error: designator length");
            return -1;
        }
        int assoc = ((ucp[1] >> 4) & 0x3);
        if (transport && assoc && (ucp[1] & 0x80) && (*transport < 0))
            *transport = (ucp[0] >> 4) & 0xf;
        if (0 != assoc)
            continue;
        const unsigned char * ip = ucp + 4;
        int c_set = (ucp[0] & 0xf);
        int desig_type = (ucp[1] & 0xf);

        int naa;
        switch (desig_type) {
        case 0: /* vendor specific */
        case 1: /* T10 vendor identification */
            break;
        case 2: /* EUI-64 based */
            if ((8 != i_len) && (12 != i_len) && (16 != i_len)) {
                snprintf(s+si, slen-si, "error: EUI-64 length");
                return -1;
            }
            if (have_scsi_ns)
                si = 0;
            si += snprintf(s+si, slen-si, "0x");
            for (int m = 0; m < i_len; ++m)
                si += snprintf(s+si, slen-si, "%02x", (unsigned int)ip[m]);
            break;
        case 3: /* NAA */
            if (1 != c_set) {
                snprintf(s+si, slen-si, "error: NAA bad code_set");
                return -1;
            }
            naa = (ip[0] >> 4) & 0xff;
            if ((naa < 2) || (naa > 6) || (4 == naa)) {
                snprintf(s+si, slen-si, "error: unexpected NAA");
                return -1;
            }
            if (have_scsi_ns)
                si = 0;
            if (2 == naa) {             /* NAA IEEE Extended */
                if (8 != i_len) {
                    snprintf(s+si, slen-si, "error: NAA 2 length");
                    return -1;
                }
                si += snprintf(s+si, slen-si, "0x");
                for (int m = 0; m < 8; ++m)
                    si += snprintf(s+si, slen-si, "%02x", (unsigned int)ip[m]);
            } else if ((3 == naa ) || (5 == naa)) {
                /* NAA=3 Locally assigned; NAA=5 IEEE Registered */
                if (8 != i_len) {
                    snprintf(s+si, slen-si, "error: NAA 3 or 5 length");
                    return -1;
                }
                si += snprintf(s+si, slen-si, "0x");
                for (int m = 0; m < 8; ++m)
                    si += snprintf(s+si, slen-si, "%02x", (unsigned int)ip[m]);
            } else if (6 == naa) {      /* NAA IEEE Registered extended */
                if (16 != i_len) {
                    snprintf(s+si, slen-si, "error: NAA 6 length");
                    return -1;
                }
                si += snprintf(s+si, slen-si, "0x");
                for (int m = 0; m < 16; ++m)
                    si += snprintf(s+si, slen-si, "%02x", (unsigned int)ip[m]);
            }
            break;
        case 4: /* Relative target port */
        case 5: /* (primary) Target port group */
        case 6: /* Logical unit group */
        case 7: /* MD5 logical unit identifier */
            break;
        case 8: /* SCSI name string */
            if (3 != c_set) {
                snprintf(s+si, slen-si, "error: SCSI name string");
                return -1;
            }
            /* does %s print out UTF-8 ok?? */
            if (si == 0) {
                si += snprintf(s+si, slen-si, "%s", (const char *)ip);
                ++have_scsi_ns;
            }
            break;
        default: /* reserved */
            break;
        }
    }
    if (-2 == u) {
        snprintf(s+si, slen-si, "error: bad structure");
        return -1;
    }
    return 0;
}

/* Sends LOG SENSE command. Returns 0 if ok, 1 if device NOT READY, 2 if
 * command not supported, 3 if field (within command) not supported or
 * returns negated errno.  SPC-3 sections 6.6 and 7.2 (rec 22a).
 * N.B. Sets PC==1 to fetch "current cumulative" log pages.
 * If known_resp_len > 0 then a single fetch is done for this response
 * length. If known_resp_len == 0 then twin fetches are performed, the
 * first to deduce the response length, then send the same command again
 * requesting the deduced response length. This protects certain fragile
 * HBAs. The twin fetch technique should not be used with the TapeAlert
 * log page since it clears its state flags after each fetch. If
 * known_resp_len < 0 then does single fetch for BufLen bytes. */
int
scsiLogSense(scsi_device * device, int pagenum, int subpagenum, uint8_t *pBuf,
             int bufLen, int known_resp_len)
{
    int pageLen;
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[10] = {};
    uint8_t sense[32];

    if (known_resp_len > bufLen)
        return -EIO;
    if (known_resp_len > 0)
        pageLen = known_resp_len;
    else if (known_resp_len < 0)
        pageLen = bufLen;
    else {      /* 0 == known_resp_len */
        /* Twin fetch strategy: first fetch to find response length */
        pageLen = 4;
        if (pageLen > bufLen)
            return -EIO;
        else
            memset(pBuf, 0, pageLen);

        io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
        io_hdr.dxfer_len = pageLen;
        io_hdr.dxferp = pBuf;
        cdb[0] = LOG_SENSE;
        cdb[2] = 0x40 | (pagenum & 0x3f);       /* Page control (PC)==1 */
        cdb[3] = subpagenum;                    /* 0 for no sub-page */
        sg_put_unaligned_be16(pageLen, cdb + 7);
        io_hdr.cmnd = cdb;
        io_hdr.cmnd_len = sizeof(cdb);
        io_hdr.sensep = sense;
        io_hdr.max_sense_len = sizeof(sense);
        io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

        if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
          return -device->get_errno();
        int res;
        if ((res = scsiSimpleSenseFilter(&sinfo)))
            return res;
        /* sanity check on response */
        if ((SUPPORTED_LPAGES != pagenum) && ((pBuf[0] & 0x3f) != pagenum))
            return SIMPLE_ERR_BAD_RESP;
        uint16_t u = sg_get_unaligned_be16(pBuf + 2);
        if (0 == u)
            return SIMPLE_ERR_BAD_RESP;
        pageLen = u + 4;
        /* some SCSI HBA don't like "odd" length transfers */
        if (pageLen % 2)
            pageLen += 1;
        if (pageLen > bufLen)
            pageLen = bufLen;
    }
    memset(pBuf, 0, 4);
    memset(&io_hdr, 0, sizeof(io_hdr));
    memset(cdb, 0, sizeof(cdb));
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = pageLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = LOG_SENSE;
    cdb[2] = 0x40 | (pagenum & 0x3f);  /* Page control (PC)==1 */
    cdb[3] = subpagenum;
    sg_put_unaligned_be16(pageLen, cdb + 7);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    int status = scsiSimpleSenseFilter(&sinfo);
    if (0 != status)
        return status;
    /* sanity check on response */
    if ((SUPPORTED_LPAGES != pagenum) && ((pBuf[0] & 0x3f) != pagenum))
        return SIMPLE_ERR_BAD_RESP;
    if (0 == sg_get_unaligned_be16(pBuf + 2))
        return SIMPLE_ERR_BAD_RESP;
    return 0;
}

/* Sends a LOG SELECT command. Can be used to set log page values
 * or reset one log page (or all of them) to its defaults (typically zero).
 * Returns 0 if ok, 1 if NOT READY, 2 if command not supported, * 3 if
 * field in command not supported, * 4 if bad parameter to command or
 * returns negated errno. SPC-4 sections 6.5 and 7.2 (rev 20) */
int
scsiLogSelect(scsi_device * device, int pcr, int sp, int pc, int pagenum,
              int subpagenum, uint8_t *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[10] = {};
    uint8_t sense[32];

    io_hdr.dxfer_dir = DXFER_TO_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = LOG_SELECT;
    cdb[1] = (pcr ? 2 : 0) | (sp ? 1 : 0);
    cdb[2] = ((pc << 6) & 0xc0) | (pagenum & 0x3f);
    cdb[3] = (subpagenum & 0xff);
    sg_put_unaligned_be16(bufLen, cdb + 7);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    return scsiSimpleSenseFilter(&sinfo);
}

/* Send MODE SENSE (6 byte) command. Returns 0 if ok, 1 if NOT READY,
 * 2 if command not supported (then MODE SENSE(10) should be supported),
 * 3 if field in command not supported or returns negated errno.
 * SPC-3 sections 6.9 and 7.4 (rev 22a) [mode subpage==0] */
int
scsiModeSense(scsi_device * device, int pagenum, int subpagenum, int pc,
              uint8_t *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[6] = {};
    uint8_t sense[32];

    if ((bufLen < 0) || (bufLen > 255))
        return -EINVAL;
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = MODE_SENSE_6;
    cdb[2] = (pc << 6) | (pagenum & 0x3f);
    cdb[3] = subpagenum;
    cdb[4] = bufLen;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    int status = scsiSimpleSenseFilter(&sinfo);
    if ((0 == status) && (ALL_MODE_PAGES != pagenum)) {
        int offset;

        offset = scsiModePageOffset(pBuf, bufLen, 0);
        if (offset < 0)
            return SIMPLE_ERR_BAD_RESP;
        else if (pagenum != (pBuf[offset] & 0x3f))
            return SIMPLE_ERR_BAD_RESP;
    }
    return status;
}

/* Sends a 6 byte MODE SELECT command. Assumes given pBuf is the response
 * from a corresponding 6 byte MODE SENSE command. Such a response should
 * have a 4 byte header followed by 0 or more 8 byte block descriptors
 * (normally 1) and then 1 mode page. Returns 0 if ok, 1 if NOT READY,
 * 2 if command not supported (then MODE SELECT(10) may be supported),
 * 3 if field in command not supported, 4 if bad parameter to command
 * or returns negated errno. SPC-3 sections 6.7 and 7.4 (rev 22a) */
int
scsiModeSelect(scsi_device * device, int sp, uint8_t *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[6] = {};
    uint8_t sense[32];
    int pg_offset, pg_len, hdr_plus_1_pg;

    pg_offset = 4 + pBuf[3];
    if (pg_offset + 2 >= bufLen)
        return -EINVAL;
    pg_len = pBuf[pg_offset + 1] + 2;
    hdr_plus_1_pg = pg_offset + pg_len;
    if (hdr_plus_1_pg > bufLen)
        return -EINVAL;
    pBuf[0] = 0;  /* Length of returned mode sense data reserved for SELECT */
    pBuf[pg_offset] &= 0x7f;    /* Mask out PS bit from byte 0 of page data */
    io_hdr.dxfer_dir = DXFER_TO_DEVICE;
    io_hdr.dxfer_len = hdr_plus_1_pg;
    io_hdr.dxferp = pBuf;
    cdb[0] = MODE_SELECT_6;
    cdb[1] = 0x10 | (sp & 1);      /* set PF (page format) bit always */
    cdb[4] = hdr_plus_1_pg; /* make sure only one page sent */
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    return scsiSimpleSenseFilter(&sinfo);
}

/* MODE SENSE (10 byte). Returns 0 if ok, 1 if NOT READY, 2 if command
 * not supported (then MODE SENSE(6) might be supported), 3 if field in
 * command not supported or returns negated errno.
 * SPC-3 sections 6.10 and 7.4 (rev 22a) [mode subpage==0] */
int
scsiModeSense10(scsi_device * device, int pagenum, int subpagenum, int pc,
                uint8_t *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[10] = {};
    uint8_t sense[32];

    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = MODE_SENSE_10;
    cdb[2] = (pc << 6) | (pagenum & 0x3f);
    cdb[3] = subpagenum;
    sg_put_unaligned_be16(bufLen, cdb + 7);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    int status = scsiSimpleSenseFilter(&sinfo);
    if ((0 == status) && (ALL_MODE_PAGES != pagenum)) {
        int offset;

        offset = scsiModePageOffset(pBuf, bufLen, 1);
        if (offset < 0)
            return SIMPLE_ERR_BAD_RESP;
        else if (pagenum != (pBuf[offset] & 0x3f))
            return SIMPLE_ERR_BAD_RESP;
    }
    return status;
}

/* Sends a 10 byte MODE SELECT command. Assumes given pBuf is the response
 * from a corresponding 10 byte MODE SENSE command. Such a response should
 * have a 8 byte header followed by 0 or more 8 byte block descriptors
 * (normally 1) and then 1 mode page. Returns 0 if ok, 1 NOT REAFY, 2 if
 * command not supported (then MODE SELECT(6) may be supported), 3 if field
 * in command not supported, 4 if bad parameter to command or returns
 * negated errno. SPC-3 sections 6.8 and 7.4 (rev 22a) */
int
scsiModeSelect10(scsi_device * device, int sp, uint8_t *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[10] = {};
    uint8_t sense[32];
    int pg_offset, pg_len, hdr_plus_1_pg;

    pg_offset = 8 + sg_get_unaligned_be16(pBuf + 6);
    if (pg_offset + 2 >= bufLen)
        return -EINVAL;
    pg_len = pBuf[pg_offset + 1] + 2;
    hdr_plus_1_pg = pg_offset + pg_len;
    if (hdr_plus_1_pg > bufLen)
        return -EINVAL;
    pBuf[0] = 0;
    pBuf[1] = 0; /* Length of returned mode sense data reserved for SELECT */
    pBuf[pg_offset] &= 0x7f;    /* Mask out PS bit from byte 0 of page data */
    io_hdr.dxfer_dir = DXFER_TO_DEVICE;
    io_hdr.dxfer_len = hdr_plus_1_pg;
    io_hdr.dxferp = pBuf;
    cdb[0] = MODE_SELECT_10;
    cdb[1] = 0x10 | (sp & 1);      /* set PF (page format) bit always */
    /* make sure only one page sent */
    sg_put_unaligned_be16(hdr_plus_1_pg, cdb + 7);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    return scsiSimpleSenseFilter(&sinfo);
}

/* Standard INQUIRY returns 0 for ok, anything else is a major problem.
 * bufLen should be 36 for unsafe devices (like USB mass storage stuff)
 * otherwise they can lock up! SPC-3 sections 6.4 and 7.6 (rev 22a) */
int
scsiStdInquiry(scsi_device * device, uint8_t *pBuf, int bufLen)
{
    struct scsi_sense_disect sinfo;
    struct scsi_cmnd_io io_hdr = {};
    int res;
    uint8_t cdb[6] = {};
    uint8_t sense[32];

    if ((bufLen < 0) || (bufLen > 1023))
        return -EINVAL;
    if (bufLen >= 36)   /* normal case */
        memset(pBuf, 0, 36);
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = INQUIRY;
    sg_put_unaligned_be16(bufLen, cdb + 3);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
        return -device->get_errno();
    res = scsiSimpleSenseFilter(&sinfo);
    if ((SIMPLE_NO_ERROR == res) && (! device->is_spc4_or_higher())) {
        if (((bufLen -  io_hdr.resid) >= 36) &&
            (pBuf[2] >= 6) &&           /* VERSION field >= SPC-4 */
            ((pBuf[3] & 0xf) == 2)) {   /* RESPONSE DATA field == 2 */
            uint8_t pdt = pBuf[0] & 0x1f;

            if ((SCSI_PT_DIRECT_ACCESS == pdt) ||
               (SCSI_PT_HOST_MANAGED == pdt) ||
               (SCSI_PT_SEQUENTIAL_ACCESS == pdt) ||
               (SCSI_PT_MEDIUM_CHANGER == pdt))
                device->set_spc4_or_higher();
        }
    }
    return res;
}

/* INQUIRY to fetch Vital Page Data.  Returns 0 if ok, 1 if NOT READY
 * (unlikely), 2 if command not supported, 3 if field in command not
 * supported, 5 if response indicates that EVPD bit ignored or returns
 * negated errno. SPC-3 section 6.4 and 7.6 (rev 22a) */
int
scsiInquiryVpd(scsi_device * device, int vpd_page, uint8_t *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[6] = {};
    uint8_t sense[32];
    int res;

    /* Assume SCSI_VPD_SUPPORTED_VPD_PAGES is first VPD page fetched */
    if ((SCSI_VPD_SUPPORTED_VPD_PAGES != vpd_page) &&
        supported_vpd_pages_p &&
        (! supported_vpd_pages_p->is_supported(vpd_page)))
        return 3;

    if ((bufLen < 0) || (bufLen > 1023))
        return -EINVAL;
try_again:
    if (bufLen > 1)
        pBuf[1] = 0x0;
    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = INQUIRY;
    cdb[1] = 0x1;       /* set EVPD bit (enable Vital Product Data) */
    cdb[2] = vpd_page;
    sg_put_unaligned_be16(bufLen, cdb + 3);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    if ((SCSI_STATUS_CHECK_CONDITION == io_hdr.scsi_status) &&
        (SCSI_SK_ILLEGAL_REQUEST == sinfo.sense_key) &&
        (SCSI_ASC_INVALID_FIELD == sinfo.asc) &&
        (cdb[3] > 0)) {
        bufLen &= 0xff; /* make sure cdb[3] is 0 next time around */
        goto try_again;
    }

    if ((res = scsiSimpleSenseFilter(&sinfo)))
        return res;
    /* Guard against devices that ignore EVPD bit and do standard INQUIRY */
    if (bufLen > 1) {
        if (vpd_page == pBuf[1]) {
            if ((0x80 == vpd_page) && (bufLen > 2) && (0x0 != pBuf[2]))
                return SIMPLE_ERR_BAD_RESP;
        } else
            return SIMPLE_ERR_BAD_RESP;
    }
    return 0;
}

/* REQUEST SENSE command. Returns 0 if ok, anything else major problem.
 * SPC-3 section 6.27 (rev 22a) */
int
scsiRequestSense(scsi_device * device, struct scsi_sense_disect * sense_info)
{
    struct scsi_cmnd_io io_hdr = {};
    uint8_t cdb[6] = {};
    uint8_t sense[32];
    uint8_t buff[18] = {};
    bool ok;
    static const int sz_buff = sizeof(buff);

    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = sz_buff;
    io_hdr.dxferp = buff;
    cdb[0] = REQUEST_SENSE;
    cdb[4] = sz_buff;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (sense_info)
        ok = scsi_pass_through_yield_sense(device, &io_hdr, *sense_info);
    else {
        scsi_sense_disect dummy_sense;
        ok = scsi_pass_through_yield_sense(device, &io_hdr, dummy_sense);
    }
    if (! ok)
      return -device->get_errno();
    if (sense_info) {
        uint8_t resp_code = buff[0] & 0x7f;
        sense_info->resp_code = resp_code;
        sense_info->sense_key = buff[2] & 0xf;
        sense_info->asc = 0;
        sense_info->ascq = 0;
        if ((0x70 == resp_code) || (0x71 == resp_code)) {
            int len = buff[7] + 8;
            if (len > 13) {
                sense_info->asc = buff[12];
                sense_info->ascq = buff[13];
            }
        }
    // fill progress indicator, if available
    sense_info->progress = -1;
    switch (resp_code) {
      const unsigned char * ucp;
      int sk, sk_pr;
      case 0x70:
      case 0x71:
          sk = (buff[2] & 0xf);
          if (! ((SCSI_SK_NO_SENSE == sk) || (SCSI_SK_NOT_READY == sk))) {
              break;
          }
          if (buff[15] & 0x80) {        /* SKSV bit set */
              sense_info->progress = sg_get_unaligned_be16(buff + 16);
              break;
          } else {
              break;
          }
      case 0x72:
      case 0x73:
          /* sense key specific progress (0x2) or progress descriptor (0xa) */
          sk = (buff[1] & 0xf);
          sk_pr = (SCSI_SK_NO_SENSE == sk) || (SCSI_SK_NOT_READY == sk);
          if (sk_pr && ((ucp = sg_scsi_sense_desc_find(buff, sz_buff, 2))) &&
              (0x6 == ucp[1]) && (0x80 & ucp[4])) {
              sense_info->progress = sg_get_unaligned_be16(ucp + 5);
              break;
          } else if (((ucp = sg_scsi_sense_desc_find(buff, sz_buff, 0xa))) &&
                     ((0x6 == ucp[1]))) {
              sense_info->progress = sg_get_unaligned_be16(ucp + 6);
              break;
          } else
              break;
      default:
          return 0;
      }
    }
    return 0;
}

/* Send Start Stop Unit command with power_condition setting and
 * Power condition command. Returns 0 if ok, anything else major problem.
 * If power_cond is 0, treat as SSU(START) as that is better than
 * SSU(STOP) which would be the case if byte 4 of the cdb was zero.
 * Ref: SBC-4 revision 22, section 4.20 SSU and power conditions.
 *
 * SCSI_POW_COND_ACTIVE                   0x1
 * SCSI_POW_COND_IDLE                     0x2
 * SCSI_POW_COND_STANDBY                  0x3
 *
 */

int
scsiSetPowerCondition(scsi_device * device, int power_cond, int pcond_modifier)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[6] = {};
    uint8_t sense[32];

    io_hdr.dxfer_dir = DXFER_NONE;
    cdb[0] = START_STOP_UNIT;
    /* IMMED bit (cdb[1] = 0x1) not set, therefore will wait */
    if (power_cond > 0) {
        cdb[3] = pcond_modifier & 0xf;
        cdb[4] = power_cond << 4;
    } else
        cdb[4] = 0x1;   /* START */
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
        return -device->get_errno();

    return scsiSimpleSenseFilter(&sinfo);
}

/* SEND DIAGNOSTIC command.  Returns 0 if ok, 1 if NOT READY, 2 if command
 * not supported, 3 if field in command not supported or returns negated
 * errno. SPC-3 section 6.28 (rev 22a) */
int
scsiSendDiagnostic(scsi_device * device, int functioncode, uint8_t *pBuf,
                   int bufLen)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[6] = {};
    uint8_t sense[32];

    io_hdr.dxfer_dir = bufLen ? DXFER_TO_DEVICE: DXFER_NONE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = SEND_DIAGNOSTIC;
    if (SCSI_DIAG_DEF_SELF_TEST == functioncode)
        cdb[1] = 0x4;  /* SelfTest bit */
    else if (SCSI_DIAG_NO_SELF_TEST != functioncode)
        cdb[1] = (functioncode & 0x7) << 5; /* SelfTest _code_ */
    else   /* SCSI_DIAG_NO_SELF_TEST == functioncode */
        cdb[1] = 0x10;  /* PF bit */
    sg_put_unaligned_be16(bufLen, cdb + 3);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    /* worst case is an extended foreground self test on a big disk */
    io_hdr.timeout = SCSI_TIMEOUT_SELF_TEST;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    return scsiSimpleSenseFilter(&sinfo);
}

/* TEST UNIT READY command. SPC-3 section 6.33 (rev 22a) */
static int
_testunitready(scsi_device * device, struct scsi_sense_disect * sinfop)
{
    struct scsi_cmnd_io io_hdr = {};
    bool ok;
    uint8_t cdb[6] = {};
    uint8_t sense[32];

    io_hdr.dxfer_dir = DXFER_NONE;
    io_hdr.dxfer_len = 0;
    io_hdr.dxferp = nullptr;
    cdb[0] = TEST_UNIT_READY;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (sinfop)
        ok = scsi_pass_through_yield_sense(device, &io_hdr, *sinfop);
    else {
        struct scsi_sense_disect dummy_si;
        ok = scsi_pass_through_yield_sense(device, &io_hdr, dummy_si);
    }
    if (! ok)
        return -device->get_errno();
    return 0;
}

/* Returns 0 for device responds and media ready, 1 for device responds and
   media not ready, or returns a negated errno value */
int
scsiTestUnitReady(scsi_device * device)
{
    struct scsi_sense_disect sinfo;
    int status;

    status = _testunitready(device, &sinfo);
    if (0 != status)
        return status;
    return scsiSimpleSenseFilter(&sinfo);
}

/* READ DEFECT (10) command. Returns 0 if ok, 1 if NOT READY, 2 if
 * command not supported, 3 if field in command not supported, 101 if
 * defect list not found (e.g. SSD may not have defect list) or returns
 * negated errno. SBC-2 section 5.12 (rev 16) */
int
scsiReadDefect10(scsi_device * device, int req_plist, int req_glist,
                 int dl_format, uint8_t *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[10] = {};
    uint8_t sense[32];

    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = READ_DEFECT_10;
    cdb[2] = (unsigned char)(((req_plist << 4) & 0x10) |
               ((req_glist << 3) & 0x8) | (dl_format & 0x7));
    sg_put_unaligned_be16(bufLen, cdb + 7);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
        return -device->get_errno();
    /* Look for "(Primary|Grown) defect list not found" */
    if ((sinfo.resp_code >= 0x70) && (0x1c == sinfo.asc))
        return 101;
    return scsiSimpleSenseFilter(&sinfo);
}

/* READ DEFECT (12) command. Returns 0 if ok, 1 if NOT READY, 2 if
 * command not supported, 3 if field in command not supported, 101 if
 * defect list not found (e.g. SSD may not have defect list) or returns
 * negated errno. SBC-3 section 5.18 (rev 35; vale Mark Evans) */
int
scsiReadDefect12(scsi_device * device, int req_plist, int req_glist,
                 int dl_format, int addrDescIndex, uint8_t *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[12] = {};
    uint8_t sense[32];

    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = READ_DEFECT_12;
    cdb[1] = (unsigned char)(((req_plist << 4) & 0x10) |
               ((req_glist << 3) & 0x8) | (dl_format & 0x7));
    sg_put_unaligned_be32(addrDescIndex, cdb + 2);
    sg_put_unaligned_be32(bufLen, cdb + 6);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    /* Look for "(Primary|Grown) defect list not found" */
    if ((sinfo.resp_code >= 0x70) && (0x1c == sinfo.asc))
        return 101;
    return scsiSimpleSenseFilter(&sinfo);
}

/* Call scsi_pass_through, and retry only if a UNIT_ATTENTION (UA) is raised.
 * When false returned, the caller should invoke device->get_error().
 * When true returned, the caller should check sinfo.
 * All SCSI commands can receive pending Unit Attentions, apart from:
 * INQUIRY, REPORT LUNS, REQUEST SENSE and NOTIFY DATA TRANSFER DEVICE
 * (ADC-3 spec). The first three are the important ones. */
bool
scsi_pass_through_yield_sense(scsi_device * device, scsi_cmnd_io * iop,
                              /* OUT param */ scsi_sense_disect & sinfo)
{
    int k;
    uint32_t opcode = (iop->cmnd_len > 0) ? iop->cmnd[0] : 0xffff;

    if (scsi_debugmode > 2) {
        bool dout = false;
        const char * ddir = "none";
        const char * np;

        if (iop->dxfer_len > 0) {
            dout = (DXFER_TO_DEVICE == iop->dxfer_dir);
            ddir = dout ? "out" : "in";
        }
        np = scsi_get_opcode_name(iop->cmnd);
        pout(" [%s: ", np ? np : "<unknown opcode>");
        pout("SCSI opcode=0x%x, CDB length=%u, data length=0x%u, data "
             "dir=%s]\n", opcode, (unsigned int)iop->cmnd_len,
             (unsigned int)iop->dxfer_len, ddir);
        if (dout && (scsi_debugmode > 3))  /* output hex without address */
            dStrHexFp(iop->dxferp, iop->dxfer_len, -1, nullptr);
    }

    if (! device->scsi_pass_through(iop))
        return false; // this will be missing device, timeout, etc

    if (scsi_debugmode > 3) {
        unsigned int req_len = iop->dxfer_len;
        unsigned int act_len;

        if ((req_len > 0) && (DXFER_FROM_DEVICE == iop->dxfer_dir) &&
            (iop->resid >= 0) && (req_len >= (unsigned int)iop->resid)) {
            act_len = req_len - (unsigned int)iop->resid;
            pout("  [data-in buffer: req_len=%u, resid=%d, gives %u "
                 "bytes]\n", req_len, iop->resid, act_len);
            dStrHexFp(iop->dxferp, act_len, -1, nullptr);
        }
    }
    scsi_do_sense_disect(iop, &sinfo);

    switch (opcode) {
    case INQUIRY:
    case REPORT_LUNS:
    case REQUEST_SENSE:
        return true;    /* in these cases, it shouldn't be a UA */
    default:
        break;  /* continue on for all other SCSI commands to check for UA */
    }

    /* There can be multiple UAs pending, allow for three */
    for (k = 0; (k < 3) && (SCSI_SK_UNIT_ATTENTION == sinfo.sense_key); ++k) {
        if (scsi_debugmode > 0)
            pout("%s Unit Attention %d: asc/ascq=0x%x,0x%x, retrying\n",
                 __func__, k + 1, sinfo.asc, sinfo.ascq);
        if (! device->scsi_pass_through(iop))
            return false;
        scsi_do_sense_disect(iop, &sinfo);
    }
    return true;
}

/* READ CAPACITY (10) command. Returns 0 if ok, 1 if NOT READY, 2 if
 * command not supported, 3 if field in command not supported or returns
 * negated errno. SBC-3 section 5.15 (rev 26) */
int
scsiReadCapacity10(scsi_device * device, unsigned int * last_lbap,
                   unsigned int * lb_sizep)
{
    int res;
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[10] = {};
    uint8_t sense[32];
    uint8_t resp[8] = {};

    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = sizeof(resp);
    io_hdr.dxferp = resp;
    cdb[0] = READ_CAPACITY_10;
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (! scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    res = scsiSimpleSenseFilter(&sinfo);
    if (res)
        return res;
    if (last_lbap)
        *last_lbap = sg_get_unaligned_be32(resp + 0);
    if (lb_sizep)
        *lb_sizep = sg_get_unaligned_be32(resp + 4);
    return 0;
}

/* READ CAPACITY (16) command. The bufLen argument should be 32. Returns 0
 * if ok, 1 if NOT READY, 2 if command not supported, 3 if field in command
 * not supported or returns negated errno. SBC-3 section 5.16 (rev 26) */
int
scsiReadCapacity16(scsi_device * device, uint8_t *pBuf, int bufLen)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    uint8_t cdb[16] = {};
    uint8_t sense[32];

    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = SERVICE_ACTION_IN_16;
    cdb[1] = SAI_READ_CAPACITY_16;
    sg_put_unaligned_be32(bufLen, cdb + 10);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (!scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    return scsiSimpleSenseFilter(&sinfo);
}

/* REPORT SUPPORTED OPERATION CODES [RSOC] command. If SIMPLE_NO_ERROR is
 * returned then the response length is written to rspLen. */
int
scsiRSOCcmd(scsi_device * device, bool rctd, uint8_t rep_opt, uint8_t opcode,
            uint16_t serv_act, uint8_t *pBuf, int bufLen, int & rspLen)
{
    struct scsi_cmnd_io io_hdr = {};
    struct scsi_sense_disect sinfo;
    int res;
    uint8_t cdb[12] = {};
    uint8_t sense[32];

    io_hdr.dxfer_dir = DXFER_FROM_DEVICE;
    io_hdr.dxfer_len = bufLen;
    io_hdr.dxferp = pBuf;
    cdb[0] = MAINTENANCE_IN_12;
    cdb[1] = MI_REP_SUP_OPCODES;
    if (rctd)
        cdb[2] |= 0x80;
    if (rep_opt > 0)
        cdb[2] |= (0x7 & rep_opt);
    cdb[3] = opcode;
    sg_put_unaligned_be16(serv_act, cdb + 4);
    sg_put_unaligned_be32(bufLen, cdb + 6);
    io_hdr.cmnd = cdb;
    io_hdr.cmnd_len = sizeof(cdb);
    io_hdr.sensep = sense;
    io_hdr.max_sense_len = sizeof(sense);
    io_hdr.timeout = SCSI_TIMEOUT_DEFAULT;

    if (!scsi_pass_through_yield_sense(device, &io_hdr, sinfo))
      return -device->get_errno();
    res = scsiSimpleSenseFilter(&sinfo);
    if (SIMPLE_NO_ERROR == res)
        rspLen = bufLen - io_hdr.resid;
    return res;
}

/* Return number of bytes of storage in 'device' or 0 if error. If
 * successful and lb_sizep is not nullptr then the logical block size in bytes
 * is written to the location pointed to by lb_sizep. If the 'Logical Blocks
 * per Physical Block Exponent' pointer (lb_per_pb_expp,) is non-null then
 * the value is written. If 'Protection information Intervals Exponent'*/
uint64_t
scsiGetSize(scsi_device * device, bool avoid_rcap16,
            struct scsi_readcap_resp * srrp)
{
    bool try_16 = false;
    bool try_12 = false;
    unsigned int last_lba = 0, lb_size = 0;
    int res;
    uint64_t ret_val = 0;
    uint8_t rc16resp[32];

    if (avoid_rcap16) {
        res = scsiReadCapacity10(device, &last_lba, &lb_size);
        if (res) {
            if (scsi_debugmode)
                pout("%s: READ CAPACITY(10) failed, res=%d\n", __func__, res);
            try_16 = true;
        } else {        /* rcap10 succeeded */
            if (0xffffffff == last_lba) {
                /* so number of blocks needs > 32 bits to represent */
                try_16 = true;
                device->set_rcap16_first();
            } else {
                ret_val = last_lba + 1;
                if (srrp) {
                    memset(srrp, 0, sizeof(*srrp));
                    srrp->num_lblocks = ret_val;
                    srrp->lb_size = lb_size;
                }
            }
        }
    } else if (SC_SUPPORT ==
               device->cmd_support_level(SERVICE_ACTION_IN_16, true,
                                         SAI_READ_CAPACITY_16))
        try_16 = true;

    if (try_16 || (! avoid_rcap16)) {
        res = scsiReadCapacity16(device, rc16resp, sizeof(rc16resp));
        if (res) {
            if (scsi_debugmode)
                pout("%s: READ CAPACITY(16) failed, res=%d\n", __func__, res);
            if (try_16)         /* so already tried rcap10 */
                return 0;
            try_12 = true;
        } else {        /* rcap16 succeeded */
            ret_val = sg_get_unaligned_be64(rc16resp + 0) + 1;
            lb_size = sg_get_unaligned_be32(rc16resp + 8);
            if (srrp) {         /* writes to all fields */
                srrp->num_lblocks = ret_val;
                srrp->lb_size = lb_size;
                bool prot_en = !!(0x1 & rc16resp[12]);
                uint8_t p_type = ((rc16resp[12] >> 1) & 0x7);
                srrp->prot_type = prot_en ? (1 + p_type) : 0;
                srrp->p_i_exp = ((rc16resp[13] >> 4) & 0xf);
                srrp->lb_p_pb_exp = (rc16resp[13] & 0xf);
                srrp->lbpme = !!(0x80 & rc16resp[14]);
                srrp->lbprz = !!(0x40 & rc16resp[14]);
                srrp->l_a_lba = sg_get_unaligned_be16(rc16resp + 14) & 0x3fff;
            }
        }
    }
    if (try_12) {  /* case where only rcap16 has been tried and failed */
        res = scsiReadCapacity10(device, &last_lba, &lb_size);
        if (res) {
            if (scsi_debugmode)
                pout("%s: 2nd READ CAPACITY(10) failed, res=%d\n", __func__,
                     res);
            return 0;
        } else {        /* rcap10 succeeded */
            ret_val = (uint64_t)last_lba + 1;
            if (srrp) {
                memset(srrp, 0, sizeof(*srrp));
                srrp->num_lblocks = ret_val;
                srrp->lb_size = lb_size;
            }
        }
    }
    return (ret_val * lb_size);
}

/* Offset into mode sense (6 or 10 byte) response that actual mode page
 * starts at (relative to resp[0]). Returns -1 if problem */
int
scsiModePageOffset(const uint8_t * resp, int len, int modese_len)
{
    int offset = -1;

    if (resp) {
        int resp_len, bd_len;
        if (10 == modese_len) {
            resp_len = sg_get_unaligned_be16(resp + 0) + 2;
            bd_len = sg_get_unaligned_be16(resp + 6);
            offset = bd_len + 8;
        } else {
            resp_len = resp[0] + 1;
            bd_len = resp[3];
            offset = bd_len + 4;
        }
        if ((offset + 2) > len) {
            pout("scsiModePageOffset: raw_curr too small, offset=%d "
                 "resp_len=%d bd_len=%d\n", offset, resp_len, bd_len);
            offset = -1;
        } else if ((offset + 2) > resp_len) {
             if ((resp_len > 2) || scsi_debugmode)
                pout("scsiModePageOffset: response length too short, "
                     "resp_len=%d offset=%d bd_len=%d\n", resp_len,
                     offset, bd_len);
            offset = -1;
        }
    }
    return offset;
}

/* IEC mode page byte 2 bit masks */
#define DEXCPT_ENABLE   0x08
#define EWASC_ENABLE    0x10
#define DEXCPT_DISABLE  0xf7
#define EWASC_DISABLE   0xef
#define TEST_DISABLE    0xfb

/* Fetches the Informational Exceptions Control mode page. First tries
 * the 6 byte MODE SENSE command and if that fails with an illegal opcode
 * tries a 10 byte MODE SENSE command. Returns 0 if successful, a positive
 * number if a known error (see  SIMPLE_ERR_ ...) or a negative errno
 * value. */
int
scsiFetchIECmpage(scsi_device * device, struct scsi_iec_mode_page *iecp,
                  int modese_len)
{
    int err = 0;

    memset(iecp, 0, sizeof(*iecp));
    iecp->modese_len = modese_len;
    iecp->requestedCurrent = 1;
    if (iecp->modese_len <= 6) {
        if ((err = scsiModeSense(device, INFORMATIONAL_EXCEPTIONS_CONTROL_PAGE,
                                 0, MPAGE_CONTROL_CURRENT,
                                 iecp->raw_curr, sizeof(iecp->raw_curr)))) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                iecp->modese_len = 10;
            else {
                iecp->modese_len = 0;
                return err;
            }
        } else if (0 == iecp->modese_len)
            iecp->modese_len = 6;
    }
    if (10 == iecp->modese_len) {
        err = scsiModeSense10(device, INFORMATIONAL_EXCEPTIONS_CONTROL_PAGE,
                              0, MPAGE_CONTROL_CURRENT,
                              iecp->raw_curr, sizeof(iecp->raw_curr));
        if (err) {
            iecp->modese_len = 0;
            return err;
        }
    }
    iecp->gotCurrent = 1;
    iecp->requestedChangeable = 1;
    if (10 == iecp->modese_len)
        err = scsiModeSense10(device, INFORMATIONAL_EXCEPTIONS_CONTROL_PAGE,
                              0, MPAGE_CONTROL_CHANGEABLE,
                              iecp->raw_chg, sizeof(iecp->raw_chg));
    else if (6 == iecp->modese_len)
        err = scsiModeSense(device, INFORMATIONAL_EXCEPTIONS_CONTROL_PAGE,
                            0, MPAGE_CONTROL_CHANGEABLE,
                            iecp->raw_chg, sizeof(iecp->raw_chg));
    if (err)
        return err;
    iecp->gotChangeable = 1;
    return 0;
}

int
scsi_IsExceptionControlEnabled(const struct scsi_iec_mode_page *iecp)
{
    if (iecp && iecp->gotCurrent) {
        int offset = scsiModePageOffset(iecp->raw_curr, sizeof(iecp->raw_curr),
                                        iecp->modese_len);
        if (offset >= 0)
            return (iecp->raw_curr[offset + 2] & DEXCPT_ENABLE) ? 0 : 1;
        else
            return 0;
    } else
        return 0;
}

int
scsi_IsWarningEnabled(const struct scsi_iec_mode_page *iecp)
{
    if (iecp && iecp->gotCurrent) {
        int offset = scsiModePageOffset(iecp->raw_curr, sizeof(iecp->raw_curr),
                                        iecp->modese_len);
        if (offset >= 0)
            return (iecp->raw_curr[offset + 2] & EWASC_ENABLE) ? 1 : 0;
        else
            return 0;
    } else
        return 0;
}

/* set EWASC and clear PERF, EBF, DEXCPT TEST and LOGERR */
#define SCSI_IEC_MP_BYTE2_ENABLED 0x10
#define SCSI_IEC_MP_BYTE2_TEST_MASK 0x4
/* exception/warning via an unrequested REQUEST SENSE command */
#define SCSI_IEC_MP_MRIE 6
#define SCSI_IEC_MP_INTERVAL_T 0
#define SCSI_IEC_MP_REPORT_COUNT 1

/* Try to set (or clear) both Exception Control and Warning in the IE
 * mode page subject to the "changeable" mask. The object pointed to
 * by iecp is (possibly) inaccurate after this call, therefore
 * scsiFetchIECmpage() should be called again if the IEC mode page
 * is to be re-examined.
 * When -r ioctl is invoked 3 or more time on 'smartctl -s on ...'
 * then set the TEST bit (causes asc,ascq pair of 0x5d,0xff). */
int
scsiSetExceptionControlAndWarning(scsi_device * device, int enabled,
                                  const struct scsi_iec_mode_page *iecp)
{
    int offset, resp_len;
    int err = 0;
    uint8_t rout[SCSI_IECMP_RAW_LEN];

    if ((! iecp) || (! iecp->gotCurrent))
        return -EINVAL;
    offset = scsiModePageOffset(iecp->raw_curr, sizeof(iecp->raw_curr),
                                iecp->modese_len);
    if (offset < 0)
        return -EINVAL;
    memcpy(rout, iecp->raw_curr, SCSI_IECMP_RAW_LEN);
    /* mask out DPOFUA device specific (disk) parameter bit */
    if (10 == iecp->modese_len) {
        resp_len = sg_get_unaligned_be16(rout + 0) + 2;
        rout[3] &= 0xef;
    } else {
        resp_len = rout[0] + 1;
        rout[2] &= 0xef;
    }
    int sp = !! (rout[offset] & 0x80); /* PS bit becomes 'SELECT's SP bit */
    if (enabled) {
        rout[offset + 2] = SCSI_IEC_MP_BYTE2_ENABLED;
        if (scsi_debugmode > 2)
            rout[offset + 2] |= SCSI_IEC_MP_BYTE2_TEST_MASK;
        rout[offset + 3] = SCSI_IEC_MP_MRIE;
        sg_put_unaligned_be32(SCSI_IEC_MP_INTERVAL_T, rout + offset + 4);
        sg_put_unaligned_be32(SCSI_IEC_MP_REPORT_COUNT, rout + offset + 8);
        if (iecp->gotChangeable) {
            uint8_t chg2 = iecp->raw_chg[offset + 2];

            rout[offset + 2] = chg2 ? (rout[offset + 2] & chg2) :
                                      iecp->raw_curr[offset + 2];
            for (int k = 3; k < 12; ++k) {
                if (0 == iecp->raw_chg[offset + k])
                    rout[offset + k] = iecp->raw_curr[offset + k];
            }
        }
        if (0 == memcmp(&rout[offset + 2], &iecp->raw_chg[offset + 2], 10)) {
            if (scsi_debugmode > 0)
                pout("scsiSetExceptionControlAndWarning: already enabled\n");
            return 0;
        }
    } else { /* disabling Exception Control and (temperature) Warnings */
        int eCEnabled = (rout[offset + 2] & DEXCPT_ENABLE) ? 0 : 1;
        int wEnabled = (rout[offset + 2] & EWASC_ENABLE) ? 1 : 0;
        if ((! eCEnabled) && (! wEnabled)) {
            if (scsi_debugmode > 0)
                pout("scsiSetExceptionControlAndWarning: already disabled\n");
            return 0;   /* nothing to do, leave other setting alone */
        }
        if (wEnabled)
            rout[offset + 2] &= EWASC_DISABLE;
        if (eCEnabled) {
            if (iecp->gotChangeable &&
                (iecp->raw_chg[offset + 2] & DEXCPT_ENABLE))
                rout[offset + 2] |= DEXCPT_ENABLE;
            rout[offset + 2] &= TEST_DISABLE; /* clear TEST bit for spec */
        }
    }
    if (10 == iecp->modese_len)
        err = scsiModeSelect10(device, sp, rout, resp_len);
    else if (6 == iecp->modese_len)
        err = scsiModeSelect(device, sp, rout, resp_len);
    return err;
}

int
scsiGetTemp(scsi_device * device, uint8_t *currenttemp, uint8_t *triptemp)
{
    uint8_t tBuf[252] = {};
    int err;

    if ((err = scsiLogSense(device, TEMPERATURE_LPAGE, 0, tBuf,
                            sizeof(tBuf), 0))) {
        *currenttemp = 0;
        *triptemp = 0;
        pout("%s for temperature failed [%s]\n", logSenStr,
             scsiErrString(err));
        return err;
    }
    *currenttemp = tBuf[9];
    *triptemp = tBuf[15];
    return 0;
}

/* Informational Exception conditions specified by spc6r06.pdf seem to be
 * associated with ASC values 0xb (warnings) and 0x5d (impending failures).
 * The asc/accq value 0x5d,0xff is reported in response to setting the TEST
 * bit in the Informationl Exception Control mode page. */

/* Read informational exception log page or Request Sense response.
 * Fetching asc/ascq code potentially flagging an exception or warning.
 * Returns 0 if ok, else error number. A current temperature of 255
 * (Celsius) implies that the temperature not available. */
int
scsiCheckIE(scsi_device * device, int hasIELogPage, int hasTempLogPage,
            uint8_t *asc, uint8_t *ascq, uint8_t *currenttemp,
            uint8_t *triptemp)
{
    uint8_t tBuf[252] = {};
    struct scsi_sense_disect sense_info;
    int err;
    uint8_t currTemp, trTemp;

    memset(&sense_info, 0, sizeof(sense_info));
    *asc = 0;
    *ascq = 0;
    *currenttemp = 0;
    *triptemp = 0;
    if (hasIELogPage) {
        if ((err = scsiLogSense(device, IE_LPAGE, 0, tBuf,
                                sizeof(tBuf), 0))) {
            pout("%s failed, IE page [%s]\n", logSenStr, scsiErrString(err));
            return err;
        }
        // pull out page size from response, don't forget to add 4
        unsigned short pagesize = sg_get_unaligned_be16(tBuf + 2) + 4;
        if ((pagesize < 4) || tBuf[4] || tBuf[5]) {
            pout("%s failed, IE page, bad parameter code or length\n",
                 logSenStr);
            return SIMPLE_ERR_BAD_PARAM;
        }
        if (tBuf[7] > 1) {
            sense_info.asc = tBuf[8];
            sense_info.ascq = tBuf[9];
            if (! hasTempLogPage) {
                if (tBuf[7] > 2)
                    *currenttemp = tBuf[10];
                if (tBuf[7] > 3)        /* IBM extension in SMART (IE) lpage */
                    *triptemp = tBuf[11];
            }
        }
    }
    if (0 == sense_info.asc) {
        /* ties in with MRIE field of 6 in IEC mode page (0x1c) */
        if ((err = scsiRequestSense(device, &sense_info))) {
            pout("Request Sense failed, [%s]\n", scsiErrString(err));
            return err;
        }
    }
    *asc = sense_info.asc;
    *ascq = sense_info.ascq;
    if (hasTempLogPage) {
        if (0 == scsiGetTemp(device, &currTemp, &trTemp)) {
            *currenttemp = currTemp;
            *triptemp = trTemp;
        }
    }
    return 0;
}

// The first character (W, C, I) tells the severity
static const char * TapeAlertsMessageTable[]= {
    " ",
    /* 0x01 */
   "W: The tape drive is having problems reading data. No data has been "
   "lost,\n"
       "  but there has been a reduction in the performance of the tape.",
    /* 0x02 */
   "W: The tape drive is having problems writing data. No data has been "
   "lost,\n"
       "  but there has been a reduction in the capacity of the tape.",
    /* 0x03 */
   "W: The operation has stopped because an error has occurred while "
   "reading\n"
       "  or writing data that the drive cannot correct.",
    /* 0x04 */
   "C: Your data is at risk:\n"
       "  1. Copy any data you require from this tape. \n"
       "  2. Do not use this tape again.\n"
       "  3. Restart the operation with a different tape.",
    /* 0x05 */
   "C: The tape is damaged or the drive is faulty. Call the tape drive\n"
       "  supplier helpline.",
    /* 0x06 */
   "C: The tape is from a faulty batch or the tape drive is faulty:\n"
       "  1. Use a good tape to test the drive.\n"
       "  2. If problem persists, call the tape drive supplier helpline.",
    /* 0x07 */
   "W: The tape cartridge has reached the end of its calculated useful "
   "life:\n"
       "  1. Copy data you need to another tape.\n"
       "  2. Discard the old tape.",
    /* 0x08 */
   "W: The tape cartridge is not data-grade. Any data you back up to the "
   "tape\n"
       "  is at risk. Replace the cartridge with a data-grade tape.",
    /* 0x09 */
   "C: You are trying to write to a write-protected cartridge. Remove the\n"
       "  write-protection or use another tape.",
    /* 0x0a */
   "I: You cannot eject the cartridge because the tape drive is in use. "
   "Wait\n"
       "  until the operation is complete before ejecting the cartridge.",
    /* 0x0b */
   "I: The tape in the drive is a cleaning cartridge.",
    /* 0x0c */
   "I: You have tried to load a cartridge of a type which is not supported\n"
       "  by this drive.",
    /* 0x0d */
   "C: The operation has failed because the tape in the drive has "
   "experienced\n"
       "  a mechanical failure:\n"
       "  1. Discard the old tape.\n"
       "  2. Restart the operation with a different tape.",
    /* 0x0e */
   "C: The operation has failed because the tape in the drive has "
   "experienced\n"
       "  a mechanical failure:\n"
       "  1. Do not attempt to extract the tape cartridge\n"
       "  2. Call the tape drive supplier helpline.",
    /* 0x0f */
   "W: The memory in the tape cartridge has failed, which reduces\n"
       "  performance. Do not use the cartridge for further write "
       "operations.",
    /* 0x10 */
   "C: The operation has failed because the tape cartridge was manually\n"
       "  de-mounted while the tape drive was actively writing or reading.",
    /* 0x11 */
   "W: You have loaded a cartridge of a type that is read-only in this "
   "drive.\n"
       "  The cartridge will appear as write-protected.",
    /* 0x12 */
   "W: The tape directory on the tape cartridge has been corrupted. File\n"
       "  search performance will be degraded. The tape directory can be "
       "rebuilt\n"
       "  by reading all the data on the cartridge.",
    /* 0x13 */
   "I: The tape cartridge is nearing the end of its calculated life. It is\n"
       "  recommended that you:\n"
       "  1. Use another tape cartridge for your next backup.\n"
       "  2. Store this tape in a safe place in case you need to restore "
       "  data from it.",
    /* 0x14 */
   "C: The tape drive needs cleaning:\n"
       "  1. If the operation has stopped, eject the tape and clean the "
       "drive.\n"
       "  2. If the operation has not stopped, wait for it to finish and "
       "then\n"
       "  clean the drive.\n"
       "  Check the tape drive users manual for device specific cleaning "
       "instructions.",
    /* 0x15 */
   "W: The tape drive is due for routine cleaning:\n"
       "  1. Wait for the current operation to finish.\n"
       "  2. The use a cleaning cartridge.\n"
       "  Check the tape drive users manual for device specific cleaning "
       "instructions.",
    /* 0x16 */
   "C: The last cleaning cartridge used in the tape drive has worn out:\n"
       "  1. Discard the worn out cleaning cartridge.\n"
       "  2. Wait for the current operation to finish.\n"
       "  3. Then use a new cleaning cartridge.",
    /* 0x17 */
   "C: The last cleaning cartridge used in the tape drive was an invalid\n"
       "  type:\n"
       "  1. Do not use this cleaning cartridge in this drive.\n"
       "  2. Wait for the current operation to finish.\n"
       "  3. Then use a new cleaning cartridge.",
    /* 0x18 */
   "W: The tape drive has requested a retention operation",
    /* 0x19 */
   "W: A redundant interface port on the tape drive has failed",
    /* 0x1a */
   "W: A tape drive cooling fan has failed",
    /* 0x1b */
   "W: A redundant power supply has failed inside the tape drive enclosure.\n"
       "  Check the enclosure users manual for instructions on replacing "
       "the\n"
       "  failed power supply.",
    /* 0x1c */
   "W: The tape drive power consumption is outside the specified range.",
    /* 0x1d */
   "W: Preventive maintenance of the tape drive is required. Check the tape\n"
       "  drive users manual for device specific preventive maintenance\n"
       "  tasks or call the tape drive supplier helpline.",
    /* 0x1e */
   "C: The tape drive has a hardware fault:\n"
       "  1. Eject the tape or magazine.\n"
       "  2. Reset the drive.\n"
       "  3. Restart the operation.",
    /* 0x1f */
   "C: The tape drive has a hardware fault:\n"
       "  1. Turn the tape drive off and then on again.\n"
       "  2. Restart the operation.\n"
    "  3. If the problem persists, call the tape drive supplier helpline.",
    /* 0x20 */
   "W: The tape drive has a problem with the application client interface:\n"
       "  1. Check the cables and cable connections.\n"
       "  2. Restart the operation.",
    /* 0x21 */
   "C: The operation has failed:\n"
       "  1. Eject the tape or magazine.\n"
       "  2. Insert the tape or magazine again.\n"
       "  3. Restart the operation.",
    /* 0x22 */
   "W: The firmware download has failed because you have tried to use the\n"
       "  incorrect firmware for this tape drive. Obtain the correct\n"
       "  firmware and try again.",
    /* 0x23 */
   "W: Environmental conditions inside the tape drive are outside the\n"
       "  specified humidity range.",
    /* 0x24 */
   "W: Environmental conditions inside the tape drive are outside the\n"
       "  specified temperature range.",
    /* 0x25 */
   "W: The voltage supply to the tape drive is outside the specified range.",
    /* 0x26 */
   "C: A hardware failure of the tape drive is predicted. Call the tape\n"
       "  drive supplier helpline.",
    /* 0x27 */
   "W: The tape drive may have a hardware fault. Run extended diagnostics to\n"
       "  verify and diagnose the problem. Check the tape drive users manual "
       "for\n"
       "  device specific instructions on running extended diagnostic tests.",
    /* 0x28 */
   "C: The changer mechanism is having difficulty communicating with the "
       "tape\n"
       "  drive:\n"
       "  1. Turn the autoloader off then on.\n"
       "  2. Restart the operation.\n"
       "  3. If problem persists, call the tape drive supplier helpline.",
    /* 0x29 */
   "C: A tape has been left in the autoloader by a previous hardware fault:\n"
       "  1. Insert an empty magazine to clear the fault.\n"
       "  2. If the fault does not clear, turn the autoloader off and then\n"
       "  on again.\n"
       "  3. If the problem persists, call the tape drive supplier helpline.",
    /* 0x2a */
   "W: There is a problem with the autoloader mechanism.",
    /* 0x2b */
   "C: The operation has failed because the autoloader door is open:\n"
       "  1. Clear any obstructions from the autoloader door.\n"
       "  2. Eject the magazine and then insert it again.\n"
       "  3. If the fault does not clear, turn the autoloader off and then\n"
       "  on again.\n"
       "  4. If the problem persists, call the tape drive supplier helpline.",
    /* 0x2c */
   "C: The autoloader has a hardware fault:\n"
       "  1. Turn the autoloader off and then on again.\n"
       "  2. Restart the operation.\n"
       "  3. If the problem persists, call the tape drive supplier helpline.\n"
       "  Check the autoloader users manual for device specific instructions\n"
       "  on turning the device power on and off.",
    /* 0x2d */
   "C: The autoloader cannot operate without the magazine,\n"
       "  1. Insert the magazine into the autoloader.\n"
       "  2. Restart the operation.",
    /* 0x2e */
   "W: A hardware failure of the changer mechanism is predicted. Call the\n"
       "  tape drive supplier helpline.",
    /* 0x2f */
   "I: Reserved.",
    /* 0x30 */
   "I: Reserved.",
    /* 0x31 */
   "I: Reserved.",
    /* 0x32 */
   "W: Media statistics have been lost at some time in the past",
    /* 0x33 */
   "W: The tape directory on the tape cartridge just unloaded has been\n"
       "  corrupted. File search performance will be degraded. The tape\n"
       "  directory can be rebuilt by reading all the data.",
    /* 0x34 */
   "C: The tape just unloaded could not write its system area successfully:\n"
       "  1. Copy data to another tape cartridge.\n"
       "  2. Discard the old cartridge.",
    /* 0x35 */
   "C: The tape system are could not be read successfully at load time:\n"
    "  1. Copy data to another tape cartridge.\n",
    /* 0x36 */
   "C: The start or data could not be found on the tape:\n"
       "  1. Check you are using the correct format tape.\n"
       "  2. Discard the tape or return the tape to your supplier",
    /* 0x37 */
    "C: The operation has failed because the media cannot be loaded\n"
        "  and threaded.\n"
        "  1. Remove the cartridge, inspect it as specified in the product\n"
        "  manual, and retry the operation.\n"
        "  2. If the problem persists, call the tape drive supplier help "
        "line.",
    /* 0x38 */
    "C: The operation has failed because the medium cannot be unloaded:\n"
        "  1. Do not attempt to extract the tape cartridge.\n"
        "  2. Call the tape driver supplier help line.",
    /* 0x39 */
    "C: The tape drive has a problem with the automation interface:\n"
        "  1. Check the power to the automation system.\n"
        "  2. Check the cables and cable connections.\n"
        "  3. Call the supplier help line if problem persists.",
    /* 0x3a */
    "W: The tape drive has reset itself due to a detected firmware\n"
        "  fault. If problem persists, call the supplier help line.",
    };

const char *
scsiTapeAlertsTapeDevice(unsigned short code)
{
    static const int num = sizeof(TapeAlertsMessageTable) /
                           sizeof(TapeAlertsMessageTable[0]);

    return (code < num) ?  TapeAlertsMessageTable[code] : "Unknown Alert";
}

// The first character (W, C, I) tells the severity
static const char * ChangerTapeAlertsMessageTable[]= {
    " ",
    /* 0x01 */
    "C: The library mechanism is having difficulty communicating with the\n"
        "  drive:\n"
        "  1. Turn the library off then on.\n"
        "  2. Restart the operation.\n"
        "  3. If the problem persists, call the library supplier help line.",
    /* 0x02 */
    "W: There is a problem with the library mechanism. If problem persists,\n"
        "  call the library supplier help line.",
    /* 0x03 */
    "C: The library has a hardware fault:\n"
        "  1. Reset the library.\n"
        "  2. Restart the operation.\n"
        "  Check the library users manual for device specific instructions on "
        "resetting\n"
        "  the device.",
    /* 0x04 */
    "C: The library has a hardware fault:\n"
        "  1. Turn the library off then on again.\n"
        "  2. Restart the operation.\n"
        "  3. If the problem persists, call the library supplier help line.\n"
        "  Check the library users manual for device specific instructions on "
        "turning the\n"
        "  device power on and off.",
    /* 0x05 */
    "W: The library mechanism may have a hardware fault.\n"
        "  Run extended diagnostics to verify and diagnose the problem. "
        "Check the library\n"
        "  users manual for device specific instructions on running extended "
        "diagnostic\n"
        "  tests.",
    /* 0x06 */
    "C: The library has a problem with the host interface:\n"
        "  1. Check the cables and connections.\n"
        "  2. Restart the operation.",
    /* 0x07 */
    "W: A hardware failure of the library is predicted. Call the library\n"
        "  supplier help line.",
    /* 0x08 */
    "W: Preventive maintenance of the library is required.\n"
        "  Check the library users manual for device specific preventative "
        "maintenance\n"
        "  tasks, or call your library supplier help line.",
    /* 0x09 */
    "C: General environmental conditions inside the library are outside the\n"
        "  specified humidity range.",
    /* 0x0a */
    "C: General environmental conditions inside the library are outside the\n"
        "  specified temperature range.",
    /* 0x0b */
    "C: The voltage supply to the library is outside the specified range.\n"
        "  There is a potential problem with the power supply or failure of\n"
        "  a redundant power supply.",
    /* 0x0c */
    "C: A cartridge has been left inside the library by a previous hardware\n"
        "  fault:\n"
        "  1. Insert an empty magazine to clear the fault.\n"
        "  2. If the fault does not clear, turn the library off and then on "
        "again.\n"
        "  3. If the problem persists, call the library supplier help line.",
    /* 0x0d */
    "W: There is a potential problem with the drive ejecting cartridges or\n"
        "  with the library mechanism picking a cartridge from a slot.\n"
        "  1. No action needs to be taken at this time.\n"
        "  2. If the problem persists, call the library supplier help line.",
    /* 0x0e */
    "W: There is a potential problem with the library mechanism placing a\n"
        "  cartridge into a slot.\n"
        "  1. No action needs to be taken at this time.\n"
        "  2. If the problem persists, call the library supplier help line.",
    /* 0x0f */
    "W: There is a potential problem with the drive or the library mechanism\n"
        "  loading cartridges, or an incompatible cartridge.",
    /* 0x10 */
    "C: The library has failed because the door is open:\n"
        "  1. Clear any obstructions from the library door.\n"
        "  2. Close the library door.\n"
        "  3. If the problem persists, call the library supplier help line.",
    /* 0x11 */
    "C: There is a mechanical problem with the library media import/export\n"
        "  mailslot.",
    /* 0x12 */
    "C: The library cannot operate without the magazine.\n"
        "  1. Insert the magazine into the library.\n"
        "  2. Restart the operation.",
    /* 0x13 */
    "W: Library security has been compromised.",
    /* 0x14 */
    "I: The library security mode has been changed.\n"
        "  The library has either been put into secure mode, or the library "
        "has exited\n"
        "  the secure mode.\n"
        "  This is for information purposes only. No action is required.",
    /* 0x15 */
    "I: The library has been manually turned offline and is unavailable for "
    "use.",
    /* 0x16 */
    "I: A drive inside the library has been taken offline.\n"
        "  This is for information purposes only. No action is required.",
    /* 0x17 */
    "W: There is a potential problem with the bar code label or the scanner\n"
        "  hardware in the library mechanism.\n"
        "  1. No action needs to be taken at this time.\n"
        "  2. If the problem persists, call the library supplier help line.",
    /* 0x18 */
    "C: The library has detected an inconsistency in its inventory.\n"
        "  1. Redo the library inventory to correct inconsistency.\n"
        "  2. Restart the operation.\n"
        "  Check the applications users manual or the hardware users manual "
        "for\n"
        "  specific instructions on redoing the library inventory.",
    /* 0x19 */
    "W: A library operation has been attempted that is invalid at this time.",
    /* 0x1a */
    "W: A redundant interface port on the library has failed.",
    /* 0x1b */
    "W: A library cooling fan has failed.",
    /* 0x1c */
    "W: A redundant power supply has failed inside the library. Check the\n"
        "  library users manual for instructions on replacing the failed "
        "power supply.",
    /* 0x1d */
    "W: The library power consumption is outside the specified range.",
    /* 0x1e */
    "C: A failure has occurred in the cartridge pass-through mechanism "
        "between\n"
        "  two library modules.",
    /* 0x1f */
    "C: A cartridge has been left in the pass-through mechanism from a\n"
        "  previous hardware fault. Check the library users guide for "
        "instructions on\n"
        "  clearing this fault.",
    /* 0x20 */
    "I: The library was unable to read the bar code on a cartridge.",
};

const char *
scsiTapeAlertsChangerDevice(unsigned short code)
{
    static const int num = sizeof(ChangerTapeAlertsMessageTable) /
                           sizeof(ChangerTapeAlertsMessageTable[0]);

    return (code < num) ?  ChangerTapeAlertsMessageTable[code] :
                           "Unknown Alert";
}

int
scsiSmartDefaultSelfTest(scsi_device * device)
{
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_DEF_SELF_TEST, nullptr, 0);
    if (res)
        pout("Default self test failed [%s]\n", scsiErrString(res));
    return res;
}

int
scsiSmartShortSelfTest(scsi_device * device)
{
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_BG_SHORT_SELF_TEST, nullptr, 0);
    if (res)
        pout("Short offline self test failed [%s]\n", scsiErrString(res));
    return res;
}

int
scsiSmartExtendSelfTest(scsi_device * device)
{
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_BG_EXTENDED_SELF_TEST, nullptr, 0);
    if (res)
        pout("Long (extended) offline self test failed [%s]\n",
             scsiErrString(res));
    return res;
}

int
scsiSmartShortCapSelfTest(scsi_device * device)
{
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_FG_SHORT_SELF_TEST, nullptr, 0);
    if (res)
        pout("Short foreground self test failed [%s]\n", scsiErrString(res));
    return res;
}

int
scsiSmartExtendCapSelfTest(scsi_device * device)
{
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_FG_EXTENDED_SELF_TEST, nullptr, 0);
    if (res)
        pout("Long (extended) foreground self test failed [%s]\n",
             scsiErrString(res));
    return res;
}

int
scsiSmartSelfTestAbort(scsi_device * device)
{
    int res;

    res = scsiSendDiagnostic(device, SCSI_DIAG_ABORT_SELF_TEST, nullptr, 0);
    if (res)
        pout("Abort self test failed [%s]\n", scsiErrString(res));
    return res;
}

/* Returns 0 and the expected duration of an extended self test (in seconds)
   if successful; any other return value indicates a failure. */
int
scsiFetchExtendedSelfTestTime(scsi_device * device, int * durationSec,
                              int modese_len)
{
    int err, offset;
    uint8_t buff[64] = {};

    if (modese_len <= 6) {
        if ((err = scsiModeSense(device, CONTROL_MODE_PAGE, 0,
                                 MPAGE_CONTROL_CURRENT,
                                 buff, sizeof(buff)))) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                modese_len = 10;
            else
                return err;
        } else if (0 == modese_len)
            modese_len = 6;
    }
    if (10 == modese_len) {
        err = scsiModeSense10(device, CONTROL_MODE_PAGE, 0,
                              MPAGE_CONTROL_CURRENT,
                              buff, sizeof(buff));
        if (err)
            return err;
    }
    offset = scsiModePageOffset(buff, sizeof(buff), modese_len);
    if (offset < 0)
        return -EINVAL;
    if (buff[offset + 1] >= 0xa) {
        int res = sg_get_unaligned_be16(buff + offset + 10);

        if (res < 0xffff) {
            *durationSec = res;
            return 0;
        }
        /* The value 0xffff (all bits set in 16 bit field) indicates that
         * the Extended Inquiry VPD page should be consulted, it has a
         * similarly named 16 bit field, but the unit is minutes. */
        uint8_t b[64];

        if ((0 == scsiInquiryVpd(device, SCSI_VPD_EXTENDED_INQUIRY_DATA,
                                 b, sizeof(b))) &&
            ((sg_get_unaligned_be16(b + 2)) > 11)) {
            res = sg_get_unaligned_be16(b + 10);
            *durationSec = res * 60;    /* VPD field is in minutes */
            return 0;
        } else
            return -EINVAL;
    } else
        return -EINVAL;
}

void
scsiDecodeErrCounterPage(unsigned char * resp, struct scsiErrorCounter *ecp,
                         int allocLen)
{
    memset(ecp, 0, sizeof(*ecp));
    int num = sg_get_unaligned_be16(resp + 2);
    unsigned char * ucp = &resp[0] + 4;

    /* allocLen is length of whole log page including 4 byte log page header */
    num = num < allocLen - 4 ? num : allocLen - 4;
    while (num >= 4) {  /* header of each parameter takes 4 bytes */
        int pc = sg_get_unaligned_be16(ucp + 0);
        int pl = ucp[3] + 4;
        uint64_t * ullp;

        if (num < pl)  /* remaining length less than a complete parameter */
            break;
        switch (pc) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
            ecp->gotPC[pc] = 1;
            ullp = &ecp->counter[pc];
        break;
        default:
            ecp->gotExtraPC = 1;
            ullp = &ecp->counter[7];
            break;
        }
        int k = pl - 4;
        unsigned char * xp = ucp + 4;
        if (k > (int)sizeof(*ullp)) {
            xp += (k - sizeof(*ullp));
            k = sizeof(*ullp);
        }
        *ullp = sg_get_unaligned_be(k, xp);
        num -= pl;
        ucp += pl;
    }
}

void
scsiDecodeNonMediumErrPage(unsigned char *resp,
                           struct scsiNonMediumError *nmep,
                           int allocLen)
{
    memset(nmep, 0, sizeof(*nmep));
    int num = sg_get_unaligned_be16(resp + 2);
    unsigned char * ucp = &resp[0] + 4;
    static int szof = sizeof(nmep->counterPC0);

    /* allocLen is length of whole log page including 4 byte log page header */
    num = num < allocLen - 4 ? num : allocLen - 4;
    while (num >= 4) {  /* header of each parameter takes 4 bytes */
        int pc = sg_get_unaligned_be16(ucp + 0);
        int pl = ucp[3] + 4;
        int k;
        unsigned char * xp;

        if (num < pl)  /* remaining length less than a complete parameter */
            break;
        switch (pc) {
        case 0:
            nmep->gotPC0 = 1;
            k = pl - 4;
            xp = ucp + 4;
            if (k > szof) {
                xp += (k - szof);
                k = szof;
            }
            nmep->counterPC0 = sg_get_unaligned_be(k, xp + 0);
            break;
        case 0x8009:
            nmep->gotTFE_H = 1;
            k = pl - 4;
            xp = ucp + 4;
            if (k > szof) {
                xp += (k - szof);
                k = szof;
            }
            nmep->counterTFE_H = sg_get_unaligned_be(k, xp + 0);
            break;
        case 0x8015:
            nmep->gotPE_H = 1;
            k = pl - 4;
            xp = ucp + 4;
            if (k > szof) {
                xp += (k - szof);
                k = szof;
            }
            nmep->counterPE_H = sg_get_unaligned_be(k, xp + 0);
            break;
        default:
            nmep->gotExtraPC = 1;
            break;
        }
        num -= pl;
        ucp += pl;
    }
}

/* Counts number of failed self-tests. Also encodes the poweron_hour
   of the most recent failed self-test. Return value is negative if
   this function has a problem (typically -1), otherwise the bottom 8
   bits are the number of failed self tests and the 16 bits above that
   are the poweron hour of the most recent failure. Note: aborted self
   tests (typically by the user) and self tests in progress are not
   considered failures. See Working Draft SCSI Primary Commands - 3
   (SPC-3) section 7.2.10 T10/1416-D (rev 22a) */
int
scsiCountFailedSelfTests(scsi_device * fd, int noisy)
{
    int num, k, err, fails, fail_hour;
    uint8_t * ucp;
    unsigned char resp[LOG_RESP_SELF_TEST_LEN];

    if ((err = scsiLogSense(fd, SELFTEST_RESULTS_LPAGE, 0, resp,
                            LOG_RESP_SELF_TEST_LEN, 0))) {
        if (noisy)
            pout("scsiCountSelfTests Failed [%s]\n", scsiErrString(err));
        return -1;
    }
    if ((resp[0] & 0x3f) != SELFTEST_RESULTS_LPAGE) {
        if (noisy)
            pout("Self-test %s Failed, page mismatch\n", logSenStr);
        return -1;
    }
    // compute page length
    num = sg_get_unaligned_be16(resp + 2);
    // Log sense page length 0x190 bytes
    if (num != 0x190) {
        if (noisy)
            pout("Self-test %s length is 0x%x not 0x190 bytes\n", logSenStr,
                 num);
        return -1;
    }
    fails = 0;
    fail_hour = 0;
    // loop through the twenty possible entries
    for (k = 0, ucp = resp + 4; k < 20; ++k, ucp += 20 ) {

        // timestamp in power-on hours (or zero if test in progress)
        int n = sg_get_unaligned_be16(ucp + 6);

        // The spec says "all 20 bytes will be zero if no test" but
        // DG has found otherwise.  So this is a heuristic.
        if ((0 == n) && (0 == ucp[4]))
            break;
        int res = ucp[4] & 0xf;
        if ((res > 2) && (res < 8)) {
            fails++;
            if (1 == fails)
                fail_hour = sg_get_unaligned_be16(ucp + 6);
        }
    }
    return (fail_hour << 8) + fails;
}

/* Returns 0 if able to read self test log page; then outputs 1 into
   *inProgress if self test still in progress, else outputs 0. */
int
scsiSelfTestInProgress(scsi_device * fd, int * inProgress)
{
    int num;
    uint8_t * ucp;
    unsigned char resp[LOG_RESP_SELF_TEST_LEN];

    if (scsiLogSense(fd, SELFTEST_RESULTS_LPAGE, 0, resp,
                     LOG_RESP_SELF_TEST_LEN, 0))
        return -1;
    if (resp[0] != SELFTEST_RESULTS_LPAGE)
        return -1;
    // compute page length
    num = sg_get_unaligned_be16(resp + 2);
    // Log sense page length 0x190 bytes
    if (num != 0x190) {
        return -1;
    }
    ucp = resp + 4;
    if (inProgress)
        *inProgress = (0xf == (ucp[4] & 0xf)) ? 1 : 0;
    return 0;
}

/* Returns a negative value if failed to fetch Control mode page or it was
   malformed. Returns 0 if GLTSD bit is zero and returns 1 if the GLTSD
   bit is set. Examines default mode page when current==0 else examines
   current mode page. */
int
scsiFetchControlGLTSD(scsi_device * device, int modese_len, int current)
{
    int err, offset;
    uint8_t buff[64] = {};
    int pc = current ? MPAGE_CONTROL_CURRENT : MPAGE_CONTROL_DEFAULT;

    if (modese_len <= 6) {
        if ((err = scsiModeSense(device, CONTROL_MODE_PAGE, 0, pc,
                                 buff, sizeof(buff)))) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                modese_len = 10;
            else
                return -EINVAL;
        } else if (0 == modese_len)
            modese_len = 6;
    }
    if (10 == modese_len) {
        err = scsiModeSense10(device, CONTROL_MODE_PAGE, 0, pc,
                              buff, sizeof(buff));
        if (err)
            return -EINVAL;
    }
    offset = scsiModePageOffset(buff, sizeof(buff), modese_len);
    if ((offset >= 0) && (buff[offset + 1] >= 0xa))
        return (buff[offset + 2] & 2) ? 1 : 0;
    return -EINVAL;
}

/* Returns a negative value on error, 0 if unknown and 1 if SSD,
 * otherwise the positive returned value is the speed in rpm. First checks
 * the Block Device Characteristics VPD page and if that fails it tries the
 * RIGID_DISK_DRIVE_GEOMETRY_PAGE mode page.
 * In SBC-4 the 2 bit ZONED field in this VPD page is written to *haw_zbcp
 * if haw_zbcp is non-nullptr. In SBC-5 the ZONED field is now obsolete,
 * the Zoned block device characteristics VPD page should be used instead. */

int
scsiGetRPM(scsi_device * device, int modese_len, int * form_factorp,
           int * haw_zbcp)
{
    int err, offset;
    uint8_t buff[64] = {};
    int pc = MPAGE_CONTROL_DEFAULT;

    if ((0 == scsiInquiryVpd(device, SCSI_VPD_BLOCK_DEVICE_CHARACTERISTICS,
                             buff, sizeof(buff))) &&
        ((sg_get_unaligned_be16(buff + 2)) > 2)) {
        int speed = sg_get_unaligned_be16(buff + 4);
        if (form_factorp)
            *form_factorp = buff[7] & 0xf;
        if (haw_zbcp)
            *haw_zbcp = (buff[8] >> 4) & 0x3;
        return speed;
    }
    if (form_factorp)
        *form_factorp = 0;
    if (haw_zbcp)
        *haw_zbcp = 0;
    if (modese_len <= 6) {
        if ((err = scsiModeSense(device, RIGID_DISK_DRIVE_GEOMETRY_PAGE, 0, pc,
                                 buff, sizeof(buff)))) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                modese_len = 10;
            else
                return -EINVAL;
        } else if (0 == modese_len)
            modese_len = 6;
    }
    if (10 == modese_len) {
        err = scsiModeSense10(device, RIGID_DISK_DRIVE_GEOMETRY_PAGE, 0, pc,
                              buff, sizeof(buff));
        if (err)
            return -EINVAL;
    }
    offset = scsiModePageOffset(buff, sizeof(buff), modese_len);
    return sg_get_unaligned_be16(buff + offset + 20);
}

/* Returns a non-zero value in case of error, wcep/rcdp == -1 - get value,
   0 - clear bit, 1 - set bit  */

int
scsiGetSetCache(scsi_device * device,  int modese_len, short int * wcep,
                short int * rcdp)
{
    int err, offset, resp_len, sp;
    uint8_t buff[64] = {};
    uint8_t ch_buff[64];
    short set_wce = *wcep;
    short set_rcd = *rcdp;

    if (modese_len <= 6) {
        err = scsiModeSense(device, CACHING_PAGE, 0, MPAGE_CONTROL_CURRENT,
                            buff, sizeof(buff));
        if (err) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                modese_len = 10;
            else {
                device->set_err(EINVAL, "SCSI MODE SENSE failed");
                return -EINVAL;
            }
        } else if (0 == modese_len)
            modese_len = 6;
    }

    if (10 == modese_len) {
        err = scsiModeSense10(device, CACHING_PAGE, 0, MPAGE_CONTROL_CURRENT,
                              buff, sizeof(buff));
        if (err) {
            device->set_err(EINVAL, "SCSI MODE SENSE failed");
            return -EINVAL;
        }
    }
    offset = scsiModePageOffset(buff, sizeof(buff), modese_len);
    if ((offset < 0) || (buff[offset + 1] < 0xa)) {
        device->set_err(EINVAL, "Bad response");
        return SIMPLE_ERR_BAD_RESP;
    }

    *wcep = ((buff[offset + 2] & 0x04) != 0);
    *rcdp = ((buff[offset + 2] & 0x01) != 0);

    if((*wcep == set_wce || set_wce == -1)
          && ((*rcdp == set_rcd) || set_rcd == -1))
      return 0; // no changes needed or nothing to set

    if (modese_len == 6)
        err = scsiModeSense(device, CACHING_PAGE, 0,
                            MPAGE_CONTROL_CHANGEABLE,
                            ch_buff, sizeof(ch_buff));
    else
        err = scsiModeSense10(device, CACHING_PAGE, 0,
                              MPAGE_CONTROL_CHANGEABLE,
                              ch_buff, sizeof(ch_buff));
    if (err) {
        device->set_err(EINVAL, "WCE/RCD bits not changeable");
        return err;
    }

    // set WCE bit
    if(set_wce >= 0 && *wcep != set_wce) {
       if (0 == (ch_buff[offset + 2] & 0x04)) {
         device->set_err(EINVAL, "WCE bit not changeable");
         return 1;
       }
       if(set_wce)
          buff[offset + 2] |= 0x04; // set bit
       else
          buff[offset + 2] &= 0xfb; // clear bit
    }
    // set RCD bit
    if(set_rcd >= 0 && *rcdp != set_rcd) {
       if (0 == (ch_buff[offset + 2] & 0x01)) {
         device->set_err(EINVAL, "RCD bit not changeable");
         return 1;
       }
       if(set_rcd)
          buff[offset + 2] |= 0x01; // set bit
       else
          buff[offset + 2] &= 0xfe; // clear bit
    }

    /* mask out DPOFUA device specific (disk) parameter bit */
    if (10 == modese_len) {
        resp_len = sg_get_unaligned_be16(buff + 0) + 2;
        buff[3] &= 0xef;
    } else {
        resp_len = buff[0] + 1;
        buff[2] &= 0xef;
    }
    sp = 0; /* Do not change saved values */
    if (10 == modese_len)
        err = scsiModeSelect10(device, sp, buff, resp_len);
    else if (6 == modese_len)
        err = scsiModeSelect(device, sp, buff, resp_len);
    if(err)
      device->set_err(EINVAL, "MODE SELECT command failed");
    return err;
}


/* Attempts to set or clear GLTSD bit in Control mode page. If enabled is
   0 attempts to clear GLTSD otherwise it attempts to set it. Returns 0 if
   successful, negative if low level error, > 0 if higher level error (e.g.
   SIMPLE_ERR_BAD_PARAM if GLTSD bit is not changeable). */
int
scsiSetControlGLTSD(scsi_device * device, int enabled, int modese_len)
{
    int err, offset, resp_len, sp;
    uint8_t buff[64] = {};
    uint8_t ch_buff[64];

    if (modese_len <= 6) {
        if ((err = scsiModeSense(device, CONTROL_MODE_PAGE, 0,
                                 MPAGE_CONTROL_CURRENT,
                                 buff, sizeof(buff)))) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                modese_len = 10;
            else
                return err;
        } else if (0 == modese_len)
            modese_len = 6;
    }
    if (10 == modese_len) {
        err = scsiModeSense10(device, CONTROL_MODE_PAGE, 0,
                              MPAGE_CONTROL_CURRENT,
                              buff, sizeof(buff));
        if (err)
            return err;
    }
    offset = scsiModePageOffset(buff, sizeof(buff), modese_len);
    if ((offset < 0) || (buff[offset + 1] < 0xa))
        return SIMPLE_ERR_BAD_RESP;

    if (enabled)
        enabled = 2;
    if (enabled == (buff[offset + 2] & 2))
        return 0;       /* GLTSD already in wanted state so nothing to do */

    if (modese_len == 6)
        err = scsiModeSense(device, CONTROL_MODE_PAGE, 0,
                            MPAGE_CONTROL_CHANGEABLE,
                            ch_buff, sizeof(ch_buff));
    else
        err = scsiModeSense10(device, CONTROL_MODE_PAGE, 0,
                              MPAGE_CONTROL_CHANGEABLE,
                              ch_buff, sizeof(ch_buff));
    if (err)
        return err;
    if (0 == (ch_buff[offset + 2] & 2))
        return SIMPLE_ERR_BAD_PARAM;  /* GLTSD bit not changeable */

    /* mask out DPOFUA device specific (disk) parameter bit */
    if (10 == modese_len) {
        resp_len = sg_get_unaligned_be16(buff + 0) + 2;
        buff[3] &= 0xef;
    } else {
        resp_len = buff[0] + 1;
        buff[2] &= 0xef;
    }
    sp = (buff[offset] & 0x80) ? 1 : 0; /* PS bit becomes 'SELECT's SP bit */
    if (enabled)
        buff[offset + 2] |= 0x2;    /* set GLTSD bit */
    else
        buff[offset + 2] &= 0xfd;   /* clear GLTSD bit */
    if (10 == modese_len)
        err = scsiModeSelect10(device, sp, buff, resp_len);
    else if (6 == modese_len)
        err = scsiModeSelect(device, sp, buff, resp_len);
    return err;
}

/* Returns a negative value if failed to fetch Protocol specific port mode
   page or it was malformed. Returns transport protocol identifier when
   value >= 0 . */
int
scsiFetchTransportProtocol(scsi_device * device, int modese_len)
{
    int err, offset;
    uint8_t buff[64] {};

    if (modese_len <= 6) {
        if ((err = scsiModeSense(device, PROTOCOL_SPECIFIC_PORT_PAGE, 0,
                                 MPAGE_CONTROL_CURRENT,
                                 buff, sizeof(buff)))) {
            if (SIMPLE_ERR_BAD_OPCODE == err)
                modese_len = 10;
            else
                return -EINVAL;
        } else if (0 == modese_len)
            modese_len = 6;
    }
    if (10 == modese_len) {
        err = scsiModeSense10(device, PROTOCOL_SPECIFIC_PORT_PAGE, 0,
                              MPAGE_CONTROL_CURRENT,
                              buff, sizeof(buff));
        if (err)
            return -EINVAL;
    }
    offset = scsiModePageOffset(buff, sizeof(buff), modese_len);
    if ((offset >= 0) && (buff[offset + 1] > 1)) {
        if ((0 == (buff[offset] & 0x40)) &&       /* SPF==0 */
            (PROTOCOL_SPECIFIC_PORT_PAGE == (buff[offset] & 0x3f)))
                return (buff[offset + 2] & 0xf);
    }
    return -EINVAL;
}

const unsigned char *
sg_scsi_sense_desc_find(const unsigned char * sensep, int sense_len,
                        int desc_type)
{
    int add_sen_len;
    const unsigned char * descp;

    if ((sense_len < 8) || (0 == (add_sen_len = sensep[7])))
        return nullptr;
    if ((sensep[0] < 0x72) || (sensep[0] > 0x73))
        return nullptr;
    add_sen_len = (add_sen_len < (sense_len - 8)) ?
                         add_sen_len : (sense_len - 8);
    descp = &sensep[8];
    for (int desc_len = 0, k = 0; k < add_sen_len; k += desc_len) {
        descp += desc_len;
        int add_len = (k < (add_sen_len - 1)) ? descp[1]: -1;
        desc_len = add_len + 2;
        if (descp[0] == desc_type)
            return descp;
        if (add_len < 0) /* short descriptor ?? */
            break;
    }
    return nullptr;
}

// Convenience function for formatting strings from SCSI identify
void
scsi_format_id_string(char * out, const uint8_t * in, int n)
{
  char tmp[65];
  n = n > 64 ? 64 : n;
  strncpy(tmp, (const char *)in, n);
  tmp[n] = '\0';

  // Find the first non-space character (maybe none).
  int first = -1;
  int i;
  for (i = 0; tmp[i]; i++)
    if (!isspace((int)tmp[i])) {
      first = i;
      break;
    }

  if (first == -1) {
    // There are only space characters.
    out[0] = '\0';
    return;
  }

  // Find the last non-space character.
  for (i = strlen(tmp)-1; i >= first && isspace((int)tmp[i]); i--);
  int last = i;

  strncpy(out, tmp+first, last-first+1);
  out[last-first+1] = '\0';
}

static const char * wn = "Warning";

static const char * wn1_9[] = {
    "specified temperature exceeded",
    "enclosure degraded",
    "background self-test failed",
    "background pre-scan detected medium error",
    "background medium scan detected medium error",
    "non-volatile cache now volatile",
    "degraded power to non-volatile cache",
    "power loss expected",
    "device statistics notification active",
};

static const char * five_d_t[] = {
    "Hardware",
    "Controller",
    "Data channel",
    "Servo",
    "Spindle",
    "Firmware",
};

static const char * impfail = "impending failure";

static const char * impending0_c[] = {
    "general hard drive failure",
    "drive error rate too high",
    "data error rate too high",
    "seek error rate too high",
    "too many block reassigns",
    "access times too high",
    "start unit times too high",
    "channel parametrics",
    "controller detected",
    "throughput performance",
    "seek time performance",
    "spin-up retry count",
    "drive calibration retry count",
};

static const char * pred = "prediction threshold exceeded";

/* The SCSI Informational Exceptions log page and various other mechanisms
 * yield an additional sense code (and its qualifier) [asc and ascq] when
 * triggered. It seems only two asc values are involved: 0xb and 0xd.
 * If asc,ascq strings are known (in spc6r06.pdf) for asc 0xb and 0x5d
 * then a pointer to that string is returned, else nullptr is returned. The
 * caller provides a buffer (b) and its length (blen) that a string (if
 * found) is placed in. So if a match is found b is returned. */
char *
scsiGetIEString(uint8_t asc, uint8_t ascq, char * b, int blen)
{
    if (asc == 0xb) {
        switch (ascq) {
        case 0:
            snprintf(b, blen, "%s", wn);
            return b;
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        case 0x8:
        case 0x9:
            snprintf(b, blen, "%s - %s", wn, wn1_9[ascq - 1]);
            return b;
        case 0x12:
            snprintf(b, blen, "%s - microcode security at risk", wn);
            return b;
        case 0x13:
            snprintf(b, blen, "%s - microcode digital signature validation "
                     "failure", wn);
            return b;
        case 0x14:
            snprintf(b, blen, "%s - physical element status change", wn);
            return b;
        default:
            if ((ascq >= 0xa) && (ascq <= 0x11)) {
                uint8_t q = ascq - 0xa;

                snprintf(b, blen, "%s - %s %s %s limit exceeded", wn,
                         (((q % 2) == 0) ? "high" : "low"),
                         ((((q / 2) % 2) == 0) ? "critical" : "operating"),
                         ((((q / 4) % 2) == 0) ? "temperature" : "humidity"));
                return b;
            } else
                return nullptr;
        }
    } else if (asc == 0x5d) {
        switch (ascq) {
        case 0:
            snprintf(b, blen, "Failure %s", pred);
            return b;
        case 1:
            snprintf(b, blen, "Media failure %s", pred);
            return b;
        case 2:
            snprintf(b, blen, "Logical unit failure %s", pred);
            return b;
        case 3:
            snprintf(b, blen, "spare area exhaustion failure %s", pred);
            return b;
        case 0x1d:
            snprintf(b, blen, "%s %s power loss protection circuit area "
                     "exhaustion failure", five_d_t[0], impfail);
            return b;
        case 0x73:
            snprintf(b, blen, "Media %s endurance limit met", impfail);
            return b;
        case 0xff:
            snprintf(b, blen, "Failure %s (false)", pred);
            return b;
        default:
            if ((ascq >= 0x10) && (ascq <= 0x6c)) {
                uint8_t q = ascq - 0x10;
                uint8_t rem = q % 0x10;

                if (rem <= 0xc) {
                    snprintf(b, blen, "%s %s %s", five_d_t[q / 0x10], impfail,
                             impending0_c[rem]);
                    return b;
                } else
                    return nullptr;
            } else
                return nullptr;
        }
    } else
        return nullptr;
}
