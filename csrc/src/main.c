#include "defer.h"
#include "ir_gen.h"
#include "ir_interpret.h"
#include <stdio.h>

#define BUFLEN 1024

int main(int argc, const char *argv[]) {
  if (argc < 1) {
    fputs("not enough arguments", stderr);
    return 1;
  }
  char defer_var(auto_free) *buffer = calloc(1, BUFLEN + 1);
  ir_ctx *ir_ctx = ir_ctx_new(BUFLEN);
  defer { ir_ctx_free(ir_ctx); };
  FILE defer_var(auto_fclose) *fp = fopen(argv[1], "r");
  if (!fp) {
    fprintf(stderr, "unable to open file '%s'\n", argv[1]);
    return 1;
  }
  size_t read_bytes = 0;
  do {
    read_bytes = fread(buffer, sizeof(char), BUFLEN, fp);
    buffer[read_bytes] = '\0';
    if (ir_ctx_parse(&ir_ctx, buffer)) {
      return 1;
    }
  } while (read_bytes > 0);
  char defer_var(auto_free) *mem = calloc(30e3, sizeof(char));
  /* ir_ctx_dump_bf(ir_ctx); */
  /* ir_ctx_dump_ir(ir_ctx); */
  ir_ctx_interpret(&ir_ctx, mem);
  return 0;
}
