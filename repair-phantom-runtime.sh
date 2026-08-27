#!/usr/bin/env bash
set -euo pipefail

ROOT="$HOME/phantom"
KERNEL="$ROOT/kernel"
KPH="$KERNEL/phantom"
LIB="$ROOT/userspace/libphantom"
BOX="$ROOT/userspace/phantombox"
ROOTFS="$ROOT/rootfs"

echo "============================================================"
echo "        PHANTOM RUNTIME REPAIR"
echo "============================================================"
echo

# ------------------------------------------------------------
# 1. PLAIN PHANTOM PANIC SYSCALL
# ------------------------------------------------------------

echo "[1/8] Installing syscall 473..."

SYSCALL_TABLE="$KERNEL/arch/x86/entry/syscalls/syscall_64.tbl"

if ! grep -qE '^[[:space:]]*473[[:space:]]+common[[:space:]]+phantom_panic[[:space:]]' \
	"$SYSCALL_TABLE"; then

	if grep -qE '^[[:space:]]*473[[:space:]]' "$SYSCALL_TABLE"; then
		echo "ERROR: syscall number 473 is already occupied:"
		grep -E '^[[:space:]]*473[[:space:]]' "$SYSCALL_TABLE"
		exit 1
	fi

	printf '%s\n' \
		'473	common	phantom_panic		sys_phantom_panic' \
		>> "$SYSCALL_TABLE"
fi

echo "Syscall 473:"
grep -E '^[[:space:]]*473[[:space:]]+common[[:space:]]+phantom_panic[[:space:]]' \
	"$SYSCALL_TABLE"

# ------------------------------------------------------------
# 2. PHANTOMBOX: PANIC ONLY THROUGH SYSCALL
# ------------------------------------------------------------

echo
echo "[2/8] Removing obsolete /proc panic path..."

cat > "$BOX/main.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include "../libphantom/phantom.h"

#include <string.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
	static const char hellomess[] =
		"Phantom OS shell. Write help for more information.\n"
		"\tAdrian Sikora 2026 Copyright\n"
		"\t\t\tPhantom OS\n\n";

	static const char prompt[] =
		"~/PhantomOS-$ ";

	static const char help[] =
		"\n"
		"help - shows this command\n"
		"exit - exits the shell\n"
		"install - opens Phantom OS installer\n"
		"panic - panics the kernel\n"
		"reboot - restarts the system\n"
		"poweroff - shuts down the system\n";

	char buf[256];
	ssize_t n;

	write(STDOUT_FILENO,
	      hellomess,
	      sizeof(hellomess) - 1);

	for (;;) {
		write(STDOUT_FILENO,
		      prompt,
		      sizeof(prompt) - 1);

		n = read(STDIN_FILENO,
			 buf,
			 sizeof(buf) - 1);

		if (n <= 0)
			break;

		if (buf[n - 1] == '\n')
			n--;

		if (n > 0 && buf[n - 1] == '\r')
			n--;

		buf[n] = '\0';

		if (strcmp(buf, "help") == 0) {
			write(STDOUT_FILENO,
			      help,
			      sizeof(help) - 1);
			continue;
		}

		if (strcmp(buf, "panic") == 0) {
			/*
			 * 473 is the Phantom panic syscall.
			 * The syscall does not return after panic().
			 */
			phantom_panic();
			continue;
		}

		if (strcmp(buf, "poweroff") == 0) {
			reboot(RB_POWER_OFF);
			continue;
		}

		if (strcmp(buf, "reboot") == 0) {
			reboot(RB_AUTOBOOT);
			continue;
		}

		if (strcmp(buf, "install") == 0) {
			pid_t pid;

			pid = fork();

			if (pid == 0) {
				execl("/bin/phatominstall",
				      "phatominstall",
				      (char *)NULL);

				_exit(127);
			}

			if (pid > 0)
				waitpid(pid, NULL, 0);

			continue;
		}

		if (strcmp(buf, "exit") == 0)
			break;
	}

	return 0;
}
SRC

# ------------------------------------------------------------
# 3. REMOVE PROC PANIC INTERFACE FROM KERNEL BUILD
# ------------------------------------------------------------

echo
echo "[3/8] Removing duplicate /proc panic interface..."

sed -i \
	'/^[[:space:]]*obj-y += phantom_panic_usrspc\.o[[:space:]]*$/d' \
	"$KPH/Makefile"

# ------------------------------------------------------------
# 4. VERIFY OSD DEVICE INIT
# ------------------------------------------------------------

echo
echo "[4/8] Verifying OSD device initialization..."

grep -q 'phantom_osd_device_init();' \
	"$KPH/phantomsysinfo.c" || {
	echo "ERROR: phantom_osd_device_init() is missing"
	exit 1
}

grep -q 'obj-y += phantom_osd_device.o' \
	"$KPH/Makefile" || {
	echo "ERROR: phantom_osd_device.o is missing from Kbuild"
	exit 1
}

echo "OSD device initialization: OK"

# ------------------------------------------------------------
# 5. ENABLE DEVTMPFS
# ------------------------------------------------------------

echo
echo "[5/8] Checking devtmpfs support..."

if ! grep -qE '^CONFIG_DEVTMPFS=y$' "$KERNEL/.config"; then
	echo "CONFIG_DEVTMPFS=y" >> "$KERNEL/.config"
fi

# Avoid depending on automatic mount.
# rootfs/init explicitly mounts devtmpfs.

# ------------------------------------------------------------
# 6. FIX ROOTFS INIT DIAGNOSTICS
# ------------------------------------------------------------

echo
echo "[6/8] Updating rootfs init..."

cat > "$ROOTFS/init" <<'SRC'
#!/bin/sh

echo "[PHANTOM] mounting proc..."
/bin/busybox mount -t proc proc /proc

echo "[PHANTOM] mounting sysfs..."
/bin/busybox mount -t sysfs sysfs /sys

echo "[PHANTOM] mounting devtmpfs..."
if /bin/busybox mount -t devtmpfs devtmpfs /dev; then
	echo "[PHANTOM] devtmpfs: OK"
else
	echo "[PHANTOM] devtmpfs: FAILED"
fi

echo
echo "        Phantom OS"
echo
echo "Kernel booted successfully."
echo

if [ -e /dev/phantom_osd ]; then
	echo "Phantom OSD device: /dev/phantom_osd"
else
	echo "WARNING: /dev/phantom_osd is missing"
fi

IFACE=""

for i in /sys/class/net/*; do
	name="${i##*/}"

	if [ "$name" != "lo" ]; then
		IFACE="$name"
		break
	fi
done

if [ -n "$IFACE" ]; then
	echo "Network interface: $IFACE"

	/bin/busybox ip link set "$IFACE" up 2>/dev/null || true

	if /bin/busybox udhcpc \
		-q \
		-n \
		-t 5 \
		-T 3 \
		-i "$IFACE" 2>/dev/null
	then
		echo "Network: DHCP OK"
	else
		echo "Network: DHCP FAILED"
	fi
else
	echo "Network: no interface found"
fi

exec /bin/phantombox
SRC

chmod +x "$ROOTFS/init"

# ------------------------------------------------------------
# 7. BUILD USERSPACE
# ------------------------------------------------------------

echo
echo "[7/8] Building userspace..."

cd "$LIB"
make clean
make

cd "$BOX"
make clean
make

cp "$BOX/phantombox" \
	"$ROOTFS/bin/phantombox"

chmod +x "$ROOTFS/bin/phantombox"

if [ -f "$INSTALLER/phatominstall" ]; then
	cp "$INSTALLER/phatominstall" \
		"$ROOTFS/bin/phatominstall"
	chmod +x "$ROOTFS/bin/phatominstall"
fi

# ------------------------------------------------------------
# 8. FINAL CHECK
# ------------------------------------------------------------

echo
echo "[8/8] Final source checks..."

echo
echo "Phantom syscall:"
grep -n 'SYSCALL_DEFINE0(phantom_panic)' \
	"$KPH/phantom_syscalls.c"

echo
echo "Syscall table:"
grep -E '^[[:space:]]*473[[:space:]]+common[[:space:]]+phantom_panic' \
	"$KERNEL/arch/x86/entry/syscalls/syscall_64.tbl"

echo
echo "OSD device:"
grep -n 'phantom_osd_device_init' \
	"$KPH/phantomsysinfo.c"

echo
echo "OSD Makefile:"
grep 'phantom_osd_device.o' \
	"$KPH/Makefile"

echo
echo "PhantomBox:"
file "$ROOTFS/bin/phantombox"

echo
echo "============================================================"
echo "                 RUNTIME REPAIR READY"
echo "============================================================"
echo
echo "NOW BUILD THE KERNEL:"
echo
echo "  cd ~/phantom/kernel"
echo "  make -j\"\$(nproc)\" bzImage"
echo
echo "THEN:"
echo
echo "  cd ~/phantom"
echo "  ./build_userspace.sh"
echo "  ./run-qemu.sh"
echo
echo "TEST:"
echo
echo "  panic"
echo "  install"
echo
