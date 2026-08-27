#!/usr/bin/env bash

set -euo pipefail

ROOT="$HOME/phantom"
KERNEL="$ROOT/kernel"
KPH="$KERNEL/phantom"
USERSPACE="$ROOT/userspace"

LIB="$USERSPACE/libphantom"
BOX="$USERSPACE/phantombox"
INSTALLER="$USERSPACE/phantominstall"
ROOTFS="$ROOT/rootfs"

echo "============================================================"
echo "             PHANTOM COMPLETE USERSpace FIX"
echo "============================================================"
echo

mkdir -p "$LIB" "$BOX" "$INSTALLER" "$ROOTFS/bin"

# ============================================================
# 1. KERNEL OSD HEADER
# ============================================================

echo "[1/10] Writing kernel OSD header..."

cat > "$KPH/osd.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_OSD_H
#define PHANTOM_OSD_H

#include <linux/bitops.h>
#include <linux/list.h>
#include <linux/types.h>

enum phantom_osd_window_flags {
	PHANTOM_OSD_FLAG_VISIBLE = BIT(0),
	PHANTOM_OSD_FLAG_FOCUSED = BIT(1),
	PHANTOM_OSD_FLAG_BORDER  = BIT(2),
};

enum phantom_osd_widget_type {
	PHANTOM_OSD_WIDGET_LABEL,
	PHANTOM_OSD_WIDGET_BUTTON,
	PHANTOM_OSD_WIDGET_MENU,
	PHANTOM_OSD_WIDGET_PROGRESS,
	PHANTOM_OSD_WIDGET_SEPARATOR,
	PHANTOM_OSD_WIDGET_STATUS,
};

struct phantom_osd_widget {
	struct list_head node;

	u32 id;

	enum phantom_osd_widget_type type;

	s32 x;
	s32 y;

	u32 width;
	u32 height;

	bool visible;
	bool enabled;

	char *text;

	union {
		struct {
			bool selected;
		} button;

		struct {
			char **items;
			u32 count;
			u32 selected;
		} menu;

		struct {
			u32 percent;
			u64 eta_seconds;
		} progress;
	};
};

struct phantom_osd_window {
	u32 id;

	s32 x;
	s32 y;

	u32 width;
	u32 height;

	u32 z_order;
	u32 flags;

	char *title;

	struct list_head node;
	struct list_head widgets;

	void *private_data;
};

int phantom_osd_init(void);
void phantom_osd_shutdown(void);

struct phantom_osd_window *
phantom_osd_window_create(const char *title,
			  s32 x,
			  s32 y,
			  u32 width,
			  u32 height,
			  u32 flags);

struct phantom_osd_window *
phantom_osd_window_create_fullscreen(const char *title,
				     u32 flags);

int phantom_osd_window_destroy(struct phantom_osd_window *window);
int phantom_osd_window_show(struct phantom_osd_window *window);
int phantom_osd_window_hide(struct phantom_osd_window *window);
int phantom_osd_window_focus(struct phantom_osd_window *window);

int phantom_osd_window_set_title(struct phantom_osd_window *window,
				 const char *title);

struct phantom_osd_widget *
phantom_osd_label_create(struct phantom_osd_window *window,
			 s32 x,
			 s32 y,
			 u32 width,
			 const char *text);

struct phantom_osd_widget *
phantom_osd_button_create(struct phantom_osd_window *window,
			  s32 x,
			  s32 y,
			  u32 width,
			  const char *text);

struct phantom_osd_widget *
phantom_osd_menu_create(struct phantom_osd_window *window,
			s32 x,
			s32 y,
			u32 width,
			u32 height,
			const char *const *items,
			u32 count);

struct phantom_osd_widget *
phantom_osd_progress_create(struct phantom_osd_window *window,
			    s32 x,
			    s32 y,
			    u32 width);

struct phantom_osd_widget *
phantom_osd_separator_create(struct phantom_osd_window *window,
			     s32 x,
			     s32 y,
			     u32 width);

struct phantom_osd_widget *
phantom_osd_status_create(struct phantom_osd_window *window,
			  s32 x,
			  s32 y,
			  u32 width,
			  const char *text);

struct phantom_osd_widget *
phantom_osd_widget_find(struct phantom_osd_window *window,
			u32 widget_id);

int phantom_osd_widget_set_text(struct phantom_osd_widget *widget,
				const char *text);

int phantom_osd_widget_show(struct phantom_osd_widget *widget);
int phantom_osd_widget_hide(struct phantom_osd_widget *widget);

int phantom_osd_widget_enable(struct phantom_osd_widget *widget);
int phantom_osd_widget_disable(struct phantom_osd_widget *widget);

int phantom_osd_button_set_selected(struct phantom_osd_widget *widget,
				    bool selected);

int phantom_osd_menu_set_selected(struct phantom_osd_widget *widget,
				  u32 selected);

int phantom_osd_progress_set(struct phantom_osd_widget *widget,
			     u32 percent,
			     u64 eta_seconds);

int phantom_osd_device_init(void);
void phantom_osd_device_shutdown(void);

#endif
SRC

# ============================================================
# 2. STABLE USERSpace OSD UAPI
# ============================================================

echo "[2/10] Writing userspace OSD UAPI..."

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

	uint32_t widget_id;
};

struct phantom_osd_uapi_menu {
	uint32_t window_id;

	int32_t x;
	int32_t y;

	uint32_t width;
	uint32_t height;

	uint32_t count;
	uint32_t widget_id;

	char items[16][64];
};

struct phantom_osd_uapi_button {
	uint32_t window_id;

	int32_t x;
	int32_t y;

	uint32_t width;
	uint32_t widget_id;

	char text[64];
};

struct phantom_osd_uapi_widget_selection {
	uint32_t window_id;
	uint32_t widget_id;
	uint32_t selected;
};

#define PHANTOM_OSD_IOC_CREATE_WINDOW \
	_IOWR(PHANTOM_OSD_IOCTL_MAGIC, 1, \
	      struct phantom_osd_uapi_window)

#define PHANTOM_OSD_IOC_LABEL \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 2, \
	     struct phantom_osd_uapi_text)

#define PHANTOM_OSD_IOC_BUTTON \
	_IOWR(PHANTOM_OSD_IOCTL_MAGIC, 3, \
	      struct phantom_osd_uapi_button)

#define PHANTOM_OSD_IOC_MENU \
	_IOWR(PHANTOM_OSD_IOCTL_MAGIC, 4, \
	      struct phantom_osd_uapi_menu)

#define PHANTOM_OSD_IOC_PROGRESS \
	_IOWR(PHANTOM_OSD_IOCTL_MAGIC, 5, \
	      struct phantom_osd_uapi_progress)

#define PHANTOM_OSD_IOC_RENDER \
	_IO(PHANTOM_OSD_IOCTL_MAGIC, 6)

#define PHANTOM_OSD_IOC_FOCUS \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 7, uint32_t)

#define PHANTOM_OSD_IOC_STATUS \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 8, \
	     struct phantom_osd_uapi_text)

#define PHANTOM_OSD_IOC_MENU_SET_SELECTED \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 9, \
	     struct phantom_osd_uapi_widget_selection)

#define PHANTOM_OSD_IOC_BUTTON_SET_SELECTED \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 10, \
	     struct phantom_osd_uapi_widget_selection)

#endif
SRC

# ============================================================
# 3. USER OSD HEADER
# ============================================================

echo "[3/10] Writing libphantom OSD header..."

cat > "$LIB/phantom_osd.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_OSD_USER_H
#define PHANTOM_OSD_USER_H

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
	uint32_t count,
	uint32_t *widget_id);

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

int phantom_osd_menu_set_selected(
	uint32_t window_id,
	uint32_t widget_id,
	uint32_t selected);

int phantom_osd_button_set_selected(
	uint32_t window_id,
	uint32_t widget_id,
	uint32_t selected);

int phantom_osd_focus(uint32_t window_id);

int phantom_osd_render(void);

#endif
SRC

# ============================================================
# 4. USER OSD LIBRARY
# ============================================================

echo "[4/10] Writing libphantom OSD implementation..."

cat > "$LIB/phantom_osd.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
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

	if (text)
		strncpy(
			request.text,
			text,
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

	if (text)
		strncpy(
			request.text,
			text,
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
	uint32_t count,
	uint32_t *widget_id)
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

	for (i = 0; i < count; i++)
		strncpy(
			request.items[i],
			items[i] ? items[i] : "",
			sizeof(request.items[i]) - 1);

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_MENU,
		    &request) < 0)
		return -errno;

	if (widget_id)
		*widget_id = request.widget_id;

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

	if (text)
		strncpy(
			request.text,
			text,
			sizeof(request.text) - 1);

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_STATUS,
		    &request) < 0)
		return -errno;

	return 0;
}

int phantom_osd_menu_set_selected(
	uint32_t window_id,
	uint32_t widget_id,
	uint32_t selected)
{
	struct phantom_osd_uapi_widget_selection request;

	if (phantom_osd_fd < 0)
		return -ENODEV;

	memset(&request, 0, sizeof(request));

	request.window_id = window_id;
	request.widget_id = widget_id;
	request.selected = selected;

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_MENU_SET_SELECTED,
		    &request) < 0)
		return -errno;

	return 0;
}

int phantom_osd_button_set_selected(
	uint32_t window_id,
	uint32_t widget_id,
	uint32_t selected)
{
	struct phantom_osd_uapi_widget_selection request;

	if (phantom_osd_fd < 0)
		return -ENODEV;

	memset(&request, 0, sizeof(request));

	request.window_id = window_id;
	request.widget_id = widget_id;
	request.selected = selected;

	if (ioctl(
		    phantom_osd_fd,
		    PHANTOM_OSD_IOC_BUTTON_SET_SELECTED,
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

cat > "$LIB/phantom.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <sys/syscall.h>
#include <unistd.h>

#ifndef __NR_phantom_panic
#define __NR_phantom_panic 473
#endif

void phantom_panic(void)
{
	(void)syscall(__NR_phantom_panic);
}
SRC

cat > "$LIB/phantom.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_H
#define PHANTOM_H

#include "phantom_osd.h"

void phantom_panic(void);

#endif
SRC

cat > "$LIB/Makefile" <<'SRC'
CC = gcc
AR = ar

CFLAGS = -Wall -Wextra -O2 -static

TARGET = libphantom.a

OBJECTS = \
	phantom.o \
	phantom_osd.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

phantom.o: phantom.c phantom.h
	$(CC) $(CFLAGS) -c phantom.c -o phantom.o

phantom_osd.o: phantom_osd.c phantom_osd.h phantom_osd_uapi.h
	$(CC) $(CFLAGS) -c phantom_osd.c -o phantom_osd.o

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
SRC

# ============================================================
# 5. INTERACTIVE INSTALLER
# ============================================================

echo "[5/10] Writing interactive installer..."

cat > "$INSTALLER/main.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../libphantom/phantom_osd.h"

enum phantom_key {
	KEY_NONE,
	KEY_UP,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
	KEY_ENTER,
	KEY_ESCAPE,
	KEY_QUIT,
};

static struct termios saved_terminal;
static int terminal_raw_enabled;

static void terminal_restore(void)
{
	if (!terminal_raw_enabled)
		return;

	tcsetattr(
		STDIN_FILENO,
		TCSANOW,
		&saved_terminal);

	terminal_raw_enabled = 0;
}

static int terminal_enable_raw(void)
{
	struct termios raw;

	if (!isatty(STDIN_FILENO))
		return -ENOTTY;

	if (tcgetattr(
		    STDIN_FILENO,
		    &saved_terminal) < 0)
		return -errno;

	raw = saved_terminal;

	/*
	 * Do not let the terminal echo:
	 *
	 * ESC [ A
	 * ESC [ B
	 * ESC [ C
	 * ESC [ D
	 *
	 * to the console.
	 */
	raw.c_lflag &= ~(ICANON | ECHO);

	/*
	 * Read one byte at a time.
	 */
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(
		    STDIN_FILENO,
		    TCSANOW,
		    &raw) < 0)
		return -errno;

	terminal_raw_enabled = 1;

	atexit(terminal_restore);

	return 0;
}

static enum phantom_key read_key(void)
{
	unsigned char c;
	unsigned char sequence[2];

	if (read(STDIN_FILENO, &c, 1) != 1)
		return KEY_NONE;

	if (c == '\r' || c == '\n')
		return KEY_ENTER;

	if (c == 'q' || c == 'Q')
		return KEY_QUIT;

	if (c != 0x1b)
		return KEY_NONE;

	if (read(STDIN_FILENO, &sequence[0], 1) != 1)
		return KEY_ESCAPE;

	if (sequence[0] != '[')
		return KEY_ESCAPE;

	if (read(STDIN_FILENO, &sequence[1], 1) != 1)
		return KEY_ESCAPE;

	switch (sequence[1]) {
	case 'A':
		return KEY_UP;

	case 'B':
		return KEY_DOWN;

	case 'C':
		return KEY_RIGHT;

	case 'D':
		return KEY_LEFT;

	default:
		return KEY_ESCAPE;
	}
}

static void status(
	uint32_t window,
	const char *text)
{
	(void)phantom_osd_status(
		window,
		2,
		22,
		70,
		text);

	(void)phantom_osd_render();
}

int main(void)
{
	int window;
	int ret;

	uint32_t menu_id;
	uint32_t selected = 0;

	const char *items[] = {
		"Install Phantom OS",
		"System check",
		"Network setup",
		"Disk setup",
		"Exit",
	};

	const uint32_t menu_count =
		sizeof(items) / sizeof(items[0]);

	ret = phantom_osd_open();

	if (ret < 0) {
		fprintf(
			stderr,
			"Phantom Installer: cannot open "
			"/dev/phantom_osd: %d\n",
			ret);
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
		fprintf(
			stderr,
			"Phantom Installer: "
			"window creation failed: %d\n",
			window);

		phantom_osd_close();

		return 1;
	}

	phantom_osd_label(
		(uint32_t)window,
		2,
		2,
		70,
		"Welcome to Phantom OS.");

	phantom_osd_label(
		(uint32_t)window,
		2,
		3,
		70,
		"UP/DOWN = select   ENTER = confirm   Q = quit");

	ret = phantom_osd_menu(
		(uint32_t)window,
		2,
		6,
		50,
		menu_count,
		items,
		menu_count,
		&menu_id);

	if (ret < 0) {
		fprintf(
			stderr,
			"Phantom Installer: "
			"menu creation failed: %d\n",
			ret);

		phantom_osd_close();

		return 1;
	}

	phantom_osd_button(
		(uint32_t)window,
		58,
		18,
		16,
		"Continue");

	phantom_osd_progress(
		(uint32_t)window,
		2,
		20,
		65,
		0,
		0);

	phantom_osd_status(
		(uint32_t)window,
		2,
		22,
		70,
		"Installer ready.");

	phantom_osd_focus(
		(uint32_t)window);

	phantom_osd_menu_set_selected(
		(uint32_t)window,
		menu_id,
		selected);

	ret = terminal_enable_raw();

	if (ret < 0) {
		fprintf(
			stderr,
			"Phantom Installer: "
			"cannot enable raw terminal: %d\n",
			ret);

		phantom_osd_close();

		return 1;
	}

	phantom_osd_render();

	for (;;) {
		enum phantom_key key;

		key = read_key();

		switch (key) {

		case KEY_UP:
			if (selected > 0)
				selected--;

			phantom_osd_menu_set_selected(
				(uint32_t)window,
				menu_id,
				selected);

			phantom_osd_render();
			break;

		case KEY_DOWN:
			if (selected + 1 < menu_count)
				selected++;

			phantom_osd_menu_set_selected(
				(uint32_t)window,
				menu_id,
				selected);

			phantom_osd_render();
			break;

		case KEY_ENTER:
			switch (selected) {

			case 0:
				status(
					(uint32_t)window,
					"Install Phantom OS selected.");
				break;

			case 1:
				status(
					(uint32_t)window,
					"System check selected.");
				break;

			case 2:
				status(
					(uint32_t)window,
					"Network setup selected.");
				break;

			case 3:
				status(
					(uint32_t)window,
					"Disk setup selected.");
				break;

			case 4:
				terminal_restore();
				phantom_osd_close();
				return 0;
			}
			break;

		case KEY_ESCAPE:
			selected = 0;

			phantom_osd_menu_set_selected(
				(uint32_t)window,
				menu_id,
				selected);

			phantom_osd_render();
			break;

		case KEY_QUIT:
			terminal_restore();
			phantom_osd_close();
			return 0;

		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_NONE:
		default:
			break;
		}
	}

	terminal_restore();
	phantom_osd_close();

	return 0;
}
SRC

cat > "$INSTALLER/Makefile" <<'SRC'
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

.PHONY: all clean
SRC

# ============================================================
# 6. PHANTOMBOX MAKEFILE
# ============================================================

echo "[6/10] Fixing PhantomBox build..."

cat > "$BOX/Makefile" <<'SRC'
CC = gcc

CFLAGS = -Wall -Wextra -O2 -static

TARGET = phantombox

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

.PHONY: all clean
SRC

# ============================================================
# 7. BUILD LIBRARY
# ============================================================

echo "[7/10] Building libphantom..."

cd "$LIB"
make clean
make

# ============================================================
# 8. BUILD INSTALLER + PHANTOMBOX
# ============================================================

echo "[8/10] Building phatominstall..."

cd "$INSTALLER"
make clean
make

echo
echo "[9/10] Building phantombox..."

cd "$BOX"
make clean
make

# ============================================================
# 9. COPY TO ROOTFS + BUILD INITRAMFS
# ============================================================

echo "[10/10] Updating initramfs..."

cp "$BOX/phantombox" \
	"$ROOTFS/bin/phantombox"

cp "$INSTALLER/phatominstall" \
	"$ROOTFS/bin/phatominstall"

chmod +x \
	"$ROOTFS/bin/phantombox" \
	"$ROOTFS/bin/phatominstall"

OUT="$KERNEL/usr/phantom-initramfs.cpio.gz"

cd "$ROOTFS"

find . -print0 \
	| cpio --null -ov --format=newc 2>/dev/null \
	| gzip -9 > "$OUT"

echo
echo "============================================================"
echo "                 PHANTOM BUILD COMPLETE"
echo "============================================================"
echo
echo "PhantomBox:"
file "$ROOTFS/bin/phantombox"
echo
echo "Installer:"
file "$ROOTFS/bin/phatominstall"
echo
echo "Initramfs:"
ls -lh "$OUT"
echo
echo "Next:"
echo
echo "  cd ~/phantom/kernel"
echo "  make -j\"\$(nproc)\" bzImage"
echo
echo "  cd ~/phantom"
echo "  ./run-qemu.sh"
echo
echo "Then:"
echo
echo "  install"
echo
echo "Controls:"
echo "  UP/DOWN = menu"
echo "  ENTER   = select"
echo "  ESC     = first item"
echo "  Q       = quit"
echo
