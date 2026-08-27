#!/usr/bin/env bash

set -euo pipefail

ROOT="$HOME/phantom"

KERNEL="$ROOT/kernel"
KPH="$KERNEL/phantom"

USERSPACE="$ROOT/userspace"
LIB="$USERSPACE/libphantom"
INSTALLER="$USERSPACE/phantominstall"
PHANTOMBOX="$USERSPACE/phantombox"

ROOTFS="$ROOT/rootfs"

echo "=== PHANTOM INSTALLER + OSD SETUP ==="
echo

mkdir -p "$LIB"
mkdir -p "$INSTALLER"
mkdir -p "$ROOTFS/bin"

# ============================================================
# 1. Kernel OSD UAPI
# ============================================================

echo "[1/8] Creating kernel OSD UAPI..."

mkdir -p "$KERNEL/include/uapi/linux"

cat > "$KERNEL/include/uapi/linux/phantom_osd.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef _UAPI_LINUX_PHANTOM_OSD_H
#define _UAPI_LINUX_PHANTOM_OSD_H

#include <linux/ioctl.h>

/*
 * Userspace/kernel stable UAPI types.
 *
 * Do NOT include linux/types.h here.
 */
#include <stdint.h>

#define PHANTOM_OSD_IOCTL_MAGIC 'P'

#define PHANTOM_OSD_WINDOW_VISIBLE (1U << 0)
#define PHANTOM_OSD_WINDOW_FOCUSED (1U << 1)
#define PHANTOM_OSD_WINDOW_BORDER  (1U << 2)

struct phantom_osd_uapi_window {
	int32_t x;
	int32_t y;

	uint32_t width;
	uint32_t height;

	uint32_t flags;
	uint32_t window_id;

	char title[64];
};

struct phantom_osd_uapi_text {
	uint32_t window_id;

	int32_t x;
	int32_t y;

	uint32_t width;

	char text[256];
};

struct phantom_osd_uapi_progress {
	uint32_t window_id;

	int32_t x;
	int32_t y;

	uint32_t width;

	uint32_t percent;
	uint64_t eta_seconds;
};

struct phantom_osd_uapi_menu {
	uint32_t window_id;

	int32_t x;
	int32_t y;

	uint32_t width;
	uint32_t height;

	uint32_t count;

	char items[16][64];
};

struct phantom_osd_uapi_button {
	uint32_t window_id;

	int32_t x;
	int32_t y;

	uint32_t width;

	char text[64];
};

#define PHANTOM_OSD_IOC_CREATE_WINDOW \
	_IOWR(PHANTOM_OSD_IOCTL_MAGIC, 1, \
	      struct phantom_osd_uapi_window)

#define PHANTOM_OSD_IOC_LABEL \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 2, \
	     struct phantom_osd_uapi_text)

#define PHANTOM_OSD_IOC_BUTTON \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 3, \
	     struct phantom_osd_uapi_button)

#define PHANTOM_OSD_IOC_MENU \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 4, \
	     struct phantom_osd_uapi_menu)

#define PHANTOM_OSD_IOC_PROGRESS \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 5, \
	     struct phantom_osd_uapi_progress)

#define PHANTOM_OSD_IOC_RENDER \
	_IO(PHANTOM_OSD_IOCTL_MAGIC, 6)

#define PHANTOM_OSD_IOC_FOCUS \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 7, uint32_t)

#define PHANTOM_OSD_IOC_STATUS \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 8, \
	     struct phantom_osd_uapi_text)

#endif
SRC

# ============================================================
# 2. Kernel OSD device
# ============================================================

echo "[2/8] Creating phantom_osd_device.c..."

cat > "$KPH/phantom_osd_device.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#include <uapi/linux/phantom_osd.h>

#include "osd.h"
#include "osd_manager.h"
#include "osd_render.h"

#define PHANTOM_OSD_MAX_WINDOWS 32

static struct phantom_osd_window *
phantom_windows[PHANTOM_OSD_MAX_WINDOWS];

static long phantom_osd_ioctl(
	struct file *file,
	unsigned int command,
	unsigned long argument)
{
	switch (command) {

	case PHANTOM_OSD_IOC_CREATE_WINDOW: {
		struct phantom_osd_uapi_window request;
		struct phantom_osd_window *window;
		int i;

		if (copy_from_user(
			    &request,
			    (void __user *)argument,
			    sizeof(request)))
			return -EFAULT;

		request.title[
			sizeof(request.title) - 1] = '\0';

		window = phantom_osd_window_create(
			request.title,
			request.x,
			request.y,
			request.width,
			request.height,
			request.flags);

		if (!window)
			return -ENOMEM;

		for (i = 0; i < PHANTOM_OSD_MAX_WINDOWS; i++) {

			if (phantom_windows[i])
				continue;

			phantom_windows[i] = window;
			request.window_id = window->id;

			if (copy_to_user(
				    (void __user *)argument,
				    &request,
				    sizeof(request))) {

				phantom_osd_window_destroy(window);
				phantom_windows[i] = NULL;

				return -EFAULT;
			}

			return 0;
		}

		phantom_osd_window_destroy(window);

		return -ENOSPC;
	}

	case PHANTOM_OSD_IOC_LABEL: {
		struct phantom_osd_uapi_text request;
		struct phantom_osd_window *window;

		if (copy_from_user(
			    &request,
			    (void __user *)argument,
			    sizeof(request)))
			return -EFAULT;

		request.text[
			sizeof(request.text) - 1] = '\0';

		window = phantom_osd_manager_find_window(
			request.window_id);

		if (!window)
			return -ENOENT;

		if (!phantom_osd_label_create(
			    window,
			    request.x,
			    request.y,
			    request.width,
			    request.text))
			return -ENOMEM;

		return 0;
	}

	case PHANTOM_OSD_IOC_BUTTON: {
		struct phantom_osd_uapi_button request;
		struct phantom_osd_window *window;

		if (copy_from_user(
			    &request,
			    (void __user *)argument,
			    sizeof(request)))
			return -EFAULT;

		request.text[
			sizeof(request.text) - 1] = '\0';

		window = phantom_osd_manager_find_window(
			request.window_id);

		if (!window)
			return -ENOENT;

		if (!phantom_osd_button_create(
			    window,
			    request.x,
			    request.y,
			    request.width,
			    request.text))
			return -ENOMEM;

		return 0;
	}

	case PHANTOM_OSD_IOC_MENU: {
		struct phantom_osd_uapi_menu request;
		struct phantom_osd_window *window;
		const char *items[16];
		uint32_t i;

		if (copy_from_user(
			    &request,
			    (void __user *)argument,
			    sizeof(request)))
			return -EFAULT;

		if (!request.count ||
		    request.count > 16)
			return -EINVAL;

		if (request.height < request.count)
			return -EINVAL;

		window = phantom_osd_manager_find_window(
			request.window_id);

		if (!window)
			return -ENOENT;

		for (i = 0; i < request.count; i++) {
			request.items[i][
				sizeof(request.items[i]) - 1] = '\0';

			items[i] = request.items[i];
		}

		if (!phantom_osd_menu_create(
			    window,
			    request.x,
			    request.y,
			    request.width,
			    request.height,
			    items,
			    request.count))
			return -ENOMEM;

		return 0;
	}

	case PHANTOM_OSD_IOC_PROGRESS: {
		struct phantom_osd_uapi_progress request;
		struct phantom_osd_window *window;
		struct phantom_osd_widget *widget;

		if (copy_from_user(
			    &request,
			    (void __user *)argument,
			    sizeof(request)))
			return -EFAULT;

		if (request.percent > 100)
			return -EINVAL;

		window = phantom_osd_manager_find_window(
			request.window_id);

		if (!window)
			return -ENOENT;

		widget = phantom_osd_progress_create(
			window,
			request.x,
			request.y,
			request.width);

		if (!widget)
			return -ENOMEM;

		return phantom_osd_progress_set(
			widget,
			request.percent,
			request.eta_seconds);
	}

	case PHANTOM_OSD_IOC_RENDER:
		return phantom_osd_render_frame();

	case PHANTOM_OSD_IOC_FOCUS: {
		uint32_t window_id;
		struct phantom_osd_window *window;

		if (copy_from_user(
			    &window_id,
			    (void __user *)argument,
			    sizeof(window_id)))
			return -EFAULT;

		window = phantom_osd_manager_find_window(
			window_id);

		if (!window)
			return -ENOENT;

		return phantom_osd_window_focus(window);
	}

	case PHANTOM_OSD_IOC_STATUS: {
		struct phantom_osd_uapi_text request;
		struct phantom_osd_window *window;

		if (copy_from_user(
			    &request,
			    (void __user *)argument,
			    sizeof(request)))
			return -EFAULT;

		request.text[
			sizeof(request.text) - 1] = '\0';

		window = phantom_osd_manager_find_window(
			request.window_id);

		if (!window)
			return -ENOENT;

		if (!phantom_osd_status_create(
			    window,
			    request.x,
			    request.y,
			    request.width,
			    request.text))
			return -ENOMEM;

		return 0;
	}

	default:
		return -ENOTTY;
	}
}

static const struct file_operations phantom_osd_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = phantom_osd_ioctl,

#ifdef CONFIG_COMPAT
	.compat_ioctl = compat_ptr_ioctl,
#endif
};

static struct miscdevice phantom_osd_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "phantom_osd",
	.fops = &phantom_osd_fops,
	.mode = 0600,
};

int phantom_osd_device_init(void)
{
	return misc_register(&phantom_osd_device);
}

void phantom_osd_device_shutdown(void)
{
	misc_deregister(&phantom_osd_device);
}
SRC

# ============================================================
# 3. Kernel Makefile
# ============================================================

echo "[3/8] Updating kernel OSD Makefile..."

if ! grep -q '^obj-y += phantom_osd_device.o' "$KPH/Makefile"; then
	printf '\nobj-y += phantom_osd_device.o\n' >> "$KPH/Makefile"
fi

# ============================================================
# 4. Kernel OSD prototypes
# ============================================================

echo "[4/8] Checking OSD device prototypes..."

if ! grep -q 'int phantom_osd_device_init(void);' \
	"$KPH/osd.h"; then

	cat >> "$KPH/osd.h" <<'SRC'

/*
 * OSD userspace device.
 */
int phantom_osd_device_init(void);
void phantom_osd_device_shutdown(void);
SRC
fi

# ============================================================
# 5. Userspace OSD library
# ============================================================

echo "[5/8] Creating libphantom..."

cat > "$LIB/phantom_osd.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_USER_OSD_H
#define PHANTOM_USER_OSD_H

#include <stdint.h>

#define PHANTOM_OSD_WINDOW_VISIBLE (1U << 0)
#define PHANTOM_OSD_WINDOW_FOCUSED (1U << 1)
#define PHANTOM_OSD_WINDOW_BORDER  (1U << 2)

int phantom_osd_open(void);
void phantom_osd_close(void);

int phantom_osd_create_window(
	const char *title,
	int32_t x,
	int32_t y,
	uint32_t width,
	uint32_t height,
	uint32_t flags);

int phantom_osd_label(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	const char *text);

int phantom_osd_button(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	const char *text);

int phantom_osd_menu(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	uint32_t height,
	const char *const *items,
	uint32_t count);

int phantom_osd_progress(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	uint32_t percent,
	uint64_t eta_seconds);

int phantom_osd_status(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	const char *text);

int phantom_osd_focus(uint32_t window_id);
int phantom_osd_render(void);

#endif
SRC

cat > "$LIB/phantom_osd_uapi.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_USER_OSD_UAPI_H
#define PHANTOM_USER_OSD_UAPI_H

#include <stdint.h>
#include <sys/ioctl.h>

#define PHANTOM_OSD_IOCTL_MAGIC 'P'

struct phantom_osd_uapi_window {
	int32_t x;
	int32_t y;

	uint32_t width;
	uint32_t height;

	uint32_t flags;
	uint32_t window_id;

	char title[64];
};

struct phantom_osd_uapi_text {
	uint32_t window_id;

	int32_t x;
	int32_t y;

	uint32_t width;

	char text[256];
};

struct phantom_osd_uapi_progress {
	uint32_t window_id;

	int32_t x;
	int32_t y;

	uint32_t width;

	uint32_t percent;
	uint64_t eta_seconds;
};

struct phantom_osd_uapi_menu {
	uint32_t window_id;

	int32_t x;
	int32_t y;

	uint32_t width;
	uint32_t height;

	uint32_t count;

	char items[16][64];
};

struct phantom_osd_uapi_button {
	uint32_t window_id;

	int32_t x;
	int32_t y;

	uint32_t width;

	char text[64];
};

#define PHANTOM_OSD_IOC_CREATE_WINDOW \
	_IOWR(PHANTOM_OSD_IOCTL_MAGIC, 1, \
	      struct phantom_osd_uapi_window)

#define PHANTOM_OSD_IOC_LABEL \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 2, \
	     struct phantom_osd_uapi_text)

#define PHANTOM_OSD_IOC_BUTTON \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 3, \
	     struct phantom_osd_uapi_button)

#define PHANTOM_OSD_IOC_MENU \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 4, \
	     struct phantom_osd_uapi_menu)

#define PHANTOM_OSD_IOC_PROGRESS \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 5, \
	     struct phantom_osd_uapi_progress)

#define PHANTOM_OSD_IOC_RENDER \
	_IO(PHANTOM_OSD_IOCTL_MAGIC, 6)

#define PHANTOM_OSD_IOC_FOCUS \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 7, uint32_t)

#define PHANTOM_OSD_IOC_STATUS \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 8, \
	     struct phantom_osd_uapi_text)

#endif
SRC

cat > "$LIB/phantom_osd.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include "phantom_osd.h"
#include "phantom_osd_uapi.h"

static int phantom_osd_fd = -1;

int phantom_osd_open(void)
{
	if (phantom_osd_fd >= 0)
		return 0;

	phantom_osd_fd = open(
		"/dev/phantom_osd",
		O_RDWR);

	if (phantom_osd_fd < 0)
		return -errno;

	return 0;
}

void phantom_osd_close(void)
{
	if (phantom_osd_fd >= 0)
		close(phantom_osd_fd);

	phantom_osd_fd = -1;
}

int phantom_osd_create_window(
	const char *title,
	int32_t x,
	int32_t y,
	uint32_t width,
	uint32_t height,
	uint32_t flags)
{
	struct phantom_osd_uapi_window request;

	if (phantom_osd_fd < 0)
		return -ENODEV;

	memset(&request, 0, sizeof(request));

	request.x = x;
	request.y = y;
	request.width = width;
	request.height = height;
	request.flags = flags;

	if (title)
		strncpy(
			request.title,
			title,
			sizeof(request.title) - 1);

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_CREATE_WINDOW,
		    &request) < 0)
		return -errno;

	return (int)request.window_id;
}

int phantom_osd_label(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	const char *text)
{
	struct phantom_osd_uapi_text request;

	if (phantom_osd_fd < 0)
		return -ENODEV;

	memset(&request, 0, sizeof(request));

	request.window_id = window_id;
	request.x = x;
	request.y = y;
	request.width = width;

	strncpy(
		request.text,
		text ? text : "",
		sizeof(request.text) - 1);

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_LABEL,
		    &request) < 0)
		return -errno;

	return 0;
}

int phantom_osd_button(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	const char *text)
{
	struct phantom_osd_uapi_button request;

	if (phantom_osd_fd < 0)
		return -ENODEV;

	memset(&request, 0, sizeof(request));

	request.window_id = window_id;
	request.x = x;
	request.y = y;
	request.width = width;

	strncpy(
		request.text,
		text ? text : "",
		sizeof(request.text) - 1);

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_BUTTON,
		    &request) < 0)
		return -errno;

	return 0;
}

int phantom_osd_menu(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	uint32_t height,
	const char *const *items,
	uint32_t count)
{
	struct phantom_osd_uapi_menu request;
	uint32_t i;

	if (phantom_osd_fd < 0)
		return -ENODEV;

	if (!items || !count || count > 16)
		return -EINVAL;

	if (height < count)
		return -EINVAL;

	memset(&request, 0, sizeof(request));

	request.window_id = window_id;
	request.x = x;
	request.y = y;
	request.width = width;
	request.height = height;
	request.count = count;

	for (i = 0; i < count; i++) {
		strncpy(
			request.items[i],
			items[i] ? items[i] : "",
			sizeof(request.items[i]) - 1);
	}

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_MENU,
		    &request) < 0)
		return -errno;

	return 0;
}

int phantom_osd_progress(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	uint32_t percent,
	uint64_t eta_seconds)
{
	struct phantom_osd_uapi_progress request;

	if (phantom_osd_fd < 0)
		return -ENODEV;

	if (percent > 100)
		return -EINVAL;

	memset(&request, 0, sizeof(request));

	request.window_id = window_id;
	request.x = x;
	request.y = y;
	request.width = width;
	request.percent = percent;
	request.eta_seconds = eta_seconds;

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_PROGRESS,
		    &request) < 0)
		return -errno;

	return 0;
}

int phantom_osd_status(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	const char *text)
{
	struct phantom_osd_uapi_text request;

	if (phantom_osd_fd < 0)
		return -ENODEV;

	memset(&request, 0, sizeof(request));

	request.window_id = window_id;
	request.x = x;
	request.y = y;
	request.width = width;

	strncpy(
		request.text,
		text ? text : "",
		sizeof(request.text) - 1);

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_STATUS,
		    &request) < 0)
		return -errno;

	return 0;
}

int phantom_osd_focus(uint32_t window_id)
{
	if (phantom_osd_fd < 0)
		return -ENODEV;

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_FOCUS,
		    &window_id) < 0)
		return -errno;

	return 0;
}

int phantom_osd_render(void)
{
	if (phantom_osd_fd < 0)
		return -ENODEV;

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_RENDER) < 0)
		return -errno;

	return 0;
}
SRC

cat > "$LIB/Makefile" <<'EOFMAKE'
CC = gcc
AR = ar

CFLAGS = -Wall -Wextra -O2 -static

TARGET = libphantom.a

all: $(TARGET)

$(TARGET): phantom_osd.o
	$(AR) rcs $@ $^

phantom_osd.o: phantom_osd.c phantom_osd.h phantom_osd_uapi.h
	$(CC) $(CFLAGS) -c phantom_osd.c -o phantom_osd.o

clean:
	rm -f phantom_osd.o $(TARGET)
EOFMAKE

# ============================================================
# 6. Installer
# ============================================================

echo "[6/8] Creating phatominstall..."

cat > "$INSTALLER/main.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <unistd.h>

#include "../libphantom/phantom_osd.h"

int main(void)
{
	int window;

	const char *items[] = {
		"Install Phantom OS",
		"System check",
		"Network setup",
		"Disk setup",
		"Exit",
	};

	if (phantom_osd_open() < 0) {
		printf("Phantom Installer: cannot open /dev/phantom_osd\n");
		return 1;
	}

	window = phantom_osd_create_window(
		"Phantom OS Installer",
		0,
		0,
		80,
		25,
		PHANTOM_OSD_WINDOW_VISIBLE |
		PHANTOM_OSD_WINDOW_FOCUSED |
		PHANTOM_OSD_WINDOW_BORDER);

	if (window < 0) {
		printf("Phantom Installer: window creation failed\n");
		phantom_osd_close();
		return 1;
	}

	phantom_osd_label(
		window,
		2,
		2,
		70,
		"Welcome to the Phantom OS installer.");

	phantom_osd_label(
		window,
		2,
		3,
		70,
		"Choose an operation:");

	phantom_osd_menu(
		window,
		2,
		5,
		45,
		5,
		items,
		5);

	phantom_osd_button(
		window,
		52,
		12,
		20,
		"Continue");

	phantom_osd_progress(
		window,
		2,
		16,
		65,
		0,
		0);

	phantom_osd_status(
		window,
		2,
		19,
		70,
		"Installer ready.");

	phantom_osd_focus(window);
	phantom_osd_render();

	for (;;)
		sleep(1);

	return 0;
}
SRC

cat > "$INSTALLER/Makefile" <<'EOFMAKE'
CC = gcc

CFLAGS = -Wall -Wextra -O2 -static

TARGET = phatominstall

LIBDIR = ../libphantom
LIB = $(LIBDIR)/libphantom.a

all: $(TARGET)

$(LIB):
	$(MAKE) -C $(LIBDIR)

$(TARGET): main.o $(LIB)
	$(CC) $(CFLAGS) main.o $(LIB) -o $(TARGET)

main.o: main.c
	$(CC) $(CFLAGS) -I$(LIBDIR) -c main.c -o main.o

clean:
	rm -f main.o $(TARGET)
EOFMAKE

# ============================================================
# 7. Build
# ============================================================

echo "[7/8] Building libphantom..."

cd "$LIB"
make clean
make

echo
echo "[8/8] Building phatominstall..."

cd "$INSTALLER"
make clean
make

cp "$INSTALLER/phatominstall" \
	"$ROOTFS/bin/phatominstall"

chmod +x "$ROOTFS/bin/phatominstall"

echo
echo "=== BUILD COMPLETE ==="
echo
echo "libphantom:"
file "$LIB/libphantom.a"

echo
echo "phatominstall:"
file "$INSTALLER/phatominstall"

echo
echo "rootfs:"
ls -lh "$ROOTFS/bin/phatominstall"
echo
echo "Next:"
echo "  cd $KERNEL"
echo "  make -j\"\$(nproc)\" bzImage"
echo
echo "  cd $ROOT"
echo "  ./build_userspace.sh"
echo
echo "  ./run-qemu.sh"
echo
echo "  install"
