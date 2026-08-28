/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Kernel module threat detector interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_DETECTOR_MODULE_H
#define PHANTOM_DETECTOR_MODULE_H

#include <linux/types.h>

#include "../core/event.h"
#include "detector.h"

/*
 * Module observation flags.
 *
 * These flags describe properties observed about a kernel module.
 * They are evidence and must not automatically be treated as proof
 * of malicious behaviour.
 */
enum phantom_module_observation_flags {
	PHANTOM_MODULE_OBS_NONE		= 0,
	PHANTOM_MODULE_OBS_UNSIGNED	= 1U << 0,
	PHANTOM_MODULE_OBS_OUT_OF_TREE	= 1U << 1,
	PHANTOM_MODULE_OBS_TAINTED	= 1U << 2,
	PHANTOM_MODULE_OBS_DUPLICATE	= 1U << 3,
	PHANTOM_MODULE_OBS_SUSPICIOUS	= 1U << 4,
	PHANTOM_MODULE_OBS_CRITICAL	= 1U << 5,
};

/*
 * Kernel module detector state.
 */
enum phantom_module_detector_state {
	PHANTOM_MODULE_DETECTOR_DISABLED = 0,
	PHANTOM_MODULE_DETECTOR_INITIALIZING,
	PHANTOM_MODULE_DETECTOR_RUNNING,
	PHANTOM_MODULE_DETECTOR_STOPPING,
	PHANTOM_MODULE_DETECTOR_ERROR,
};

/**
 * struct phantom_module_observation - observed kernel module metadata.
 * @flags: Module observation flags.
 * @state: Module state when available.
 * @refcnt: Reference count snapshot when available.
 * @core_size: Size of the module's core text/data area.
 * @init_size: Size of the module's initialization area.
 * @name: Module name.
 */
struct phantom_module_observation {
	u32 flags;
	u32 state;
	u32 refcnt;

	u64 core_size;
	u64 init_size;

	char name[MODULE_NAME_LEN];
};

/**
 * struct phantom_module_detector - kernel module detector state.
 * @state: Current detector state.
 * @enabled: Whether module detection is active.
 * @observations: Number of module observations.
 * @suspicious: Number of suspicious observations.
 * @threats: Number of confirmed threat observations.
 * @errors: Number of detector errors.
 */
struct phantom_module_detector {
	u8 state;
	bool enabled;

	u64 observations;
	u64 suspicious;
	u64 threats;
	u64 errors;
};

/**
 * phantom_module_detector_init() - initialize module detector.
 * @detector: Module detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_module_detector_init(
	struct phantom_module_detector *detector);

/**
 * phantom_module_detector_start() - start module detector.
 * @detector: Module detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_module_detector_start(
	struct phantom_module_detector *detector);

/**
 * phantom_module_detector_stop() - stop module detector.
 * @detector: Module detector instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_module_detector_stop(
	struct phantom_module_detector *detector);

/**
 * phantom_module_detector_classify() - classify a module observation.
 * @detector: Module detector instance.
 * @observation: Observation to classify.
 *
 * Return: Detector classification result.
 */
enum phantom_detector_result
phantom_module_detector_classify(
	struct phantom_module_detector *detector,
	const struct phantom_module_observation *observation);

/**
 * phantom_module_detector_create_event() - create event from observation.
 * @detector: Module detector instance.
 * @observation: Module observation.
 *
 * Allocates and initializes a threat event.
 *
 * Return: Newly allocated event or %NULL on failure.
 */
struct phantom_threat_event *
phantom_module_detector_create_event(
	struct phantom_module_detector *detector,
	const struct phantom_module_observation *observation);

/**
 * phantom_module_detector_reset_stats() - reset module detector statistics.
 * @detector: Module detector instance.
 */
void phantom_module_detector_reset_stats(
	struct phantom_module_detector *detector);

#endif /* PHANTOM_DETECTOR_MODULE_H */
