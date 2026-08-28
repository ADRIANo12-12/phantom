/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat subsystem security interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_THREAT_SECURE_H
#define PHANTOM_THREAT_SECURE_H

#include <linux/types.h>

#include "event.h"

/*
 * Security subsystem state.
 */
enum phantom_secure_state {
	PHANTOM_SECURE_DISABLED = 0,
	PHANTOM_SECURE_INITIALIZING,
	PHANTOM_SECURE_ACTIVE,
	PHANTOM_SECURE_DEGRADED,
	PHANTOM_SECURE_LOCKDOWN,
	PHANTOM_SECURE_STOPPING,
	PHANTOM_SECURE_ERROR,
};

/*
 * Integrity state of the Phantom threat subsystem.
 */
enum phantom_secure_integrity {
	PHANTOM_INTEGRITY_UNKNOWN = 0,
	PHANTOM_INTEGRITY_VALID,
	PHANTOM_INTEGRITY_SUSPICIOUS,
	PHANTOM_INTEGRITY_COMPROMISED,
};

/*
 * Security policy flags.
 *
 * These flags describe what protections are currently active.
 */
enum phantom_secure_flags {
	PHANTOM_SECURE_FLAG_NONE		= 0,
	PHANTOM_SECURE_FLAG_INTEGRITY		= 1U << 0,
	PHANTOM_SECURE_FLAG_EVENT_VALIDATION	= 1U << 1,
	PHANTOM_SECURE_FLAG_LOCKDOWN		= 1U << 2,
	PHANTOM_SECURE_FLAG_PANIC_MONITOR	= 1U << 3,
	PHANTOM_SECURE_FLAG_SELF_CHECK		= 1U << 4,
};

/**
 * struct phantom_secure - security state of Phantom threat subsystem.
 * @state: Current security subsystem state.
 * @integrity: Current integrity state.
 * @flags: Enabled security mechanisms.
 * @integrity_checks: Number of integrity checks performed.
 * @integrity_failures: Number of failed integrity checks.
 * @invalid_events: Number of rejected invalid events.
 * @lockdown_events: Number of events that forced or confirmed lockdown.
 */
struct phantom_secure {
	u8 state;
	u8 integrity;

	u32 flags;

	u64 integrity_checks;
	u64 integrity_failures;
	u64 invalid_events;
	u64 lockdown_events;
};

/**
 * phantom_secure_init() - initialize security subsystem.
 * @secure: Security state object.
 *
 * Initializes all security mechanisms and starts with a conservative
 * active configuration.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_secure_init(struct phantom_secure *secure);

/**
 * phantom_secure_enable() - enable security subsystem.
 * @secure: Security state object.
 *
 * Enables active security checks.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_secure_enable(struct phantom_secure *secure);

/**
 * phantom_secure_disable() - disable security subsystem.
 * @secure: Security state object.
 *
 * Disables security checks.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_secure_disable(struct phantom_secure *secure);

/**
 * phantom_secure_enter_lockdown() - enter security lockdown.
 * @secure: Security state object.
 *
 * Changes security state to lockdown.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_secure_enter_lockdown(struct phantom_secure *secure);

/**
 * phantom_secure_check_integrity() - perform subsystem integrity check.
 * @secure: Security state object.
 *
 * Performs available integrity checks and updates the integrity state.
 *
 * Return: 0 if integrity is valid, or a negative errno value if the
 * integrity check fails.
 */
int phantom_secure_check_integrity(struct phantom_secure *secure);

/**
 * phantom_secure_validate_event() - validate a threat event.
 * @secure: Security state object.
 * @event: Event to validate.
 *
 * Validates mandatory event fields before an event enters the security
 * pipeline.
 *
 * Return: 0 if valid or a negative errno value otherwise.
 */
int phantom_secure_validate_event(
	struct phantom_secure *secure,
	const struct phantom_threat_event *event);

/**
 * phantom_secure_is_locked_down() - test lockdown state.
 * @secure: Security state object.
 *
 * Return: %true when the subsystem is in lockdown.
 */
bool phantom_secure_is_locked_down(
	const struct phantom_secure *secure);

/**
 * phantom_secure_is_healthy() - test subsystem health.
 * @secure: Security state object.
 *
 * Return: %true when the security subsystem is active and integrity
 * is currently valid.
 */
bool phantom_secure_is_healthy(
	const struct phantom_secure *secure);

/**
 * phantom_secure_reset_stats() - reset security statistics.
 * @secure: Security state object.
 *
 * Resets security counters without changing the current security state.
 */
void phantom_secure_reset_stats(struct phantom_secure *secure);

#endif /* PHANTOM_THREAT_SECURE_H */
