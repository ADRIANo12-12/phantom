// SPDX-License-Identifier: GPL-2.0

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "../libphantom/phantom_osd.h"

enum phantom_key {
	KEY_NONE,
	KEY_UP,
	KEY_DOWN,
	KEY_LEFT,
	KEY_RIGHT,
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

	tcsetattr(
		STDIN_FILENO,
		TCSANOW,
		&saved_terminal);

	terminal_raw_enabled = 0;
}

static int terminal_enable_raw(void)
{
	struct termios raw;

	if (!isatty(STDIN_FILENO))
		return -ENOTTY;

	if (tcgetattr(
		    STDIN_FILENO,
		    &saved_terminal) < 0)
		return -errno;

	raw = saved_terminal;

	/*
	 * Do not let the terminal echo:
	 *
	 * ESC [ A
	 * ESC [ B
	 * ESC [ C
	 * ESC [ D
	 *
	 * to the console.
	 */
	raw.c_lflag &= ~(ICANON | ECHO);

	/*
	 * Read one byte at a time.
	 */
	raw.c_cc[VMIN] = 1;
	raw.c_cc[VTIME] = 0;

	if (tcsetattr(
		    STDIN_FILENO,
		    TCSANOW,
		    &raw) < 0)
		return -errno;

	terminal_raw_enabled = 1;

	atexit(terminal_restore);

	return 0;
}

static enum phantom_key read_key(void)
{
	unsigned char c;
	unsigned char sequence[2];

	if (read(STDIN_FILENO, &c, 1) != 1)
		return KEY_NONE;

	if (c == '\r' || c == '\n')
		return KEY_ENTER;

	if (c == 'q' || c == 'Q')
		return KEY_QUIT;

	if (c != 0x1b)
		return KEY_NONE;

	if (read(STDIN_FILENO, &sequence[0], 1) != 1)
		return KEY_ESCAPE;

	if (sequence[0] != '[')
		return KEY_ESCAPE;

	if (read(STDIN_FILENO, &sequence[1], 1) != 1)
		return KEY_ESCAPE;

	switch (sequence[1]) {
	case 'A':
		return KEY_UP;

	case 'B':
		return KEY_DOWN;

	case 'C':
		return KEY_RIGHT;

	case 'D':
		return KEY_LEFT;

	default:
		return KEY_ESCAPE;
	}
}

static void status(
	uint32_t window,
	const char *text)
{
	(void)phantom_osd_status(
		window,
		2,
		22,
		70,
		text);

	(void)phantom_osd_render();
}

int main(void)
{
	int window;
	int ret;

	uint32_t menu_id;
	uint32_t selected = 0;

	const char *items[] = {
		"Install Phantom OS",
		"System check",
		"Network setup",
		"Disk setup",
		"Exit",
	};

	const uint32_t menu_count =
		sizeof(items) / sizeof(items[0]);

	ret = phantom_osd_open();

	if (ret < 0) {
		fprintf(
			stderr,
			"Phantom Installer: cannot open "
			"/dev/phantom_osd: %d\n",
			ret);
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
		fprintf(
			stderr,
			"Phantom Installer: "
			"window creation failed: %d\n",
			window);

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

	ret = phantom_osd_menu(
		(uint32_t)window,
		2,
		6,
		50,
		menu_count,
		items,
		menu_count,
		&menu_id);

	if (ret < 0) {
		fprintf(
			stderr,
			"Phantom Installer: "
			"menu creation failed: %d\n",
			ret);

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

	phantom_osd_focus(
		(uint32_t)window);

	phantom_osd_menu_set_selected(
		(uint32_t)window,
		menu_id,
		selected);

	ret = terminal_enable_raw();

	if (ret < 0) {
		fprintf(
			stderr,
			"Phantom Installer: "
			"cannot enable raw terminal: %d\n",
			ret);

		phantom_osd_close();

		return 1;
	}

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
				status(
					(uint32_t)window,
					"Install Phantom OS selected.");
				break;

			case 1:
				status(
					(uint32_t)window,
					"System check selected.");
				break;

			case 2:
				status(
					(uint32_t)window,
					"Network setup selected.");
				break;

			case 3:
				status(
					(uint32_t)window,
					"Disk setup selected.");
				break;

			case 4:
				terminal_restore();
				phantom_osd_close();
				return 0;
			}
			break;

		case KEY_ESCAPE:
			selected = 0;

			phantom_osd_menu_set_selected(
				(uint32_t)window,
				menu_id,
				selected);

			phantom_osd_render();
			break;

		case KEY_QUIT:
			terminal_restore();
			phantom_osd_close();
			return 0;

		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_NONE:
		default:
			break;
		}
	}

	terminal_restore();
	phantom_osd_close();

	return 0;
}
