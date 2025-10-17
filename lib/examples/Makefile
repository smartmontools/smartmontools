#
# Makefile for libsmartmon examples
#
# Home page of code is: https://www.smartmontools.org
#
# Copyright (C) 2025 Christian Franke
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

# Platform specific extra libraries
## Linux
#LIBS =
## Darwin
#LIBS = -framework CoreFoundation -framework IOKit
## FreeBSD
#LIBS = -lcam -lsbuf # -lusb
## NetBSD, OpenBSD
#LIBS = -lutil
## Windows
#LIBS = -lole32 -loleaut32 # -ladvapi -lwinpthread
## Cygwin
#LIBS = -lole32 -loleaut32

CPPFLAGS =
CXXFLAGS = -O -Wall -W
LDFLAGS = -static
LDLIBS = -lsmartmon $(LIBS)

PROGRAMS = lsdisk

all: $(PROGRAMS)

.cpp:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(PROGRAMS)
