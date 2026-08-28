/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Threat scanner implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "scanner.h"

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/printk.h>

/*
 * Validate scanner target.
 */
static bool phantom_scanner_target_valid(
	enum phantom_scanner_target target)
{
	return target > PHANTOM_SCAN_TARGET_UNKNOWN &&
	       target <= PHANTOM_SCAN_TARGET_INTEGRITY;
}

/*
 * Validate scanner state.
 */
static bool phantom_scanner_state_valid(u8 state)
{
	return state <= PHANTOM_SCANNER_ERROR;
}

/*
 * Validate scanner flags.
 */
static bool phantom_scanner_flags_valid(u32 flags)
{
	const u32 valid_flags =
		PHANTOM_SCANNER_FLAG_ON_DEMAND |
		PHANTOM_SCANNER_FLAG_PERIODIC |
		PHANTOM_SCANNER_FLAG_BOOT |
		PHANTOM_SCANNER_FLAG_DEEP |
		PHANTOM_SCANNER_FLAG_INCREMENTAL;

	return !(flags & ~valid_flags);
}

/*
 * Convert an event source to a scanner target.
 */
static enum phantom_scanner_target
phantom_scanner_target_from_event(
	const struct phantom_threat_event *event)
{
	switch (event->source) {
	case PHANTOM_SOURCE_PROCESS:
		return PHANTOM_SCAN_TARGET_PROCESS;

	case PHANTOM_SOURCE_MEMORY:
		return PHANTOM_SCAN_TARGET_MEMORY;

	case PHANTOM_SOURCE_MODULE:
		return PHANTOM_SCAN_TARGET_MODULE;

	case PHANTOM_SOURCE_FILE:
		return PHANTOM_SCAN_TARGET_FILE;

	case PHANTOM_SOURCE_NETWORK:
		return PHANTOM_SCAN_TARGET_NETWORK;

	case PHANTOM_SOURCE_INTEGRITY:
		return PHANTOM_SCAN_TARGET_INTEGRITY;

	default:
		return PHANTOM_SCAN_TARGET_UNKNOWN;
	}
}

/**
 * phantom_scanner_init() - initialize scanner.
 * @scanner: Scanner instance.
 * @queue: Event queue used by scanner.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_init(struct phantom_scanner *scanner,
			 struct phantom_event_queue *queue)
{
	if (!scanner || !queue)
		return -EINVAL;

	scanner->state = PHANTOM_SCANNER_INITIALIZING;
	scanner->enabled = false;

	scanner->targets = 0;
	scanner->flags = PHANTOM_SCANNER_FLAG_NONE;

	scanner->scans_started = 0;
	scanner->scans_completed = 0;
	scanner->threats_found = 0;
	scanner->scan_errors = 0;
	scanner->events_queued = 0;

	scanner->queue = queue;

	scanner->state = PHANTOM_SCANNER_DISABLED;

	return 0;
}

/**
 * phantom_scanner_start() - start scanner.
 * @scanner: Scanner instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_start(struct phantom_scanner *scanner)
{
	if (!scanner)
		return -EINVAL;

	if (!phantom_scanner_state_valid(scanner->state))
		return -EINVAL;

	if (scanner->state != PHANTOM_SCANNER_DISABLED)
		return -EBUSY;

	if (!scanner->targets)
		return -ENODEV;

	scanner->enabled = true;
	scanner->state = PHANTOM_SCANNER_IDLE;

	return 0;
}

/**
 * phantom_scanner_stop() - stop scanner.
 * @scanner: Scanner instance.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_stop(struct phantom_scanner *scanner)
{
	if (!scanner)
		return -EINVAL;

	if (scanner->state != PHANTOM_SCANNER_IDLE &&
	    scanner->state != PHANTOM_SCANNER_RUNNING)
		return -EINVAL;

	scanner->enabled = false;
	scanner->state = PHANTOM_SCANNER_STOPPING;
	scanner->state = PHANTOM_SCANNER_DISABLED;

	return 0;
}

/**
 * phantom_scanner_enable_target() - enable scanner target.
 * @scanner: Scanner instance.
 * @target: Target to enable.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_enable_target(
	struct phantom_scanner *scanner,
	enum phantom_scanner_target target)
{
	if (!scanner)
		return -EINVAL;

	if (!phantom_scanner_target_valid(target))
		return -EINVAL;

	scanner->targets |= BIT(target);

	return 0;
}

/**
 * phantom_scanner_disable_target() - disable scanner target.
 * @scanner: Scanner instance.
 * @target: Target to disable.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_disable_target(
	struct phantom_scanner *scanner,
	enum phantom_scanner_target target)
{
	if (!scanner)
		return -EINVAL;

	if (!phantom_scanner_target_valid(target))
		return -EINVAL;

	scanner->targets &= ~BIT(target);

	return 0;
}

/**
 * phantom_scanner_target_enabled() - test scanner target.
 * @scanner: Scanner instance.
 * @target: Target to test.
 *
 * Return: %true when target is enabled, otherwise %false.
 */
bool phantom_scanner_target_enabled(
	const struct phantom_scanner *scanner,
	enum phantom_scanner_target target)
{
	if (!scanner)
		return false;

	if (!phantom_scanner_target_valid(target))
		return false;

	return test_bit(target, &scanner->targets);
}

/**
 * phantom_scanner_scan_event() - classify an existing event.
 * @scanner: Scanner instance.
 * @event: Event to inspect.
 *
 * This function does not claim that an event is malware merely because
 * it contains privileged or security-sensitive metadata. Concrete
 * scanner backends will provide actual evidence later.
 *
 * Return: Scan result.
 */
enum phantom_scan_result
phantom_scanner_scan_event(
	struct phantom_scanner *scanner,
	struct phantom_threat_event *event)
{
	if (!scanner || !event)
		return PHANTOM_SCAN_ERROR;

	if (!scanner->enabled ||
	    scanner->state != PHANTOM_SCANNER_IDLE)
		return PHANTOM_SCAN_ERROR;

	switch (event->severity) {
	case PHANTOM_SEVERITY_CRITICAL:
		return PHANTOM_SCAN_CRITICAL;

	case PHANTOM_SEVERITY_HIGH:
		return PHANTOM_SCAN_THREAT;

	case PHANTOM_SEVERITY_MEDIUM:
		return PHANTOM_SCAN_SUSPICIOUS;

	case PHANTOM_SEVERITY_LOW:
	case PHANTOM_SEVERITY_INFO:
	case PHANTOM_SEVERITY_UNKNOWN:
	default:
		return PHANTOM_SCAN_CLEAN;
	}
}

/**
 * phantom_scanner_submit_event() - submit event for scanning.
 * @scanner: Scanner instance.
 * @event: Event to submit.
 *
 * A successful submission adds one queue-owned reference to @event.
 * The caller retains its own reference.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_submit_event(
	struct phantom_scanner *scanner,
	struct phantom_threat_event *event)
{
	enum phantom_scanner_target target;
	int ret;

	if (!scanner || !event)
		return -EINVAL;

	if (!scanner->enabled ||
	    scanner->state != PHANTOM_SCANNER_IDLE)
		return -EAGAIN;

	if (!scanner->queue)
		return -ENODEV;

	target = phantom_scanner_target_from_event(event);

	if (!phantom_scanner_target_valid(target))
		return -EINVAL;

	if (!phantom_scanner_target_enabled(scanner, target))
		return -EACCES;

	ret = phantom_event_queue_push(scanner->queue, event);
	if (ret)
		return ret;

	event->flags |= PHANTOM_EVENT_FLAG_SCAN;
	scanner->events_queued++;

	return 0;
}

/**
 * phantom_scanner_run() - run one scanner pass.
 * @scanner: Scanner instance.
 *
 * Processes queued events currently available to the scanner. This
 * function intentionally performs only scanner-side classification;
 * source-specific scanning backends will be invoked from here later.
 *
 * Return: 0 when the pass completed, -EINVAL for invalid input,
 *         -EAGAIN when the scanner is not active, or another negative
 *         errno value on failure.
 */
int phantom_scanner_run(struct phantom_scanner *scanner)
{
	struct phantom_threat_event *event;
	enum phantom_scan_result result;

	if (!scanner)
		return -EINVAL;

	if (!scanner->enabled ||
	    scanner->state != PHANTOM_SCANNER_IDLE)
		return -EAGAIN;

	if (!scanner->queue)
		return -ENODEV;

	scanner->state = PHANTOM_SCANNER_RUNNING;
	scanner->scans_started++;

	while ((event = phantom_event_queue_pop(scanner->queue)) != NULL) {
		result = phantom_scanner_scan_event(scanner, event);

		switch (result) {
		case PHANTOM_SCAN_CLEAN:
			scanner->scans_completed++;
			event->result = PHANTOM_RESULT_ALLOWED;
			break;

		case PHANTOM_SCAN_SUSPICIOUS:
			scanner->scans_completed++;
			scanner->threats_found++;

			event->severity = max_t(u8,
						event->severity,
						PHANTOM_SEVERITY_MEDIUM);
			event->result = PHANTOM_RESULT_DETECTED;
			break;

		case PHANTOM_SCAN_THREAT:
			scanner->scans_completed++;
			scanner->threats_found++;

			event->severity = max_t(u8,
						event->severity,
						PHANTOM_SEVERITY_HIGH);
			event->result = PHANTOM_RESULT_DETECTED;
			break;

		case PHANTOM_SCAN_CRITICAL:
			scanner->scans_completed++;
			scanner->threats_found++;

			event->severity = PHANTOM_SEVERITY_CRITICAL;
			event->flags |= PHANTOM_EVENT_FLAG_CRITICAL;
			event->result = PHANTOM_RESULT_DETECTED;
			break;

		case PHANTOM_SCAN_NONE:
		case PHANTOM_SCAN_ERROR:
		default:
			scanner->scan_errors++;
			event->result = PHANTOM_RESULT_FAILED;
			phantom_threat_event_put(event);
			scanner->state = PHANTOM_SCANNER_IDLE;

			return -EIO;
		}

		/*
		 * Release the queue-owned reference.
		 */
		phantom_threat_event_put(event);
	}

	scanner->state = PHANTOM_SCANNER_IDLE;

	return 0;
}

/**
 * phantom_scanner_set_flags() - configure scanner flags.
 * @scanner: Scanner instance.
 * @flags: New scanner flags.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
int phantom_scanner_set_flags(
	struct phantom_scanner *scanner,
	u32 flags)
{
	if (!scanner)
		return -EINVAL;

	if (!phantom_scanner_flags_valid(flags))
		return -EINVAL;

	scanner->flags = flags;

	return 0;
}

/**
 * phantom_scanner_reset_stats() - reset scanner statistics.
 * @scanner: Scanner instance.
 */
void phantom_scanner_reset_stats(struct phantom_scanner *scanner)
{
	if (!scanner)
		return;

	scanner->scans_started = 0;
	scanner->scans_completed = 0;
	scanner->threats_found = 0;
	scanner->scan_errors = 0;
	scanner->events_queued = 0;
}
