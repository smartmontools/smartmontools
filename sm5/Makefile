# Makefile for smartmontools
#
# Home page: http://smartmontools.sourceforge.net
#
# $Id: Makefile,v 1.60 2003/04/09 09:02:56 ballen4705 Exp $
#
# Copyright (C) 2002-3 Bruce Allen <smartmontools-support@lists.sourceforge.net>
# 
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 2, or (at your option) any later
# version.
# 
# You should have received a copy of the GNU General Public License (for
# example COPYING); if not, write to the Free Software Foundation, Inc., 675
# Mass Ave, Cambridge, MA 02139, USA.
#
# This code was originally developed as a Senior Thesis by Michael Cornwell
# at the Concurrent Systems Laboratory (now part of the Storage Systems
# Research Center), Jack Baskin School of Engineering, University of
# California, Santa Cruz. http://ssrc.soe.ucsc.edu/

CC	= gcc

# Debugging
# CFLAGS = -fsigned-char -Wall -g

# Build against kernel header files.  Change linux-2.4 to correct path for your system
# CFLAGS	= -fsigned-char -Wall -O2 -I./usr/src/linux-2.4/include

# Normal build NOTE: I have had reports that with gcc 3.2 this code
# fails if you use anything but -Os.  I'll remove this comment when
# this is resolved, or I am reminded of it! GCC GNATS bug report
# #8404.  If you are getting strange output from gcc 3.2 try
# uncommenting LDFLAGS -s below.  Stripping the symbols seems to fix
# the problem.
CFLAGS	 = -fsigned-char -Wall -O2
CPPFLAGS = -DHAVE_GETOPT_H -DHAVE_GETOPT_LONG
LDFLAGS  = # -s

es=examplescripts

releasefiles=atacmds.c atacmds.h ataprint.c ataprint.h CHANGELOG COPYING \
  extern.h knowndrives.c knowndrives.h Makefile README scsicmds.c scsicmds.h \
  scsiprint.c scsiprint.h smartctl.8 smartctl.c smartctl.h smartd.8 smartd.c \
  smartd.h smartd.initd TODO WARNINGS VERSION smartd.conf smartd.conf.5 \
  utility.c utility.h examplescripts/

counter=$(shell cat VERSION)
pkgname=smartmontools-5.1
pkgname2=$(pkgname)-$(counter)

all: smartd smartctl
	@echo -e "\n\nSmartd can now use a configuration file /etc/smartd.conf. Do:\n\n\tman ./smartctl.8\n\tman ./smartd.8\n\tman ./smartd.conf.5\n"
	@echo -e "to read the manual pages now.  Unless you do a \"make install\" the manual pages won't be installed.\n"

smartctl: smartctl.c atacmds.o ataprint.o scsicmds.o scsiprint.o utility.o knowndrives.o\
          smartctl.h atacmds.h ataprint.h scsicmds.h scsiprint.h \
          utility.h extern.h VERSION Makefile
	$(CC) -DSMARTMONTOOLS_VERSION=$(counter) -o smartctl $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) smartctl.c \
                                      atacmds.o ataprint.o knowndrives.o scsicmds.o scsiprint.o utility.o

smartd:  smartd.c atacmds.o ataprint.o scsicmds.o utility.o knowndrives.o \
         smartd.h atacmds.h ataprint.h scsicmds.h utility.h extern.h VERSION \
         Makefile
	$(CC) -DSMARTMONTOOLS_VERSION=$(counter) -o smartd $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) smartd.c \
                                      atacmds.o ataprint.o knowndrives.o scsicmds.o utility.o 

atacmds.o: atacmds.c atacmds.h utility.h extern.h Makefile
	$(CC) $(CFLAGS) $(CPPFLAGS) -c atacmds.c 

ataprint.o: ataprint.c atacmds.h ataprint.h knowndrives.h smartctl.h extern.h utility.h \
            Makefile
	$(CC) $(CFLAGS) $(CPPFLAGS) -c ataprint.c

scsicmds.o: scsicmds.c scsicmds.h extern.h Makefile
	$(CC) $(CFLAGS) $(CPPFLAGS) -c scsicmds.c

scsiprint.o: scsiprint.c extern.h scsicmds.h scsiprint.h smartctl.h utility.h Makefile
	$(CC) $(CFLAGS) $(CPPFLAGS) -c scsiprint.c

utility.o: utility.c utility.h Makefile
	$(CC) $(CFLAGS) $(CPPFLAGS) -c utility.c

knowndrives.o: knowndrives.c knowndrives.h Makefile
	$(CC) $(CFLAGS) $(CPPFLAGS) -c knowndrives.c



# This extracts the configuration file directives from smartd.8 and
# inserts them into smartd.conf.5
smartd.conf.5: smartd.8
	sed '1,/STARTINCLUDE/ D;/ENDINCLUDE/,$$D' < smartd.8 > tmp.directives
	sed '/STARTINCLUDE/,$$D'  < smartd.conf.5 > tmp.head
	sed '1,/ENDINCLUDE/D'   < smartd.conf.5 > tmp.tail
	cat tmp.head            > smartd.conf.5
	echo "\# STARTINCLUDE" >> smartd.conf.5
	cat tmp.directives     >> smartd.conf.5
	echo "\# ENDINCLUDE"   >> smartd.conf.5
	cat tmp.tail           >> smartd.conf.5
	rm -f tmp.head tmp.tail tmp.directives
clean:
	rm -f *.o smartctl smartd *~ \#*\# smartmontools*.tar.gz smartmontools*.rpm temp.* smart*.8.gz smart*.5.gz

install:
	if [ ! -f smartd -o ! -f smartctl ] ; then echo -e "\n\nYOU MUST FIRST DO \"make\"\n" ; exit 1 ; fi
	/bin/gzip -c smartctl.8 > smartctl.8.gz
	/bin/gzip -c smartd.8   > smartd.8.gz
	/bin/gzip -c smartd.conf.5 > smartd.conf.5.gz
	rm -f $(DESTDIR)/usr/share/man/man8/smartctl.8
	rm -f $(DESTDIR)/usr/share/man/man8/smartd.8
	install -m 755 -D smartctl         $(DESTDIR)/usr/sbin/smartctl
	install -m 755 -D smartd           $(DESTDIR)/usr/sbin/smartd
	install -m 755 -D smartd.initd     $(DESTDIR)/etc/rc.d/init.d/smartd
	install -m 644 -D smartctl.8.gz    $(DESTDIR)/usr/share/man/man8/smartctl.8.gz
	install -m 644 -D smartd.8.gz      $(DESTDIR)/usr/share/man/man8/smartd.8.gz
	install -m 644 -D smartd.conf.5.gz $(DESTDIR)/usr/share/man/man5/smartd.conf.5.gz
	install -m 644 -D CHANGELOG        $(DESTDIR)/usr/share/doc/smartmontools-5.1/CHANGELOG
	install -m 644 -D COPYING          $(DESTDIR)/usr/share/doc/smartmontools-5.1/COPYING
	install -m 644 -D README           $(DESTDIR)/usr/share/doc/smartmontools-5.1/README
	install -m 644 -D TODO             $(DESTDIR)/usr/share/doc/smartmontools-5.1/TODO
	install -m 644 -D VERSION          $(DESTDIR)/usr/share/doc/smartmontools-5.1/VERSION
	install -m 644 -D WARNINGS         $(DESTDIR)/usr/share/doc/smartmontools-5.1/WARNINGS
	install -m 644 -D smartd.conf      $(DESTDIR)/usr/share/doc/smartmontools-5.1/smartd.conf
	install -m 644 -D $(es)/README     $(DESTDIR)/usr/share/doc/smartmontools-5.1/$(es)/README
	install -m 755 -D $(es)/Example1   $(DESTDIR)/usr/share/doc/smartmontools-5.1/$(es)/Example1
	install -m 755 -D $(es)/Example2   $(DESTDIR)/usr/share/doc/smartmontools-5.1/$(es)/Example2
	install -m 755 -D $(es)/Example3   $(DESTDIR)/usr/share/doc/smartmontools-5.1/$(es)/Example3
	install -m 644 -D smartd.conf      $(DESTDIR)/etc/smartd.conf.example
	if [ ! -f $(DESTDIR)/etc/smartd.conf ] ; then install -m 644 -D smartd.conf $(DESTDIR)/etc/smartd.conf ; fi
	@echo -e "\n\nTo manually start smartd on bootup, run /etc/rc.d/init.d/smartd start"
	@echo "To automatically start smartd on bootup, run /sbin/chkconfig --add smartd"
	@echo -e "\n\nSmartd can now use a configuration file /etc/smartd.conf. Do:\nman 8 smartd\n."
	@echo -e "A sample configuration file may be found in /usr/share/doc/smartmontools-5.1 and /etc/smartd.conf.example/\n\n"

# perhaps for consistency I should also have $(DESTDIR) for the uninstall...
uninstall:
	rm -f /usr/share/man/man8/smartctl.8 /usr/share/man/man8/smartd.8 /usr/sbin/smartctl \
              /usr/share/man/man8/smartctl.8.gz /usr/share/man/man8/smartd.8.gz \
              /usr/share/man/man5/smartd.conf.5.gz /usr/sbin/smartd 
	rm -rf /usr/share/doc/smartmontools-5.1/
	if [ -f /var/lock/subsys/smartd -a -f /etc/rc.d/init.d/smartd ] ; then /etc/rc.d/init.d/smartd stop ; fi
	if [ -f /etc/rc.d/init.d/smartd ] ; then /sbin/chkconfig --del smartd ; fi
	if [ -f /etc/rc.d/init.d/smartd ] ; then  rm -f /etc/rc.d/init.d/smartd ; fi
	if [ -f /etc/smartd.conf.example ] ; then rm -f /etc/smartd.conf.example ; fi
	if [ -f /etc/smartd.conf ] ; then echo -e "\n\nWe have NOT REMOVED /etc/smartd.conf\n\n" ; fi


# Some of this mess is to automatically increment the release numbers.
# The number of the next release is kept in the file "VERSION"
release: smartd.conf.5
	if [ ! -f add -o ! -d CVS ] ; then echo "The make release target requires files checked out of CVS" ; exit 1 ; fi
	cat smartmontools.spec | sed '/Release:/d' > temp.spec
	echo "Release: " $(counter) > temp.version
	cat temp.version temp.spec > smartmontools.spec
	rm -f temp.spec temp.version
	. cvs-script && cvs commit -m "Release 5.1.$(counter)"
	. cvs-script && cvs tag -d "RELEASE_5_1_$(counter)" && cvs tag "RELEASE_5_1_$(counter)"
	rm -rf $(pkgname)
	mkdir $(pkgname)
	cp -a $(releasefiles) $(pkgname)
	rm -rf $(pkgname)/examplescripts/CVS
	tar zcvf $(pkgname).tar.gz $(pkgname)
	mv -f $(pkgname) $(pkgname2)
	tar zcvf $(pkgname2).tar.gz $(pkgname2)
	rm -rf $(pkgname2)
	mv -f $(pkgname).tar.gz /usr/src/redhat/SOURCES/
	rpm -ba smartmontools.spec
	mv /usr/src/redhat/RPMS/i386/$(pkgname)*.rpm .
	mv /usr/src/redhat/SRPMS/$(pkgname)*rpm .
	rm -f /usr/src/redhat/SOURCES/$(pkgname).tar.gz
	echo `hostname` | grep -q lap && echo $(shell ./add) > VERSION
