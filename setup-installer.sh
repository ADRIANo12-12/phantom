
#!/usr/bin/env bash

set -euo pipefail

ROOT="$HOME/phantom"
KERNEL="$ROOT/kernel"
PHANTOM_KERNEL="$KERNEL/phantom"

USERSPACE="$ROOT/userspace"
INSTALLER="$USERSPACE/phantominstall"
LIB="$USERSPACE/libphantom"

echo "=== PHANTOM INSTALLER SETUP ==="
echo

mkdir -p "$INSTALLER"
mkdir -p "$LIB"

echo "[1/8] Creating installer headers..."

cat > "$INSTALLER/installer.h" <<'EOF'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_INSTALLER_H
#define PHANTOM_INSTALLER_H

int phantom_installer_run(void);

#endif
EOF

cat > "$INSTALLER/system.h" <<'EOF'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_INSTALLER_SYSTEM_H
#define PHANTOM_INSTALLER_SYSTEM_H

int phantom_installer_check_system(void);

#endif
EOF

cat > "$INSTALLER/network.h" <<'EOF'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_INSTALLER_NETWORK_H
#define PHANTOM_INSTALLER_NETWORK_H

int phantom_installer_network_check(void);

#endif
EOF

cat > "$INSTALLER/package.h" <<'EOF'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_INSTALLER_PACKAGE_H
#define PHANTOM_INSTALLER_PACKAGE_H

int phantom_installer_package_check(void);

#endif
EOF

cat > "$INSTALLER/storage.h" <<'EOF'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_INSTALLER_STORAGE_H
#define PHANTOM_INSTALLER_STORAGE_H

int phantom_installer_storage_check(void);

#endif
EOF

echo "[2/8] Creating system module..."

cat > "$INSTALLER/system.c" <<'EOF'
// SPDX-License-Identifier: GPL-2.0
//
// Phantom OS Installer - system detection
//

#include <stdio.h>
#include <sys/utsname.h>

#include "system.h"

int phantom_installer_check_system(void)
{
	struct utsname info;

	if (uname(&info) < 0)
		return -1;

	printf("System check\n");
	printf("  Kernel      : %s\n", info.release);
	printf("  Architecture: %s\n", info.machine);
	printf("  Host        : %s\n", info.sysname);

	return 0;
}
EOF

echo "[3/8] Creating network module..."

cat > "$INSTALLER/network.c" <<'EOF'
// SPDX-License-Identifier: GPL-2.0
//
// Phantom OS Installer - network detection
//

#include <stdio.h>
#include <unistd.h>

#include "network.h"

int phantom_installer_network_check(void)
{
	if (access("/sys/class/net", R_OK) != 0) {
		printf("Network      : unavailable\n");
		return -1;
	}

	printf("Network      : network subsystem detected\n");

	return 0;
}
EOF

echo "[4/8] Creating storage module..."

cat > "$INSTALLER/storage.c" <<'EOF'
// SPDX-License-Identifier: GPL-2.0
//
// Phantom OS Installer - storage detection
//

#include <stdio.h>
#include <unistd.h>

#include "storage.h"

int phantom_installer_storage_check(void)
{
	if (access("/sys/block", R_OK) != 0) {
		printf("Storage      : unavailable\n");
		return -1;
	}

	printf("Storage      : block subsystem detected\n");

	return 0;
}
EOF

echo "[5/8] Creating package module..."

cat > "$INSTALLER/package.c" <<'EOF'
// SPDX-License-Identifier: GPL-2.0
//
// Phantom OS Installer - package backend
//

#include <stdio.h>

#include "package.h"

int phantom_installer_package_check(void)
{
	/*
	 * libalpm integration comes here.
	 *
	 * This module is intentionally kept separate from
	 * the installer UI and system detection.
	 */
	printf("Package      : ALPM backend not initialized yet\n");

	return 0;
}
EOF

echo "[6/8] Creating installer..."

cat > "$INSTALLER/installer.c" <<'EOF'
// SPDX-License-Identifier: GPL-2.0
//
// Phantom OS Installer
//

#include <stdio.h>

#include "installer.h"
#include "system.h"
#include "network.h"
#include "storage.h"
#include "package.h"

int phantom_installer_run(void)
{
	printf("\n");
	printf("============================================================\n");
	printf("                  PHANTOM OS INSTALLER\n");
	printf("============================================================\n");
	printf("\n");

	printf("Running system checks...\n\n");

	phantom_installer_check_system();
	phantom_installer_network_check();
	phantom_installer_storage_check();
	phantom_installer_package_check();

	printf("\n");
	printf("System checks completed.\n");
	printf("\n");

	return 0;
}
EOF

cat > "$INSTALLER/main.c" <<'EOF'
// SPDX-License-Identifier: GPL-2.0
//
// Phantom OS Installer entry point
//

#include "installer.h"

int main(void)
{
	return phantom_installer_run();
}
EOF

cat > "$INSTALLER/Makefile" <<'EOF'
CC = gcc

CFLAGS = -Wall -Wextra -O2 -static

TARGET = phatominstall

SOURCES = \
	main.c \
	installer.c \
	system.c \
	network.c \
	storage.c \
	package.c

OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS)
EOF

echo "[7/8] Creating Phantom userspace OSD API..."

cat > "$LIB/phantom_osd.h" <<'EOF'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_USER_OSD_H
#define PHANTOM_USER_OSD_H

int phantom_osd_init(void);
int phantom_osd_shutdown(void);

int phantom_osd_open(void);
void phantom_osd_close(void);

#endif
EOF

cat > "$LIB/phantom_osd.c" <<'EOF'
// SPDX-License-Identifier: GPL-2.0
//
// Phantom userspace OSD interface
//

#include <fcntl.h>
#include <unistd.h>

#include "phantom_osd.h"

static int osd_fd = -1;

int phantom_osd_init(void)
{
	osd_fd = open("/dev/phantom_osd", O_RDWR);

	if (osd_fd < 0)
		return -1;

	return 0;
}

int phantom_osd_shutdown(void)
{
	if (osd_fd < 0)
		return 0;

	close(osd_fd);
	osd_fd = -1;

	return 0;
}

int phantom_osd_open(void)
{
	return phantom_osd_init();
}

void phantom_osd_close(void)
{
	phantom_osd_shutdown();
}
EOF

cat > "$LIB/phantom.h" <<'EOF'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_H
#define PHANTOM_H

#include "phantom_osd.h"

void phantom_panic(void);

#endif
EOF

echo "[8/8] Adding installer launch to phantombox..."

PHANTOM_BOX="$USERSPACE/phantombox/main.c"

python3 - "$PHANTOM_BOX" <<'PY'
from pathlib import Path
import sys

path = Path(sys.argv[1])
text = path.read_text()

if '#include <sys/wait.h>' not in text:
    text = text.replace(
        '#include <sys/types.h>',
        '#include <sys/types.h>\n#include <sys/wait.h>'
    )

old = '''if (strcmp(buf, "panic") == 0) {
        phantom_panic();
    }'''

new = '''if (strcmp(buf, "panic") == 0) {
        phantom_panic();
    }

    if (strcmp(buf, "install") == 0) {
        pid_t pid = fork();

        if (pid == 0) {
            execl("/bin/phatominstall",
                  "phatominstall",
                  (char *)NULL);

            _exit(127);
        }

        if (pid > 0)
            waitpid(pid, NULL, 0);
    }'''

if old in text and '"/bin/phatominstall"' not in text:
    text = text.replace(old, new)

path.write_text(text)
PY

cat > "$INSTALLER/README.md" <<'EOF'
# Phantom OS Installer

`phatominstall` is the Phantom OS installation frontend.

Architecture:

    phatominstall
        |
        +-- system
        +-- network
        +-- storage
        +-- package
        |
        +-- Phantom OSD
EOF

echo
echo "============================================================"
echo " PHANTOM INSTALLER SKELETON CREATED"
echo "============================================================"
echo
echo "Installer:"
echo "  $INSTALLER"
echo
echo "Binary:"
echo "  $INSTALLER/phatominstall"
echo
echo "Next build:"
echo "  cd $INSTALLER"
echo "  make"
echo
echo "Test:"
echo "  ./phatominstall"
echo
