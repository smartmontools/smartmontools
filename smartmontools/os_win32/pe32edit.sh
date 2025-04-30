#! /bin/bash
#
# Display or edit PE32 file header fields
#
# Home page of code is: https://www.smartmontools.org
#
# Copyright (C) 2023 Christian Franke
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# $Id$
#

set -e -o pipefail

myname=$0

usage()
{
  cat <<EOF
Display or edit PE32 file header fields

Usage: $myname [-s SUBSYSTEM] [-t SECONDS_OR_FILE] [-v] FILE.exe

  -s SUBSYSTEM        Set Subsystem: 2 = GUI, 3 = Console
  -t SECONDS          Set header timestamp to SECONDS since epoch
  -t FILE             Set header timestamp to last modifcation time of FILE
  -v                  Verbose output
EOF
  exit 1
}

error()
{
  echo "$myname: $*" >&2
  exit 1
}

v=

vecho()
{
  test -z "$v" || echo "$*"
}

# is_num STRING
is_num()
{
  test -n "$1" || return 1
  test "${1//[0-9]/}" = "" || return 1
  return 0
}

# time2str SECONDS
time2str()
{
  # Requires GNU version of 'date'
  date -d "@$1" -u +'%Y-%m-%d %H:%M:%S UTC' 2>/dev/null || echo "?"
}

# Parse options
subsystem=; timefile=
while true; do case $1 in
  -s) shift; is_num "$1" || usage; subsystem=$1 ;;
  -t) shift; test -n "$1" || usage; timefile=$1 ;;
  -v) v=t ;;
  -*) usage ;;
  *)  break ;;
esac; shift; done

test $# = 1 || usage

file=$1
tempfile="$file.tmp"

if [ -n "$subsystem" ]; then case $((subsystem)) in
  [23]) ;;
  *) error "Subsystem must be 2 (GUI) or 3 (Console)" ;;
esac; fi

if [ -n "$timefile" ]; then
  if is_num "$timefile"; then
    timestamp=$timefile
  else
    # Requires GNU version of 'stat'
    timestamp=$(stat -c '%Y' "$timefile") || exit 1
    vecho "Using modification time of $timefile: $timestamp [$(time2str $timestamp)]"
  fi
  test $((timestamp & ~0xffffffff)) = 0 || error "Timestamp out of range: $timestamp"
fi

# Read first 512 bytes for quick access to header
x=$(od -A n -N 512 -t u1 -v -w1 "$file") || exit 1
mapfile header <<<"$x"
test "${#header[*]}" == 512 || error "$file: Invalid file size"

# get_uint16 OFFSET
get_uint16()
{
  test "$1" -lt 510 || error "$file: Invalid file offset: $2"
  echo $((${header[$1]} + (${header[$1+1]} << 8)))
}

# get_uint32 OFFSET
get_uint32()
{
  test "$1" -lt 508 || error "$file: Invalid file offset: $2"
  echo $((${header[$1]} + (${header[$1+1]} << 8) \
       + (${header[$1+2]} << 16)  + (${header[$1+3]} << 24)))
}

# IMAGE_DOS_HEADER.pe_magic == "MZ" ?
x=$(get_uint16 0)
test "$x" = $((0x5a4d)) || error "$file: MS-DOS 'MZ' header not present"
# pehdr_offs = IMAGE_DOS_HEADER.lfa_new
pehdr_offs=$(get_uint32 $((0x3c)))
test $((0x40)) -le "$pehdr_offs" && test "$pehdr_offs" -le $((0x180)) \
|| error "$file: Invalid PE header offset: $pehdr_offs"
# IMAGE_NT_HEADERS(32|64).Signature == "PE" ?
x=$(get_uint16 $pehdr_offs)
test "$x" = $((0x4550)) || error "$file: 'PE' header not found at $pehdr_offs"

timestamp_offs=$((pehdr_offs+0x08)) # IMAGE_NT_HEADERS(32|64).FileHeader.TimeDateStamp
checksum_offs=$((pehdr_offs+0x58))  # IMAGE_NT_HEADERS(32|64).OptionalHeader.CheckSum
subsystem_offs=$((pehdr_offs+0x5c)) # IMAGE_NT_HEADERS(32|64).OptionalHeader.Subsystem

machine=$(get_uint16 $((pehdr_offs+0x04))) # IMAGE_NT_HEADERS(32|64).FileHeader.Machine
magic=$(get_uint16 $((pehdr_offs+0x18)))   # IMAGE_NT_HEADERS(32|64).FileHeader.Magic

print_header()
{
  local txt val
  printf 'PE Header Offset:   0x%08x\n' $pehdr_offs

  case $machine in
    "$((0x014c))") txt="i386" ;;
    "$((0x8664))") txt="amd64" ;;
    *)             txt="*Unknown*" ;;
  esac
  printf 'Machine:                0x%04x [%s]\n' $machine "$txt"

  val=$(get_uint32 $timestamp_offs)
  printf 'TimeDateStamp:      0x%08x (%u) [%s]\n' $val $val "$(time2str $val)"

  val=$(get_uint16 $((pehdr_offs+0x16)))
  txt=
  test $((val & 0x0001)) = 0 || txt+=",relocs_stripped"
  test $((val & 0x0002)) = 0 || txt+=",executable"
  test $((val & 0x0004)) = 0 || txt+=",line_nums_stripped"
  test $((val & 0x0008)) = 0 || txt+=",local_syms_stripped"
  test $((val & 0x0020)) = 0 || txt+=",large_addr_aware"
  test $((val & 0x0100)) = 0 || txt+=",32bit"
  test $((val & 0x0200)) = 0 || txt+=",debug_stripped"
  test $((val & 0x2000)) = 0 || txt+=",dll"
  test $((val & 0xdcd0)) = 0 || txt+=",*other*"
  printf 'Characteristics:        0x%04x [%s]\n' $val "${txt#,}"

  case $magic in
    "$((0x010b))") txt="PE32" ;;
    "$((0x020b))") txt="PE32+" ;;
    *)             txt="*Unknown*" ;;
  esac
  printf 'Magic:                  0x%04x [%s]\n' $magic "$txt"

  printf 'CheckSum:           0x%08x\n' "$(get_uint32 $checksum_offs)"

  val=$(get_uint16 $subsystem_offs)
  case $val in
    2) txt="GUI" ;;
    3) txt="Console" ;;
    *) txt="*Unknown*" ;;
  esac
  printf 'Subsystem:              0x%04x [%s]\n' $val "$txt"

  val=$(get_uint16 $((pehdr_offs+0x5e)))
  txt=
  test $((val & 0x0020)) = 0 || txt+=",high_entropy_va"
  test $((val & 0x0040)) = 0 || txt+=",dynamic_base"
  test $((val & 0x0100)) = 0 || txt+=",nx_compat"
  test $((val & 0x4000)) = 0 || txt+=",cf_guard"
  test $((val & 0x8000)) = 0 || txt+=",ts_aware"
  test $((val & 0x3e9f)) = 0 || txt+=",*other*"
  printf 'DllCharacteristics:     0x%04x [%s]\n' $val "${txt#,}"
}

# Print header if no changes requested
if [ -z "$subsystem$timestamp" ]; then
  echo "$file:"
  print_header
  exit 0
fi

# Check file type
case "$machine:$magic:$(get_uint16 $subsystem_offs)" in
  $((0x014c)):$((0x010b)):[23]) ;; # i386:PE32:GUI/Console
  $((0x8664)):$((0x020b)):[23]) ;; # amd64:PE32+:GUI/Console
  *) error "$file: Unknown image type"
esac

# Prepare checksum update
checksum=$(get_uint32 $checksum_offs)
filesize=$(wc -c <"$file") || exit 1
changed=

# Create tempfile
cp -p "$file" "$tempfile" || exit 1

# put_uint16 OFFSET VALUE
put_uint16()
{
  local sum oldval b0 b1
  test $(($1 & 1)) = 0 || error "$file: Offset is not word aligned: $1"

  # Return if unchanged
  oldval=$(get_uint16 $1)
  test $2 != $oldval || return 0
  changed=t

  # Update checksum if valid
  if [ $checksum -gt $filesize ]; then
    sum=$((checksum - filesize))
    if [ $oldval -lt $sum ]; then
      sum=$((sum - oldval + $2))
    else
      sum=$((sum + (0xffff - oldval) + $2));
    fi
    sum=$(((sum + (sum >> 16)) & 0xffff));
    checksum=$((sum + filesize))
  fi

  # Patch the file
  b0=$(($2 & 0xff))
  b1=$((($2 >> 8) & 0xff))
  printf "$(printf '\\x%02x\\x%02x' $b0 $b1)" \
  | dd bs=2 seek=$(($1 >> 1)) count=1 conv=notrunc of="$tempfile" 2>/dev/null \
  || error "$file: Patch at offset $1 failed"

  # Update local copy
  header[$1]=$b0
  header[$1 + 1]=$b1
}

# put_uint32 OFFSET VALUE
put_uint32()
{
  put_uint16 $1 $(($2 & 0xffff))
  put_uint16 $(($1 + 2)) $((($2 >> 16) & 0xffff))
}

# Update requested header fields
test -z "$subsystem" || put_uint16 $subsystem_offs $subsystem
test -z "$timestamp" || put_uint32 $timestamp_offs $timestamp

if [ -n "$changed" ]; then
  put_uint32 $checksum_offs $checksum
  mv -f "$tempfile" "$file" || exit 1
  vecho "$file: Changed"
else
  rm -f "$tempfile"
  vecho "$file: Unchanged"
fi
test -z "$v" || print_header
