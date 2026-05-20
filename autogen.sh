#!/bin/sh
#
# autogen.sh
#
# Home page of code is: https://www.smartmontools.org
#
# Copyright (C) 2003    Guido Guenther
# Copyright (C) 2003-08 Bruce Allen
# Copyright (C) 2004-26 Christian Franke
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

# Generate ./configure from configure.ac and Makefile.in from Makefile.am.
# This also adds files like missing,depcomp,install-sh to the source
# directory.

# Missing double quotes around $warnings and $v are intentional
# shellcheck disable=SC2086

set -e

myname=$0

clean=false; force=false; v=; warnings=
while [ $# -gt 0 ]; do case $1 in
  -v) v=-v ;;
  --clean) clean=true ;;
  --force) force=true ;;
  --warnings=?*) warnings="${warnings}${warnings:+ }$1" ;;
  *) echo "Usage: [AUTOMAKE=...] [ACLOCAL=...] [LIBTOOLIZE=...] \\"
     echo "       $0 [-v] [--force] [--warnings=CATEGORY ...]";
     echo "       $0 [-v] [--clean]"; exit 1 ;;
esac; shift; done

error()
{
  echo "$myname: $*" >&2
  exit 1
}

test "${myname%/*}" = "." || error "Not run from \${srcdir}"

if $clean || $force; then
  rm -f $v \
    aclocal.m4 compile configure configure~ config.guess config.h.in \
    config.h.in~ config.sub depcomp install-sh ltmain.sh missing \
    m4/libtool.m4 m4/libtool.m4~ m4/lt*.m4 m4/lt*.m4~ m4/pkg.m4 \
    Makefile.in include/Makefile.in lib/Makefile.in src/Makefile.in
  rm -f -r $v autom4te.cache
  test ! -d m4 || rmdir $v m4 || exit 1
  $force || exit 0
fi

# Check for CR/LF line endings
if od -A n -t x1 src/smartctl.h | grep ' 0d' >/dev/null; then
  echo "Warning: Checkout with CR/LF line endings, 'make dist' and related targets will not work."
fi

# Find automake
if [ -n "$AUTOMAKE" ]; then
  ver=$("$AUTOMAKE" --version) || exit 1
else
  maxver=
  for i in 1.18 1.17 1.16 1.15 1.14 1.13; do
    minver=$i; test -n "$maxver" || maxver=$i
    ver=$(automake-$i --version 2>/dev/null) || continue
    AUTOMAKE="automake-$i"
    break
  done
  test -n "$AUTOMAKE" || error "GNU Automake $minver (up to $maxver) is required"
fi

ver=$(echo "$ver" | sed -n '1s,^.*[^.0-9]\([12]\.[0-9][-.0-9pl]*\).*$,\1,p')
test -n "$ver" || error "$AUTOMAKE: Unable to determine automake version"

# Check aclocal
if [ -z "$ACLOCAL" ]; then
  ACLOCAL="aclocal$(echo "$AUTOMAKE" | sed -n 's,^.*automake\(-[.0-9]*\),\1,p')"
fi
"$ACLOCAL" --version >/dev/null || exit 1

# Check for [g]libtoolize
if [ -z "$LIBTOOLIZE" ] && glibtoolize --version >/dev/null 2>&1; then
  LIBTOOLIZE=glibtoolize
else
  test -n "$LIBTOOLIZE" || LIBTOOLIZE=libtoolize
  "$LIBTOOLIZE" --version >/dev/null || exit 1
fi

# Warn if Automake version was not tested
case "$ver" in
  1.[0-9]|1.[0-9].*|1.1[0-2]|1.1[0-2].*)
    echo "GNU Automake $ver is not supported."; exit 1
    ;;

  1.13.[34]|1.14|1.14.1|1.15|1.15.1|1.16|1.16.[1-5]|1.17|1.18|1.18.1)
    # OK
    ;;

  *)
    echo "Note: GNU Automake version ${ver} was not tested by the developers."
    echo "Please report success/failure to the smartmontools-support mailing list."
esac

vrun()
{
  test -z "$v" || echo "$*"
  "$@"
}

# Same order as 'autoreconf --install'
vrun "$ACLOCAL" --install $warnings
vrun "$LIBTOOLIZE" --copy $warnings
vrun autoconf $warnings
vrun autoheader $warnings
vrun "$AUTOMAKE" --add-missing --copy $warnings
