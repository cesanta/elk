#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elk.h"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s script.js\n", argv[0]);
    return 1;
  }

  const char *filename = argv[1];
  FILE *fp = fopen(filename, "rb");
  if (!fp) {
    perror("fopen");
    return 1;
  }

  fseek(fp, 0, SEEK_END);
  long len = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *data = malloc(len + 1);
  if (!data) return 1;

  fread(data, 1, len, fp);
  data[len] = 0;
  fclose(fp);

  char mem[4096];
  struct js *js = js_create(mem, sizeof(mem));
  js_setmaxcss(js, 2048);

  jsval_t res = js_eval(js, data, len);
  printf("%s\n", js_str(js, res));

  free(data);
  return 0;
}
