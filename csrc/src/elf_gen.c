#include "elf_gen.h"
#include "common.h"
#include <elf.h>
#include <stdint.h>
#include <unistd.h>

#define PAGE_SIZE 4096

/* TODO:
 * - do i want to go nonstandard and write headers then the data?
 *   - we have to read large amounts of the file to get data that we want
 * though.
 * */

#define bswap(sz, val) __builtin_bswap##sz(val)

typedef vec_t(uint8_t) bytes_t;
typedef map_t(const char *) map_str_t;

elf_gen_ctx elf_gen_ctx_init() {
  elf_gen_ctx elf_ctx = {0};
  elf_ctx.elf_header = (Elf32_Ehdr){
      .e_ident =
          {
              [EI_MAG0] = ELFMAG0,
              [EI_MAG1] = ELFMAG1,
              [EI_MAG2] = ELFMAG2,
              [EI_MAG3] = ELFMAG3,
              [EI_CLASS] = ELFCLASS32,
              [EI_DATA] = ELFDATA2LSB,
              [EI_VERSION] = EV_CURRENT,
              [EI_OSABI] = ELFOSABI_NONE,
          },
      .e_type = ET_EXEC,
      .e_machine = EM_386,
      .e_version = EV_CURRENT,
      .e_entry = 0,
      .e_phoff = 0, /* updated in finalize_elf_ctx */
      .e_shoff = 0, /* updated in finalize_elf_ctx */
      .e_flags = 0,
      .e_ehsize = sizeof(Elf32_Ehdr),
      .e_phentsize = 0, /* updated in finalize_elf_ctx */
      .e_phnum = 0,     /* updated in finalize_elf_ctx */
      .e_shentsize = 0, /* updated in finalize_elf_ctx */
      .e_shnum = 0,     /* updated in finalize_elf_ctx */
      .e_shstrndx = SHN_UNDEF,
  };
  return elf_ctx;
}

void elf_gen_ctx_free(elf_gen_ctx *ctx) {
  const char *name = NULL;
  map_iter_t iter = map_iter(&ctx->segments);
  while ((name = map_next(&ctx->segments, &iter))) {
    program_t *program = map_get(&ctx->segments, name);
    vec_deinit(program);
  }
  map_deinit(&ctx->segments);

  iter = map_iter(&ctx->sections);
  while ((name = map_next(&ctx->sections, &iter))) {
    section_t *section = map_get(&ctx->sections, name);
    vec_deinit(section);
  }
  map_deinit(&ctx->sections);
}

program_t *gen_program_header(elf_gen_ctx *ctx, const char *name,
                              Elf32_Phdr header) {
  header.p_align = PAGE_SIZE;
  map_set(&ctx->segments, name, ((program_t){.header = header}));
  return map_get(&ctx->segments, name);
}

section_t *gen_section_header(elf_gen_ctx *ctx, const char *name,
                              Elf32_Shdr header) {
  header.sh_addralign = PAGE_SIZE;
  map_set(&ctx->sections, name, ((section_t){.header = header}));
  return map_get(&ctx->sections, name);
}

int64_t index_of_section(section_map_t *sections, const char *key) {
  const section_t *section = map_get(sections, key);
  const char *name = NULL;
  if (!section)
    return -1;

  map_iter_t iter = map_iter(sections);
  /* we start counting at 1 since there is the NULL header is 0 */
  for (uint64_t idx = 1; (name = map_next(sections, &iter)); idx++) {
    const section_t *test = map_get(sections, name);
    if (test == section)
      return idx;
  }
  return -1;
}

/* after generating our headers we need to go back to calculate the offsets */
void fix_header_offsets(elf_gen_ctx *ctx) {
  const char *name = NULL;
  /* calculate offsets for program segments */
  map_iter_t iter = map_iter(&ctx->segments);
  uint64_t offset = align_to(sizeof(ctx->elf_header) +
                                 ctx->segments.count * sizeof(Elf32_Phdr),
                             PAGE_SIZE);
  while ((name = map_next(&ctx->segments, &iter))) {
    program_t *program = map_get(&ctx->segments, name);
    uint64_t length = program->length;
    if (program->length && program->capacity) {
      program->header.p_filesz = length;
      program->header.p_memsz = length;
      program->header.p_offset = offset;
      /* TODO: automatically allocate these virutal adresses */
      /* program->header.p_vaddr = 0; */
      offset = align_to(offset + length, PAGE_SIZE);
    } else if (program->length) {
      program->header.p_memsz = program->length;
      program->header.p_offset = offset;
    }
  }

  ctx->sections.shstrtab =
      (section_t){.header = (Elf32_Shdr){.sh_name = 1,
                                         .sh_type = SHT_STRTAB,
                                         .sh_addralign = PAGE_SIZE,
                                         .sh_flags = SHF_STRINGS}};

  vec_push(&ctx->sections.shstrtab, '\0');
  vec_pusharr(&ctx->sections.shstrtab, ".shstrtab", strlen(".shstrtab") + 1);

  iter = map_iter(&ctx->sections);
  while ((name = map_next(&ctx->sections, &iter))) {
    section_t *section = map_get(&ctx->sections, name);
    uint64_t length = section->length;
    section->header.sh_name = ctx->sections.shstrtab.length;
    vec_pusharr(&ctx->sections.shstrtab, name, strlen(name) + 1);
    if (section->length && section->header.sh_type & ~SHT_NOBITS) {
      offset = align_to(offset + length, PAGE_SIZE);
    }
    program_t *program = map_get(&ctx->segments, name);
    if (program) {
      section->header.sh_size = program->length;
      section->header.sh_offset = program->header.p_offset;
      section->header.sh_addr = program->header.p_vaddr;
    } else if (section->length) {
      section->header.sh_size = length;
      section->header.sh_offset = offset;
      /* TODO: automatically allocate these virutal adresses */
      /* section->header.sh_addr = 0; */
    }
  }

  ctx->sections.shstrtab.header.sh_size = ctx->sections.shstrtab.length;
  ctx->sections.shstrtab.header.sh_offset = offset;
  /* elf_header->e_shstrndx = elf_header->e_shnum - 1; */

  /* add .shstrtab section and increment section count in elf header*/
  map_set(&ctx->sections, ".shstrtab", ctx->sections.shstrtab);
  ctx->elf_header.e_shstrndx = index_of_section(&ctx->sections, ".shstrtab");
  /* ctx->elf_header.e_shnum++; */
}

void finalize_elf_ctx(elf_gen_ctx *ctx) {
  fix_header_offsets(ctx);

  if (ctx->segments.count) {
    ctx->elf_header.e_phnum = ctx->segments.count;
    ctx->elf_header.e_phentsize = sizeof(Elf32_Phdr);
    ctx->elf_header.e_phoff = sizeof(Elf32_Ehdr);
  }

  /* add 1 to account for NULL section */
  ctx->elf_header.e_shnum = ctx->sections.count + 1;
  ctx->elf_header.e_shentsize = sizeof(Elf32_Shdr);
  ctx->elf_header.e_shoff =
      sizeof(Elf32_Ehdr) + ctx->segments.count * sizeof(Elf32_Phdr);
}

void gen_elf_file(FILE *fp, elf_gen_ctx *ctx) {
  /* fix offsets and whatnot */
  finalize_elf_ctx(ctx);

  /* write the ELF header */
  fwrite(&ctx->elf_header, sizeof(ctx->elf_header), 1, fp);

  /* write the program headers */
  const char *name = NULL;
  map_iter_t iter = map_iter(&ctx->segments);
  while ((name = map_next(&ctx->segments, &iter))) {
    program_t *program = map_get(&ctx->segments, name);
    fwrite(&program->header, sizeof(program->header), 1, fp);
  }

  /* write NULL section */
  fwrite(&(Elf32_Shdr){.sh_type = SHT_NULL}, sizeof(Elf32_Shdr), 1, fp);

  /* write the section headers */
  iter = map_iter(&ctx->sections);
  while ((name = map_next(&ctx->sections, &iter))) {
    section_t *section = map_get(&ctx->sections, name);
    fwrite(&section->header, sizeof(section->header), 1, fp);
  }

  /* write the actual segments */
  iter = map_iter(&ctx->segments);
  while ((name = map_next(&ctx->segments, &iter))) {
    program_t *program = map_get(&ctx->segments, name);
    if (!program->capacity)
      continue;
    fseek(fp, program->header.p_offset, SEEK_SET);
    fwrite(program->data, sizeof(uint8_t), program->length, fp);
  }

  /* write the actual sections */
  iter = map_iter(&ctx->sections);
  while ((name = map_next(&ctx->sections, &iter))) {
    section_t *section = map_get(&ctx->sections, name);
    if (!section->capacity)
      continue;
    fseek(fp, section->header.sh_offset, SEEK_SET);
    fwrite(section->data, sizeof(uint8_t), section->length, fp);
  }
}
