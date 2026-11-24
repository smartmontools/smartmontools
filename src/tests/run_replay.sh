#!/bin/sh
#
# run_replay.sh - replay recorded SCSI traces through smartctl and compare
#                 the output with the output of the real device
#
# Usage: run_replay.sh SMARTCTL DATADIR [DRIVEDB]
#
# For each DATADIR/NAME.TYPE.OPTS.expected runs
#   SMARTCTL [-B DRIVEDB] -d TYPE -OPT1 -OPT2 ... - < DATADIR/NAME.trace
# where OPTS is a '_' separated list of single letter options, e.g. "x" or "i_H_A".
# DRIVEDB replaces the drive database, so that an installed one (global or
# local entries) does not affect the result.
#
# Text output: version, copyright and local time lines are ignored, digit
# grouping is removed and decimal commas become points (the expected output
# may stem from a non-C locale).
# The estimated times of the SCT temperature history depend on the current
# time and are replaced by a placeholder. The drive database version is
# removed as it changes with each commit.
#
# JSON output: the option "j" selects the JSON output as single assignments
# ("--json=g") instead. All "json.smartctl.*" values except exit_status and
# messages (version, build, command line) and "json.local_time" are ignored.
# These expected files are not output of the real device but of the replay
# itself, they pin the JSON output. To regenerate one, run the command above
# and remove the ignored lines.
#
# Copyright (C) 2026 smartmontools contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

smartctl=$1
datadir=$2
drivedb=$3
if [ -z "$smartctl" ] || [ -z "$datadir" ]; then
  echo "Usage: $0 SMARTCTL DATADIR [DRIVEDB]" >&2
  exit 2
fi
set --
[ -n "$drivedb" ] && set -- -B "$drivedb"

# POSIX sed only (no GNU extensions): a trailing blank marks the end of line
normalize()
{
  tr -d '\r' |
  sed -e '/^smartctl [^ ]* [0-9-]* .*\[.*\]/d' \
      -e '/^Copyright (C)/d' \
      -e '/^Local Time is:/d' \
      -e 's/^\(Device is: *In smartctl database\).*/\1/' \
      -e 's/^\(Device is: *Not in smartctl database\).*/\1/' \
      -e 's/^\( *[0-9][0-9]*  *\)[0-9]\{4\}-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9] /\1YYYY-MM-DD hh:mm /' \
      -e 's/$/ /' \
      -e ':a' -e 's/\([0-9]\)[.,]\([0-9][0-9][0-9][^0-9]\)/\1\2/' -e 'ta' \
      -e 's/\([0-9]\),\([0-9]\{1,2\}[^0-9]\)/\1.\2/g' \
      -e 's/ $//'
}

normalize_json()
{
  tr -d '\r' |
  sed -e '/^json\.smartctl\.exit_status /b' \
      -e '/^json\.smartctl\.messages/b' \
      -e '/^json\.smartctl\./d' \
      -e '/^json\.local_time/d'
}

# mktemp is not POSIX, mkdir also fails if the directory exists
tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/run_replay.XXXXXX" 2>/dev/null) ||
  { tmpdir=${TMPDIR:-/tmp}/run_replay.$$; mkdir -m 700 "$tmpdir" || exit 2; }
trap 'rm -rf "$tmpdir"' 0
tmp=$tmpdir/r
fails=0
total=0
for exp in "$datadir"/*.expected; do
  [ -f "$exp" ] || continue
  base=${exp%.expected}            # .../NAME.TYPE.OPTS
  opts=${base##*.}; base=${base%.*}
  type=${base##*.}; name=${base%.*}
  args=$(echo "$opts" | sed -e 's/^/-/' -e 's/_/ -/g')
  json=
  case " $args " in
    *" -j "*) json=1
              args=$(echo " $args " | sed -e 's/ -j / --json=g /' -e 's/^ //' -e 's/ $//') ;;
  esac
  total=$((total + 1))
  # shellcheck disable=SC2086
  "$smartctl" "$@" -d "$type" $args - < "$name.trace" > "$tmp.out" 2>&1
  rc=$?
  if [ -n "$json" ]; then
    normalize_json < "$tmp.out" > "$tmp.act"
    normalize_json < "$exp" > "$tmp.exp"
  else
    normalize < "$tmp.out" > "$tmp.act"
    normalize < "$exp" > "$tmp.exp"
  fi
  # Exit status bit 0 (command line error) must not be set, the other bits
  # depend on the recorded device (e.g. device open failed)
  if [ $((rc & 1)) -ne 0 ]; then
    echo "FAIL: ${name##*/} (-d $type $args): exit status $rc"
    fails=$((fails + 1))
  elif diff -u "$tmp.exp" "$tmp.act" > "$tmp.diff"; then
    echo "PASS: ${name##*/} (-d $type $args)"
  else
    echo "FAIL: ${name##*/} (-d $type $args)"
    cat "$tmp.diff"
    fails=$((fails + 1))
  fi
done
# Debug output of a replay: the data length of the first INQUIRY (VPD page
# list, 252 bytes) must be printed as decimal number, not with a misleading
# "0x" prefix
trace=$datadir/sandisk-extreme-55dd-1tb.trace
if [ -f "$trace" ]; then
  total=$((total + 1))
  "$smartctl" "$@" -d scsi -i -r scsiioctl,3 - < "$trace" > "$tmp.dbg" 2>&1
  first=$(grep 'data length=' "$tmp.dbg" | head -n 1)
  case $first in
    *'data length=252,'*) echo "PASS: debug output data length (-r scsiioctl,3)" ;;
    *) echo "FAIL: debug output data length (-r scsiioctl,3): $first"
       fails=$((fails + 1)) ;;
  esac
fi

echo "$total replay tests, $fails failed"
[ "$total" -gt 0 ] && [ "$fails" -eq 0 ]
