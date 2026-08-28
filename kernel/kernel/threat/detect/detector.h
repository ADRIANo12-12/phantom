/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat detector interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_DETECTOR_H
#define PHANTOM_DETECTOR_H

#include <linux/types.h>

#include "../core/event.h"
#include "../core/queue.h"

/*
 * Detector operating state.
 */
enum phantom_detector_state {
	PHANTOM_DETECTOR_DISABLED = 0,
	PHANTOM_DETECTOR_INITIALIZING,
	PHANTOM_DETECTOR_RUNNING,
	PHANTOM_DETECTOR_STOPPING,
	PHANTOM_DETECTOR_ERROR,
};

/*
 * Detector source.
 */
enum phantom_detector_source {
	PHANTOM_DETECTOR_SOURCE_UNKNOWN = 0,
	PHANTOM_DETECTOR_SOURCE_SYSCALL,
	PHANTOM_DETECTOR_SOURCE_PROCESS,
	PHANTOM_DETECTOR_SOURCE_MEMORY,
	PHANTOM_DETECTOR_SOURCE_FILE,
	PHANTOM_DETECTOR_SOURCE_NETWORK,
	PHANTOM_DETECTOR_SOURCE_MODULE,
	PHANTOM_DETECTOR_SOURCE_INTEGRITY,
};

/*
 * Detector result.
 */
enum phantom_detector_result {
	PHANTOM_DETECTOR_RESULT_NONE = 0,
	PHANTOM_DETECTOR_RESULT_CLEAN,
	PHANTOM_DETECTOR_RESULT_SUSPICIOUS,
	PHANTOM_DETECTOR_RESULT_THREAT,
	PHANTOM_DETECTOR_RESULT_CRITICAL,
	PHANTOM_DETECTOR_RESULT_ERROR,
};

/**
 * struct phantom_detector - Phantom detection engine.
 * @state: Current detector state.
 * @enabled: Whether the detector is active.
 * @sources: Bitmask of enabled detector sources.
 * @events_received: Events received for analysis.
 * @events_analyzed: Events successfully analyzed.
 * @events_detected: Threats detected.
 * @events_rejected: Invalid events rejected.
 * @analysis_errors: Analysis failures.
 * @queue: Event queue consumed by the detector.
 */
struct phantom_detector {
	u8 state;
	bool enabled;

	unsigned long sources;

	u64 events_received;
	u64 events_analyzed;
	u64 events_detected;
	u64 events_rejected;
	u64 analysis_errors;

	struct phantom_event_queue *queue;
};

/**
 * phantom_detector_init() - initialize detector.
 * @detector: Detector instance.
 * @queue: Event queue used as detector input.
 *
 * Return: 0 on success, negative errno on failure.
 */
int phantom_detector_init(struct phantom_detector *detector,
			  struct phantom_event_queue *queue);

/**
 * phantom_detector_start() - start detector.
 * @detector: Detector instance.
 *
 * Return: 0 on success, negative errno on failure.
 */
int phantom_detector_start(struct phantom_detector *detector);

/**
 * phantom_detector_stop() - stop detector.
 * @detector: Detector instance.
 *
 * Return: 0 on success, negative errno on failure.
 */
int phantom_detector_stop(struct phantom_detector *detector);

/**
 * phantom_detector_enable_source() - enable a source.
 * @detector: Detector instance.
 * @source: Detector source.
 *
 * Return: 0 on success, negative errno on failure.
 */
int phantom_detector_enable_source(struct phantom_detector *detector,
				   enum phantom_detector_source source);

/**
 * phantom_detector_disable_source() - disable a source.
 * @detector: Detector instance.
 * @source: Detector source.
 *
 * Return: 0 on success, negative errno on failure.
 */
int phantom_detector_disable_source(struct phantom_detector *detector,
				    enum phantom_detector_source source);

/**
 * phantom_detector_source_enabled() - test source state.
 * @detector: Detector instance.
 * @source: Detector source.
 *
 * Return: %true if enabled, otherwise %false.
 */
bool phantom_detector_source_enabled(
	const struct phantom_detector *detector,
	enum phantom_detector_source source);

/**
 * phantom_detector_analyze() - analyze one event.
 * @detector: Detector instance.
 * @event: Event to analyze.
 *
 * Classifies an already collected event. It does not perform
 * neutralization.
 *
 * Return: Detector result.
 */
enum phantom_detector_result
phantom_detector_analyze(struct phantom_detector *detector,
			 struct phantom_threat_event *event);

/**
 * phantom_detector_process_next() - process one queued event.
 * @detector: Detector instance.
 *
 * Removes one event from the detector queue, analyzes it and releases
 * the queue-owned reference.
 *
 * Return: 0 when an event was processed, -ENOENT when the queue is empty,
 *         or another negative errno on failure.
 */
int phantom_detector_process_next(struct phantom_detector *detector);

/**
 * phantom_detector_reset_stats() - reset detector counters.
 * @detector: Detector instance.
 */
void phantom_detector_reset_stats(struct phantom_detector *detector);

#endif /* PHANTOM_DETECTOR_H */
