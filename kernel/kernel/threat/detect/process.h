/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Process threat detector interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_DETECTOR_PROCESS_H
#define PHANTOM_DETECTOR_PROCESS_H

#include <linux/types.h>

#include "../core/event.h"
#include "detector.h"

/*
 * Process detector state.
 */
enum phantom_process_detector_state {
	PHANTOM_PROCESS_DETECTOR_DISABLED = 0,
	PHANTOM_PROCESS_DETECTOR_INITIALIZING,
	PHANTOM_PROCESS_DETECTOR_RUNNING,
	PHANTOM_PROCESS_DETECTOR_STOPPING,
	PHANTOM_PROCESS_DETECTOR_ERROR,
};

/*
 * Process observation flags.
 *
 * These describe properties observed on a process. They are not
 * themselves proof that the process is malicious.
 */
enum phantom_process_observation_flags {
	PHANTOM_PROCESS_OBS_NONE		= 0,
	PHANTOM_PROCESS_OBS_ROOT		= 1U << 0,
	PHANTOM_PROCESS_OBS_SETUID		= 1U << 1,
	PHANTOM_PROCESS_OBS_SETGID		= 1U << 2,
	PHANTOM_PROCESS_OBS_KERNEL_THREAD	= 1U << 3,
	PHANTOM_PROCESS_OBS_EXECUTABLE		= 1U << 4,
	PHANTOM_PROCESS_OBS_SUSPICIOUS		= 1U << 5,
	PHANTOM_PROCESS_OBS_CRITICAL		= 1U << 6,
};

/**
 * struct phantom_process_observation - process observation.
 * @pid: Process ID.
 * @tgid: Thread-group ID.
 * @uid: Real user ID.
 * @euid: Effective user ID.
 * @gid: Real group ID.
 * @egid: Effective group ID.
 * @flags: Observation flags.
 * @comm: Task command name.
 */
struct phantom_process_observation {
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
 * struct phantom_process_detector - process detector state.
 * @state: Current detector state.
 * @enabled: Whether process detection is active.
 * @observations: Number of process observations.
 * @suspicious: Number of suspicious observations.
 * @threats: Number of confirmed threat events.
 * @errors: Number of detector errors.
 */
struct phantom_process_detector {
	u8 state;
	bool enabled;

	u64 observations;
	u64 suspicious;
	u64 threats;
	u64 errors;
};

/**
 * phantom_process_detector_init() - initialize process detector.
 * @detector: Process detector instance.
 *
 * Return: 0 on success or negative errno on failure.
 */
int phantom_process_detector_init(
	struct phantom_process_detector *detector);

/**
 * phantom_process_detector_start() - start process detection.
 * @detector: Process detector instance.
 *
 * Return: 0 on success or negative errno on failure.
 */
int phantom_process_detector_start(
	struct phantom_process_detector *detector);

/**
 * phantom_process_detector_stop() - stop process detection.
 * @detector: Process detector instance.
 *
 * Return: 0 on success or negative errno on failure.
 */
int phantom_process_detector_stop(
	struct phantom_process_detector *detector);

/**
 * phantom_process_detector_inspect() - inspect a task.
 * @detector: Process detector instance.
 * @task: Task to inspect.
 * @observation: Destination observation structure.
 *
 * Collects process metadata without modifying the task.
 *
 * Return: 0 on success or negative errno on failure.
 */
int phantom_process_detector_inspect(
	struct phantom_process_detector *detector,
	struct task_struct *task,
	struct phantom_process_observation *observation);

/**
 * phantom_process_detector_classify() - classify an observation.
 * @detector: Process detector instance.
 * @observation: Process observation.
 *
 * Classification is evidence-based and does not perform neutralization.
 *
 * Return: Detector result.
 */
enum phantom_detector_result
phantom_process_detector_classify(
	struct phantom_process_detector *detector,
	const struct phantom_process_observation *observation);

/**
 * phantom_process_detector_create_event() - create event from observation.
 * @detector: Process detector instance.
 * @observation: Process observation.
 *
 * Allocates and initializes a threat event representing the observation.
 *
 * Return: Allocated event or %NULL on failure.
 */
struct phantom_threat_event *
phantom_process_detector_create_event(
	struct phantom_process_detector *detector,
	const struct phantom_process_observation *observation);

/**
 * phantom_process_detector_reset_stats() - reset process statistics.
 * @detector: Process detector instance.
 */
void phantom_process_detector_reset_stats(
	struct phantom_process_detector *detector);

#endif /* PHANTOM_DETECTOR_PROCESS_H */
