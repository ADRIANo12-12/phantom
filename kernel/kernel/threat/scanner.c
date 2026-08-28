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
#include <linux/string.h>

/*
 * Check whether a scanner target is valid.
 */
static bool phantom_scanner_target_valid(
	enum phantom_scanner_target target)
{
	return target > PHANTOM_SCAN_TARGET_UNKNOWN &&
	       target <= PHANTOM_SCAN_TARGET_INTEGRITY;
}

/*
 * Initialize scanner.
 */
int phantom_scanner_init(struct phantom_scanner *scanner)
{
	if (!scanner)
		return -EINVAL;

	scanner->state = PHANTOM_SCANNER_INITIALIZING;
	scanner->enabled = false;
	scanner->targets = 0;

	scanner->scans_started = 0;
	scanner->scans_completed = 0;
	scanner->threats_found = 0;
	scanner->scan_errors = 0;

	scanner->state = PHANTOM_SCANNER_DISABLED;

	return 0;
}

/*
 * Start scanner.
 */
int phantom_scanner_start(struct phantom_scanner *scanner)
{
	if (!scanner)
		return -EINVAL;

	if (scanner->state != PHANTOM_SCANNER_DISABLED)
		return -EBUSY;

	scanner->enabled = true;
	scanner->state = PHANTOM_SCANNER_IDLE;

	return 0;
}

/*
 * Stop scanner.
 */
int phantom_scanner_stop(struct phantom_scanner *scanner)
{
	if (!scanner)
		return -EINVAL;

	if (scanner->state != PHANTOM_SCANNER_IDLE &&
	    scanner->state != PHANTOM_SCANNER_RUNNING)
		return -EINVAL;

	scanner->state = PHANTOM_SCANNER_STOPPING;
	scanner->enabled = false;

	scanner->state = PHANTOM_SCANNER_DISABLED;

	return 0;
}

/*
 * Enable one scanner target.
 */
int phantom_scanner_enable_target(struct phantom_scanner *scanner,
				  enum phantom_scanner_target target)
{
	if (!scanner)
		return -EINVAL;

	if (!phantom_scanner_target_valid(target))
		return -EINVAL;

	scanner->targets |= BIT(target);

	return 0;
}

/*
 * Disable one scanner target.
 */
int phantom_scanner_disable_target(struct phantom_scanner *scanner,
				   enum phantom_scanner_target target)
{
	if (!scanner)
		return -EINVAL;

	if (!phantom_scanner_target_valid(target))
		return -EINVAL;

	scanner->targets &= ~BIT(target);

	return 0;
}

/*
 * Check whether a scan target is enabled.
 */
bool phantom_scanner_is_target_enabled(
	struct phantom_scanner *scanner,
	enum phantom_scanner_target target)
{
	if (!scanner)
		return false;

	if (!phantom_scanner_target_valid(target))
		return false;

	return test_bit(target, &scanner->targets);
}

/*
 * Perform scanner-side inspection of an existing event.
 *
 * This is intentionally conservative. The scanner does not invent
 * evidence; it evaluates the metadata already attached to the event.
 *
 * Concrete scanners will later inspect actual kernel objects and
 * create properly populated events.
 */
enum phantom_scan_result
phantom_scanner_scan_event(struct phantom_scanner *scanner,
			   struct phantom_threat_event *event)
{
	if (!scanner || !event)
		return PHANTOM_SCAN_ERROR;

	if (!scanner->enabled)
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

/*
 * Submit an event to the scanner.
 */
int phantom_scanner_submit_event(struct phantom_scanner *scanner,
				 struct phantom_threat_event *event)
{
	enum phantom_scan_result result;

	if (!scanner || !event)
		return -EINVAL;

	if (!scanner->enabled ||
	    scanner->state != PHANTOM_SCANNER_IDLE)
		return -EAGAIN;

	/*
	 * Do not scan an event originating from an unknown source.
	 */
	if (event->source == PHANTOM_SOURCE_UNKNOWN) {
		scanner->scan_errors++;
		return -EINVAL;
	}

	/*
	 * The corresponding scanner target must be enabled.
	 */
	switch (event->source) {
	case PHANTOM_SOURCE_PROCESS:
		if (!phantom_scanner_is_target_enabled(
				scanner, PHANTOM_SCAN_TARGET_PROCESS))
			return -EACCES;
		break;

	case PHANTOM_SOURCE_MEMORY:
		if (!phantom_scanner_is_target_enabled(
				scanner, PHANTOM_SCAN_TARGET_MEMORY))
			return -EACCES;
		break;

	case PHANTOM_SOURCE_FILE:
		if (!phantom_scanner_is_target_enabled(
				scanner, PHANTOM_SCAN_TARGET_FILE))
			return -EACCES;
		break;

	case PHANTOM_SOURCE_NETWORK:
		if (!phantom_scanner_is_target_enabled(
				scanner, PHANTOM_SCAN_TARGET_NETWORK))
			return -EACCES;
		break;

	case PHANTOM_SOURCE_MODULE:
		if (!phantom_scanner_is_target_enabled(
				scanner, PHANTOM_SCAN_TARGET_MODULE))
			return -EACCES;
		break;

	case PHANTOM_SOURCE_INTEGRITY:
		if (!phantom_scanner_is_target_enabled(
				scanner, PHANTOM_SCAN_TARGET_INTEGRITY))
			return -EACCES;
		break;

	case PHANTOM_SOURCE_SYSCALL:
		/*
		 * Syscall observations are detector-driven. They are not
		 * an active scanner target yet.
		 */
		return -EOPNOTSUPP;

	default:
		scanner->scan_errors++;
		return -EINVAL;
	}

	scanner->state = PHANTOM_SCANNER_RUNNING;
	scanner->scans_started++;

	result = phantom_scanner_scan_event(scanner, event);

	switch (result) {
	case PHANTOM_SCAN_CLEAN:
	case PHANTOM_SCAN_SUSPICIOUS:
	case PHANTOM_SCAN_THREAT:
	case PHANTOM_SCAN_CRITICAL:
		scanner->scans_completed++;
		break;

	case PHANTOM_SCAN_NONE:
	default:
		scanner->scan_errors++;
		scanner->state = PHANTOM_SCANNER_IDLE;
		return -EIO;
	}

	if (result == PHANTOM_SCAN_SUSPICIOUS ||
	    result == PHANTOM_SCAN_THREAT ||
	    result == PHANTOM_SCAN_CRITICAL) {
		scanner->threats_found++;

		event->flags |= PHANTOM_EVENT_FLAG_SCAN;

		if (result == PHANTOM_SCAN_CRITICAL)
			event->flags |= PHANTOM_EVENT_FLAG_CRITICAL;

		if (result == PHANTOM_SCAN_SUSPICIOUS &&
		    event->severity < PHANTOM_SEVERITY_MEDIUM)
			event->severity = PHANTOM_SEVERITY_MEDIUM;

		if (result == PHANTOM_SCAN_THREAT &&
		    event->severity < PHANTOM_SEVERITY_HIGH)
			event->severity = PHANTOM_SEVERITY_HIGH;

		if (result == PHANTOM_SCAN_CRITICAL &&
		    event->severity < PHANTOM_SEVERITY_CRITICAL)
			event->severity = PHANTOM_SEVERITY_CRITICAL;

		event->result = PHANTOM_RESULT_DETECTED;
	}

	scanner->state = PHANTOM_SCANNER_IDLE;

	return 0;
}

/*
 * Reset scanner statistics.
 */
void phantom_scanner_reset_stats(struct phantom_scanner *scanner)
{
	if (!scanner)
		return;

	scanner->scans_started = 0;
	scanner->scans_completed = 0;
	scanner->threats_found = 0;
	scanner->scan_errors = 0;
}
