Release:  10
Summary:	SMARTmontools - for monitoring S.M.A.R.T. disks and devices
Name:		smartmontools
Version:	5.0
License:	GPL
Group:		Applications/System
Source0:	%{name}-%{version}.tar.gz
URL:            http://smartmontools.sourceforge.net/
Prereq:		/sbin/chkconfig
BuildRoot:	%{_builddir}/%{name}-%{version}-root
Obsoletes:	smartctl
Obsoletes:      smartd
Obsoletes:	ucsc-smartsuite
Obsoletes:      smartsuite
Packager:       Bruce Allen <smartmontools-support@lists.sourceforge.net>

# SOURCE CODE CAN BE FOUND AT:
# http://telia.dl.sourceforge.net/sourceforge/smartmontools/smartmontools-%{version}-%{release}.tar.gz

# Copyright (C) 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
# Home page: http://smartmontools.sourceforge.net
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


%description
SMARTmontools controls and monitors storage devices using
the Self-Monitoring, Analysis and Reporting Technology System
(S.M.A.R.T.) built into ATA and SCSI Hard Drives. This is used to
check the reliability of the hard drive and to predict drive
failures. The suite is derived from the smartsuite package, and
contains two utilities.  The first, smartctl, is a command line
utility designed to perform simple S.M.A.R.T. tasks. The second,
smartd, is a daemon that periodically monitors smart status and
reports errors to syslog.  The package is compatible with the
ATA/ATAPI-5 specification.  Future releases will be compatible with
the ATA/ATAPI-6 andATA/ATAPI-7 specifications.  The package is
intended to incorporate as much "vendor specific" and "reserved"
information as possible about disk drives.  man smartctl and man
smartd will provide more information. This RPM file is compatible with
all RedHat releases back to at least 6.2 and should work OK on any
modern linux distribution.  The most recent versions of this package
and additional information can be found at the URL:
http://smartmontools.sourceforge.net/


# The following sections are executed by the SRPM file
%prep

%setup -q

%build
make

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%files
%defattr(-,root,root)
/usr/sbin/smartd
/usr/sbin/smartctl
/etc/rc.d/init.d/smartd
%doc %attr(644,root,root) /usr/share/man/man8/smartctl.8.gz
%doc %attr(644,root,root) /usr/share/man/man8/smartd.8.gz
%doc CHANGELOG COPYING TODO README VERSION

%clean
rm -rf $RPM_BUILD_ROOT
rm -rf %{_builddir}/%{name}-%{version}

# The following are executed only by the binary RPM at install/uninstall

%post
if [ -f /var/lock/subsys/smartd ]; then
        /etc/rc.d/init.d/smartd restart 1>&2
	echo "Restarted smartd services"
else
        echo "Run \"/etc/rc.d/init.d/smartd start\" to start smartd service now."
	echo "Run \"/sbin/chkconfig --add smartd\", to start smartd service on system boot"
fi

%preun
if [ -f /var/lock/subsys/smartd ]; then
        /etc/rc.d/init.d/smartd stop 1>&2
	echo "Stopping smartd services"
fi
/sbin/chkconfig --del smartd

%define date	%(echo `LC_ALL="C" date +"%a %b %d %Y"`)
%changelog
* Thu Oct 17 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
-   Removed extraneous space before some error message printing.
-   Fixed some character buffers that were too short for contents.
    Only used for unrecognized drives, so probably damage was minimal.
* Wed Oct 16 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
-   Initial release.  Code is derived from smartsuite, and is
    intended to be compatible with the ATA/ATAPI-5 specifications.
-   For IBM disks whose raw temp data includes three temps. print all
    three
-   print timestamps for error log to msec precision
-   added -m option for Hitachi disks that store power on life in
    minutes
-   added -L option for printing self-test error logs
-   in -l option, now print power on lifetime, so that one can see
    when the error took place
-   updated SMART structure definitions to ATA-5 spec
-   added -p option
-   added -f and -F options to enable/disable autosave threshold
    parameters

