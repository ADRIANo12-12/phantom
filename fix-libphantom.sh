#!/usr/bin/env bash

set -euo pipefail

ROOT="$HOME/phantom"
LIB="$ROOT/userspace/libphantom"
BOX="$ROOT/userspace/phantombox"

echo "=== FIX LIBPHANTOM ==="

mkdir -p "$LIB"

cat > "$LIB/phantom.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_H
#define PHANTOM_H

#include "phantom_osd.h"

void phantom_panic(void);

#endif
SRC

cat > "$LIB/phantom.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_phantom_panic
#define __NR_phantom_panic 473
#endif

void phantom_panic(void)
{
	(void)syscall(__NR_phantom_panic);
}
SRC

cat > "$LIB/Makefile" <<'SRC'
CC = gcc
AR = ar

CFLAGS = -Wall -Wextra -O2 -static

TARGET = libphantom.a

OBJECTS = \
	phantom.o \
	phantom_osd.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

phantom.o: phantom.c phantom.h
	$(CC) $(CFLAGS) -c phantom.c -o phantom.o

phantom_osd.o: phantom_osd.c phantom_osd.h phantom_osd_uapi.h
	$(CC) $(CFLAGS) -c phantom_osd.c -o phantom_osd.o

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
SRC

echo "[1/4] Building libphantom..."
cd "$LIB"
make clean
make

echo
echo "[2/4] Checking panic symbol..."
nm "$LIB/libphantom.a" | grep ' phantom_panic$'

echo
echo "[3/4] Building phantombox..."
cd "$BOX"
make clean
make

echo
echo "[4/4] Verifying..."
file "$BOX/phantombox"
file "$LIB/libphantom.a"

echo
echo "=== DONE ==="
echo
echo "Next:"
echo
echo "  cd ~/phantom"
echo "  ./build_userspace.sh"
echo
