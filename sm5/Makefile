# Makefile for smartmontools
#
# $Id: Makefile,v 1.9 2002/10/11 09:20:32 ballen4705 Exp $
#
# Copyright (C) 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
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
# CFLAGS = -fsigned-char -Wall -g 
CFLAGS	= -fsigned-char -Wall -O2

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
	rm -f *.o smartctl smartd *~ \#*\# smartmontools*.tar.gz smartmontools*.rpm

install: smartctl smartd smartctl.8 smartd.8 smartd.initd
	install -m 755 -o root -g root -D smartctl $(DESTDIR)/usr/sbin/smartctl
	install -m 755 -o root -g root -D smartd $(DESTDIR)/usr/sbin/smartd
	install -m 644 -o root -g root -D smartctl.8 $(DESTDIR)/usr/share/man/man8/smartctl.8
	install -m 644 -o root -g root -D smartd.8 $(DESTDIR)/usr/share/man/man8/smartd.8
	install -m 755 -o root -g root -D smartd.initd $(DESTDIR)/etc/rc.d/init.d/smartd
	echo "To manually start smartd on bootup, run etc/rc.d/init.d/smartd start"
	echo "To Automatically start smartd on bootup, run /sbin/chkconfig --add smartd"

uninstall:
	rm -f /usr/sbin/smartctl /usr/sbin/smartd /usr/share/man/man8/smartctl.8 /usr/share/man/man8/smartd.8  /usr/share/man/man8/smartctl.8.gz /usr/share/man/man8/smartd.8.gz
	/sbin/chkconfig --del smartd
	/etc/rc.d/init.d/smartd stop
	rm -f /etc/rc.d/init.d/smartd

releasefiles=atacmds.c atacmds.h ataprint.c ataprint.h CHANGELOG COPYING extern.h Makefile\
  README scsicmds.c scsicmds.h scsiprint.c scsiprint.h smartctl.8 smartctl.c smartctl.h\
  smartd.8 smartd.c smartd.h smartd.initd TODO

pkgname=smartmontools-5.0
workdir=$(pkgname)

release: $(releasefiles)
	rm -rf $(workdir)
	mkdir $(workdir)
	cp -a $(releasefiles) $(workdir)
	tar zcvf $(pkgname).tar.gz $(workdir)
	rm -rf $(workdir)
	cp $(pkgname).tar.gz /usr/src/redhat/SOURCES
	rpm -ba smartmontools.spec
	mv /usr/src/redhat/RPMS/i386/$(pkgname)*.rpm .
	mv /usr/src/redhat/SRPMS/$(pkgname)*rpm .
	rm -f /usr/src/redhat/SOURCES/$(pkgname).tar.gz

