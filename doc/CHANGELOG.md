# smartmontools Changelog

## smartmontools 8.0 (not yet released)

### What's new

- `libsmartmon`: work on a smartmontools library has begun.
Possibly reusable files have been moved from `src` to new directories `lib` and
`include/smartmon`.
Library symbols have been moved to `namespace smartmon`.
Symbols referenced from the library but located in `smartctl` and `smartd` have been
replaced by library hooks.
Source code examples have been added to the new directory `lib/examples`.
Work towards a more consistent and flexible API is in progress.

- `smartctl -j`: the new JSON values `host_reads: {...}` and `host_writes: {...}` have been
added for ATA and NVMe.

- `smartctl -j`: the new JSON values `scsi_self_test_status: {...}` have been added for SCSI.

- `smartctl -j`: the new JSON values `thermal_mgmt_temperature_*` have been added to the
structure `nvme_smart_health_information_log`.

- `smartctl [-j] -l devstat`: informal strings for the `Device Statistics` values
`Logical Sectors Read/Written` and `Date and Time Stamp` have been added.

- `smartd`: the new command line option `-j PREFIX, --jsonstate=PREFIX` has been added to write
per-device JSON state files after each successful check cycle.
The JSON syntax is aligned with `smartctl`.
This lets external tools read cached health data without spawning `smartctl` for each device.
See also `configure --with-jsonstate` below.

- ATA/RAID: device types `-d jmb39x*,...` and `-d jms56x,...`: limited support for NO DATA, DATA
OUT and 48-bit ATA commands has been added.
This enables usage of `smartctl` options like
`-t TEST -l selftest -l scttemp -l directory -l sataphy`
and of `smartd.conf` directives `-s REGEXP -l selftest`.

- USB/NVMe/SAT: device types `-d snt*/sat`: the NVMe/SAT autodetection enabled by these options
now also checks for nonempty NVMe identify controller data before assuming that a NVMe device is
connected.

- Linux: a check of runtime power management has been added for ATA, SCSI and NVMe devices.
If the `-n standby` option or directive is specified, `/sys/.../device/power/control` and
`/sys/.../device/power/runtime_status` are checked before opening the device.
If a `suspending` or `suspended` runtime status is indicated, the device is not accessed.
This could prevent that a device open or a pass-through call that checks the actual powermode
spins up a disk.

- Linux: `smartctl --scan -d by-id` and `DEVICESCAN -d by-id` now also include
`/dev/disk/by-id` links to NVMe devices.
Duplicates, including multiple namespaces, are removed.
The option `-d nvme,0xffffffff` is always set to ensure that the broadcast namespace is
monitored.

- Linux: `smartctl --scan -d TYPE` and `DEVICESCAN -d TYPE` no longer include devices
behind `megaraid` or `sssraid` controllers if `TYPE` is `sat` and/or `scsi`.
The new scan options `-d megaraid` and `-d sssraid` have been added to include these.
If no `-d TYPE` option is specified, these controllers are included as before.

- Linux: `smartctl --scan` and `DEVICESCAN` now also scans for "hidden" disks behind `/dev/sg*`
devices which do not have an associated `/dev/sd*` device.
This detects physical disks of RAID volumes if the controller uses the mptsas/mpt3sas driver.
It may also work with other drivers.
For now, this experimental feature needs to be enabled with the scan option `-d sg`.

- Windows installer: a GUI option and a command line flag (`/SO ...,syspath,...`) to add the
installation directory to the System `PATH` instead of the User `PATH` has been added.

- Windows installer: signed versions of the installer now also contain signed versions of
all `*.exe` and `*.ps1` files.

- HDD, SSD and USB entries have been added to the drive database.

- `update-smart-drivedb`: the expiration date of the `drivedb.h` signing key (key ID
[721042C5](https://keyserver.ubuntu.com/pks/lookup?search=0x721042C5&fingerprint=on&op=vindex))
has been enhanced to 2030-12-31.

- Windows: `update-smart-drivedb.ps1`: now also works with the Cygwin and MSYS2 versions of
`gpg`.
One of these versions is assumed if the `cygpath` tool is present in the same directory.

- `configure`: the new option `--with-jsonstate` has been added to specify a default path for the
new `smartd` command line option `-j PREFIX, --jsonstate=PREFIX`.

- `configure`: the new option `--enable-static-link` has been added to enable static linkage.
It replaces `LDFLAGS=-static` which no longer works due to the usage of `libtool`.

- `configure`: the new option `--without-devel` has been added to disable the installation of
`libsmartmon` development files.

- `configure`: the new options `--with-devel-src` and `--with-devel-bin` have been added
to configure the install path (or disable the installation) of the library example sources
and binaries.

- `configure`: the new option `--with-smartd-scripts` has been added as a replacement for the
now misleading option `--with-exampledir`.
The latter could still be used but will be removed in a future version of smartmontools.

- `configure`: the new option `--with-build-info='(TEXT)'` has been added to specify the build
information printed in the first output line.
It is no longer needed to use `make BUILD_INFO='"(TEXT)"'` but still possible.

- Reproducible builds: if `SOURCE_DATE_EPOCH` is passed to `configure`, it is now also
exported to the environment during `make`.
This requires that `make` supports `export` (GNU make) or `.export` (BSD make).
`configure` prints a warning if this is not the case.

### What changed

- NVMe: it is now assumed that the NVMe Error Information log is missing if only one entry is
reported.
This log is mandatory but some (USB-)devices which emulate NVMe SMART/Health Information do not
provide it.
The decision could be overridden (only) for `smartctl` with the option `-l error,1`.

- `smartd`: no longer ignores the signals `SIGINT`, `SIGQUIT`, `SIGHUP`, `SIGTERM` and
`SIGUSR1` if ignored at startup.

- The version information string of pre-release builds now uses the form `pre-X.Y-NNN` where
`X.Y` is the version of the next release and `NNN` is the number of git commits since the
previous release.
The VERSIONINFO resources of Windows executables are set to `X.Y.0.NNN` for pre-releases
or to `X.Y.0.1000` for releases.

- The drive database branches now use git branch names `drivedb/X.Y`.

- `update-smart-drivedb`: no longer creates `drivedb.h.raw` files.

- `update-smart-drivedb`: no longer contains the signing key for drive database branches before
`7.0`.

- The source tree has been reorganized.
The source directory `smartmontools` has been renamed to `src`.
See `libsmartmon` above for further changes.

- `configure`: now fails if the option `--without-nvme-devicescan` is used.
NVMe device scanning is now always enabled by default if supported.

- Windows: `configure`: `LDFLAGS=-static -Wl,--nxcompat,--tsaware` is no longer automatically
set for MinGW-w64 builds.
The new option `--enable-static-link` is now required to build the Windows installer.

- The handling of unaligned integers in data structures has been reworked.
Most `packed` structure attributes are no longer needed and have been removed.

### Bug fixes

- `smartctl`: SCSI: fixed a possible stack buffer overflow via bogus result from Supported Log
Pages request.

- `smartd`: SCSI: fixed a possible stack out-of-bounds read via bogus result from Supported Log
Pages request.

- `smartctl -c`: no longer prints bogus NVMe `Namespace Features` if no namespace is
available.

- `smartctl -j`: no longer outputs invalid UTF-8 sequences in strings.

- `smartctl --json=y`: no longer leaves some reserved YAML strings unquoted.

- `smartctl -l farm`: fixed null pointer dereference on unknown form factor value.

- `smartctl -l farm`: fixed the output format of the fields `ATA Security State` and
`ATA Features Supported`.

- `smartctl -l farm`: SCSI: no longer suggests to use `-a` or `-x`.

- `smartd`: fixed the syntax of NVMe self-test related log messages.

- Windows: `update-smart-drivedb.ps1`: fixed failure of the rename command if the output path
is not a plain filename.

### Moved to GitHub (2025-06-01)

- More than 22 years after the first CVS commit
([4f7b211e3c3a](https://github.com/smartmontools/smartmontools/commit/4f7b211e3c3a))
at [SourceForge](https://sourceforge.net/p/smartmontools/code/HEAD/tree/trunk/), the official
upstream repository has been moved to [GitHub](https://github.com/smartmontools/smartmontools).

- The custom `svn2git` conversion script mapped the svn directories to git branches as follows:  
`trunk -> svn/trunk`,  `branches/* -> svn/branches/*`, `tags/* -> svn/tags/*`.

- The new development branch `main` started at `svn/trunk`
([fe3e09d3fe9a](https://github.com/smartmontools/smartmontools/commit/fe3e09d3fe9a)).

- The independent branch `master` and other branches from previous R/O mirroring of svn
to GitHub have been retired and are subject to remove.

- The drive database branches
[7.0](https://sourceforge.net/p/smartmontools/code/HEAD/tree/branches/RELEASE_7_0_DRIVEDB/) to
[7.5](https://sourceforge.net/p/smartmontools/code/HEAD/tree/branches/RELEASE_7_5_DRIVEDB/) of
the old svn repository at SourceForge will be updated in the future.
This will keep the `update-smart-drivedb` scripts of these versions functional.

## smartmontools 5.0 (2002-10-10) to 7.5 (2025-04-30)

Please see
[doc/old/NEWS-5.0-7.5.txt](https://github.com/smartmontools/smartmontools/blob/main/doc/old/NEWS-5.0-7.5.txt)
for older releases.
