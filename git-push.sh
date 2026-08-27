#!/usr/bin/env bash
# Phantom OS — szybki commit + push
# Użycie: ./git-push.sh
#         ./git-push.sh "opcjonalna wiadomość commita"

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
	echo "Błąd: $ROOT nie jest repozytorium git."
	exit 1
fi

BRANCH="$(git branch --show-current 2>/dev/null || true)"
if [[ -z "$BRANCH" ]]; then
	echo "Błąd: nie jesteś na żadnej gałęzi (detached HEAD?)."
	exit 1
fi

echo "=============================================="
echo "  Phantom git-push"
echo "  Repo   : $ROOT"
echo "  Branch : $BRANCH"
echo "=============================================="
echo

# Status
echo "--- git status ---"
git status --short
echo

# Czy cokolwiek do zrobienia?
if git diff --quiet && git diff --cached --quiet && \
   [[ -z "$(git ls-files --others --exclude-standard)" ]]; then
	echo "Brak zmian do commitowania."
	# Może są lokalne commity do pusha?
	if git rev-parse @{u} >/dev/null 2>&1; then
		AHEAD="$(git rev-list --count @{u}..HEAD 2>/dev/null || echo 0)"
		if [[ "$AHEAD" -gt 0 ]]; then
			echo "Masz $AHEAD lokalnych commit(ów) do pusha."
			read -r -p "Push na origin/$BRANCH? [y/N] " ans
			if [[ "$ans" =~ ^[yY]$ ]]; then
				git push -u origin "$BRANCH"
				echo "Push OK."
			fi
		fi
	fi
	exit 0
fi

echo "--- zmienione / nowe pliki ---"
git status --short
echo

read -r -p "Dodać wszystkie zmiany (git add -A)? [Y/n] " add_ans
add_ans="${add_ans:-Y}"
if [[ ! "$add_ans" =~ ^[yY]$ ]]; then
	echo "Anulowano (nic nie dodano)."
	exit 0
fi

git add -A

echo
echo "--- staged ---"
git status --short
echo

if git diff --cached --quiet; then
	echo "Nic nie jest staged — koniec."
	exit 0
fi

# Commit message
if [[ $# -ge 1 && -n "${1:-}" ]]; then
	MSG="$1"
	echo "Commit message (z argumentu): $MSG"
else
	echo "Podaj commit message (puste = anuluj):"
	read -r -p "> " MSG
	if [[ -z "$MSG" ]]; then
		echo "Anulowano."
		exit 0
	fi
fi

git commit -m "$MSG"
echo
echo "Commit utworzony."
echo

read -r -p "Push na origin/$BRANCH? [Y/n] " push_ans
push_ans="${push_ans:-Y}"
if [[ ! "$push_ans" =~ ^[yY]$ ]]; then
	echo "Commit lokalny, bez pusha."
	exit 0
fi

git push -u origin "$BRANCH"
echo
echo "Gotowe: origin/$BRANCH"
