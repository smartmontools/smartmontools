# Makefile for smartmontools
#
# Copyright (C) 2002 Bruce Allen <ballen@uwm.edu>
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2, or (at your option) any later
# version.
# 
# You should have received a copy of the GNU General Public License (for
# example COPYING); if not, write to the Free Software Foundation, Inc., 675
# Mass Ave, Cambridge, MA 02139, USA.

CC	= gcc
CFLAGS	= -fsigned-char -Wall -g 

all: smartd smartctl

smartctl: atacmds.o scsicmds.o smartctl.c smartctl.h ataprint.o scsiprint.o atacmds.h ataprint.h scsicmds.h scsiprint.h
	${CC} -o smartctl ${CFLAGS} atacmds.o scsicmds.o ataprint.o smartctl.c scsiprint.o

smartd:  atacmds.o scsicmds.o smartd.c smartd.h atacmds.h scsicmds.h
	${CC} -o smartd ${CFLAGS} scsicmds.o atacmds.o smartd.c

ataprint.o: atacmds.o ataprint.h ataprint.c smartctl.h extern.h
	${CC} ${CFLAGS} -c ataprint.c

scsiprint.o: scsiprint.h scsiprint.c scsicmds.o smartctl.h extern.h scsicmds.h
	${CC} ${CFLAGS} -c scsiprint.c 

atacmds.o: atacmds.h atacmds.c
	${CC} ${CFLAGS} -c atacmds.c 

scsicmds.o: scsicmds.h scsicmds.c 
	${CC} ${CFLAGS} -c scsicmds.c

clean:
	rm -f *.o smartctl smartd *~

install: smartctl smartd smartctl.8 smartd.8 smartd.initd
	install -m 755 -o root -g root smartctl /usr/sbin
	install -m 755 -o root -g root smartd /usr/sbin
	install -m 644 -o root -g root smartctl.8 /usr/share/man/man8
	install -m 644 -o root -g root smartd.8 /usr/share/man/man8
	cp ./smartd.initd /etc/rc.d/init.d/smartd
	/sbin/chkconfig --add smartd

uninstall:
	rm -f /usr/sbin/smartctl /usr/sbin/smartd /usr/share/man/man8/smartctl.8 /usr/share/man/man8/smartd.8  /usr/share/man/man8/smartctl.8.gz /usr/share/man/man8/smartd.8.gz
	/sbin/chkconfig --del smartd
	rm -f /etc/rc.d/init.d/smartd
