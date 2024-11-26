#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  size_t len;
  char content[1]; // flexible array in TString
} TString;

TString *new_string(const char *str, size_t len) {
  TString *s = (TString *)malloc(sizeof(TString) + len * sizeof(char));
  s->len = len;
  memcpy(s->content, str, len);
  s->content[len] = '\0';
  return s;
}

int main() {
  const char *str = "Hello World";
  TString *ts = new_string(str, strlen(str));
  printf("len of TString: %zu\n", ts->len);
  printf("content of TString: %s\n", ts->content);
  return 0;
}
