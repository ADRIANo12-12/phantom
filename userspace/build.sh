#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_DIR="${1:-${SCRIPT_DIR}/out}"

mkdir -p "${OUT_DIR}/bin" "${OUT_DIR}/share"

echo "Building userspace tools..."

"${SCRIPT_DIR}/scripts/build-gum.sh" "${OUT_DIR}/bin"
"${SCRIPT_DIR}/scripts/build-superfile.sh" "${OUT_DIR}/bin"
"${SCRIPT_DIR}/scripts/build-nvim.sh" "${OUT_DIR}/bin"
"${SCRIPT_DIR}/scripts/build-btop.sh" "${OUT_DIR}/bin"
"${SCRIPT_DIR}/scripts/build-lazygit.sh" "${OUT_DIR}/bin"

echo ""
echo "All tools built successfully!"
ls -la "${OUT_DIR}/bin/"