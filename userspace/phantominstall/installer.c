// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>

#include "installer.h"
#include "system.h"
#include "network.h"
#include "storage.h"
#include "package.h"

int phantom_installer_run(void)
{
	int network_ok;
	int internet_ok;
	int package_ok;

	printf("\n");
	printf("============================================================\n");
	printf("                 PHANTOM OS INSTALLER\n");
	printf("============================================================\n");
	printf("\n");

	phantom_installer_system_check();

	network_ok = phantom_installer_network_start();

	if (network_ok == 0)
		internet_ok = phantom_installer_network_test();
	else
		internet_ok = -1;

	phantom_installer_storage_list();

	package_ok = phantom_installer_package_check();

	printf("============================================================\n");
	printf("INSTALLER STATUS\n");
	printf("============================================================\n");

	printf("Network     : %s\n",
	       network_ok == 0 ? "OK" : "FAILED");

	printf("Internet    : %s\n",
	       internet_ok == 0 ? "OK" : "FAILED");

	printf("Packages    : %s\n",
	       package_ok == 0 ? "OK" : "UNAVAILABLE");

	printf("\n");

	if (network_ok == 0 && internet_ok == 0)
		printf("Phantom has network access.\n");
	else
		printf("Network configuration requires attention.\n");

	printf("\n");

	return 0;
}
