Release:  3
Summary:	SMARTmontools - for monitoring S.M.A.R.T. disks and devices
Name:		smartmontools
Version:	5.0
License:	GPL
Group:		Applications/System
Source0:	http://prdownloads.sourceforge.net/%{name}-%{version}.tar.gz
URL:            http://smartmontools.sourceforge.net/
Prereq:		/sbin/chkconfig
BuildRoot:	%{_builddir}/%{name}-%{version}-root
Obsoletes:	smartctl
Obsoletes:      smartd
Obsoletes:	ucsc-smartsuite
Obsoletes:      smartsuite

%description 
SMARTmontools controls and monitors storage devices using the
Self-Monitoring, Analysis and Reporting Technology System (S.M.A.R.T.)
build into ATA and SCSI Hard Drives. This is used to check the
reliability of the hard drive and predict drive failures. The suite
contents two utilities.  The first, smartctl, is a command line
utility designed to perform simple S.M.A.R.T. tasks. The second,
smartd, is a daemon that periodically monitors smart status and
reports errors to syslog.  The package is compatible with the
ATA/ATAPI-5 specification.  Future releases will be compatible with
the ATA/ATAPI-6 andATA/ATAPI-7 specifications.  The package is
intended to incorporate as much "vendor specific" and "reserved"
information as possible about disk drives.  man smartctl and man
smartd will provide more information.

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
%doc CHANGELOG COPYING TODO README

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
* %{date} Bruce Allen smartmontools-support@lists.sourceforge.net
Initial release.  Code is derived from smartsuite, and is
   intended to be compatible with the ATA/ATAPI-5 specifications.
