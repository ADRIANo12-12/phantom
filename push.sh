#!/usr/bin/env bash
#
# Phantom OS — commit + push na gałąź master.
#
# Użycie:
#   ./push.sh "wiadomość commita"     # commit + push (zakładając brak wcześniejszych)
#   ./push.sh                          # zapyta o wiadomość
#
# Flagi:
#   --no-commit   tylko push lokalnych commitów
#   --no-push     tylko commit

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/scripts/common.sh"

DO_COMMIT=1
DO_PUSH=1

ARGS=()
for a in "$@"; do
	case "$a" in
		--no-commit) DO_COMMIT=0 ;;
		--no-push)   DO_PUSH=0 ;;
		*)           ARGS+=("$a") ;;
	esac
done

MSG="${ARGS[0]:-}"

phantom_require_branch "$DEV_BRANCH"

echo "--- git status ---"
git status --short

if [[ "$DO_COMMIT" == "1" ]]; then
	if git diff --quiet && git diff --cached --quiet && \
	   [[ -z "$(git ls-files --others --exclude-standard)" ]]; then
		echo "Brak zmian do commitowania."
		DO_COMMIT=0
	else
		if [[ -z "$MSG" ]]; then
			read -r -p "Wiadomość commita: " MSG
			[[ -n "$MSG" ]] || die "Anulowano (pusta wiadomość)."
		fi

		git add -A
		git commit -m "$MSG"
		ok "Commit: $MSG"
	fi
fi

if [[ "$DO_PUSH" == "1" ]]; then
	if git rev-parse @{u} >/dev/null 2>&1; then
		git push origin "$DEV_BRANCH"
	else
		git push -u origin "$DEV_BRANCH"
	fi
	ok "Pushed to origin/$DEV_BRANCH"
fi