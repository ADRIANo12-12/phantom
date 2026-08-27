#!/usr/bin/env bash

set -euo pipefail

# ============================================================
# Phantom OS Build & Release System
# ============================================================

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$PROJECT_ROOT/kernel"
ROOTFS_DIR="$PROJECT_ROOT/rootfs"
OUT_DIR="$PROJECT_ROOT/builds"

JOBS=6

VERSION_FILE="$PROJECT_ROOT/.phantom-version"

# ------------------------------------------------------------
# Colors
# ------------------------------------------------------------

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

info() {
    echo -e "${BLUE}[PHANTOM]${NC} $*"
}

ok() {
    echo -e "${GREEN}[ OK ]${NC} $*"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $*"
}

die() {
    echo -e "${RED}[FAIL]${NC} $*" >&2
    exit 1
}

# ------------------------------------------------------------
# Environment checks
# ------------------------------------------------------------

cd "$PROJECT_ROOT"

command -v git >/dev/null || die "git not found"
command -v make >/dev/null || die "make not found"
command -v cpio >/dev/null || die "cpio not found"
command -v gzip >/dev/null || die "gzip not found"

[[ -d "$KERNEL_DIR" ]] || die "kernel/ directory not found"

# ------------------------------------------------------------
# Branch safety
# ------------------------------------------------------------

CURRENT_BRANCH="$(git branch --show-current)"

if [[ "$CURRENT_BRANCH" != "master" ]]; then
    die "Run build.sh from master. Current branch: $CURRENT_BRANCH"
fi

if [[ -n "$(git status --porcelain)" ]]; then
    die "Working tree is not clean. Commit your changes first."
fi

# ------------------------------------------------------------
# Load / initialize Phantom version
# ------------------------------------------------------------

if [[ ! -f "$VERSION_FILE" ]]; then
    cat > "$VERSION_FILE" <<EOF
VER=0
SUBVER=0
PATCHLEVEL=0
CHANNEL=alpha
BUILD=0
EOF
fi

source "$VERSION_FILE"

BUILD=$((BUILD + 1))

VERSION="${VER}.${SUBVER}.${PATCHLEVEL}-${CHANNEL}.${BUILD}"

echo
echo "=============================================="
echo "              PHANTOM OS BUILD"
echo "=============================================="
echo
echo "Version: ${VERSION}"
echo

read -r -p "Codename (example: Suicidal Squirrel): " CODENAME

[[ -n "$CODENAME" ]] || die "Codename cannot be empty."

SAFE_CODENAME="$(echo "$CODENAME" \
    | tr '[:upper:]' '[:lower:]' \
    | tr -cs 'a-z0-9' '-' \
    | sed 's/^-//;s/-$//')"

RELEASE_DIR="$OUT_DIR/$VERSION-$SAFE_CODENAME"

if [[ -e "$RELEASE_DIR" ]]; then
    die "Release directory already exists: $RELEASE_DIR"
fi

mkdir -p "$RELEASE_DIR"

# ------------------------------------------------------------
# Save version state
# ------------------------------------------------------------

cat > "$VERSION_FILE" <<EOF
VER=$VER
SUBVER=$SUBVER
PATCHLEVEL=$PATCHLEVEL
CHANNEL=$CHANNEL
BUILD=$BUILD
EOF

# ------------------------------------------------------------
# Build kernel
# ------------------------------------------------------------

info "Building Phantom kernel..."

cd "$KERNEL_DIR"

make -j"$JOBS"

KERNEL_IMAGE="$KERNEL_DIR/arch/x86/boot/bzImage"

[[ -f "$KERNEL_IMAGE" ]] || die "bzImage was not produced."

ok "Kernel built successfully."

# ------------------------------------------------------------
# Build initramfs
# ------------------------------------------------------------

cd "$PROJECT_ROOT"

if [[ -d "$ROOTFS_DIR" ]]; then

    info "Building initramfs..."

    ROOTFS_IMAGE="$RELEASE_DIR/phantom-rootfs.cpio.gz"

    (
        cd "$ROOTFS_DIR"
        find . -print0 \
            | cpio --null -ov --format=newc \
            | gzip -9
    ) > "$ROOTFS_IMAGE"

    ok "Initramfs built."

elif [[ -f "$PROJECT_ROOT/phantom-rootfs.cpio.gz" ]]; then

    info "Using existing initramfs."

    cp \
        "$PROJECT_ROOT/phantom-rootfs.cpio.gz" \
        "$RELEASE_DIR/phantom-rootfs.cpio.gz"

    ok "Existing initramfs copied."

else
    warn "No rootfs directory or phantom-rootfs.cpio.gz found."
fi

# ------------------------------------------------------------
# Copy kernel
# ------------------------------------------------------------

cp "$KERNEL_IMAGE" "$RELEASE_DIR/bzImage"

# ------------------------------------------------------------
# Git revision
# ------------------------------------------------------------

GIT_COMMIT="$(git rev-parse HEAD)"

# ------------------------------------------------------------
# Build manifest
# ------------------------------------------------------------

cat > "$RELEASE_DIR/BUILD.md" <<EOF
# Phantom OS Build

## Version

${VERSION}

## Codename

${CODENAME}

## Git Commit

${GIT_COMMIT}

## Branch

master

## Architecture

x86_64

## Kernel

Linux-based Phantom kernel

## Build Jobs

${JOBS}

## Build Date

$(date -u +"%Y-%m-%d %H:%M:%S UTC")

## Artifacts

- bzImage
- phantom-rootfs.cpio.gz (when available)
EOF

# ------------------------------------------------------------
# Checksums
# ------------------------------------------------------------

cd "$RELEASE_DIR"

sha256sum * > SHA256SUMS

cd "$PROJECT_ROOT"

# ------------------------------------------------------------
# Release information
# ------------------------------------------------------------

info "Release created:"
echo
echo "  Version : $VERSION"
echo "  Codename: $CODENAME"
echo "  Output  : $RELEASE_DIR"
echo

cat "$RELEASE_DIR/BUILD.md"

echo
read -r -p "Publish this build to main? [y/N]: " CONFIRM

if [[ ! "$CONFIRM" =~ ^[Yy]$ ]]; then
    warn "Build created locally. Nothing pushed."
    exit 0
fi

# ------------------------------------------------------------
# Switch to main
# ------------------------------------------------------------

info "Switching to main..."

git add "$VERSION_FILE"

git commit \
    -m "phantom: prepare build ${VERSION} (${CODENAME})"

git switch main

# Make main contain current master
git merge --ff-only master

# ------------------------------------------------------------
# Commit release metadata
# ------------------------------------------------------------

git add "$VERSION_FILE" "$RELEASE_DIR"

git commit \
    -m "phantom: release ${VERSION} ${CODENAME}"

# ------------------------------------------------------------
# Tag
# ------------------------------------------------------------

git tag \
    -a "v${VERSION}" \
    -m "Phantom OS ${VERSION} - ${CODENAME}"

# ------------------------------------------------------------
# Push
# ------------------------------------------------------------

info "Pushing main..."

git push origin main

info "Pushing release tag..."

git push origin "v${VERSION}"

# ------------------------------------------------------------
# Return to development branch
# ------------------------------------------------------------

git switch master

ok "Phantom OS build ${VERSION} published."

echo
echo "=============================================="
echo "              BUILD COMPLETE"
echo "=============================================="
echo
echo "Version : ${VERSION}"
echo "Codename: ${CODENAME}"
echo "Tag     : v${VERSION}"
echo
echo "Development branch: master"
echo "Stable branch     : main"
echo
