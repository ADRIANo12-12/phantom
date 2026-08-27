#include <unistd.h>

int main(void) {
  const char stdoutmess[] = "stdout\n";
  const char stderrmess[] = "stderr\n";

  write(1, stdoutmess, sizeof(stdoutmess) - 1);
  write(2, stderrmess, sizeof(stderrmess) - 1);

  return 0;
}
