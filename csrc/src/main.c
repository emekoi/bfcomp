#include "ir_gen.h"
#include "ir_interpret.h"
#define DMT_STACK_TRACE
#define DMT_IMPL
#include "rxi/dmt.h"
#define VEC_IMPL
#include "defer.h"
#include "rxi/vec.h"
#include <stdio.h>

#define BUFLEN 1024

void auto_dmt_free(void *data) {
  if (*(void **)data)
    dmt_free(*(void **)data);
}

int main(int argc, const char *argv[]) {
  if (argc < 1) {
    fputs("not enough arguments", stderr);
    return 1;
  }
  FILE *dmt_file = fopen("dmt_dump.txt", "w");
  defer {
    dmt_dump(dmt_file);
    fclose(dmt_file);
  };
  char defer_var(auto_dmt_free) *buffer = dmt_calloc(1, BUFLEN + 1);
  ir_ctx ir_ctx = {0};
  vec_reserve(&ir_ctx, BUFLEN);
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
  char defer_var(auto_dmt_free) *mem = dmt_calloc(30e3, sizeof(char));
  /* ir_ctx_dump_bf(ir_ctx); */
  /* ir_ctx_dump_ir(ir_ctx); */
  ir_ctx_interpret(&ir_ctx, mem);
  return 0;
}
