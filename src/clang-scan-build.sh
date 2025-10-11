#!/bin/sh
#
# clang-scan-build.sh - run Clang++ scan-build for smartmontools build
#
# Home page of code is: https://www.smartmontools.org
#
# Copyright (C) 2025 Christian Franke
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

set -e
myname=$0

usage()
{
  echo "Usage: $myname [-f OUTPUT.tar.gz] MAKE [ARG...]"
  exit 1
}

error()
{
  echo "$myname: $*" >&2
  exit 1
}

tarfile=
case $1 in
  -f) shift; tarfile=$1; shift 2>/dev/null || usage ;;
esac
case $1 in
  ''|-*) usage ;;
esac

mf="Makefile"
sb="scan-build"
dir="clang-report"

# From Clang 21.1.1 'scan-build --help':
extra_checkers="
  nullability.NullableDereferenced  # Warns when a nullable pointer is dereferenced
  nullability.NullablePassedToNonnull # Warns when a nullable pointer is passed to a pointer which has a _Nonnull type
  nullability.NullableReturnedFromNonnull # Warns when a nullable pointer is returned from a function that has _Nonnull return type
  optin.core.EnumCastOutOfRange # Check integer to enumeration casts for out of range values
  optin.cplusplus.UninitializedObject # Reports uninitialized fields after object construction
  optin.cplusplus.VirtualCall # Check virtual function calls during construction/destruction
  #optin.performance.Padding # Check for excessively padded structs
  optin.portability.UnixAPI # Finds implementation-defined behavior in UNIX/Posix functions
  optin.taint.GenericTaint # Reports potential injection vulnerabilities
  optin.taint.TaintedAlloc # Check for memory allocations, where the size parameter might be a tainted value
  optin.taint.TaintedDiv # Check for divisions where the denominator is tainted and might be 0
  security.ArrayBound # Warn about out of bounds access to memory
  security.FloatLoopCounter # Warn on using a floating point value as a loop counter
  #security.MmapWriteExec # Warn on mmap() calls with both writable and executable access
  security.PointerSub # Check for pointer subtractions on two pointers pointing to different memory chunks
  security.PutenvStackArray # Finds calls to the function 'putenv' which pass a pointer to an automatic array
  security.SetgidSetuidOrder # Warn on possible reversed order of 'setgid(getgid()))' and 'setuid(getuid())'
  security.cert.env.InvalidPtr # Finds usages of possibly invalidated pointers
  security.insecureAPI.DeprecatedOrUnsafeBufferHandling # Warn on uses of unsecure or deprecated buffer manipulating functions
  security.insecureAPI.bcmp # Warn on uses of the 'bcmp' function
  security.insecureAPI.bcopy # Warn on uses of the 'bcopy' function
  security.insecureAPI.bzero # Warn on uses of the 'bzero' function
  #security.insecureAPI.rand # Warn on uses of the 'rand', 'random', and related functions
  security.insecureAPI.strcpy # Warn on uses of the 'strcpy' and 'strcat' functions
  valist.CopyToSelf #  Check for va_lists which are copied onto itself
  valist.Uninitialized # Check for usages of uninitialized (or already released) va_lists
  valist.Unterminated # Check for va_lists which are not released by a va_end call
"

# Get CC and CXX from Makefile
test -f "$mf" || error "$mf: missing"
cc=$(sed -n 's/^CC = \(.*\)$/\1/p' "$mf")
cxx=$(sed -n 's/^CXX = \(.*\)$/\1/p' "$mf")
test "${cc:+t}${cxx:+t}" = "tt" || error "$mf: missing CC/CXX"

# Get help text to test for supported checkers
sbhelp=$("$sb" --help 2>&1) || error "$sb --help: command failed"

# Create list of '-enable-checker' options
enable_checkers=$(
  # shellcheck disable=SC2034
  echo "$extra_checkers" | while read c r; do
    case $c in ''|\#*) continue ;; esac
    case "$(echo "$sbhelp" | grep "^[ +]*${c}\\b")" in
      '')   echo "$myname: $c: checker not supported" >&2 ;;
      \ +*) echo "$myname: $c: checker enabled by default" >&2 ;;
      *)    echo -enable-checker "$c" ;;
    esac
  done
)

# Run scan-build
rm -rf "$dir"
echo "$sb" --use-cc="$cc" --use-c++="$cxx" -o "$dir" ... "$@"
"$sb" --use-cc="$cc" --use-c++="$cxx" -o "$dir" \
  -analyzer-config "stable-report-filename=true" \
  $enable_checkers "$@"

# Print GHA compatible annotation
# Create tar file if requested
if ls "$dir"/*/index.html >/dev/null 2>&1; then
  n=$(sed -n 's/^.*>All Bugs<[^>]*><td[^>]*>\([0-9]*\)<.*$/\1/p' \
      "$dir"/*/index.html 2>/dev/null) ||:
  echo "::warning:: ${n:-Some} issue(s) found by Clang Static Analyzer"
  test -z "$tarfile" || tar -c -z -f "$tarfile" "$dir" || exit 1
else
  echo "::notice:: No issues found by Clang Static Analyzer"
  test -z "$tarfile" || rm -f "$tarfile" || exit 1
fi
