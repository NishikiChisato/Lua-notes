#include <stdio.h>
#include <string.h>
#include "inc_malloc.h"

#ifndef NOUSE_JEMALLOC

#include <jemalloc/jemalloc.h>

#define MEM_MALLOCED 1
#define MEM_FREE 2

typedef struct {
  size_t mem_size;
  uint32_t tag;
  size_t cookie_size;
} mem_cookie;

#define PREFIX_SIZE sizeof(mem_cookie)

static void* fill_prefix(void* ptr, size_t sz, size_t cookie_size) {
  mem_cookie* st = (mem_cookie*)ptr; 
  st->mem_size = sz;
  st->tag = MEM_MALLOCED;
  char* ret = (char*)st + cookie_size;
  memcpy(ret - sizeof(cookie_size), &cookie_size, sizeof(cookie_size));
  return ret;
}

static size_t get_cookie_size(void* ptr) {
  size_t sz;
  memcpy(&sz, (char*)ptr - sizeof(sz), sizeof(sz));
  return sz;
}

static void* clear_prefix(void* ptr) {
  size_t cookie_size = get_cookie_size(ptr);
  mem_cookie* st = (mem_cookie*)((char*)ptr - cookie_size);
  st->tag = MEM_FREE;
  return st;
}

void* inc_malloc(size_t sz) {
  void* ptr = je_malloc(sz + PREFIX_SIZE);
  return fill_prefix(ptr, sz, PREFIX_SIZE);
}

void inc_free(void* ptr) {
  if(ptr == NULL) {
    return;
  }
  void* rawptr = clear_prefix(ptr);
  je_free(rawptr);
  return;
}

void dumpmem(void* ptr) {
  size_t cookie_size = get_cookie_size(ptr);
  mem_cookie* st = (mem_cookie*)((char*)ptr - cookie_size);
  fprintf(stdout, "[mem_cookie: %p]: mem_size: %zu bytes, tag: %s, cookie_size: %zu bytes\nptr: %p\n"
          , st
          , st->mem_size
          , st->tag == MEM_MALLOCED ? ("MEM_MALLOCED") : st->tag == MEM_FREE ? "MEM_FREE" : "ERR"
          , st->cookie_size, ptr);
  fflush(stdout);
}

#else

void dumpmem(void* ptr) {
  fprintf(stdout, "not use jemalloc\n");
  fflush(stdout);
}

#endif
