#!/usr/bin/env bash

set -euo pipefail

ROOT="$HOME/phantom"
KERNEL="$ROOT/kernel"
KPH="$KERNEL/phantom"

echo "=== FIX PHANTOM OSD FLAG NAMES ==="

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

/*
 * OSD core.
 */
int phantom_osd_init(void);
void phantom_osd_shutdown(void);

/*
 * Window API.
 */
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

/*
 * Widget API.
 */
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

/*
 * OSD userspace device.
 */
int phantom_osd_device_init(void);
void phantom_osd_device_shutdown(void);

#endif
SRC

# Zamień stare nazwy tylko w kodzie kernela OSD.
for file in \
	"$KPH/osd.c" \
	"$KPH/osd_window.c" \
	"$KPH/osd_manager.c" \
	"$KPH/osd_render.c" \
	"$KPH/phantom_osd_device.c"
do
	if [ -f "$file" ]; then
		sed -i \
			-e 's/PHANTOM_OSD_WINDOW_VISIBLE/PHANTOM_OSD_FLAG_VISIBLE/g' \
			-e 's/PHANTOM_OSD_WINDOW_FOCUSED/PHANTOM_OSD_FLAG_FOCUSED/g' \
			-e 's/PHANTOM_OSD_WINDOW_BORDER/PHANTOM_OSD_FLAG_BORDER/g' \
			"$file"
	fi
done

echo
echo "=== FIX COMPLETE ==="
echo
echo "Kernel flags:"
echo "  PHANTOM_OSD_FLAG_VISIBLE"
echo "  PHANTOM_OSD_FLAG_FOCUSED"
echo "  PHANTOM_OSD_FLAG_BORDER"
echo
echo "UAPI flags remain:"
echo "  PHANTOM_OSD_WINDOW_VISIBLE"
echo "  PHANTOM_OSD_WINDOW_FOCUSED"
echo "  PHANTOM_OSD_WINDOW_BORDER"
echo
echo "Now build:"
echo "  cd $KERNEL"
echo "  make -j\"\$(nproc)\" bzImage"
