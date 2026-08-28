/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Integrity threat detector implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "integrity.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>

/*
 * Validate detector state.
 */
static bool phantom_integrity_detector_state_valid(u8 state)
{
	return state <= PHANTOM_INTEGRITY_DETECTOR_ERROR;
}

/*
 * Validate an integrity observation.
 */
int phantom_integrity_detector_inspect(
	struct phantom_integrity_detector *detector,
	const struct phantom_integrity_observation *observation)
{
	if (!detector || !observation)
		return -EINVAL;

	if (!detector->enabled ||
	    detector->state != PHANTOM_INTEGRITY_DETECTOR_RUNNING)
		return -EAGAIN;

	if (!observation->name[0]) {
		detector->errors++;
		return -EINVAL;
	}

	/*
	 * An integrity mismatch must identify some form of changed or
	 * unverifiable state.
	 */
	if (observation->flags &
	    (PHANTOM_INTEGRITY_OBS_HASH_MISMATCH |
	     PHANTOM_INTEGRITY_OBS_SIGNATURE_FAIL |
	     PHANTOM_INTEGRITY_OBS_MEMORY_CHANGE |
	     PHANTOM_INTEGRITY_OBS_CODE_CHANGE |
	     PHANTOM_INTEGRITY_OBS_MODULE_CHANGE |
	     PHANTOM_INTEGRITY_OBS_TAINT |
	     PHANTOM_INTEGRITY_OBS_SUSPICIOUS |
	     PHANTOM_INTEGRITY_OBS_CRITICAL)) {
		detector->mismatches++;
	}

	detector->observations++;

	return 0;
}

/*
 * Initialize integrity detector.
 */
int phantom_integrity_detector_init(
	struct phantom_integrity_detector *detector)
{
	if (!detector)
		return -EINVAL;

	detector->state = PHANTOM_INTEGRITY_DETECTOR_INITIALIZING;
	detector->enabled = false;

	detector->observations = 0;
	detector->suspicious = 0;
	detector->threats = 0;
	detector->mismatches = 0;
	detector->errors = 0;

	detector->state = PHANTOM_INTEGRITY_DETECTOR_DISABLED;

	return 0;
}

/*
 * Start integrity detector.
 */
int phantom_integrity_detector_start(
	struct phantom_integrity_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (!phantom_integrity_detector_state_valid(detector->state))
		return -EINVAL;

	if (detector->state != PHANTOM_INTEGRITY_DETECTOR_DISABLED)
		return -EBUSY;

	detector->enabled = true;
	detector->state = PHANTOM_INTEGRITY_DETECTOR_RUNNING;

	return 0;
}

/*
 * Stop integrity detector.
 */
int phantom_integrity_detector_stop(
	struct phantom_integrity_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (detector->state != PHANTOM_INTEGRITY_DETECTOR_RUNNING)
		return -EINVAL;

	detector->enabled = false;
	detector->state = PHANTOM_INTEGRITY_DETECTOR_STOPPING;

	detector->state = PHANTOM_INTEGRITY_DETECTOR_DISABLED;

	return 0;
}

/*
 * Classify integrity evidence.
 *
 * A signature failure or executable/code modification is stronger
 * evidence than a generic suspicious flag.
 */
enum phantom_detector_result
phantom_integrity_detector_classify(
	struct phantom_integrity_detector *detector,
	const struct phantom_integrity_observation *observation)
{
	u32 critical_flags;

	if (!detector || !observation)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	if (!detector->enabled ||
	    detector->state != PHANTOM_INTEGRITY_DETECTOR_RUNNING)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	critical_flags =
		PHANTOM_INTEGRITY_OBS_SIGNATURE_FAIL |
		PHANTOM_INTEGRITY_OBS_CODE_CHANGE;

	if (observation->flags & PHANTOM_INTEGRITY_OBS_CRITICAL)
		return PHANTOM_DETECTOR_RESULT_CRITICAL;

	if (observation->flags & critical_flags)
		return PHANTOM_DETECTOR_RESULT_THREAT;

	if (observation->flags &
	    (PHANTOM_INTEGRITY_OBS_HASH_MISMATCH |
	     PHANTOM_INTEGRITY_OBS_MEMORY_CHANGE |
	     PHANTOM_INTEGRITY_OBS_MODULE_CHANGE |
	     PHANTOM_INTEGRITY_OBS_SUSPICIOUS))
		return PHANTOM_DETECTOR_RESULT_SUSPICIOUS;

	/*
	 * A taint flag by itself is not proof of compromise.
	 */
	return PHANTOM_DETECTOR_RESULT_CLEAN;
}

/*
 * Create a threat event from integrity evidence.
 */
struct phantom_threat_event *
phantom_integrity_detector_create_event(
	struct phantom_integrity_detector *detector,
	const struct phantom_integrity_observation *observation)
{
	struct phantom_threat_event *event;
	enum phantom_detector_result result;
	u8 severity;
	int ret;

	if (!detector || !observation)
		return NULL;

	ret = phantom_integrity_detector_inspect(detector, observation);
	if (ret)
		return NULL;

	result = phantom_integrity_detector_classify(detector, observation);

	switch (result) {
	case PHANTOM_DETECTOR_RESULT_CRITICAL:
		severity = PHANTOM_SEVERITY_CRITICAL;
		break;

	case PHANTOM_DETECTOR_RESULT_THREAT:
		severity = PHANTOM_SEVERITY_HIGH;
		break;

	case PHANTOM_DETECTOR_RESULT_SUSPICIOUS:
		severity = PHANTOM_SEVERITY_MEDIUM;
		break;

	case PHANTOM_DETECTOR_RESULT_CLEAN:
	case PHANTOM_DETECTOR_RESULT_NONE:
		severity = PHANTOM_SEVERITY_INFO;
		break;

	case PHANTOM_DETECTOR_RESULT_ERROR:
	default:
		detector->errors++;
		return NULL;
	}

	event = phantom_threat_event_alloc(GFP_ATOMIC);
	if (!event) {
		detector->errors++;
		return NULL;
	}

	ret = phantom_threat_event_init(
		event,
		0,
		severity,
		PHANTOM_SOURCE_INTEGRITY);
	if (ret) {
		phantom_threat_event_put(event);
		detector->errors++;
		return NULL;
	}

	event->flags |= PHANTOM_EVENT_FLAG_KERNEL;

	if (result == PHANTOM_DETECTOR_RESULT_CRITICAL)
		event->flags |= PHANTOM_EVENT_FLAG_CRITICAL;

	strscpy(event->name,
		observation->name,
		sizeof(event->name));

	snprintf(event->description,
		 sizeof(event->description),
		 "object=%llu flags=0x%x",
		 (unsigned long long)observation->object_id,
		 observation->flags);

	if (result == PHANTOM_DETECTOR_RESULT_SUSPICIOUS)
		detector->suspicious++;

	if (result == PHANTOM_DETECTOR_RESULT_THREAT ||
	    result == PHANTOM_DETECTOR_RESULT_CRITICAL)
		detector->threats++;

	return event;
}

/*
 * Reset integrity detector statistics.
 */
void phantom_integrity_detector_reset_stats(
	struct phantom_integrity_detector *detector)
{
	if (!detector)
		return;

	detector->observations = 0;
	detector->suspicious = 0;
	detector->threats = 0;
	detector->mismatches = 0;
	detector->errors = 0;
}
