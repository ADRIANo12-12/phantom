#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="${1:-$(dirname "$0")/../out/bin}"

mkdir -p "$OUT_DIR"

cd /tmp
git clone --depth 1 https://github.com/tsowell/wiremix.git
cd wiremix
cargo build --release --target x86_64-unknown-linux-musl
cp target/x86_64-unknown-linux-musl/release/wiremix "$OUT_DIR/wiremix"
echo "Built wiremix -> $OUT_DIR/wiremix"