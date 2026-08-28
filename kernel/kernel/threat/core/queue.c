/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat event queue implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "queue.h"

#include <linux/errno.h>
#include <linux/kernel.h>

/**
 * phantom_event_queue_init() - initialize an event queue.
 * @queue: Queue to initialize.
 * @max_depth: Maximum queue depth. Zero selects the default.
 *
 * Return: 0 on success or -EINVAL for invalid arguments.
 */
int phantom_event_queue_init(struct phantom_event_queue *queue,
			     u32 max_depth)
{
	if (!queue)
		return -EINVAL;

	if (!max_depth)
		max_depth = PHANTOM_EVENT_QUEUE_DEFAULT_MAX;

	if (!max_depth)
		return -EINVAL;

	spin_lock_init(&queue->lock);
	INIT_LIST_HEAD(&queue->head);

	queue->depth = 0;
	queue->max_depth = max_depth;

	queue->enqueued = 0;
	queue->dequeued = 0;
	queue->dropped = 0;

	return 0;
}

/**
 * phantom_event_queue_push() - add an event to the queue.
 * @queue: Destination queue.
 * @event: Event to queue.
 *
 * The queue acquires a reference before linking the event. This means
 * the event remains alive even if the caller releases its own reference
 * immediately after this function returns.
 *
 * Return: 0 on success, -EINVAL for invalid arguments,
 *         -ENOSPC if the queue is full.
 */
int phantom_event_queue_push(struct phantom_event_queue *queue,
			     struct phantom_threat_event *event)
{
	unsigned long flags;

	if (!queue || !event)
		return -EINVAL;

	spin_lock_irqsave(&queue->lock, flags);

	if (queue->depth >= queue->max_depth) {
		queue->dropped++;
		spin_unlock_irqrestore(&queue->lock, flags);

		return -ENOSPC;
	}

	/*
	 * The queue owns one independent reference.
	 */
	phantom_threat_event_get(event);

	list_add_tail(&event->list, &queue->head);
	queue->depth++;
	queue->enqueued++;

	spin_unlock_irqrestore(&queue->lock, flags);

	return 0;
}

/**
 * phantom_event_queue_pop() - remove the oldest event.
 * @queue: Source queue.
 *
 * Removes the oldest event and transfers the queue-owned reference
 * to the caller.
 *
 * Return: Event pointer or %NULL if the queue is empty.
 */
struct phantom_threat_event *
phantom_event_queue_pop(struct phantom_event_queue *queue)
{
	struct phantom_threat_event *event;
	unsigned long flags;

	if (!queue)
		return NULL;

	spin_lock_irqsave(&queue->lock, flags);

	if (list_empty(&queue->head)) {
		spin_unlock_irqrestore(&queue->lock, flags);
		return NULL;
	}

	event = list_first_entry(&queue->head,
				 struct phantom_threat_event,
				 list);

	list_del_init(&event->list);

	queue->depth--;
	queue->dequeued++;

	spin_unlock_irqrestore(&queue->lock, flags);

	return event;
}

/**
 * phantom_event_queue_empty() - test whether queue is empty.
 * @queue: Queue to inspect.
 *
 * Return: %true if empty or invalid, otherwise %false.
 */
bool phantom_event_queue_empty(struct phantom_event_queue *queue)
{
	unsigned long flags;
	bool empty;

	if (!queue)
		return true;

	spin_lock_irqsave(&queue->lock, flags);
	empty = list_empty(&queue->head);
	spin_unlock_irqrestore(&queue->lock, flags);

	return empty;
}

/**
 * phantom_event_queue_depth() - return current queue depth.
 * @queue: Queue to inspect.
 *
 * Return: Current number of queued events.
 */
u32 phantom_event_queue_depth(struct phantom_event_queue *queue)
{
	unsigned long flags;
	u32 depth;

	if (!queue)
		return 0;

	spin_lock_irqsave(&queue->lock, flags);
	depth = queue->depth;
	spin_unlock_irqrestore(&queue->lock, flags);

	return depth;
}

/**
 * phantom_event_queue_set_max_depth() - change queue capacity.
 * @queue: Queue to modify.
 * @max_depth: New maximum queue depth.
 *
 * Return: 0 on success, -EINVAL for invalid arguments, or -EBUSY if
 * the new capacity would be smaller than the current queue depth.
 */
int phantom_event_queue_set_max_depth(struct phantom_event_queue *queue,
				      u32 max_depth)
{
	unsigned long flags;

	if (!queue || !max_depth)
		return -EINVAL;

	spin_lock_irqsave(&queue->lock, flags);

	if (max_depth < queue->depth) {
		spin_unlock_irqrestore(&queue->lock, flags);
		return -EBUSY;
	}

	queue->max_depth = max_depth;

	spin_unlock_irqrestore(&queue->lock, flags);

	return 0;
}

/**
 * phantom_event_queue_reset_stats() - reset queue statistics.
 * @queue: Queue whose statistics should be reset.
 */
void phantom_event_queue_reset_stats(struct phantom_event_queue *queue)
{
	unsigned long flags;

	if (!queue)
		return;

	spin_lock_irqsave(&queue->lock, flags);

	queue->enqueued = 0;
	queue->dequeued = 0;
	queue->dropped = 0;

	spin_unlock_irqrestore(&queue->lock, flags);
}

/**
 * phantom_event_queue_flush() - remove all events.
 * @queue: Queue to flush.
 *
 * The queue lock is held only while unlinking events. References are
 * released after dropping the lock so the destruction path cannot
 * perform arbitrary work while holding the queue spinlock.
 */
void phantom_event_queue_flush(struct phantom_event_queue *queue)
{
	LIST_HEAD(release_list);
	struct phantom_threat_event *event;
	struct phantom_threat_event *tmp;
	unsigned long flags;

	if (!queue)
		return;

	spin_lock_irqsave(&queue->lock, flags);

	list_splice_init(&queue->head, &release_list);

	queue->depth = 0;

	spin_unlock_irqrestore(&queue->lock, flags);

	list_for_each_entry_safe(event, tmp, &release_list, list) {
		list_del_init(&event->list);
		phantom_threat_event_put(event);
	}
}
