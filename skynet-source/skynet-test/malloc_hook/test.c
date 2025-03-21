#include "inc_malloc.h"

void dumpmem(void* ptr);

int main() {
  const int cnt = 3;
  int* ptr = (int*)inc_malloc(sizeof(int) * cnt);
  dumpmem(ptr);
  inc_free(ptr);
  dumpmem(ptr);
  return 0;
}
