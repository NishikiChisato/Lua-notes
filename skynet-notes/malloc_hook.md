# [Skynet] Understanding malloc hook

<!--toc:start-->
- [Malloc_hook](#mallochook)
  - [Intro](#intro)
  - [Description](#description)
  - [Conclusion](#conclusion)
<!--toc:end-->

## Intro

When I first encoutered `skynet_malloc.h` in skynet's source code, I was much confused by this design:

- Why does the header both define marcos(e.g. `skynet_malloc`)and declarations(e.g. `void* skynet_malloc(size_t sz)`)?
- Why is there no `skynet_malloc.c` file to provide these definitions?

Additionally, while `malloc_hook.h` and `malloc_hook.c` form a compilation unit. `malloc_hook.h` does not declare `skynet_malloc`, instead, `malloc_hook.c` defines it.
If we just simply treat it as deplication, when we try to remove this function, although we can pass compilation safely, we cannot run skynet framework.


In this article, I aim to provide a clear and understandable explanation of this design. This article is relatively short, as the concept may seem complex at first but is actually quite simple.

## Description

Let's first example the content of `skynet_malloc.h`

```c
#define skynet_malloc malloc
#define skynet_calloc calloc
#define skynet_realloc realloc
#define skynet_free free
#define skynet_memalign memalign
#define skynet_aligned_alloc aligned_alloc
#define skynet_posix_memalign posix_memalign

void * skynet_malloc(size_t sz);
void * skynet_calloc(size_t nmemb,size_t size);
void * skynet_realloc(void *ptr, size_t size);
void skynet_free(void *ptr);
char * skynet_strdup(const char *str);
void * skynet_lalloc(void *ptr, size_t osize, size_t nsize);	// use for lua
void * skynet_memalign(size_t alignment, size_t size);
void * skynet_aligned_alloc(size_t alignment, size_t size);
int skynet_posix_memalign(void **memptr, size_t alignment, size_t size);
```

As we know, marcos essentially perform a text replacement. So, when another source file includes this header, all instances of `skynet_malloc` will
be replaced by `malloc`. In other words, the content of `skynet_malloc.h` after preprocessor becomes:

```c
void * malloc(size_t sz);
void * calloc(size_t nmemb,size_t size);
void * realloc(void *ptr, size_t size);
void free(void *ptr);
char * strdup(const char *str);
void * lalloc(void *ptr, size_t osize, size_t nsize);	// use for lua
void * memalign(size_t alignment, size_t size);
void * aligned_alloc(size_t alignment, size_t size);
int posix_memalign(void **memptr, size_t alignment, size_t size);
```

Although there are function names, they are still replaced by preprocessor. To verify this, we can check whether `skynet_malloc` is replaced by `malloc` by processing a source file with preprocessor.

The `skynet_mq.c` is a good example, we can run the following command:

```bash
clang -E -o _skynet -Iskynet-src -I3rd/jemalloc/include/jemalloc skynet-src/skynet_mq.c
nvim _skynet
```

In this file, `skynet_mq_create` calls `skynet_malloc`, **but after preprocessoing, `skynet_malloc` is replaced with `malloc`**. 
Now, we can draw a prelimilary conclusion that, all instances of `skynet_malloc` are replaced by `malloc`. 
Furthermore, any file including `skynet.h` will also have this replacement.

Additionally, `malloc_hook.c` **defines** `skynet_malloc`. **This symblo, too, is replaced by `malloc` during preprocessor**.
Essentially, this definition prepares for the definition of `malloc` rather than `skynet_malloc`.

By combining these conclusions together, we can deduce:

- If a file wants to invoke `skynet_malloc`, it must include `skynet_malloc`.
- Since `skynet_malloc.h` defines the marcos and declares for `skynet_malloc`, so any instances of `skynet_malloc` are replaced by `malloc`.
- Due to we also compile with `malloc_hook.c` together(we can check makefile to prove it), this unit is used to provide the definition of `malloc`.
- When `jemalloc` is enabled, this definition adds extra functionality to track memory usage.
  - Due to we compile `jemalloc` with prefix `je_`(check makefile), this definition essentially calls `jemalloc` interface.

We can further verify this by executing the following command to check `malloc_hook.c`:

```bash
clang -E -o _skynet -Iskynet-src -I3rd/jemalloc/include/jemalloc skynet-src/malloc_hook.c
nvim _skynet
```

This file does not contain the definition of `skynet_malloc`; instead, the original definition of `skynet_malloc` is replaced by the definition of `malloc`.

## Conclusion

The final conclusions are:

- Skynet framework uses its internal memory allocation interface `skynet_malloc` to allocate memory.
- At compile time, every instnace of `skynet_malloc` is replaced by `malloc`.
- Without `jemalloc`, `malloc` in standard library is used.
- With `jemalloc` enabled, we can track extra information for memory usage and invoke `jemalloc` internally.
  - Additionally, only when we let `jemalloc` enabled, `malloc` is re-defined, which can further confirming these conclusions.
