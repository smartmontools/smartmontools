#!/bin/sh -e
# $Id: autogen.sh,v 1.2 2003/10/02 10:07:44 ballen4705 Exp $

aclocal
autoheader
automake --add-missing --copy --foreign
autoconf
