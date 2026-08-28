/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat neutralizer interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_THREAT_NEUTRALIZER_H
#define PHANTOM_THREAT_NEUTRALIZER_H

#include <linux/types.h>

#include "event.h"

/*
 * Neutralizer state.
 */
enum phantom_neutralizer_state {
	PHANTOM_NEUTRALIZER_DISABLED = 0,
	PHANTOM_NEUTRALIZER_ENABLED,
};

/*
 * Supported neutralization actions.
 *
 * These values mirror the event action namespace but only expose
 * the actions the neutralizer currently knows how to execute.
 */
enum phantom_neutralizer_action {
	PHANTOM_NEUTRALIZE_NONE = 0,
	PHANTOM_NEUTRALIZE_LOG,
	PHANTOM_NEUTRALIZE_ALERT,
	PHANTOM_NEUTRALIZE_BLOCK,
};

/**
 * struct phantom_neutralizer - neutralizer subsystem state.
 * @state: Current neutralizer state.
 * @enabled: Whether active neutralization is enabled.
 * @neutralizations: Number of events neutralized.
 * @blocked: Number of events that resulted in blocking.
 */
struct phantom_neutralizer {
	u8 state;
	bool enabled;

	u64 neutralizations;
	u64 blocked;
};

/**
 * phantom_neutralizer_init() - initialize the neutralizer subsystem.
 * @neutralizer: Neutralizer instance to initialize.
 *
 * Initializes the neutralizer in a disabled, non-destructive state.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_neutralizer_init(struct phantom_neutralizer *neutralizer);

/**
 * phantom_neutralizer_enable() - enable active neutralization.
 * @neutralizer: Neutralizer instance.
 *
 * Enables execution of blocking actions for events that request them.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_neutralizer_enable(struct phantom_neutralizer *neutralizer);

/**
 * phantom_neutralizer_disable() - disable active neutralization.
 * @neutralizer: Neutralizer instance.
 *
 * Restores the default detect-only behaviour.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_neutralizer_disable(struct phantom_neutralizer *neutralizer);

/**
 * phantom_neutralizer_execute() - execute the action selected for an event.
 * @neutralizer: Neutralizer instance.
 * @event: Event whose selected action should be executed.
 *
 * Applies @event->action. Non-destructive by default; BLOCK is only
 * performed when the neutralizer is enabled.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_neutralizer_execute(struct phantom_neutralizer *neutralizer,
				struct phantom_threat_event *event);

/**
 * phantom_neutralizer_reset_stats() - reset neutralizer statistics.
 * @neutralizer: Neutralizer instance.
 *
 * Resets neutralization counters to zero.
 */
void phantom_neutralizer_reset_stats(struct phantom_neutralizer *neutralizer);

#endif /* PHANTOM_THREAT_NEUTRALIZER_H */