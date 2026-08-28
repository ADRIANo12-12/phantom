#!/usr/bin/env bash
set -euo pipefail

VERSION="0.44.0"
OUT_DIR="${1:-$(dirname "$0")/../out/bin}"

mkdir -p "$OUT_DIR"

cd /tmp
curl -sSL "https://github.com/jesseduffield/lazygit/releases/download/v${VERSION}/lazygit_${VERSION}_Linux_x86_64.tar.gz" | tar -xz
cp lazygit "$OUT_DIR/lazygit"
chmod +x "$OUT_DIR/lazygit"
echo "Built lazygit v${VERSION} -> $OUT_DIR/lazygit"