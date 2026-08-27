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

static struct termios saved_terminal;
static int terminal_raw_enabled;

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

	*cols = 80;
	*rows = 24;

	if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == 0) {
		if (ws.ws_col >= 40)
			*cols = ws.ws_col;
		if (ws.ws_row >= 12)
			*rows = ws.ws_row;
	}

	if (*cols > 120)
		*cols = 120;
	if (*rows > 40)
		*rows = 40;
}

static enum phantom_key read_key(void)
{
	unsigned char c;
	unsigned char seq[2];

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

static void set_status(uint32_t window, uint32_t rows, const char *text)
{
	uint32_t y = rows > 4 ? rows - 4 : 1;

	phantom_osd_status(window, 2, (int32_t)y, 70, text);
	phantom_osd_render();
}

int main(void)
{
	int window;
	int ret;
	uint32_t selected = 0;
	uint32_t menu_id = 0;
	uint32_t cols;
	uint32_t rows;

	const char *items[] = {
		"Install Phantom OS",
		"System check",
		"Network setup",
		"Disk setup",
		"Exit",
	};
	const uint32_t menu_count = sizeof(items) / sizeof(items[0]);

	detect_size(&cols, &rows);

	ret = phantom_osd_open();
	if (ret < 0) {
		fprintf(stderr, "Phantom Installer: cannot open /dev/phantom_osd: %d\n", ret);
		return phantom_installer_run();
	}

	phantom_osd_set_desktop(cols, rows);

	window = phantom_osd_create_window(
		"Phantom OS Installer",
		0, 0, cols, rows,
		PHANTOM_OSD_WINDOW_VISIBLE |
		PHANTOM_OSD_WINDOW_FOCUSED |
		PHANTOM_OSD_WINDOW_BORDER);

	if (window < 0) {
		fprintf(stderr, "Phantom Installer: window creation failed: %d\n", window);
		phantom_osd_close();
		return phantom_installer_run();
	}

	phantom_osd_label((uint32_t)window, 2, 1, cols > 6 ? cols - 6 : 20,
			  "Welcome to Phantom OS.");
	phantom_osd_label((uint32_t)window, 2, 2, cols > 6 ? cols - 6 : 20,
			  "UP/DOWN or J/K   ENTER   Q = quit");

	ret = phantom_osd_menu(
		(uint32_t)window,
		2, 5,
		cols > 10 ? cols - 10 : 30,
		menu_count,
		items,
		menu_count,
		&menu_id);

	if (ret < 0) {
		fprintf(stderr, "Phantom Installer: menu creation failed: %d\n", ret);
		phantom_osd_close();
		return phantom_installer_run();
	}

	phantom_osd_progress(
		(uint32_t)window,
		2,
		(int32_t)(rows > 6 ? rows - 6 : 10),
		cols > 8 ? cols - 8 : 20,
		0, 0, NULL);

	phantom_osd_status((uint32_t)window, 2,
			   (int32_t)(rows > 4 ? rows - 4 : 1),
			   70, "Installer ready.");

	phantom_osd_focus((uint32_t)window);
	phantom_osd_menu_set_selected((uint32_t)window, menu_id, selected);

	ret = terminal_enable_raw();
	if (ret < 0) {
		phantom_osd_close();
		return phantom_installer_run();
	}

	phantom_osd_render();

	for (;;) {
		enum phantom_key key = read_key();

		switch (key) {
		case KEY_UP:
			if (selected > 0)
				selected--;
			phantom_osd_menu_set_selected((uint32_t)window, menu_id, selected);
			phantom_osd_render();
			break;

		case KEY_DOWN:
			if (selected + 1 < menu_count)
				selected++;
			phantom_osd_menu_set_selected((uint32_t)window, menu_id, selected);
			phantom_osd_render();
			break;

		case KEY_ENTER:
			terminal_restore();
			printf("\n");

			switch (selected) {
			case 0:
				set_status((uint32_t)window, rows, "Running: Install...");
				phantom_installer_do_install();
				break;
			case 1:
				set_status((uint32_t)window, rows, "Running: System check...");
				phantom_installer_do_system_check();
				break;
			case 2:
				set_status((uint32_t)window, rows, "Running: Network setup...");
				phantom_installer_do_network_setup();
				break;
			case 3:
				set_status((uint32_t)window, rows, "Running: Disk setup...");
				phantom_installer_do_disk_setup();
				break;
			case 4:
				phantom_osd_close();
				printf("\033[2J\033[H\033[?25h");
				return 0;
			}

			printf("\nPress ENTER to return to menu...\n");
			getchar();
			terminal_enable_raw();
			phantom_osd_menu_set_selected((uint32_t)window, menu_id, selected);
			set_status((uint32_t)window, rows, "Installer ready.");
			break;

		case KEY_ESCAPE:
			selected = 0;
			phantom_osd_menu_set_selected((uint32_t)window, menu_id, selected);
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
