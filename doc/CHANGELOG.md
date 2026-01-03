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

- The NVMe/SAT autodetection enabled by the options `-d snt*/sat` now also checks for nonempty
NVMe identify controller data before assuming that a NVMe device is connected.

- Linux: `smartctl --scan -d by-id` and `DEVICESCAN -d by-id` now also include `/dev/by-id`
links to NVMe devices.
Duplicates, including multiple namespaces, are removed and option `-d nvme,0xffffffff` is set.

- Linux: `smartctl --scan -d TYPE` and `DEVICESCAN -d TYPE` no longer include devices
behind `megaraid` or `sssraid` controllers if `TYPE` is `sat` and/or `scsi`.
The new scan options `-d megaraid` and `-d sssraid` have been added to include these.
If no `-d TYPE` option is specified, these controllers are included as before.

- HDD, SSD and USB entries have been added to the drive database.

- `update-smart-drivedb`: the expiration date of the `drivedb.h` signing key (key ID
[721042C5](https://keyserver.ubuntu.com/pks/lookup?search=0x721042C5&fingerprint=on&op=vindex))
has been enhanced to 2030-12-31.

- Windows: `update-smart-drivedb.ps1`: now also works with the Cygwin and MSYS2 versions of
`gpg`.
One of these versions is assumed if the `cygpath` tool is in present in the same directory.

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

### What changed

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

- Windows: `configure`: `LDFLAGS=-static -Wl,--nxcompat,--tsaware` is no longer automatically
set for MinGW-w64 builds.
The new option `--enable-static-link` is now required to build the Windows installer.

- The handling of unaligned integers in data structures has been reworked.
Most `packed` structure attributes are no longer needed and have been removed.

### Bug fixes

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
for older versions.
