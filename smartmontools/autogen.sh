#!/bin/sh
# $Id$
#
# Generate ./configure from configure.ac and Makefile.in from Makefile.am.
# This also adds files like missing,depcomp,install-sh to the source
# directory. To update these files at a later date use:
#	autoreconf -f -i -v

force=; warnings=
while [ $# -gt 0 ]; do case $1 in
  --force) force=$1; shift ;;
  --warnings=?*) warnings="${warnings} $1"; shift ;;
  *) echo "Usage: $0 [--force] [--warnings=CATEGORY ...]"; exit 1 ;;
esac; done

# Cygwin?
test -x /usr/bin/uname && /usr/bin/uname | grep -i CYGWIN >/dev/null &&
{
    # Check for Unix text file type
    echo > dostest.tmp
    test "`wc -c < dostest.tmp`" -eq 1 ||
        echo "Warning: DOS text file type set, 'make dist' and related targets will not work."
    rm -f dostest.tmp
}

# Find automake
if [ -n "$AUTOMAKE" ]; then
  ver=$("$AUTOMAKE" --version) || exit 1
else
  maxver=
  for v in 1.15 1.14 1.13 1.12 1.11 1.10; do
    minver=$v; test -n "$maxver" || maxver=$v
    ver=$(automake-$v --version 2>/dev/null) || continue
    AUTOMAKE="automake-$v"
    break
  done
  if [ -z "$AUTOMAKE" ]; then
    echo "GNU Automake $minver (up to $maxver) is required to bootstrap smartmontools from SVN."
    exit 1;
  fi
fi

ver=$(echo "$ver" | sed -n '1s,^.*[^.0-9]\([12]\.[0-9][-.0-9pl]*\).*$,\1,p')
if [ -z "$ver" ]; then
  echo "$AUTOMAKE: Unable to determine automake version."
  exit 1
fi

# Check aclocal
if [ -z "$ACLOCAL" ]; then
  ACLOCAL="aclocal$(echo "$AUTOMAKE" | sed -n 's,^.*automake\(-[.0-9]*\),\1,p')"
fi

"$ACLOCAL" --version >/dev/null || exit 1

# Warn if Automake version was not tested
amwarnings=$warnings
case "$ver" in
  1.10|1.10.[123]|1.11|1.11.[1-6]|1.12.[2-6]|1.13.[34])
    # OK
    ;;

  1.14|1.14.1|1.15)
    # TODO: Enable 'subdir-objects' in configure.ac
    # For now, suppress 'subdir-objects' forward-incompatibility warning
    test -n "$warnings" || amwarnings="--warnings=no-unsupported"
    ;;

  *)
    echo "Note: GNU Automake version ${ver} was not tested by the developers."
    echo "Please report success/failure to the smartmontools-support mailing list."
esac

# required for aclocal-1.10 --install
test -d m4 || mkdir m4 || exit 1

set -e	# stops on error status

test -z "$warnings" || set -x

${ACLOCAL} -I m4 --install $force $warnings
autoheader $force $warnings
${AUTOMAKE} --add-missing --copy ${force:+--force-missing} $amwarnings
autoconf $force $warnings
