#!/bin/bash
#
# Set file modification times to git commit time
#
# Home page of code is: https://www.smartmontools.org
#
# Copyright (C) 2026 Christian Franke
#
# SPDX-License-Identifier: GPL-2.0-or-later
#

set -e
myname=$0

usage()
{
  cat <<EOF
Set file modification times to git commit times

Usage: $myname [-N] [-a] [-f] [-n] [-q] [-v] [FILE...]

  -N          Limit git log to N entries
  -a          Use author date instead of commit date
  -f          Follow renames (slow)
  -n          Do not touch any file (dry-run)
  -o          Do not touch older files
  -q          Do not output 'touch' commands (quiet)
  -v          Verbose output
EOF
  exit 1
}

# Parse options
depth=
who=c
follow=false
dryrun=false
keepold=false
quiet=false
verbose=false
while :; do case $1 in
  -[0-9]*) [[ $1 =~ -[0-9]+ ]] || usage; depth=$1 ;;
  -a) who=a ;;
  -f) follow=true ;;
  -n) dryrun=true ;;
  -o) keepold=true ;;
  -q) quiet=true; verbose=false ;;
  -v) verbose=true; quiet=false ;;
  -*) usage ;;
  *) break ;;
esac; shift; done

vecho()
{
  ! $verbose || echo "$*"
}

# Check 'date(1)' command
date="date"
if ! "$date" -Iseconds -u -r . >/dev/null 2>&1; then
  if $keepold; then
    echo "$myname: '-o' requires a BSD or GNU version of 'date(1)'" >&2
    exit 1
  fi
  vecho "# Missing BSD or GNU version of 'date(1)'"
  date=
fi

# Change to top level directory
x=$(git rev-parse --show-toplevel) || exit 1
if [ "$x" != "$(pwd)" ]; then
  vecho cd "'$x'"
  cd "$x" || exit 1
fi

# Get list of tracked files
declare -A files
x=$(git ls-files "$@") || exit 1
while read -r f; do
  case $f in
    *[\ \"]*) vecho "# '$f': ignored (unsupported filename)"; continue ;;
  esac
  test -f "$f" || continue
  ! test -L "$f" || continue
  files["$f"]=$f
done <<<"$x"

# Exclude uncommitted files
x=$(git status -s "$@") || exit 1
while read -r a f; do
  case $f in *[\ \"]*) continue ;; esac
  f=${f/* -> /}
  test -n "$f" || continue
  test ${files[$f]+y} || continue
  vecho "# '$f': ignored (not committed)";
  unset 'files[$f]'
done <<<"$x"

test ${#files[*]} != 0 || exit 0

# Set 'git log' command line
gitlog=(git log --name-status --date=iso-strict-local --format="format:%${who}d")
test -z "$depth" || gitlog+=("$depth")
if [ $# != 0 ] && ! $follow; then
  gitlog+=(-- "$@")
elif [ $# = 1 ] && $follow; then
  gitlog+=(--follow -- "$1")
fi
vecho "# ${gitlog[*]}"

# Parse the 'git log' output
TZ='' LC_ALL=C "${gitlog[@]}" | \
(
  do_touch() # TIMESTAMP FILE ORIGIN
  {
    local s=
    if [ -z "$3" ]; then
      s=" origin: unknown"
    elif [ "$2" != "$3" ]; then
      s=" origin: '$3'"
    fi
    if [ -n "$date" ]; then
      local t
      # "./" prevents that BSD 'date(1)' interprets the filename as seconds
      t=$("$date" -Iseconds -u -r "./$2")
      t="${t%+00:00}Z"
      if [ "$t" = "$1" ]; then
        vecho ": touch -d '$1' '$2' #$s (already done)"
        return 0
      elif $keepold && [ "$t" '<' "$1" ]; then
        vecho ": touch -d '$1' '$2' #$s (older: $t)"
        return 0
      fi
    fi
    $quiet || echo "touch -d '$1' '$2'${s:+ #}$s"
    # 'touch -d "yyyy-mm-ddThh:mm:ssZ"' is part of POSIX 1003.1-2018
    $dryrun || touch -d "$1" "$2" || exit 1
  }

  ignored=; ts=; tsold=
  while read -r x; do
    test -z "$ignored" || vecho "# git log: ignored: '$ignored'"
    ignored=$x

    case $x in
      '') # End of log entry
        ignored=; ts=; continue
        ;;
      *[\;\"]*) # Unexpected char or quoted filename
        continue
        ;;
    esac

    IFS=$'\t' read -r a b c d <<<"$x"

    case "$a;${b:+y};${c:+y};${d:+y}" in
      2*-*-*T*:*:*Z\;\;\;) # Commit timestamp
        ignored=; ts=$a; tsold=$a; continue
        ;;
      *)
        test -n "$ts" || continue
        ;;
    esac

    ! $follow || case "$a:${b:+y}:${c:+y}:${d:+y}" in
      R100:y:y:) # Renamed without modifications => update name map
        ignored=
        f=${files["$c"]}
        test -n "$f" || continue
        vecho "# mv '$b' '$c' # current: '$f'"
        unset 'files[$c]'
        files["$b"]=$f
        continue
        ;;
    esac

    case "$a;${b:+y};${c:+y};${d:+y}" in
      [AM]\;y\;\;) # Added or modified => touch
        ignored=
        test ${files["$b"]+y} || continue
        f=$b
        ;;
      R[0-9]*\;y\;y\;) # Renamed and (modified or !$follow) => touch
        ignored=
        test ${files["$c"]+y} || continue
        f=$c
        ;;
      D\;y\;\;) # Deleted => ignore
        ignored=
        continue
        ;;
      *) # Unknown status
        continue
        ;;
    esac

    ft=${files["$f"]}
    test -f "$ft" || continue
    do_touch "$ts" "$ft" "$f" || exit 1
    unset 'files[$f]'
    test ${#files[*]} -ne 0 || break
  done

  # Use oldest timestamp for remaining files
  test -z "$tsold" || for f in ${!files[*]}; do
    ft=${files["$f"]}
    test -f "$ft" || continue
    do_touch "$tsold" "$ft" || exit 1
    unset 'files[$f]'
  done

  # List untouched files
  ! $verbose || for f in ${!files[*]}; do
    vecho "# unknown: '$f' (current: '${files["$f"]}')"
  done
) || exit 1
s=${PIPESTATUS[0]}

# Assume SIGPIPE if 'git log' exit status is >= 128
test "$s" = 0 || test "$s" -ge 128 || exit 1
