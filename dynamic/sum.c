/* Sum of 1..n, again with -fwrapv. */
#include <stdio.h>
#include <stdlib.h>

int sum(int n) {
  int s = 1;
  for (int i = 2; i <= n; i++)
    s = s + i;
  return s;
}

int main(int argc, char **argv) {
  int n = argc > 1 ? atoi(argv[1]) : 5;
  printf("sum(%d) = %d\n", n, sum(n));
  return 0;
}
