/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat scanner interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_SCANNER_H
#define PHANTOM_SCANNER_H

#include <linux/types.h>

#include "../core/event.h"
#include "../core/queue.h"

/*
 * Scanner operating state.
 */
enum phantom_scanner_state {
	PHANTOM_SCANNER_DISABLED = 0,
	PHANTOM_SCANNER_INITIALIZING,
	PHANTOM_SCANNER_IDLE,
	PHANTOM_SCANNER_RUNNING,
	PHANTOM_SCANNER_STOPPING,
	PHANTOM_SCANNER_ERROR,
};

/*
 * Scanner targets.
 *
 * Each bit represents one class of objects that may be scanned.
 */
enum phantom_scanner_target {
	PHANTOM_SCAN_TARGET_UNKNOWN = 0,
	PHANTOM_SCAN_TARGET_PROCESS,
	PHANTOM_SCAN_TARGET_MEMORY,
	PHANTOM_SCAN_TARGET_MODULE,
	PHANTOM_SCAN_TARGET_FILE,
	PHANTOM_SCAN_TARGET_NETWORK,
	PHANTOM_SCAN_TARGET_INTEGRITY,
};

/*
 * Scan result.
 */
enum phantom_scan_result {
	PHANTOM_SCAN_NONE = 0,
	PHANTOM_SCAN_CLEAN,
	PHANTOM_SCAN_SUSPICIOUS,
	PHANTOM_SCAN_THREAT,
	PHANTOM_SCAN_CRITICAL,
	PHANTOM_SCAN_ERROR,
};

/*
 * Scanner operation flags.
 */
enum phantom_scanner_flags {
	PHANTOM_SCANNER_FLAG_NONE		= 0,
	PHANTOM_SCANNER_FLAG_ON_DEMAND		= 1U << 0,
	PHANTOM_SCANNER_FLAG_PERIODIC		= 1U << 1,
	PHANTOM_SCANNER_FLAG_BOOT		= 1U << 2,
	PHANTOM_SCANNER_FLAG_DEEP		= 1U << 3,
	PHANTOM_SCANNER_FLAG_INCREMENTAL	= 1U << 4,
};

/**
 * struct phantom_scanner - Phantom scanner state.
 * @state: Current scanner state.
 * @enabled: Whether scanning is enabled.
 * @targets: Bitmask of enabled scan targets.
 * @flags: Current scanner operation flags.
 * @scans_started: Number of scans started.
 * @scans_completed: Number of scans completed.
 * @threats_found: Number of threats found.
 * @scan_errors: Number of scan errors.
 * @events_queued: Number of scan events submitted to the event queue.
 * @queue: Event queue receiving scanner events.
 */
struct phantom_scanner {
	u8 state;
	bool enabled;

	unsigned long targets;
	u32 flags;

	u64 scans_started;
	u64 scans_completed;
	u64 threats_found;
	u64 scan_errors;
	u64 events_queued;

	struct phantom_event_queue *queue;
};

/**
 * phantom_scanner_init() - initialize scanner.
 * @scanner: Scanner instance.
 * @queue: Event queue used by the scanner.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_init(struct phantom_scanner *scanner,
			 struct phantom_event_queue *queue);

/**
 * phantom_scanner_start() - start scanner.
 * @scanner: Scanner instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_start(struct phantom_scanner *scanner);

/**
 * phantom_scanner_stop() - stop scanner.
 * @scanner: Scanner instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_stop(struct phantom_scanner *scanner);

/**
 * phantom_scanner_enable_target() - enable scan target.
 * @scanner: Scanner instance.
 * @target: Target to enable.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_enable_target(
	struct phantom_scanner *scanner,
	enum phantom_scanner_target target);

/**
 * phantom_scanner_disable_target() - disable scan target.
 * @scanner: Scanner instance.
 * @target: Target to disable.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_disable_target(
	struct phantom_scanner *scanner,
	enum phantom_scanner_target target);

/**
 * phantom_scanner_target_enabled() - test scan target.
 * @scanner: Scanner instance.
 * @target: Target to test.
 *
 * Return: %true if target is enabled, otherwise %false.
 */
bool phantom_scanner_target_enabled(
	const struct phantom_scanner *scanner,
	enum phantom_scanner_target target);

/**
 * phantom_scanner_scan_event() - analyze one event.
 * @scanner: Scanner instance.
 * @event: Event to inspect.
 *
 * Return: Scan result.
 */
enum phantom_scan_result
phantom_scanner_scan_event(
	struct phantom_scanner *scanner,
	struct phantom_threat_event *event);

/**
 * phantom_scanner_submit_event() - submit an event for scanning.
 * @scanner: Scanner instance.
 * @event: Event to submit.
 *
 * The scanner does not take ownership of the caller's reference.
 * The queue acquires its own reference when the event is successfully
 * queued.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_submit_event(
	struct phantom_scanner *scanner,
	struct phantom_threat_event *event);

/**
 * phantom_scanner_run() - run one scanner pass.
 * @scanner: Scanner instance.
 *
 * Executes one scanner pass using the currently enabled targets.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_run(struct phantom_scanner *scanner);

/**
 * phantom_scanner_set_flags() - set scanner operation flags.
 * @scanner: Scanner instance.
 * @flags: New scanner flags.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_set_flags(
	struct phantom_scanner *scanner,
	u32 flags);

/**
 * phantom_scanner_reset_stats() - reset scanner statistics.
 * @scanner: Scanner instance.
 */
void phantom_scanner_reset_stats(struct phantom_scanner *scanner);

#endif /* PHANTOM_SCANNER_H */
