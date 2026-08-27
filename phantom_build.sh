#!/usr/bin/env bash
set -Eeuo pipefail

# ============================================================
# Phantom OS Release Builder
# ------------------------------------------------------------
# - syncs local master -> GitHub master
# - builds Linux kernel + initramfs
# - creates a QEMU-runnable release artifact
# - asks for release channel and codename
# - creates versioned artifacts, manifest and checksums
# - syncs master -> main
# - creates and pushes a Git tag
#
# Expected tree:
#   /home/adrian/phantom
#   /home/adrian/phantom/kernel
#   /home/adrian/phantom/rootfs   (preferred)
#
# QEMU is always run with -nographic.
# ============================================================

PROJECT_ROOT="${PROJECT_ROOT:-/home/adrian/phantom}"
REMOTE="${REMOTE:-https://github.com/ADRIANo12-12/phantom.git}"
DEV_BRANCH="master"
RELEASE_BRANCH="main"
ARCH="x86_64"
JOBS="${JOBS:-$(nproc)}"
BUILD_ROOT="$PROJECT_ROOT/builds"
VERSION_FILE="$PROJECT_ROOT/.phantom-version"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m'

info() { echo -e "${BLUE}[PHANTOM]${NC} $*"; }
ok()   { echo -e "${GREEN}[ OK ]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
die()  { echo -e "${RED}[FAIL]${NC} $*" >&2; exit 1; }

cleanup() {
    [[ -n "${TMPDIR_RELEASE:-}" && -d "$TMPDIR_RELEASE" ]] && rm -rf "$TMPDIR_RELEASE"
}
trap cleanup EXIT

require_cmd() {
    command -v "$1" >/dev/null 2>&1 || die "Brak programu: $1"
}

cd "$PROJECT_ROOT" || die "Nie można wejść do $PROJECT_ROOT"

info "Phantom OS Release Builder"
echo

echo "Projekt : $PROJECT_ROOT"
echo "Repo   : $REMOTE"
echo "Dev    : $DEV_BRANCH"
echo "Stable : $RELEASE_BRANCH"
echo

require_cmd git
require_cmd make
require_cmd cpio
require_cmd gzip
require_cmd sha256sum
require_cmd rsync
require_cmd qemu-system-x86_64

[[ -d "$PROJECT_ROOT/.git" ]] || die "$PROJECT_ROOT nie jest repozytorium Git"
[[ -d "$PROJECT_ROOT/kernel" ]] || die "Brak katalogu kernel/"

CURRENT_BRANCH="$(git branch --show-current)"
[[ "$CURRENT_BRANCH" == "$DEV_BRANCH" ]] || die "Uruchom skrypt na branchu master (obecnie: $CURRENT_BRANCH)"

if [[ -n "$(git status --porcelain)" ]]; then
    die "Working tree nie jest czyste. Najpierw zapisz zmiany na master."
fi

# ------------------------------------------------------------
# Ask release channel
# ------------------------------------------------------------
echo "Wybierz typ wydania:"
echo "  1) official  - oficjalne stabilne wydanie"
echo "  2) beta      - beta"
echo "  3) pre       - pre-release / release candidate"
echo "  4) alpha     - alpha"
echo "  5) debug     - debug build"
echo

while true; do
    read -r -p "Typ [1-5]: " CHANNEL_CHOICE
    case "$CHANNEL_CHOICE" in
        1) CHANNEL="official"; break ;;
        2) CHANNEL="beta"; break ;;
        3) CHANNEL="pre"; break ;;
        4) CHANNEL="alpha"; break ;;
        5) CHANNEL="debug"; break ;;
        *) echo "Wybierz 1, 2, 3, 4 albo 5." ;;
    esac
done

# ------------------------------------------------------------
# Version management
# ------------------------------------------------------------
if [[ ! -f "$VERSION_FILE" ]]; then
    cat > "$VERSION_FILE" <<'VER'
VER=0
SUBVER=1
PATCHLEVEL=0
BUILD=0
VER
fi

# shellcheck disable=SC1090
source "$VERSION_FILE"

: "${VER:?}"
: "${SUBVER:?}"
: "${PATCHLEVEL:?}"
: "${BUILD:=0}"

while true; do
    read -r -p "VER [$VER]: " INPUT
    [[ -z "$INPUT" ]] || VER="$INPUT"
    [[ "$VER" =~ ^[0-9]+$ ]] && break
    echo "VER musi być liczbą."
done

while true; do
    read -r -p "SUBVER [$SUBVER]: " INPUT
    [[ -z "$INPUT" ]] || SUBVER="$INPUT"
    [[ "$SUBVER" =~ ^[0-9]+$ ]] && break
    echo "SUBVER musi być liczbą."
done

while true; do
    read -r -p "PATCHLEVEL [$PATCHLEVEL]: " INPUT
    [[ -z "$INPUT" ]] || PATCHLEVEL="$INPUT"
    [[ "$PATCHLEVEL" =~ ^[0-9]+$ ]] && break
    echo "PATCHLEVEL musi być liczbą."
done

VERSION="${VER}.${SUBVER}.${PATCHLEVEL}"
BUILD=$((BUILD + 1))

# ------------------------------------------------------------
# Codename
# ------------------------------------------------------------
while true; do
    read -r -p "Nazwa kodowa (np. Suicidal Squirrel): " CODENAME
    [[ -n "$CODENAME" ]] && break
    echo "Nazwa kodowa nie może być pusta."
done

SAFE_CODENAME="$(printf '%s' "$CODENAME" \
    | tr '[:upper:]' '[:lower:]' \
    | tr -cs 'a-z0-9' '-' \
    | sed 's/^-//; s/-$//')"

[[ -n "$SAFE_CODENAME" ]] || die "Nie udało się utworzyć bezpiecznej nazwy kodowej."

case "$CHANNEL" in
    official)
        RELEASE_VERSION="$VERSION"
        CHANNEL_LABEL="Official"
        TAG="v${VERSION}"
        ;;
    beta)
        RELEASE_VERSION="${VERSION}-beta.${BUILD}"
        CHANNEL_LABEL="Beta"
        TAG="v${VERSION}-beta.${BUILD}"
        ;;
    pre)
        RELEASE_VERSION="${VERSION}-pre.${BUILD}"
        CHANNEL_LABEL="Pre"
        TAG="v${VERSION}-pre.${BUILD}"
        ;;
    alpha)
        RELEASE_VERSION="${VERSION}-alpha.${BUILD}"
        CHANNEL_LABEL="Alpha"
        TAG="v${VERSION}-alpha.${BUILD}"
        ;;
    debug)
        RELEASE_VERSION="${VERSION}-debug.${BUILD}"
        CHANNEL_LABEL="Debug"
        TAG="v${VERSION}-debug.${BUILD}"
        ;;
esac

RELEASE_NAME="PhantomOS-${RELEASE_VERSION}-${SAFE_CODENAME}-${ARCH}"
RELEASE_DIR="$BUILD_ROOT/$RELEASE_NAME"
KERNEL_NAME="phantom-kernel-${RELEASE_VERSION}-${SAFE_CODENAME}-${ARCH}.bzImage"
ROOTFS_NAME="phantom-rootfs-${RELEASE_VERSION}-${SAFE_CODENAME}-${ARCH}.cpio.gz"
ISO_NAME="${RELEASE_NAME}.tar.gz"

if [[ -e "$RELEASE_DIR" || -e "$BUILD_ROOT/$ISO_NAME" ]]; then
    die "Build już istnieje: $RELEASE_NAME"
fi

printf '\n'
echo -e "${CYAN}==============================================${NC}"
echo -e "${CYAN}          PHANTOM OS RELEASE PLAN             ${NC}"
echo -e "${CYAN}==============================================${NC}"
echo "Base version : $VERSION"
echo "Release      : $RELEASE_VERSION"
echo "Channel      : $CHANNEL_LABEL"
echo "Codename     : $CODENAME"
echo "Architecture : $ARCH"
echo "Build number : $BUILD"
echo "Git tag      : $TAG"
echo "Output       : $RELEASE_DIR"
echo

read -r -p "Kontynuować ten release? [y/N]: " CONFIRM
[[ "$CONFIRM" =~ ^[Yy]$ ]] || { warn "Anulowano."; exit 0; }

# ------------------------------------------------------------
# Push current master first
# ------------------------------------------------------------
info "Synchronizacja lokalnego master -> GitHub master..."
git push origin "$DEV_BRANCH"
ok "master wysłany."

# ------------------------------------------------------------
# Build
# ------------------------------------------------------------
mkdir -p "$RELEASE_DIR"

info "Budowanie kernela ($JOBS jobs)..."
make -C "$PROJECT_ROOT/kernel" -j"$JOBS"

KERNEL_IMAGE="$PROJECT_ROOT/kernel/arch/x86/boot/bzImage"
[[ -f "$KERNEL_IMAGE" ]] || die "Kernel nie wygenerował $KERNEL_IMAGE"
cp "$KERNEL_IMAGE" "$RELEASE_DIR/$KERNEL_NAME"
ok "Kernel gotowy: $KERNEL_NAME"

# ------------------------------------------------------------
# Build rootfs/initramfs
# ------------------------------------------------------------
if [[ -d "$PROJECT_ROOT/rootfs" ]]; then
    info "Budowanie initramfs..."
    (
        cd "$PROJECT_ROOT/rootfs"
        find . -print0 \
            | cpio --null -o --format=newc \
            | gzip -9
    ) > "$RELEASE_DIR/$ROOTFS_NAME"
    ok "Initramfs gotowy: $ROOTFS_NAME"
else
    die "Brak rootfs/. Nie tworzę release bez rootfs, bo QEMU nie będzie miał kompletnego systemu."
fi

# ------------------------------------------------------------
# QEMU helper
# ------------------------------------------------------------
QEMU_SCRIPT="$RELEASE_DIR/run-qemu.sh"
cat > "$QEMU_SCRIPT" <<EOF_QEMU
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="\$(cd "\$(dirname "\${BASH_SOURCE[0]}")" && pwd)"
KERNEL="\$ROOT_DIR/$KERNEL_NAME"
INITRD="\$ROOT_DIR/$ROOTFS_NAME"

exec qemu-system-x86_64 \\
    -m 1024 \\
    -kernel "\$KERNEL" \\
    -initrd "\$INITRD" \\
    -append "console=ttyS0 panic=-1" \\
    -serial mon:stdio \\
    -nographic
EOF_QEMU
chmod +x "$QEMU_SCRIPT"

# ------------------------------------------------------------
# Manifest
# ------------------------------------------------------------
GIT_COMMIT="$(git rev-parse HEAD)"
BUILD_DATE="$(date -u +'%Y-%m-%d %H:%M:%S UTC')"

cat > "$RELEASE_DIR/BUILD.md" <<EOF_BUILD
# Phantom OS Release

Version: $RELEASE_VERSION
Base version: $VERSION
Channel: $CHANNEL_LABEL
Codename: $CODENAME
Architecture: $ARCH
Build number: $BUILD
Git branch: $DEV_BRANCH
Git commit: $GIT_COMMIT
Build date: $BUILD_DATE

## Artifacts

- $KERNEL_NAME
- $ROOTFS_NAME
- run-qemu.sh
- SHA256SUMS
EOF_BUILD

# Human-readable release metadata
cat > "$RELEASE_DIR/VERSION" <<EOF_VERSION
PHANTOM_VERSION=$VERSION
PHANTOM_RELEASE=$RELEASE_VERSION
PHANTOM_CHANNEL=$CHANNEL
PHANTOM_CODENAME=$CODENAME
PHANTOM_BUILD=$BUILD
PHANTOM_ARCH=$ARCH
PHANTOM_GIT_COMMIT=$GIT_COMMIT
EOF_VERSION

# ------------------------------------------------------------
# Self-test through QEMU
# ------------------------------------------------------------
info "Test QEMU release image (-nographic)..."
QEMU_LOG="$RELEASE_DIR/qemu-test.log"
set +e
timeout 12s qemu-system-x86_64 \
    -m 1024 \
    -kernel "$RELEASE_DIR/$KERNEL_NAME" \
    -initrd "$RELEASE_DIR/$ROOTFS_NAME" \
    -append "console=ttyS0 panic=-1" \
    -serial stdio \
    -display none \
    -nographic \
    >"$QEMU_LOG" 2>&1
QEMU_STATUS=$?
set -e

# timeout(124) is acceptable: it means QEMU was alive until the test window ended.
if [[ "$QEMU_STATUS" != 0 && "$QEMU_STATUS" != 124 ]]; then
    warn "QEMU zakończył test kodem $QEMU_STATUS. Log: $QEMU_LOG"
else
    ok "QEMU uruchomił release w trybie -nographic."
fi

# ------------------------------------------------------------
# Checksums + archive
# ------------------------------------------------------------
info "Tworzenie manifestu SHA-256..."
(
    cd "$RELEASE_DIR"
    sha256sum "$KERNEL_NAME" "$ROOTFS_NAME" VERSION BUILD.md run-qemu.sh qemu-test.log > SHA256SUMS
)

info "Tworzenie archiwum release..."
(
    cd "$BUILD_ROOT"
    tar -czf "$ISO_NAME" "$RELEASE_NAME"
)
ok "Build archive: $BUILD_ROOT/$ISO_NAME"

# ------------------------------------------------------------
# Persist version state on master
# ------------------------------------------------------------
cat > "$VERSION_FILE" <<EOF_VERSION_STATE
VER=$VER
SUBVER=$SUBVER
PATCHLEVEL=$PATCHLEVEL
BUILD=$BUILD
EOF_VERSION_STATE

git add "$VERSION_FILE" "$RELEASE_DIR" "$BUILD_ROOT/$ISO_NAME"
git commit -m "phantom: prepare ${RELEASE_VERSION} (${CODENAME})"

# ------------------------------------------------------------
# Sync master -> main
# ------------------------------------------------------------
info "Synchronizacja master -> main..."

REMOTE_MAIN_SHA="$(git ls-remote origin "refs/heads/$RELEASE_BRANCH" | awk '{print $1}')"
REMOTE_MASTER_SHA="$(git ls-remote origin "refs/heads/$DEV_BRANCH" | awk '{print $1}')"

[[ -n "$REMOTE_MASTER_SHA" ]] || die "Nie znaleziono origin/master"

if [[ -n "$REMOTE_MAIN_SHA" ]]; then
    # Main is intentionally made identical to the release source from master.
    git push origin "$DEV_BRANCH:$RELEASE_BRANCH" \
        --force-with-lease="refs/heads/$RELEASE_BRANCH:$REMOTE_MAIN_SHA"
else
    git push origin "$DEV_BRANCH:$RELEASE_BRANCH"
fi

ok "main teraz wskazuje na release commit z master."

# ------------------------------------------------------------
# Tag and publish
# ------------------------------------------------------------
if git rev-parse "$TAG" >/dev/null 2>&1; then
    die "Tag $TAG już istnieje lokalnie."
fi

info "Tworzenie tagu $TAG..."
git tag -a "$TAG" -m "Phantom OS ${RELEASE_VERSION} - ${CODENAME}"
git push origin "$TAG"
ok "Tag $TAG wysłany."

# Keep local checkout on master.
git switch "$DEV_BRANCH"

# ------------------------------------------------------------
# Final summary
# ------------------------------------------------------------
echo
echo -e "${GREEN}==============================================${NC}"
echo -e "${GREEN}          PHANTOM OS RELEASE READY            ${NC}"
echo -e "${GREEN}==============================================${NC}"
echo "Version : $RELEASE_VERSION"
echo "Codename: $CODENAME"
echo "Channel : $CHANNEL_LABEL"
echo "Tag     : $TAG"
echo "Build   : $BUILD"
echo "Artifact: $BUILD_ROOT/$ISO_NAME"
echo "QEMU    : $RELEASE_DIR/run-qemu.sh"
echo
echo "Master  : $DEV_BRANCH"
echo "Stable  : $RELEASE_BRANCH"
echo
echo "Uruchomienie release:"
echo "  $RELEASE_DIR/run-qemu.sh"
echo
