// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <sys/utsname.h>

#include "system.h"

int phantom_installer_system_check(void)
{
	struct utsname info;

	if (uname(&info) < 0) {
		perror("uname");
		return -1;
	}

	printf("[SYSTEM]\n");
	printf("  OS           : %s\n", info.sysname);
	printf("  Kernel       : %s\n", info.release);
	printf("  Architecture : %s\n", info.machine);
	printf("  Node         : %s\n", info.nodename);
	printf("\n");

	return 0;
}
