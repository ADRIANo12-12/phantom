/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat event queue interface.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_THREAT_QUEUE_H
#define PHANTOM_THREAT_QUEUE_H

#include <linux/spinlock.h>
#include <linux/types.h>

#include "event.h"

/*
 * Default maximum number of events held by the queue.
 *
 * A bounded queue prevents untrusted or extremely noisy event
 * producers from consuming unbounded kernel memory.
 */
#define PHANTOM_EVENT_QUEUE_DEFAULT_MAX	1024

/**
 * struct phantom_event_queue - thread-safe threat event queue.
 * @lock: Protects queue state and list operations.
 * @head: Queue entries.
 * @depth: Current number of queued events.
 * @max_depth: Maximum number of queued events.
 * @enqueued: Number of successfully queued events.
 * @dequeued: Number of events removed from the queue.
 * @dropped: Number of events rejected because the queue was full.
 */
struct phantom_event_queue {
	spinlock_t lock;
	struct list_head head;

	u32 depth;
	u32 max_depth;

	u64 enqueued;
	u64 dequeued;
	u64 dropped;
};

/**
 * phantom_event_queue_init() - initialize an event queue.
 * @queue: Queue to initialize.
 * @max_depth: Maximum queue depth. Zero selects the default.
 *
 * Return: 0 on success or -EINVAL for invalid arguments.
 */
int phantom_event_queue_init(struct phantom_event_queue *queue,
			     u32 max_depth);

/**
 * phantom_event_queue_push() - add an event to the queue.
 * @queue: Destination queue.
 * @event: Event to queue.
 *
 * The queue acquires one reference to @event. The caller retains its
 * existing reference and remains responsible for releasing it.
 *
 * Return: 0 on success, -EINVAL for invalid arguments,
 *         -ENOSPC if the queue is full.
 */
int phantom_event_queue_push(struct phantom_event_queue *queue,
			     struct phantom_threat_event *event);

/**
 * phantom_event_queue_pop() - remove the oldest event.
 * @queue: Source queue.
 *
 * Returns one queue-owned reference to the oldest event. The caller
 * must release it with phantom_threat_event_put().
 *
 * Return: Event pointer or %NULL if the queue is empty.
 */
struct phantom_threat_event *
phantom_event_queue_pop(struct phantom_event_queue *queue);

/**
 * phantom_event_queue_empty() - test whether the queue is empty.
 * @queue: Queue to inspect.
 *
 * Return: %true if empty or invalid, otherwise %false.
 */
bool phantom_event_queue_empty(struct phantom_event_queue *queue);

/**
 * phantom_event_queue_depth() - return current queue depth.
 * @queue: Queue to inspect.
 *
 * Return: Number of queued events, or zero for an invalid queue.
 */
u32 phantom_event_queue_depth(struct phantom_event_queue *queue);

/**
 * phantom_event_queue_set_max_depth() - change queue capacity.
 * @queue: Queue to modify.
 * @max_depth: New maximum depth.
 *
 * The new limit cannot be lower than the current number of queued
 * events.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_event_queue_set_max_depth(struct phantom_event_queue *queue,
				      u32 max_depth);

/**
 * phantom_event_queue_reset_stats() - reset queue statistics.
 * @queue: Queue whose counters should be reset.
 */
void phantom_event_queue_reset_stats(struct phantom_event_queue *queue);

/**
 * phantom_event_queue_flush() - remove all events from the queue.
 * @queue: Queue to flush.
 *
 * Releases the queue-owned reference for every removed event.
 */
void phantom_event_queue_flush(struct phantom_event_queue *queue);

#endif /* PHANTOM_THREAT_QUEUE_H */
