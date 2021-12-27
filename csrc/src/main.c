#include "ir_gen.h"
#include "ir_interpret.h"
#include "elf_gen.h"
#include "rxi/dmt.h"
#include "rxi/vec.h"
#include "defer.h"
#include <stdio.h>

#define BUFLEN 1024

void auto_dmt_free(void *data) {
  if (*(void **)data)
    dmt_free(*(void **)data);
}

void write_elf_file(const char *file) {
  FILE defer_var(auto_fclose) *fp = fopen(file, "w");
  elf_gen_ctx elf_ctx = elf_gen_ctx_init();

  program_t *text = gen_program_header(&elf_ctx, ".text", (Elf32_Phdr){
      .p_type = PT_LOAD,
      .p_vaddr = 0x08048000,
      .p_flags = PF_R | PF_X
  });
  vec_push_arr(text, ((uint8_t[]){
      0xb8,0x01,0x00,0x00,0x00,      /*   movl   $1,%eax               */
      0xbb,0x2a,0x00,0x00,0x00,      /*   movl   $42,%ebx               */
      0xcd,0x80,                     /*   int    $0x80                 */
  }));
  gen_section_header(&elf_ctx, ".text", (Elf32_Shdr){
      .sh_type = SHT_PROGBITS,
      .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
      .sh_addr = text->header.p_vaddr
  });
  elf_ctx.elf_header.e_entry = text->header.p_vaddr;

  gen_elf_file(fp, &elf_ctx);
  elf_gen_ctx_free(&elf_ctx);
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
