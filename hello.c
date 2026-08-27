#include <unistd.h>
int main(void) {
  ssize_t written;
  const char message[] = "Hello Linux!\n";
  written = write(1, message, sizeof(message) - 1);
  return written < 0;
}
