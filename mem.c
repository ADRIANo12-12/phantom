#include <stdio.h>
#include <stdlib.h>

int global = 42;

int main(void) {
  int local = 10;
  int *heap = malloc(sizeof(int));
  printf("Allocated heap: %p\n\n", (void *)&heap);

  printf("\t\tMemory:\n\n");
  printf("\tGlobal: %d\n", global);
  printf("Local: %d\n", local);
  printf("Heap: %d\n", *heap);

  free(heap);
}
