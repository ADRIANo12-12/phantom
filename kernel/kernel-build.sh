#!/usr/bin/env bash
set -e

KERNEL_DIR="$HOME/phantom/kernel"
LOG_FILE="$HOME/phantom/kernel-build.log"

START=$(date +%s)

printf '\033[2J\033[H'
echo "PHANTOM KERNEL BUILD"
echo

# Approximate total work items from the kernel's dry run.
TOTAL=$(make -C "$KERNEL_DIR" -n 2>/dev/null | wc -l)

if [ "$TOTAL" -le 0 ]; then
    TOTAL=1
fi

COUNT=0

make -C "$KERNEL_DIR" V=1 2>&1 |
while IFS= read -r line; do
    printf '%s\n' "$line" >> "$LOG_FILE"

    if [[ "$line" =~ ^[[:space:]]*(CC|HOSTCC|HOSTCXX|AS|LD|AR|GEN|LEX|YACC|CHK|MODPOST|UPD|OBJCOPY|INSTALL)[[:space:]] ]]; then
        ((COUNT++)) || true

        NOW=$(date +%s)
        ELAPSED=$((NOW - START))

        if [ "$COUNT" -ge "$TOTAL" ]; then
            PERCENT=100
        else
            PERCENT=$((COUNT * 100 / TOTAL))
        fi

        if [ "$COUNT" -gt 0 ] && [ "$ELAPSED" -gt 0 ]; then
            ETA=$((ELAPSED * (TOTAL - COUNT) / COUNT))
        else
            ETA=0
        fi

        BAR_SIZE=30
        FILLED=$((PERCENT * BAR_SIZE / 100))
        EMPTY=$((BAR_SIZE - FILLED))

        BAR=$(printf '%*s' "$FILLED" '' | tr ' ' '#')
        BAR+=$(printf '%*s' "$EMPTY" '' | tr ' ' '-')

        printf '\r[%s] %3d%% | ETA %02d:%02d:%02d' \
            "$BAR" \
            "$PERCENT" \
            $((ETA / 3600)) \
            $(((ETA % 3600) / 60)) \
            $((ETA % 60))
    fi
done

echo
echo "BUILD FINISHED"
echo "Log: $LOG_FILE"
