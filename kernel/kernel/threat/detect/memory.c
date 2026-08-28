/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Memory threat detector implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "memory.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>

/*
 * Validate memory detector state.
 */
static bool phantom_memory_detector_state_valid(u8 state)
{
	return state <= PHANTOM_MEMORY_DETECTOR_ERROR;
}

/*
 * Validate a memory observation.
 */
int phantom_memory_detector_inspect(
	struct phantom_memory_detector *detector,
	const struct phantom_memory_observation *observation)
{
	if (!detector || !observation)
		return -EINVAL;

	if (!detector->enabled ||
	    detector->state != PHANTOM_MEMORY_DETECTOR_RUNNING)
		return -EAGAIN;

	if (!observation->size) {
		detector->errors++;
		return -EINVAL;
	}

	if (!observation->address) {
		detector->errors++;
		return -EINVAL;
	}

	if (observation->pid <= 0 || observation->tgid <= 0) {
		detector->errors++;
		return -EINVAL;
	}

	detector->observations++;

	return 0;
}

/*
 * Initialize memory detector.
 */
int phantom_memory_detector_init(
	struct phantom_memory_detector *detector)
{
	if (!detector)
		return -EINVAL;

	detector->state = PHANTOM_MEMORY_DETECTOR_INITIALIZING;
	detector->enabled = false;

	detector->observations = 0;
	detector->suspicious = 0;
	detector->threats = 0;
	detector->errors = 0;

	detector->state = PHANTOM_MEMORY_DETECTOR_DISABLED;

	return 0;
}

/*
 * Start memory detector.
 */
int phantom_memory_detector_start(
	struct phantom_memory_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (!phantom_memory_detector_state_valid(detector->state))
		return -EINVAL;

	if (detector->state != PHANTOM_MEMORY_DETECTOR_DISABLED)
		return -EBUSY;

	detector->enabled = true;
	detector->state = PHANTOM_MEMORY_DETECTOR_RUNNING;

	return 0;
}

/*
 * Stop memory detector.
 */
int phantom_memory_detector_stop(
	struct phantom_memory_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (detector->state != PHANTOM_MEMORY_DETECTOR_RUNNING)
		return -EINVAL;

	detector->enabled = false;
	detector->state = PHANTOM_MEMORY_DETECTOR_STOPPING;

	detector->state = PHANTOM_MEMORY_DETECTOR_DISABLED;

	return 0;
}

/*
 * Classify one memory observation.
 *
 * W+X memory and permission changes are security-relevant evidence,
 * but one observation alone is not automatically treated as malware.
 */
enum phantom_detector_result
phantom_memory_detector_classify(
	struct phantom_memory_detector *detector,
	const struct phantom_memory_observation *observation)
{
	if (!detector || !observation)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	if (!detector->enabled ||
	    detector->state != PHANTOM_MEMORY_DETECTOR_RUNNING)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	if (observation->flags & PHANTOM_MEMORY_OBS_CRITICAL)
		return PHANTOM_DETECTOR_RESULT_CRITICAL;

	if (observation->flags & PHANTOM_MEMORY_OBS_CORRUPTION)
		return PHANTOM_DETECTOR_RESULT_THREAT;

	if (observation->flags & PHANTOM_MEMORY_OBS_SUSPICIOUS)
		return PHANTOM_DETECTOR_RESULT_SUSPICIOUS;

	if (observation->flags & PHANTOM_MEMORY_OBS_SUSPICIOUS_REGION)
		return PHANTOM_DETECTOR_RESULT_SUSPICIOUS;

	if (observation->flags & PHANTOM_MEMORY_OBS_WRITABLE_EXEC ||
	    observation->flags & PHANTOM_MEMORY_OBS_EXECUTABLE_WRITABLE)
		return PHANTOM_DETECTOR_RESULT_SUSPICIOUS;

	return PHANTOM_DETECTOR_RESULT_CLEAN;
}

/*
 * Create a threat event from a memory observation.
 */
struct phantom_threat_event *
phantom_memory_detector_create_event(
	struct phantom_memory_detector *detector,
	const struct phantom_memory_observation *observation)
{
	struct phantom_threat_event *event;
	enum phantom_detector_result result;
	u8 severity;
	int ret;

	if (!detector || !observation)
		return NULL;

	ret = phantom_memory_detector_inspect(detector, observation);
	if (ret)
		return NULL;

	result = phantom_memory_detector_classify(detector, observation);

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
		PHANTOM_SOURCE_MEMORY);
	if (ret) {
		phantom_threat_event_put(event);
		detector->errors++;
		return NULL;
	}

	event->pid = observation->pid;
	event->tgid = observation->tgid;
	event->flags |= PHANTOM_EVENT_FLAG_KERNEL;

	if (result == PHANTOM_DETECTOR_RESULT_CRITICAL)
		event->flags |= PHANTOM_EVENT_FLAG_CRITICAL;

	strscpy(event->name,
		"memory-observation",
		sizeof(event->name));

	snprintf(event->description,
		 sizeof(event->description),
		 "address=%px size=%llu prot=0x%x flags=0x%x",
		 (void *)observation->address,
		 (unsigned long long)observation->size,
		 observation->prot,
		 observation->flags);

	if (result == PHANTOM_DETECTOR_RESULT_SUSPICIOUS)
		detector->suspicious++;

	if (result == PHANTOM_DETECTOR_RESULT_THREAT ||
	    result == PHANTOM_DETECTOR_RESULT_CRITICAL)
		detector->threats++;

	return event;
}

/*
 * Reset memory detector statistics.
 */
void phantom_memory_detector_reset_stats(
	struct phantom_memory_detector *detector)
{
	if (!detector)
		return;

	detector->observations = 0;
	detector->suspicious = 0;
	detector->threats = 0;
	detector->errors = 0;
}
