#include "bfcomp.h"
#include <string.h>

cg_ctx *cg_ctx_new(uint64_t cap) {
  cg_ctx *result = calloc(1, sizeof(cg_ctx) + cap * sizeof(op_t));
  if (result) {
    result->patch = NULL;
    result->cap = cap;
    result->len = 0;
  }
  return result;
}

void cg_ctx_free(cg_ctx *ctx) {
  if (!ctx)
    return;
  while (ctx->patch) {
    cg_patch_t *prev = ctx->patch->prev;
    free(ctx->patch);
    ctx->patch = prev;
  }
  free(ctx);
}

cg_ctx *cg_ctx_grow(cg_ctx *ctx) {
  cg_ctx *result = calloc(1, sizeof(cg_ctx) + ctx->cap * 2 * sizeof(op_t));
  if (result) {
    result->cap = ctx->cap * 2;
    result->len = ctx->len;
    result->patch = ctx->patch;
    memcpy(result->ptr, ctx->ptr, ctx->cap * sizeof(op_t));
    free(ctx);
  }
  return result;
}

uint64_t count_char(const char *src, char c) {
  const char *ptr = src;
  while (*(++ptr) == c)
    ;
  return (uint64_t)(ptr - src);
}

size_t cg_ctx_compile(cg_ctx **ctx_ptr, const char *src) {
  const char *ptr = src;
  cg_ctx *ctx = *ctx_ptr;
  while (*ptr) {
    if (cg_ctx_full(ctx)) {
      ctx = cg_ctx_grow(ctx);
      *ctx_ptr = ctx;
    }
    uint64_t count = count_char(ptr, *ptr);
#define push_op(op, v)                                                         \
  cg_ctx_push(ctx) = (op_t){.kind = op, .arg = v};                             \
  break
    switch (*ptr) {
    case ',':
      push_op(OP_READ, count);
    case '.':
      push_op(OP_WRITE, count);
    case '+':
      push_op(OP_CELL, count);
    case '-':
      push_op(OP_CELL, -count);
    case '>':
      push_op(OP_TAPE, count);
    case '<':
      push_op(OP_TAPE, -count);
#undef push_op
    case '[': {
      cg_patch_t *next = calloc(1, sizeof(cg_patch_t));
      *next = (cg_patch_t){.addr = ctx->len, .prev = ctx->patch};
      ctx->patch = next;
      cg_ctx_push(ctx) = (op_t){.kind = OP_PATCH, .arg = 0};
      ptr++;
      continue;
    }
    case ']': {
      if (ctx->patch) {
        cg_ctx_idx(ctx, ctx->patch->addr) =
            (op_t){.kind = (op_kind_t)LOOP_START, .arg = ctx->len};
        cg_ctx_push(ctx) =
            (op_t){.kind = (op_kind_t)LOOP_END, .arg = ctx->patch->addr};
        cg_patch_t *tmp = ctx->patch->prev;
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

void cg_ctx_dump_bf(cg_ctx *ctx) {
  for (uint64_t i = 0; i < ctx->len; i++) {
    op_t opcode = cg_ctx_idx(ctx, i);
    switch (opcode.kind & OP_MAX) {
    case OP_READ:
      putcc(',', opcode.arg);
      break;
    case OP_WRITE:
      putcc('.', opcode.arg);
      break;
    case OP_CELL: {
      if (opcode.arg < 0) {
        putcc('-', -opcode.arg);
      } else {
        putcc('+', opcode.arg);
      }
      break;
    }
    case OP_TAPE: {
      if (opcode.arg < 0) {
        putcc('<', -opcode.arg);
      } else {
        putcc('>', opcode.arg);
      }
      break;
    }
    case OP_LOOP:
      if (opcode.kind == (op_kind_t)LOOP_END) {
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

const char *fmt_op(op_t opcode, char *buf) {
  static const char *add_sub[2] = {"add", "sub"};
  static const char *jmp_table[2] = {"jz", "jnz"};
  const int64_t abs_table[2] = {opcode.arg, -opcode.arg};

  size_t written = 0;
  switch (opcode.kind & OP_MAX) {
  case OP_READ:
    written = sprintf(buf, "read %ld", opcode.arg);
    break;
  case OP_WRITE:
    written = sprintf(buf, "write %ld", opcode.arg);
    break;
  case OP_CELL:
    written = sprintf(buf, "%s [$tape] %ld", add_sub[opcode.arg < 0],
                      abs_table[opcode.arg < 0]);
    break;
  case OP_TAPE:
    written = sprintf(buf, "%s $tape %ld", add_sub[opcode.arg < 0],
                      abs_table[opcode.arg < 0]);
    break;
  case OP_LOOP:
    written =
        sprintf(buf, "%s %ld", jmp_table[opcode.kind == (op_kind_t)LOOP_END],
                abs_table[0]);
    break;
  case OP_MAX:
    written = sprintf(buf, "halt");
    break;
  default:
    written = sprintf(buf, "invalid * %ld", opcode.arg);
  }
  buf[written] = '\0';
  return buf;
}

void cg_ctx_dump_ir(cg_ctx *ctx) {
  char buf[1024] = {0};
  for (uint64_t i = 0; i < ctx->len; i++) {
    op_t opcode = cg_ctx_idx(ctx, i);
    printf("[%ld] %s\n", i, fmt_op(opcode, buf));
  }
}

size_t cg_ctx_interpret(cg_ctx **ctx, char *mem) {
  /* patch code to have a halt instruction */
  if (cg_ctx_full(*ctx)) {
    *ctx = cg_ctx_grow(*ctx);
  }
  cg_ctx_push(*ctx) = (op_t){.kind = OP_MAX, .arg = 0};
  static void *dispatch_table[] = {
      &&op_tape, &&op_cell, &&op_loop,  &&op_write,
      &&op_read, &&op_set,  &&op_patch, &&op_halt,
  };
  size_t ip = 0;
  size_t sp = 0;
  char buf[1024] = {0};
#define opcode(op_ip) cg_ctx_idx((*ctx), op_ip - 1)
#define dispatch(label, blk)                                                   \
  label : {                                                                    \
    if (0) {                                                                   \
      printf("[%ld;%ld] %s\n", ip, sp, fmt_op(opcode(ip), buf));               \
    } else {                                                                   \
      unused(buf);                                                             \
    }                                                                          \
    blk;                                                                       \
    goto *dispatch_table[opcode(++ip).kind & OP_MAX];                          \
  }

  goto *dispatch_table[opcode(++ip).kind];
  dispatch(op_tape, { sp += opcode(ip).arg; });
  dispatch(op_cell, { mem[sp] += opcode(ip).arg; });
  dispatch(op_loop, {
    int64_t new_ip = opcode(ip).arg;
    if (!mem[sp] && opcode(ip).kind == (op_kind_t)LOOP_START)
      ip = new_ip;
    else if (mem[sp] && opcode(ip).kind == (op_kind_t)LOOP_END)
      ip = new_ip;
  });
  dispatch(op_write,
           { fwrite(mem + sp, sizeof(char), opcode(ip).arg, stdout); });
  dispatch(op_read, { fread(mem + sp, sizeof(char), opcode(ip).arg, stdout); });
  dispatch(op_set, { mem[sp] = opcode(ip).arg; });
  dispatch(op_patch, {
    fputs("invalid instruction in stream", stderr);
    return 1;
  });
  dispatch(op_halt, { return 0; });
}
