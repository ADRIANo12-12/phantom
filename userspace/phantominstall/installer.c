// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>

#include "installer.h"
#include "system.h"
#include "network.h"
#include "storage.h"
#include "package.h"

int phantom_installer_do_system_check(void)
{
	printf("\n========== SYSTEM CHECK ==========\n\n");

	phantom_installer_system_check();
	phantom_installer_storage_list();
	phantom_installer_package_check();

	printf("==================================\n\n");
	return 0;
}

int phantom_installer_do_network_setup(void)
{
	int network_ok;
	int internet_ok;

	printf("\n========== NETWORK SETUP ==========\n\n");

	network_ok = phantom_installer_network_start();

	if (network_ok == 0)
		internet_ok = phantom_installer_network_test();
	else
		internet_ok = -1;

	printf("----------------------------------\n");
	printf("Network  : %s\n", network_ok == 0 ? "OK" : "FAILED");
	printf("Internet : %s\n", internet_ok == 0 ? "OK" : "FAILED");
	printf("==================================\n\n");

	return (network_ok == 0 && internet_ok == 0) ? 0 : -1;
}

int phantom_installer_do_disk_setup(void)
{
	printf("\n========== DISK SETUP ==========\n\n");
	printf("Available block devices:\n\n");

	phantom_installer_storage_list();

	printf("Disk selection and partitioning will be added in Phase 2.\n");
	printf("================================\n\n");
	return 0;
}

int phantom_installer_do_install(void)
{
	printf("\n========== INSTALL PHANTOM OS ==========\n\n");
	printf("Full disk installation is not implemented yet (Phase 2).\n");
	printf("This will later:\n");
	printf("  1. Select target disk\n");
	printf("  2. Partition + format\n");
	printf("  3. Copy rootfs\n");
	printf("  4. Install bootloader\n");
	printf("  5. Write fstab\n");
	printf("========================================\n\n");
	return 0;
}

int phantom_installer_run(void)
{
	printf("\n");
	printf("============================================================\n");
	printf("                 PHANTOM OS INSTALLER\n");
	printf("============================================================\n");
	printf("\n");

	phantom_installer_do_system_check();
	phantom_installer_do_network_setup();
	phantom_installer_do_disk_setup();

	printf("Installer modules ready.\n\n");
	return 0;
}
