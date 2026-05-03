#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "elk.h"

__AFL_FUZZ_INIT();

int main() {

#ifdef __AFL_HAVE_MANUAL_CONTROL
  __AFL_INIT();
#endif

  unsigned char *buf = __AFL_FUZZ_TESTCASE_BUF;  // must be after __AFL_INIT
                                                 // and before __AFL_LOOP!

  while (__AFL_LOOP(10000)) {

    int len = __AFL_FUZZ_TESTCASE_LEN;  // don't use the macro directly in a
                                        // call!
    
    char *input = malloc(len + 1);
    memcpy(input, buf, len);
    input[len] = '\0';

    char mem[4096];
    /* Setup function call, e.g. struct target *tmp = libtarget_init() */
    struct js *js = js_create(mem, sizeof(mem));
    js_setmaxcss(js, 2048);

    /* Call function to be fuzzed, e.g.: */
    js_eval(js, (const char *)input, ~0U);
    /* Reset state. e.g. libtarget_free(tmp) */

  }

  return 0;

}