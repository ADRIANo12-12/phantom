#!/usr/bin/env bash
set -euo pipefail

VERSION="1.6.0"
OUT_DIR="${1:-$(dirname "$0")/../out/bin}"

mkdir -p "$OUT_DIR"

cd /tmp
curl -sSL "https://github.com/yorukot/superfile/releases/download/v${VERSION}/superfile-linux-v${VERSION}-amd64.tar.gz" | tar -xz
cp dist/superfile-linux-v${VERSION}-amd64/spf "$OUT_DIR/superfile"
chmod +x "$OUT_DIR/superfile"
echo "Built superfile v${VERSION} -> $OUT_DIR/superfile"