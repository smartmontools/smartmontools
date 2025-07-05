# smartmontools Changelog

## smartmontools 8.0 (not yet released)

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

## smartmontools 5.0 to 7.5

Please see
[doc/old/NEWS-5.0-7.5.txt](https://github.com/smartmontools/smartmontools/blob/main/doc/old/NEWS-5.0-7.5.txt)
for older versions.
