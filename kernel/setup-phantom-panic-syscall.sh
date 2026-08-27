#!/usr/bin/env bash

set -euo pipefail

ROOT="$HOME/phantom"
KERNEL="$ROOT/kernel"
PHANTOM_DIR="$KERNEL/phantom"
USERSPACE="$ROOT/userspace"
LIB="$USERSPACE/libphantom"
BOX="$USERSPACE/phantombox"

SYSCALL_TABLE="$KERNEL/arch/x86/entry/syscalls/syscall_64.tbl"
SYSCALL_NAME="phantom_panic"

echo "=== PHANTOM PANIC SYSCALL SETUP ==="

for path in "$KERNEL" "$PHANTOM_DIR" "$BOX" "$SYSCALL_TABLE"; do
    if [[ ! -e "$path" ]]; then
        echo "ERROR: missing: $path"
        exit 1
    fi
done

mkdir -p "$LIB"

echo "[1/7] Finding free x86-64 syscall number..."

USED_MAX=$(
    awk '
        /^[[:space:]]*[0-9]+[[:space:]]+(common|64)[[:space:]]/ {
            if ($1 < 512 && $1 > max)
                max=$1
        }
        END { print max }
    ' "$SYSCALL_TABLE"
)

if [[ -z "$USED_MAX" ]]; then
    echo "ERROR: could not determine syscall number."
    exit 1
fi

SYSCALL_NR=$((USED_MAX + 1))

# Never use the legacy x32 range.
if (( SYSCALL_NR >= 512 && SYSCALL_NR < 548 )); then
    SYSCALL_NR=548
fi

if grep -Eq "^[[:space:]]*$SYSCALL_NR[[:space:]]" "$SYSCALL_TABLE"; then
    echo "ERROR: syscall number $SYSCALL_NR is already used."
    exit 1
fi

if grep -Eq "[[:space:]]$SYSCALL_NAME[[:space:]]" "$SYSCALL_TABLE"; then
    echo "ERROR: $SYSCALL_NAME already exists in syscall table."
    exit 1
fi

echo "Using syscall number: $SYSCALL_NR"

echo "[2/7] Creating kernel syscall implementation..."

cat > "$PHANTOM_DIR/phantom_syscalls.c" <<SRC
// SPDX-License-Identifier: GPL-2.0
//
// Phantom userspace syscall interface
//

#include <linux/kernel.h>
#include <linux/syscalls.h>

SYSCALL_DEFINE0(phantom_panic)
{
    panic("Phantom: userspace requested kernel panic");
}
SRC

echo "[3/7] Adding phantom_syscalls.o to kernel build..."

MAKEFILE="$PHANTOM_DIR/Makefile"

touch "$MAKEFILE"

if ! grep -qE '^[[:space:]]*obj-y[[:space:]]*\\+=[[:space:]]*phantom_syscalls\\.o[[:space:]]*$' "$MAKEFILE"; then
    printf '\nobj-y += phantom_syscalls.o\n' >> "$MAKEFILE"
fi

echo "[4/7] Adding syscall prototype..."

SYS_HEADER="$KERNEL/include/linux/syscalls.h"

if ! grep -qE 'sys_phantom_panic' "$SYS_HEADER"; then
    printf '\nasmlinkage long sys_phantom_panic(void);\n' >> "$SYS_HEADER"
fi

echo "[5/7] Wiring syscall into x86-64 table..."

printf '%s common %s sys_%s\n' \
    "$SYSCALL_NR" \
    "$SYSCALL_NAME" \
    "$SYSCALL_NAME" >> "$SYSCALL_TABLE"

echo "[6/7] Creating libphantom..."

cat > "$LIB/phantom.h" <<'SRC'
#ifndef PHANTOM_H
#define PHANTOM_H

void phantom_panic(void);

#endif
SRC

cat > "$LIB/phantom.c" <<SRC
#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_phantom_panic
#define __NR_phantom_panic $SYSCALL_NR
#endif

void phantom_panic(void)
{
    syscall(__NR_phantom_panic);
}
SRC

cat > "$LIB/Makefile" <<'SRC'
CC = gcc
AR = ar

CFLAGS = -Wall -Wextra -O2 -static

TARGET = libphantom.a
OBJECT = phantom.o

all: $(TARGET)

$(TARGET): $(OBJECT)
	$(AR) rcs $@ $^

phantom.o: phantom.c phantom.h
	$(CC) $(CFLAGS) -c phantom.c -o phantom.o

clean:
	rm -f $(OBJECT) $(TARGET)
SRC

echo "[7/7] Updating phantombox..."

MAIN="$BOX/main.c"

if ! grep -q '#include "../libphantom/phantom.h"' "$MAIN"; then
    sed -i '1i#include "../libphantom/phantom.h"' "$MAIN"
fi

python3 - "$MAIN" <<'PY'
from pathlib import Path
import re
import sys

path = Path(sys.argv[1])
text = path.read_text()

old = '''if (strcmp(buf, "panic") == 0) {
        int fd;

        fd = open("/proc/phantom_panic", O_WRONLY);

        if (fd >= 0) {
            write(fd, "1", 1);
            close(fd);
        }
    }'''

new = '''if (strcmp(buf, "panic") == 0) {
        phantom_panic();
    }'''

if old in text:
    text = text.replace(old, new)
else:
    # If the old /proc implementation is absent, just insert a new command
    # before the existing poweroff handler.
    marker = 'if (strcmp(buf, "poweroff") == 0) {'
    if 'phantom_panic();' not in text and marker in text:
        block = '''if (strcmp(buf, "panic") == 0) {
        phantom_panic();
    }

    '''
        text = text.replace(marker, block + marker, 1)

path.write_text(text)
PY

echo
echo "=== DONE ==="
echo
echo "Syscall:"
echo "  $SYSCALL_NAME = $SYSCALL_NR"
echo
echo "Kernel:"
echo "  $PHANTOM_DIR/phantom_syscalls.c"
echo
echo "Library:"
echo "  $LIB/phantom.h"
echo "  $LIB/phantom.c"
echo "  $LIB/Makefile"
echo
echo "Next:"
echo "  cd $ROOT"
echo "  make -C kernel -j\"$(nproc)\" bzImage"
echo "  ./build_userspace.sh"
echo "  ./run-qemu.sh"
