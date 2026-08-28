#!/usr/bin/env bash
#
# Phantom OS — wspólna biblioteka dla skryptów build/release/run.
#
# Źródło: source scripts/common.sh

set -euo pipefail

# ------------------------------------------------------------
# Ścieżki
# ------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

KERNEL_DIR="$PROJECT_ROOT/kernel"
ROOTFS_DIR="$PROJECT_ROOT/rootfs"
USERSPACE_DIR="$PROJECT_ROOT/userspace"
BUILDS_DIR="$PROJECT_ROOT/builds"

VERSION_FILE="$PROJECT_ROOT/.phantom-version"

LATEST_DIR="$BUILDS_DIR/latest"

DEV_BRANCH="master"
RELEASE_BRANCH="main"

# ------------------------------------------------------------
# Kolory i logging
# ------------------------------------------------------------

if [[ -t 1 ]]; then
	RED='\033[0;31m'
	GREEN='\033[0;32m'
	YELLOW='\033[1;33m'
	BLUE='\033[0;34m'
	CYAN='\033[0;36m'
	NC='\033[0m'
else
	RED=''
	GREEN=''
	YELLOW=''
	BLUE=''
	CYAN=''
	NC=''
fi

info()  { echo -e "${BLUE}[PHANTOM]${NC} $*"; }
ok()    { echo -e "${GREEN}[ OK ]${NC} $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC} $*" >&2; }
die()   { echo -e "${RED}[FAIL]${NC} $*" >&2; exit 1; }

# ------------------------------------------------------------
# Wersjonowanie
# ------------------------------------------------------------

phantom_version_default() {
	cat <<'EOF'
VER=1
SUBVER=0
PATCHLEVEL=0
CHANNEL=alpha
BUILD=0
CODENAME="Phantom"
EOF
}

phantom_version_load() {
	if [[ ! -f "$VERSION_FILE" ]]; then
		phantom_version_default > "$VERSION_FILE"
	fi

	source "$VERSION_FILE"
}

phantom_version_save() {
	cat > "$VERSION_FILE" <<EOF
VER=$VER
SUBVER=$SUBVER
PATCHLEVEL=$PATCHLEVEL
CHANNEL=$CHANNEL
BUILD=$BUILD
CODENAME="$CODENAME"
EOF
}

phantom_version_full() {
	printf '%s.%s.%s-%s.%s' \
		"$VER" "$SUBVER" "$PATCHLEVEL" "$CHANNEL" "$BUILD"
}

phantom_version_read() {
	phantom_version_load
	phantom_version_full
}

# ------------------------------------------------------------
# Wersja kernela (z kernel/Makefile)
# ------------------------------------------------------------

phantom_kernel_version() {
	local kv kp ks
	kv="$(sed -n 's/^VERSION[[:space:]]*=[[:space:]]*//p' "$KERNEL_DIR/Makefile")"
	kp="$(sed -n 's/^PATCHLEVEL[[:space:]]*=[[:space:]]*//p' "$KERNEL_DIR/Makefile")"
	ks="$(sed -n 's/^SUBLEVEL[[:space:]]*=[[:space:]]*//p' "$KERNEL_DIR/Makefile")"

	[[ -n "$kv" ]] || die "Nie można odczytać wersji kernela z $KERNEL_DIR/Makefile"

	printf '%s.%s.%s' "$kv" "${kp:-0}" "${ks:-0}"
}

# Krótka forma dla nazw plików: 7.2.0 -> 7.2
phantom_kernel_short() {
	local k
	k="$(phantom_kernel_version)"
	if [[ "$k" == *.0 ]]; then
		printf '%s' "${k%.0}"
	else
		printf '%s' "$k"
	fi
}

# ------------------------------------------------------------
# Wspólne targety artefaktów
# ------------------------------------------------------------

phantom_bzimage() { printf '%s' "$KERNEL_DIR/arch/x86/boot/bzImage"; }

# ------------------------------------------------------------
# Git
# ------------------------------------------------------------

phantom_require_clean_tree() {
	if [[ -n "$(git status --porcelain)" ]]; then
		die "Drzewo robocze nie jest czyste. Zacommituj zmiany najpierw."
	fi
}

phantom_require_branch() {
	local want="$1"
	local have
	have="$(git branch --show-current)"

	if [[ "$have" != "$want" ]]; then
		die "Ten skrypt musi działać z gałęzi '$want'. Jesteś na: '$have'."
	fi
}

# ------------------------------------------------------------
# Narzędzia
# ------------------------------------------------------------

phantom_require_cmds() {
	local c
	for c in "$@"; do
		command -v "$c" >/dev/null 2>&1 || \
			die "Nie znaleziono narzędzia '$c'."
	done
}