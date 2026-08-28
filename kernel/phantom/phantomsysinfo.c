// SPDX-License-Identifier: GPL-2.0

/*
 *
 *                  Phantom OS
 *
 * Written by Adrian Sikora, Project Lead, Maintainer, Founder.
 * Aug 2026.
 */

#include <linux/init.h>
#include <linux/printk.h>
#include <linux/workqueue.h>
#include <linux/timekeeping.h>
#include <linux/utsname.h>

#define WRITE_BUF 1024

bool kern_env_is_set_up;

static struct work_struct pwork;
static struct timespec64 l_time;

static void sys_info(struct work_struct *work)
{
	ktime_get_real_ts64(&l_time);

	pr_info("Phantom OS -\n");
	pr_info("Kernel: %s\n", init_uts_ns.name.release);
	pr_info("\t%s\n", init_uts_ns.name.version);

	pr_info("Phantom - \n");

	pr_notice_once(
		"Phantom Version: 0.0.1-alpha build unrealeased! "
		"(Insider version) Unauthrized access will be punished!\n");

	pr_alert(
		"Setting up custom Phantom OS kernel enviroment...\n");
}

static int __init psysinfo_init(void)
{
	INIT_WORK(&pwork, sys_info);

	ktime_get_real_ts64(&l_time);

	pr_info_once(
		"Phantom info: Phantom Information System loaded [%lld]\n",
		(long long)l_time.tv_sec);

	schedule_work(&pwork);

	kern_env_is_set_up = true;

	return 0;
}

subsys_initcall(psysinfo_init);