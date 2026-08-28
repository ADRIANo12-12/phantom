#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="${1:-$(dirname "$0")/../out/bin}"

mkdir -p "$OUT_DIR"

cd /tmp
git clone --depth 1 https://github.com/JamesTiberiusKirk/impala-go.git
cd impala-go
CGO_ENABLED=0 go build -ldflags="-s -w" -o impala .
cp impala "$OUT_DIR/impala"
echo "Built impala -> $OUT_DIR/impala"