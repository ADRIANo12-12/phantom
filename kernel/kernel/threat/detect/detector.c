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
 * Validate detector source.
 */
static bool phantom_detector_source_valid(
	enum phantom_detector_source source)
{
	return source > PHANTOM_DETECTOR_SOURCE_UNKNOWN &&
	       source <= PHANTOM_DETECTOR_SOURCE_INTEGRITY;
}

/*
 * Validate detector state.
 */
static bool phantom_detector_state_valid(u8 state)
{
	return state <= PHANTOM_DETECTOR_ERROR;
}

/*
 * Convert event source into detector source.
 */
static enum phantom_detector_source
phantom_detector_source_from_event(
	const struct phantom_threat_event *event)
{
	switch (event->source) {
	case PHANTOM_SOURCE_SYSCALL:
		return PHANTOM_DETECTOR_SOURCE_SYSCALL;

	case PHANTOM_SOURCE_PROCESS:
		return PHANTOM_DETECTOR_SOURCE_PROCESS;

	case PHANTOM_SOURCE_MEMORY:
		return PHANTOM_DETECTOR_SOURCE_MEMORY;

	case PHANTOM_SOURCE_FILE:
		return PHANTOM_DETECTOR_SOURCE_FILE;

	case PHANTOM_SOURCE_NETWORK:
		return PHANTOM_DETECTOR_SOURCE_NETWORK;

	case PHANTOM_SOURCE_MODULE:
		return PHANTOM_DETECTOR_SOURCE_MODULE;

	case PHANTOM_SOURCE_INTEGRITY:
		return PHANTOM_DETECTOR_SOURCE_INTEGRITY;

	default:
		return PHANTOM_DETECTOR_SOURCE_UNKNOWN;
	}
}

/*
 * Analyze one event.
 *
 * This is the central classification point. Concrete detection
 * backends will populate event metadata before it reaches here.
 */
enum phantom_detector_result
phantom_detector_analyze(struct phantom_detector *detector,
			 struct phantom_threat_event *event)
{
	if (!detector || !event)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	if (!detector->enabled ||
	    detector->state != PHANTOM_DETECTOR_RUNNING)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	switch (event->severity) {
	case PHANTOM_SEVERITY_CRITICAL:
		return PHANTOM_DETECTOR_RESULT_CRITICAL;

	case PHANTOM_SEVERITY_HIGH:
		return PHANTOM_DETECTOR_RESULT_THREAT;

	case PHANTOM_SEVERITY_MEDIUM:
		return PHANTOM_DETECTOR_RESULT_SUSPICIOUS;

	case PHANTOM_SEVERITY_LOW:
	case PHANTOM_SEVERITY_INFO:
	case PHANTOM_SEVERITY_UNKNOWN:
	default:
		return PHANTOM_DETECTOR_RESULT_CLEAN;
	}
}

/*
 * Initialize detector.
 */
int phantom_detector_init(struct phantom_detector *detector,
			  struct phantom_event_queue *queue)
{
	if (!detector || !queue)
		return -EINVAL;

	detector->state = PHANTOM_DETECTOR_INITIALIZING;
	detector->enabled = false;
	detector->sources = 0;

	detector->events_received = 0;
	detector->events_analyzed = 0;
	detector->events_detected = 0;
	detector->events_rejected = 0;
	detector->analysis_errors = 0;

	detector->queue = queue;

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

	if (!phantom_detector_state_valid(detector->state))
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

	detector->enabled = false;
	detector->state = PHANTOM_DETECTOR_STOPPING;

	detector->state = PHANTOM_DETECTOR_DISABLED;

	return 0;
}

/*
 * Enable detector source.
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
 * Disable detector source.
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
 * Check whether detector source is enabled.
 */
bool phantom_detector_source_enabled(
	const struct phantom_detector *detector,
	enum phantom_detector_source source)
{
	if (!detector)
		return false;

	if (!phantom_detector_source_valid(source))
		return false;

	return test_bit(source, &detector->sources);
}

/*
 * Process one event from the detector queue.
 */
int phantom_detector_process_next(struct phantom_detector *detector)
{
	struct phantom_threat_event *event;
	enum phantom_detector_source source;
	enum phantom_detector_result result;

	if (!detector)
		return -EINVAL;

	if (!detector->enabled ||
	    detector->state != PHANTOM_DETECTOR_RUNNING)
		return -EAGAIN;

	if (!detector->queue)
		return -ENODEV;

	event = phantom_event_queue_pop(detector->queue);
	if (!event)
		return -ENOENT;

	detector->events_received++;

	source = phantom_detector_source_from_event(event);

	if (source == PHANTOM_DETECTOR_SOURCE_UNKNOWN) {
		detector->events_rejected++;
		phantom_threat_event_put(event);
		return -EINVAL;
	}

	if (!phantom_detector_source_enabled(detector, source)) {
		detector->events_rejected++;
		phantom_threat_event_put(event);
		return -EACCES;
	}

	result = phantom_detector_analyze(detector, event);

	switch (result) {
	case PHANTOM_DETECTOR_RESULT_CLEAN:
		detector->events_analyzed++;
		break;

	case PHANTOM_DETECTOR_RESULT_SUSPICIOUS:
		detector->events_analyzed++;
		detector->events_detected++;

		event->severity = max_t(u8,
					event->severity,
					PHANTOM_SEVERITY_MEDIUM);
		event->result = PHANTOM_RESULT_DETECTED;
		break;

	case PHANTOM_DETECTOR_RESULT_THREAT:
		detector->events_analyzed++;
		detector->events_detected++;

		event->severity = max_t(u8,
					event->severity,
					PHANTOM_SEVERITY_HIGH);
		event->result = PHANTOM_RESULT_DETECTED;
		break;

	case PHANTOM_DETECTOR_RESULT_CRITICAL:
		detector->events_analyzed++;
		detector->events_detected++;

		event->severity = PHANTOM_SEVERITY_CRITICAL;
		event->flags |= PHANTOM_EVENT_FLAG_CRITICAL;
		event->result = PHANTOM_RESULT_DETECTED;
		break;

	case PHANTOM_DETECTOR_RESULT_NONE:
		detector->events_analyzed++;
		break;

	case PHANTOM_DETECTOR_RESULT_ERROR:
	default:
		detector->analysis_errors++;
		phantom_threat_event_put(event);
		return -EIO;
	}

	/*
	 * The detector no longer owns the queue reference.
	 */
	phantom_threat_event_put(event);

	return 0;
}

/*
 * Reset detector statistics.
 */
void phantom_detector_reset_stats(struct phantom_detector *detector)
{
	if (!detector)
		return;

	detector->events_received = 0;
	detector->events_analyzed = 0;
	detector->events_detected = 0;
	detector->events_rejected = 0;
	detector->analysis_errors = 0;
}
