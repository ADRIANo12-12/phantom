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

phantom_require_cmds make cpio gzip curl tar git bzip2
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

# Build external TUI tools
"$USERSPACE_DIR/build.sh" "$USERSPACE_DIR/out"

# Verify critical binaries
for bin in gum superfile nvim btop lazygit phantom-desktop; do
    [[ -x "$USERSPACE_DIR/out/bin/$bin" ]] || die "$bin nie został zbudowany."
done

ok "Userspace built."

# ------------------------------------------------------------
# 3. Initramfs (rootfs + świeże binarki, przez staging w builds/)
# ------------------------------------------------------------

[[ -d "$ROOTFS_DIR" ]] || die "Brak katalogu rootfs: $ROOTFS_DIR"

info "Preparing initramfs..."

STAGE="$BUILDS_DIR/rootfs-stage"
rm -rf "$STAGE"
cp -a "$ROOTFS_DIR/." "$STAGE/"
mkdir -p "$STAGE/bin" "$STAGE/usr/share" "$STAGE/etc/phantom"

# Copy userspace binaries
cp "$USERSPACE_DIR/out/bin/"* "$STAGE/bin/"
chmod 0755 "$STAGE/bin/"*

# Add busybox symlinks for common commands
for cmd in mkdir cp ln rm ls cat echo sleep loadkeys clear; do
    ln -sf busybox "$STAGE/bin/$cmd"
done

# Copy themes and configs
cp -r "$USERSPACE_DIR/out/share/"* "$STAGE/usr/share/"

# Copy nvim runtime
mkdir -p "$STAGE/usr/share/nvim"
cp -r "$USERSPACE_DIR/out/share/nvim/"* "$STAGE/usr/share/nvim/" 2>/dev/null || true

# Create symlink for phantom-desktop as default shell
ln -sf phantom-desktop "$STAGE/bin/init-desktop"

(
    cd "$STAGE"
    find . -print0 | cpio --null -ov --format=newc | gzip -9
) > "$OUT_DIR/phantom-initramfs.cpio.gz"

rm -rf "$STAGE"

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