// Copyright (c) 2013-2021 Cesanta Software Limited
// All rights reserved
//
// Example Elk integration. Demontrates how to implement "require".
// Compile main.c and run:
//   $ cc main.c ../../elk.c -I../.. -o cli -DJS_DUMP
//   $ ./cli 'let math = require("api.js"); math.mul(2,3);'
//   6
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elk.h"

// Function that loads JS code from a given file.
static jsval_t js_require(struct js *js, jsval_t *args, int nargs) {
  if (nargs != 1) return js_mkundef();
  char data[1024];
  char *filename = js_getstr(js, args[0], NULL);
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) return js_mkundef();
  size_t len = fread(data, 1, sizeof(data), fp);
  return js_eval(js, data, len);
}

int main(int argc, char *argv[]) {
  char mem[8192], dump = 0;
  struct js *js = js_create(mem, sizeof(mem));
  jsval_t res = js_mkundef();

  // Implement `require` function
  js_set(js, js_glob(js), "require", js_mkfun(js_require));

  // Treat every argument as JS expressions. Execute all one by one
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-d") == 0) {
      dump++;
    } else if (strcmp(argv[i], "-gct") == 0 && i + 1 < argc) {
      js_setgct(js, strtoul(argv[++i], 0, 0));
    } else {
      res = js_eval(js, argv[i], ~0U);
    }
  }

  // Print the result of the last one
  printf("%s\n", js_str(js, res));
  if (dump) js_dump(js);

  return EXIT_SUCCESS;
}
