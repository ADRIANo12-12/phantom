// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_OSD_USER_H
#define PHANTOM_OSD_USER_H

#include <stdint.h>

#define PHANTOM_OSD_WINDOW_VISIBLE (1U << 0)
#define PHANTOM_OSD_WINDOW_FOCUSED (1U << 1)
#define PHANTOM_OSD_WINDOW_BORDER  (1U << 2)

int phantom_osd_open(void);
void phantom_osd_close(void);

int phantom_osd_set_desktop(uint32_t width, uint32_t height);

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
	const char *text,
	uint32_t *widget_id);

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
	uint64_t eta_seconds,
	uint32_t *widget_id);

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
