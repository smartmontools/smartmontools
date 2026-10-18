## About Smartmontools
The smartmontools package contains two utility programs (`smartctl` and `smartd`) 
to control and monitor storage systems using the **Self-Monitoring, Analysis and 
Reporting Technology System** (SMART) built into most modern ATA/SATA, SCSI/SAS and NVMe disks. 
In many cases, these utilities will provide advanced warning of disk degradation and failure.

Smartmontools was originally derived from the Linux [smartsuite package](https://sourceforge.net/projects/smartsuite/) and supports ATA/SATA, SCSI/SAS, and NVMe disks and also SCSI/SAS tape devices.
It should run on any modern Linux, FreeBSD, NetBSD, OpenBSD, Darwin (macOS), Solaris, Windows, Cygwin, OS/2, eComStation, or QNX system.
Smartmontools can also be run from one of many different Live CDs/DVDs.

## Windows binaries
The Windows installer offers x86 (32-bit) and x86_64 (64-bit) program sets. These are architecture choices; the current installer does not offer separate XP/NT5 and NT6 builds.

`smartctl-nc.exe` is a non-console build of `smartctl` provided for GSmartControl. The installer also uses `runcmdu.exe` for Start Menu example shortcuts and `runcmda.exe` for Explorer drive-menu commands. Both helpers run a console command and wait for Enter; `runcmda.exe` requests administrator privileges.

`smartd_warning.cmd` uses `wtssendmsg.exe` to display `smartd` warning message boxes on Windows sessions when the first warning destination is `console`, `active`, or `connected`.

## Important links
- [Project homepage](https://www.smartmontools.org/)
- [GitHub repository](https://github.com/smartmontools/smartmontools)
- [CI builds](https://github.com/smartmontools/smartmontools-builds/releases)
- [Project Releases](https://github.com/smartmontools/smartmontools/releases)


## Code Signing
This program uses free code signing provided by [SignPath.io](https://signpath.io) and a free code signing certificate by the [SignPath Foundation](https://signpath.org)

## License
Smartmontools uses [GNU GPL Version 2](https://www.gnu.org/licenses/gpl-2.0.html#SEC1) license. 
