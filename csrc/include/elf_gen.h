#pragma once

#include "rxi/map.h"
#include "rxi/vec.h"
#include <elf.h>
#include <stdio.h>

typedef struct {
  vec_t(uint8_t);
} elf_gen_ctx;

typedef struct {
  vec_t(uint8_t);
  Elf32_Phdr header;
} program_t;

typedef map_t(program_t) program_map_t;

program_t *gen_prog_header(program_map_t *m, const char *name,
                           Elf32_Phdr header);
void gen_elf_file(FILE *fp, program_map_t *programs);
