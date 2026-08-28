/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Process scanner interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_SCANNER_PROCESS_H
#define PHANTOM_SCANNER_PROCESS_H

#include <linux/types.h>

#include "../core/event.h"
#include "../detect/detector.h"

/*
 * Process scan state.
 */
enum phantom_process_scanner_state {
	PHANTOM_PROCESS_SCANNER_DISABLED = 0,
	PHANTOM_PROCESS_SCANNER_INITIALIZING,
	PHANTOM_PROCESS_SCANNER_RUNNING,
	PHANTOM_PROCESS_SCANNER_STOPPING,
	PHANTOM_PROCESS_SCANNER_ERROR,
};

/*
 * Process scan findings.
 *
 * Findings represent observations, not automatic malware verdicts.
 */
enum phantom_process_scan_flags {
	PHANTOM_PROCESS_SCAN_NONE		= 0,
	PHANTOM_PROCESS_SCAN_ROOT		= 1U << 0,
	PHANTOM_PROCESS_SCAN_SETUID		= 1U << 1,
	PHANTOM_PROCESS_SCAN_SETGID		= 1U << 2,
	PHANTOM_PROCESS_SCAN_KERNEL_THREAD	= 1U << 3,
	PHANTOM_PROCESS_SCAN_ABNORMAL		= 1U << 4,
	PHANTOM_PROCESS_SCAN_SUSPICIOUS		= 1U << 5,
	PHANTOM_PROCESS_SCAN_CRITICAL		= 1U << 6,
};

/**
 * struct phantom_process_scan - process scan result.
 * @pid: Process ID.
 * @tgid: Thread-group ID.
 * @uid: Real user ID.
 * @euid: Effective user ID.
 * @gid: Real group ID.
 * @egid: Effective group ID.
 * @flags: Scan findings.
 * @comm: Process command name.
 */
struct phantom_process_scan {
	pid_t pid;
	pid_t tgid;

	kuid_t uid;
	kuid_t euid;

	kgid_t gid;
	kgid_t egid;

	u32 flags;

	char comm[TASK_COMM_LEN];
};

/**
 * struct phantom_process_scanner - process scanner state.
 * @state: Current scanner state.
 * @enabled: Whether process scanning is active.
 * @scans: Number of process scans performed.
 * @suspicious: Number of suspicious processes observed.
 * @threats: Number of threat-level findings.
 * @errors: Number of scan errors.
 */
struct phantom_process_scanner {
	u8 state;
	bool enabled;

	u64 scans;
	u64 suspicious;
	u64 threats;
	u64 errors;
};

/**
 * phantom_process_scanner_init() - initialize process scanner.
 * @scanner: Scanner instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_process_scanner_init(
	struct phantom_process_scanner *scanner);

/**
 * phantom_process_scanner_start() - start process scanner.
 * @scanner: Scanner instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_process_scanner_start(
	struct phantom_process_scanner *scanner);

/**
 * phantom_process_scanner_stop() - stop process scanner.
 * @scanner: Scanner instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_process_scanner_stop(
	struct phantom_process_scanner *scanner);

/**
 * phantom_process_scanner_scan_task() - scan one task.
 * @scanner: Scanner instance.
 * @task: Task to inspect.
 * @result: Destination scan result.
 *
 * Performs read-only inspection of the supplied task.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_process_scanner_scan_task(
	struct phantom_process_scanner *scanner,
	struct task_struct *task,
	struct phantom_process_scan *result);

/**
 * phantom_process_scanner_classify() - classify process scan result.
 * @scanner: Scanner instance.
 * @result: Process scan result.
 *
 * Return: Detector classification result.
 */
enum phantom_detector_result
phantom_process_scanner_classify(
	struct phantom_process_scanner *scanner,
	const struct phantom_process_scan *result);

/**
 * phantom_process_scanner_create_event() - create event from scan.
 * @scanner: Scanner instance.
 * @result: Process scan result.
 *
 * Return: Newly allocated event or %NULL on failure.
 */
struct phantom_threat_event *
phantom_process_scanner_create_event(
	struct phantom_process_scanner *scanner,
	const struct phantom_process_scan *result);

/**
 * phantom_process_scanner_reset_stats() - reset scanner statistics.
 * @scanner: Scanner instance.
 */
void phantom_process_scanner_reset_stats(
	struct phantom_process_scanner *scanner);

#endif /* PHANTOM_SCANNER_PROCESS_H */
