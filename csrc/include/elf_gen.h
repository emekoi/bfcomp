#pragma once

#include "common.h"
#include <elf.h>
#include <stdio.h>

#define align_to(val, align) (((val) + (align)-1) & ~((align)-1))
#define align_to_down(val, align) ((val) & ~((align)-1))

typedef struct {
  vec_t(uint8_t);
  Elf32_Phdr header;
} program_t;

typedef struct {
  vec_t(uint8_t);
  Elf32_Shdr header;
} section_t;

typedef map_t(program_t) program_map_t;

typedef struct {
  map_t(section_t);
  section_t shstrtab;
} section_map_t;

typedef struct {
  Elf32_Ehdr elf_header;
  program_map_t segments;
  section_map_t sections;
} elf_gen_ctx;

elf_gen_ctx elf_gen_ctx_init();

void elf_gen_ctx_free(elf_gen_ctx *ctx);

program_t *gen_program_header(elf_gen_ctx *ctx, const char *name,
                              Elf32_Phdr header);
section_t *gen_section_header(elf_gen_ctx *ctx, const char *name,
                              Elf32_Shdr header);
void gen_elf_file(FILE *fp, elf_gen_ctx *ctx);
