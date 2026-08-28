/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat policy interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_THREAT_POLICY_H
#define PHANTOM_THREAT_POLICY_H

#include <linux/types.h>

#include "event.h"

/*
 * Policy execution mode.
 *
 * DETECT:
 *      Detection and reporting only.
 *
 * ENFORCE:
 *      Policy decisions may result in active neutralization.
 *
 * LOCKDOWN:
 *      Most restrictive policy mode.
 */
enum phantom_policy_mode {
	PHANTOM_POLICY_DISABLED = 0,
	PHANTOM_POLICY_DETECT,
	PHANTOM_POLICY_ENFORCE,
	PHANTOM_POLICY_LOCKDOWN,
};

/**
 * struct phantom_policy - active Phantom security policy.
 * @mode: Current policy execution mode.
 * @info_action: Action for informational events.
 * @low_action: Action for low severity events.
 * @medium_action: Action for medium severity events.
 * @high_action: Action for high severity events.
 * @critical_action: Action for critical events.
 */
struct phantom_policy {
	u8 mode;

	u8 info_action;
	u8 low_action;
	u8 medium_action;
	u8 high_action;
	u8 critical_action;
};

/**
 * phantom_policy_init() - initialize a policy.
 * @policy: Policy object to initialize.
 *
 * Initializes a conservative detect-only policy.
 *
 * Return: 0 on success, -EINVAL if @policy is NULL.
 */
int phantom_policy_init(struct phantom_policy *policy);

/**
 * phantom_policy_set_mode() - set policy operating mode.
 * @policy: Policy object.
 * @mode: New policy mode.
 *
 * Return: 0 on success, -EINVAL for invalid arguments.
 */
int phantom_policy_set_mode(struct phantom_policy *policy, u8 mode);

/**
 * phantom_policy_get_mode() - get current policy mode.
 * @policy: Policy object.
 *
 * Return: Current policy mode, or PHANTOM_POLICY_DISABLED for NULL.
 */
u8 phantom_policy_get_mode(const struct phantom_policy *policy);

/**
 * phantom_policy_set_action() - configure severity action.
 * @policy: Policy object.
 * @severity: Threat severity.
 * @action: Action to execute for that severity.
 *
 * Return: 0 on success, -EINVAL for invalid arguments.
 */
int phantom_policy_set_action(struct phantom_policy *policy,
			      u8 severity,
			      u8 action);

/**
 * phantom_policy_get_action() - determine action for an event.
 * @policy: Active policy.
 * @event: Threat event.
 *
 * Does not modify @event.
 *
 * Return: Selected action.
 */
u8 phantom_policy_get_action(const struct phantom_policy *policy,
			     const struct phantom_threat_event *event);

/**
 * phantom_policy_apply() - apply policy decision to an event.
 * @policy: Active policy.
 * @event: Event to modify.
 *
 * Stores the selected action in @event->action.
 *
 * Return: 0 on success, -EINVAL for invalid arguments,
 *         -EACCES if policy is disabled.
 */
int phantom_policy_apply(struct phantom_policy *policy,
			 struct phantom_threat_event *event);

/**
 * phantom_policy_reset() - restore default policy.
 * @policy: Policy object.
 *
 * Restores the conservative detect-only policy.
 */
void phantom_policy_reset(struct phantom_policy *policy);

#endif /* PHANTOM_THREAT_POLICY_H */
