#include <assert.h>
#include <limits.h>
#include <memory.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* inspired by lstring.h and lstring.c */

/* type definition */

#define cast(t, v) ((t)(v))
#define cast2i(v) cast(int32_t, v)
#define cast2ui(v) cast(uint32_t, v)
#define cast2s(v) cast(size_t, v)
#define castp2u(v) cast(uint64_t, cast2s(v) & UINT64_MAX)

#define MAX_SIZE (cast2s(~cast2s(0)))
#define limit(n, t)                                                            \
  (cast2s(n) < MAX_SIZE / sizeof(t) ? n : MAX_SIZE / sizeof(t))

#define MAXSHRLEN 48
#define MAXSTRTAB limit(MAX_SIZE, String *)

#define INTERNAL_API static
#define EXTERNAL_API extern

typedef unsigned char byte;

typedef struct String {
  byte shrlen;
  uint32_t hash;        /* hash value for this string */
  struct String *hnext; /* for hash table */
  char contents[1];
} String;

typedef struct StringTable {
  String **hash;
  uint32_t size; /* size of hash slots */
  uint32_t nuse; /* number of used elements */
} StringTable;

#define check_exp(cond, exp) (assert(cond), exp)
#define isshrstr(s) (s->shrlen <= MAXSHRLEN)

#define getstr(s) check_exp(isshrstr(s), s->contents)
#define stringsize(clen)                                                       \
  (offsetof(String, contents) + (clen + 1) * sizeof(char))

/* size must the power of 2 */
#define hmod(hash, size)                                                       \
  check_exp((size & (size - 1)) == 0, cast2ui(hash & (size - 1)))

#define createstrobj(str, slen, allen, s)                                      \
  (s = (String *)malloc(allen), s->shrlen = slen, s->hnext = NULL,             \
   s->hash = stringhash(str, slen, seed), memcpy(s->contents, str, slen))

/* interface */

/* for string */
INTERNAL_API uint32_t stringhash(const char *str, byte l, uint32_t seed) {
  uint32_t hash = seed ^ cast2ui(l);
  for (; l > 0; l--) {
    hash ^= ((hash << 5) + (hash >> 2) + cast2ui(str[l - 1]));
  }
  return hash;
}

/* for string table */
INTERNAL_API void tablerehash(String **vect, size_t old, size_t new) {
  for (size_t i = old; i < new; i++) {
    vect[i] = NULL; /* reset new slots */
  }
  /* rehash old slots */
  for (size_t i = 0; i < old; i++) {
    String *p = vect[i];
    vect[i] = NULL; /* reset this slot */
    while (p) {
      String *nxt = p->hnext; /* save the next item */
      uint32_t slot = hmod(p->hash, new);
      p->hnext = vect[slot];
      vect[slot] = p;
      p = nxt;
    }
  }
}

INTERNAL_API int growstrtable(StringTable *st) {
  assert(cast2s(st->size * 2) <= MAXSTRTAB);
  String **nvec = (String **)realloc(st->hash, st->size * 2);
  if (nvec) {
    st->hash = nvec;
    return 1;
  }
  return 0;
}

INTERNAL_API void resizetable(StringTable *st, size_t new) {
  if (new < st->size) {
    tablerehash(st->hash, st->size, new);
  } else if (new > st->size) {
    growstrtable(st);
    tablerehash(st->hash, st->size, new);
  }
}

INTERNAL_API void insertstr(StringTable *st, String *s) {
  uint32_t slot = hmod(s->hash, st->size);
  s->hnext = st->hash[slot];
  st->hash[slot] = s;
  st->nuse++;
}

INTERNAL_API void removestr(StringTable *st, String *s) {
  uint32_t slot = hmod(s->hash, st->size);
  String **p = &st->hash[slot];
  while (*p != s) {
    *p = (*p)->hnext;
  }
  *p = (*p)->hnext;
  free(s); /* free string */
}

EXTERNAL_API StringTable *initstrtable() {
  StringTable *st = (StringTable *)malloc(sizeof(StringTable));
  st->size = (1 << 4); /* initial size of 16 */
  st->nuse = 0;
  st->hash = (String **)malloc(sizeof(String *) * st->size);
  return st;
}

/* for string */
INTERNAL_API String *internstring(StringTable *st, const char *str, byte slen,
                                  uint32_t seed) {
  String *s;
  uint32_t hash = stringhash(str, slen, seed); /* calculate hash for str */
  uint32_t slot = hmod(hash, st->size);
  String **list = &st->hash[slot]; /* locate all string placed in this slot */
  for (String *p = *list; p;
       p = p->hnext) { /* hash must identical in this slot */
    char *c1 = getstr(p);
    if (p->shrlen == slen &&
        (memcmp(getstr(p), str, slen * sizeof(char)) == 0)) {
      return p; /* reuse string */
    }
  }
  if (++st->nuse >= st->size) { /* need to grow table ? */
    growstrtable(st);
  }
  /* create new string */
  uint32_t allen = stringsize(slen);
  createstrobj(str, slen, allen, s);
  insertstr(st, s); /* insert it into string table */
  return s;
}

EXTERNAL_API String *createstr(StringTable *st, const char *str, byte slen) {
  assert(slen <= MAXSHRLEN);
  uint32_t seed = 0xAAAB;
  return internstring(st, str, slen, seed);
}

/* create literal string */
#define createltrstr(st, str) createstr(st, str, sizeof(str) / sizeof(char))

EXTERNAL_API void releasestr(StringTable *st, String *s) { removestr(st, s); }

#define eqstring(l, r) (l == r)

int main() {
  StringTable *st = initstrtable();
  String *s1 = createltrstr(st, "Hotaru");
  String *s2 = createltrstr(st, "Suki");

  String *s3 = createltrstr(st, "Hotaru");
  String *s4 = createltrstr(st, "Suki");

  assert(eqstring(s1, s3));
  assert(eqstring(s2, s4));

  return 0;
}
