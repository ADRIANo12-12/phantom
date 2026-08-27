#!/usr/bin/env bash

set -euo pipefail

ROOT="$HOME/phantom"
KERNEL="$ROOT/kernel"
PHANTOM="$KERNEL/phantom"

SYSINFO="$PHANTOM/phantomsysinfo.c"

echo "=== PHANTOM OSD DEVICE SETUP ==="
echo

if ! grep -q '#include "osd.h"' "$SYSINFO"; then
	echo "ERROR: osd.h is not included in phantomsysinfo.c"
	exit 1
fi

echo "[1/3] Adding OSD device initialization..."

python3 - "$SYSINFO" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

old = """        ret = phantom_osd_init();

        if (ret)
                pr_err("Phantom OSD: initialization failed: %d\\n", ret);

        return 0;
"""

new = """        ret = phantom_osd_init();

        if (ret) {
                pr_err("Phantom OSD: initialization failed: %d\\n", ret);
                return ret;
        }

        ret = phantom_osd_device_init();

        if (ret) {
                pr_err("Phantom OSD: device initialization failed: %d\\n", ret);
                return ret;
        }

        pr_info("Phantom OSD: /dev/phantom_osd ready\\n");

        return 0;
"""

if 'phantom_osd_device_init();' not in text:
    if old not in text:
        raise SystemExit("Could not find expected OSD init block")
    text = text.replace(old, new, 1)

path.write_text(text)
PY

echo "[2/3] Verifying source..."

grep -n -A 15 -B 3 'phantom_osd_device_init' "$SYSINFO"

echo
echo "[3/3] Building kernel..."
cd "$KERNEL"
make -j"$(nproc)" bzImage

echo
echo "=== OSD DEVICE SETUP COMPLETE ==="
echo
echo "Kernel contains:"
echo "  phantom_osd_device.o"
echo
echo "Boot will initialize:"
echo "  Phantom OSD"
echo "  /dev/phantom_osd"
echo
echo "Next:"
echo "  cd $ROOT"
echo "  ./setup-phantom-installer-osd.sh"
echo "  ./run-qemu.sh"
