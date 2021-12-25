#include "ir_interpret.h"
#include "rxi/vec.h"
#include <string.h>

size_t ir_ctx_interpret(ir_ctx *ctx, char *mem) {
  /* patch code to have a halt instruction */
  vec_push(ctx, ((ir_op_t){.kind = IR_OP_MAX, .arg = 0}));
  static void *dispatch_table[] = {
      &&ir_op_tape, &&ir_op_cell, &&ir_op_loop,  &&ir_op_write,
      &&ir_op_read, &&ir_op_set,  &&ir_op_patch, &&ir_op_halt,
  };
  size_t ip = 0;
  size_t sp = 0;
  char buf[1024] = {0};
#define opcode(ir_op_ip) ctx->data[ir_op_ip - 1]
#define dispatch(label, blk)                                                   \
  label : {                                                                    \
    if (0) {                                                                   \
      printf("[%ld;%ld] %s\n", ip, sp, ir_fmt_op(opcode(ip), buf));            \
    } else {                                                                   \
      unused(buf);                                                             \
    }                                                                          \
    blk;                                                                       \
    goto *dispatch_table[opcode(++ip).kind & IR_OP_MAX];                       \
  }

  goto *dispatch_table[opcode(++ip).kind];
  dispatch(ir_op_tape, { sp += opcode(ip).arg; });
  dispatch(ir_op_cell, { mem[sp] += opcode(ip).arg; });
  dispatch(ir_op_loop, {
    int64_t new_ip = opcode(ip).arg;
    if (!mem[sp] && opcode(ip).kind == (ir_op_kind_t)LOOP_START)
      ip = new_ip;
    else if (mem[sp] && opcode(ip).kind == (ir_op_kind_t)LOOP_END)
      ip = new_ip;
  });
  dispatch(ir_op_write,
           { fwrite(mem + sp, sizeof(char), opcode(ip).arg, stdout); });
  dispatch(ir_op_read,
           { fread(mem + sp, sizeof(char), opcode(ip).arg, stdout); });
  dispatch(ir_op_set, { mem[sp] = opcode(ip).arg; });
  dispatch(ir_op_patch, {
    fputs("invalid instruction in stream", stderr);
    return 1;
  });
  dispatch(ir_op_halt, { return 0; });
}
