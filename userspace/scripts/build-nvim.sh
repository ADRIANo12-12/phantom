#!/usr/bin/env bash
set -euo pipefail

VERSION="0.12.5"
OUT_DIR="${1:-$(dirname "$0")/../out/bin}"

mkdir -p "$OUT_DIR"

cd /tmp
curl -sSL "https://github.com/neovim/neovim/releases/download/v${VERSION}/nvim-linux-x86_64.tar.gz" | tar -xz
cp nvim-linux-x86_64/bin/nvim "$OUT_DIR/nvim"
chmod +x "$OUT_DIR/nvim"

# Also copy runtime
mkdir -p "$(dirname "$0")/../out/share/nvim"
cp -r nvim-linux-x86_64/share/nvim/* "$(dirname "$0")/../out/share/nvim/"

echo "Built nvim v${VERSION} -> $OUT_DIR/nvim"