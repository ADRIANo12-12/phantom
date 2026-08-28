// SPDX-License-Identifier: GPL-2.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>

#include "network.h"

static int command_available(const char *command)
{
	char test[256];

	snprintf(test, sizeof(test),
		 "command -v %s >/dev/null 2>&1", command);
	return system(test) == 0;
}

static int find_eth_interface(char *out, size_t out_len)
{
	DIR *dir;
	struct dirent *entry;

	dir = opendir("/sys/class/net");
	if (!dir)
		return -1;

	while ((entry = readdir(dir)) != NULL) {
		if (entry->d_name[0] == '.')
			continue;
		if (!strcmp(entry->d_name, "lo"))
			continue;

		snprintf(out, out_len, "%s", entry->d_name);
		closedir(dir);
		return 0;
	}

	closedir(dir);
	return -1;
}

int phantom_installer_network_start(void)
{
	char iface[64];

	printf("[NETWORK]\n");

	if (access("/sys/class/net", R_OK) != 0) {
		printf("  ERROR: /sys/class/net unavailable\n\n");
		return -1;
	}

	if (!command_available("udhcpc") &&
	    access("/bin/busybox", X_OK) != 0) {
		printf("  ERROR: udhcpc / busybox unavailable\n\n");
		return -1;
	}

	printf("  Network subsystem : OK\n");
	printf("  DHCP client       : OK\n");

	if (find_eth_interface(iface, sizeof(iface)) != 0) {
		printf("  Ethernet interface: NOT FOUND\n\n");
		return -1;
	}

	printf("  Ethernet interface: %s\n", iface);
	printf("  Requesting DHCP...\n");

	{
		char cmd[256];

		snprintf(cmd, sizeof(cmd),
			 "/bin/busybox udhcpc -q -n -t 5 -T 3 -A -i %s",
			 iface);

		if (system(cmd) != 0) {
			printf("  DHCP             : FAILED\n\n");
			return -1;
		}
	}

	printf("  DHCP             : OK\n\n");
	return 0;
}

int phantom_installer_network_test(void)
{
	printf("[INTERNET]\n");

	if (system("/bin/busybox wget -q -O /tmp/phantom-network-test "
		   "http://example.com") != 0) {
		printf("  Internet: FAILED\n\n");
		unlink("/tmp/phantom-network-test");
		return -1;
	}

	unlink("/tmp/phantom-network-test");
	printf("  Internet: OK\n\n");
	return 0;
}
