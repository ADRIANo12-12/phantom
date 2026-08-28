/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Kernel module threat detector implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "module.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/string.h>

/*
 * Validate module detector state.
 */
static bool phantom_module_detector_state_valid(u8 state)
{
	return state <= PHANTOM_MODULE_DETECTOR_ERROR;
}

/*
 * Initialize module detector.
 */
int phantom_module_detector_init(
	struct phantom_module_detector *detector)
{
	if (!detector)
		return -EINVAL;

	detector->state = PHANTOM_MODULE_DETECTOR_INITIALIZING;
	detector->enabled = false;

	detector->observations = 0;
	detector->suspicious = 0;
	detector->threats = 0;
	detector->errors = 0;

	detector->state = PHANTOM_MODULE_DETECTOR_DISABLED;

	return 0;
}

/*
 * Start module detector.
 */
int phantom_module_detector_start(
	struct phantom_module_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (!phantom_module_detector_state_valid(detector->state))
		return -EINVAL;

	if (detector->state != PHANTOM_MODULE_DETECTOR_DISABLED)
		return -EBUSY;

	detector->enabled = true;
	detector->state = PHANTOM_MODULE_DETECTOR_RUNNING;

	return 0;
}

/*
 * Stop module detector.
 */
int phantom_module_detector_stop(
	struct phantom_module_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (detector->state != PHANTOM_MODULE_DETECTOR_RUNNING)
		return -EINVAL;

	detector->enabled = false;
	detector->state = PHANTOM_MODULE_DETECTOR_STOPPING;

	detector->state = PHANTOM_MODULE_DETECTOR_DISABLED;

	return 0;
}

/*
 * Classify one module observation.
 *
 * An unsigned or out-of-tree module is not automatically malicious.
 * The detector requires explicit suspicious/critical evidence before
 * escalating the event.
 */
enum phantom_detector_result
phantom_module_detector_classify(
	struct phantom_module_detector *detector,
	const struct phantom_module_observation *observation)
{
	if (!detector || !observation)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	if (!detector->enabled ||
	    detector->state != PHANTOM_MODULE_DETECTOR_RUNNING)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	if (observation->flags & PHANTOM_MODULE_OBS_CRITICAL)
		return PHANTOM_DETECTOR_RESULT_CRITICAL;

	if (observation->flags & PHANTOM_MODULE_OBS_SUSPICIOUS)
		return PHANTOM_DETECTOR_RESULT_SUSPICIOUS;

	return PHANTOM_DETECTOR_RESULT_CLEAN;
}

/*
 * Create a threat event from a module observation.
 */
struct phantom_threat_event *
phantom_module_detector_create_event(
	struct phantom_module_detector *detector,
	const struct phantom_module_observation *observation)
{
	struct phantom_threat_event *event;
	enum phantom_detector_result result;
	u8 severity;
	int ret;

	if (!detector || !observation)
		return NULL;

	if (!detector->enabled ||
	    detector->state != PHANTOM_MODULE_DETECTOR_RUNNING)
		return NULL;

	result = phantom_module_detector_classify(detector, observation);

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

	event = phantom_threat_event_alloc(GFP_KERNEL);
	if (!event) {
		detector->errors++;
		return NULL;
	}

	ret = phantom_threat_event_init(
		event,
		0,
		severity,
		PHANTOM_SOURCE_MODULE);
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

	if (!event->name[0])
		strscpy(event->name,
			"module-observation",
			sizeof(event->name));

	snprintf(event->description,
		 sizeof(event->description),
		 "module=%s state=%u refcnt=%u core_size=%llu init_size=%llu flags=0x%x",
		 observation->name,
		 observation->state,
		 observation->refcnt,
		 (unsigned long long)observation->core_size,
		 (unsigned long long)observation->init_size,
		 observation->flags);

	if (result == PHANTOM_DETECTOR_RESULT_SUSPICIOUS)
		detector->suspicious++;

	if (result == PHANTOM_DETECTOR_RESULT_THREAT ||
	    result == PHANTOM_DETECTOR_RESULT_CRITICAL)
		detector->threats++;

	return event;
}

/*
 * Reset module detector statistics.
 */
void phantom_module_detector_reset_stats(
	struct phantom_module_detector *detector)
{
	if (!detector)
		return;

	detector->observations = 0;
	detector->suspicious = 0;
	detector->threats = 0;
	detector->errors = 0;
}
