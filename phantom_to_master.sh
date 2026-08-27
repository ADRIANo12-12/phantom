#!/usr/bin/env bash
set -Eeuo pipefail

# Synchronizuje CAŁĄ zawartość /home/adrian/phantom do:
# https://github.com/ADRIANo12-12/phantom.git
# branch: master
#
# Nie kopiuje lokalnego .git.
# Nie respektuje .gitignore — wszystkie pliki z katalogu źródłowego
# zostaną dodane do commita.

SRC="/home/adrian/phantom"
REMOTE="https://github.com/ADRIANo12-12/phantom.git"
BRANCH="master"

fail() {
    echo
    echo "ERROR: $*" >&2
    exit 1
}

cleanup() {
    if [[ -n "${WORKDIR:-}" && -d "${WORKDIR}" ]]; then
        rm -rf "${WORKDIR}"
    fi
}
trap cleanup EXIT

echo "[1/7] Sprawdzanie środowiska..."
[[ -d "$SRC" ]] || fail "Nie istnieje katalog: $SRC"
command -v git >/dev/null 2>&1 || fail "Brak git. Zainstaluj: sudo dnf install git"
command -v rsync >/dev/null 2>&1 || fail "Brak rsync. Zainstaluj: sudo dnf install rsync"
[[ -r "$SRC" ]] || fail "Brak uprawnień do odczytu: $SRC"

AUTHOR_NAME="$(git -C "$SRC" config --get user.name 2>/dev/null || true)"
AUTHOR_EMAIL="$(git -C "$SRC" config --get user.email 2>/dev/null || true)"
[[ -n "$AUTHOR_NAME" ]] || AUTHOR_NAME="$(git config --global --get user.name 2>/dev/null || true)"
[[ -n "$AUTHOR_EMAIL" ]] || AUTHOR_EMAIL="$(git config --global --get user.email 2>/dev/null || true)"

[[ -n "$AUTHOR_NAME" ]] || fail "Brak git user.name. Ustaw: git config --global user.name 'Twoje Imię'"
[[ -n "$AUTHOR_EMAIL" ]] || fail "Brak git user.email. Ustaw: git config --global user.email 'twoj@email'"

echo "    Źródło : $SRC"
echo "    Repo   : $REMOTE"
echo "    Branch : $BRANCH"
echo "    Autor  : $AUTHOR_NAME <$AUTHOR_EMAIL>"

WORKDIR="$(mktemp -d -t phantom-master-XXXXXX)"
REPO="$WORKDIR/repo"

echo
echo "[2/7] Pobieranie aktualnego branchu $BRANCH..."
git clone --depth 1 --no-tags --single-branch --branch "$BRANCH" "$REMOTE" "$REPO" >/dev/null

git -C "$REPO" config user.name "$AUTHOR_NAME"
git -C "$REPO" config user.email "$AUTHOR_EMAIL"
REMOTE_MASTER_SHA="$(git -C "$REPO" rev-parse HEAD)"

echo
echo "[3/7] Kopiowanie całej zawartości /home/adrian/phantom..."
# --delete usuwa z master pliki, których nie ma już lokalnie.
# .git wyłączamy, żeby nie przenosić metadanych Git z lokalnego repo.
rsync -a --delete --exclude='.git/' "$SRC/" "$REPO/"

echo
echo "[4/7] Dodawanie wszystkich plików do commita..."
# -f ignoruje .gitignore, aby objąć wszystkie pliki z katalogu źródłowego.
git -C "$REPO" add -f -A

if git -C "$REPO" diff --cached --quiet; then
    echo
echo "[5/7] Brak zmian."
    echo "    master już odpowiada zawartości /home/adrian/phantom."
    exit 0
fi

echo
echo "[5/7] Tworzenie commita..."
git -C "$REPO" commit -m "Sync Phantom from /home/adrian/phantom"
NEW_SHA="$(git -C "$REPO" rev-parse HEAD)"

echo
echo "[6/7] Wysyłanie do GitHub -> $BRANCH..."
# Jeśli master zmienił się na GitHubie od czasu klonowania, push zostanie odrzucony.
git -C "$REPO" push \
    --force-with-lease="refs/heads/$BRANCH:$REMOTE_MASTER_SHA" \
    origin \
    "HEAD:$BRANCH"

echo
echo "[7/7] GOTOWE"
echo "    Branch : $BRANCH"
echo "    Commit : $NEW_SHA"
echo "    Repo   : $REMOTE"
echo
echo "Wszystka zawartość /home/adrian/phantom została zsynchronizowana z master."
