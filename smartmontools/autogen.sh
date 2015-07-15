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

typep()
{
    cmd=$1 ; TMP=$IFS ; IFS=: ; set $PATH
    for dir
    do
	if [ -x "$dir/$cmd" ]; then
	    echo "$dir/$cmd"
	    IFS=$TMP
	    return 0
        fi
    done
    IFS=$TMP
    return 1
}

test -x "$AUTOMAKE" ||
    AUTOMAKE=`typep automake-1.15` || AUTOMAKE=`typep automake-1.14` ||
    AUTOMAKE=`typep automake-1.13` || AUTOMAKE=`typep automake-1.12` ||
    AUTOMAKE=`typep automake-1.11` || AUTOMAKE=`typep automake-1.10` ||
{
echo
echo "You must have at least GNU Automake 1.10 (up to 1.15) installed"
echo "in order to bootstrap smartmontools from SVN. Download the"
echo "appropriate package for your distribution, or the source tarball"
echo "from ftp://ftp.gnu.org/gnu/automake/ ."
echo
echo "Also note that support for new Automake series (anything newer"
echo "than 1.15) is only added after extensive tests. If you live in"
echo "the bleeding edge, you should know what you're doing, mainly how"
echo "to test it before the developers. Be patient."
exit 1;
}

test -x "$ACLOCAL" || ACLOCAL="aclocal`echo "$AUTOMAKE" | sed 's/.*automake//'`" && ACLOCAL=`typep "$ACLOCAL"` ||
{
echo
echo "autogen.sh found automake-1.X, but not the respective aclocal-1.X."
echo "Your installation of GNU Automake is broken or incomplete."
exit 2;
}

ver="`$AUTOMAKE --version | sed -n '1s,^.*[^.0-9]\([12]\.[0-9][-.0-9pl]*\).*$,\1,p'`"
ver="${ver:-?.?.?}"

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
