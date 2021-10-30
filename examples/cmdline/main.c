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
#include <time.h>

#include "elk.h"

static struct js *s_js;

// Function that loads JS code from a given file.
static jsval_t require(const char *filename) {
  char data[32 * 1024];
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) return 0;
  size_t len = fread(data, 1, sizeof(data), fp);
  return js_eval(s_js, data, len);
}

static jsval_t eval(const char *code) {
  return js_eval(s_js, code, ~0);
}

static void print(const char *str) {
  printf("%s", str);
}

static const char *str(jsval_t v) {
  return js_str(s_js, v);
}

int main(int argc, char *argv[]) {
  char mem[8192];
  struct js *js = s_js = js_create(mem, sizeof(mem));
  clock_t beginning = clock();
  jsval_t res = js_eval(js, "undefined", ~0);
  int i, verbose = 0;

  // Import our custom function "require" into the global namespace.
  js_set(js, js_glob(js), "require", js_import(js, (uintptr_t) require, "js"));
  js_set(js, js_glob(js), "eval", js_import(js, (uintptr_t) eval, "js"));
  js_set(js, js_glob(js), "print", js_import(js, (uintptr_t) print, "vs"));
  js_set(js, js_glob(js), "str", js_import(js, (uintptr_t) str, "sj"));

  // Process command line args
  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-v") == 0) {
      verbose++;
    } else if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
      res = js_eval(js, argv[++i], ~0);
    } else {
      printf("Usage: %s [-v] [-e EXPRESSION] ...\n", argv[0]);
    }
  }

  printf("%s\n", js_str(js, res));

  if (verbose) {
    double ms = (double) (clock() - beginning) * 1000 / CLOCKS_PER_SEC;
    printf("Executed in %g ms. Mem usage is %d%% of %d bytes.\n", ms,
           js_usage(js), (int) sizeof(mem));
  }
  return EXIT_SUCCESS;
}
