// Copyright (c) 2013-2019 Cesanta Software Limited
// All rights reserved

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elk.h"

int main(int argc, char *argv[]) {
  char mem[8192];
  int i, show_debug = 0;
  struct js *js = js_create(mem, sizeof(mem));
  jsval_t res = 0;
  js_import(js, "ffi", (uintptr_t) js_ffi, "jms");
  js_import(js, "strlen", (uintptr_t) strlen, "is");
  js_import(js, "atoi", (uintptr_t) atoi, "is");

  for (i = 1; i < argc && argv[i][0] == '-'; i++) {
    if (strcmp(argv[i], "-e") == 0 && i + 1 < argc) {
      const char *code = argv[++i];
      js_gc(js, res);
      res = js_eval(js, code, strlen(code));
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      printf("Usage: %s [-e js_expression]\n", argv[0]);
      return EXIT_SUCCESS;
    } else if (strcmp(argv[i], "-v") == 0) {
      show_debug++;
    } else {
      fprintf(stderr, "Unknown flag: [%s]\n", argv[i]);
      return EXIT_FAILURE;
    }
  }
  printf("%s%s%s\n", js_str(js, res), show_debug ? "  " : "",
         show_debug ? js_info(js) : "");
  return EXIT_SUCCESS;
}
