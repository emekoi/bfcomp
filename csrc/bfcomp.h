#pragma once

#include <elf.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define unused(x) ((void)x)

typedef enum {
  OP_TAPE = 0x0,  /* > | < */
  OP_CELL = 0x1,  /* + | - */
  OP_LOOP = 0x2,  /* [op_t] */
  OP_WRITE = 0x3, /* , */
  OP_READ = 0x4,  /* . */
  OP_SET = 0x5,   /* for statically determined expressions */
  OP_PATCH = 0x6, /* used to mark that this opcode must be patched */
  OP_MAX = 0x7,
} op_kind_t;

typedef enum {
  LOOP_START = OP_LOOP,
  LOOP_END = OP_LOOP | (0x1L << (8 * sizeof(op_kind_t) - 1))
} op_loop_flag;

typedef struct {
  op_kind_t kind;
  int64_t arg;
} op_t;

typedef struct cg_patch {
  uint64_t addr;
  struct cg_patch *prev;
} cg_patch_t;

typedef struct {
  cg_patch_t *patch;
  uint64_t cap;
  uint64_t len;
  op_t ptr[];
} cg_ctx;

/* the index at `ctx->len` is the next empty memory location */
#define cg_ctx_push(ctx) (ctx)->ptr[(ctx)->len++]
/* prefix here since we need to first decrement `ctx->len` to
 * get the last occupied slot
 * */
#define cg_ctx_pop(ctx) (ctx)->ptr[--(ctx)->len]
#define cg_ctx_idx(ctx, idx) (ctx)->ptr[idx]
#define cg_ctx_empty(ctx) ((ctx) && (ctx)->len == 0 && (ctx)->cap != 0)
#define cg_ctx_full(ctx) ((ctx) && (ctx)->len >= (ctx)->cap)

cg_ctx *cg_ctx_new();
void cg_ctx_free(cg_ctx *ctx);
size_t cg_ctx_compile(cg_ctx **ctx, const char *src);
void cg_ctx_dump_bf(cg_ctx *ctx);
void cg_ctx_dump_ir(cg_ctx *ctx);
size_t cg_ctx_interpret(cg_ctx **ctx, char *mem);
