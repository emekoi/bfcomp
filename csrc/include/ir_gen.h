#pragma once

#include "common.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define unused(x) ((void)x)

typedef enum {
  IR_OP_TAPE = 0x0,        /* > | < */
  IR_OP_CELL = 0x1,        /* + | - */
  IR_OP_LOOP_START = 0x2,  /* [ */
  IR_OP_LOOP_END = 0x3,    /* ] */ 
  IR_OP_WRITE = 0x4,       /* , */
  IR_OP_READ = 0x5,        /* . */
  IR_OP_SET = 0x6,         /* for statically determined expressions */
  IR_OP_MAX = 0x7,
} ir_op_kind_t;

typedef struct {
  ir_op_kind_t kind;
  int64_t arg;
} ir_op_t;

typedef struct ir_patch {
  uint64_t addr;
  struct ir_patch *prev;
} ir_patch_t;

typedef struct {
  vec_t(ir_op_t);
  ir_patch_t *patch;
} ir_ctx;

void ir_ctx_free(ir_ctx *ctx);
size_t ir_ctx_parse(ir_ctx *ctx, const char *src);
void ir_ctx_dump_bf(ir_ctx *ctx);
void ir_ctx_dump_ir(ir_ctx *ctx);

const char *ir_fmt_op(ir_op_t opcode, char *buf);
