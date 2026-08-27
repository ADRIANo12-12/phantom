#!/usr/bin/env bash
set -euo pipefail

ROOT="$HOME/phantom"
KERNEL="$ROOT/kernel"
KPH="$KERNEL/phantom"
LIB="$ROOT/userspace/libphantom"
INSTALLER="$ROOT/userspace/phantominstall"

echo "=== PHANTOM OSD INPUT SETUP ==="
echo

mkdir -p "$LIB" "$INSTALLER"

echo "[1/6] Writing kernel OSD API..."

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

/* Core */
int phantom_osd_init(void);
void phantom_osd_shutdown(void);

/* Windows */
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

/* Widgets */
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

/* Widget state */
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

/* Userspace device */
int phantom_osd_device_init(void);
void phantom_osd_device_shutdown(void);

#endif
SRC

echo "[2/6] Replacing kernel OSD window implementation..."

cat > "$KPH/osd_window.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>

#include "osd.h"
#include "osd_desktop.h"
#include "osd_manager.h"
#include "osd_window.h"

static u32 next_window_id;
static u32 next_widget_id;

static void phantom_osd_widget_free(struct phantom_osd_widget *widget)
{
	u32 i;

	if (!widget)
		return;

	if (widget->type == PHANTOM_OSD_WIDGET_MENU &&
	    widget->menu.items) {
		for (i = 0; i < widget->menu.count; i++)
			kfree(widget->menu.items[i]);

		kfree(widget->menu.items);
	}

	kfree(widget->text);
	kfree(widget);
}

static void phantom_osd_free_widgets(
	struct phantom_osd_window *window)
{
	struct phantom_osd_widget *widget;
	struct phantom_osd_widget *tmp;

	list_for_each_entry_safe(widget, tmp,
				 &window->widgets, node) {
		list_del_init(&widget->node);
		phantom_osd_widget_free(widget);
	}
}

struct phantom_osd_window *
phantom_osd_window_create(const char *title,
			  s32 x,
			  s32 y,
			  u32 width,
			  u32 height,
			  u32 flags)
{
	struct phantom_osd_window *window;
	int ret;

	if (!width || !height)
		return NULL;

	window = kzalloc(sizeof(*window), GFP_KERNEL);
	if (!window)
		return NULL;

	window->id = ++next_window_id;
	window->x = x;
	window->y = y;
	window->width = width;
	window->height = height;
	window->flags = flags;

	if (title) {
		window->title = kstrdup(title, GFP_KERNEL);
		if (!window->title) {
			kfree(window);
			return NULL;
		}
	}

	INIT_LIST_HEAD(&window->node);
	INIT_LIST_HEAD(&window->widgets);

	ret = phantom_osd_manager_add_window(window);
	if (ret) {
		kfree(window->title);
		kfree(window);
		return NULL;
	}

	return window;
}

struct phantom_osd_window *
phantom_osd_window_create_fullscreen(
	const char *title,
	u32 flags)
{
	u32 width;
	u32 height;

	width = phantom_osd_desktop_width();
	height = phantom_osd_desktop_height();

	if (!width || !height)
		return NULL;

	return phantom_osd_window_create(
		title,
		0,
		0,
		width,
		height,
		flags);
}

int phantom_osd_window_destroy(
	struct phantom_osd_window *window)
{
	int ret;

	if (!window)
		return -EINVAL;

	ret = phantom_osd_manager_remove_window(window);
	if (ret)
		return ret;

	phantom_osd_free_widgets(window);

	kfree(window->title);
	kfree(window);

	return 0;
}

int phantom_osd_window_show(
	struct phantom_osd_window *window)
{
	if (!window)
		return -EINVAL;

	window->flags |= PHANTOM_OSD_FLAG_VISIBLE;

	return 0;
}

int phantom_osd_window_hide(
	struct phantom_osd_window *window)
{
	if (!window)
		return -EINVAL;

	window->flags &= ~PHANTOM_OSD_FLAG_VISIBLE;
	window->flags &= ~PHANTOM_OSD_FLAG_FOCUSED;

	return 0;
}

int phantom_osd_window_focus(
	struct phantom_osd_window *window)
{
	return phantom_osd_manager_focus_window(window);
}

int phantom_osd_window_set_title(
	struct phantom_osd_window *window,
	const char *title)
{
	char *new_title;

	if (!window || !title)
		return -EINVAL;

	new_title = kstrdup(title, GFP_KERNEL);
	if (!new_title)
		return -ENOMEM;

	kfree(window->title);
	window->title = new_title;

	return 0;
}

static struct phantom_osd_widget *
phantom_osd_widget_create(
	struct phantom_osd_window *window,
	enum phantom_osd_widget_type type,
	s32 x,
	s32 y,
	u32 width,
	u32 height)
{
	struct phantom_osd_widget *widget;

	if (!window || !width || !height)
		return NULL;

	widget = kzalloc(sizeof(*widget), GFP_KERNEL);
	if (!widget)
		return NULL;

	widget->id = ++next_widget_id;
	widget->type = type;
	widget->x = x;
	widget->y = y;
	widget->width = width;
	widget->height = height;
	widget->visible = true;
	widget->enabled = true;

	INIT_LIST_HEAD(&widget->node);

	list_add_tail(&widget->node, &window->widgets);

	return widget;
}

static int phantom_osd_widget_set_text_internal(
	struct phantom_osd_widget *widget,
	const char *text)
{
	char *new_text;

	if (!widget || !text)
		return -EINVAL;

	new_text = kstrdup(text, GFP_KERNEL);
	if (!new_text)
		return -ENOMEM;

	kfree(widget->text);
	widget->text = new_text;

	return 0;
}

struct phantom_osd_widget *
phantom_osd_widget_find(
	struct phantom_osd_window *window,
	u32 widget_id)
{
	struct phantom_osd_widget *widget;

	if (!window)
		return NULL;

	list_for_each_entry(widget, &window->widgets, node) {
		if (widget->id == widget_id)
			return widget;
	}

	return NULL;
}

struct phantom_osd_widget *
phantom_osd_label_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width,
	const char *text)
{
	struct phantom_osd_widget *widget;

	widget = phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_LABEL,
		x, y, width, 1);

	if (!widget)
		return NULL;

	if (phantom_osd_widget_set_text_internal(widget, text)) {
		list_del_init(&widget->node);
		kfree(widget);
		return NULL;
	}

	return widget;
}

struct phantom_osd_widget *
phantom_osd_button_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width,
	const char *text)
{
	struct phantom_osd_widget *widget;

	widget = phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_BUTTON,
		x, y, width, 1);

	if (!widget)
		return NULL;

	if (phantom_osd_widget_set_text_internal(widget, text)) {
		list_del_init(&widget->node);
		kfree(widget);
		return NULL;
	}

	return widget;
}

struct phantom_osd_widget *
phantom_osd_menu_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width,
	u32 height,
	const char *const *items,
	u32 count)
{
	struct phantom_osd_widget *widget;
	u32 i;

	if (!items || !count || count > height)
		return NULL;

	widget = phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_MENU,
		x, y, width, height);

	if (!widget)
		return NULL;

	widget->menu.items =
		kcalloc(count, sizeof(char *), GFP_KERNEL);

	if (!widget->menu.items) {
		list_del_init(&widget->node);
		kfree(widget);
		return NULL;
	}

	for (i = 0; i < count; i++) {
		widget->menu.items[i] =
			kstrdup(items[i], GFP_KERNEL);

		if (!widget->menu.items[i]) {
			while (i)
				kfree(widget->menu.items[--i]);

			kfree(widget->menu.items);
			list_del_init(&widget->node);
			kfree(widget);

			return NULL;
		}
	}

	widget->menu.count = count;
	widget->menu.selected = 0;

	return widget;
}

struct phantom_osd_widget *
phantom_osd_progress_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width)
{
	struct phantom_osd_widget *widget;

	widget = phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_PROGRESS,
		x, y, width, 1);

	if (!widget)
		return NULL;

	widget->progress.percent = 0;
	widget->progress.eta_seconds = 0;

	return widget;
}

struct phantom_osd_widget *
phantom_osd_separator_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width)
{
	return phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_SEPARATOR,
		x, y, width, 1);
}

struct phantom_osd_widget *
phantom_osd_status_create(
	struct phantom_osd_window *window,
	s32 x,
	s32 y,
	u32 width,
	const char *text)
{
	struct phantom_osd_widget *widget;

	widget = phantom_osd_widget_create(
		window,
		PHANTOM_OSD_WIDGET_STATUS,
		x, y, width, 1);

	if (!widget)
		return NULL;

	if (phantom_osd_widget_set_text_internal(widget, text)) {
		list_del_init(&widget->node);
		kfree(widget);
		return NULL;
	}

	return widget;
}

int phantom_osd_widget_set_text(
	struct phantom_osd_widget *widget,
	const char *text)
{
	return phantom_osd_widget_set_text_internal(
		widget, text);
}

int phantom_osd_widget_show(
	struct phantom_osd_widget *widget)
{
	if (!widget)
		return -EINVAL;

	widget->visible = true;

	return 0;
}

int phantom_osd_widget_hide(
	struct phantom_osd_widget *widget)
{
	if (!widget)
		return -EINVAL;

	widget->visible = false;

	return 0;
}

int phantom_osd_widget_enable(
	struct phantom_osd_widget *widget)
{
	if (!widget)
		return -EINVAL;

	widget->enabled = true;

	return 0;
}

int phantom_osd_widget_disable(
	struct phantom_osd_widget *widget)
{
	if (!widget)
		return -EINVAL;

	widget->enabled = false;

	return 0;
}

int phantom_osd_button_set_selected(
	struct phantom_osd_widget *widget,
	bool selected)
{
	if (!widget ||
	    widget->type != PHANTOM_OSD_WIDGET_BUTTON)
		return -EINVAL;

	widget->button.selected = selected;

	return 0;
}

int phantom_osd_menu_set_selected(
	struct phantom_osd_widget *widget,
	u32 selected)
{
	if (!widget ||
	    widget->type != PHANTOM_OSD_WIDGET_MENU)
		return -EINVAL;

	if (selected >= widget->menu.count)
		return -EINVAL;

	widget->menu.selected = selected;

	return 0;
}

int phantom_osd_progress_set(
	struct phantom_osd_widget *widget,
	u32 percent,
	u64 eta_seconds)
{
	if (!widget ||
	    widget->type != PHANTOM_OSD_WIDGET_PROGRESS)
		return -EINVAL;

	if (percent > 100)
		return -EINVAL;

	widget->progress.percent = percent;
	widget->progress.eta_seconds = eta_seconds;

	return 0;
}
SRC

echo "[3/6] Fixing renderer flag names..."

sed -i \
	-e 's/PHANTOM_OSD_WINDOW_VISIBLE/PHANTOM_OSD_FLAG_VISIBLE/g' \
	-e 's/PHANTOM_OSD_WINDOW_FOCUSED/PHANTOM_OSD_FLAG_FOCUSED/g' \
	-e 's/PHANTOM_OSD_WINDOW_BORDER/PHANTOM_OSD_FLAG_BORDER/g' \
	"$KPH/osd_render.c"

echo "[4/6] Fixing kernel manager flag names..."

sed -i \
	-e 's/PHANTOM_OSD_WINDOW_VISIBLE/PHANTOM_OSD_FLAG_VISIBLE/g' \
	-e 's/PHANTOM_OSD_WINDOW_FOCUSED/PHANTOM_OSD_FLAG_FOCUSED/g' \
	-e 's/PHANTOM_OSD_WINDOW_BORDER/PHANTOM_OSD_FLAG_BORDER/g' \
	"$KPH/osd_manager.c"

echo "[5/6] Adding interactive installer API..."

cat > "$INSTALLER/main.c" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <stdio.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "../libphantom/phantom_osd.h"

enum phantom_key {
	KEY_NONE,
	KEY_UP,
	KEY_DOWN,
	KEY_ENTER,
	KEY_ESC,
	KEY_QUIT,
};

static struct termios saved_termios;
static int raw_mode;

static void restore_terminal(void)
{
	if (!raw_mode)
		return;

	tcsetattr(
		STDIN_FILENO,
		TCSANOW,
		&saved_termios);

	raw_mode = 0;
}

static int enable_raw_terminal(void)
{
	struct termios raw;

	if (!isatty(STDIN_FILENO))
		return -1;

	if (tcgetattr(
		    STDIN_FILENO,
		    &saved_termios) < 0)
		return -1;

	raw = saved_termios;

	raw.c_lflag &= ~(ICANON | ECHO);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(
		    STDIN_FILENO,
		    TCSANOW,
		    &raw) < 0)
		return -1;

	raw_mode = 1;

	atexit(restore_terminal);

	return 0;
}

static enum phantom_key read_key(void)
{
	unsigned char c;
	unsigned char a;
	unsigned char b;

	if (read(STDIN_FILENO, &c, 1) != 1)
		return KEY_NONE;

	if (c == '\n' || c == '\r')
		return KEY_ENTER;

	if (c == 'q' || c == 'Q')
		return KEY_QUIT;

	if (c != 0x1b)
		return KEY_NONE;

	if (read(STDIN_FILENO, &a, 1) != 1)
		return KEY_ESC;

	if (a != '[')
		return KEY_ESC;

	if (read(STDIN_FILENO, &b, 1) != 1)
		return KEY_ESC;

	switch (b) {
	case 'A':
		return KEY_UP;

	case 'B':
		return KEY_DOWN;

	default:
		return KEY_ESC;
	}
}

static void show_status(int window, const char *text)
{
	phantom_osd_status(
		(uint32_t)window,
		2,
		22,
		70,
		text);

	phantom_osd_render();
}

int main(void)
{
	int window;
	uint32_t menu_id;
	unsigned int selected = 0;
	const unsigned int menu_count = 5;

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

	if (phantom_osd_menu(
		    (uint32_t)window,
		    2,
		    6,
		    50,
		    5,
		    items,
		    menu_count,
		    &menu_id) < 0) {

		printf("Phantom Installer: menu creation failed\n");
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

	if (enable_raw_terminal() < 0) {
		printf("Phantom Installer: raw input unavailable\n");
		phantom_osd_close();
		return 1;
	}

	phantom_osd_menu_set_selected(
		(uint32_t)window,
		menu_id,
		selected);

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
				show_status(
					window,
					"Install Phantom OS selected.");
				break;

			case 1:
				show_status(
					window,
					"System check selected.");
				break;

			case 2:
				show_status(
					window,
					"Network setup selected.");
				break;

			case 3:
				show_status(
					window,
					"Disk setup selected.");
				break;

			case 4:
				restore_terminal();
				phantom_osd_close();
				return 0;
			}

			break;

		case KEY_ESC:
			selected = 0;

			phantom_osd_menu_set_selected(
				(uint32_t)window,
				menu_id,
				selected);

			phantom_osd_render();
			break;

		case KEY_QUIT:
			restore_terminal();
			phantom_osd_close();
			return 0;

		default:
			break;
		}
	}
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

echo
echo "=== OSD INPUT SETUP COMPLETE ==="
echo
echo "Build kernel:"
echo "  cd $KERNEL"
echo "  make -j\"\$(nproc)\" bzImage"
echo
echo "Build installer:"
echo "  cd $INSTALLER"
echo "  make clean"
echo "  make"
echo
