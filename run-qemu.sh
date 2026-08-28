#!/usr/bin/env bash
#
# Phantom OS — uruchomienie systemu w QEMU.
#
# QEMU jest ZAWSZE odpalany w trybie -nographic (serial console ttyS0).
#
# Użycie:
#   ./run-qemu.sh           # używa builds/latest
#   PHANTOM_OUT=path ./run-qemu.sh
#
# Opcjonalne: PHANTOM_QEMU_KVM=1 dla -enable-kvm.

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/scripts/common.sh"

phantom_require_cmds qemu-system-x86_64

OUT_DIR="${PHANTOM_OUT:-$LATEST_DIR}"
BZIMAGE="$OUT_DIR/bzImage"
INITRAMFS="$OUT_DIR/phantom-initramfs.cpio.gz"

# Fallback: artefakty wprost z katalogu kernel
if [[ ! -f "$BZIMAGE" ]]; then
	BZIMAGE="$(phantom_bzimage)"
fi
if [[ ! -f "$INITRAMFS" ]]; then
	INITRAMFS="$KERNEL_DIR/usr/phantom-initramfs.cpio.gz"
fi

[[ -f "$BZIMAGE" ]] || die "brak kernela: $BZIMAGE (uruchom ./build.sh)"
[[ -f "$INITRAMFS" ]] || die "brak initramfs: $INITRAMFS (uruchom ./build.sh)"

KVM_ARGS=()
if [[ "${PHANTOM_QEMU_KVM:-}" == "1" ]]; then
	KVM_ARGS=( -enable-kvm -cpu host )
fi

info "Phantom OS — QEMU (-nographic)"
echo "  Kernel   : $BZIMAGE"
echo "  Initramfs: $INITRAMFS"
echo "  Init     : /init"
echo "  Terminal : 80x24 zalecany rozmiar"
echo

exec qemu-system-x86_64 \
	"${KVM_ARGS[@]}" \
	-m 2G \
	-smp 2 \
	-kernel "$BZIMAGE" \
	-nic user,model=e1000 \
	-initrd "$INITRAMFS" \
	-append "console=tty0 earlycon=ttyS0 console=ttyS0 rdinit=/init" \
	-nographic