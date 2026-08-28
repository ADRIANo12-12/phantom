/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat neutralizer implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "neutralizer.h"

#include <linux/errno.h>
#include <linux/kernel.h>

/*
 * Validate a neutralization action.
 */
static bool phantom_neutralizer_action_valid(
	enum phantom_neutralizer_action action)
{
	return action <= PHANTOM_NEUTRALIZE_BLOCK;
}

/*
 * Initialize neutralizer state.
 */
int phantom_neutralizer_init(struct phantom_neutralizer *neutralizer)
{
	if (!neutralizer)
		return -EINVAL;

	neutralizer->state = PHANTOM_NEUTRALIZER_DISABLED;
	neutralizer->enabled = false;

	neutralizer->neutralizations = 0;
	neutralizer->blocked = 0;

	return 0;
}

/*
 * Enable active neutralization.
 */
int phantom_neutralizer_enable(struct phantom_neutralizer *neutralizer)
{
	if (!neutralizer)
		return -EINVAL;

	if (neutralizer->state == PHANTOM_NEUTRALIZER_DISABLED) {
		neutralizer->state = PHANTOM_NEUTRALIZER_ENABLED;
		neutralizer->enabled = true;
	}

	return 0;
}

/*
 * Disable active neutralization.
 */
int phantom_neutralizer_disable(struct phantom_neutralizer *neutralizer)
{
	if (!neutralizer)
		return -EINVAL;

	neutralizer->state = PHANTOM_NEUTRALIZER_DISABLED;
	neutralizer->enabled = false;

	return 0;
}

/*
 * Execute the selected action for an event.
 *
 * Deliberately conservative: logging and alerting are always safe,
 * while blocking is only performed when the neutralizer is enabled.
 */
int phantom_neutralizer_execute(struct phantom_neutralizer *neutralizer,
				struct phantom_threat_event *event)
{
	if (!neutralizer || !event)
		return -EINVAL;

	if (!phantom_neutralizer_action_valid(event->action))
		return -EINVAL;

	switch (event->action) {
	case PHANTOM_ACTION_NONE:
	case PHANTOM_ACTION_LOG:
	case PHANTOM_ACTION_ALERT:
		/*
		 * Informational actions never require the neutralizer.
		 */
		return 0;

	case PHANTOM_ACTION_BLOCK:
		/*
		 * Blocking requires active neutralization.
		 */
		if (!neutralizer->enabled)
			return -EACCES;

		event->result = PHANTOM_RESULT_BLOCKED;
		neutralizer->blocked++;
		neutralizer->neutralizations++;
		return 0;

	case PHANTOM_ACTION_QUARANTINE:
	case PHANTOM_ACTION_ISOLATE:
	case PHANTOM_ACTION_TERMINATE:
		/*
		 * Not implemented in the currently safe pipeline.
		 */
		return -EOPNOTSUPP;

	default:
		return -EINVAL;
	}
}

/*
 * Reset neutralizer statistics.
 */
void phantom_neutralizer_reset_stats(struct phantom_neutralizer *neutralizer)
{
	if (!neutralizer)
		return;

	neutralizer->neutralizations = 0;
	neutralizer->blocked = 0;
}