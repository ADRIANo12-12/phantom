/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat subsystem security implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "secure.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/string.h>

/*
 * Check whether a security state is valid.
 */
static bool phantom_secure_state_valid(u8 state)
{
	return state <= PHANTOM_SECURE_ERROR;
}

/*
 * Check whether an integrity state is valid.
 */
static bool phantom_secure_integrity_valid(u8 integrity)
{
	return integrity <= PHANTOM_INTEGRITY_COMPROMISED;
}

/*
 * Check whether an event severity is valid.
 */
static bool phantom_secure_severity_valid(u8 severity)
{
	return severity <= PHANTOM_SEVERITY_CRITICAL;
}

/*
 * Check whether an event source is valid.
 */
static bool phantom_secure_source_valid(u8 source)
{
	return source <= PHANTOM_SOURCE_POLICY;
}

/*
 * Check whether an event action is valid.
 */
static bool phantom_secure_action_valid(u8 action)
{
	return action <= PHANTOM_ACTION_TERMINATE;
}

/*
 * Check whether an event result is valid.
 */
static bool phantom_secure_result_valid(u8 result)
{
	return result <= PHANTOM_RESULT_FAILED;
}

/*
 * Check whether a fixed-size NUL-terminated string is valid.
 *
 * A completely filled buffer without a NUL terminator is considered
 * invalid because kernel code must not treat it as a C string.
 */
static bool phantom_secure_string_valid(const char *str, size_t size)
{
	if (!str || !size)
		return false;

	return strnlen(str, size) < size;
}

/*
 * Initialize the security subsystem.
 */
int phantom_secure_init(struct phantom_secure *secure)
{
	if (!secure)
		return -EINVAL;

	secure->state = PHANTOM_SECURE_INITIALIZING;
	secure->integrity = PHANTOM_INTEGRITY_UNKNOWN;
	secure->flags = PHANTOM_SECURE_FLAG_NONE;

	secure->integrity_checks = 0;
	secure->integrity_failures = 0;
	secure->invalid_events = 0;
	secure->lockdown_events = 0;

	/*
	 * Enable the protections owned by this layer.
	 *
	 * Panic monitoring itself is registered by the subsystem's panic
	 * notifier and is therefore not enabled here.
	 */
	secure->flags =
		PHANTOM_SECURE_FLAG_INTEGRITY |
		PHANTOM_SECURE_FLAG_EVENT_VALIDATION |
		PHANTOM_SECURE_FLAG_SELF_CHECK;

	secure->integrity = PHANTOM_INTEGRITY_VALID;
	secure->state = PHANTOM_SECURE_ACTIVE;

	return 0;
}

/*
 * Enable the security subsystem.
 */
int phantom_secure_enable(struct phantom_secure *secure)
{
	if (!secure)
		return -EINVAL;

	if (!phantom_secure_state_valid(secure->state))
		return -EINVAL;

	if (secure->state == PHANTOM_SECURE_ERROR)
		return -EIO;

	if (secure->state == PHANTOM_SECURE_LOCKDOWN)
		return -EBUSY;

	if (secure->state == PHANTOM_SECURE_ACTIVE)
		return 0;

	secure->state = PHANTOM_SECURE_ACTIVE;

	secure->flags |=
		PHANTOM_SECURE_FLAG_INTEGRITY |
		PHANTOM_SECURE_FLAG_EVENT_VALIDATION |
		PHANTOM_SECURE_FLAG_SELF_CHECK;

	return 0;
}

/*
 * Disable the security subsystem.
 */
int phantom_secure_disable(struct phantom_secure *secure)
{
	if (!secure)
		return -EINVAL;

	if (secure->state == PHANTOM_SECURE_LOCKDOWN)
		return -EBUSY;

	if (secure->state != PHANTOM_SECURE_ACTIVE &&
	    secure->state != PHANTOM_SECURE_DEGRADED)
		return -EINVAL;

	secure->state = PHANTOM_SECURE_STOPPING;
	secure->flags = PHANTOM_SECURE_FLAG_NONE;

	secure->state = PHANTOM_SECURE_DISABLED;

	return 0;
}

/*
 * Enter lockdown mode.
 */
int phantom_secure_enter_lockdown(struct phantom_secure *secure)
{
	if (!secure)
		return -EINVAL;

	if (!phantom_secure_state_valid(secure->state))
		return -EINVAL;

	if (secure->state == PHANTOM_SECURE_DISABLED)
		return -EACCES;

	secure->state = PHANTOM_SECURE_LOCKDOWN;

	secure->flags |=
		PHANTOM_SECURE_FLAG_INTEGRITY |
		PHANTOM_SECURE_FLAG_EVENT_VALIDATION |
		PHANTOM_SECURE_FLAG_LOCKDOWN |
		PHANTOM_SECURE_FLAG_SELF_CHECK;

	secure->lockdown_events++;

	return 0;
}

/*
 * Check the internal state of the security subsystem.
 *
 * This is a state-consistency check, not a cryptographic verification
 * of kernel text or modules.
 */
int phantom_secure_check_integrity(struct phantom_secure *secure)
{
	if (!secure)
		return -EINVAL;

	secure->integrity_checks++;

	if (!phantom_secure_state_valid(secure->state) ||
	    !phantom_secure_integrity_valid(secure->integrity)) {
		secure->integrity = PHANTOM_INTEGRITY_COMPROMISED;
		secure->integrity_failures++;
		secure->state = PHANTOM_SECURE_ERROR;

		return -EIO;
	}

	/*
	 * Active security state must have the mandatory protections.
	 */
	if (secure->state == PHANTOM_SECURE_ACTIVE) {
		const u32 required_flags =
			PHANTOM_SECURE_FLAG_INTEGRITY |
			PHANTOM_SECURE_FLAG_EVENT_VALIDATION |
			PHANTOM_SECURE_FLAG_SELF_CHECK;

		if ((secure->flags & required_flags) != required_flags) {
			secure->integrity = PHANTOM_INTEGRITY_SUSPICIOUS;
			secure->integrity_failures++;
			secure->state = PHANTOM_SECURE_DEGRADED;

			return -EUCLEAN;
		}
	}

	/*
	 * Lockdown must always keep the lockdown flag set.
	 */
	if (secure->state == PHANTOM_SECURE_LOCKDOWN &&
	    !(secure->flags & PHANTOM_SECURE_FLAG_LOCKDOWN)) {
		secure->integrity = PHANTOM_INTEGRITY_SUSPICIOUS;
		secure->integrity_failures++;
		secure->state = PHANTOM_SECURE_ERROR;

		return -EIO;
	}

	secure->integrity = PHANTOM_INTEGRITY_VALID;

	return 0;
}

/*
 * Validate one threat event before it enters the security pipeline.
 */
int phantom_secure_validate_event(
	struct phantom_secure *secure,
	const struct phantom_threat_event *event)
{
	if (!secure || !event)
		return -EINVAL;

	if (secure->state == PHANTOM_SECURE_DISABLED)
		return -EACCES;

	if (secure->state == PHANTOM_SECURE_ERROR)
		return -EIO;

	if (!(secure->flags & PHANTOM_SECURE_FLAG_EVENT_VALIDATION))
		return -EACCES;

	/*
	 * Basic numeric field validation.
	 */
	if (!event->log_id)
		goto invalid_event;

	if (!phantom_secure_severity_valid(event->severity))
		goto invalid_event;

	if (!phantom_secure_source_valid(event->source))
		goto invalid_event;

	if (!phantom_secure_action_valid(event->action))
		goto invalid_event;

	if (!phantom_secure_result_valid(event->result))
		goto invalid_event;

	/*
	 * A kernel threat event must have a valid monotonic timestamp.
	 */
	if (!event->timestamp_mono_ns)
		goto invalid_event;

	/*
	 * Realtime can theoretically be unavailable/invalid very early
	 * during boot, so it is not treated as a fatal validation failure
	 * here.
	 */

	/*
	 * Validate all fixed-size strings before passing the event to
	 * code that may use them with %s.
	 */
	if (!phantom_secure_string_valid(event->name,
					sizeof(event->name)))
		goto invalid_event;

	if (!phantom_secure_string_valid(event->description,
					sizeof(event->description)))
		goto invalid_event;

	if (!phantom_secure_string_valid(event->comm,
					sizeof(event->comm)))
		goto invalid_event;

	/*
	 * A critical event must carry the critical flag.
	 */
	if (event->severity == PHANTOM_SEVERITY_CRITICAL &&
	    !(event->flags & PHANTOM_EVENT_FLAG_CRITICAL))
		goto invalid_event;

	return 0;

invalid_event:
	secure->invalid_events++;
	return -EINVAL;
}

/*
 * Return whether the security subsystem is in lockdown.
 */
bool phantom_secure_is_locked_down(
	const struct phantom_secure *secure)
{
	if (!secure)
		return false;

	return secure->state == PHANTOM_SECURE_LOCKDOWN;
}

/*
 * Return whether the security subsystem is healthy.
 */
bool phantom_secure_is_healthy(
	const struct phantom_secure *secure)
{
	if (!secure)
		return false;

	if (secure->state != PHANTOM_SECURE_ACTIVE)
		return false;

	return secure->integrity == PHANTOM_INTEGRITY_VALID;
}

/*
 * Reset security statistics.
 */
void phantom_secure_reset_stats(struct phantom_secure *secure)
{
	if (!secure)
		return;

	secure->integrity_checks = 0;
	secure->integrity_failures = 0;
	secure->invalid_events = 0;
	secure->lockdown_events = 0;
}
