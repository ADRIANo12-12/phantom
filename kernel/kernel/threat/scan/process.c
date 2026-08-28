/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Process scanner implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "process.h"

#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>

/*
 * Validate process scanner state.
 */
static bool phantom_process_scanner_state_valid(u8 state)
{
	return state <= PHANTOM_PROCESS_SCANNER_ERROR;
}

/*
 * Initialize process scanner.
 */
int phantom_process_scanner_init(
	struct phantom_process_scanner *scanner)
{
	if (!scanner)
		return -EINVAL;

	scanner->state = PHANTOM_PROCESS_SCANNER_INITIALIZING;
	scanner->enabled = false;

	scanner->scans = 0;
	scanner->suspicious = 0;
	scanner->threats = 0;
	scanner->errors = 0;

	scanner->state = PHANTOM_PROCESS_SCANNER_DISABLED;

	return 0;
}

/*
 * Start process scanner.
 */
int phantom_process_scanner_start(
	struct phantom_process_scanner *scanner)
{
	if (!scanner)
		return -EINVAL;

	if (!phantom_process_scanner_state_valid(scanner->state))
		return -EINVAL;

	if (scanner->state != PHANTOM_PROCESS_SCANNER_DISABLED)
		return -EBUSY;

	scanner->enabled = true;
	scanner->state = PHANTOM_PROCESS_SCANNER_RUNNING;

	return 0;
}

/*
 * Stop process scanner.
 */
int phantom_process_scanner_stop(
	struct phantom_process_scanner *scanner)
{
	if (!scanner)
		return -EINVAL;

	if (scanner->state != PHANTOM_PROCESS_SCANNER_RUNNING)
		return -EINVAL;

	scanner->enabled = false;
	scanner->state = PHANTOM_PROCESS_SCANNER_STOPPING;
	scanner->state = PHANTOM_PROCESS_SCANNER_DISABLED;

	return 0;
}

/*
 * Scan a single task.
 *
 * This routine performs read-only inspection. It does not terminate,
 * signal, freeze or otherwise modify the task.
 */
int phantom_process_scanner_scan_task(
	struct phantom_process_scanner *scanner,
	struct task_struct *task,
	struct phantom_process_scan *result)
{
	const struct cred *cred;

	if (!scanner || !task || !result)
		return -EINVAL;

	if (!scanner->enabled ||
	    scanner->state != PHANTOM_PROCESS_SCANNER_RUNNING)
		return -EAGAIN;

	memset(result, 0, sizeof(*result));

	result->pid = task_pid_nr(task);
	result->tgid = task_tgid_nr(task);

	rcu_read_lock();

	cred = __task_cred(task);

	result->uid = cred->uid;
	result->euid = cred->euid;
	result->gid = cred->gid;
	result->egid = cred->egid;

	if (uid_eq(cred->uid, GLOBAL_ROOT_UID))
		result->flags |= PHANTOM_PROCESS_SCAN_ROOT;

	if (!uid_eq(cred->uid, cred->euid))
		result->flags |= PHANTOM_PROCESS_SCAN_SETUID;

	if (!gid_eq(cred->gid, cred->egid))
		result->flags |= PHANTOM_PROCESS_SCAN_SETGID;

	rcu_read_unlock();

	if (task->flags & PF_KTHREAD)
		result->flags |= PHANTOM_PROCESS_SCAN_KERNEL_THREAD;

	get_task_comm(result->comm, task);

	scanner->scans++;

	return 0;
}

/*
 * Classify one process scan.
 *
 * Privileged processes are not automatically considered malicious.
 * Concrete behavioural rules will add SUSPICIOUS or CRITICAL evidence.
 */
enum phantom_detector_result
phantom_process_scanner_classify(
	struct phantom_process_scanner *scanner,
	const struct phantom_process_scan *result)
{
	if (!scanner || !result)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	if (!scanner->enabled ||
	    scanner->state != PHANTOM_PROCESS_SCANNER_RUNNING)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	if (result->flags & PHANTOM_PROCESS_SCAN_CRITICAL)
		return PHANTOM_DETECTOR_RESULT_CRITICAL;

	if (result->flags & PHANTOM_PROCESS_SCAN_SUSPICIOUS)
		return PHANTOM_DETECTOR_RESULT_SUSPICIOUS;

	return PHANTOM_DETECTOR_RESULT_CLEAN;
}

/*
 * Create a threat event from a process scan.
 */
struct phantom_threat_event *
phantom_process_scanner_create_event(
	struct phantom_process_scanner *scanner,
	const struct phantom_process_scan *result)
{
	struct phantom_threat_event *event;
	enum phantom_detector_result detection;
	u8 severity;
	int ret;

	if (!scanner || !result)
		return NULL;

	detection = phantom_process_scanner_classify(scanner, result);

	if (detection == PHANTOM_DETECTOR_RESULT_ERROR) {
		scanner->errors++;
		return NULL;
	}

	switch (detection) {
	case PHANTOM_DETECTOR_RESULT_CRITICAL:
		severity = PHANTOM_SEVERITY_CRITICAL;
		break;

	case PHANTOM_DETECTOR_RESULT_THREAT:
		severity = PHANTOM_SEVERITY_HIGH;
		break;

	case PHANTOM_DETECTOR_RESULT_SUSPICIOUS:
		severity = PHANTOM_SEVERITY_MEDIUM;
		break;

	case PHANTOM_DETECTOR_RESULT_CLEAN:
	case PHANTOM_DETECTOR_RESULT_NONE:
	default:
		severity = PHANTOM_SEVERITY_INFO;
		break;
	}

	event = phantom_threat_event_alloc(GFP_KERNEL);
	if (!event) {
		scanner->errors++;
		return NULL;
	}

	ret = phantom_threat_event_init(
		event,
		0,
		severity,
		PHANTOM_SOURCE_PROCESS);

	if (ret) {
		phantom_threat_event_put(event);
		scanner->errors++;
		return NULL;
	}

	event->pid = result->pid;
	event->tgid = result->tgid;
	event->flags |= PHANTOM_EVENT_FLAG_KERNEL;

	if (detection == PHANTOM_DETECTOR_RESULT_CRITICAL)
		event->flags |= PHANTOM_EVENT_FLAG_CRITICAL;

	strscpy(event->comm,
		result->comm,
		sizeof(event->comm));

	strscpy(event->name,
		"process-scan",
		sizeof(event->name));

	if (result->flags & PHANTOM_PROCESS_SCAN_ROOT)
		strscpy(event->description,
			"Process uses root credentials",
			sizeof(event->description));
	else if (result->flags & PHANTOM_PROCESS_SCAN_SETUID)
		strscpy(event->description,
			"Process has differing real and effective UID",
			sizeof(event->description));
	else if (result->flags & PHANTOM_PROCESS_SCAN_SETGID)
		strscpy(event->description,
			"Process has differing real and effective GID",
			sizeof(event->description));
	else if (result->flags & PHANTOM_PROCESS_SCAN_KERNEL_THREAD)
		strscpy(event->description,
			"Kernel thread observed",
			sizeof(event->description));
	else
		strscpy(event->description,
			"Process scan completed",
			sizeof(event->description));

	if (detection == PHANTOM_DETECTOR_RESULT_SUSPICIOUS)
		scanner->suspicious++;

	if (detection == PHANTOM_DETECTOR_RESULT_THREAT ||
	    detection == PHANTOM_DETECTOR_RESULT_CRITICAL)
		scanner->threats++;

	return event;
}

/*
 * Reset process scanner statistics.
 */
void phantom_process_scanner_reset_stats(
	struct phantom_process_scanner *scanner)
{
	if (!scanner)
		return;

	scanner->scans = 0;
	scanner->suspicious = 0;
	scanner->threats = 0;
	scanner->errors = 0;
}
