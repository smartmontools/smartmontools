#!/bin/sh
#
# autogen.sh
#
# Generate ./configure from configure.ac and Makefile.in from Makefile.am.
# This also adds files like missing,depcomp,install-sh to the source
# directory. To update these files at a later date use:
#	autoreconf -f -i -v

force=; warnings=
while [ $# -gt 0 ]; do case $1 in
  --force) force=$1; shift ;;
  --warnings=?*) warnings="${warnings} $1"; shift ;;
  *) echo "Usage: [AUTOMAKE=...] [ACLOCAL=...] [LIBTOOLIZE=...] \\"
     echo "       $0 [--force] [--warnings=CATEGORY ...]"; exit 1 ;;
esac; done

# Check for CR/LF line endings
if od -A n -t x1 src/smartctl.h | grep ' 0d' >/dev/null; then
  echo "Warning: Checkout with CR/LF line endings, 'make dist' and related targets will not work."
fi

# Find automake
if [ -n "$AUTOMAKE" ]; then
  ver=$("$AUTOMAKE" --version) || exit 1
else
  maxver=
  for v in 1.18 1.17 1.16 1.15 1.14 1.13; do
    minver=$v; test -n "$maxver" || maxver=$v
    ver=$(automake-$v --version 2>/dev/null) || continue
    AUTOMAKE="automake-$v"
    break
  done
  if [ -z "$AUTOMAKE" ]; then
    echo "GNU Automake $minver (up to $maxver) is required"
    exit 1
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

set -e	# stops on error status

test -z "$warnings" || set -x

# Same order as 'autoreconf --install'
"$ACLOCAL" --install $force $warnings
"$LIBTOOLIZE" --copy $force $warnings
autoconf $force $warnings
autoheader $force $warnings
"$AUTOMAKE" --add-missing --copy ${force:+--force-missing} $warnings
