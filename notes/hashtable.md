<!--toc:start-->
- [Design of Table](#design-of-table)
- [Operation for Table](#operation-for-table)
  - [Auxiliary function](#auxiliary-function)
    - [Finding main position of key](#finding-main-position-of-key)
    - [Get free position](#get-free-position)
    - [Array part](#array-part)
    - [Rehash](#rehash)
  - [Insert operation](#insert-operation)
  - [Get operation](#get-operation)
    - [Main get function & Generic get function](#main-get-function-generic-get-function)
      - [Main get function](#main-get-function)
      - [Generic get function](#generic-get-function)
    - [Next function](#next-function)
    - [Auxiliary get function](#auxiliary-get-function)
      - [Get integer from hash table](#get-integer-from-hash-table)
      - [Get short string from hash table](#get-short-string-from-hash-table)
      - [Get long string from hash table](#get-long-string-from-hash-table)
  - [Set function](#set-function)
  - [Search](#search)
<!--toc:end-->

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

- Memory occupation

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

TODO:
- The design of `next` field:

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
  - Furthermore, the capacity(space allocated by allocator) of hash part is always the power of 2, and the capacity of array part is also the power of 2.  
  - The capacity of hash part can be calculated by `2^(lsizenode)`

- `alimit` do not represent the actually size(or real size) of array part. We can consider it as a **hint**, with which we can quickly check whether a integer index exist in array part of not.
  - The capacity of array part is **the smallest power of 2 but greater than `alimit`**, which is done by `luaH_realasize`.
  - If we explicitly call `setrealasize` marco, `alimit` is going to represent real size of array part. **Note that it will happen only when we call `setlimittosize`**.
  - For other case, `alimit` may be set to other value to represent a 'false positive' boundary of array pary(a hint).

# Operation for Table

For basic operation of table, we're going to focus on its sematic, namely, implication of input and output, and inner implementation.

## Auxiliary function

### Finding main position of key

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

### Get free position

Starting from `t->node`, this function iterates node by node to find the first node with empty key.
Node that this function do not check whether the capacity is enough or not, so this additional check must be done before calling this function.

```c
static Node *getfreepos (Table *t) {
  if (!isdummy(t)) {
    while (t->lastfree > t->node) {
      t->lastfree--;
      if (keyisnil(t->lastfree))
        return t->lastfree;
    }
  }
  return NULL;  /* could not find a free place */
}
```

### Array part

The following two function should be treated together. `arrayindex` merely check if integer key out of bound(index bigger than the largest array capacity) or not.
If failed, return zero; otherwise, do a convertion from integer to `unsigned int`.

The second function try to find integer index of given key. This function should explicitly specific array capacity. Let's dive in details.
This function `findindex` is only called in `luaH_next`, we can condsider it as auxiliary function to `luaH_next`. 

Due to the integer key of lua begin from `1` but `t->array` begin from `0`, so we should check `key - 1` instead of `key` whether out of bound.
Due to the exsitence of this offset, if we pass the last integer key of array part to `findindex`, it actually do not retrive that key.

For example, if table in lua layer is `t = {[1] = 1, [2] = 2, [3] = 3, ["key"] = "val"}` and we push `3` to stack for `luaH_next` as start key, it will not iterate key `3` but key `key`.
Because `findindex` will return `3` in this scenario. Although it less than `asize`, but `t->array[3]` is empty(only `t->array[0:2]` have value), so it goes to iterate hash part.

In hash part, we should inherit this attribute. If we merely return `i + asize`, it will iterate input key, which is not satify our expectation, we should return `(i + 1) + asize`.

```c
/*
** returns the index for 'k' if 'k' is an appropriate key to live in
** the array part of a table, 0 otherwise.
*/
static unsigned int arrayindex (lua_Integer k) {
  if (l_castS2U(k) - 1u < MAXASIZE)  /* 'k' in [1, MAXASIZE]? */
    return cast_uint(k);  /* 'key' is an appropriate array index */
  else
    return 0;
}

/*
** returns the index of a 'key' for table traversals. First goes all
** elements in the array part, then elements in the hash part. The
** beginning of a traversal is signaled by 0.
*/
static unsigned int findindex (lua_State *L, Table *t, TValue *key,
                               unsigned int asize) {
  unsigned int i;
  if (ttisnil(key)) return 0;  /* first iteration */
  i = ttisinteger(key) ? arrayindex(ivalue(key)) : 0;
  if (i - 1u < asize)  /* is 'key' inside array part? */
    return i;  /* yes; that's the index */
  else {
    const TValue *n = getgeneric(t, key, 1);
    if (l_unlikely(isabstkey(n)))
      luaG_runerror(L, "invalid key to 'next'");  /* key not found */
    i = cast_int(nodefromval(n) - gnode(t, 0));  /* key index in hash table */
    /* hash elements are numbered after array ones */
    return (i + 1) + asize;
  }
}
```

### Rehash

The following series function form the rehash and resize functionality of table. 
If we read from top to bottom according from source code order, it's extremely hard to understand the implementation of rehash, therefore we rearrange its order for better understanding.

Let's take a look at the upper layer function, which can help to understand what's sematic of each auxiliary function(lower layer).
Thanks to annotations after each line, we can roughy know functionality of each function.

Function `numusearray` used to count the number of keys in array part, and function `numusehash` used to count the number of **integer keys** in hash part.
Variable `na` is the number of keys in array part, and array `nums` with only `MAXABITS + 1` sizes serves as a buffer to distrubute each integer key to intevals with a power of 2 size and count how many keys exit in each intevals.
For example, if the key is dense, say `[1, 100]`, the content of `nums` should be:

```
nums[] = {0}
index i represent intevals (2^(i - 1), 2^i]
nums[0] += 1 // how many keys lie in [1, 2] 
nums[1] += 2 // how many keys lie in (2, 4]
nums[2] += 4 // how many keys lie in (4, 8]
nums[3] += 8 // how many keys lie in (8, 16]
...
nums[31] += ...
```

According the implication of this array, function `countint` used to distrubuting(apply ceil of log2 to) a integer key to proper index of `nums`
Note that the return value of `numusearray` and `numusehash` both represent the number of key(irrespective its type) in array and hash part, but under the hook, `numusehash` only accumulate integer key and store result in `nums` and `na`.
Now, we have known meaning of each variables. `na` is the number of keys in all table and `nums` is the distribution of each integer key.
We pass these two variables to `computesizes` to calculate the new size of **array part**, which should satify has the half integer keys should goes into new array part.
Note that we pass by reference of `na` to `computesizes`, so it will assign it to the number of integer goes to array part.
After that, having been getted the new size of array part, we pass it and the new size of hash part(total key minus all integer keys going to array part) to resize the whole table.

```c
/*
** nums[i] = number of keys 'k' where 2^(i - 1) < k <= 2^i
*/
static void rehash (lua_State *L, Table *t, const TValue *ek) {
  unsigned int asize;  /* optimal size for array part */
  unsigned int na;  /* number of keys in the array part */
  unsigned int nums[MAXABITS + 1];
  int i;
  int totaluse;
  for (i = 0; i <= MAXABITS; i++) nums[i] = 0;  /* reset counts */
  setlimittosize(t);
  na = numusearray(t, nums);  /* count keys in array part */
  totaluse = na;  /* all those keys are integer keys */
  totaluse += numusehash(t, nums, &na);  /* count keys in hash part */
  /* count extra key */
  if (ttisinteger(ek))
    na += countint(ivalue(ek), nums);
  totaluse++;
  /* compute new size for array part */
  asize = computesizes(nums, &na);
  /* resize the table to new computed sizes */
  luaH_resize(L, t, asize, totaluse - na);
}

```

This function calculate the ceil of log2 of key and add value with corresponding index in `nums` array. 

```c
static int countint (lua_Integer key, unsigned int *nums) {
  unsigned int k = arrayindex(key);
  if (k != 0) {  /* is 'key' an appropriate array index? */
    nums[luaO_ceillog2(k)]++;  /* count as such */
    return 1;
  }
  else
    return 0;
}
```

The following two function calculate the number of keys in array part and hash part, respectively.
For `numusehash`, it just reversely iterate all hash node and call `countint` to accumulately count integer keys.  
For `numusearray`, it manually divery each integer key to `nums` array.

```c
/*
** Count keys in array part of table 't': Fill 'nums[i]' with
** number of keys that will go into corresponding slice and return
** total number of non-nil keys.
*/
static unsigned int numusearray (const Table *t, unsigned int *nums) {
  int lg;
  unsigned int ttlg;  /* 2^lg */
  unsigned int ause = 0;  /* summation of 'nums' */
  unsigned int i = 1;  /* count to traverse all array keys */
  unsigned int asize = limitasasize(t);  /* real array size */
  /* traverse each slice */
  for (lg = 0, ttlg = 1; lg <= MAXABITS; lg++, ttlg *= 2) {
    unsigned int lc = 0;  /* counter */
    unsigned int lim = ttlg;
    if (lim > asize) {
      lim = asize;  /* adjust upper limit */
      if (i > lim)
        break;  /* no more elements to count */
    }
    /* count elements in range (2^(lg - 1), 2^lg] */
    for (; i <= lim; i++) {
      if (!isempty(&t->array[i-1]))
        lc++;
    }
    nums[lg] += lc;
    ause += lc;
  }
  return ause;
}

static int numusehash (const Table *t, unsigned int *nums, unsigned int *pna) {
  int totaluse = 0;  /* total number of elements */
  int ause = 0;  /* elements added to 'nums' (can go to array part) */
  int i = sizenode(t);
  while (i--) {
    Node *n = &t->node[i];
    if (!isempty(gval(n))) {
      if (keyisinteger(n))
        ause += countint(keyival(n), nums);
      totaluse++;
    }
  }
  *pna += ause;
  return totaluse;
}
```

Now, we have known the meaning of two input argument. `nums` represent the distribution of integer keys and `pna` represent how many integer keys. 
This function should return new size(called optimal size) of array part, and this return value should hold several attributes.

- The optimal size should be the power of 2.
- The half of optimal size should less than the number of total integer keys and the optimal size should greater than and equal to the number of total integer keys. 

The internal of this function is pretty easy to understand. It just accumulate `nums` by index `i` and check current value `a` greater than two power of `i` or not. 
If so, it assign `optimal` to the value `2^i` and `na` to current accumulate value `a`, meaning that how many integer key should go to new array part.

```c
/*
** Compute the optimal size for the array part of table 't'. 'nums' is a
** "count array" where 'nums[i]' is the number of integers in the table
** between 2^(i - 1) + 1 and 2^i. 'pna' enters with the total number of
** integer keys in the table and leaves with the number of keys that
** will go to the array part; return the optimal size.  (The condition
** 'twotoi > 0' in the for loop stops the loop if 'twotoi' overflows.)
*/
static unsigned int computesizes (unsigned int nums[], unsigned int *pna) {
  int i;
  unsigned int twotoi;  /* 2^i (candidate for optimal size) */
  unsigned int a = 0;  /* number of elements smaller than 2^i */
  unsigned int na = 0;  /* number of elements to go to array part */
  unsigned int optimal = 0;  /* optimal size for array part */
  /* loop while keys can fill more than half of total size */
  for (i = 0, twotoi = 1;
       twotoi > 0 && *pna > twotoi / 2;
       i++, twotoi *= 2) {
    a += nums[i];
    if (a > twotoi/2) {  /* more than half elements present? */
      optimal = twotoi;  /* optimal size (till now) */
      na = a;  /* all elements up to 'optimal' will go to array part */
    }
  }
  lua_assert((optimal == 0 || optimal / 2 < na) && na <= optimal);
  *pna = na;
  return optimal;
}
```

The following four functions are pretty easy to understand, we just take a glance with them.

Function `setnodevector` just reset hash part of table according to input `size`(`size` is not in byte but in number of elements). If `size` is zero, it do not actually allocate space, it just assign `t->node` to global static dummy node.
It means that, if many table both empty, the `t->node` field of these table both have the identical value. If we coincidentally load `ltable.c` many times(global static dummy node defines here), it may course some bugs.
For each hash nodes, we simplely set its key and value to empty. For `t->lastfree`, it will be assigned to the next position of the last node(to find a free node, check `getfreepos` function).

`reinsert` function just iterate all not-empty node from `ot` and insert it into `t` by calling `luaH_set`

`exchangehashpart` function just exchange hash fields(which is `t->node, t->lsizenode, t->lastfree`), do not deep copy.

`getfreepos` function will try to find the first empty starting from the last node of whole hash node array. If all nodes are not empty, it return `null` representing there aren't free space and need to rehash it.

```c
/*
** Creates an array for the hash part of a table with the given
** size, or reuses the dummy node if size is zero.
** The computation for size overflow is in two steps: the first
** comparison ensures that the shift in the second one does not
** overflow.
*/
static void setnodevector (lua_State *L, Table *t, unsigned int size) {
  if (size == 0) {  /* no elements to hash part? */
    t->node = cast(Node *, dummynode);  /* use common 'dummynode' */
    t->lsizenode = 0;
    t->lastfree = NULL;  /* signal that it is using dummy node */
  }
  else {
    int i;
    int lsize = luaO_ceillog2(size);
    if (lsize > MAXHBITS || (1u << lsize) > MAXHSIZE)
      luaG_runerror(L, "table overflow");
    size = twoto(lsize);
    t->node = luaM_newvector(L, size, Node);
    for (i = 0; i < cast_int(size); i++) {
      Node *n = gnode(t, i);
      gnext(n) = 0;
      setnilkey(n);
      setempty(gval(n));
    }
    t->lsizenode = cast_byte(lsize);
    t->lastfree = gnode(t, size);  /* all positions are free */
  }
}

/*
** (Re)insert all elements from the hash part of 'ot' into table 't'.
*/
static void reinsert (lua_State *L, Table *ot, Table *t) {
  int j;
  int size = sizenode(ot);
  for (j = 0; j < size; j++) {
    Node *old = gnode(ot, j);
    if (!isempty(gval(old))) {
      /* doesn't need barrier/invalidate cache, as entry was
         already present in the table */
      TValue k;
      getnodekey(L, &k, old);
      luaH_set(L, t, &k, gval(old));
    }
  }
}

/*
** Exchange the hash part of 't1' and 't2'.
*/
static void exchangehashpart (Table *t1, Table *t2) {
  lu_byte lsizenode = t1->lsizenode;
  Node *node = t1->node;
  Node *lastfree = t1->lastfree;
  t1->lsizenode = t2->lsizenode;
  t1->node = t2->node;
  t1->lastfree = t2->lastfree;
  t2->lsizenode = lsizenode;
  t2->node = node;
  t2->lastfree = lastfree;
}

static Node *getfreepos (Table *t) {
  if (!isdummy(t)) {
    while (t->lastfree > t->node) {
      t->lastfree--;
      if (keyisnil(t->lastfree))
        return t->lastfree;
    }
  }
  return NULL;  /* could not find a free place */
}
```

The following function used to resize total hash table.
According to the explanation of [rehash](#rehash), we have known the meaning of two input argument.
`newasize` represent the new size of array part, `nhsize` represent the new size of hash part.
Pay attention to the definition of size here, it's not in byte, but in the number of elements.
At the same time, the value of `newasize` and `nhsize` may not the power of 2, but the capacity of these part must be.

This function declares a table **on stack** and **only initializes its hash part(all elements to empty)**, intending to make it as temporary hash part of table.
The process of resize is as follows:
- First, check whether array part of table will shrink or not
  -  If so, intert elements with out of bound to hash part of temporary table.
    - Here, there may be a question why we pretend array has a new size(small size)?
    - Because we should shrink `t->alimit` in case of original key existing in array part may reinsert into array part(we expect these key goes into hash part).
  - Otherwise, we simplely reallocate array part. If reallocation failed, we release hash part of temporary table and raise an error with array have beed resize.
- Second, we set key and value to empty in new allocated space, exchange hash part to larger one(exchange hash part of current table to temporary one) and insert keys with previously exists in array part into hash part.

```c
/*
** Resize table 't' for the new given sizes. Both allocations (for
** the hash part and for the array part) can fail, which creates some
** subtleties. If the first allocation, for the hash part, fails, an
** error is raised and that is it. Otherwise, it copies the elements from
** the shrinking part of the array (if it is shrinking) into the new
** hash. Then it reallocates the array part.  If that fails, the table
** is in its original state; the function frees the new hash part and then
** raises the allocation error. Otherwise, it sets the new hash part
** into the table, initializes the new part of the array (if any) with
** nils and reinserts the elements of the old hash back into the new
** parts of the table.
*/
void luaH_resize (lua_State *L, Table *t, unsigned int newasize,
                                          unsigned int nhsize) {
  unsigned int i;
  Table newt;  /* to keep the new hash part */
  unsigned int oldasize = setlimittosize(t);
  TValue *newarray;
  /* create new hash part with appropriate size into 'newt' */
  setnodevector(L, &newt, nhsize);
  if (newasize < oldasize) {  /* will array shrink? */
    t->alimit = newasize;  /* pretend array has new size... */
    exchangehashpart(t, &newt);  /* and new hash */
    /* re-insert into the new hash the elements from vanishing slice */
    for (i = newasize; i < oldasize; i++) {
      if (!isempty(&t->array[i]))
        luaH_setint(L, t, i + 1, &t->array[i]);
    }
    t->alimit = oldasize;  /* restore current size... */
    exchangehashpart(t, &newt);  /* and hash (in case of errors) */
  }
  /* allocate new array */
  newarray = luaM_reallocvector(L, t->array, oldasize, newasize, TValue);
  if (l_unlikely(newarray == NULL && newasize > 0)) {  /* allocation failed? */
    freehash(L, &newt);  /* release new hash part */
    luaM_error(L);  /* raise error (with array unchanged) */
  }
  /* allocation ok; initialize new part of the array */
  exchangehashpart(t, &newt);  /* 't' has the new hash ('newt' has the old) */
  t->array = newarray;  /* set new array part */
  t->alimit = newasize;
  for (i = oldasize; i < newasize; i++)  /* clear new slice of the array */
     setempty(&t->array[i]);
  /* re-insert elements from old hash part into new parts */
  reinsert(L, &newt, t);  /* 'newt' now has the old hash */
  freehash(L, &newt);  /* free old hash part */
}
```

The following function just a wrapper of `luaH_resize`, the input argument `nasize` is new array size.
We pass it and size of hash part to `luaH_resize` function, meaning that we only intends to resize the size of array part.

```c
void luaH_resizearray (lua_State *L, Table *t, unsigned int nasize) {
  int nsize = allocsizenode(t);
  luaH_resize(L, t, nasize, nsize);
}
```

## Insert operation

As annotation shown, this function aims to insert a new key into table as following steps:
- Check key's main position is free:
  - If free, that's it.
  - Otherwise, check colliding key is in its main position:
    - If so, new key goes to empty position(don't move colliding key).
    - Otherwise, move colliding key to empty position and insert new key to its main position.

Miscellaneous:
- If new key is floating-point, try to convert it to integer, that is apply floor and check whether equal or not.
- `mp` is main position of new key(if not empty, that's colliding key), `othern` is main position of colliding key(which is `mp`).
- why we loop `othern + gnext(othern) != mp` will eventually find the previous key of `mp`
  - Due to colliding key is inserted before new key, therefore its main position must come before new key. So, if we iterate from colliding key's main position, we can eventually reach current colliding position. 
  - After we find previous key of colliding key, we add a new key after it(copy `mp` to `f`) and correct field `next` of `f` and `mp`(`gnext(f)` should goes to distence between `mp` and `f`, and `gnext(mp)` should set to zero due to it's the last node in this chain).

```c
/*
** inserts a new key into a hash table; first, check whether key's main
** position is free. If not, check whether colliding node is in its main
** position or not: if it is not, move colliding node to an empty place and
** put new key in its main position; otherwise (colliding node is in its main
** position), new key goes to an empty position.
*/
static void luaH_newkey (lua_State *L, Table *t, const TValue *key,
                                                 TValue *value) {
  Node *mp;
  TValue aux;
  if (l_unlikely(ttisnil(key)))
    luaG_runerror(L, "table index is nil");
  else if (ttisfloat(key)) {
    lua_Number f = fltvalue(key);
    lua_Integer k;
    if (luaV_flttointeger(f, &k, F2Ieq)) {  /* does key fit in an integer? */
      setivalue(&aux, k);
      key = &aux;  /* insert it as an integer */
    }
    else if (l_unlikely(luai_numisnan(f)))
      luaG_runerror(L, "table index is NaN");
  }
  if (ttisnil(value))
    return;  /* do not insert nil values */
  mp = mainpositionTV(t, key);
  if (!isempty(gval(mp)) || isdummy(t)) {  /* main position is taken? */
    Node *othern;
    Node *f = getfreepos(t);  /* get a free place */
    if (f == NULL) {  /* cannot find a free place? */
      rehash(L, t, key);  /* grow table */
      /* whatever called 'newkey' takes care of TM cache */
      luaH_set(L, t, key, value);  /* insert key into grown table */
      return;
    }
    lua_assert(!isdummy(t));
    othern = mainpositionfromnode(t, mp);
    if (othern != mp) {  /* is colliding node out of its main position? */
      /* yes; move colliding node into free position */
      while (othern + gnext(othern) != mp)  /* find previous */
        othern += gnext(othern);
      gnext(othern) = cast_int(f - othern);  /* rechain to point to 'f' */
      *f = *mp;  /* copy colliding node into free pos. (mp->next also goes) */
      if (gnext(mp) != 0) {
        gnext(f) += cast_int(mp - f);  /* correct 'next' */
        gnext(mp) = 0;  /* now 'mp' is free */
      }
      setempty(gval(mp));
    }
    else {  /* colliding node is in its own main position */
      /* new node will go into free position */
      if (gnext(mp) != 0)
        gnext(f) = cast_int((mp + gnext(mp)) - f);  /* chain new position */
      else lua_assert(gnext(f) == 0);
      gnext(mp) = cast_int(f - mp);
      mp = f;
    }
  }
  setnodekey(L, mp, key);
  luaC_barrierback(L, obj2gco(t), key);
  lua_assert(isempty(gval(mp)));
  setobj2t(L, gval(mp), value);
}
```

## Get operation

### Main get function & Generic get function

All get function will return the value of input key.

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

### Next function

This function simplely iterate all elements from key's index getting by `findindex`. 
We have explained the hook of `findindex`, it will return the next index of key's index, and the implementation of `luaH_next` is pretty intuitive. 

```c
int luaH_next (lua_State *L, Table *t, StkId key) {
  unsigned int asize = luaH_realasize(t);
  unsigned int i = findindex(L, t, s2v(key), asize);  /* find original key */
  for (; i < asize; i++) {  /* try first array part */
    if (!isempty(&t->array[i])) {  /* a non-empty entry? */
      setivalue(s2v(key), i + 1);
      setobj2s(L, key + 1, &t->array[i]);
      return 1;
    }
  }
  for (i -= asize; cast_int(i) < sizenode(t); i++) {  /* hash part */
    if (!isempty(gval(gnode(t, i)))) {  /* a non-empty entry? */
      Node *n = gnode(t, i);
      getnodekey(L, s2v(key), n);
      setobj2s(L, key + 1, gval(n));
      return 1;
    }
  }
  return 0;  /* no more elements */
}
```

### Auxiliary get function

#### Get integer from hash table

The key point of this function is check whether a integer key whthin the range of array part or not.

For simplest situation, if key in `[1, t->alimit]`, we directly retrive array part(note that `alimit` may not the real size of the array part, but in this scenario, we don't care it).
If not, due to `alimit` may not the real size of array part, we do the following check:

- Objectives: check `key - 1` whether whthin the capacity of array part.
  - Firstly, we have known the capacity of array part is the smallest power of 2 but greater than `alimit`, which means that: `2^p < alimit <= 2^(p + 1)`.
  - Second, we know `key - 1` is greater than and equal to `alimit` now.
  - Therefore, we should check **whether `key - 1` is less than `2^(p + 1)` or not**.

We can clear `p`-th bit of `key - 1`, which we call `res`.

If `key - 1` is greater than and equal to `2^(p + 1)`, `res` will have some bits higher than `p`, so, it must greater than and equal to `alimit`(`alimit <= 2(p + 1)`)
If `key - 1` is less than `2^(p + 1)`, `res` absolutely less than `2^p`(since `p`-th bit is cleared), so, it must less than `alimit`(`alimit > 2^p`)

Clearing `p`-th of `key - 1` can be done by apply `(key - 1) & ~(alimit - 1)`. Since `2^p < alimit <= 2 ^ (p + 1)`, so `2^p <= alimit - 1 < 2 ^ (p + 1)`
Flipping `alimit - 1` will set all bits higher than `p`-th to one but `p`-th bit to zero, and other bits can be ignored. Therefore, do a binary and operation between `key - 1` and `~(alimit - 1)` will clear `p`-th bits of `key - 1`.

If this key not exists in array part, we directly pass its value to hash and probe in hash part.
The probe process is similar with `getgeneric` function, find its main position and iterate it from low address to high address.

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

Due to string always exists in hash part, so its getter function is pretty intuitive: calculating main position of input key, iterating node by node.

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

For long string, directly call generic get function.

Pay attention that the way to get short string is similar with one to get long string.
No matter its length, we both levearge its hash value to address in hash part. The difference is, the hash value for short string is pre-calculate, and one for long string is lzay-calculate.

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

The `slot` field is a pointer to the value of `key`, so its value should be gained by previous getter function, and we can directly use `setobj` marco to set its value.

If `isabstkey(slot)` is true, means that this key do not exist in table, we should [insert a new key](#insert-operation) into it.

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

The following two function just a wrapper of `luaH_finishset`. 
The first is generic function to set any-type of key to table, and the second is specifical function to set integer key to table.

```c
/*
** beware: when using this function you probably need to check a GC
** barrier and invalidate the TM cache.
*/
void luaH_set (lua_State *L, Table *t, const TValue *key, TValue *value) {
  const TValue *slot = luaH_get(t, key);
  luaH_finishset(L, t, key, slot, value);
}

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

## Search 

TODO: About finding absent key.

The key point of this form of binary search is that left endpoint always satifies condition but right endpoint always not satifies condition.
So, we initialize left and right endpoint to **left close and right open**, and we eventually return left endpoint, since left endpoint always satifies condition.
Similarly, if we want to return right endpoint, that is right endpoint always satifies condition, we should initialize left and right endpoint to **left open and right close**. 

The function `binsearch` just the last section of `hash_search`, so we omit it.

```c
/*
** Try to find a boundary in the hash part of table 't'. From the
** caller, we know that 'j' is zero or present and that 'j + 1' is
** present. We want to find a larger key that is absent from the
** table, so that we can do a binary search between the two keys to
** find a boundary. We keep doubling 'j' until we get an absent index.
** If the doubling would overflow, we try LUA_MAXINTEGER. If it is
** absent, we are ready for the binary search. ('j', being max integer,
** is larger or equal to 'i', but it cannot be equal because it is
** absent while 'i' is present; so 'j > i'.) Otherwise, 'j' is a
** boundary. ('j + 1' cannot be a present integer key because it is
** not a valid integer in Lua.)
*/
static lua_Unsigned hash_search (Table *t, lua_Unsigned j) {
  lua_Unsigned i;
  if (j == 0) j++;  /* the caller ensures 'j + 1' is present */
  do {
    i = j;  /* 'i' is a present index */
    if (j <= l_castS2U(LUA_MAXINTEGER) / 2)
      j *= 2;
    else {
      j = LUA_MAXINTEGER;
      if (isempty(luaH_getint(t, j)))  /* t[j] not present? */
        break;  /* 'j' now is an absent index */
      else  /* weird case */
        return j;  /* well, max integer is a boundary... */
    }
  } while (!isempty(luaH_getint(t, j)));  /* repeat until an absent t[j] */
  /* i < j  &&  t[i] present  &&  t[j] absent */
  while (j - i > 1u) {  /* do a binary search between them */
    lua_Unsigned m = (i + j) / 2;
    if (isempty(luaH_getint(t, m))) j = m;
    else i = m;
  }
  return i;
}

static unsigned int binsearch (const TValue *array, unsigned int i,
                                                    unsigned int j) {
  while (j - i > 1u) {  /* binary search */
    unsigned int m = (i + j) / 2;
    if (isempty(&array[m - 1])) j = m;
    else i = m;
  }
  return i;
}
```

TODO:

```c
/*
** Try to find a boundary in table 't'. (A 'boundary' is an integer index
** such that t[i] is present and t[i+1] is absent, or 0 if t[1] is absent
** and 'maxinteger' if t[maxinteger] is present.)
** (In the next explanation, we use Lua indices, that is, with base 1.
** The code itself uses base 0 when indexing the array part of the table.)
** The code starts with 'limit = t->alimit', a position in the array
** part that may be a boundary.
**
** (1) If 't[limit]' is empty, there must be a boundary before it.
** As a common case (e.g., after 't[#t]=nil'), check whether 'limit-1'
** is present. If so, it is a boundary. Otherwise, do a binary search
** between 0 and limit to find a boundary. In both cases, try to
** use this boundary as the new 'alimit', as a hint for the next call.
**
** (2) If 't[limit]' is not empty and the array has more elements
** after 'limit', try to find a boundary there. Again, try first
** the special case (which should be quite frequent) where 'limit+1'
** is empty, so that 'limit' is a boundary. Otherwise, check the
** last element of the array part. If it is empty, there must be a
** boundary between the old limit (present) and the last element
** (absent), which is found with a binary search. (This boundary always
** can be a new limit.)
**
** (3) The last case is when there are no elements in the array part
** (limit == 0) or its last element (the new limit) is present.
** In this case, must check the hash part. If there is no hash part
** or 'limit+1' is absent, 'limit' is a boundary.  Otherwise, call
** 'hash_search' to find a boundary in the hash part of the table.
** (In those cases, the boundary is not inside the array part, and
** therefore cannot be used as a new limit.)
*/
lua_Unsigned luaH_getn (Table *t) {
  unsigned int limit = t->alimit;
  if (limit > 0 && isempty(&t->array[limit - 1])) {  /* (1)? */
    /* there must be a boundary before 'limit' */
    if (limit >= 2 && !isempty(&t->array[limit - 2])) {
      /* 'limit - 1' is a boundary; can it be a new limit? */
      if (ispow2realasize(t) && !ispow2(limit - 1)) {
        t->alimit = limit - 1;
        setnorealasize(t);  /* now 'alimit' is not the real size */
      }
      return limit - 1;
    }
    else {  /* must search for a boundary in [0, limit] */
      unsigned int boundary = binsearch(t->array, 0, limit);
      /* can this boundary represent the real size of the array? */
      if (ispow2realasize(t) && boundary > luaH_realasize(t) / 2) {
        t->alimit = boundary;  /* use it as the new limit */
        setnorealasize(t);
      }
      return boundary;
    }
  }
  /* 'limit' is zero or present in table */
  if (!limitequalsasize(t)) {  /* (2)? */
    /* 'limit' > 0 and array has more elements after 'limit' */
    if (isempty(&t->array[limit]))  /* 'limit + 1' is empty? */
      return limit;  /* this is the boundary */
    /* else, try last element in the array */
    limit = luaH_realasize(t);
    if (isempty(&t->array[limit - 1])) {  /* empty? */
      /* there must be a boundary in the array after old limit,
         and it must be a valid new limit */
      unsigned int boundary = binsearch(t->array, t->alimit, limit);
      t->alimit = boundary;
      return boundary;
    }
    /* else, new limit is present in the table; check the hash part */
  }
  /* (3) 'limit' is the last element and either is zero or present in table */
  lua_assert(limit == luaH_realasize(t) &&
             (limit == 0 || !isempty(&t->array[limit - 1])));
  if (isdummy(t) || isempty(luaH_getint(t, cast(lua_Integer, limit + 1))))
    return limit;  /* 'limit + 1' is absent */
  else  /* 'limit + 1' is also present */
    return hash_search(t, limit);
}

```
