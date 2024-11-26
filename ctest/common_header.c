#include <stdio.h>
#include <stdlib.h>

typedef struct {
  int type;
} CommonHeader;

typedef struct {
  CommonHeader header;
  int val;
} IntObj;

typedef struct {
  CommonHeader header;
  float val;
} FltObj;

typedef union {
  CommonHeader header;
  IntObj iobj;
  FltObj fobj;
} UniObj;

#define INT_TYPE 1
#define FLT_TYPE 2

#define cast2u(obj) ((UniObj *)(obj))
#define cast2i(obj) (cast2u(obj)->iobj)
#define cast2f(obj) (cast2u(obj)->fobj)

#define checktype(obj, t) (cast2u(obj)->header.type == (t))

int main() {
  IntObj iobj = {{INT_TYPE}, 1};
  FltObj fobj = {{FLT_TYPE}, 2};
  UniObj *uobj = (UniObj *)malloc(sizeof(UniObj));

  uobj->iobj = iobj;

  if (checktype(uobj, INT_TYPE)) {
    printf("header: %d, val: %d\n", cast2u(uobj)->header.type,
           cast2i(uobj).val);
  }

  uobj->fobj = fobj;

  if (checktype(uobj, FLT_TYPE)) {
    printf("header: %d, val: %f\n", cast2u(uobj)->header.type,
           cast2f(uobj).val);
  }

  return 0;
}
