## Process this file with automake to produce Makefile.in
#
# $Id: Makefile.am,v 1.7 2003/09/22 14:23:37 guidog Exp $
#
sbin_PROGRAMS = smartd 	\
		smartctl

AM_CFLAGS = -fsigned-char -Wall

smartd_SOURCES = smartd.c \
                atacmds.c	\
                ataprint.c 	\
                atacmdnames.c	\
                escalade.c	\
                knowndrives.c	\
                scsicmds.c	\
                scsiprint.c	\
                utility.c 	\
                atacmds.h	\
                atacmdnames.h	\
                ataprint.h	\
                escalade.h	\
                knowndrives.h	\
                scsicmds.h	\
                scsiprint.h	\
                smartd.h	\
                utility.h	\
                extern.h 

smartctl_SOURCES= smartctl.c \
                atacmds.c	\
                ataprint.c	\
                atacmdnames.c	\
                escalade.c	\
                knowndrives.c	\
                scsicmds.c	\
                scsiprint.c	\
                smartctl.h	\
                utility.c	\
                atacmds.h	\
                atacmdnames.h	\
                ataprint.h	\
                escalade.h	\
                knowndrives.h	\
                scsicmds.h	\
                scsiprint.h	\
                utility.h	\
                extern.h 

man_MANS = smartd.conf.5	\
                smartctl.8	\
                smartd.8

docsdir=$(docdir)
docs_DATA = README    \
            TODO      \
            WARNINGS  \
            CHANGELOG \
            COPYING   \
            smartd.conf

sysconf_DATA = smartd.conf

initddir=$(sysconfdir)/rc.d/init.d
initd_DATA = smartd.initd

EXTRA_DIST = $(man_MANS)		\
                smartmontools.spec	\
                smartd.initd		\
		smartd.8.in		\
		smartctl.8.in		\
		smartd.conf.5.in	\
                $(docs_DATA)

DISTCLEANFILES = smartd.conf.5 \
                 smartd.8      \
	         smartctl.8

smartd.conf.5.in: smartd.8.in
	sed '1,/STARTINCLUDE/ D;/ENDINCLUDE/,$$D' < $(srcdir)/smartd.8.in > $(top_builddir)/tmp.directives
	sed '/STARTINCLUDE/,$$D'  < $(srcdir)/smartd.conf.5.in > $(top_builddir)/tmp.head
	sed '1,/ENDINCLUDE/D'   < $(srcdir)/smartd.conf.5.in > $(top_builddir)/tmp.tail
	cat $(top_builddir)/tmp.head > $(srcdir)/smartd.conf.5.in
	echo "\# STARTINCLUDE" >> $(srcdir)/smartd.conf.5.in
	cat $(top_builddir)/tmp.directives >> $(srcdir)/smartd.conf.5.in
	echo "\# ENDINCLUDE"   >> $(srcdir)/smartd.conf.5.in
	cat $(top_builddir)/tmp.tail >> $(srcdir)/smartd.conf.5.in
	rm -f $(top_builddir)/tmp.head $(top_builddir)/tmp.tail $(top_builddir)/tmp.directives

install-initdDATA: $(initd_DATA)
	$(mkinstalldirs) $(initddir)
	$(INSTALL_SCRIPT) $(srcdir)/smartd.initd $(initddir)/smartd
	@echo -e "\n\nTo manually start smartd on bootup, run /etc/rc.d/init.d/smartd start"
	@echo "To automatically start smartd on bootup, run /sbin/chkconfig --add smartd"
	@echo -e "\n\nSmartd can now use a configuration file /etc/smartd.conf. Do:\nman 8 smartd\n."
	@echo -e "A sample configuration file may be found in ${docdir}.\n\n"

uninstall-initdDATA:
	rm -rf $(initddir)/smartd

uninstall-docsDATA:
	rm -rf $(docsdir)

# Todo:
#   documentation: $(docsdir)
#   initd.script: $(initddir)
smart%: smart%.in
	sed "s|/usr/share/man/|$(mandir)/|g" $< | \
	sed "s|/usr/sbin/|$(sbindir)/|g" | \
	sed "s|/etc/|$(sysconfdir)/|g" > $@

SUBDIRS= . examplescripts
