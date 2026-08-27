#!/bin/bash

rm -f phantomsysinfo.o .phantomsysinfo.o.cmd

tmp=$(mktemp)

make -C .. phantom/phantomsysinfo.o >"$tmp" 2>&1
status=$?

if [ $status -ne 0 ]; then
    grep -E 'error:|fatal error:|warning:' "$tmp"
fi

rm -f "$tmp"

exit $status
