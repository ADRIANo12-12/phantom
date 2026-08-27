#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv) {

  int fd;
  char buf[4096];
  ssize_t n;

  if (argc < 2) {
    return 1;
  }

  fd = open(argv[1], O_RDONLY);

  if (fd == -1) {
    return 1;
  }

  n = read(fd, buf, sizeof(buf));

  if (n == -1) {
    return 1;
  }

  write(1, buf, n);
  close(fd);

  return 0;
}
