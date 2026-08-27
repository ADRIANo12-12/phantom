#!/usr/bin/env bash

set -e

ROOT="$HOME/phantom"
KERNEL="$ROOT/kernel"
IMAGE="$KERNEL/arch/x86/boot/bzImage"

if [[ ! -f "$IMAGE" ]]; then
    echo "ERROR: kernel image not found:"
    echo "  $IMAGE"
    echo
    echo "Build it first with:"
    echo "  ./build-kernel.sh"
    exit 1
fi

exec qemu-system-x86_64 \
    -m 2G \
    -smp 2 \
    -kernel "$IMAGE" \
    -append "console=ttyS0 earlycon=ttyS0" \
    -nographic
