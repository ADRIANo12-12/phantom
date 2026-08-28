#!/usr/bin/env bash
set -euo pipefail

VERSION="0.14.1"
OUT_DIR="${1:-$(dirname "$0")/../out/bin}"

mkdir -p "$OUT_DIR"

cd /tmp
curl -sSL "https://github.com/charmbracelet/gum/releases/download/v${VERSION}/gum_${VERSION}_Linux_x86_64.tar.gz" | tar -xz
cp "gum_${VERSION}_Linux_x86_64/gum" "$OUT_DIR/gum"
chmod +x "$OUT_DIR/gum"
echo "Built gum v${VERSION} -> $OUT_DIR/gum"