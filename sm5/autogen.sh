#!/bin/sh -e
# $Id: autogen.sh,v 1.3 2003/12/15 17:29:32 guidog Exp $
#
# Generate ./configure from config.in and Makefile.in from Makefile.am.
# This also adds files like missing,depcomp,install-sh to the source
# direcory. To update these files at a later date use:
#	autoreconf -f -i -v

aclocal
autoheader
automake --add-missing --copy --foreign
autoconf
