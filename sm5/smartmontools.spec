Release:  15
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

# CVS ID of this file is:
# $Id: smartmontools.spec,v 1.26 2002/10/25 17:07:17 ballen4705 Exp $

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

# since this installs the gzipped documentation files, remove
# non-gzipped ones of the same name.
%pre
rm -f /usr/share/man/man8/smartctl.8
rm -f /usr/share/man/man8/smartd.8

%post
if [ -f /var/lock/subsys/smartd ]; then
        /etc/rc.d/init.d/smartd restart 1>&2
	echo "Restarted smartd services"
else
        echo "Run \"/etc/rc.d/init.d/smartd start\" to start smartd service now."
	echo "Run \"/sbin/chkconfig --add smartd\", to start smartd service on system boot"
fi
echo "Note that you can now use a configuration file /etc/smartd.conf to control the"
echo "startup behavior of the smartd daemon.  See man 8 smartd for details."

%preun
if [ -f /var/lock/subsys/smartd ]; then
        /etc/rc.d/init.d/smartd stop 1>&2
	echo "Stopping smartd services"
fi
/sbin/chkconfig --del smartd

%define date	%(echo `LC_ALL="C" date +"%a %b %d %Y"`)
%changelog
* Fri Oct 25 2002 Bruce Allen  <smartmontools-support@lists.sourceforge.net>
- changes to the Makefile and spec file so that if there are ungzipped manual
  pages in place these will be removed so that the new gzipped man pages are
  visible.
- smartd on startup now looks in the configuration file /etc/smartd.conf for
  a list of devices which to include in its monitoring list.  See man page
  (man smartd) for syntax. If not found, try all ata and ide devices.
- smartd: close file descriptors of SCSI device if not SMART capable
  Closes ALL file descriptors after forking to daemon.
- added new temperature attribute (231, temperature)
- smartd: now open ATA disks using O_RDONLY
* Thu Oct 24 2002 Bruce Allen <smartmontools-support@lists.sourceforge.net>
- smartd now prints the name of a failed or changed attribute into logfile,
  not just ID number
- Changed name of -p (print version) option to -V
- Minor change in philosophy: if a SMART command fails or the device
    appears incapable of a SMART command that the user has asked for,
    complain by printing an error message, but go ahead and try
    anyway.  Since unimplemented SMART commands should just return an
    error but not cause disk problems, this should't cause any
    difficulty.
- Added two new flags: q and Q.  q is quiet mode - only print: For
    the -l option, errors recorded in the SMART error log; For the -L
    option, errors recorded in the device self-test log; For the -c
    SMART "disk failing" status or device attributes (pre-failure or
    usage) which failed either now or in the past; For the -v option
    device attributes (pre-failure or usage) which failed either now
    or in the past.  Q is Very Quiet mode: Print no ouput.  The only
    way to learn about what was found is to use the exit status of
    smartctl.
- smartctl now returns sensible values (bitmask).  See smartctl.h
    for the values, and the man page for documentation.
- The SMART status check now uses the correct ATA call.  If failure
    is detected we search through attributes to list the failed ones.
    If the SMART status check shows GOOD, we then look to see if their
    are any usage attributes or prefail attributes have failed at any
    time.  If so we print them.
- Modified function that prints vendor attributes to say if the
    attribute has currently failed or has ever failed.
- -p option now prints out license info and CVS strings for all
    modules in the code, nicely formatted.
- Previous versions of this code (and Smartsuite) only generate
    SMART failure errors if the value of an attribute is below the
    threshold and the prefailure bit is set.  However the ATA Spec
    (ATA4 <=Rev 4) says that it is a SMART failure if the value of an
    attribute is LESS THAN OR EQUAL to the threshold and the
    prefailure bit is set.  This is now fixed in both smartctl and
    smartd.  Note that this is a troubled subject -- the original
    SFF 8035i specification defining SMART was inconsistent about
    this.  One section says that Attribute==Threshold is pass,
    and another section says it is fail.  However the ATA specs are
    consistent and say Attribute==Threshold is a fail.
- smartd did not print the correct value of any failing SMART attribute.  It
    printed the index in the attribute table, not the attribute
    ID. This is fixed.
- when starting self-tests in captive mode ioctl returns EIO because
    the drive has been busied out.  Detect this and don't return an eror
    in this case.  Check this this is correct (or how to fix it?)
 - fixed possible error in how to determine ATA standard support
    for devices with no ATA minor revision number.
- device opened only in read-only not read-write mode.  Don't need R/W 
    access to get smart data. Check this with Andre.
- smartctl now handles all possible choices of "multiple options"
    gracefully.  It goes through the following phases of operation,
    in order: INFORMATION, ENABLE/DISABLE, DISPLAY DATA, RUN/ABORT TESTS.
    Documentation has bee updated to explain the different phases of
    operation.  Control flow through ataPrintMain()
    simplified.
- If reading device identity information fails, try seeing if the info
    can be accessed using a "DEVICE PACKET" command.  This way we can
    at least get device info.
- Modified Makefile to automatically tag CVS archive on issuance of
    a release
- Modified drive detection so minor device ID code showing ATA-3 rev
    0 (no SMART) is known to not be SMART capable.
- Now verify the checksum of the device ID data structure, and of the
    attributes threshold structure.  Before neither of these
    structures had their checksums verified.
- New behavior vis-a-vis checksums.  If they are wrong, we log
    warning messages to stdout, stderr, and syslog, but carry on
    anyway.  All functions now call a checksumwarning routine if the
    checksum doesn't vanish as it should.
- Changed Read Hard Disk Identity function to get fresh info from
    the disk on each call rather than to use the values that were read
    upon boot-up into the BIOS.  This is the biggest change in this
    release.  The ioctl(device, HDIO_GET_IDENTITY, buf ) call should
    be avoided in such code.  Note that if people get garbled strings
    for the model, serial no and firmware versions of their drives,
    then blame goes here (the BIOS does the byte swapping for you,
    apparently!)
- Function ataSmartSupport now looks at correct bits in drive
    identity structure to verify first that these bits are valid,
    before using them.
- Function ataIsSmartEnabled() written which uses the Drive ID state
    information to tell if SMART is enabled or not.  We'll carry this
    along for the moment without using it.
- Function ataDoesSmartWork() guaranteed to work if the device
    supports SMART.
- Replace some numbers by #define MACROS
- Wrote Function TestTime to return test time associated with each
    different type of test.
- Thinking of the future, have added a new function called
    ataSmartStatus2().  Eventually when I understand how to use the
    TASKFILE API and am sure that this works correctly, it will
    replace ataSmartStatus().  This queries the drive directly to
    see if the SMART status is OK, rather than comparing thresholds to
    attribute values ourselves. But I need to get some drives that fail
    their SMART status to check it.
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

