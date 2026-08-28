/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat event implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "event.h"

#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/timekeeping.h>

/*
 * Global monotonically increasing event ID.
 *
 * atomic64_t matches the u64 log_id stored in struct
 * phantom_threat_event.
 */
static atomic64_t phantom_event_log_id = ATOMIC64_INIT(0);

/**
 * phantom_threat_event_alloc() - allocate a new threat event
 * @gfp: GFP allocation flags for the caller's context
 *
 * Allocates and initializes a new event object.
 *
 * The returned event owns one reference. The caller must eventually
 * release it with phantom_threat_event_put().
 *
 * Return: Pointer to the event, or %NULL on allocation failure.
 */
struct phantom_threat_event *
phantom_threat_event_alloc(gfp_t gfp)
{
	struct phantom_threat_event *event;

	event = kzalloc(sizeof(*event), gfp);
	if (!event)
		return NULL;

	refcount_set(&event->refcount, 1);
	INIT_LIST_HEAD(&event->list);

	return event;
}

/**
 * phantom_threat_event_put() - release an event reference
 * @event: Event whose reference is being released
 *
 * Releases one reference to the event. The final reference frees
 * the event after verifying that it is not currently linked into
 * an event list.
 */
void phantom_threat_event_put(struct phantom_threat_event *event)
{
	if (!event)
		return;

	if (refcount_dec_and_test(&event->refcount)) {
		WARN_ON_ONCE(!list_empty(&event->list));
		kfree(event);
	}
}

/**
 * phantom_threat_event_set_name() - set the event threat name
 * @event: Event to modify
 * @name: NUL-terminated threat name
 *
 * Copies @name into the event. The destination is always NUL-terminated.
 * Passing %NULL clears the current name.
 */
void phantom_threat_event_set_name(struct phantom_threat_event *event,
				   const char *name)
{
	if (!event)
		return;

	if (!name) {
		event->name[0] = '\0';
		return;
	}

	strscpy(event->name, name, sizeof(event->name));
}

/**
 * phantom_threat_event_set_description() - set event description
 * @event: Event to modify
 * @description: NUL-terminated description
 *
 * Copies @description into the event. The destination is always
 * NUL-terminated. Passing %NULL clears the current description.
 */
void phantom_threat_event_set_description(struct phantom_threat_event *event,
					  const char *description)
{
	if (!event)
		return;

	if (!description) {
		event->description[0] = '\0';
		return;
	}

	strscpy(event->description, description,
		sizeof(event->description));
}

/**
 * phantom_threat_event_init() - initialize a threat event
 * @event: Event to initialize
 * @threat_id: Numeric threat identifier
 * @severity: Threat severity
 * @source: Component that generated the event
 *
 * Initializes the complete event state, assigns a globally unique
 * monotonically increasing log ID, records realtime and monotonic
 * timestamps, and records the current task context.
 *
 * The event must have been allocated with phantom_threat_event_alloc()
 * or otherwise have a valid lifetime reference and list state.
 *
 * Return: 0 on success, -EINVAL if @event is %NULL.
 */
int phantom_threat_event_init(struct phantom_threat_event *event,
			      u16 threat_id,
			      u8 severity,
			      u8 source)
{
	if (!event)
		return -EINVAL;

	/*
	 * Every initialized event receives a new unique ID.
	 */
	event->log_id = atomic64_inc_return(&phantom_event_log_id);

	/*
	 * Store both wall-clock and monotonic timestamps.
	 *
	 * Realtime is useful for human/audit output.
	 * Monotonic time is useful for ordering and measuring intervals.
	 */
	event->timestamp_real_ns = ktime_get_real_ns();
	event->timestamp_mono_ns = ktime_get_mono_fast_ns();

	/*
	 * Threat metadata.
	 */
	event->threat_id = threat_id;
	event->severity = severity;
	event->source = source;

	/*
	 * No action has been selected yet.
	 * No action result exists yet.
	 */
	event->action = PHANTOM_ACTION_NONE;
	event->result = PHANTOM_RESULT_PENDING;

	/*
	 * No special event properties by default.
	 */
	event->flags = PHANTOM_EVENT_FLAG_NONE;

	/*
	 * Record task context.
	 */
	event->pid = current->pid;
	event->tgid = current->tgid;
	event->cpu = raw_smp_processor_id();

	/*
	 * Clear textual fields.
	 */
	event->name[0] = '\0';
	event->description[0] = '\0';
	event->comm[0] = '\0';

	/*
	 * Save the current task command name.
	 */
	strscpy(event->comm, current->comm, sizeof(event->comm));

	return 0;
}
