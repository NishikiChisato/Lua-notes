CC = clang
CFLAGS = -g
LDFLAGS = -ldl -llua -lm -lpthread

.PHONY: clean

clua: clua.c
  $(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
  rm clua
