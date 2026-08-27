// SPDX-License-Identifier: GPL-2.0

#include <dirent.h>
#include <stdio.h>

#include "storage.h"

int phantom_installer_storage_list(void)
{
	DIR *dir;
	struct dirent *entry;

	printf("[STORAGE]\n");

	dir = opendir("/sys/block");
	if (!dir) {
		printf("  ERROR: /sys/block unavailable\n\n");
		return -1;
	}

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;

		printf("  /dev/%s\n", entry->d_name);
	}

	closedir(dir);

	printf("\n");

	return 0;
}
