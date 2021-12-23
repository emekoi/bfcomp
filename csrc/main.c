#include "bfcomp.h"
#include <stdio.h>

#define BUFLEN 1024

int main(int argc, const char *argv[]) {
  if (argc < 1) {
    fputs("not enough arguments", stderr);
    return 1;
  }
  char *buffer = calloc(1, BUFLEN + 1);
  cg_ctx *compile_ctx = cg_ctx_new(BUFLEN);
  FILE *fp = fopen(argv[1], "r");
  if (!fp)
    fprintf(stderr, "unable to open file '%s'\n", argv[1]);
  size_t read_bytes = 0;
  do {
    read_bytes = fread(buffer, sizeof(char), BUFLEN, fp);
    buffer[read_bytes] = '\0';
    if (cg_ctx_compile(&compile_ctx, buffer)) {
      goto cleanup;
    }
  } while (read_bytes > 0);
  char *mem = calloc(30e3, sizeof(char));
  /* cg_ctx_dump_bf(compile_ctx); */
  /* cg_ctx_dump_ir(compile_ctx); */
  cg_ctx_interpret(&compile_ctx, mem);
cleanup:
  fclose(fp);
  cg_ctx_free(compile_ctx);
  free(buffer);
  free(mem);
  return 0;
}
