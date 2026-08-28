/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Memory threat detector interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_DETECTOR_MEMORY_H
#define PHANTOM_DETECTOR_MEMORY_H

#include <linux/types.h>

#include "../core/event.h"
#include "detector.h"

/*
 * Memory observation flags.
 *
 * These flags represent observed properties. They do not by themselves
 * prove malicious activity.
 */
enum phantom_memory_observation_flags {
	PHANTOM_MEMORY_OBS_NONE		= 0,
	PHANTOM_MEMORY_OBS_WRITABLE_EXEC	= 1U << 0,
	PHANTOM_MEMORY_OBS_EXECUTABLE_WRITABLE	= 1U << 1,
	PHANTOM_MEMORY_OBS_PERMISSION_CHANGE	= 1U << 2,
	PHANTOM_MEMORY_OBS_SUSPICIOUS_REGION	= 1U << 3,
	PHANTOM_MEMORY_OBS_CORRUPTION		= 1U << 4,
	PHANTOM_MEMORY_OBS_SUSPICIOUS		= 1U << 5,
	PHANTOM_MEMORY_OBS_CRITICAL		= 1U << 6,
};

/*
 * Memory detector state.
 */
enum phantom_memory_detector_state {
	PHANTOM_MEMORY_DETECTOR_DISABLED = 0,
	PHANTOM_MEMORY_DETECTOR_INITIALIZING,
	PHANTOM_MEMORY_DETECTOR_RUNNING,
	PHANTOM_MEMORY_DETECTOR_STOPPING,
	PHANTOM_MEMORY_DETECTOR_ERROR,
};

/**
 * struct phantom_memory_observation - observed memory metadata.
 * @address: Start address of the observed region.
 * @size: Size of the observed region.
 * @flags: Memory observation flags.
 * @pid: PID associated with the observation, if applicable.
 * @tgid: TGID associated with the observation, if applicable.
 * @prot: Observed memory protection bits.
 */
struct phantom_memory_observation {
	unsigned long address;
	u64 size;

	u32 flags;
	u32 prot;

	pid_t pid;
	pid_t tgid;
};

/**
 * struct phantom_memory_detector - memory detector state.
 * @state: Current detector state.
 * @enabled: Whether memory detection is active.
 * @observations: Number of memory observations.
 * @suspicious: Number of suspicious observations.
 * @threats: Number of confirmed threat observations.
 * @errors: Number of detector errors.
 */
struct phantom_memory_detector {
	u8 state;
	bool enabled;

	u64 observations;
	u64 suspicious;
	u64 threats;
	u64 errors;
};

/**
 * phantom_memory_detector_init() - initialize memory detector.
 * @detector: Memory detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_memory_detector_init(
	struct phantom_memory_detector *detector);

/**
 * phantom_memory_detector_start() - start memory detector.
 * @detector: Memory detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_memory_detector_start(
	struct phantom_memory_detector *detector);

/**
 * phantom_memory_detector_stop() - stop memory detector.
 * @detector: Memory detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_memory_detector_stop(
	struct phantom_memory_detector *detector);

/**
 * phantom_memory_detector_inspect() - validate a memory observation.
 * @detector: Memory detector instance.
 * @observation: Memory observation.
 *
 * Validates the observation and records detector statistics.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_memory_detector_inspect(
	struct phantom_memory_detector *detector,
	const struct phantom_memory_observation *observation);

/**
 * phantom_memory_detector_classify() - classify memory observation.
 * @detector: Memory detector instance.
 * @observation: Observation to classify.
 *
 * Return: Detector classification result.
 */
enum phantom_detector_result
phantom_memory_detector_classify(
	struct phantom_memory_detector *detector,
	const struct phantom_memory_observation *observation);

/**
 * phantom_memory_detector_create_event() - create event from observation.
 * @detector: Memory detector instance.
 * @observation: Memory observation.
 *
 * Allocates and initializes a threat event from the observation.
 *
 * Return: Newly allocated event or %NULL on failure.
 */
struct phantom_threat_event *
phantom_memory_detector_create_event(
	struct phantom_memory_detector *detector,
	const struct phantom_memory_observation *observation);

/**
 * phantom_memory_detector_reset_stats() - reset memory statistics.
 * @detector: Memory detector instance.
 */
void phantom_memory_detector_reset_stats(
	struct phantom_memory_detector *detector);

#endif /* PHANTOM_DETECTOR_MEMORY_H */
