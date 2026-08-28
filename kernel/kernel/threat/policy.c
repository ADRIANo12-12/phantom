/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat policy implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "policy.h"

#include <linux/errno.h>
#include <linux/kernel.h>

/*
 * Check whether a policy mode is valid.
 */
static bool phantom_policy_mode_valid(u8 mode)
{
	return mode <= PHANTOM_POLICY_LOCKDOWN;
}

/*
 * Check whether an action is valid.
 */
static bool phantom_policy_action_valid(u8 action)
{
	return action <= PHANTOM_ACTION_TERMINATE;
}

/*
 * Check whether a severity is valid.
 */
static bool phantom_policy_severity_valid(u8 severity)
{
	return severity <= PHANTOM_SEVERITY_CRITICAL;
}

/*
 * Return the configured action for a severity.
 */
static u8 phantom_policy_action_for_severity(
	const struct phantom_policy *policy,
	u8 severity)
{
	switch (severity) {
	case PHANTOM_SEVERITY_CRITICAL:
		return policy->critical_action;

	case PHANTOM_SEVERITY_HIGH:
		return policy->high_action;

	case PHANTOM_SEVERITY_MEDIUM:
		return policy->medium_action;

	case PHANTOM_SEVERITY_LOW:
		return policy->low_action;

	case PHANTOM_SEVERITY_INFO:
	case PHANTOM_SEVERITY_UNKNOWN:
	default:
		return policy->info_action;
	}
}

/*
 * Convert an active enforcement action into a safe reporting action
 * when the policy is running in detect-only mode.
 */
static u8 phantom_policy_detect_only_action(u8 action)
{
	switch (action) {
	case PHANTOM_ACTION_BLOCK:
	case PHANTOM_ACTION_QUARANTINE:
	case PHANTOM_ACTION_ISOLATE:
	case PHANTOM_ACTION_TERMINATE:
		return PHANTOM_ACTION_ALERT;

	default:
		return action;
	}
}

/**
 * phantom_policy_init() - initialize a policy.
 * @policy: Policy object.
 *
 * Starts in detect-only mode. No active neutralization is performed.
 *
 * Return: 0 on success, -EINVAL if @policy is NULL.
 */
int phantom_policy_init(struct phantom_policy *policy)
{
	if (!policy)
		return -EINVAL;

	policy->mode = PHANTOM_POLICY_DETECT;

	/*
	 * Conservative defaults.
	 */
	policy->info_action = PHANTOM_ACTION_LOG;
	policy->low_action = PHANTOM_ACTION_LOG;
	policy->medium_action = PHANTOM_ACTION_ALERT;
	policy->high_action = PHANTOM_ACTION_ALERT;
	policy->critical_action = PHANTOM_ACTION_ALERT;

	return 0;
}

/**
 * phantom_policy_set_mode() - set policy mode.
 * @policy: Policy object.
 * @mode: New policy mode.
 *
 * Return: 0 on success, -EINVAL for invalid arguments.
 */
int phantom_policy_set_mode(struct phantom_policy *policy, u8 mode)
{
	if (!policy)
		return -EINVAL;

	if (!phantom_policy_mode_valid(mode))
		return -EINVAL;

	policy->mode = mode;

	return 0;
}

/**
 * phantom_policy_get_mode() - get current policy mode.
 * @policy: Policy object.
 *
 * Return: Current mode or PHANTOM_POLICY_DISABLED if @policy is NULL.
 */
u8 phantom_policy_get_mode(const struct phantom_policy *policy)
{
	if (!policy)
		return PHANTOM_POLICY_DISABLED;

	return policy->mode;
}

/**
 * phantom_policy_set_action() - configure action for a severity.
 * @policy: Policy object.
 * @severity: Severity to configure.
 * @action: Desired action.
 *
 * Return: 0 on success, -EINVAL for invalid arguments.
 */
int phantom_policy_set_action(struct phantom_policy *policy,
			      u8 severity,
			      u8 action)
{
	if (!policy)
		return -EINVAL;

	if (!phantom_policy_severity_valid(severity))
		return -EINVAL;

	if (!phantom_policy_action_valid(action))
		return -EINVAL;

	switch (severity) {
	case PHANTOM_SEVERITY_INFO:
	case PHANTOM_SEVERITY_UNKNOWN:
		policy->info_action = action;
		break;

	case PHANTOM_SEVERITY_LOW:
		policy->low_action = action;
		break;

	case PHANTOM_SEVERITY_MEDIUM:
		policy->medium_action = action;
		break;

	case PHANTOM_SEVERITY_HIGH:
		policy->high_action = action;
		break;

	case PHANTOM_SEVERITY_CRITICAL:
		policy->critical_action = action;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

/**
 * phantom_policy_get_action() - determine event action.
 * @policy: Active policy.
 * @event: Threat event.
 *
 * The function is side-effect free. It only computes the decision.
 *
 * Return: Selected action.
 */
u8 phantom_policy_get_action(const struct phantom_policy *policy,
			     const struct phantom_threat_event *event)
{
	u8 action;

	if (!policy || !event)
		return PHANTOM_ACTION_NONE;

	if (!phantom_policy_mode_valid(policy->mode))
		return PHANTOM_ACTION_NONE;

	if (policy->mode == PHANTOM_POLICY_DISABLED)
		return PHANTOM_ACTION_NONE;

	action = phantom_policy_action_for_severity(policy, event->severity);

	if (!phantom_policy_action_valid(action))
		return PHANTOM_ACTION_NONE;

	/*
	 * Detect-only mode is observation/reporting only.
	 */
	if (policy->mode == PHANTOM_POLICY_DETECT)
		return phantom_policy_detect_only_action(action);

	/*
	 * Lockdown is deliberately stricter.
	 *
	 * Critical events become termination requests.
	 * High events become block requests.
	 */
	if (policy->mode == PHANTOM_POLICY_LOCKDOWN) {
		if (event->severity >= PHANTOM_SEVERITY_CRITICAL)
			return PHANTOM_ACTION_TERMINATE;

		if (event->severity >= PHANTOM_SEVERITY_HIGH)
			return PHANTOM_ACTION_BLOCK;

		if (action == PHANTOM_ACTION_NONE)
			return PHANTOM_ACTION_ALERT;
	}

	return action;
}

/**
 * phantom_policy_apply() - apply policy decision to an event.
 * @policy: Active policy.
 * @event: Event to modify.
 *
 * Stores the computed action in the event. No neutralization is
 * performed here.
 *
 * Return: 0 on success, -EINVAL for invalid arguments,
 *         -EACCES if the policy is disabled.
 */
int phantom_policy_apply(struct phantom_policy *policy,
			 struct phantom_threat_event *event)
{
	u8 action;

	if (!policy || !event)
		return -EINVAL;

	if (policy->mode == PHANTOM_POLICY_DISABLED)
		return -EACCES;

	action = phantom_policy_get_action(policy, event);

	if (action == PHANTOM_ACTION_NONE)
		return -EINVAL;

	event->action = action;

	return 0;
}

/**
 * phantom_policy_reset() - restore default policy.
 * @policy: Policy object.
 *
 * Restores the conservative detect-only configuration.
 */
void phantom_policy_reset(struct phantom_policy *policy)
{
	if (!policy)
		return;

	policy->mode = PHANTOM_POLICY_DETECT;

	policy->info_action = PHANTOM_ACTION_LOG;
	policy->low_action = PHANTOM_ACTION_LOG;
	policy->medium_action = PHANTOM_ACTION_ALERT;
	policy->high_action = PHANTOM_ACTION_ALERT;
	policy->critical_action = PHANTOM_ACTION_ALERT;
}
