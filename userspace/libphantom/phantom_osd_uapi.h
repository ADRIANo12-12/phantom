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
