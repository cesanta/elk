// Copyright (c) 2013-2019 Cesanta Software Limited
// All rights reserved

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elk.h"

int main(int argc, char *argv[]) {
  uint8_t mem[8192];
  char buf[1024];
  int i, show_debug = 0;
  struct js *js = js_create(mem, sizeof(mem));
  jsval_t res = 0;

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
  printf("%s%s", js_fmt(js, res, buf, sizeof(buf)), show_debug ? "  " : "\n");
  /*if (show_debug) js_info(js, stdout);*/
  return EXIT_SUCCESS;
}
