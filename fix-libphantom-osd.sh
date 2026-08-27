#!/usr/bin/env bash

set -euo pipefail

LIB="$HOME/phantom/userspace/libphantom"

echo "=== FIX LIBPHANTOM OSD ==="

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

	if (widget_id)
		*widget_id = request.widget_id;

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

echo "[1/2] Building libphantom..."
cd "$LIB"
make clean
make

echo
echo "[2/2] Verifying symbols..."
nm "$LIB/libphantom.a" | grep -E ' phantom_(panic|osd_menu|osd_menu_set_selected)$'

echo
echo "=== FIXED ==="
