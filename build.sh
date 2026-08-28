#!/usr/bin/env bash
#
# Phantom OS — build kernela + userspace + initramfs.
#
# Użycie:
#   ./build.sh              # build do builds/latest
#   PHANTOM_OUT=path ./build.sh
#
# Wymagania: gałąź master, czyste drzewo robocze.

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/scripts/common.sh"

phantom_require_cmds make cpio gzip
phantom_require_branch "$DEV_BRANCH"

JOBS="${JOBS:-$(nproc)}"
OUT_DIR="${PHANTOM_OUT:-$LATEST_DIR}"

phantom_version_load
VERSION="$(phantom_version_full)"

info "Building Phantom ${VERSION} (kernel $(phantom_kernel_version))"
info "Jobs: $JOBS"
info "Output: $OUT_DIR"

mkdir -p "$OUT_DIR"

# ------------------------------------------------------------
# 1. Kernel
# ------------------------------------------------------------

info "Building kernel..."

make -C "$KERNEL_DIR" -j"$JOBS"

BZIMAGE="$(phantom_bzimage)"
[[ -f "$BZIMAGE" ]] || die "bzImage nie został wygenerowany ($BZIMAGE)."
ok "Kernel built."

# ------------------------------------------------------------
# 2. Userspace
# ------------------------------------------------------------

info "Building userspace..."

make -C "$USERSPACE_DIR/libphantom"
make -C "$USERSPACE_DIR/phantombox"
make -C "$USERSPACE_DIR/phantominstall"

[[ -x "$USERSPACE_DIR/phantombox/phantombox" ]] || \
	die "phantombox nie został zbudowany."
[[ -x "$USERSPACE_DIR/phantominstall/phatominstall" ]] || \
	die "phatominstall nie został zbudowany."

ok "Userspace built."

# ------------------------------------------------------------
# 3. Initramfs (rootfs + świeże binarki)
# ------------------------------------------------------------

[[ -d "$ROOTFS_DIR" ]] || die "Brak katalogu rootfs: $ROOTFS_DIR"

info "Preparing initramfs..."

mkdir -p "$ROOTFS_DIR/bin"
cp "$USERSPACE_DIR/phantombox/phantombox"     "$ROOTFS_DIR/bin/phantombox"
cp "$USERSPACE_DIR/phantominstall/phatominstall" "$ROOTFS_DIR/bin/phatominstall"
chmod 0755 "$ROOTFS_DIR/bin/phantombox" "$ROOTFS_DIR/bin/phatominstall"

(
	cd "$ROOTFS_DIR"
	find . -print0 | cpio --null -ov --format=newc | gzip -9
) > "$OUT_DIR/phantom-initramfs.cpio.gz"

[[ -s "$OUT_DIR/phantom-initramfs.cpio.gz" ]] || \
	die "initramfs nie został wygenerowany."

# ------------------------------------------------------------
# 4. Publikacja artefaktów
# ------------------------------------------------------------

cp "$BZIMAGE" "$OUT_DIR/bzImage"

ok "Build complete:"
echo "  Version  : $VERSION"
echo "  Kernel   : $(phantom_kernel_version)"
echo "  bzImage  : $OUT_DIR/bzImage"
echo "  initramfs: $OUT_DIR/phantom-initramfs.cpio.gz"