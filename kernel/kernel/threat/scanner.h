/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat scanner interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_THREAT_SCANNER_H
#define PHANTOM_THREAT_SCANNER_H

#include <linux/types.h>

#include "event.h"

/*
 * Maximum number of scanner targets that can be enabled
 * simultaneously.
 */
#define PHANTOM_SCANNER_MAX_TARGETS	8

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
 * Scanner target.
 *
 * Each target represents a different class of kernel-visible
 * objects that the scanner may inspect.
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
 * Result of one scan operation.
 */
enum phantom_scan_result {
	PHANTOM_SCAN_NONE = 0,
	PHANTOM_SCAN_CLEAN,
	PHANTOM_SCAN_SUSPICIOUS,
	PHANTOM_SCAN_THREAT,
	PHANTOM_SCAN_CRITICAL,
	PHANTOM_SCAN_ERROR,
};

/**
 * struct phantom_scanner - scanner subsystem state.
 * @state: Current scanner state.
 * @enabled: Whether scanning is enabled.
 * @targets: Bitmask of enabled scan targets.
 * @scans_started: Number of scan operations started.
 * @scans_completed: Number of successfully completed scans.
 * @threats_found: Number of threats detected during scans.
 * @scan_errors: Number of scan operations that failed.
 */
struct phantom_scanner {
	u8 state;
	bool enabled;

	unsigned long targets;

	u64 scans_started;
	u64 scans_completed;
	u64 threats_found;
	u64 scan_errors;
};

/**
 * phantom_scanner_init() - initialize scanner subsystem.
 * @scanner: Scanner instance.
 *
 * Initializes scanner state and clears all scan targets.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_init(struct phantom_scanner *scanner);

/**
 * phantom_scanner_start() - start scanner.
 * @scanner: Scanner instance.
 *
 * Enables scanning and places the scanner into the idle state.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_start(struct phantom_scanner *scanner);

/**
 * phantom_scanner_stop() - stop scanner.
 * @scanner: Scanner instance.
 *
 * Stops new scans from being started.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_stop(struct phantom_scanner *scanner);

/**
 * phantom_scanner_enable_target() - enable a scan target.
 * @scanner: Scanner instance.
 * @target: Target to enable.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_enable_target(struct phantom_scanner *scanner,
				  enum phantom_scanner_target target);

/**
 * phantom_scanner_disable_target() - disable a scan target.
 * @scanner: Scanner instance.
 * @target: Target to disable.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_disable_target(struct phantom_scanner *scanner,
				   enum phantom_scanner_target target);

/**
 * phantom_scanner_is_target_enabled() - check scan target state.
 * @scanner: Scanner instance.
 * @target: Target to check.
 *
 * Return: %true if enabled, otherwise %false.
 */
bool phantom_scanner_is_target_enabled(
	struct phantom_scanner *scanner,
	enum phantom_scanner_target target);

/**
 * phantom_scanner_scan_event() - scan an existing threat event.
 * @scanner: Scanner instance.
 * @event: Event to inspect.
 *
 * Performs scanner-side analysis of an existing event.
 *
 * Return: Scan result.
 */
enum phantom_scan_result
phantom_scanner_scan_event(struct phantom_scanner *scanner,
			   struct phantom_threat_event *event);

/**
 * phantom_scanner_submit_event() - submit an event to the scanner.
 * @scanner: Scanner instance.
 * @event: Event to scan.
 *
 * Scans @event and updates scanner statistics.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_submit_event(struct phantom_scanner *scanner,
				 struct phantom_threat_event *event);

/**
 * phantom_scanner_reset_stats() - reset scanner statistics.
 * @scanner: Scanner instance.
 *
 * Resets all scanner counters to zero.
 */
void phantom_scanner_reset_stats(struct phantom_scanner *scanner);

#endif /* PHANTOM_THREAT_SCANNER_H */
