/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#ifndef PHANTOM_INFORMATOR_THREAT_H
#define PHANTOM_INFORMATOR_THREAT_H

#include <linux/notifier.h>
#include <linux/types.h>
#include <linux/workqueue.h>

/*
 * Maximum length of a threat name.
 */
#define PHANTOM_THREAT_NAME_SIZE 512

/*
 * Maximum length of a formatted informator message.
 */
#define PHANTOM_INFO_BUFF_SIZE 4096

/*
 * Threat categories.
 */
enum phantom_threat_type {
	PHANTOM_THREAT_UNKNOWN = 0,
	PHANTOM_THREAT_SYSCALL,
	PHANTOM_THREAT_MEMORY,
	PHANTOM_THREAT_PROCESS,
	PHANTOM_THREAT_FILE,
	PHANTOM_THREAT_NETWORK,
};

/*
 * Information attached to one threat event.
 */
struct prepare_threat_info {
	u16 threat_id;
	u64 log_id;
	u64 timestamp;
	char name[PHANTOM_THREAT_NAME_SIZE];
};

/*
 * Main informator API.
 */
void ph_send_info_threat(const char *fmt, ...);
void ph_send_debug_threat(const char *fmt, ...);
void ph_send_err_threat(const char *fmt, ...);
void ph_send_detection(const char *fmt, ...);
void ph_send_neutralization(const char *fmt, ...);

void ph_send_scan_succ(const char *fmt, ...);
void ph_send_scan_warn(const char *fmt, ...);
void ph_send_scan_err(const char *fmt, ...);
void ph_send_scan_det(const char *fmt, ...);

void ph_send_fatal(const char *fmt, ...);
void ph_send_type(const char *fmt, ...);
void ph_send_before_panic_dump(const char *fmt, ...);

/*
 * Workqueue callbacks.
 */
void ph_send_info_threat_work(struct work_struct *work);
void ph_send_debug_threat_work(struct work_struct *work);
void ph_send_err_threat_work(struct work_struct *work);
void ph_send_detection_work(struct work_struct *work);
void ph_send_neutralization_work(struct work_struct *work);

void ph_send_scan_succ_work(struct work_struct *work);
void ph_send_scan_warn_work(struct work_struct *work);
void ph_send_scan_err_work(struct work_struct *work);
void ph_send_scan_det_work(struct work_struct *work);

void ph_send_fatal_work(struct work_struct *work);
void ph_send_type_work(struct work_struct *work);
void ph_send_before_panic_dump_work(struct work_struct *work);

#endif /* PHANTOM_INFORMATOR_THREAT_H */
