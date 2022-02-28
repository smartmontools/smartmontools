#!/bin/bash -e

tmp=$(tempfile)
cat >$tmp

run-parts --report --lsbsysinit --arg=$tmp --arg="$1" \
    --arg="$2" --arg="$3" -- /etc/smartmontools/run.d

rm -f $tmp

