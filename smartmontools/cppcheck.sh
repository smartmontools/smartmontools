#!/bin/sh
#
# cppcheck.sh - run cppcheck on smartmontools $srcdir
#
# Home page of code is: https://www.smartmontools.org
#
# Copyright (C) 2019-20 Christian Franke
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# $Id$
#

set -e

myname=$0

usage()
{
  echo "Usage: $myname [-v|-q] [-jJOBS] [--library=CFG] [--platform=TYPE] [FILE ...]"
  exit 1
}

# Parse options
jobs=
v=
library="--library=posix"
platform="--platform=unix64"
unused_func=",unusedFunction"

while true; do case $1 in
  -j?*) jobs=$1; unused_func= ;;
  -q) v="-q" ;;
  -v) v="-v" ;;
  --platform=*) platform=$1 ;;
  --library=*) library=$1 ;;
  -*) usage ;;
  *) break ;;
esac; shift; done

# Set file list from command line or $srcdir
if [ $# -ne 0 ]; then
  files=$(echo "$@")
  files_v=$files
  unused_func=
else
  srcdir=${myname%/*}
  if [ "$srcdir" = "$myname" ]; then
    echo "$myname: \$srcdir not found" >&2
    exit 1
  fi
  cd "$srcdir" || exit 1
  files_v="*.cpp *.h os_win32/*.cpp os_win32/*.h"
  files=$(echo $files_v)
  case $files in
    *\**) echo "$myname: Not run from \$srcdir" >&2; exit 1 ;;
  esac
fi

# Check cppcheck version
ver=$(cppcheck --version) || exit 1
ver=${ver##* }
case $ver in
  1.85) ;;
  *) echo "$myname: cppcheck $ver not tested with this script" ;;
esac

# Build cppcheck settings
enable="warning,style,performance,portability,information${unused_func}"

sup_list="
  #warning
  syntaxError:drivedb.h
  #style
  asctime_rCalled:utility.cpp
  asctime_sCalled:utility.cpp
  bzeroCalled
  bcopyCalled
  ftimeCalled
  readdirCalled
  strtokCalled
  missingOverride
  unusedStructMember
  unusedFunction:sg_unaligned.h
  unmatchedSuppression
"

suppress=
for s in $sup_list; do
  case $s in
    \#*) continue ;;
    unusedFunction:*) test -n "$unused_func" || continue ;;
    unmatchedSuppression) test $# -ne 0 || continue ;;
  esac
  suppress="${suppress}${suppress:+ }--suppress=${s%%#*}"
done

defs="\
  -U__KERNEL__
  -U__LP64__
  -U__VERSION__
  -U_NETWARE
  -DBUILD_INFO=\"(...)\"
  -UCLOCK_MONOTONIC
  -DENOTSUP=1
  -DHAVE_ATTR_PACKED
  -DHAVE_CONFIG_H
  -DSG_IO=1
  -DSMARTMONTOOLS_SVN_REV=\"r1\"
  -DSMARTMONTOOLS_ATTRIBUTELOG=\"/file\"
  -DSMARTMONTOOLS_SAVESTATES=\"/file\"
  -DSMARTMONTOOLS_DRIVEDBDIR=\"/dir\"
  -Umakedev
  -Ustricmp"

# Print brief version of command
cat <<EOF
cppcheck-$ver \\
  --enable=$enable \\
  $library \\
  $platform \\
  ... \\
$(echo "$defs" | sed 's,$, \\,')
$(for s in $suppress; do echo "  $s \\"; done)
  $files_v
EOF

# Run cppcheck with swapped stdout<>stderr
cppcheck \
  $v \
  $jobs \
  --enable="$enable" \
  --template='{file}:{line}: {severity}: ({id}) {message}' \
  --force \
  --inline-suppr \
  --language=c++ \
  $library \
  $platform \
  $defs \
  $suppress \
  $files \
  3>&2 2>&1 1>&3 3>&-
