#include "../libphantom/phantom.h"
#include <fcntl.h>
#include <string.h>
#include <sys/reboot.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char **argv) {
  static const char hellomess[] =
      "Phantom OS shell. Write help for more information.\n"
      "\tAdrian Sikora 2026 Copyright\n"
      "\t\t\tPhantom OS\n\n";

  static const char input[] = "~/PhantomOS-$ ";

  static const char help[] = "\nhelp - shows this command\n"
                             "exit - shuts down system\n"
                             "install - opens install Phantom OS\n"
                             "panic - panics the system\n"
                             "reboot - restarts the system\n"
                             "poweroff - shuts down the system\n";

  int fd;
  ssize_t n;
  char buf[4096];

  if (argc > 2)
    return 1;

  write(1, hellomess, sizeof(hellomess) - 1);

  for (;;) {
    write(1, input, sizeof(input) - 1);

    n = read(0, buf, sizeof(buf));

    if (n <= 0)
      break;

    if (buf[n - 1] == '\n')
      buf[n - 1] = '\0';

    if (strcmp(buf, "panic") == 0) {
        phantom_panic();
    }

    if (strcmp(buf, "install") == 0) {
        pid_t pid = fork();

        if (pid == 0) {
            execl("/bin/phatominstall",
                  "phatominstall",
                  (char *)NULL);

            _exit(127);
        }

        if (pid > 0)
            waitpid(pid, NULL, 0);
    }

    if (strcmp(buf, "poweroff") == 0) {
      reboot(RB_POWER_OFF);
    }

    if (strcmp(buf, "reboot") == 0) {
      reboot(RB_AUTOBOOT);
    }

    if (strcmp(buf, "panic") == 0) {
      int fd;

      fd = open("/proc/phantom_panic", O_WRONLY);

      if (fd < 0) {
        write(1, "PhantomBox: cannot open /proc/phantom_panic\n", 45);
      } else {
        write(fd, "1", 1);
        close(fd);
      }
    }

    if (strcmp(buf, "help") == 0) {
      write(1, help, sizeof(help) - 1);
    }
  }

  return 0;
}
