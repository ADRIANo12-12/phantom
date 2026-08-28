/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Syscall threat detector interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_DETECTOR_SYSCALL_H
#define PHANTOM_DETECTOR_SYSCALL_H

#include <linux/types.h>

#include "detector.h"
#include "../core/event.h"

/*
 * Syscall observation flags.
 *
 * These describe facts observed around a syscall. They are not,
 * by themselves, proof of malicious activity.
 */
enum phantom_syscall_observation_flags {
	PHANTOM_SYSCALL_OBS_NONE		= 0,
	PHANTOM_SYSCALL_OBS_PRIVILEGED		= 1U << 0,
	PHANTOM_SYSCALL_OBS_FILESYSTEM		= 1U << 1,
	PHANTOM_SYSCALL_OBS_PROCESS_CONTROL	= 1U << 2,
	PHANTOM_SYSCALL_OBS_MEMORY		= 1U << 3,
	PHANTOM_SYSCALL_OBS_NETWORK		= 1U << 4,
	PHANTOM_SYSCALL_OBS_SUSPICIOUS		= 1U << 5,
	PHANTOM_SYSCALL_OBS_CRITICAL		= 1U << 6,
};

/*
 * Syscall detector state.
 */
enum phantom_syscall_detector_state {
	PHANTOM_SYSCALL_DETECTOR_DISABLED = 0,
	PHANTOM_SYSCALL_DETECTOR_INITIALIZING,
	PHANTOM_SYSCALL_DETECTOR_RUNNING,
	PHANTOM_SYSCALL_DETECTOR_STOPPING,
	PHANTOM_SYSCALL_DETECTOR_ERROR,
};

/**
 * struct phantom_syscall_observation - observed syscall metadata.
 * @nr: Syscall number.
 * @pid: PID of the calling task.
 * @tgid: TGID of the calling task.
 * @flags: Observation flags.
 * @return_value: Return value when available.
 * @comm: Command name of the calling task.
 */
struct phantom_syscall_observation {
	long nr;

	pid_t pid;
	pid_t tgid;

	u32 flags;

	long return_value;

	char comm[TASK_COMM_LEN];
};

/**
 * struct phantom_syscall_detector - syscall detector state.
 * @state: Current detector state.
 * @enabled: Whether syscall detection is active.
 * @observations: Number of syscall observations.
 * @suspicious: Number of suspicious observations.
 * @threats: Number of threat observations.
 * @errors: Number of detector errors.
 */
struct phantom_syscall_detector {
	u8 state;
	bool enabled;

	u64 observations;
	u64 suspicious;
	u64 threats;
	u64 errors;
};

/**
 * phantom_syscall_detector_init() - initialize syscall detector.
 * @detector: Detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_syscall_detector_init(
	struct phantom_syscall_detector *detector);

/**
 * phantom_syscall_detector_start() - enable syscall detector.
 * @detector: Detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_syscall_detector_start(
	struct phantom_syscall_detector *detector);

/**
 * phantom_syscall_detector_stop() - disable syscall detector.
 * @detector: Detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_syscall_detector_stop(
	struct phantom_syscall_detector *detector);

/**
 * phantom_syscall_detector_inspect() - inspect one syscall observation.
 * @detector: Detector instance.
 * @observation: Syscall observation to classify.
 *
 * Does not modify the calling task or syscall execution.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_syscall_detector_inspect(
	struct phantom_syscall_detector *detector,
	const struct phantom_syscall_observation *observation);

/**
 * phantom_syscall_detector_classify() - classify syscall observation.
 * @detector: Detector instance.
 * @observation: Observation to classify.
 *
 * Classification is based only on evidence already collected.
 *
 * Return: Detector result.
 */
enum phantom_detector_result
phantom_syscall_detector_classify(
	struct phantom_syscall_detector *detector,
	const struct phantom_syscall_observation *observation);

/**
 * phantom_syscall_detector_create_event() - create event from observation.
 * @detector: Detector instance.
 * @observation: Syscall observation.
 *
 * Allocates and initializes a Phantom threat event.
 *
 * Return: Newly allocated event or %NULL on failure.
 */
struct phantom_threat_event *
phantom_syscall_detector_create_event(
	struct phantom_syscall_detector *detector,
	const struct phantom_syscall_observation *observation);

/**
 * phantom_syscall_detector_reset_stats() - reset detector statistics.
 * @detector: Detector instance.
 */
void phantom_syscall_detector_reset_stats(
	struct phantom_syscall_detector *detector);

#endif /* PHANTOM_DETECTOR_SYSCALL_H */
