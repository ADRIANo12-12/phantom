/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Process threat detector implementation.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "process.h"

#include <linux/cred.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/sched.h>
#include <linux/string.h>

/*
 * Validate process detector state.
 */
static bool phantom_process_detector_state_valid(u8 state)
{
	return state <= PHANTOM_PROCESS_DETECTOR_ERROR;
}

/*
 * Initialize process detector.
 */
int phantom_process_detector_init(
	struct phantom_process_detector *detector)
{
	if (!detector)
		return -EINVAL;

	detector->state = PHANTOM_PROCESS_DETECTOR_INITIALIZING;
	detector->enabled = false;

	detector->observations = 0;
	detector->suspicious = 0;
	detector->threats = 0;
	detector->errors = 0;

	detector->state = PHANTOM_PROCESS_DETECTOR_DISABLED;

	return 0;
}

/*
 * Start process detector.
 */
int phantom_process_detector_start(
	struct phantom_process_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (!phantom_process_detector_state_valid(detector->state))
		return -EINVAL;

	if (detector->state != PHANTOM_PROCESS_DETECTOR_DISABLED)
		return -EBUSY;

	detector->enabled = true;
	detector->state = PHANTOM_PROCESS_DETECTOR_RUNNING;

	return 0;
}

/*
 * Stop process detector.
 */
int phantom_process_detector_stop(
	struct phantom_process_detector *detector)
{
	if (!detector)
		return -EINVAL;

	if (detector->state != PHANTOM_PROCESS_DETECTOR_RUNNING)
		return -EINVAL;

	detector->enabled = false;
	detector->state = PHANTOM_PROCESS_DETECTOR_STOPPING;

	detector->state = PHANTOM_PROCESS_DETECTOR_DISABLED;

	return 0;
}

/*
 * Collect process metadata.
 *
 * This function observes the task only. It does not modify the task,
 * send signals, terminate it or otherwise interfere with execution.
 */
int phantom_process_detector_inspect(
	struct phantom_process_detector *detector,
	struct task_struct *task,
	struct phantom_process_observation *observation)
{
	const struct cred *cred;

	if (!detector || !task || !observation)
		return -EINVAL;

	if (!detector->enabled ||
	    detector->state != PHANTOM_PROCESS_DETECTOR_RUNNING)
		return -EAGAIN;

	memset(observation, 0, sizeof(*observation));

	observation->pid = task_pid_nr(task);
	observation->tgid = task_tgid_nr(task);

	/*
	 * Credentials are RCU protected.
	 */
	rcu_read_lock();

	cred = __task_cred(task);

	observation->uid = cred->uid;
	observation->euid = cred->euid;
	observation->gid = cred->gid;
	observation->egid = cred->egid;

	if (uid_eq(cred->uid, GLOBAL_ROOT_UID))
		observation->flags |= PHANTOM_PROCESS_OBS_ROOT;

	if (!uid_eq(cred->uid, cred->euid))
		observation->flags |= PHANTOM_PROCESS_OBS_SETUID;

	if (!gid_eq(cred->gid, cred->egid))
		observation->flags |= PHANTOM_PROCESS_OBS_SETGID;

	rcu_read_unlock();

	/*
	 * PF_KTHREAD identifies kernel threads.
	 */
	if (task->flags & PF_KTHREAD)
		observation->flags |= PHANTOM_PROCESS_OBS_KERNEL_THREAD;

	get_task_comm(observation->comm, task);

	detector->observations++;

	return 0;
}

/*
 * Classify process observation.
 *
 * Important: root, setuid/setgid and kernel-thread status are
 * observations, not proof of malware. They therefore do not
 * automatically become threats.
 */
enum phantom_detector_result
phantom_process_detector_classify(
	struct phantom_process_detector *detector,
	const struct phantom_process_observation *observation)
{
	if (!detector || !observation)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	if (!detector->enabled ||
	    detector->state != PHANTOM_PROCESS_DETECTOR_RUNNING)
		return PHANTOM_DETECTOR_RESULT_ERROR;

	/*
	 * A higher-level detector or future process heuristic can mark
	 * an observation as suspicious. Until then we avoid treating
	 * legitimate privileged processes as malware.
	 */
	if (observation->flags & PHANTOM_PROCESS_OBS_CRITICAL)
		return PHANTOM_DETECTOR_RESULT_CRITICAL;

	if (observation->flags & PHANTOM_PROCESS_OBS_SUSPICIOUS)
		return PHANTOM_DETECTOR_RESULT_SUSPICIOUS;

	return PHANTOM_DETECTOR_RESULT_CLEAN;
}

/*
 * Create a Phantom threat event from a process observation.
 */
struct phantom_threat_event *
phantom_process_detector_create_event(
	struct phantom_process_detector *detector,
	const struct phantom_process_observation *observation)
{
	struct phantom_threat_event *event;
	enum phantom_detector_result result;
	u8 severity;
	int ret;

	if (!detector || !observation)
		return NULL;

	if (!detector->enabled ||
	    detector->state != PHANTOM_PROCESS_DETECTOR_RUNNING)
		return NULL;

	result = phantom_process_detector_classify(detector, observation);

	switch (result) {
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
		severity = PHANTOM_SEVERITY_INFO;
		break;

	case PHANTOM_DETECTOR_RESULT_ERROR:
	default:
		detector->errors++;
		return NULL;
	}

	event = phantom_threat_event_alloc(GFP_KERNEL);
	if (!event) {
		detector->errors++;
		return NULL;
	}

	ret = phantom_threat_event_init(event,
					0,
					severity,
					PHANTOM_SOURCE_PROCESS);
	if (ret) {
		phantom_threat_event_put(event);
		detector->errors++;
		return NULL;
	}

	event->pid = observation->pid;
	event->tgid = observation->tgid;

	/*
	 * Preserve the fact that the event came from kernel-side
	 * observation.
	 */
	event->flags |= PHANTOM_EVENT_FLAG_KERNEL;

	/*
	 * Preserve critical classification.
	 */
	if (severity == PHANTOM_SEVERITY_CRITICAL)
		event->flags |= PHANTOM_EVENT_FLAG_CRITICAL;

	strscpy(event->comm,
		observation->comm,
		sizeof(event->comm));

	/*
	 * The initial event name identifies the detector source.
	 * A higher-level rule engine can replace it with a concrete
	 * threat name later.
	 */
	strscpy(event->name,
		"process-observation",
		sizeof(event->name));

	if (observation->flags & PHANTOM_PROCESS_OBS_ROOT)
		strscpy(event->description,
			"Process has root credentials",
			sizeof(event->description));
	else if (observation->flags & PHANTOM_PROCESS_OBS_SETUID)
		strscpy(event->description,
			"Process has different real and effective UID",
			sizeof(event->description));
	else if (observation->flags & PHANTOM_PROCESS_OBS_SETGID)
		strscpy(event->description,
			"Process has different real and effective GID",
			sizeof(event->description));

	if (result == PHANTOM_DETECTOR_RESULT_SUSPICIOUS)
		detector->suspicious++;

	if (result == PHANTOM_DETECTOR_RESULT_THREAT ||
	    result == PHANTOM_DETECTOR_RESULT_CRITICAL)
		detector->threats++;

	return event;
}

/*
 * Reset process detector statistics.
 */
void phantom_process_detector_reset_stats(
	struct phantom_process_detector *detector)
{
	if (!detector)
		return;

	detector->observations = 0;
	detector->suspicious = 0;
	detector->threats = 0;
	detector->errors = 0;
}
