#!/usr/bin/env bash
set -euo pipefail

VERSION="1.4.7"
OUT_DIR="${1:-$(dirname "$0")/../out/bin}"

mkdir -p "$OUT_DIR"

cd /tmp
curl -sSL "https://github.com/aristocratos/btop/releases/download/v${VERSION}/btop-x86_64-unknown-linux-musl.tar.gz" | tar -xz
cp btop/bin/btop "$OUT_DIR/btop"
chmod +x "$OUT_DIR/btop"
echo "Built btop v${VERSION} -> $OUT_DIR/btop"