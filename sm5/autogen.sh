#!/bin/sh
# $Id: autogen.sh,v 1.8 2004/05/27 15:23:16 card_captor Exp $
#
# Generate ./configure from config.in and Makefile.in from Makefile.am.
# This also adds files like missing,depcomp,install-sh to the source
# direcory. To update these files at a later date use:
#	autoreconf -f -i -v


# Cygwin?
test -x /usr/bin/uname && /usr/bin/uname | grep -i CYGWIN >/dev/null &&
{
    # Enable strict case checking
    # (to avoid e.g "DIST_COMMON = ... ChangeLog ..." in Makefile.in)
    export CYGWIN="${CYGWIN}${CYGWIN:+ }check_case:strict"

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

test -x "$AUTOMAKE" || AUTOMAKE=`typep automake-1.8` || AUTOMAKE=`typep automake-1.7` || AUTOMAKE=`typep automake17` ||
{
echo
echo "You must have at least GNU Automake 1.7 (up to 1.8.x) installed"
echo "in order to bootstrap smartmontools from CVS. Download the"
echo "appropriate package for your distribution, or the source tarball"
echo "from ftp://ftp.gnu.org/gnu/automake/ ."
echo
echo "Also note that support for new Automake series (anything newer"
echo "than 1.8.x) is only added after extensive tests. If you live in"
echo "the bleeding edge, you should know what you're doing, mainly how"
echo "to test it before the developers. Be patient."
exit 1;
}

test -x "$ACLOCAL" || ACLOCAL="aclocal`echo "$AUTOMAKE" | sed 's/.*automake//'`" && ACLOCAL=`typep "$ACLOCAL"` ||
{
echo
echo "autogen.sh found automake-1.7, or automake-1.8 in"
echo "your PATH, but not the respective aclocal-1.7, or"
echo "aclocal-1.8. Your installation of GNU Automake is broken or"
echo "incomplete."
exit 2;
}

set -e	# stops on error status

${ACLOCAL}
autoheader
${AUTOMAKE} --add-missing --copy --foreign
autoconf
