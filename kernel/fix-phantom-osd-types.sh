#!/usr/bin/env bash

set -euo pipefail

ROOT="$HOME/phantom"
KERNEL="$ROOT/kernel"
LIB="$ROOT/userspace/libphantom"

echo "=== FIX PHANTOM OSD UAPI TYPES ==="

mkdir -p "$KERNEL/include/uapi/linux"
mkdir -p "$LIB"

echo "[1/4] Fixing kernel UAPI..."

cat > "$KERNEL/include/uapi/linux/phantom_osd.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef _UAPI_LINUX_PHANTOM_OSD_H
#define _UAPI_LINUX_PHANTOM_OSD_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define PHANTOM_OSD_IOCTL_MAGIC 'P'

#define PHANTOM_OSD_WINDOW_VISIBLE (1U << 0)
#define PHANTOM_OSD_WINDOW_FOCUSED (1U << 1)
#define PHANTOM_OSD_WINDOW_BORDER  (1U << 2)

struct phantom_osd_uapi_window {
	__s32 x;
	__s32 y;

	__u32 width;
	__u32 height;

	__u32 flags;
	__u32 window_id;

	char title[64];
};

struct phantom_osd_uapi_text {
	__u32 window_id;

	__s32 x;
	__s32 y;

	__u32 width;

	char text[256];
};

struct phantom_osd_uapi_progress {
	__u32 window_id;

	__s32 x;
	__s32 y;

	__u32 width;

	__u32 percent;
	__u64 eta_seconds;
};

struct phantom_osd_uapi_menu {
	__u32 window_id;

	__s32 x;
	__s32 y;

	__u32 width;
	__u32 height;

	__u32 count;

	char items[16][64];
};

struct phantom_osd_uapi_button {
	__u32 window_id;

	__s32 x;
	__s32 y;

	__u32 width;

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
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 7, __u32)

#define PHANTOM_OSD_IOC_STATUS \
	_IOW(PHANTOM_OSD_IOCTL_MAGIC, 8, \
	     struct phantom_osd_uapi_text)

#endif
SRC

echo "[2/4] Creating userspace UAPI mirror..."

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

echo "[3/4] Rebuilding libphantom..."

cd "$LIB"
make clean
make

echo "[4/4] Verifying ioctl layouts..."

cat > /tmp/check_phantom_osd.c <<'SRC'
#include <stdio.h>
#include <stdint.h>

#include "phantom_osd_uapi.h"

int main(void)
{
	printf("window=%zu\n", sizeof(struct phantom_osd_uapi_window));
	printf("text=%zu\n", sizeof(struct phantom_osd_uapi_text));
	printf("progress=%zu\n", sizeof(struct phantom_osd_uapi_progress));
	printf("menu=%zu\n", sizeof(struct phantom_osd_uapi_menu));
	printf("button=%zu\n", sizeof(struct phantom_osd_uapi_button));

	return 0;
}
SRC

gcc -Wall -Wextra -I"$LIB" \
	/tmp/check_phantom_osd.c \
	-o /tmp/check_phantom_osd

/tmp/check_phantom_osd

echo
echo "=== OSD UAPI FIXED ==="
echo
echo "Now:"
echo
echo "  cd $KERNEL"
echo "  make -j\"\$(nproc)\" bzImage"
echo
