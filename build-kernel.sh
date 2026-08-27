#!/usr/bin/env bash

set -e

cd "$HOME/phantom/kernel"

make -j"$(nproc)" bzImage

echo
echo "================================"
echo " PHANTOM KERNEL BUILD COMPLETE"
echo "================================"
echo
echo "Kernel:"
echo "$HOME/phantom/kernel/arch/x86/boot/bzImage"
