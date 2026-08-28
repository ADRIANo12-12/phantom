/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Syscall threat detector implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "syscall.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>

/*
 * Validate syscall detector state.
 */
static bool phantom_syscall_detector_state_valid(u8 state)
{
	return state <= PHANTOM_SYSCALL_DETECTOR_ERROR;
}

/*
 * Validate a syscall number.
 *
 * We intentionally do not impose an architecture-specific maximum here.
 * The architecture-specific syscall hook is responsible for supplying
 * a valid syscall number for the running architecture.
 */
static bool phantom_syscall_number_valid(long nr)
{
	return nr >= 0;
}

/*
 * Initialize syscall detector.
 */
int phantom_syscall_detector_init(
	struct phantom_syscall_detector *detector)
{
	if (!detector)
		return -EINVAL;

	detector->state = PHANTOM_SYSCALL_DETECTOR_INITIALIZING;
	detector->enabled = false;

	detector->observations = 0;
	detector->suspicious = 0;
	detector->threats = 0;
	detector->errors = 0;

	detector->state = PHANTOM_SYSCALL_DETECTOR_DISABLED;

	return 0;
}

/*
 * Start syscall detector.
 */
int phantom_syscall_detector_start(
	struct phantom_syscall_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (!phantom_syscall_detector_state_valid(detector->state))
		return -EINVAL;

	if (detector->state != PHANTOM_SYSCALL_DETECTOR_DISABLED)
		return -EBUSY;

	detector->enabled = true;
	detector->state = PHANTOM_SYSCALL_DETECTOR_RUNNING;

	return 0;
}

/*
 * Stop syscall detector.
 */
int phantom_syscall_detector_stop(
	struct phantom_syscall_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (detector->state != PHANTOM_SYSCALL_DETECTOR_RUNNING)
		return -EINVAL;

	detector->enabled = false;
	detector->state = PHANTOM_SYSCALL_DETECTOR_STOPPING;

	detector->state = PHANTOM_SYSCALL_DETECTOR_DISABLED;

	return 0;
}

/*
 * Inspect one syscall observation.
 */
int phantom_syscall_detector_inspect(
	struct phantom_syscall_detector *detector,
	const struct phantom_syscall_observation *observation)
{
	if (!detector || !observation)
		return -EINVAL;

	if (!detector->enabled ||
	    detector->state != PHANTOM_SYSCALL_DETECTOR_RUNNING)
		return -EAGAIN;

	if (!phantom_syscall_number_valid(observation->nr)) {
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
 * Classify one syscall observation.
 *
 * This function intentionally treats flags as evidence rather than
 * automatically declaring every privileged or filesystem syscall
 * malicious. Concrete syscall rules will later populate the
 * SUSPICIOUS/CRITICAL flags based on actual behaviour.
 */
enum phantom_detector_result
phantom_syscall_detector_classify(
	struct phantom_syscall_detector *detector,
	const struct phantom_syscall_observation *observation)
{
	if (!detector || !observation)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	if (!detector->enabled ||
	    detector->state != PHANTOM_SYSCALL_DETECTOR_RUNNING)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	if (observation->flags & PHANTOM_SYSCALL_OBS_CRITICAL)
		return PHANTOM_DETECTOR_RESULT_CRITICAL;

	if (observation->flags & PHANTOM_SYSCALL_OBS_SUSPICIOUS)
		return PHANTOM_DETECTOR_RESULT_SUSPICIOUS;

	/*
	 * A syscall can have security-sensitive properties without being
	 * malicious. Do not turn these flags into false positives.
	 */
	return PHANTOM_DETECTOR_RESULT_CLEAN;
}

/*
 * Create a threat event from a syscall observation.
 */
struct phantom_threat_event *
phantom_syscall_detector_create_event(
	struct phantom_syscall_detector *detector,
	const struct phantom_syscall_observation *observation)
{
	struct phantom_threat_event *event;
	enum phantom_detector_result result;
	u8 severity;
	int ret;

	if (!detector || !observation)
		return NULL;

	ret = phantom_syscall_detector_inspect(detector, observation);
	if (ret)
		return NULL;

	result = phantom_syscall_detector_classify(detector, observation);

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

	ret = phantom_threat_event_init(event,
					0,
					severity,
					PHANTOM_SOURCE_SYSCALL);
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

	/*
	 * Use a stable generic name for now. A future rule engine should
	 * provide a more specific name once it has concrete evidence.
	 */
	strscpy(event->name,
		"syscall-observation",
		sizeof(event->name));

	snprintf(event->description,
		 sizeof(event->description),
		 "syscall=%ld return=%ld flags=0x%x",
		 observation->nr,
		 observation->return_value,
		 observation->flags);

	strscpy(event->comm,
		observation->comm,
		sizeof(event->comm));

	if (result == PHANTOM_DETECTOR_RESULT_SUSPICIOUS)
		detector->suspicious++;

	if (result == PHANTOM_DETECTOR_RESULT_THREAT ||
	    result == PHANTOM_DETECTOR_RESULT_CRITICAL)
		detector->threats++;

	return event;
}

/*
 * Reset syscall detector statistics.
 */
void phantom_syscall_detector_reset_stats(
	struct phantom_syscall_detector *detector)
{
	if (!detector)
		return;

	detector->observations = 0;
	detector->suspicious = 0;
	detector->threats = 0;
	detector->errors = 0;
}
