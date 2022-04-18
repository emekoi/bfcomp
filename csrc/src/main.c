#include "common.h"
#include "elf_gen.h"
#include "ir_compile.h"
#include "ir_gen.h"
#include "ir_interpret.h"
#include "options.h"
#include <ir_compile.h>
#include <stdio.h>
#include <sys/stat.h>

#define BUFLEN 1024

#if 0
/* NOTE: we need to emit this as a start up for our elf files */
struct start_info {
  struct {
    int32_t c;
    int32_t *v;
  } arg;
  /* NULL byte */
  struct {
    int32_t c;
    int32_t *v;
  } env;
  /* NULL byte */
  struct {
    int32_t c;
    struct {
      int32_t type;
      int32_t val;
    } * v;
  } aux;
  /* NULL byte */
};

static void extract_start_info() {
  int32_t *esp;
  asm inline("movl %%esp, %0" : "=r"(esp));
  struct start_info info = {0};
  info.arg.c = *esp++;
  info.arg.v = esp++;
  info.env.v = ++esp; /* skip NULL byte */
  while (*esp++) {
    info.env.c++;
  }
  info.aux.v = (__typeof__(info.aux.v))++esp; /* skip NULL byte */
  while (*esp != AT_NULL) {
    info.aux.c++;
    esp += 2;
  }
}

static int32_t get_aux_val(struct start_info info, int32_t type) {
  int32_t idx = 0;
  while (idx < info.aux.c && info.aux.v[idx].type != type) {
    idx++;
  }
  return info.aux.v[idx].val;
}
#endif

void make_exe(FILE *fp) {
  int fd = fileno(fp);
  struct stat statbuf = {0};
  fstat(fd, &statbuf);
  fchmod(fd, statbuf.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
}

void write_elf_file(ir_ctx *ir_ctx, FILE *fp) {
  elf_gen_ctx elf_ctx = elf_gen_ctx_init();
  defer { elf_gen_ctx_free(&elf_ctx); };

  program_t *text = gen_program_header(&elf_ctx, ".text",
                                       (Elf32_Phdr){.p_type = PT_LOAD,
                                                    .p_vaddr = 0x08048000,
                                                    .p_flags = PF_R | PF_X});
  gen_section_header(&elf_ctx, ".text",
                     (Elf32_Shdr){.sh_type = SHT_PROGBITS,
                                  .sh_flags = SHF_ALLOC | SHF_EXECINSTR,
                                  .sh_addr = text->header.p_vaddr});
  elf_ctx.elf_header.e_entry = text->header.p_vaddr;

  /* this is an ugly terrible hack. */
  /* since we know that .bss comes directly after .text we hardcode loading
   * SP_REG with the address of .bss. we can't do it before since we don't know
   * how big .text will be and we don't it where we generate the rest of the
   * machine code because i would like to think that code is somewhat clean and
   * i don't want to ruin that. too much. */
  /* preload SP_REG with the location of .bss that we will patch later */
  ctx_push_code(text, 0xb8 + SP_REG);
  vec_push_as_bytes(text, &text->header.p_vaddr);
  ir_ctx_compile((compile_ctx *)text, ir_ctx);

  program_t *bss = gen_program_header(
      &elf_ctx, ".bss",
      (Elf32_Phdr){.p_type = PT_LOAD,
                   .p_vaddr =
                       align_to(text->header.p_vaddr + text->length, 0x1000),
                   .p_flags = PF_R | PF_W});
  bss->length = 30e3;
  gen_section_header(&elf_ctx, ".bss",
                     (Elf32_Shdr){.sh_type = SHT_NOBITS,
                                  .sh_flags = SHF_ALLOC | SHF_WRITE,
                                  .sh_addr = bss->header.p_vaddr});
  *(int32_t *)(text->data + 1) = bss->header.p_vaddr;
  gen_elf_file(fp, &elf_ctx);
  make_exe(fp);
}

int main(int argc, char *argv[]) {
  options_t options = {NULL, NULL, 0};
  if (parse_options(&options, argc, argv)) {
    exit(EXIT_FAILURE);
  }

  defer {
    if (options.output_name != NULL) {
      free(options.output_name);
    }
    if (options.input_name != NULL)
      free(options.input_name);
  };

  char defer_var(auto_free) *buffer = calloc(1, BUFLEN + 1);
  ir_ctx defer_var(ir_ctx_free) ir_ctx = {0};
  vec_reserve(&ir_ctx, BUFLEN);

  FILE defer_var(auto_fclose) *in_file = fopen(options.input_name, "r");
  if (!in_file) {
    fprintf(stderr, "unable to open file '%s'\n", options.input_name);
    return 1;
  }

  size_t read_bytes = 0;
  do {
    read_bytes = fread(buffer, sizeof(char), BUFLEN, in_file);
    buffer[read_bytes] = '\0';
    if (ir_ctx_parse(&ir_ctx, buffer)) {
      return 1;
    }
  } while (read_bytes > 0);

  if (options.dump_ir) {
    ir_ctx_dump_ir(&ir_ctx);
    return 0;
  }

  if (!options.output_name) {
    interpret_ctx_t interpret_ctx = {0};
    vec_reserve(&interpret_ctx, 30e3);
    memset(interpret_ctx.data, 0, interpret_ctx.capacity);
    defer { vec_deinit(&interpret_ctx); };
    /* ir_ctx_dump_bf(ir_ctx); */
    /* ir_ctx_dump_ir(ir_ctx); */
    ir_interpret(&ir_ctx, &interpret_ctx);
  } else {
    FILE defer_var(auto_fclose) *out_file = fopen(options.output_name, "w");
    if (!out_file) {
      fprintf(stderr, "unable to open file '%s'\n", options.output_name);
      return 1;
    }
    write_elf_file(&ir_ctx, out_file);
  }
  return 0;
}
