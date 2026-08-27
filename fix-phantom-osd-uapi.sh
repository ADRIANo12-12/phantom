#!/usr/bin/env bash

set -euo pipefail

ROOT="$HOME/phantom"
LIB="$ROOT/userspace/libphantom"
INSTALLER="$ROOT/userspace/phantominstall"

echo "=== FIX PHANTOM OSD UAPI ==="

cat > "$LIB/phantom_osd.h" <<'SRC'
// SPDX-License-Identifier: GPL-2.0

#ifndef PHANTOM_OSD_H
#define PHANTOM_OSD_H

#include <stdint.h>

/*
 * Phantom OSD window flags.
 */
#define PHANTOM_OSD_WINDOW_VISIBLE (1U << 0)
#define PHANTOM_OSD_WINDOW_FOCUSED (1U << 1)
#define PHANTOM_OSD_WINDOW_BORDER  (1U << 2)

/*
 * Open / close OSD device.
 */
int phantom_osd_open(void);
void phantom_osd_close(void);

/*
 * Window API.
 */
int phantom_osd_create_window(
	const char *title,
	int32_t x,
	int32_t y,
	uint32_t width,
	uint32_t height,
	uint32_t flags);

int phantom_osd_focus(uint32_t window_id);

/*
 * Widgets.
 */
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
	const char *text);

int phantom_osd_menu(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	uint32_t height,
	const char *const *items,
	uint32_t count);

int phantom_osd_progress(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	uint32_t percent,
	uint64_t eta_seconds);

int phantom_osd_status(
	uint32_t window_id,
	int32_t x,
	int32_t y,
	uint32_t width,
	const char *text);

/*
 * Rendering.
 */
int phantom_osd_render(void);

#endif
SRC

echo "[1/2] Rebuilding libphantom..."

cd "$LIB"
make clean
make

echo "[2/2] Rebuilding phatominstall..."

cd "$INSTALLER"
make clean
make

echo
echo "=== FIX COMPLETE ==="
echo
file "$LIB/libphantom.a"
file "$INSTALLER/phatominstall"
echo
