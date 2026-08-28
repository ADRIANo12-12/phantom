/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Phantom OS
 *
 * Kernel threat detection subsystem entry point.
 *
 * Copyright (C) 2026 Adrian Sikora
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/printk.h>

#include "detector.h"
#include "event.h"
#include "informator.h"
#include "neutralizer.h"
#include "policy.h"
#include "scanner.h"
#include "secure.h"

/*
 * Global Phantom threat subsystem context.
 *
 * This structure belongs only to the top-level coordinator.
 * Individual components own their internal state.
 */
struct phantom_threat_context {
	struct phantom_detector detector;
	struct phantom_scanner scanner;
	struct phantom_policy policy;
	struct phantom_neutralizer neutralizer;
	struct phantom_secure secure;
};

/*
 * Single subsystem context.
 *
 * The threat subsystem is initialized once during kernel boot.
 */
static struct phantom_threat_context phantom_threat;

/**
 * phantom_threat_init_components() - initialize all threat components.
 *
 * Initialization order is intentional:
 *
 *   security
 *       ↓
 *   event/detection infrastructure
 *       ↓
 *   policy
 *       ↓
 *   scanner
 *       ↓
 *   neutralizer
 *
 * Active enforcement is not enabled automatically.
 */
static int phantom_threat_init_components(void)
{
	int ret;

	ret = phantom_secure_init(&phantom_threat.secure);
	if (ret)
		return ret;

	ret = phantom_secure_check_integrity(&phantom_threat.secure);
	if (ret)
		return ret;

	ret = phantom_detector_init(&phantom_threat.detector);
	if (ret)
		return ret;

	ret = phantom_policy_init(&phantom_threat.policy);
	if (ret)
		return ret;

	ret = phantom_scanner_init(&phantom_threat.scanner);
	if (ret)
		return ret;

	ret = phantom_neutralizer_init(&phantom_threat.neutralizer);
	if (ret)
		return ret;

	return 0;
}

/**
 * phantom_threat_start_components() - start safe detection pipeline.
 *
 * Detection starts before active neutralization. The default policy
 * is detect-only, so no automatic destructive action is enabled here.
 */
static int phantom_threat_start_components(void)
{
	int ret;

	/*
	 * Start detector.
	 */
	ret = phantom_detector_start(&phantom_threat.detector);
	if (ret)
		return ret;

	/*
	 * Enable the currently supported scanner targets.
	 *
	 * Concrete scanning backends will later decide which targets
	 * are actually implemented.
	 */
	ret = phantom_scanner_enable_target(
		&phantom_threat.scanner,
		PHANTOM_SCAN_TARGET_PROCESS);
	if (ret)
		goto stop_detector;

	ret = phantom_scanner_enable_target(
		&phantom_threat.scanner,
		PHANTOM_SCAN_TARGET_MEMORY);
	if (ret)
		goto stop_detector;

	ret = phantom_scanner_enable_target(
		&phantom_threat.scanner,
		PHANTOM_SCAN_TARGET_MODULE);
	if (ret)
		goto stop_detector;

	ret = phantom_scanner_enable_target(
		&phantom_threat.scanner,
		PHANTOM_SCAN_TARGET_FILE);
	if (ret)
		goto stop_detector;

	ret = phantom_scanner_enable_target(
		&phantom_threat.scanner,
		PHANTOM_SCAN_TARGET_NETWORK);
	if (ret)
		goto stop_detector;

	ret = phantom_scanner_enable_target(
		&phantom_threat.scanner,
		PHANTOM_SCAN_TARGET_INTEGRITY);
	if (ret)
		goto stop_detector;

	/*
	 * Start scanner.
	 */
	ret = phantom_scanner_start(&phantom_threat.scanner);
	if (ret)
		goto stop_detector;

	/*
	 * Neutralizer remains disabled while the default policy is
	 * detect-only.
	 */
	return 0;

stop_detector:
	phantom_detector_stop(&phantom_threat.detector);
	return ret;
}

/**
 * phantom_threat_init() - initialize Phantom threat subsystem.
 *
 * This is the subsystem's boot-time entry point.
 *
 * Return: 0 on success or a negative errno value on failure.
 */
static int __init phantom_threat_init(void)
{
	int ret;

	ret = phantom_threat_init_components();
	if (ret) {
		pr_err("[PHANTOM THREAT DETECTION] "
		       "component initialization failed: %d\n",
		       ret);
		return ret;
	}

	ret = phantom_threat_start_components();
	if (ret) {
		pr_err("[PHANTOM THREAT DETECTION] "
		       "component startup failed: %d\n",
		       ret);
		return ret;
	}

	ph_send_info_threat(
		"Phantom Threat Detection subsystem initialized");

	ph_send_info_threat(
		"Detection engine: active");

	ph_send_info_threat(
		"Policy mode: detect-only");

	ph_send_info_threat(
		"Automatic neutralization: disabled");

	return 0;
}

subsys_initcall(phantom_threat_init);
