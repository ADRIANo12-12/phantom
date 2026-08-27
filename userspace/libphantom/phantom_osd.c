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

	phantom_osd_fd = open("/dev/phantom_osd", O_RDWR);
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
		strncpy(request.title, title, sizeof(request.title) - 1);

	if (ioctl(phantom_osd_fd, PHANTOM_OSD_IOC_CREATE_WINDOW, &request) < 0)
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
		strncpy(request.text, text, sizeof(request.text) - 1);

	if (ioctl(phantom_osd_fd, PHANTOM_OSD_IOC_LABEL, &request) < 0)
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
		strncpy(request.text, text, sizeof(request.text) - 1);

	if (ioctl(phantom_osd_fd, PHANTOM_OSD_IOC_BUTTON, &request) < 0)
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

	for (i = 0; i < count; i++)
		strncpy(request.items[i],
			items[i] ? items[i] : "",
			sizeof(request.items[i]) - 1);

	if (ioctl(phantom_osd_fd, PHANTOM_OSD_IOC_MENU, &request) < 0)
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

	if (ioctl(phantom_osd_fd, PHANTOM_OSD_IOC_PROGRESS, &request) < 0)
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
		strncpy(request.text, text, sizeof(request.text) - 1);

	if (ioctl(phantom_osd_fd, PHANTOM_OSD_IOC_STATUS, &request) < 0)
		return -errno;

	return 0;
}

int phantom_osd_focus(uint32_t window_id)
{
	if (phantom_osd_fd < 0)
		return -ENODEV;

	if (ioctl(phantom_osd_fd, PHANTOM_OSD_IOC_FOCUS, &window_id) < 0)
		return -errno;

	return 0;
}

int phantom_osd_render(void)
{
	if (phantom_osd_fd < 0)
		return -ENODEV;

	if (ioctl(phantom_osd_fd, PHANTOM_OSD_IOC_RENDER) < 0)
		return -errno;

	return 0;
}
