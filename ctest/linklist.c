#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* inspired by luaS_remove */

typedef struct linklist {
  int val;
  struct linklist *next;
} linklist;

#define LINK_ELEMTS 3

/* initialize some elements */
void init_linklist(linklist **l) {
  *l = (linklist *)malloc(sizeof(linklist));
  (*l)->val = 0;
  (*l)->next = NULL;
  linklist *p = *l;
  for (int i = 1; i < LINK_ELEMTS; i++) {
    linklist *n = (linklist *)malloc(sizeof(linklist));
    n->val = i;
    n->next = NULL;

    p->next = n;
    p = n;
  }
}

void dump_linklist(linklist *l, const char *hint) {
  linklist *p = l;
  printf("========== Dump linklist begin ==========\n");
  printf("base address: %p, hint: %s\n", l,
         (strcmp(hint, "") == 0) ? "(null)" : hint);
  printf("link list contents: \n");
  while (p) {
    printf("addr: %p, val: %d, next: %p\n", p, p->val, p->next);
    p = p->next;
  }
  printf("========== Dump linklist done ==========\n");
  printf("\n");
}

/*
 * remove elem from link list
 *
 * since we may remove the first element in link list, we must pass pointer of
 * pointer to this function
 *
 * linklist is pointer by itself, if this function receives a pointer of
 * linklist, it equivalent to pass by value instead of pass by reference
 *
 * to remove one element, we must pass the address of that element
 * */
void remove_elem(linklist **l, linklist *elem) {
  /*
   * this pointer holds the address of pointer pointing to actual memory
   * address. there are some key point of this uses:
   *
   * first: if we derefer this pointer, we actually get the pointer pointing to
   * each nodes in link list
   *
   * second: after derefer, the pointer we get can be treated as priori element
   * compared with using pointer directly pointing to each node
   * */
  linklist **p = l;
  while (*p != elem) {
    p = &(*p)->next;
  }
  *p = (*p)->next;
  free(elem);
}

int main() {
  linklist *l1;
  linklist *l2;

  init_linklist(&l1);
  init_linklist(&l2);

  dump_linklist(l1, "");
  dump_linklist(l2, "");

  /* third elem in l1 */
  linklist *re1 = l1->next->next;
  /* first elem in l2 */
  linklist *re2 = l2;

  remove_elem(&l1, re1);
  remove_elem(&l2, re2);

  /* dump again */
  dump_linklist(l1, "remove third element");
  dump_linklist(l2, "remove first element");

  return 0;
}
