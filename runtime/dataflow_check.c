/* Runtime half of `--pass-pipeline='builtin.module(sign-analysis{instrument})'`
 * (see Instrument.h).  Every proved fact calls this; a false `ok` means the
 * static analysis claimed something the running program contradicts. */
#include <stdio.h>
#include <stdlib.h>

void __dataflow_check(_Bool ok, long long observed, const char *message) {
  if (ok)
    return;
  fprintf(stderr, "dataflow assertion failed: %s, but observed %lld\n",
          message, observed);
  abort();
}
