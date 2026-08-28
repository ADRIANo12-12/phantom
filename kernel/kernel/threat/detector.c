/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat detector implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "detector.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/printk.h>

/*
 * Validate a detector source.
 */
static bool phantom_detector_source_valid(
	enum phantom_detector_source source)
{
	return source > PHANTOM_DETECTOR_SOURCE_UNKNOWN &&
	       source <= PHANTOM_DETECTOR_SOURCE_INTEGRITY;
}

/*
 * Convert a detector source into an event source.
 *
 * The detector and event layers intentionally have separate enums:
 * detector source describes the detector subsystem, while event source
 * describes the origin recorded in a threat event.
 */
static u8 phantom_detector_event_source(
	enum phantom_detector_source source)
{
	switch (source) {
	case PHANTOM_DETECTOR_SOURCE_SYSCALL:
		return PHANTOM_SOURCE_SYSCALL;

	case PHANTOM_DETECTOR_SOURCE_PROCESS:
		return PHANTOM_SOURCE_PROCESS;

	case PHANTOM_DETECTOR_SOURCE_MEMORY:
		return PHANTOM_SOURCE_MEMORY;

	case PHANTOM_DETECTOR_SOURCE_FILE:
		return PHANTOM_SOURCE_FILE;

	case PHANTOM_DETECTOR_SOURCE_NETWORK:
		return PHANTOM_SOURCE_NETWORK;

	case PHANTOM_DETECTOR_SOURCE_MODULE:
		return PHANTOM_SOURCE_MODULE;

	case PHANTOM_DETECTOR_SOURCE_INTEGRITY:
		return PHANTOM_SOURCE_INTEGRITY;

	case PHANTOM_DETECTOR_SOURCE_UNKNOWN:
	default:
		return PHANTOM_SOURCE_UNKNOWN;
	}
}

/*
 * Return the event severity associated with a detection result.
 */
static u8 phantom_detector_result_severity(
	enum phantom_detection_result result)
{
	switch (result) {
	case PHANTOM_DETECTION_CRITICAL:
		return PHANTOM_SEVERITY_CRITICAL;

	case PHANTOM_DETECTION_THREAT:
		return PHANTOM_SEVERITY_HIGH;

	case PHANTOM_DETECTION_SUSPICIOUS:
		return PHANTOM_SEVERITY_MEDIUM;

	case PHANTOM_DETECTION_NONE:
	default:
		return PHANTOM_SEVERITY_INFO;
	}
}

/*
 * Initialize detector state.
 */
int phantom_detector_init(struct phantom_detector *detector)
{
	if (!detector)
		return -EINVAL;

	detector->state = PHANTOM_DETECTOR_INITIALIZING;
	detector->enabled = false;
	detector->sources = 0;

	detector->events_detected = 0;
	detector->events_reported = 0;
	detector->events_blocked = 0;

	detector->state = PHANTOM_DETECTOR_DISABLED;

	return 0;
}

/*
 * Start detector.
 */
int phantom_detector_start(struct phantom_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (detector->state != PHANTOM_DETECTOR_DISABLED)
		return -EBUSY;

	detector->enabled = true;
	detector->state = PHANTOM_DETECTOR_RUNNING;

	return 0;
}

/*
 * Stop detector.
 */
int phantom_detector_stop(struct phantom_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (detector->state != PHANTOM_DETECTOR_RUNNING)
		return -EINVAL;

	detector->state = PHANTOM_DETECTOR_STOPPING;
	detector->enabled = false;

	detector->state = PHANTOM_DETECTOR_DISABLED;

	return 0;
}

/*
 * Enable one detector source.
 */
int phantom_detector_enable_source(struct phantom_detector *detector,
				   enum phantom_detector_source source)
{
	if (!detector)
		return -EINVAL;

	if (!phantom_detector_source_valid(source))
		return -EINVAL;

	detector->sources |= BIT(source);

	return 0;
}

/*
 * Disable one detector source.
 */
int phantom_detector_disable_source(struct phantom_detector *detector,
				    enum phantom_detector_source source)
{
	if (!detector)
		return -EINVAL;

	if (!phantom_detector_source_valid(source))
		return -EINVAL;

	detector->sources &= ~BIT(source);

	return 0;
}

/*
 * Check whether a source is enabled.
 */
bool phantom_detector_is_source_enabled(
	struct phantom_detector *detector,
	enum phantom_detector_source source)
{
	if (!detector)
		return false;

	if (!phantom_detector_source_valid(source))
		return false;

	return test_bit(source, &detector->sources);
}

/*
 * Analyze a prepared threat event.
 *
 * This function is deliberately conservative. The detector currently
 * classifies information already present in the event; actual detection
 * engines will be added later and will populate event metadata based
 * on concrete kernel observations.
 */
enum phantom_detection_result
phantom_detector_scan_event(struct phantom_detector *detector,
			    struct phantom_threat_event *event)
{
	if (!detector || !event)
		return PHANTOM_DETECTION_NONE;

	if (!detector->enabled)
		return PHANTOM_DETECTION_NONE;

	/*
	 * An already critical event must remain critical.
	 */
	if (event->severity >= PHANTOM_SEVERITY_CRITICAL)
		return PHANTOM_DETECTION_CRITICAL;

	/*
	 * An event which has already reached HIGH severity is considered
	 * a confirmed threat by the current detector layer.
	 */
	if (event->severity >= PHANTOM_SEVERITY_HIGH)
		return PHANTOM_DETECTION_THREAT;

	/*
	 * Medium severity represents suspicious activity.
	 */
	if (event->severity >= PHANTOM_SEVERITY_MEDIUM)
		return PHANTOM_DETECTION_SUSPICIOUS;

	return PHANTOM_DETECTION_NONE;
}

/*
 * Report a detected event to the detector subsystem.
 */
int phantom_detector_report(struct phantom_detector *detector,
			    struct phantom_threat_event *event)
{
	enum phantom_detection_result result;
	u8 severity;
	u8 source;

	if (!detector || !event)
		return -EINVAL;

	if (!detector->enabled ||
	    detector->state != PHANTOM_DETECTOR_RUNNING)
		return -EAGAIN;

	/*
	 * Do not accept events whose source is unknown.
	 */
	if (event->source == PHANTOM_SOURCE_UNKNOWN)
		return -EINVAL;

	/*
	 * Convert event source into the detector source namespace.
	 */
	switch (event->source) {
	case PHANTOM_SOURCE_SYSCALL:
		source = PHANTOM_DETECTOR_SOURCE_SYSCALL;
		break;

	case PHANTOM_SOURCE_PROCESS:
		source = PHANTOM_DETECTOR_SOURCE_PROCESS;
		break;

	case PHANTOM_SOURCE_MEMORY:
		source = PHANTOM_DETECTOR_SOURCE_MEMORY;
		break;

	case PHANTOM_SOURCE_FILE:
		source = PHANTOM_DETECTOR_SOURCE_FILE;
		break;

	case PHANTOM_SOURCE_NETWORK:
		source = PHANTOM_DETECTOR_SOURCE_NETWORK;
		break;

	case PHANTOM_SOURCE_MODULE:
		source = PHANTOM_DETECTOR_SOURCE_MODULE;
		break;

	case PHANTOM_SOURCE_INTEGRITY:
		source = PHANTOM_DETECTOR_SOURCE_INTEGRITY;
		break;

	default:
		return -EINVAL;
	}

	/*
	 * Ignore events generated by disabled detector sources.
	 */
	if (!phantom_detector_is_source_enabled(detector, source))
		return -EACCES;

	result = phantom_detector_scan_event(detector, event);

	detector->events_reported++;

	if (result == PHANTOM_DETECTION_NONE)
		return 0;

	/*
	 * Update the event's severity according to the detector result.
	 */
	severity = phantom_detector_result_severity(result);

	if (severity > event->severity)
		event->severity = severity;

	/*
	 * Mark the event as originating from the detector.
	 */
	event->flags |= PHANTOM_EVENT_FLAG_KERNEL;

	/*
	 * Detection has occurred.
	 */
	detector->events_detected++;
	event->result = PHANTOM_RESULT_DETECTED;

	if (result == PHANTOM_DETECTION_CRITICAL)
		event->flags |= PHANTOM_EVENT_FLAG_CRITICAL;

	return 0;
}

/*
 * Reset detector statistics.
 */
void phantom_detector_reset_stats(struct phantom_detector *detector)
{
	if (!detector)
		return;

	detector->events_detected = 0;
	detector->events_reported = 0;
	detector->events_blocked = 0;
}
