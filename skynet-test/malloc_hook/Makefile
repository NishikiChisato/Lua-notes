.PHONY: with without clean

without:
	clang -I. -ljemalloc test.c malloc_hooc.c -DNOUSE_JEMALLOC -o test

with:
	clang -I. -ljemalloc test.c malloc_hooc.c -o test

clean:
	rm test
