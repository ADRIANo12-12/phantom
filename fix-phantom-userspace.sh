#!/usr/bin/env bash

set -euo pipefail

ROOT="$HOME/phantom"
LIB="$ROOT/userspace/libphantom"
BOX="$ROOT/userspace/phantombox"

echo "=== FIX PHANTOM USERSPACE ==="

mkdir -p "$LIB"

echo "[1/5] Creating libphantom..."

cat > "$LIB/phantom.h" <<'SRC'
#ifndef PHANTOM_H
#define PHANTOM_H

void phantom_panic(void);

#endif
SRC

cat > "$LIB/phantom.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <sys/syscall.h>
#include <unistd.h>

/*
 * Must match the syscall number used by the Phantom kernel.
 */
#ifndef __NR_phantom_panic
#define __NR_phantom_panic 548
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
OBJECTS = phantom.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

phantom.o: phantom.c phantom.h
	$(CC) $(CFLAGS) -c phantom.c -o phantom.o

clean:
	rm -f $(OBJECTS) $(TARGET)
SRC

echo "[2/5] Building libphantom..."

cd "$LIB"
make clean
make

echo "[3/5] Replacing phantombox Makefile..."

cat > "$BOX/Makefile" <<'SRC'
CC = gcc

CFLAGS = -Wall -Wextra -O2 -static

TARGET = phantombox

SOURCES = main.c
OBJECTS = main.o

LIBDIR = ../libphantom
LIB = $(LIBDIR)/libphantom.a

all: $(TARGET)

$(TARGET): $(OBJECTS) $(LIB)
	$(CC) $(CFLAGS) $(OBJECTS) $(LIB) -o $(TARGET)

main.o: main.c
	$(CC) $(CFLAGS) -I$(LIBDIR) -c main.c -o main.o

$(LIB):
	$(MAKE) -C $(LIBDIR)

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all clean
SRC

echo "[4/5] Building phantombox..."

cd "$BOX"
make clean
make

echo
echo "Phantombox:"
file "$BOX/phantombox"

echo
echo "libphantom:"
file "$LIB/libphantom.a"

echo "[5/5] Done."

echo
echo "=== USERSPACE FIXED ==="
echo
echo "Now run:"
echo
echo "  cd $ROOT"
echo "  ./setup-phantom-installer.sh"
echo
