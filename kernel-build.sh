#!/usr/bin/env bash

set -o pipefail

KERNEL="$HOME/phantom/kernel"
LOG="$HOME/phantom/kernel-build.log"

rm -f "$LOG"

clear
echo "PHANTOM KERNEL BUILD"
echo

TOTAL=$(make -C "$KERNEL" -n 2>/dev/null | \
    grep -E '^[[:space:]]*(CC|HOSTCC|HOSTCXX|AS|LD|AR|GEN|LEX|YACC|CHK|MODPOST|UPD|OBJCOPY)[[:space:]]' | \
    wc -l)

if (( TOTAL == 0 )); then
    echo "Nie znaleziono zadan do wykonania."
    echo "Sprawdzam normalny build..."
    exec make -C "$KERNEL"
fi

COUNT=0
START=$(date +%s)

stdbuf -oL -eL make -C "$KERNEL" V=1 2>&1 | \
stdbuf -oL -eL awk -v total="$TOTAL" -v start="$START" -v log="$LOG" '
{
    print >> log

    if ($0 ~ /^[[:space:]]*(CC|HOSTCC|HOSTCXX|AS|LD|AR|GEN|LEX|YACC|CHK|MODPOST|UPD|OBJCOPY)[[:space:]]/) {
        count++

        percent = int((count / total) * 100)
        if (percent > 100)
            percent = 100

        now = systime()
        elapsed = now - start

        if (count > 0)
            eta = int(elapsed * (total - count) / count)
        else
            eta = 0

        filled = int(percent * 30 / 100)

        bar = ""
        for (i = 0; i < filled; i++)
            bar = bar "#"

        for (i = filled; i < 30; i++)
            bar = bar "-"

        printf "\r[%s] %3d%% | ETA %02d:%02d:%02d",
               bar,
               percent,
               int(eta / 3600),
               int((eta % 3600) / 60),
               eta % 60

        fflush()
    }
}

END {
    print ""
}
'

STATUS=${PIPESTATUS[0]}

echo

if (( STATUS == 0 )); then
    echo "BUILD FINISHED"
else
    echo "BUILD FAILED"
    echo "Log: $LOG"
fi

exit "$STATUS"
