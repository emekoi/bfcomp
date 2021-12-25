#include <stdio.h>
#include "ir_interpret.h"
#include "ir_gen.h"

#define BUFLEN 1024

int main(int argc, const char *argv[]) {
  if (argc < 1) {
    fputs("not enough arguments", stderr);
    return 1;
  }
  char *buffer = calloc(1, BUFLEN + 1);
  ir_ctx *ir_ctx = ir_ctx_new(BUFLEN);
  FILE *fp = fopen(argv[1], "r");
  if (!fp)
    fprintf(stderr, "unable to open file '%s'\n", argv[1]);
  size_t read_bytes = 0;
  do {
    read_bytes = fread(buffer, sizeof(char), BUFLEN, fp);
    buffer[read_bytes] = '\0';
    if (ir_ctx_parse(&ir_ctx, buffer)) {
      goto cleanup;
    }
  } while (read_bytes > 0);
  char *mem = calloc(30e3, sizeof(char));
  /* ir_ctx_dump_bf(ir_ctx); */
  /* ir_ctx_dump_ir(ir_ctx); */
  ir_ctx_interpret(&ir_ctx, mem);
  free(mem);
cleanup:
  fclose(fp);
  ir_ctx_free(ir_ctx);
  free(buffer);
  return 0;
}
