## Process this file with automake to produce Makefile.in
#
# $Id: Makefile.am,v 1.43 2003/12/11 19:29:16 card_captor Exp $
#

@SET_MAKE@

AUTOMAKE_OPTIONS = 1.6

sbin_PROGRAMS = smartd 	\
		smartctl

AM_CPPFLAGS = -DSYSCONFDIR=\"$(sysconfdir)\"

smartd_SOURCES =  smartd.c       \
                  smartd.h       \
                  atacmdnames.c	\
                  atacmdnames.h	\
                  atacmds.c	\
                  atacmds.h	\
                  ataprint.c 	\
                  ataprint.h	\
                  extern.h       \
                  knowndrives.c	\
                  knowndrives.h	\
                  scsicmds.c	\
                  scsicmds.h	\
                  scsiprint.c	\
                  scsiprint.h	\
                  utility.c 	\
                  utility.h

smartd_LDADD = @os_deps@ @os_libs@
smartd_DEPENDENCIES = @os_deps@
EXTRA_smartd_SOURCES = os_linux.c \
                       os_linux.h \
		       os_freebsd.c \
		       os_freebsd.h \
		       os_solaris.c \
		       os_solaris.h \
                       os_generic.c \
                       os_generic.h


smartctl_SOURCES= smartctl.c    \
                  smartctl.h    \
                  atacmdnames.c	\
                  atacmdnames.h	\
                  atacmds.c	\
                  atacmds.h	\
                  ataprint.c	\
                  ataprint.h	\
                  extern.h      \
                  knowndrives.c	\
                  knowndrives.h	\
                  scsicmds.c	\
                  scsicmds.h	\
                  scsiprint.c	\
                  scsiprint.h	\
                  utility.c	\
                  utility.h

smartctl_LDADD = @os_deps@ @os_libs@
smartctl_DEPENDENCIES = @os_deps@
EXTRA_smartctl_SOURCES = os_linux.c \
                       os_linux.h   \
		       os_freebsd.c \
		       os_freebsd.h \
		       os_solaris.c \
		       os_solaris.h \
		       os_generic.c \
		       os_generic.h



if OS_SOLARIS
# For systems that has automake-unfriendly manual page sectioning policy
extra_MANS =      smartd.conf.4 \
                  smartctl.1m   \
                  smartd.1m
install-man: $(extra_MANS)
	@$(NORMAL_INSTALL)
	$(mkinstalldirs) $(DESTDIR)$(mandir)/man4
	$(mkinstalldirs) $(DESTDIR)$(mandir)/man1m
	for i in $(extra_MANS); do \
	  if test -f $(srcdir)/$$i; then file=$(srcdir)/$$i; \
	  else file=$$i; fi; \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " $(INSTALL_DATA) $$file $(DESTDIR)$(mandir)/man$$ext/$$inst"; \
	  $(INSTALL_DATA) $$file $(DESTDIR)$(mandir)/man$$ext/$$inst; \
	done
uninstall-man:
	@$(NORMAL_UNINSTALL)
	for i in $(extra_MANS); do \
	  if test -f $(srcdir)/$$i; then file=$(srcdir)/$$i; \
	  else file=$$i; fi; \
	  ext=`echo $$i | sed -e 's/^.*\\.//'`; \
	  inst=`echo $$i | sed -e 's/\\.[0-9a-z]*$$//'`; \
	  inst=`echo $$inst | sed -e 's/^.*\///'`; \
	  inst=`echo $$inst | sed '$(transform)'`.$$ext; \
	  echo " rm -f $(DESTDIR)$(mandir)/man$$ext/$$inst"; \
	  rm -f $(DESTDIR)$(mandir)/man$$ext/$$inst; \
	done
%.1m: %.8
	awk '/^.TH/ {$$3="1m"} {print}' < $< | \
	sed -e 's/smartd\.conf\(.*\)(5)/smartd.conf\1(4)/g' \
	    -e 's/smartctl\(.*\)(8)/smartctl\1(1m)/g' \
	    -e 's/smartd\(.*\)(8)/smartd\1(1m)/g' > $@
%.4: %.5
	awk '/^.TH/ {$$3="4"}  {print}' < $< | \
	sed -e 's/smartd\.conf\(.*\)(5)/smartd.conf\1(4)/g' \
	    -e 's/smartctl\(.*\)(8)/smartdctl\1(1m)/g' \
	    -e 's/smartd\(.*\)(8)/smartd\1(1m)/g' > $@
else
# For systems that adopts traditional manner
man_MANS =        smartd.conf.5 \
                  smartctl.8    \
                  smartd.8
endif

docsdir=$(docdir)
docs_DATA = AUTHORS   \
            CHANGELOG \
            COPYING   \
            INSTALL   \
            NEWS      \
            README    \
            TODO      \
            WARNINGS  \
            smartd.conf

sysconf_DATA = smartd.conf$(smartd_suffix)

if SMARTD_SUFFIX
smartd.conf$(smartd_suffix): smartd.conf
	cp smartd.conf smartd.conf$(smartd_suffix)
endif

initd_DATA = smartd.initd

EXTRA_DIST = \
                smartmontools.spec	\
                smartd.initd.in		\
		smartd.8.in		\
		smartctl.8.in		\
		smartd.conf.5.in	\
		smartd.conf		\
		autogen.sh		\
                $(docs_DATA)

DISTCLEANFILES = smartd.conf.5 \
                 smartd.conf.4 \
                 smartd.8      \
                 smartd.1m     \
	         smartctl.8    \
	         smartctl.1m   \
		 smartd.initd

smartd.conf.5.in: smartd.8.in
	sed '1,/STARTINCLUDE/ D;/ENDINCLUDE/,$$D' < $(srcdir)/smartd.8.in > $(top_builddir)/tmp.directives
	sed '/STARTINCLUDE/,$$D'  < $(srcdir)/smartd.conf.5.in > $(top_builddir)/tmp.head
	sed '1,/ENDINCLUDE/D'   < $(srcdir)/smartd.conf.5.in > $(top_builddir)/tmp.tail
	cat $(top_builddir)/tmp.head > $(srcdir)/smartd.conf.5.in
	echo '.\" STARTINCLUDE' >> $(srcdir)/smartd.conf.5.in
	cat $(top_builddir)/tmp.directives >> $(srcdir)/smartd.conf.5.in
	echo '.\" ENDINCLUDE'   >> $(srcdir)/smartd.conf.5.in
	cat $(top_builddir)/tmp.tail >> $(srcdir)/smartd.conf.5.in
	rm -f $(top_builddir)/tmp.head $(top_builddir)/tmp.tail $(top_builddir)/tmp.directives

install-initdDATA: $(initd_DATA)
	$(mkinstalldirs) $(DESTDIR)$(initddir)
	$(INSTALL_SCRIPT) $(top_builddir)/smartd.initd $(DESTDIR)$(initddir)/smartd$(smartd_suffix)
	@echo -e "\n\n####################################################################\n#"
	@echo -e "#                       PLEASE READ THIS BOX!\n#"
	@echo -e "#   To manually start the smartd daemon, run:\n#   ${initddir}/smartd start\n#"
	@echo -e "#   To automatically start smartd on bootup, run:\n#   /sbin/chkconfig --add smartd\n#"
	@echo -e "#   smartd can now use a configuration file ${sysconfdir}/smartd.conf. Do:\n#   man 8 smartd"
	@echo -e "#   to learn about it. A sample configuration file can be found in:\n#   ${docdir}\n#"
	@echo -e "####################################################################\n\n"

uninstall-initdDATA:
	rm -rf $(DESTDIR)$(initddir)/smartd$(smartd_suffix)

uninstall-docsDATA:
	rm -rf $(DESTDIR)$(docsdir)

smart%: $(srcdir)/smart%.in Makefile
	sed "s|CURRENT_CVS_VERSION|$(releaseversion)|g" $< | \
	sed "s|CURRENT_CVS_DATE|$(smartmontools_release_date)|g" | \
	sed "s|CURRENT_CVS_TIME|$(smartmontools_release_time)|g" | \
	sed "s|/usr/share/man/|$(mandir)/|g" | \
	sed "s|/usr/sbin/|$(sbindir)/|g" | \
	sed "s|/etc/rc\\.d/init.d/|$(initddir)/|g" | \
	sed "s|/usr/share/doc/smartmontools-5\\.1/|$(docsdir)/|g" | \
	sed "s|/etc/smartd\\.conf|$(sysconfdir)/smartd\\.conf|g" > $@

SUBDIRS= . examplescripts
