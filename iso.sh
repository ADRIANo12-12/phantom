#!/usr/bin/env bash
#
# Phantom OS — buduje bootowalne ISO (GRUB2).
#
# Wymaga artefaktów z builds/latest (./build.sh) oraz narzędzi:
#   xorriso + grub2-tools-extra (Fedora) / grub-pc-bin (Debian).
#
# Użycie:
#   ./iso.sh                          # ISO z builds/latest
#   PHANTOM_OUT=path ./iso.sh         # ISO z wybranego katalogu
#   ./iso.sh /path/wyjście.iso        # jawna ścieżka wyjścia

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/scripts/common.sh"

phantom_require_cmds xorriso

# grub2-mkrescue (Fedora) albo grub-mkrescue (Debian/Ubuntu)
MKRESCUE="$(command -v grub2-mkrescue || command -v grub-mkrescue || true)"
[[ -n "$MKRESCUE" ]] || die "nie znaleziono grub2-mkrescue/grub-mkrescue."

OUT_DIR="${PHANTOM_OUT:-$LATEST_DIR}"
BZIMAGE="$OUT_DIR/bzImage"
INITRAMFS="$OUT_DIR/phantom-initramfs.cpio.gz"

[[ -f "$BZIMAGE" ]] || die "brak kernela: $BZIMAGE (uruchom ./build.sh)"
[[ -f "$INITRAMFS" ]] || die "brak initramfs: $INITRAMFS (uruchom ./build.sh)"

# Wersje do nazwy pliku
VERSION="$(phantom_version_read)"
KVER="$(phantom_kernel_short)"

ISO_PATH="${1:-$OUT_DIR/phantom-${VERSION}-k${KVER}.iso}"
ISO_PATH="$(realpath -m "$ISO_PATH")"

ISO_MOUNT_PT="boot/grub"  # grub2-mkrescue szuka tu grub.cfg

STAGE="$BUILDS_DIR/iso-stage"
rm -rf "$STAGE"
mkdir -p "$STAGE/boot/grub"

cp "$BZIMAGE"    "$STAGE/boot/bzImage"
cp "$INITRAMFS"  "$STAGE/boot/phantom-initramfs.cpio.gz"

cat > "$STAGE/boot/grub/grub.cfg" <<'GRUB'
set timeout=0
set default=0
set terminal=console
set gfxmode=text

menuentry "Phantom OS" {
	linux  /boot/bzImage console=tty0 earlycon=ttyS0 console=ttyS0 rdinit=/init
	initrd /boot/phantom-initramfs.cpio.gz
}
GRUB

info "Building ISO $VERSION (kernel $KVER)..."
info "Output: $ISO_PATH"

mkdir -p "$(dirname "$ISO_PATH")"

"$MKRESCUE" \
	--output "$ISO_PATH" \
	"$STAGE" >/dev/null

[[ -f "$ISO_PATH" ]] || die "ISO nie został wygenerowany."

ok "ISO ready:"
echo "  $(du -h "$ISO_PATH" | cut -f1)  $ISO_PATH"

rm -rf "$STAGE"