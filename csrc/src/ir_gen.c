#include "ir_gen.h"
#include "common.h"
#include <string.h>

void ir_ctx_free(ir_ctx *ctx) {
  if (ctx->patch) {
    fprintf(stderr, "unmatched '['");
  }
  while (ctx->patch) {
    ir_patch_t *prev = ctx->patch->prev;
    dmt_free(ctx->patch);
    ctx->patch = prev;
  }
  vec_deinit(ctx);
}

static uint64_t count_char(const char *src, char c) {
  const char *ptr = src;
  while (*(++ptr) == c)
    ;
  return (uint64_t)(ptr - src);
}

size_t ir_ctx_parse(ir_ctx *ctx, const char *src) {
  const char *ptr = src;
  /* } is not significant in macros so we need to wrap struct literals */
#define push_op(op, v)                                                         \
  vec_push(ctx, ((ir_op_t){op, v}));                                           \
  break
  while (*ptr) {
    uint64_t count = count_char(ptr, *ptr);
    switch (*ptr) {
    case ',':
      push_op(IR_OP_READ, count);
    case '.':
      push_op(IR_OP_WRITE, count);
    case '+':
      push_op(IR_OP_CELL, count);
    case '-':
      push_op(IR_OP_CELL, -count);
    case '>':
      push_op(IR_OP_TAPE, count);
    case '<':
      push_op(IR_OP_TAPE, -count);
#undef push_op
    case '[': {
      ir_patch_t *next = dmt_calloc(1, sizeof(ir_patch_t));
      *next = (ir_patch_t){.addr = ctx->length, .prev = ctx->patch};
      ctx->patch = next;
      vec_push(ctx, ((ir_op_t){.kind = IR_OP_PATCH, .arg = 0}));
      ptr++;
      continue;
    }
    case ']': {
      if (ctx->patch) {
        ctx->data[ctx->patch->addr] =
            (ir_op_t){.kind = (ir_op_kind_t)LOOP_START, .arg = ctx->length};
        vec_push(ctx, ((ir_op_t){.kind = (ir_op_kind_t)LOOP_END,
                                 .arg = ctx->patch->addr}));
        ir_patch_t *tmp = ctx->patch->prev;
        dmt_free(ctx->patch);
        ctx->patch = tmp;
        ptr++;
        continue;
      } else {
        fputs("unmatched ']'", stderr);
        return 1;
      }
    }
    }
    ptr += count;
  }
  return 0;
}

#define putcc(c, count)                                                        \
  for (int j = 0; j < count; j++)                                              \
  putchar(c)

void ir_ctx_dump_bf(ir_ctx *ctx) {
  for (uint64_t i = 0; i < ctx->length; i++) {
    ir_op_t opcode = ctx->data[i];
    switch (opcode.kind & IR_OP_MAX) {
    case IR_OP_READ:
      putcc(',', opcode.arg);
      break;
    case IR_OP_WRITE:
      putcc('.', opcode.arg);
      break;
    case IR_OP_CELL: {
      if (opcode.arg < 0) {
        putcc('-', -opcode.arg);
      } else {
        putcc('+', opcode.arg);
      }
      break;
    }
    case IR_OP_TAPE: {
      if (opcode.arg < 0) {
        putcc('<', -opcode.arg);
      } else {
        putcc('>', opcode.arg);
      }
      break;
    }
    case IR_OP_LOOP:
      if (opcode.kind == (ir_op_kind_t)LOOP_END) {
        putchar(']');
      } else {
        putchar('[');
      }
      break;
    default:
      putcc('#', opcode.arg);
    }
  }
  puts("");
}
#undef putcc

const char *ir_fmt_op(ir_op_t opcode, char *buf) {
  static const char *add_sub[2] = {"add", "sub"};
  static const char *jmp_table[2] = {"jz", "jnz"};
  const int64_t abs_table[2] = {opcode.arg, -opcode.arg};

  size_t written = 0;
  switch (opcode.kind & IR_OP_MAX) {
  case IR_OP_READ:
    written = sprintf(buf, "read %ld", opcode.arg);
    break;
  case IR_OP_WRITE:
    written = sprintf(buf, "write %ld", opcode.arg);
    break;
  case IR_OP_CELL:
    written = sprintf(buf, "%s [$tape] %ld", add_sub[opcode.arg < 0],
                      abs_table[opcode.arg < 0]);
    break;
  case IR_OP_TAPE:
    written = sprintf(buf, "%s $tape %ld", add_sub[opcode.arg < 0],
                      abs_table[opcode.arg < 0]);
    break;
  case IR_OP_LOOP:
    written =
        sprintf(buf, "%s %ld", jmp_table[opcode.kind == (ir_op_kind_t)LOOP_END],
                opcode.arg);
    break;
  case IR_OP_MAX:
    written = sprintf(buf, "halt");
    break;
  default:
    written = sprintf(buf, "invalid * %ld", opcode.arg);
  }
  buf[written] = '\0';
  return buf;
}

void ir_ctx_dump_ir(ir_ctx *ctx) {
  char buf[1024] = {0};
  /* for (uint64_t i = 0; i < ctx->len; i++) { */
  vec_for(ctx, opcode, i) {
    printf("[%d] %s\n", iter.i, ir_fmt_op(iter.opcode, buf));
  }
}
