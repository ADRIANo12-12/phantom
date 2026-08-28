#!/usr/bin/env bash
#
# Phantom OS — pełny release.
#
# Flow:
#   1. build kernela + userspace + initramfs (build.sh, PHANTOM_OUT=releases/...)
#   2. bootowalne ISO (iso.sh)
#   3. BUILD.md + SHA256SUMS
#   4. bump .phantom-version (BUILD++) i commit na master
#   5. switch main -> merge --ff-only master -> tag v<ver>
#   6. push main + tag (opcjonalnie GitHub Release przez gh)
#
# Użycie:
#   ./release.sh
#   ./release.sh --no-push          # build i commit, ale bez pusha/tagi
#   ./release.sh --yes              # bez pytań (oprócz gh release)

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/scripts/common.sh"

phantom_require_cmds make cpio gzip xorriso
phantom_require_branch "$DEV_BRANCH"
phantom_require_clean_tree

DO_PUSH=1
ASSUME_YES=0

for a in "$@"; do
	case "$a" in
		--no-push) DO_PUSH=0 ;;
		--yes)     ASSUME_YES=1 ;;
		*) die "Nieznana flaga: $a" ;;
	esac
done

confirm() {
	[[ "$ASSUME_YES" == "1" ]] && return 0
	read -r -p "Kontynuować? [y/N] " ans
	[[ "$ans" =~ ^[Yy]$ ]]
}

# ------------------------------------------------------------
# Wersja
# ------------------------------------------------------------

phantom_version_load

BUILD=$((BUILD + 1))
VERSION="$(phantom_version_full)"
KVER="$(phantom_kernel_version)"
KSHORT="$(phantom_kernel_short)"

if [[ -z "${CODENAME:-}" ]]; then
	CODENAME="${CODENAME_DEFAULT:-Phantom}"
fi

echo "======================================================"
echo "               PHANTOM OS RELEASE"
echo "======================================================"
echo
echo "  Wersja systemu: $VERSION"
echo "  Wersja kernela: $KVER"
echo "  Codename      : $CODENAME"
echo
confirm || { warn "Anulowano."; exit 0; }

RELEASE_DIR="$BUILDS_DIR/phantom-$VERSION-$CODENAME"

if [[ -e "$RELEASE_DIR" ]]; then
	die "Katalog release już istnieje: $RELEASE_DIR"
fi

mkdir -p "$RELEASE_DIR"

# ------------------------------------------------------------
# Build + ISO
# ------------------------------------------------------------

info "Building system..."
PHANTOM_OUT="$RELEASE_DIR" "$PROJECT_ROOT/build.sh"

info "Building ISO..."
PHANTOM_OUT="$RELEASE_DIR" "$PROJECT_ROOT/iso.sh"

# ------------------------------------------------------------
# Manifest + checksums
# ------------------------------------------------------------

GIT_COMMIT="$(git rev-parse HEAD)"

cat > "$RELEASE_DIR/BUILD.md" <<EOF
# Phantom OS — Build

## System

- Wersja:  ${VERSION}
- Codename: ${CODENAME}

## Kernel

- Wersja: ${KVER}

## Repozytorium

- Branch: ${DEV_BRANCH}
- Commit: ${GIT_COMMIT}

## Build

- Data: $(date -u +"%Y-%m-%d %H:%M:%S UTC")
- Jobs: ${JOBS:-$(nproc)}

## Artefakty

- bzImage
- phantom-initramfs.cpio.gz
- phantom-${VERSION}-k${KSHORT}.iso
EOF

(
	cd "$RELEASE_DIR"
	sha256sum bzImage phantom-initramfs.cpio.gz "phantom-${VERSION}-k${KSHORT}.iso" > SHA256SUMS
)

ok "Release ready:"
echo "  $RELEASE_DIR"
echo

confirm || { warn "Artefakty gotowe lokalnie, bez commita/pusha."; exit 0; }

# ------------------------------------------------------------
# Commit wersji na master
# ------------------------------------------------------------

phantom_version_save

git add "$VERSION_FILE"
git commit -m "phantom: prepare build ${VERSION} (${CODENAME})"

if [[ "$DO_PUSH" == "0" ]]; then
	ok "Commit na master: ${VERSION}. (--no-push, bez release)"
	exit 0
fi

# ------------------------------------------------------------
# Release na main + tag
# ------------------------------------------------------------

info "Switching to $RELEASE_BRANCH ..."

git switch "$RELEASE_BRANCH"
git merge --ff-only master || {
	warn "Nie udał się ff-merge marki. Wróć na master."
	git switch "$DEV_BRANCH"
	exit 1
}

TAG="v${VERSION}"
git tag -a "$TAG" -m "Phantom OS ${VERSION} — ${CODENAME}"

info "Pushing $RELEASE_BRANCH ..."
git push origin "$RELEASE_BRANCH"

info "Pushing tag $TAG ..."
git push origin "$TAG"

git switch "$DEV_BRANCH"

ok "Release ${VERSION} published."

# ------------------------------------------------------------
# GitHub Release (opcjonalnie)
# ------------------------------------------------------------

if command -v gh >/dev/null 2>&1; then
	read -r -p "Utworzyć GitHub Release (gh) dla $TAG? [y/N] " gh_ans
	if [[ "$gh_ans" =~ ^[Yy]$ ]]; then
		gh release create "$TAG" \
			"$RELEASE_DIR/phantom-${VERSION}-k${KSHORT}.iso" \
			"$RELEASE_DIR/bzImage" \
			"$RELEASE_DIR/phantom-initramfs.cpio.gz" \
			--title "Phantom OS ${VERSION} — ${CODENAME}" \
			--notes "Kernel ${KVER}, commit ${GIT_COMMIT}"
		ok "GitHub Release utworzony."
	fi
else
	warn "gh nie znaleziono — pominięto GitHub Release."
fi

echo
info "Gotowe."
echo "  $RELEASE_DIR"
echo "  Tag: $TAG"