#!/usr/bin/env bash

set -o pipefail

ROOT="$HOME/phantom"
KERNEL="$ROOT/kernel"
LOG="$ROOT/kernel-build.log"
RUN="$ROOT/run-qemu.sh"

CPU="$(nproc)"
START="$(date +%s)"

rm -f "$LOG"

clear
printf '\n'
printf 'PHANTOM KERNEL BUILD\n'
printf '====================\n\n'

# Make sure .config exists.
if [[ ! -f "$KERNEL/.config" ]]; then
    echo "ERROR: $KERNEL/.config does not exist."
    echo
    echo "Create a kernel configuration first, for example:"
    echo "  make -C $KERNEL x86_64_defconfig"
    exit 1
fi

echo "Building: arch/x86/boot/bzImage"
echo "Jobs:     $CPU"
echo "Log:      $LOG"
echo

# Estimate total work from a dry run.
TOTAL="$(
    make -C "$KERNEL" -n bzImage -j"$CPU" 2>/dev/null |
    grep -E '(^|[[:space:]])(CC|HOSTCC|HOSTCXX|AS|LD|AR|GEN|LEX|YACC|CHK|MODPOST|UPD|OBJCOPY)[[:space:]]' |
    wc -l
)"

(( TOTAL > 0 )) || TOTAL=1

COUNT_FILE="$(mktemp)"
printf '0\n' > "$COUNT_FILE"

cleanup() {
    rm -f "$COUNT_FILE"
}
trap cleanup EXIT

make -C "$KERNEL" bzImage -j"$CPU" V=1 2>&1 |
while IFS= read -r line; do
    printf '%s\n' "$line" >> "$LOG"

    case "$line" in
        *" CC "*|*" HOSTCC "*|*" HOSTCXX "*|*" AS "*|*" LD "*|*" AR "*|*" GEN "*|*" LEX "*|*" YACC "*|*" CHK "*|*" MODPOST "*|*" UPD "*|*" OBJCOPY "*)
            COUNT=$(( $(cat "$COUNT_FILE") + 1 ))
            printf '%s\n' "$COUNT" > "$COUNT_FILE"

            NOW="$(date +%s)"
            ELAPSED=$((NOW - START))

            PERCENT=$((COUNT * 100 / TOTAL))
            (( PERCENT > 100 )) && PERCENT=100

            if (( COUNT > 0 && ELAPSED > 0 )); then
                ETA=$((ELAPSED * (TOTAL - COUNT) / COUNT))
                (( ETA < 0 )) && ETA=0
            else
                ETA=0
            fi

            WIDTH=30
            FILLED=$((PERCENT * WIDTH / 100))
            EMPTY=$((WIDTH - FILLED))

            BAR="$(printf '%*s' "$FILLED" '' | tr ' ' '#')"
            BAR+="$(printf '%*s' "$EMPTY" '' | tr ' ' '-')"

            # Save cursor, move to bottom, redraw progress, restore cursor.
            printf '\033[s'
            printf '\033[999;1H'
            printf '\033[2K'
            printf '[%s] %3d%% | ETA %02d:%02d:%02d' \
                "$BAR" \
                "$PERCENT" \
                $((ETA / 3600)) \
                $(((ETA % 3600) / 60)) \
                $((ETA % 60))
            printf '\033[u'
            ;;
    esac
done

STATUS=${PIPESTATUS[0]}

# Final progress line.
printf '\033[s'
printf '\033[999;1H'
printf '\033[2K'

if (( STATUS == 0 )); then
    printf '[##############################] 100%% | BUILD OK'
else
    printf '[##############################] FAIL | see %s' "$LOG"
fi

printf '\033[u'
printf '\n\n'

if (( STATUS != 0 )); then
    echo "KERNEL BUILD FAILED"
    echo "Log: $LOG"
    exit "$STATUS"
fi

IMAGE="$KERNEL/arch/x86/boot/bzImage"

if [[ ! -f "$IMAGE" ]]; then
    echo "ERROR: bzImage was not created."
    exit 1
fi

echo "KERNEL BUILD COMPLETE"
echo "Kernel: $IMAGE"
echo
echo "Starting QEMU..."
echo

exec "$RUN"
