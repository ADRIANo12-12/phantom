#!/usr/bin/env bash

set -euo pipefail

REPO="$HOME/phantom"

cd "$REPO"

echo "============================================================"
echo "             PHANTOM -> GITHUB + GIT LFS"
echo "============================================================"
echo

command -v git >/dev/null 2>&1 || {
	echo "ERROR: git not found"
	exit 1
}

command -v git-lfs >/dev/null 2>&1 || {
	echo "ERROR: git-lfs not found"
	echo
	echo "Fedora:"
	echo "  sudo dnf install git-lfs"
	exit 1
}

echo "[1/9] Repository..."
git rev-parse --is-inside-work-tree

echo
echo "Remote:"
git remote -v

echo
echo "Current branch:"
git branch --show-current

echo
echo "[2/9] Initializing Git LFS..."
git lfs install

echo
echo "[3/9] Creating LFS rules..."

cat > .gitattributes <<'ATTR'
*.7z filter=lfs diff=lfs merge=lfs -text
*.zip filter=lfs diff=lfs merge=lfs -text
*.tar filter=lfs diff=lfs merge=lfs -text
*.tar.gz filter=lfs diff=lfs merge=lfs -text
*.tgz filter=lfs diff=lfs merge=lfs -text
*.cpio filter=lfs diff=lfs merge=lfs -text
*.cpio.gz filter=lfs diff=lfs merge=lfs -text
*.img filter=lfs diff=lfs merge=lfs -text
*.iso filter=lfs diff=lfs merge=lfs -text
*.bin filter=lfs diff=lfs merge=lfs -text
*.elf filter=lfs diff=lfs merge=lfs -text
*.a filter=lfs diff=lfs merge=lfs -text
*.o filter=lfs diff=lfs merge=lfs -text
*.so filter=lfs diff=lfs merge=lfs -text
*.so.* filter=lfs diff=lfs merge=lfs -text
*.gz filter=lfs diff=lfs merge=lfs -text
*.xz filter=lfs diff=lfs merge=lfs -text
*.bz2 filter=lfs diff=lfs merge=lfs -text
*.zst filter=lfs diff=lfs merge=lfs -text
ATTR

echo
echo "LFS rules:"
cat .gitattributes

echo
echo "[4/9] Fetching GitHub..."
git fetch origin --prune

echo
echo "[5/9] Saving current state..."

BACKUP_BRANCH="backup-before-lfs-$(date +%Y%m%d-%H%M%S)"

git branch "$BACKUP_BRANCH"

echo "Backup branch:"
echo "  $BACKUP_BRANCH"

echo
echo "[6/9] Adding current Phantom tree..."

git add .gitattributes

git add -A

echo
echo "Git status:"
git status --short

echo
echo "[7/9] Checking large files..."

echo
echo "Largest files currently in working tree:"
find . \
	-type f \
	-not -path './.git/*' \
	-printf '%s %p\n' \
	| sort -nr \
	| head -30 \
	| awk '
	{
		size=$1
		$1=""
		printf "%12.1f MiB  %s\n", size/1048576, substr($0,2)
	}'

echo
echo "[8/9] Creating commit..."

if git diff --cached --quiet; then
	echo "Nothing new to commit."
else
	git commit -m "Phantom OS: checkpoint before installer development"
fi

echo
echo "[9/9] LFS status..."
git lfs status

echo
echo "============================================================"
echo " LOCAL PHANTOM CHECKPOINT READY"
echo "============================================================"
echo
echo "Current commit:"
git rev-parse HEAD

echo
echo "Backup branch:"
echo "  $BACKUP_BRANCH"

echo
echo "Remote main:"
git rev-parse --verify origin/main 2>/dev/null || true

echo
echo "------------------------------------------------------------"
echo "NEXT PUSH"
echo "------------------------------------------------------------"
echo
echo "First try the safe push:"
echo
echo "  git push origin HEAD:main"
echo
echo "If GitHub says non-fast-forward, STOP there."
echo
echo "Do NOT use --force yet."
echo
