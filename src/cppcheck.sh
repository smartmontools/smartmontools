#!/bin/sh
#
# cppcheck.sh - run cppcheck on smartmontools $srcdir
#
# Home page of code is: https://www.smartmontools.org
#
# Copyright (C) 2019-26 Christian Franke
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

set -e

myname=$0

usage()
{
  echo "Usage: $myname [-v|-q] [-c CPPCHECK] [-jJOBS] [--library=CFG] [--platform=TYPE] [FILE ...]"
  exit 1
}

# Parse options
jobs=
v=
cppcheck="cppcheck"
library="--library=posix"
platform="--platform=unix64"
unused_func=",unusedFunction"

while true; do case $1 in
  -c) shift; test -n "$1" || usage; cppcheck=$1 ;;
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
  # shellcheck disable=SC2116
  files=$(echo "$@")
  files_v=$files
  unused_func=
else
  srcdir=${myname%/*}
  if [ "$srcdir" = "$myname" ]; then
    echo "$myname: \$srcdir not found" >&2
    exit 1
  fi
  srcdir="$srcdir/.."
  cd "$srcdir" || exit 1
  files_v="include/smartmon/*.h lib/*.cpp lib/*.h src/*.cpp src/*.h"
  files_v="$files_v src/os_win32/*.cpp src/os_win32/*.h"
  # shellcheck disable=SC2086
  if ! files=$(ls -d $files_v 2>/dev/null); then
    echo "$myname: Not run from \$srcdir" >&2
    exit 1
  fi
fi

# Check cppcheck version
ver=$("$cppcheck" --version) || exit 1
ver=${ver##* }
case $ver in
  [01].*|2.[0-9]|2.1[01]) # <= 2.11
    echo "$myname: cppcheck $ver is not supported" >&2; exit 1 ;;
  2.1[39].0) ;;
  *) echo "$myname: cppcheck $ver not tested with this script" ;;
esac

# Build cppcheck settings
enable="warning,style,performance,portability,information${unused_func}"

sup_list="
  #style
  asctime_rCalled:lib/utility.cpp
  asctime_sCalled:lib/utility.cpp
  cstyleCast:include/smartmon/sg_unaligned.h
  getgrgidCalled:src/popen_as_ugid.cpp
  getgrnamCalled:src/popen_as_ugid.cpp
  getpwnamCalled:src/popen_as_ugid.cpp
  getpwuidCalled:src/popen_as_ugid.cpp
  readdirCalled
  strtokCalled
  unusedStructMember
  unusedFunction:include/smartmon/sg_unaligned.h
  #information
  missingInclude
  missingIncludeSystem
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

# shellcheck disable=SC2089
defs="\
  -U__BYTE_ORDER__
  -U__GNUC__
  -U__KERNEL__
  -U__LITTLE_ENDIAN__
  -U__LP64__
  -U__MINGW64_VERSION_STR
  -U__VERSION__
  -U__clang__
  -U_MSVC_LANG
  -U_NETWARE
  -DBUILD_INFO=\"(...)\"
  -UCLOCK_MONOTONIC
  -DENOTSUP=1
  -DHAVE_ATTR_PACKED
  -UHAVE_BYTESWAP_H
  -DHAVE_CONFIG_H
  -DHAVE_UNISTD_H
  -UIGNORE_FAST_LEBE
  -DPACKAGE_BUGREPORT=\"email\"
  -DPACKAGE_URL=\"https://...\"
  -DPACKAGE_VERSION=\"8.0\"
  -DSMARTMON_HAVE_ATTR_PACKED
  -DSMARTMONTOOLS_BUILD_HOST=\"host\"
  -DSMARTMONTOOLS_ATTRIBUTELOG=\"/file\"
  -DSMARTMONTOOLS_SAVESTATES=\"/file\"
  -DSMARTMONTOOLS_SMARTDSCRIPTDIR=\"/dir\"
  -DSMARTMONTOOLS_DRIVEDBDIR=\"/dir\"
  -USMARTMONTOOLS_RELEASE_DATE
  -USMARTMONTOOLS_RELEASE_TIME
  -USMARTMONTOOLS_GIT_REV
  -USMARTMONTOOLS_GIT_REV_DATE
  -DSMARTMONTOOLS_GIT_VER_DESC=\"pre-8.0\"
  -DSOURCE_DATE_EPOCH=1767225600
  -Umakedev
  -Ustricmp"

# File for report of active checkers (cppcheck >= 2.12.0)
checkers_report=$(mktemp -t "cppcheck.sh.tmp.XXXXXXXXXX")

# Print brief version of command
cat <<EOF
# Cppcheck $ver
$cppcheck \\
  --enable=$enable \\
  --inconclusive \\
  $library \\
  $platform \\
  ... \\
$(echo "$defs" | sed 's,$, \\,')
$(for s in $suppress; do echo "  $s \\"; done)
  $files_v
EOF

rc=0

# Run cppcheck with swapped stdout<>stderr
# shellcheck disable=SC2086,SC2090
"$cppcheck" \
  $v \
  $jobs \
  --enable="$enable" \
  --inconclusive \
  --template='{file}:{line}: {severity}:{inconclusive:inconclusive:} ({id}) {message}' \
  --force \
  --inline-suppr \
  --language=c++ \
  --std=c++11 \
  --checkers-report=$checkers_report \
  -I "include" \
  $library \
  $platform \
  $defs \
  $suppress \
  $files \
  3>&2 2>&1 1>&3 3>&- || rc=$?

# Append report of active checkers if available
if [ -f "$checkers_report" ]; then
  cat - "$checkers_report" <<EOF

===============
Checkers Report
===============

EOF
  rm -f "$checkers_report"
fi

exit $rc
