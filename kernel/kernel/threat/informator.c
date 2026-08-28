/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include "informator.h"

#include <linux/atomic.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/notifier.h>
#include <linux/panic_notifier.h>
#include <linux/printk.h>
#include <linux/time64.h>
#include <linux/timekeeping.h>
#include <linux/workqueue.h>

/*
 * Global monotonically increasing log ID.
 *
 * atomic64 is used because prepare_threat_info::log_id is u64.
 */
static atomic64_t ph_log_id = ATOMIC64_INIT(0);

/*
 * Main informator work item.
 */
static struct work_struct ph_info_work;

/*
 * Generate a new unique log ID.
 */
static u64 ph_next_log_id(void)
{
	return atomic64_inc_return(&ph_log_id);
}

/*
 * Return current UTC time.
 *
 * The fast realtime clock is suitable for restricted contexts such
 * as panic reporting.
 */
static void ph_get_utc_time(struct tm *tm)
{
	u64 ns;
	time64_t seconds;

	ns = ktime_get_real_fast_ns();
	seconds = div_u64(ns, NSEC_PER_SEC);

	time64_to_tm(seconds, 0, tm);
}

/*
 * Common formatter/output routine.
 *
 * struct va_format lets printk consume the variadic argument list
 * without creating a large temporary buffer on the kernel stack.
 */
static void ph_log_info(const char *level,
			const char *fmt,
			va_list *args)
{
	struct tm tm;
	struct va_format vaf;
	u64 log_id;

	log_id = ph_next_log_id();
	ph_get_utc_time(&tm);

	vaf.fmt = fmt;
	vaf.va = args;

	pr_info("[PHANTOM THREAT DETECTION] "
		"[%s] "
		"[LOG:%llu] "
		"[%02d:%02d:%02d UTC] "
		"%pV\n",
		level,
		(unsigned long long)log_id,
		tm.tm_hour,
		tm.tm_min,
		tm.tm_sec,
		&vaf);
}

/*
 * INFO
 */
void ph_send_info_threat(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	ph_log_info("INFO", fmt, &args);
	va_end(args);
}

/*
 * DEBUG
 */
void ph_send_debug_threat(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	ph_log_info("DEBUG", fmt, &args);
	va_end(args);
}

/*
 * ERROR
 */
void ph_send_err_threat(const char *fmt, ...)
{
	struct tm tm;
	struct va_format vaf;
	va_list args;
	u64 log_id;

	log_id = ph_next_log_id();
	ph_get_utc_time(&tm);

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_err("[PHANTOM THREAT DETECTION] "
	       "[ERROR] "
	       "[LOG:%llu] "
	       "[%02d:%02d:%02d UTC] "
	       "%pV\n",
	       (unsigned long long)log_id,
	       tm.tm_hour,
	       tm.tm_min,
	       tm.tm_sec,
	       &vaf);

	va_end(args);
}

/*
 * DETECTION
 */
void ph_send_detection(const char *fmt, ...)
{
	struct tm tm;
	struct va_format vaf;
	va_list args;
	u64 log_id;

	log_id = ph_next_log_id();
	ph_get_utc_time(&tm);

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_alert("[PHANTOM THREAT DETECTION] "
		 "[DETECTION] "
		 "[LOG:%llu] "
		 "[%02d:%02d:%02d UTC] "
		 "%pV\n",
		 (unsigned long long)log_id,
		 tm.tm_hour,
		 tm.tm_min,
		 tm.tm_sec,
		 &vaf);

	va_end(args);
}

/*
 * NEUTRALIZATION
 */
void ph_send_neutralization(const char *fmt, ...)
{
	struct tm tm;
	struct va_format vaf;
	va_list args;
	u64 log_id;

	log_id = ph_next_log_id();
	ph_get_utc_time(&tm);

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_notice("[PHANTOM THREAT DETECTION] "
		  "[NEUTRALIZATION] "
		  "[LOG:%llu] "
		  "[%02d:%02d:%02d UTC] "
		  "%pV\n",
		  (unsigned long long)log_id,
		  tm.tm_hour,
		  tm.tm_min,
		  tm.tm_sec,
		  &vaf);

	va_end(args);
}

/*
 * SCAN SUCCESS
 */
void ph_send_scan_succ(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	ph_log_info("SCAN-SUCCESS", fmt, &args);
	va_end(args);
}

/*
 * SCAN WARNING
 */
void ph_send_scan_warn(const char *fmt, ...)
{
	struct tm tm;
	struct va_format vaf;
	va_list args;
	u64 log_id;

	log_id = ph_next_log_id();
	ph_get_utc_time(&tm);

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_warn("[PHANTOM THREAT DETECTION] "
		"[SCAN-WARN] "
		"[LOG:%llu] "
		"[%02d:%02d:%02d UTC] "
		"%pV\n",
		(unsigned long long)log_id,
		tm.tm_hour,
		tm.tm_min,
		tm.tm_sec,
		&vaf);

	va_end(args);
}

/*
 * SCAN ERROR
 */
void ph_send_scan_err(const char *fmt, ...)
{
	struct tm tm;
	struct va_format vaf;
	va_list args;
	u64 log_id;

	log_id = ph_next_log_id();
	ph_get_utc_time(&tm);

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_err("[PHANTOM THREAT DETECTION] "
	       "[SCAN-ERR] "
	       "[LOG:%llu] "
	       "[%02d:%02d:%02d UTC] "
	       "%pV\n",
	       (unsigned long long)log_id,
	       tm.tm_hour,
	       tm.tm_min,
	       tm.tm_sec,
	       &vaf);

	va_end(args);
}

/*
 * SCAN DETECTION
 */
void ph_send_scan_det(const char *fmt, ...)
{
	struct tm tm;
	struct va_format vaf;
	va_list args;
	u64 log_id;

	log_id = ph_next_log_id();
	ph_get_utc_time(&tm);

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_alert("[PHANTOM THREAT DETECTION] "
		 "[SCAN-DETECTION] "
		 "[LOG:%llu] "
		 "[%02d:%02d:%02d UTC] "
		 "%pV\n",
		 (unsigned long long)log_id,
		 tm.tm_hour,
		 tm.tm_min,
		 tm.tm_sec,
		 &vaf);

	va_end(args);
}

/*
 * FATAL
 */
void ph_send_fatal(const char *fmt, ...)
{
	struct tm tm;
	struct va_format vaf;
	va_list args;
	u64 log_id;

	log_id = ph_next_log_id();
	ph_get_utc_time(&tm);

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_emerg("[PHANTOM THREAT DETECTION] "
		 "[FATAL] "
		 "[LOG:%llu] "
		 "[%02d:%02d:%02d UTC] "
		 "%pV\n",
		 (unsigned long long)log_id,
		 tm.tm_hour,
		 tm.tm_min,
		 tm.tm_sec,
		 &vaf);

	va_end(args);
}

/*
 * THREAT TYPE
 */
void ph_send_type(const char *fmt, ...)
{
	struct tm tm;
	struct va_format vaf;
	va_list args;
	u64 log_id;

	log_id = ph_next_log_id();
	ph_get_utc_time(&tm);

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_alert("[PHANTOM THREAT DETECTION] "
		 "[TYPE] "
		 "[LOG:%llu] "
		 "[%02d:%02d:%02d UTC] "
		 "%pV\n",
		 (unsigned long long)log_id,
		 tm.tm_hour,
		 tm.tm_min,
		 tm.tm_sec,
		 &vaf);

	va_end(args);

	pr_alert("[PHANTOM THREAT DETECTION] DO NOT IGNORE!\n");
}

/*
 * BEFORE PANIC DUMP
 */
void ph_send_before_panic_dump(const char *fmt, ...)
{
	struct tm tm;
	struct va_format vaf;
	va_list args;
	u64 log_id;

	log_id = ph_next_log_id();
	ph_get_utc_time(&tm);

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	pr_emerg("[PHANTOM THREAT DETECTION] "
		 "[PANIC-DUMP] "
		 "[LOG:%llu] "
		 "[%02d:%02d:%02d UTC] "
		 "%pV\n",
		 (unsigned long long)log_id,
		 tm.tm_hour,
		 tm.tm_min,
		 tm.tm_sec,
		 &vaf);

	va_end(args);
}

/*
 * ------------------------------------------------------------
 * WORKQUEUE CALLBACKS
 * ------------------------------------------------------------
 */

void ph_send_info_threat_work(struct work_struct *work)
{
	ph_send_info_threat(
		"Phantom Threat Detection started in kernel mode (Ring 0)");
}

void ph_send_debug_threat_work(struct work_struct *work)
{
	ph_send_debug_threat(
		"Phantom Threat Detection debug worker started");
}

void ph_send_err_threat_work(struct work_struct *work)
{
	ph_send_err_threat(
		"Phantom Threat Detection error worker executed");
}

void ph_send_detection_work(struct work_struct *work)
{
	ph_send_detection(
		"Threat detection worker executed");
}

void ph_send_neutralization_work(struct work_struct *work)
{
	ph_send_neutralization(
		"Threat neutralization worker executed");
}

void ph_send_scan_succ_work(struct work_struct *work)
{
	ph_send_scan_succ(
		"Threat scan completed successfully");
}

void ph_send_scan_warn_work(struct work_struct *work)
{
	ph_send_scan_warn(
		"Threat scan completed with warning");
}

void ph_send_scan_err_work(struct work_struct *work)
{
	ph_send_scan_err(
		"Threat scan failed");
}

void ph_send_scan_det_work(struct work_struct *work)
{
	ph_send_scan_det(
		"Threat detected during scan");
}

void ph_send_fatal_work(struct work_struct *work)
{
	ph_send_fatal(
		"Fatal threat state reported");
}

void ph_send_type_work(struct work_struct *work)
{
	ph_send_type(
		"Unknown threat type");
}

void ph_send_before_panic_dump_work(struct work_struct *work)
{
	ph_send_before_panic_dump(
		"Preparing panic dump");
}

/*
 * ------------------------------------------------------------
 * PANIC NOTIFIER
 * ------------------------------------------------------------
 */

static int ph_panic_notifier(struct notifier_block *nb,
			     unsigned long event,
			     void *data)
{
	const char *panic_message = data;

	/*
	 * Do NOT schedule work here.
	 *
	 * The kernel is already in panic handling. The worker may never
	 * get a chance to execute.
	 */

	pr_emerg("\n");
	pr_emerg("==================================================\n");
	pr_emerg("[PHANTOM THREAT DETECTION] KERNEL PANIC DETECTED\n");
	pr_emerg("==================================================\n");

	if (panic_message)
		pr_emerg("[PHANTOM PANIC] %s\n", panic_message);

	ph_send_before_panic_dump(
		"Phantom informator received kernel panic notification");

	return NOTIFY_OK;
}

static struct notifier_block ph_panic_nb = {
	.notifier_call = ph_panic_notifier,
	.priority = INT_MAX,
};

/*
 * ------------------------------------------------------------
 * INITIALIZATION
 * ------------------------------------------------------------
 */

static int __init ph_threat_init(void)
{
	int ret;

	/*
	 * Register panic notifier.
	 */
	ret = atomic_notifier_chain_register(&panic_notifier_list,
					     &ph_panic_nb);

	if (ret) {
		pr_err("[PHANTOM THREAT DETECTION] "
		       "failed to register panic notifier: %d\n",
		       ret);
		return ret;
	}

	/*
	 * Initialize workqueue item.
	 */
	INIT_WORK(&ph_info_work, ph_send_info_threat_work);

	/*
	 * Queue initial informator message.
	 */
	schedule_work(&ph_info_work);

	pr_info("[PHANTOM THREAT DETECTION] informator initialized\n");

	return 0;
}

subsys_initcall(ph_threat_init);
