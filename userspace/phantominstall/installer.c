// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/mount.h>
#include <fcntl.h>

#include "installer.h"
#include "system.h"
#include "network.h"
#include "storage.h"
#include "package.h"

#define TARGET_ROOT "/mnt/phantom-target"
#define TARGET_LOG  TARGET_ROOT "/var/log/phantom-install.log"

static void report(phantom_progress_fn cb, void *user,
		   uint32_t percent, const char *msg)
{
	printf("  [%3u%%] %s\n", percent, msg);
	fflush(stdout);
	if (cb)
		cb(percent, msg, user);
}

static int ensure_dir(const char *path, mode_t mode)
{
	char tmp[256];
	char *p;
	size_t len;

	if (!path || !*path)
		return -1;

	snprintf(tmp, sizeof(tmp), "%s", path);
	len = strlen(tmp);
	if (len == 0)
		return -1;

	for (p = tmp + 1; *p; p++) {
		if (*p != '/')
			continue;
		*p = '\0';
		if (mkdir(tmp, mode) < 0 && errno != EEXIST)
			return -1;
		*p = '/';
	}

	if (mkdir(tmp, mode) < 0 && errno != EEXIST)
		return -1;

	return 0;
}

static int write_file(const char *path, const char *content)
{
	FILE *f;

	f = fopen(path, "w");
	if (!f)
		return -1;

	fputs(content, f);
	fclose(f);
	return 0;
}

static int copy_file(const char *src, const char *dst)
{
	char buf[4096];
	FILE *in, *out;
	size_t n;

	in = fopen(src, "rb");
	if (!in)
		return -1;

	out = fopen(dst, "wb");
	if (!out) {
		fclose(in);
		return -1;
	}

	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			fclose(in);
			fclose(out);
			return -1;
		}
	}

	fclose(in);
	fclose(out);
	return 0;
}

static int pick_first_disk(char *out, size_t out_len)
{
	DIR *dir;
	struct dirent *entry;
	const char *name;

	dir = opendir("/sys/block");
	if (!dir)
		return -1;

	while ((entry = readdir(dir)) != NULL) {
		name = entry->d_name;
		if (name[0] == '.')
			continue;
		if (!strncmp(name, "loop", 4) ||
		    !strncmp(name, "ram", 3) ||
		    !strncmp(name, "fd", 2))
			continue;

		snprintf(out, out_len, "/dev/%s", name);
		closedir(dir);
		return 0;
	}

	closedir(dir);
	return -1;
}

int phantom_installer_do_system_check_with_progress(
	phantom_progress_fn cb, void *user)
{
	struct utsname info;

	report(cb, user, 10, "Reading kernel info...");
	if (uname(&info) == 0) {
		printf("     OS    : %s\n", info.sysname);
		printf("     Kernel: %s\n", info.release);
		printf("     Arch  : %s\n", info.machine);
		printf("     Host  : %s\n", info.nodename);
	}

	report(cb, user, 40, "Scanning block devices...");
	phantom_installer_storage_list();

	report(cb, user, 70, "Checking package backend...");
	phantom_installer_package_check();

	report(cb, user, 100, "System check done.");
	return 0;
}

int phantom_installer_do_network_setup_with_progress(
	phantom_progress_fn cb, void *user)
{
	int net_ok, net_test;

	report(cb, user, 20, "Starting network...");
	net_ok = phantom_installer_network_start();

	report(cb, user, 60, "Testing internet...");
	if (net_ok == 0)
		net_test = phantom_installer_network_test();
	else
		net_test = -1;

	report(cb, user, 100,
	       (net_ok == 0 && net_test == 0)
		       ? "Network ready."
		       : "Network incomplete.");

	return (net_ok == 0 && net_test == 0) ? 0 : -1;
}

int phantom_installer_do_disk_setup_with_progress(
	phantom_progress_fn cb, void *user)
{
	char disk[64];

	report(cb, user, 30, "Listing disks...");
	phantom_installer_storage_list();

	report(cb, user, 70, "Selecting target...");
	if (pick_first_disk(disk, sizeof(disk)) == 0)
		printf("     Suggested target: %s\n", disk);
	else
		printf("     No disk found — install will use directory target.\n");

	report(cb, user, 100, "Disk setup done.");
	return 0;
}

int phantom_installer_do_install_with_progress(
	phantom_progress_fn cb, void *user)
{
	char disk[64];
	char path[256];
	struct utsname info;
	int has_disk;
	FILE *log;

	report(cb, user, 0, "Starting Phantom OS install...");

	/* 1. Target root */
	report(cb, user, 5, "Creating target directories...");
	if (ensure_dir(TARGET_ROOT, 0755) < 0) {
		report(cb, user, 100, "FAILED: cannot create target root");
		return -1;
	}

	ensure_dir(TARGET_ROOT "/bin", 0755);
	ensure_dir(TARGET_ROOT "/sbin", 0755);
	ensure_dir(TARGET_ROOT "/etc", 0755);
	ensure_dir(TARGET_ROOT "/var/log", 0755);
	ensure_dir(TARGET_ROOT "/usr/bin", 0755);
	ensure_dir(TARGET_ROOT "/proc", 0755);
	ensure_dir(TARGET_ROOT "/sys", 0755);
	ensure_dir(TARGET_ROOT "/dev", 0755);
	ensure_dir(TARGET_ROOT "/mnt", 0755);
	ensure_dir(TARGET_ROOT "/root", 0755);
	ensure_dir(TARGET_ROOT "/tmp", 1777);

	/* 2. Log */
	report(cb, user, 10, "Opening install log...");
	log = fopen(TARGET_LOG, "w");
	if (log) {
		fprintf(log, "Phantom OS install log\n");
		fprintf(log, "Target: %s\n", TARGET_ROOT);
	}

	/* 3. System info */
	report(cb, user, 15, "Recording system info...");
	if (uname(&info) == 0 && log) {
		fprintf(log, "Kernel: %s %s\n", info.sysname, info.release);
		fprintf(log, "Arch: %s\n", info.machine);
	}

	/* 4. Disk */
	report(cb, user, 20, "Detecting disks...");
	has_disk = pick_first_disk(disk, sizeof(disk));
	if (has_disk == 0) {
		printf("     Disk present: %s (NOT partitioned — safe mode)\n", disk);
		if (log)
			fprintf(log, "Disk: %s (safe mode, no partition)\n", disk);
	} else {
		printf("     No block disk — directory install only\n");
		if (log)
			fprintf(log, "Disk: none\n");
		snprintf(disk, sizeof(disk), "none");
	}

	/* 5. Copy busybox / essential binaries if present */
	report(cb, user, 35, "Copying busybox...");
	if (access("/bin/busybox", R_OK) == 0) {
		snprintf(path, sizeof(path), "%s/bin/busybox", TARGET_ROOT);
		if (copy_file("/bin/busybox", path) == 0)
			chmod(path, 0755);
	}

	report(cb, user, 45, "Copying installer binary...");
	if (access("/bin/phatominstall", R_OK) == 0) {
		snprintf(path, sizeof(path), "%s/bin/phatominstall", TARGET_ROOT);
		copy_file("/bin/phatominstall", path);
		chmod(path, 0755);
	} else if (access("/bin/install", R_OK) == 0) {
		snprintf(path, sizeof(path), "%s/bin/install", TARGET_ROOT);
		copy_file("/bin/install", path);
		chmod(path, 0755);
	}

	/* 6. Hostname + issue */
	report(cb, user, 55, "Writing system config...");
	snprintf(path, sizeof(path), "%s/etc/hostname", TARGET_ROOT);
	write_file(path, "phantom\n");

	snprintf(path, sizeof(path), "%s/etc/issue", TARGET_ROOT);
	write_file(path,
		   "Phantom OS\\n\\l\n"
		   "Installed by phatominstall\n");

	snprintf(path, sizeof(path), "%s/etc/phantom-release", TARGET_ROOT);
	write_file(path, "Phantom OS install tree\nVERSION=0.1-dev\n");

	/* 7. fstab template */
	report(cb, user, 65, "Writing fstab template...");
	snprintf(path, sizeof(path), "%s/etc/fstab", TARGET_ROOT);
	write_file(path,
		   "# Phantom OS fstab (template)\n"
		   "proc /proc proc defaults 0 0\n"
		   "sysfs /sys sysfs defaults 0 0\n"
		   "devtmpfs /dev devtmpfs defaults 0 0\n");

	/* 8. Marker file */
	report(cb, user, 75, "Writing install marker...");
	snprintf(path, sizeof(path), "%s/etc/phantom-installed", TARGET_ROOT);
	{
		char marker[256];

		snprintf(marker, sizeof(marker),
			 "installed=1\n"
			 "target=%s\n"
			 "disk=%s\n"
			 "mode=directory-safe\n",
			 TARGET_ROOT, disk);
		write_file(path, marker);
	}

	/* 9. Simple init script for target */
	report(cb, user, 85, "Writing target init stub...");
	snprintf(path, sizeof(path), "%s/sbin/init", TARGET_ROOT);
	write_file(path,
		   "#!/bin/sh\n"
		   "echo Phantom OS target root\n"
		   "exec /bin/busybox sh\n");
	chmod(path, 0755);

	/* 10. Finish log */
	report(cb, user, 95, "Finalizing...");
	if (log) {
		fprintf(log, "Status: OK\n");
		fclose(log);
	}

	report(cb, user, 100, "Install finished.");

	printf("\n");
	printf("============================================================\n");
	printf("  INSTALL COMPLETE (safe directory mode)\n");
	printf("============================================================\n");
	printf("  Target tree : %s\n", TARGET_ROOT);
	printf("  Marker      : %s/etc/phantom-installed\n", TARGET_ROOT);
	printf("  Log         : %s\n", TARGET_LOG);
	printf("  Disks were NOT partitioned or formatted.\n");
	printf("============================================================\n");
	printf("\n");

	return 0;
}

int phantom_installer_do_system_check(void)
{
	return phantom_installer_do_system_check_with_progress(NULL, NULL);
}

int phantom_installer_do_network_setup(void)
{
	return phantom_installer_do_network_setup_with_progress(NULL, NULL);
}

int phantom_installer_do_disk_setup(void)
{
	return phantom_installer_do_disk_setup_with_progress(NULL, NULL);
}

int phantom_installer_do_install(void)
{
	return phantom_installer_do_install_with_progress(NULL, NULL);
}

int phantom_installer_run(void)
{
	printf("\nPHANTOM OS INSTALLER (text mode)\n\n");
	phantom_installer_do_system_check();
	phantom_installer_do_network_setup();
	phantom_installer_do_disk_setup();
	return 0;
}
