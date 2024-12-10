#include <stdlib.h>
#include <stdio.h>
#include <math.h>

double luai_nummod(double a, double b) {
  double r = fmod(a, b); // truncate
  if(r > 0 ? (b < 0) : (r < 0 && b > 0)) {
    r += b;
  }
  return r;
}

double luai_nummod1(double a, double b) {
  double n1 = (long long)(a / b); // truncate
  double r = a - n1 * b;
  if(r > 0 ? (b < 0) : (r < 0 && b > 0)) {
    r += b;
  }
  return r;
}

/** 
 * luai_nummod is equivalent to luai_nummod1, but we expect its result is equal to luai_nummod2.
 *
 * the remainder of truncate has the same sign of dividend, but the remainder of floor has the same sign of divisor.
 *
 * the difference between them is the result of truncate(n1) is less then the result of floor(n2) by one, 
 * when the division has not-integer negative number.
 *
 * the finial result of truncate is less then the finial result of floor by divisor, 
 * when a and b has different sign and remainder is not zero(which means that the quotient of a and b is float number)
 *
 * so, we only need to judge dividend and divisor has different sign and the remainder is not-zero, then we add divisor to remainder.
 * after that, we will get the same result of floor
 *
 * */

double luai_nummod2(double a, double b) {
  double n2 = floor(a / b); // floor
  double r = a - n2 * b;
  if(r > 0 ? (b < 0) : (r < 0 && b > 0)) {
    r += b;
  }
  return r;
}

int main() {
  printf("%lf\n", luai_nummod(5, -2));
  printf("%lf\n", luai_nummod(-5, 2));
  printf("%lf\n", luai_nummod(-5, -2));
  printf("%lf\n", luai_nummod(5, 2));

  printf("%lf\n", luai_nummod1(5, -2));
  printf("%lf\n", luai_nummod1(-5, 2));
  printf("%lf\n", luai_nummod1(-5, -2));
  printf("%lf\n", luai_nummod1(5, 2));

  printf("%lf\n", luai_nummod2(5, -2));
  printf("%lf\n", luai_nummod2(-5, 2));
  printf("%lf\n", luai_nummod2(-5, -2));
  printf("%lf\n", luai_nummod2(5, 2));
  return 0;
}
