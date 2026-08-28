// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../libphantom/phantom_osd.h"
#include "installer.h"

enum phantom_key {
	KEY_NONE,
	KEY_UP,
	KEY_DOWN,
	KEY_ENTER,
	KEY_ESCAPE,
	KEY_QUIT,
};

enum view {
	VIEW_MAIN,
	VIEW_ACTION,
};

static struct termios saved_terminal;
static int terminal_raw_enabled;

static uint32_t g_cols = 80;
static uint32_t g_rows = 24;
static uint32_t g_window;
static uint32_t g_menu_id;
static uint32_t g_progress_id;
static uint32_t g_selected;
static enum view g_view = VIEW_MAIN;

static void terminal_restore(void)
{
	if (!terminal_raw_enabled)
		return;
	tcsetattr(STDIN_FILENO, TCSANOW, &saved_terminal);
	terminal_raw_enabled = 0;
	printf("\033[?25h");
	fflush(stdout);
}

static int terminal_enable_raw(void)
{
	struct termios raw;

	if (!isatty(STDIN_FILENO))
		return -ENOTTY;
	if (tcgetattr(STDIN_FILENO, &saved_terminal) < 0)
		return -errno;

	raw = saved_terminal;
	raw.c_lflag &= ~(ICANON | ECHO);
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0)
		return -errno;

	terminal_raw_enabled = 1;
	atexit(terminal_restore);
	return 0;
}

static void detect_size(uint32_t *cols, uint32_t *rows)
{
	struct winsize ws;
	const char *e;

	*cols = 80;
	*rows = 24;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
		if (ws.ws_col >= 40)
			*cols = ws.ws_col;
		if (ws.ws_row >= 12)
			*rows = ws.ws_row;
	}

	if (*cols < 40) {
		e = getenv("COLUMNS");
		if (e && atoi(e) >= 40)
			*cols = (uint32_t)atoi(e);
	}
	if (*rows < 12) {
		e = getenv("LINES");
		if (e && atoi(e) >= 12)
			*rows = (uint32_t)atoi(e);
	}

	if (*cols < 40)
		*cols = 80;
	if (*rows < 12)
		*rows = 24;
	if (*cols > 120)
		*cols = 120;
	if (*rows > 40)
		*rows = 40;
}

static enum phantom_key read_key(void)
{
	unsigned char c, seq[2];

	if (read(STDIN_FILENO, &c, 1) != 1)
		return KEY_NONE;
	if (c == '\r' || c == '\n')
		return KEY_ENTER;
	if (c == 'q' || c == 'Q')
		return KEY_QUIT;
	if (c == 'k' || c == 'K' || c == 'w' || c == 'W')
		return KEY_UP;
	if (c == 'j' || c == 'J' || c == 's' || c == 'S')
		return KEY_DOWN;
	if (c != 0x1b)
		return KEY_NONE;
	if (read(STDIN_FILENO, &seq[0], 1) != 1)
		return KEY_ESCAPE;
	if (seq[0] != '[')
		return KEY_ESCAPE;
	if (read(STDIN_FILENO, &seq[1], 1) != 1)
		return KEY_ESCAPE;
	if (seq[1] == 'A')
		return KEY_UP;
	if (seq[1] == 'B')
		return KEY_DOWN;
	return KEY_ESCAPE;
}

static void ui_status(const char *text)
{
	uint32_t y = g_rows > 3 ? g_rows - 3 : 1;
	uint32_t w = g_cols > 6 ? g_cols - 6 : 20;

	phantom_osd_status(g_window, 2, (int32_t)y, w, text);
}

static void ui_progress(uint32_t percent)
{
	uint32_t y = g_rows > 5 ? g_rows - 5 : 2;
	uint32_t w = g_cols > 8 ? g_cols - 8 : 20;

	/* Nowe widgety progress — kernel na razie nie ma update po id;
	 * kolejne create odświeża pasek na tej samej pozycji. */
	phantom_osd_progress(g_window, 2, (int32_t)y, w, percent, 0, &g_progress_id);
}

static void on_progress(uint32_t percent, const char *msg, void *user)
{
	(void)user;
	ui_status(msg ? msg : "");
	ui_progress(percent);
	phantom_osd_render();
}

static void show_action_header(const char *title)
{
	uint32_t w = g_cols > 6 ? g_cols - 6 : 20;

	phantom_osd_label(g_window, 2, 1, w, title);
	phantom_osd_label(g_window, 2, 2, w,
			  "Working... please wait.  Q aborts to shell after.");
	ui_progress(0);
	ui_status("Starting...");
	phantom_osd_render();
}

static void run_action(uint32_t selected)
{
	g_view = VIEW_ACTION;

	switch (selected) {
	case 0:
		show_action_header("Install Phantom OS");
		phantom_installer_do_install_with_progress(on_progress, NULL);
		ui_status("Install complete. ENTER = back to menu");
		break;
	case 1:
		show_action_header("System check");
		phantom_installer_do_system_check_with_progress(on_progress, NULL);
		ui_status("Check complete. ENTER = back to menu");
		break;
	case 2:
		show_action_header("Network setup");
		phantom_installer_do_network_setup_with_progress(on_progress, NULL);
		ui_status("Network done. ENTER = back to menu");
		break;
	case 3:
		show_action_header("Disk setup");
		phantom_installer_do_disk_setup_with_progress(on_progress, NULL);
		ui_status("Disk setup done. ENTER = back to menu");
		break;
	case 4:
		terminal_restore();
		phantom_osd_close();
		printf("\033[2J\033[H\033[?25h");
		exit(0);
	}

	ui_progress(100);
	phantom_osd_render();

	/* Czekaj na ENTER w raw mode */
	for (;;) {
		enum phantom_key k = read_key();

		if (k == KEY_ENTER || k == KEY_ESCAPE)
			break;
		if (k == KEY_QUIT) {
			terminal_restore();
			phantom_osd_close();
			printf("\033[2J\033[H\033[?25h");
			exit(0);
		}
	}

	/* Powrót: odśwież główne menu (set selected + status) */
	g_view = VIEW_MAIN;
	phantom_osd_menu_set_selected(g_window, g_menu_id, g_selected);
	ui_progress(0);
	ui_status("Installer ready.");
	phantom_osd_render();
}

int main(void)
{
	int window;
	int ret;
	uint32_t menu_id = 0;
	uint32_t label_w, menu_w;

	const char *items[] = {
		"Install Phantom OS",
		"System check",
		"Network setup",
		"Disk setup",
		"Exit",
	};
	const uint32_t menu_count = sizeof(items) / sizeof(items[0]);

	detect_size(&g_cols, &g_rows);

	ret = phantom_osd_open();
	if (ret < 0) {
		fprintf(stderr, "cannot open /dev/phantom_osd: %d\n", ret);
		return phantom_installer_run();
	}

	phantom_osd_set_desktop(g_cols, g_rows);

	window = phantom_osd_create_window(
		"Phantom OS Installer",
		0, 0, g_cols, g_rows,
		PHANTOM_OSD_WINDOW_VISIBLE |
		PHANTOM_OSD_WINDOW_FOCUSED |
		PHANTOM_OSD_WINDOW_BORDER);

	if (window < 0) {
		phantom_osd_close();
		return phantom_installer_run();
	}

	g_window = (uint32_t)window;
	label_w = g_cols > 6 ? g_cols - 6 : 20;
	menu_w = g_cols > 8 ? g_cols - 8 : 30;

	phantom_osd_label(g_window, 2, 1, label_w, "Welcome to Phantom OS.");
	phantom_osd_label(g_window, 2, 2, label_w,
			  "UP/DOWN or J/K   ENTER = open   Q = quit");

	ret = phantom_osd_menu(g_window, 2, 5, menu_w, menu_count,
			       items, menu_count, &menu_id);
	if (ret < 0) {
		phantom_osd_close();
		return phantom_installer_run();
	}

	g_menu_id = menu_id;
	g_selected = 0;

	ui_progress(0);
	ui_status("Installer ready.");
	phantom_osd_focus(g_window);
	phantom_osd_menu_set_selected(g_window, menu_id, 0);

	if (terminal_enable_raw() < 0) {
		phantom_osd_close();
		return phantom_installer_run();
	}

	phantom_osd_render();

	for (;;) {
		enum phantom_key key = read_key();

		if (g_view != VIEW_MAIN)
			continue;

		switch (key) {
		case KEY_UP:
			if (g_selected > 0)
				g_selected--;
			phantom_osd_menu_set_selected(g_window, menu_id, g_selected);
			phantom_osd_render();
			break;

		case KEY_DOWN:
			if (g_selected + 1 < menu_count)
				g_selected++;
			phantom_osd_menu_set_selected(g_window, menu_id, g_selected);
			phantom_osd_render();
			break;

		case KEY_ENTER:
			run_action(g_selected);
			break;

		case KEY_ESCAPE:
			g_selected = 0;
			phantom_osd_menu_set_selected(g_window, menu_id, 0);
			phantom_osd_render();
			break;

		case KEY_QUIT:
			terminal_restore();
			phantom_osd_close();
			printf("\033[2J\033[H\033[?25h");
			return 0;

		default:
			break;
		}
	}

	terminal_restore();
	phantom_osd_close();
	return 0;
}
