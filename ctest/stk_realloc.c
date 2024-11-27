#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

typedef union StkId {
  int* p;
  ptrdiff_t offset;
}StkId;

StkId * base;
StkId* new_base;

void dump_stack(StkId* st, int cnt) {
  printf("dumping stack...\n");
  for(int i = 0; i < cnt; i ++) {
    printf("addr: %p, offset: %ld\n", st[i].p, st[i].offset);
  }
  printf("stack dumped...\n");
}

int main() {
  printf("used to explain why does StkId neeed to design a p and offset?\n");
  int cnt = 3;
  base = (StkId*)malloc(sizeof(StkId) * cnt);
  printf("base stack: %p\n", base);
  for(int i = 0; i < cnt; i ++) {
    base[i].p = (int*)malloc(sizeof(int));
    *base[i].p = i;
  }

  printf("dump original stack\n");
  dump_stack(base, cnt);

  printf("before reallocing\n");
  for(int i = 0; i < cnt; i ++) {
    base[i].offset = (void*)base[i].p - (void*)base;
  }

  printf("dump original stack\n");
  dump_stack(base, cnt);

  new_base = realloc(base, 2 * cnt * sizeof(StkId));

  printf("new stack: %p\n", new_base);
  printf("dump new stack\n");
  dump_stack(new_base, 2 * cnt);

  printf("convert to new stack\n");
  for(int i = 0; i < cnt; i ++) {
    new_base[i].p = (int*)((unsigned char*)new_base + new_base[i].offset);
  }

  printf("dump new stack\n");
  dump_stack(new_base, 2 * cnt);

  return 0;
}
