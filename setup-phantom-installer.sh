#!/usr/bin/env bash

set -euo pipefail

ROOT="$HOME/phantom"

USERSPACE="$ROOT/userspace"
PHANTOMBOX="$USERSPACE/phantombox"
INSTALLER="$USERSPACE/phantominstall"

ROOTFS="$ROOT/rootfs"

echo "=== PHANTOM INSTALLER SETUP ==="
echo

echo "[1/6] Building phantombox..."

if [ ! -f "$PHANTOMBOX/Makefile" ]; then
	echo "ERROR: phantombox Makefile not found:"
	echo "  $PHANTOMBOX/Makefile"
	exit 1
fi

cd "$PHANTOMBOX"

make clean
make

if [ ! -x "$PHANTOMBOX/phantombox" ]; then
	echo "ERROR: phantombox build failed."
	exit 1
fi

echo
echo "Phantombox:"
file "$PHANTOMBOX/phantombox"
echo

echo "[2/6] Building phatominstall..."

cd "$INSTALLER"

make clean
make

if [ ! -x "$INSTALLER/phatominstall" ]; then
	echo "ERROR: phatominstall build failed."
	exit 1
fi

echo
echo "Installer:"
file "$INSTALLER/phatominstall"
echo

echo "[3/6] Installing userspace binaries..."

mkdir -p "$ROOTFS/bin"

cp "$PHANTOMBOX/phantombox" "$ROOTFS/bin/phantombox"
cp "$INSTALLER/phatominstall" "$ROOTFS/bin/phatominstall"

chmod +x \
	"$ROOTFS/bin/phantombox" \
	"$ROOTFS/bin/phatominstall"

echo
echo "Installed:"
ls -lh \
	"$ROOTFS/bin/phantombox" \
	"$ROOTFS/bin/phatominstall"
echo

echo "[4/6] Checking /init..."

if [ ! -x "$ROOTFS/init" ]; then
	echo "ERROR: missing executable /init:"
	echo "  $ROOTFS/init"
	exit 1
fi

echo "OK: $ROOTFS/init"
echo

echo "[5/6] Building initramfs..."

OUT="$ROOT/kernel/usr/phantom-initramfs.cpio.gz"

mkdir -p "$(dirname "$OUT")"

cd "$ROOTFS"

find . -print0 \
	| cpio --null -ov --format=newc 2>/dev/null \
	| gzip -9 > "$OUT"

echo
echo "Initramfs:"
ls -lh "$OUT"
echo

echo "[6/6] Verifying initramfs..."

TMP="/tmp/phantom-installer-check"

rm -rf "$TMP"
mkdir -p "$TMP"

cd "$TMP"

zcat "$OUT" \
	| cpio -id 2>/dev/null

echo
echo "Initramfs contains:"
ls -lh \
	"$TMP/init" \
	"$TMP/bin/phantombox" \
	"$TMP/bin/phatominstall"

echo
echo "=== PHANTOM INSTALLER READY ==="
echo
echo "Start:"
echo "  cd ~/phantom"
echo "  ./run-qemu.sh"
echo
echo "Then:"
echo "  ~/PhantomOS-$ install"
echo
