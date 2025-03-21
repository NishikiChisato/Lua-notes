#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * convert a string(decimal and hexadecimal) to double
 * */
void test_strtod(const char *str) {
  char *end;
  double value;
  errno = 0;

  printf("To-be-converted str: %s\n", str);
  value = strtod(str, &end);

  if (errno == ERANGE) {
    if (value == HUGE_VAL) {
      printf("OVERFLOW\n");
    } else if (value == -HUGE_VAL || value == 0) {
      printf("UNDERFLOW\n");
    }
  } else if (errno != 0) {
    perror("strtod");
  } else {
    printf("Converted value: %lf\n", value);
    printf("First non-converted address: %s\n", end);
  }

  printf("\n");
}

int main() {

  // decimal
  test_strtod("123.45");
  test_strtod("  -987.65e+2 ");
  test_strtod("1.23e-4000"); // Underflow example
  test_strtod("1.23e+4000"); // Overflow example
  test_strtod("abc123");
  test_strtod("123abc");

  // hexadecimal
  test_strtod("0x1");
  test_strtod("0x1p1");
  test_strtod("0x1p-1");
  test_strtod("0x123.45");
  test_strtod("0x123.ab");
  test_strtod("0x123.abp1");
  test_strtod("0x123.abp-1");

  return 0;
}
