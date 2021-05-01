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
//   $ ./cli 'let math = require(0, "api.js"); math.mul(2,3);'
//   6
//   Executed in 0.663 ms. Mem usage is 3% of 8192 bytes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "elk.h"

// Function that loads JS code from a given file.
static jsval_t require(struct js *js, const char *filename) {
  char data[32 * 1024];
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) return 0;
  size_t len = fread(data, 1, sizeof(data), fp);
  return js_eval(js, data, len);
}

int main(int argc, char *argv[]) {
  char mem[8192];
  struct js *js = js_create(mem, sizeof(mem));
  const char *code = argc > 1 ? argv[1] : "";
  clock_t beginning = clock();

  // Import our custom function "require" into the global namespace.
  js_set(js, js_glob(js), "require", js_import(js, (uintptr_t) require, "jms"));
  jsval_t res = js_eval(js, code, strlen(code));
  printf("%s\n", js_str(js, res));

  double ms = (double) (clock() - beginning) * 1000 / CLOCKS_PER_SEC;
  printf("Executed in %g ms. Mem usage is %d%% of %d bytes.\n", ms,
         js_usage(js), (int) sizeof(mem));
  return EXIT_SUCCESS;
}
