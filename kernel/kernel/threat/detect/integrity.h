/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Integrity threat detector interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_DETECTOR_INTEGRITY_H
#define PHANTOM_DETECTOR_INTEGRITY_H

#include <linux/types.h>

#include "../core/event.h"
#include "detector.h"

/*
 * Integrity observation flags.
 */
enum phantom_integrity_observation_flags {
	PHANTOM_INTEGRITY_OBS_NONE		= 0,
	PHANTOM_INTEGRITY_OBS_HASH_MISMATCH	= 1U << 0,
	PHANTOM_INTEGRITY_OBS_SIGNATURE_FAIL	= 1U << 1,
	PHANTOM_INTEGRITY_OBS_MEMORY_CHANGE	= 1U << 2,
	PHANTOM_INTEGRITY_OBS_CODE_CHANGE	= 1U << 3,
	PHANTOM_INTEGRITY_OBS_MODULE_CHANGE	= 1U << 4,
	PHANTOM_INTEGRITY_OBS_TAINT		= 1U << 5,
	PHANTOM_INTEGRITY_OBS_SUSPICIOUS	= 1U << 6,
	PHANTOM_INTEGRITY_OBS_CRITICAL		= 1U << 7,
};

/*
 * Integrity detector state.
 */
enum phantom_integrity_detector_state {
	PHANTOM_INTEGRITY_DETECTOR_DISABLED = 0,
	PHANTOM_INTEGRITY_DETECTOR_INITIALIZING,
	PHANTOM_INTEGRITY_DETECTOR_RUNNING,
	PHANTOM_INTEGRITY_DETECTOR_STOPPING,
	PHANTOM_INTEGRITY_DETECTOR_ERROR,
};

/**
 * struct phantom_integrity_observation - integrity evidence.
 * @flags: Integrity observation flags.
 * @object_id: Identifier of the inspected object.
 * @expected_hash: Expected digest representation.
 * @actual_hash: Observed digest representation.
 * @name: Name of the inspected object.
 */
struct phantom_integrity_observation {
	u32 flags;

	u64 object_id;

	u8 expected_hash[64];
	u8 actual_hash[64];

	char name[128];
};

/**
 * struct phantom_integrity_detector - integrity detector state.
 * @state: Current detector state.
 * @enabled: Whether integrity detection is active.
 * @observations: Number of integrity observations.
 * @suspicious: Number of suspicious observations.
 * @threats: Number of confirmed threats.
 * @mismatches: Number of detected integrity mismatches.
 * @errors: Number of detector errors.
 */
struct phantom_integrity_detector {
	u8 state;
	bool enabled;

	u64 observations;
	u64 suspicious;
	u64 threats;
	u64 mismatches;
	u64 errors;
};

/**
 * phantom_integrity_detector_init() - initialize integrity detector.
 * @detector: Integrity detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_integrity_detector_init(
	struct phantom_integrity_detector *detector);

/**
 * phantom_integrity_detector_start() - start integrity detector.
 * @detector: Integrity detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_integrity_detector_start(
	struct phantom_integrity_detector *detector);

/**
 * phantom_integrity_detector_stop() - stop integrity detector.
 * @detector: Integrity detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_integrity_detector_stop(
	struct phantom_integrity_detector *detector);

/**
 * phantom_integrity_detector_inspect() - validate integrity observation.
 * @detector: Integrity detector instance.
 * @observation: Integrity observation.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_integrity_detector_inspect(
	struct phantom_integrity_detector *detector,
	const struct phantom_integrity_observation *observation);

/**
 * phantom_integrity_detector_classify() - classify integrity evidence.
 * @detector: Integrity detector instance.
 * @observation: Integrity observation.
 *
 * Return: Detector classification result.
 */
enum phantom_detector_result
phantom_integrity_detector_classify(
	struct phantom_integrity_detector *detector,
	const struct phantom_integrity_observation *observation);

/**
 * phantom_integrity_detector_create_event() - create threat event.
 * @detector: Integrity detector instance.
 * @observation: Integrity observation.
 *
 * Return: Newly allocated event or %NULL on failure.
 */
struct phantom_threat_event *
phantom_integrity_detector_create_event(
	struct phantom_integrity_detector *detector,
	const struct phantom_integrity_observation *observation);

/**
 * phantom_integrity_detector_reset_stats() - reset integrity statistics.
 * @detector: Integrity detector instance.
 */
void phantom_integrity_detector_reset_stats(
	struct phantom_integrity_detector *detector);

#endif /* PHANTOM_DETECTOR_INTEGRITY_H */
