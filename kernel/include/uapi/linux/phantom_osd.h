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
