# Design of Table

```c
/*
** Nodes for Hash tables: A pack of two TValue's (key-value pairs)
** plus a 'next' field to link colliding entries. The distribution
** of the key's fields ('key_tt' and 'key_val') not forming a proper
** 'TValue' allows for a smaller size for 'Node' both in 4-byte
** and 8-byte alignments.
*/
typedef union Node {
  struct NodeKey {
    TValuefields;  /* fields for value */
    lu_byte key_tt;  /* key type */
    int next;  /* for chaining */
    Value key_val;  /* key value */
  } u;
  TValue i_val;  /* direct access to node's value as a proper 'TValue' */
} Node;
```

Note that `node` is a union, and it contains both key and value for key-value pairs. Besides, lua rearrange the order of each field in node,
it can optimize the occupy of memory.

For example, in 64-bits system, the size of `node` is `3 * 8 = 24`:

```c
typedef union Node {
  struct NodeKey {
    Value value_;   /* start at 0 offset, alignment is 8 */
    ly_byte tt_;    /* start at 8 offset, alignment is 1 */
    lu_byte key_tt; /* start at 9 offset, alignment is 1 */
    int next;       /* start at 12 offset(two bytes pedding before), alignment is 4 */
    Value key_val;  /* start at 16 offset, alignment is 8 */
  } u;
  TValue i_val;     /* in union and size is less than u */
} Node;
```

By contrast, if we write `node` like this:

```c
typedef union Node {
  struct NodeKey {
    Value value_;
    ly_byte tt_;
    lu_byte key_tt;
    Value key_val;
    int next;
  } u;
  TValue i_val;
} Node;
```

It will occupy `4 * 8 = 32` bits in 64-bits system.

```c
typedef struct Table {
  CommonHeader;
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */
  lu_byte lsizenode;  /* log2 of size of 'node' array */
  unsigned int alimit;  /* "limit" of 'array' array */
  TValue *array;  /* array part */
  Node *node;
  Node *lastfree;  /* any free position is before this position */
  struct Table *metatable;
  GCObject *gclist;
} Table;
```

For this definition, there are several key point to be pay attention:

- `array` and `node` field are consecutive, without another field gap into them. Since this design, we can iterate array part and node part of table by using the same index(in `luaH_next` function).

- `lsizenode` is the ceil of log2 to size of node array, namely, `ceil(log2(node size))`.
  - Furthermore, the capasity(space allocated by allocator) of hash array is always the power of 2, 

- `alimit` do not represent the actually size of array part.


# Operation for Table

For basic operation of table, we're going to focus on its sematic, namely, implication of input and output, and inner implementation.

## Auxiliary function

Given a key, try to find its main position, namely, the place we directly apply hash function to calculate.

If the key is integer, we directly hash its value; if the key is float, we use custom hash function for floating-point.
For bool type, it always map to index of `0` or `1` in hash part of table.
For string, we use its hash value for addressing(specially, for long string, we lazy calculate its hash value). For light userdata, light C function, and other type, we both hash its pointer address;  

```c
/*
** returns the 'main' position of an element in a table (that is,
** the index of its hash value).
*/
static Node *mainpositionTV (const Table *t, const TValue *key) {
  switch (ttypetag(key)) {
    case LUA_VNUMINT: {
      lua_Integer i = ivalue(key);
      return hashint(t, i);
    }
    case LUA_VNUMFLT: {
      lua_Number n = fltvalue(key);
      return hashmod(t, l_hashfloat(n));
    }
    case LUA_VSHRSTR: {
      TString *ts = tsvalue(key);
      return hashstr(t, ts);
    }
    case LUA_VLNGSTR: {
      TString *ts = tsvalue(key);
      return hashpow2(t, luaS_hashlongstr(ts));
    }
    case LUA_VFALSE:
      return hashboolean(t, 0);
    case LUA_VTRUE:
      return hashboolean(t, 1);
    case LUA_VLIGHTUSERDATA: {
      void *p = pvalue(key);
      return hashpointer(t, p);
    }
    case LUA_VLCF: {
      lua_CFunction f = fvalue(key);
      return hashpointer(t, f);
    }
    default: {
      GCObject *o = gcvalue(key);
      return hashpointer(t, o);
    }
  }
}

```

The contrast version of last function, given a hash node, try to find the main position of its key.

Note that, the reason we introduce these two function is if collision is occured(two different key map to identical hash slot), we must shift one of them to another position(in Lua, usually to high address space).
so, we must write a function to get the main position of one key. 

```c
l_sinline Node *mainpositionfromnode (const Table *t, Node *nd) {
  TValue key;
  getnodekey(cast(lua_State *, NULL), &key, nd);
  return mainpositionTV(t, &key);
}
```

## Get operation

### Main get function & Generic get function

#### Main get function

```c
/*
** main search function
*/
const TValue *luaH_get (Table *t, const TValue *key) {
  switch (ttypetag(key)) {
    case LUA_VSHRSTR: return luaH_getshortstr(t, tsvalue(key));
    case LUA_VNUMINT: return luaH_getint(t, ivalue(key));
    case LUA_VNIL: return &absentkey;
    case LUA_VNUMFLT: {
      lua_Integer k;
      if (luaV_flttointeger(fltvalue(key), &k, F2Ieq)) /* integral index? */
        return luaH_getint(t, k);  /* use specialized version */
      /* else... */
    }  /* FALLTHROUGH */
    default:
      return getgeneric(t, key, 0);
  }
}
```

#### Generic get function

This function simplely check whether the key from slot is identical to input key starting from main position of input key. 

Lua use `gnext` marco to chain each hash slot. If one slot has empty next field(which is zero), meaning that it's last slot in hash table.

```c
/*
** "Generic" get version. (Not that generic: not valid for integers,
** which may be in array part, nor for floats with integral values.)
** See explanation about 'deadok' in function 'equalkey'.
*/
static const TValue *getgeneric (Table *t, const TValue *key, int deadok) {
  Node *n = mainpositionTV(t, key);
  for (;;) {  /* check whether 'key' is somewhere in the chain */
    if (equalkey(key, n, deadok))
      return gval(n);  /* that's it */
    else {
      int nx = gnext(n);
      if (nx == 0)
        return &absentkey;  /* not found */
      n += nx;
    }
  }
}
```

### Auxiliary get function

#### Get integer from hash table

```c
/*
** Search function for integers. If integer is inside 'alimit', get it
** directly from the array part. Otherwise, if 'alimit' is not
** the real size of the array, the key still can be in the array part.
** In this case, do the "Xmilia trick" to check whether 'key-1' is
** smaller than the real size.
** The trick works as follow: let 'p' be an integer such that
**   '2^(p+1) >= alimit > 2^p', or  '2^(p+1) > alimit-1 >= 2^p'.
** That is, 2^(p+1) is the real size of the array, and 'p' is the highest
** bit on in 'alimit-1'. What we have to check becomes 'key-1 < 2^(p+1)'.
** We compute '(key-1) & ~(alimit-1)', which we call 'res'; it will
** have the 'p' bit cleared. If the key is outside the array, that is,
** 'key-1 >= 2^(p+1)', then 'res' will have some bit on higher than 'p',
** therefore it will be larger or equal to 'alimit', and the check
** will fail. If 'key-1 < 2^(p+1)', then 'res' has no bit on higher than
** 'p', and as the bit 'p' itself was cleared, 'res' will be smaller
** than 2^p, therefore smaller than 'alimit', and the check succeeds.
** As special cases, when 'alimit' is 0 the condition is trivially false,
** and when 'alimit' is 1 the condition simplifies to 'key-1 < alimit'.
** If key is 0 or negative, 'res' will have its higher bit on, so that
** if cannot be smaller than alimit.
*/
const TValue *luaH_getint (Table *t, lua_Integer key) {
  lua_Unsigned alimit = t->alimit;
  if (l_castS2U(key) - 1u < alimit)  /* 'key' in [1, t->alimit]? */
    return &t->array[key - 1];
  else if (!isrealasize(t) &&  /* key still may be in the array part? */
           (((l_castS2U(key) - 1u) & ~(alimit - 1u)) < alimit)) {
    t->alimit = cast_uint(key);  /* probably '#t' is here now */
    return &t->array[key - 1];
  }
  else {  /* key is not in the array part; check the hash */
    Node *n = hashint(t, key);
    for (;;) {  /* check whether 'key' is somewhere in the chain */
      if (keyisinteger(n) && keyival(n) == key)
        return gval(n);  /* that's it */
      else {
        int nx = gnext(n);
        if (nx == 0) break;
        n += nx;
      }
    }
    return &absentkey;
  }
}
```



#### Get short string from hash table

```c
/*
** search function for short strings
*/
const TValue *luaH_getshortstr (Table *t, TString *key) {
  Node *n = hashstr(t, key);
  lua_assert(key->tt == LUA_VSHRSTR);
  for (;;) {  /* check whether 'key' is somewhere in the chain */
    if (keyisshrstr(n) && eqshrstr(keystrval(n), key))
      return gval(n);  /* that's it */
    else {
      int nx = gnext(n);
      if (nx == 0)
        return &absentkey;  /* not found */
      n += nx;
    }
  }
}
```

#### Get long string from hash table

```c
const TValue *luaH_getstr (Table *t, TString *key) {
  if (key->tt == LUA_VSHRSTR)
    return luaH_getshortstr(t, key);
  else {  /* for long strings, use generic case */
    TValue ko;
    setsvalue(cast(lua_State *, NULL), &ko, key);
    return getgeneric(t, &ko, 0);
  }
}
```

## Set function

```c
/*
** Finish a raw "set table" operation, where 'slot' is where the value
** should have been (the result of a previous "get table").
** Beware: when using this function you probably need to check a GC
** barrier and invalidate the TM cache.
*/
void luaH_finishset (lua_State *L, Table *t, const TValue *key,
                                   const TValue *slot, TValue *value) {
  if (isabstkey(slot))
    luaH_newkey(L, t, key, value);
  else
    setobj2t(L, cast(TValue *, slot), value);
}

```


```c
/*
** beware: when using this function you probably need to check a GC
** barrier and invalidate the TM cache.
*/
void luaH_set (lua_State *L, Table *t, const TValue *key, TValue *value) {
  const TValue *slot = luaH_get(t, key);
  luaH_finishset(L, t, key, slot, value);
}

```

```c
void luaH_setint (lua_State *L, Table *t, lua_Integer key, TValue *value) {
  const TValue *p = luaH_getint(t, key);
  if (isabstkey(p)) {
    TValue k;
    setivalue(&k, key);
    luaH_newkey(L, t, &k, value);
  }
  else
    setobj2t(L, cast(TValue *, p), value);
}
```
