#!/usr/bin/env bash

set -e

ROOT="$HOME/phantom"
KERNEL="$ROOT/kernel"
IMAGE="$KERNEL/arch/x86/boot/bzImage"
INITRAMFS="$KERNEL/usr/phantom-initramfs.cpio.gz"

if [ ! -f "$IMAGE" ]; then
    echo "ERROR: kernel not found:"
    echo "  $IMAGE"
    exit 1
fi

if [ ! -f "$INITRAMFS" ]; then
    echo "ERROR: initramfs not found:"
    echo "  $INITRAMFS"
    exit 1
fi

echo "=== PHANTOM QEMU ==="
echo "Kernel:    $IMAGE"
echo "Initramfs: $INITRAMFS"
echo "Init:      /init"
echo

exec qemu-system-x86_64 \
    -m 2G \
    -smp 2 \
    -kernel "$IMAGE" \
    -nic user,model=e1000 \
    -initrd "$INITRAMFS" \
    -append "console=ttyS0 earlycon=ttyS0 rdinit=/init" \
    -nographic
    
