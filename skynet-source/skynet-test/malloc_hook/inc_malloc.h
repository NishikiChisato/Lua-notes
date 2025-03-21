#ifndef __INC_MALLOC_H__
#define __INC_MALLOC_H__

#include <stddef.h>

#define inc_malloc malloc
#define inc_free free

void* inc_malloc(size_t sz);
void inc_free(void* ptr);

#endif
