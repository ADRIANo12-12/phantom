// SPDX-License-Identifier: GPL-2.0

#include <linux/console.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "osd.h"
#include "osd_desktop.h"
#include "osd_manager.h"
#include "osd_render.h"

#define PHANTOM_OSD_MAX_W 200
#define PHANTOM_OSD_MAX_H 60

struct phantom_osd_frame {
	u32 width;
	u32 height;
	char *cells;
	u8 *reverse;
};

static void osd_write(const char *buf, unsigned int len)
{
	struct console *c;

	console_lock();
	for_each_console(c) {
		if ((c->flags & CON_ENABLED) && c->write)
			c->write(c, buf, len);
	}
	console_unlock();
}

static void osd_puts(const char *s)
{
	if (s)
		osd_write(s, strlen(s));
}

static char *row(struct phantom_osd_frame *f, u32 y)
{
	return f->cells + y * (f->width + 1);
}

static void frame_clear(struct phantom_osd_frame *f)
{
	u32 y;

	for (y = 0; y < f->height; y++) {
		memset(row(f, y), ' ', f->width);
		row(f, y)[f->width] = '\0';
		f->reverse[y] = 0;
	}
}

static void frame_put(struct phantom_osd_frame *f, s32 x, s32 y, char c)
{
	if (x < 0 || y < 0)
		return;
	if ((u32)x >= f->width || (u32)y >= f->height)
		return;
	row(f, y)[x] = c;
}

static void frame_text_pad(struct phantom_osd_frame *f,
			   s32 x, s32 y, u32 width, const char *text)
{
	u32 i = 0;

	if (y < 0 || (u32)y >= f->height)
		return;

	for (i = 0; i < width; i++)
		frame_put(f, x + (s32)i, y, ' ');

	i = 0;
	if (!text)
		return;
	while (text[i] && i < width) {
		frame_put(f, x + (s32)i, y, text[i]);
		i++;
	}
}

static void draw_border(struct phantom_osd_frame *f,
			const struct phantom_osd_window *w)
{
	s32 L = w->x, T = w->y;
	s32 R = w->x + (s32)w->width - 1;
	s32 B = w->y + (s32)w->height - 1;
	s32 x, y;

	for (x = L; x <= R; x++) {
		frame_put(f, x, T, '-');
		frame_put(f, x, B, '-');
	}
	for (y = T; y <= B; y++) {
		frame_put(f, L, y, '|');
		frame_put(f, R, y, '|');
	}
	frame_put(f, L, T, '+');
	frame_put(f, R, T, '+');
	frame_put(f, L, B, '+');
	frame_put(f, R, B, '+');
}

static void draw_menu(struct phantom_osd_frame *f,
		      const struct phantom_osd_window *w,
		      const struct phantom_osd_widget *wg)
{
	u32 i;
	char buf[160];
	u32 width = wg->width;

	if (width >= sizeof(buf))
		width = sizeof(buf) - 1;

	for (i = 0; i < wg->menu.count && i < wg->height; i++) {
		const char *item = wg->menu.items[i] ? wg->menu.items[i] : "";
		s32 ly = w->y + 1 + wg->y + (s32)i;
		bool sel = wg->enabled && (i == wg->menu.selected);

		/* BEZ strzałki — sam tekst */
		scnprintf(buf, sizeof(buf), " %-*s",
			  (int)(width > 1 ? width - 1 : 1), item);

		frame_text_pad(f, w->x + 1 + wg->x, ly, wg->width, buf);

		/* Zaznaczenie = podświetlenie całej linii */
		if (sel && ly >= 0 && (u32)ly < f->height)
			f->reverse[ly] = 1;
	}
}

static void draw_label(struct phantom_osd_frame *f,
		       const struct phantom_osd_window *w,
		       const struct phantom_osd_widget *wg)
{
	frame_text_pad(f, w->x + 1 + wg->x, w->y + 1 + wg->y,
		       wg->width, wg->text);
}

static void draw_status(struct phantom_osd_frame *f,
			const struct phantom_osd_window *w,
			const struct phantom_osd_widget *wg)
{
	frame_text_pad(f, w->x + 1 + wg->x, w->y + 1 + wg->y,
		       wg->width, wg->text);
}

static void draw_button(struct phantom_osd_frame *f,
			const struct phantom_osd_window *w,
			const struct phantom_osd_widget *wg)
{
	char buf[128];
	const char *t = wg->text ? wg->text : "";

	scnprintf(buf, sizeof(buf), wg->button.selected ? "[ %s ]" : "  %s  ", t);
	frame_text_pad(f, w->x + 1 + wg->x, w->y + 1 + wg->y, wg->width, buf);
}

static void draw_progress(struct phantom_osd_frame *f,
			  const struct phantom_osd_window *w,
			  const struct phantom_osd_widget *wg)
{
	char buf[160];
	u32 bw = wg->width > 40 ? 40 : wg->width;
	u32 filled, i;
	u64 eta;

	if (bw < 10)
		return;

	filled = (bw * wg->progress.percent) / 100;
	for (i = 0; i < bw; i++)
		buf[i] = (i < filled) ? '#' : '-';
	buf[bw] = '\0';

	eta = wg->progress.eta_seconds;
	scnprintf(buf + bw, sizeof(buf) - bw, " %u%% ETA %02llu:%02llu",
		  wg->progress.percent, eta / 60, eta % 60);

	frame_text_pad(f, w->x + 1 + wg->x, w->y + 1 + wg->y, wg->width, buf);
}

static void draw_separator(struct phantom_osd_frame *f,
			   const struct phantom_osd_window *w,
			   const struct phantom_osd_widget *wg)
{
	u32 i;

	for (i = 0; i < wg->width; i++)
		frame_put(f, w->x + 1 + wg->x + (s32)i, w->y + 1 + wg->y, '-');
}

static int draw_window(struct phantom_osd_window *w, void *data)
{
	struct phantom_osd_frame *f = data;
	struct phantom_osd_widget *wg;

	if (!(w->flags & PHANTOM_OSD_FLAG_VISIBLE))
		return 0;

	if (w->flags & PHANTOM_OSD_FLAG_BORDER)
		draw_border(f, w);

	if (w->title) {
		u32 tw = w->width > 4 ? w->width - 4 : 1;
		u32 i = 0;

		while (w->title[i] && i < tw) {
			frame_put(f, w->x + 2 + (s32)i, w->y, w->title[i]);
			i++;
		}
	}

	list_for_each_entry(wg, &w->widgets, node) {
		if (!wg->visible)
			continue;
		switch (wg->type) {
		case PHANTOM_OSD_WIDGET_LABEL:
			draw_label(f, w, wg);
			break;
		case PHANTOM_OSD_WIDGET_BUTTON:
			draw_button(f, w, wg);
			break;
		case PHANTOM_OSD_WIDGET_MENU:
			draw_menu(f, w, wg);
			break;
		case PHANTOM_OSD_WIDGET_PROGRESS:
			draw_progress(f, w, wg);
			break;
		case PHANTOM_OSD_WIDGET_SEPARATOR:
			draw_separator(f, w, wg);
			break;
		case PHANTOM_OSD_WIDGET_STATUS:
			draw_status(f, w, wg);
			break;
		}
	}
	return 0;
}

int phantom_osd_render_init(void)
{
	return 0;
}

void phantom_osd_render_shutdown(void)
{
}

int phantom_osd_render_frame(void)
{
	struct phantom_osd_frame f;
	int ret;
	u32 y, wi, he;
	char *line;

	wi = phantom_osd_desktop_width();
	he = phantom_osd_desktop_height();
	if (!wi || !he)
		return -ENODEV;
	if (wi > PHANTOM_OSD_MAX_W)
		wi = PHANTOM_OSD_MAX_W;
	if (he > PHANTOM_OSD_MAX_H)
		he = PHANTOM_OSD_MAX_H;

	f.width = wi;
	f.height = he;
	f.cells = kzalloc(he * (wi + 1), GFP_KERNEL);
	f.reverse = kzalloc(he, GFP_KERNEL);
	if (!f.cells || !f.reverse) {
		kfree(f.cells);
		kfree(f.reverse);
		return -ENOMEM;
	}

	frame_clear(&f);
	ret = phantom_osd_manager_for_each_window(draw_window, &f);
	if (ret)
		goto out;

	/* Czyść ekran + ukryj kursor — update w miejscu */
	osd_puts("\033[?25l\033[2J\033[H");

	line = kmalloc(wi + 32, GFP_KERNEL);
	if (!line) {
		ret = -ENOMEM;
		goto out;
	}

	for (y = 0; y < he; y++) {
		if (f.reverse[y])
			scnprintf(line, wi + 32, "\033[7m%.*s\033[0m\n",
				  (int)wi, row(&f, y));
		else
			scnprintf(line, wi + 32, "%.*s\n",
				  (int)wi, row(&f, y));
		osd_puts(line);
	}
	kfree(line);

out:
	kfree(f.cells);
	kfree(f.reverse);
	return ret;
}
