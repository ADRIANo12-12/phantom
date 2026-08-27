// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "package.h"

int phantom_installer_package_check(void)
{
	printf("[PACKAGES]\n");

	/*
	 * Current Phantom rootfs is Alpine/BusyBox based.
	 * Therefore apk is the native package backend for now.
	 */
	if (access("/sbin/apk", X_OK) == 0) {
		printf("  Backend : apk\n");
		printf("  Package manager: available\n\n");
		return 0;
	}

	if (access("/bin/apk", X_OK) == 0) {
		printf("  Backend : apk\n");
		printf("  Package manager: available\n\n");
		return 0;
	}

	printf("  Backend : none\n");
	printf("  Package manager is not present in initramfs\n\n");

	return -1;
}

int phantom_installer_package_install(const char *package)
{
	char command[512];

	if (!package || !*package)
		return -1;

	if (access("/sbin/apk", X_OK) == 0) {
		snprintf(
			command,
			sizeof(command),
			"/sbin/apk add %s",
			package);

		return system(command);
	}

	if (access("/bin/apk", X_OK) == 0) {
		snprintf(
			command,
			sizeof(command),
			"/bin/apk add %s",
			package);

		return system(command);
	}

	return -1;
}
