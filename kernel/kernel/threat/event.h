/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat event interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_THREAT_EVENT_H
#define PHANTOM_THREAT_EVENT_H

#include <linux/gfp_types.h>
#include <linux/list.h>
#include <linux/refcount.h>
#include <linux/types.h>

/*
 * Maximum length of a threat name.
 */
#define PHANTOM_EVENT_NAME_LEN		128

/*
 * Maximum length of an event description.
 */
#define PHANTOM_EVENT_DESC_LEN		512

/*
 * Maximum length of a task command name.
 */
#define PHANTOM_EVENT_COMM_LEN		16

/*
 * Threat severity.
 *
 * Values increase with severity.
 */
enum phantom_threat_severity {
	PHANTOM_SEVERITY_UNKNOWN = 0,
	PHANTOM_SEVERITY_INFO,
	PHANTOM_SEVERITY_LOW,
	PHANTOM_SEVERITY_MEDIUM,
	PHANTOM_SEVERITY_HIGH,
	PHANTOM_SEVERITY_CRITICAL,
};

/*
 * Component that generated the event.
 */
enum phantom_threat_source {
	PHANTOM_SOURCE_UNKNOWN = 0,
	PHANTOM_SOURCE_SYSCALL,
	PHANTOM_SOURCE_PROCESS,
	PHANTOM_SOURCE_MEMORY,
	PHANTOM_SOURCE_FILE,
	PHANTOM_SOURCE_NETWORK,
	PHANTOM_SOURCE_MODULE,
	PHANTOM_SOURCE_INTEGRITY,
	PHANTOM_SOURCE_SCANNER,
	PHANTOM_SOURCE_POLICY,
};

/*
 * Action selected by the security policy.
 */
enum phantom_threat_action {
	PHANTOM_ACTION_NONE = 0,
	PHANTOM_ACTION_LOG,
	PHANTOM_ACTION_ALERT,
	PHANTOM_ACTION_BLOCK,
	PHANTOM_ACTION_QUARANTINE,
	PHANTOM_ACTION_ISOLATE,
	PHANTOM_ACTION_TERMINATE,
};

/*
 * Result of the selected action.
 */
enum phantom_threat_result {
	PHANTOM_RESULT_PENDING = 0,
	PHANTOM_RESULT_ALLOWED,
	PHANTOM_RESULT_DETECTED,
	PHANTOM_RESULT_BLOCKED,
	PHANTOM_RESULT_QUARANTINED,
	PHANTOM_RESULT_ISOLATED,
	PHANTOM_RESULT_TERMINATED,
	PHANTOM_RESULT_FAILED,
};

/*
 * Event flags.
 *
 * These values represent independent properties and therefore
 * should be treated as bit flags.
 */
enum phantom_threat_event_flags {
	PHANTOM_EVENT_FLAG_NONE		= 0,
	PHANTOM_EVENT_FLAG_KERNEL	= 1U << 0,
	PHANTOM_EVENT_FLAG_USER		= 1U << 1,
	PHANTOM_EVENT_FLAG_PANIC		= 1U << 2,
	PHANTOM_EVENT_FLAG_SCAN		= 1U << 3,
	PHANTOM_EVENT_FLAG_ASYNC		= 1U << 4,
	PHANTOM_EVENT_FLAG_CRITICAL	= 1U << 5,
};

/**
 * struct phantom_threat_event - one Phantom security event.
 * @refcount: Reference count controlling object lifetime.
 * @list: List node used when the event is queued.
 * @log_id: Unique monotonically increasing event identifier.
 * @timestamp_real_ns: Realtime timestamp in nanoseconds since Unix epoch.
 * @timestamp_mono_ns: Monotonic timestamp in nanoseconds.
 * @threat_id: Numeric identifier of the detected threat.
 * @severity: Severity of the threat.
 * @source: Component that generated the event.
 * @action: Action selected by the security policy.
 * @result: Result of the selected action.
 * @flags: Additional event properties.
 * @pid: PID of the task associated with the event.
 * @tgid: TGID of the task associated with the event.
 * @cpu: CPU on which the event was created.
 * @name: Short threat name.
 * @description: Detailed event description.
 * @comm: Command name of the task associated with the event.
 */
struct phantom_threat_event {
	refcount_t refcount;
	struct list_head list;

	u64 log_id;
	u64 timestamp_real_ns;
	u64 timestamp_mono_ns;

	u16 threat_id;

	u8 severity;
	u8 source;
	u8 action;
	u8 result;

	u32 flags;

	pid_t pid;
	pid_t tgid;

	u32 cpu;

	char name[PHANTOM_EVENT_NAME_LEN];
	char description[PHANTOM_EVENT_DESC_LEN];
	char comm[PHANTOM_EVENT_COMM_LEN];
};

/**
 * phantom_threat_event_alloc() - allocate a new threat event.
 * @gfp: Allocation flags appropriate for the caller's context.
 *
 * Allocates and initializes a new event object.
 *
 * Return: Pointer to the event on success, %NULL on allocation failure.
 */
struct phantom_threat_event *
phantom_threat_event_alloc(gfp_t gfp);

/**
 * phantom_threat_event_get() - acquire an event reference.
 * @event: Event whose reference should be acquired.
 *
 * The caller must already hold a valid reference to @event.
 */
static inline void phantom_threat_event_get(struct phantom_threat_event *event)
{
	refcount_inc(&event->refcount);
}

/**
 * phantom_threat_event_put() - release an event reference.
 * @event: Event whose reference should be released.
 *
 * Releases one reference. The final reference causes the event to
 * be freed by event.c.
 */
void phantom_threat_event_put(struct phantom_threat_event *event);

/**
 * phantom_threat_event_set_name() - set the threat name.
 * @event: Event to modify.
 * @name: NUL-terminated threat name.
 */
void phantom_threat_event_set_name(struct phantom_threat_event *event,
				   const char *name);

/**
 * phantom_threat_event_set_description() - set event description.
 * @event: Event to modify.
 * @description: NUL-terminated event description.
 */
void phantom_threat_event_set_description(struct phantom_threat_event *event,
					  const char *description);

/**
 * phantom_threat_event_init() - initialize a threat event.
 * @event: Event to initialize.
 * @threat_id: Numeric threat identifier.
 * @severity: Threat severity.
 * @source: Event source.
 *
 * Initializes event metadata, assigns a unique log ID, records
 * timestamps and stores information about the current task.
 *
 * Return: 0 on success, -EINVAL if @event is %NULL.
 */
int phantom_threat_event_init(struct phantom_threat_event *event,
			      u16 threat_id,
			      u8 severity,
			      u8 source);

#endif /* PHANTOM_THREAT_EVENT_H */
