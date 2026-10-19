/* Built with -fwrapv: signed overflow is defined (wraps), so a wrong sign
 * claim below is the analysis's fault, not undefined behaviour. */
#include <stdio.h>
#include <stdlib.h>

int factorial(int n) {
  int r = 1;
  for (int i = 1; i <= n; i++)
    r = r * i;
  return r;
}

int main(int argc, char **argv) {
  int n = argc > 1 ? atoi(argv[1]) : 5;
  printf("%d! = %d\n", n, factorial(n));
  return 0;
}
