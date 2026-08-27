// SPDX-License-Identifier: GPL-2.0

#include "../libphantom/phantom.h"

#include <string.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
	static const char hellomess[] =
		"Phantom OS shell. Write help for more information.\n"
		"\tAdrian Sikora 2026 Copyright\n"
		"\t\t\tPhantom OS\n\n";

	static const char prompt[] =
		"~/PhantomOS-$ ";

	static const char help[] =
		"\n"
		"help - shows this command\n"
		"exit - exits the shell\n"
		"install - opens Phantom OS installer\n"
		"panic - panics the kernel\n"
		"reboot - restarts the system\n"
		"poweroff - shuts down the system\n";

	char buf[256];
	ssize_t n;

	write(STDOUT_FILENO,
	      hellomess,
	      sizeof(hellomess) - 1);

	for (;;) {
		write(STDOUT_FILENO,
		      prompt,
		      sizeof(prompt) - 1);

		n = read(STDIN_FILENO,
			 buf,
			 sizeof(buf) - 1);

		if (n <= 0)
			break;

		if (buf[n - 1] == '\n')
			n--;

		if (n > 0 && buf[n - 1] == '\r')
			n--;

		buf[n] = '\0';

		if (strcmp(buf, "help") == 0) {
			write(STDOUT_FILENO,
			      help,
			      sizeof(help) - 1);
			continue;
		}

		if (strcmp(buf, "panic") == 0) {
			/*
			 * 473 is the Phantom panic syscall.
			 * The syscall does not return after panic().
			 */
			phantom_panic();
			continue;
		}

		if (strcmp(buf, "poweroff") == 0) {
			reboot(RB_POWER_OFF);
			continue;
		}

		if (strcmp(buf, "reboot") == 0) {
			reboot(RB_AUTOBOOT);
			continue;
		}

		if (strcmp(buf, "install") == 0) {
			pid_t pid;

			pid = fork();

			if (pid == 0) {
				execl("/bin/phatominstall",
				      "phatominstall",
				      (char *)NULL);

				_exit(127);
			}

			if (pid > 0)
				waitpid(pid, NULL, 0);

			continue;
		}

		if (strcmp(buf, "exit") == 0)
			break;
	}

	return 0;
}
