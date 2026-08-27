// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "network.h"

static int command_available(const char *command)
{
	char test[256];

	snprintf(
		test,
		sizeof(test),
		"command -v %s >/dev/null 2>&1",
		command);

	return system(test) == 0;
}

int phantom_installer_network_start(void)
{
	printf("[NETWORK]\n");

	if (access("/sys/class/net", R_OK) != 0) {
		printf("  ERROR: /sys/class/net unavailable\n\n");
		return -1;
	}

	if (!command_available("udhcpc")) {
		printf("  ERROR: udhcpc unavailable\n\n");
		return -1;
	}

	printf("  Network subsystem : OK\n");
	printf("  DHCP client       : OK\n");

	/*
	 * Find a non-loopback interface.
	 * The current Phantom rootfs uses BusyBox.
	 */
	if (system(
		"/bin/busybox ip link 2>/dev/null | "
		"grep -E '^[0-9]+: ' | "
		"grep -v ' lo:' >/dev/null 2>&1") != 0) {

		printf("  Ethernet interface: NOT FOUND\n\n");
		return -1;
	}

	printf("  Ethernet interface: detected\n");
	printf("  Requesting DHCP...\n");

	if (system(
		"/bin/busybox udhcpc "
		"-q "
		"-n "
		"-t 5 "
		"-T 3 "
		"-A "
		"-i eth0") != 0) {

		printf("  DHCP             : FAILED\n\n");
		return -1;
	}

	printf("  DHCP             : OK\n\n");

	return 0;
}

int phantom_installer_network_test(void)
{
	printf("[INTERNET]\n");

	if (system(
		"/bin/busybox wget "
		"-q "
		"-O /tmp/phantom-network-test "
		"https://example.com") != 0) {

		printf("  Internet: FAILED\n\n");
		unlink("/tmp/phantom-network-test");
		return -1;
	}

	unlink("/tmp/phantom-network-test");

	printf("  Internet: OK\n\n");

	return 0;
}
