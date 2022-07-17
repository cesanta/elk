// Copyright (c) 2013-2021 Cesanta Software Limited
// All rights reserved
//
// Example Elk integration. Demontrates how to implement "require".
// Create file "api.js" with the following content:
//  ({
//    add : function(a, b) { return a + b; },
//    mul : function(a, b) { return a * b; },
//  })
//
// Compile main.c and run:
//   $ cc main.c ../../elk.c -I../.. -o cli
//   $ ./cli 'let math = require("api.js"); math.mul(2,3);'
//   6
//   Executed in 0.663 ms. Mem usage is 3% of 8192 bytes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elk.h"

static struct js *s_js;

// Prints all arguments, one by one, delimit by space
static jsval_t js_print(struct js *js, jsval_t *args, int nargs) {
  for (int i = 0; i < nargs; i++) {
    const char *space = i == 0 ? "" : " ";
    printf("%s%s", space, js_str(js, args[i]));
  }
  putchar('\n');  // Finish by newline
  return js_mkval(JS_UNDEF);
}

int main(int argc, char *argv[]) {
  char mem[8192];
  struct js *js = s_js = js_create(mem, sizeof(mem));
  jsval_t res = js_mkval(JS_UNDEF);

  // Implement `print` function
  js_set(js, js_glob(js), "print", js_mkfun(js_print));

  // Treat every argument as JS expressions. Execute all one by one
  for (int i = 1; i < argc; i++) {
    res = js_eval(js, argv[i], ~0U);
  }

  // Print the result of the last one
  printf("%s\n", js_str(js, res));

  return EXIT_SUCCESS;
}
