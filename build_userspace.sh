#!/bin/bash
set -e

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
USERSPACE="$PROJECT_DIR/userspace/phantombox"
ROOTFS="$PROJECT_DIR/rootfs"
OUT="$PROJECT_DIR/kernel/usr"

echo "=== PHANTOM USERSPACE BUILD ==="

echo "[1/4] Kompilowanie phantombox..."
cd "$USERSPACE"

make clean
make

echo "[2/4] Kopiowanie phantombox do rootfs..."
mkdir -p "$ROOTFS/bin"
cp phantombox "$ROOTFS/bin/phantombox"
chmod +x "$ROOTFS/bin/phantombox"

echo "[3/4] Sprawdzanie /init..."

if [ ! -x "$ROOTFS/init" ]; then
    echo "ERROR: $ROOTFS/init nie istnieje albo nie jest wykonywalny!"
    exit 1
fi

echo "[4/4] Budowanie initramfs..."

mkdir -p "$OUT"

cd "$ROOTFS"

find . -print0 | cpio --null -ov --format=newc 2>/dev/null \
    | gzip -9 > "$OUT/phantom-initramfs.cpio.gz"

echo
echo "================================"
echo " PHANTOM USERSPACE READY"
echo "================================"
echo

echo "Phantombox:"
file "$ROOTFS/bin/phantombox"

echo
echo "Initramfs:"
echo "  $OUT/phantom-initramfs.cpio.gz"
echo
echo "Phantombox:"
echo "  $ROOTFS/bin/phantombox"
