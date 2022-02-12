#include "ir_interpret.h"
#include "common.h"
#include <string.h>

size_t ir_interpret(ir_ctx *ir_ctx, interpret_ctx_t *ctx) {
  /* patch code to have a halt instruction */
  vec_push(ir_ctx, ((ir_op_t){.kind = IR_OP_MAX, .arg = 0}));
  static void *dispatch_table[] = {
      [IR_OP_TAPE] = &&ir_op_tape,
      [IR_OP_CELL] = &&ir_op_cell,
      [IR_OP_LOOP_START] = &&ir_op_loop_start,
      [IR_OP_LOOP_END] = &&ir_op_loop_end,
      [IR_OP_WRITE] = &&ir_op_write,
      [IR_OP_READ] = &&ir_op_read,
      [IR_OP_SET] = &&ir_op_set,
      [IR_OP_MAX] = &&ir_op_halt,
  };
  size_t ip = 0;
  size_t sp = 0;
  char buf[1024] = {0};
#define opcode(ir_op_ip) ir_ctx->data[ir_op_ip]
#define dispatch(label, blk)                                                   \
  label : {                                                                    \
    if (0) {                                                                   \
      printf("[%8ld;%8ld] %s\n", ip, sp, ir_fmt_op(opcode(ip), buf));          \
    } else {                                                                   \
      unused(buf);                                                             \
    }                                                                          \
    blk;                                                                       \
    goto *dispatch_table[opcode(++ip).kind & IR_OP_MAX];                       \
  }

  goto *dispatch_table[opcode(ip).kind];
  /* really bad. we need to optimize this */
  /* dispatch(ir_op_tape, { sp = (sp + opcode(ip).arg) % ctx->capacity; }); */
  dispatch(ir_op_tape, {
    sp += opcode(ip).arg;
    if (sp >= ctx->capacity) {
      sp -= ctx->capacity;
    }
  });
  dispatch(ir_op_cell, { ctx->data[sp] += opcode(ip).arg; });
  /* branchless loads perform much worse */
  dispatch(ir_op_loop_start, {
    int64_t delta = opcode(ip).arg;
    /* ip += (ctx->data[sp] == 0) * delta; */
    if (!ctx->data[sp]) {
      ip += delta;
    }
  });
  dispatch(ir_op_loop_end, {
    int64_t delta = opcode(ip).arg;
    /* ip += (ctx->data[sp] != 0) * delta; */
    if (ctx->data[sp]) {
      ip += delta;
    }
  });
  dispatch(ir_op_write, {
    for (int64_t i = 0; i < opcode(ip).arg; i++) {
      fwrite(ctx->data + sp, sizeof(char), 1, stdout);
    }
  });
  dispatch(ir_op_read, {
    for (int64_t i = 0; i < opcode(ip).arg; i++) {
      fread(ctx->data + sp, sizeof(char), 1, stdin);
    }
  });
  dispatch(ir_op_set, { ctx->data[sp] = opcode(ip).arg; });
  dispatch(ir_op_halt, {
    fflush(stdout);
    return 0;
  });
}
