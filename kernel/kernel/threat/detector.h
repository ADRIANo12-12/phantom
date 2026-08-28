/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat detection interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_THREAT_DETECTOR_H
#define PHANTOM_THREAT_DETECTOR_H

#include <linux/types.h>

#include "event.h"

/*
 * Maximum number of detection sources supported by the detector
 * subsystem.
 */
#define PHANTOM_DETECTOR_MAX_SOURCES	16

/*
 * Detector state.
 */
enum phantom_detector_state {
	PHANTOM_DETECTOR_DISABLED = 0,
	PHANTOM_DETECTOR_INITIALIZING,
	PHANTOM_DETECTOR_RUNNING,
	PHANTOM_DETECTOR_STOPPING,
};

/*
 * Detection source.
 *
 * A detector source identifies where suspicious behaviour was observed.
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
 * Detection result.
 */
enum phantom_detection_result {
	PHANTOM_DETECTION_NONE = 0,
	PHANTOM_DETECTION_SUSPICIOUS,
	PHANTOM_DETECTION_THREAT,
	PHANTOM_DETECTION_CRITICAL,
};

/**
 * struct phantom_detector - detector subsystem state.
 * @state: Current detector state.
 * @enabled: Whether detection is currently enabled.
 * @sources: Bitmask of enabled detection sources.
 * @events_detected: Number of events detected.
 * @events_reported: Number of events reported to the informator.
 * @events_blocked: Number of events that resulted in blocking.
 */
struct phantom_detector {
	u32 state;
	bool enabled;
	unsigned long sources;

	u64 events_detected;
	u64 events_reported;
	u64 events_blocked;
};

/**
 * phantom_detector_init() - initialize the detector subsystem.
 * @detector: Detector instance to initialize.
 *
 * Initializes detector state and enables no detection sources by default.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_detector_init(struct phantom_detector *detector);

/**
 * phantom_detector_start() - start threat detection.
 * @detector: Detector instance.
 *
 * Enables the detector after successful initialization.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_detector_start(struct phantom_detector *detector);

/**
 * phantom_detector_stop() - stop threat detection.
 * @detector: Detector instance.
 *
 * Stops active detection and prevents new events from being generated.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_detector_stop(struct phantom_detector *detector);

/**
 * phantom_detector_enable_source() - enable a detection source.
 * @detector: Detector instance.
 * @source: Detection source to enable.
 *
 * Enables one detector source.
 *
 * Return: 0 on success, or a negative errno value on failure.
 */
int phantom_detector_enable_source(struct phantom_detector *detector,
				   enum phantom_detector_source source);

/**
 * phantom_detector_disable_source() - disable a detection source.
 * @detector: Detector instance.
 * @source: Detection source to disable.
 *
 * Disables one detector source.
 *
 * Return: 0 on success, or a negative errno value on failure.
 */
int phantom_detector_disable_source(struct phantom_detector *detector,
				    enum phantom_detector_source source);

/**
 * phantom_detector_is_source_enabled() - test a detection source.
 * @detector: Detector instance.
 * @source: Detection source to test.
 *
 * Return: %true if the source is enabled, otherwise %false.
 */
bool phantom_detector_is_source_enabled(
	struct phantom_detector *detector,
	enum phantom_detector_source source);

/**
 * phantom_detector_report() - report a detected threat event.
 * @detector: Detector instance.
 * @event: Threat event to report.
 *
 * Passes a fully initialized threat event to the next stage of the
 * security pipeline.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_detector_report(struct phantom_detector *detector,
			    struct phantom_threat_event *event);

/**
 * phantom_detector_scan_event() - inspect an event for suspicious behaviour.
 * @detector: Detector instance.
 * @event: Event to inspect.
 *
 * Performs detector-side inspection and classifies the event.
 *
 * Return: Detection result.
 */
enum phantom_detection_result
phantom_detector_scan_event(struct phantom_detector *detector,
			    struct phantom_threat_event *event);

/**
 * phantom_detector_reset_stats() - reset detector statistics.
 * @detector: Detector instance.
 *
 * Resets detection counters to zero.
 */
void phantom_detector_reset_stats(struct phantom_detector *detector);

#endif /* PHANTOM_THREAT_DETECTOR_H */
