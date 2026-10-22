#!/usr/bin/env python3
#
# scsiioctl2trace.py
#
# Convert "smartctl -r scsiioctl,4 ..." debug output into the SCSI trace
# format replayed by "smartctl -d scsi ... -" (see lib/dev_scsi_trace.cpp).
#
# Copyright (C) 2026 smartmontools contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

import re
import sys

CDB_RE = re.compile(r'^ \[([^:\]]+): ((?:[0-9a-f]{2} )+)\]$')
# Printed before the CDB; "data length=0x%u" is actually decimal
DESC_RE = re.compile(r'^ \[[^:\]]+: SCSI opcode=0x[0-9a-f]+, CDB length=\d+, '
                     r'data length=0x(\d+), data dir=(\w+)\]$')
SENSE_RE = re.compile(r'^  >>> Sense buffer, len=(\d+):')
STATUS_RE = re.compile(r'^  status=([0-9a-f]+):')
DATAIN_RE = re.compile(r'^  \[data-in buffer: req_len=(\d+), resid=(-?\d+), gives (\d+) bytes\]')
INCOMING_RE = re.compile(r'^  Incoming data, len=(\d+), resid=(-?\d+)(.*):')
HEX_OFS_RE = re.compile(r'^ [0-9a-f]+ {3,}((?:[0-9a-f]{2} +)*[0-9a-f]{2})\s*$')
HEX_RAW_RE = re.compile(r'^((?:[0-9a-f]{2} +)*[0-9a-f]{2})\s*$')
IOCTL_ERR_RE = re.compile(r'IOCTL_SCSI_PASS_THROUGH\w* failed, Error=(\d+)')


def read_hex(lines, i, count, with_offset):
    out = []
    rx = HEX_OFS_RE if with_offset else HEX_RAW_RE
    while i < len(lines) and len(out) < count:
        m = rx.match(lines[i])
        if not m:
            break
        out += [int(b, 16) for b in m.group(1).split()]
        i += 1
    return out[:count], i


def convert(lines):
    cmds = []
    cur = None
    desc = None
    i = 0
    while i < len(lines):
        line = lines[i].rstrip('\r\n')
        m = DESC_RE.match(line)
        if m:
            desc = (int(m.group(1)), m.group(2))
            i += 1
            continue
        m = CDB_RE.match(line)
        if m:
            dlen, ddir = desc if desc else (0, 'none')
            desc = None
            cur = {'name': m.group(1), 'cdb': m.group(2).split(), 'alloc': dlen,
                   'dir': ddir, 'status': 0, 'error': None, 'sense': [],
                   'data': None, 'truncated': None}
            cmds.append(cur)
            i += 1
            continue
        if cur is None:
            i += 1
            continue
        m = SENSE_RE.match(line)
        if m:
            cur['sense'], i = read_hex([l.rstrip('\r\n') for l in lines], i + 1, int(m.group(1)), True)
            continue
        m = STATUS_RE.match(line)
        if m:
            cur['status'] = int(m.group(1), 16)
            i += 1
            continue
        m = DATAIN_RE.match(line)
        if m:
            cur['alloc'] = int(m.group(1))
            cur['data'], i = read_hex([l.rstrip('\r\n') for l in lines], i + 1, int(m.group(3)), False)
            continue
        m = INCOMING_RE.match(line)
        if m:
            cur['alloc'] = int(m.group(1))
            data, i = read_hex([l.rstrip('\r\n') for l in lines], i + 1, int(m.group(1)), True)
            if 'only first' in m.group(3):
                cur['truncated'] = data
            elif cur['data'] is None:
                cur['data'] = data
            continue
        m = IOCTL_ERR_RE.search(line)
        if m:
            cur['status'] = -1
            cur['error'] = int(m.group(1))
        i += 1
    for c in cmds:
        # Data-in command without any response: pass-through itself failed
        if (c['dir'] == 'in' and c['alloc'] > 0 and c['status'] == 0
                and c['data'] is None and c['truncated'] is None):
            c['status'] = -1
            c['error'] = 0
    return cmds


def emit(cmds, src, out):
    out.write('# converted from "smartctl -r scsiioctl,4" output: %s\n' % src)
    for c in cmds:
        if c['data'] is None and c['status'] == 0 and c['truncated'] is not None:
            # Incomplete response is worse than none: omit, a complete
            # recording of the same CDB may be appended to the trace.
            out.write('\n# %s: OMITTED, data truncated to 256 bytes in debug output\n'
                      '# CDB %s\n' % (c['name'], ' '.join(c['cdb'])))
            continue
        out.write('\n# %s\n' % c['name'])
        out.write('CDB %s\n' % ' '.join(c['cdb']))
        out.write('ALLOC %d\n' % c['alloc'])
        if c['status'] == -1:
            out.write('STATUS -1 ERROR %d\n' % c['error'])
        else:
            out.write('STATUS %d\n' % c['status'])
        out.write('SENSE %s\n' % ' '.join('%02x' % b for b in c['sense']))
        data = c['data']
        if c['status'] != 0:
            data = []
        out.write('DATA %s\n' % ' '.join('%02x' % b for b in (data or [])))
        out.write('END\n')


def main(argv):
    if len(argv) < 2:
        sys.stderr.write('Usage: %s SMARTCTL_DEBUG_OUTPUT... > FILE.trace\n' % argv[0])
        return 1
    for path in argv[1:]:
        with open(path, encoding='utf-8', errors='replace') as f:
            emit(convert(f.readlines()), path.rsplit('/', 1)[-1], sys.stdout)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
