#pragma once

#include "common.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define unused(x) ((void)x)

typedef enum {
  IR_OP_TAPE = 0x0,  /* > | < */
  IR_OP_CELL = 0x1,  /* + | - */
  IR_OP_LOOP = 0x2,  /* [ir_op_t] */
  IR_OP_WRITE = 0x3, /* , */
  IR_OP_READ = 0x4,  /* . */
  IR_OP_SET = 0x5,   /* for statically determined expressions */
  IR_OP_PATCH = 0x6, /* used to mark that this opcode must be patched */
  IR_OP_MAX = 0x7,
} ir_op_kind_t;

typedef enum {
  LOOP_START = IR_OP_LOOP,
  LOOP_END = IR_OP_LOOP | (0x1L << (8 * sizeof(ir_op_kind_t) - 1))
} ir_op_flag;

typedef struct {
  ir_op_kind_t kind;
  int64_t arg;
} ir_op_t;

typedef struct ir_patch {
  uint64_t addr;
  struct ir_patch *prev;
} ir_patch_t;

typedef struct {
  ir_patch_t *patch;
  vec_t(ir_op_t);
} ir_ctx;

/* #define ir_ctx_push(ctx) (ctx)->ptr[(ctx)->len++] */
/* #define ir_ctx_pop(ctx) (ctx)->ptr[--(ctx)->len] */
/* #define ir_ctx_idx(ctx, idx) (ctx)->ptr[idx] */
/* #define ir_ctx_empty(ctx) ((ctx) && (ctx)->len == 0 && (ctx)->cap != 0) */
/* #define ir_ctx_full(ctx) ((ctx) && (ctx)->len >= (ctx)->cap) */

/* ir_ctx *ir_ctx_grow(ir_ctx *ctx); */
void ir_ctx_free(ir_ctx *ctx);
size_t ir_ctx_parse(ir_ctx *ctx, const char *src);
void ir_ctx_dump_bf(ir_ctx *ctx);
void ir_ctx_dump_ir(ir_ctx *ctx);

const char *ir_fmt_op(ir_op_t opcode, char *buf);
