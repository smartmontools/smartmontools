#!/bin/sh
#
# run_replay.sh - replay recorded SCSI traces through smartctl and compare
#                 the output with the output of the real device
#
# Usage: run_replay.sh SMARTCTL DATADIR
#
# For each DATADIR/NAME.TYPE.OPTS.expected runs
#   SMARTCTL -d TYPE -OPT1 -OPT2 ... - < DATADIR/NAME.trace
# where OPTS is a '_' separated list of single letter options, e.g. "x" or "i_H_A".
# Version, copyright and local time lines are ignored, and digit grouping and
# decimal commas are removed (the expected output may stem from a non-C locale).
#
# Copyright (C) 2026 smartmontools contributors
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

smartctl=$1
datadir=$2
if [ -z "$smartctl" ] || [ -z "$datadir" ]; then
  echo "Usage: $0 SMARTCTL DATADIR" >&2
  exit 2
fi

# POSIX sed only (no GNU extensions): a trailing blank marks the end of line
normalize()
{
  tr -d '\r' |
  sed -e '/^smartctl [^ ]* [0-9-]* .*\[.*\]/d' \
      -e '/^Copyright (C)/d' \
      -e '/^Local Time is:/d' \
      -e 's/$/ /' \
      -e ':a' -e 's/\([0-9]\)[.,]\([0-9][0-9][0-9][^0-9]\)/\1\2/' -e 'ta' \
      -e 's/\[\([0-9][0-9]*\),\([0-9][0-9]*\) \([KMGTP]*B\)\]/[\1.\2 \3]/g' \
      -e 's/ $//'
}

tmp=${TMPDIR:-/tmp}/run_replay.$$
trap 'rm -f "$tmp".*' 0
fails=0
total=0
for exp in "$datadir"/*.expected; do
  [ -f "$exp" ] || continue
  base=${exp%.expected}            # .../NAME.TYPE.OPTS
  opts=${base##*.}; base=${base%.*}
  type=${base##*.}; name=${base%.*}
  args=$(echo "$opts" | sed -e 's/^/-/' -e 's/_/ -/g')
  total=$((total + 1))
  # shellcheck disable=SC2086
  "$smartctl" -d "$type" $args - < "$name.trace" > "$tmp.out" 2>&1
  normalize < "$tmp.out" > "$tmp.act"
  normalize < "$exp" > "$tmp.exp"
  if diff -u "$tmp.exp" "$tmp.act" > "$tmp.diff"; then
    echo "PASS: ${name##*/} (-d $type $args)"
  else
    echo "FAIL: ${name##*/} (-d $type $args)"
    cat "$tmp.diff"
    fails=$((fails + 1))
  fi
done
echo "$total replay tests, $fails failed"
[ "$total" -gt 0 ] && [ "$fails" -eq 0 ]
