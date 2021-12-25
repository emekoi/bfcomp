#include "ir_gen.h"
#include <string.h>

ir_ctx *ir_ctx_new(uint64_t cap) {
  ir_ctx *result = calloc(1, sizeof(ir_ctx) + cap * sizeof(ir_op_t));
  if (result) {
    result->patch = NULL;
    result->cap = cap;
    result->len = 0;
  }
  return result;
}

void ir_ctx_free(ir_ctx *ctx) {
  if (!ctx)
    return;
  while (ctx->patch) {
    ir_patch_t *prev = ctx->patch->prev;
    free(ctx->patch);
    ctx->patch = prev;
  }
  free(ctx);
}

ir_ctx *ir_ctx_grow(ir_ctx *ctx) {
  ir_ctx *result = calloc(1, sizeof(ir_ctx) + ctx->cap * 2 * sizeof(ir_op_t));
  if (result) {
    result->cap = ctx->cap * 2;
    result->len = ctx->len;
    result->patch = ctx->patch;
    memcpy(result->ptr, ctx->ptr, ctx->cap * sizeof(ir_op_t));
    free(ctx);
  }
  return result;
}

static uint64_t count_char(const char *src, char c) {
  const char *ptr = src;
  while (*(++ptr) == c)
    ;
  return (uint64_t)(ptr - src);
}

size_t ir_ctx_parse(ir_ctx **ctx_ptr, const char *src) {
  const char *ptr = src;
  ir_ctx *ctx = *ctx_ptr;
  while (*ptr) {
    if (ir_ctx_full(ctx)) {
      ctx = ir_ctx_grow(ctx);
      *ctx_ptr = ctx;
    }
    uint64_t count = count_char(ptr, *ptr);
#define push_op(op, v)                                                         \
  ir_ctx_push(ctx) = (ir_op_t){.kind = op, .arg = v};                          \
  break
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
      ir_patch_t *next = calloc(1, sizeof(ir_patch_t));
      *next = (ir_patch_t){.addr = ctx->len, .prev = ctx->patch};
      ctx->patch = next;
      ir_ctx_push(ctx) = (ir_op_t){.kind = IR_OP_PATCH, .arg = 0};
      ptr++;
      continue;
    }
    case ']': {
      if (ctx->patch) {
        ir_ctx_idx(ctx, ctx->patch->addr) =
            (ir_op_t){.kind = (ir_op_kind_t)LOOP_START, .arg = ctx->len};
        ir_ctx_push(ctx) =
            (ir_op_t){.kind = (ir_op_kind_t)LOOP_END, .arg = ctx->patch->addr};
        ir_patch_t *tmp = ctx->patch->prev;
        free(ctx->patch);
        ctx->patch = tmp;
        ptr++;
        continue;
      } else {
        fputs("misplaced ']'", stderr);
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
  for (uint64_t i = 0; i < ctx->len; i++) {
    ir_op_t opcode = ir_ctx_idx(ctx, i);
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
                abs_table[0]);
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
  for (uint64_t i = 0; i < ctx->len; i++) {
    ir_op_t opcode = ir_ctx_idx(ctx, i);
    printf("[%ld] %s\n", i, ir_fmt_op(opcode, buf));
  }
}
