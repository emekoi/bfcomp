#include "ir_compile.h"
#include "common.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

static const char *compile_result_str[] = {
    [COMPILE_OPERAND_SIZE] = "operand size too big",
    [COMPILE_OK] = "ok",
};

/* use inc for registers, add for mem locs. add is 1 uop less. */
static inline compile_result emit_add_sub(compile_ctx *ctx, mod_rm_mode mode,
                                          ir_op_t op) {
  if (op.arg == 0)
    return COMPILE_OK;
  if (op.arg > INT32_MAX || op.arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  /* x86 instruction encoding is a pain. no wonder people say RISC-V is the
   * future. */
  switch (op.arg) {
  case 1:
    ctx_push_code(ctx, 0x40 + SP_REG);
    break; /* inc %SP_REG */
  case -1:
    ctx_push_code(ctx, 0x48 + SP_REG);
    break; /* dec %SP_REG */
  default:
    if (op.arg > 0) {
      if (op.arg <= 0xFF) {
        /* add %SP_REG, imm8 */
        ctx_push_code(ctx, 0x80, mod_rm(mode, 0, SP_REG), (uint8_t)op.arg);
      } else {
        /* add %SP_REG, imm16/imm32 */
        uint32_t arg = (uint32_t)op.arg;
        ctx_push_code(ctx, 0x81, mod_rm(mode, 0, SP_REG));
        vec_push_as_bytes(ctx, &arg);
      }
    } else {
      if (op.arg <= 0xFF) {
        /* add %SP_REG, imm8 */
        ctx_push_code(ctx, 0x80, mod_rm(mode, 5, SP_REG), (uint8_t)op.arg);
      } else {
        uint32_t arg = -op.arg;
        /* add %SP_REG, imm16/imm32 */
        ctx_push_code(ctx, 0x81, mod_rm(mode, 5, SP_REG));
        vec_push_as_bytes(ctx, &arg);
      }
    }
  }
  return COMPILE_OK;
}

static inline compile_result emit_code_tape(compile_ctx *ctx, ir_op_t op) {
  return emit_add_sub(ctx, MODE_REG_DIRECT, op);
}

static inline compile_result emit_code_cell(compile_ctx *ctx, ir_op_t op) {
  return emit_add_sub(ctx, MODE_REG_INDIRECT, op);
}

static inline compile_result emit_code_loop(compile_ctx *ctx, ir_op_t op) {
  if (op.arg > INT32_MAX || op.arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  /* cmp (%SP_REG), $0 */
  ctx_push_code(ctx, 0x80, mod_rm(MODE_REG_INDIRECT, 0x7, SP_REG), 0x0);
  /* check if we can do a short jump */
  if (op.arg >= INT8_MIN && op.arg <= INT8_MAX) {
    if (op.kind & LOOP_START) {
      /* je imm8 */
      ctx_push_code(ctx, 0x74, (int8_t)op.arg);
    } else {
      /* jne imm8 */
      ctx_push_code(ctx, 0x75, (int8_t)op.arg);
    }
  } else {
    /* we must do a near jump */
    if (op.arg >= INT8_MIN && op.arg <= INT8_MAX) {
      /* mov %eax, imm16/imm32 */
      /* ctx_push_code(ctx, 0xB8 + REG_EAX, op.arg); */
      if (op.kind & LOOP_START) {
        /* je imm16/imm32 */
        ctx_push_code(ctx, 0x0F, 0x84); /* 0x0F is a prefix byte */
        int32_t arg = (int32_t)op.arg;
        vec_push_as_bytes(ctx, &arg);
      } else {
        /* jne imm16/imm32 */
        ctx_push_code(ctx, 0x0F, 0x85); /* 0x0F is a prefix byte */
        int32_t arg = (int32_t)op.arg;
        vec_push_as_bytes(ctx, &arg);
      }
    }
  }
  return COMPILE_OK;
}

static inline compile_result __emit_syscall(compile_ctx *ctx, syscall_t sys,
                                            int32_t n, ...) {
  static const reg_t arg_reg[] = {REG_EBX, REG_ECX, REG_EDX,
                                  REG_ESI, REG_EDI, REG_EBP};
  va_list args = {0};
  va_start(args, n);
  n = n > 6 ? 6 : n;
  /* load syscall into eax */
  ctx_push_code(ctx, 0xb8 + REG_EAX, sys, 0x0, 0x0, 0x0);
  for (int32_t i = 0; i < n; i++) {
    int32_t arg = va_arg(args, int32_t);
    /* emit mov %arg_reg, */
    ctx_push_code(ctx, 0xb8 + arg_reg[i]);
    /* emit the n'th argument */
    /* vec_pusharr(ctx, (uint8_t)&arg, 4); */
    vec_push_as_bytes(ctx, &arg);
  }
  va_end(args);
  /* call the interrupt */
  ctx_push_code(ctx, 0xcd, 0x80);
  return COMPILE_OK;
}

#define emit_syscall(ctx, sys, ...)                                            \
  __emit_syscall(ctx, sys, macro_count_args(__VA_ARGS__), ##__VA_ARGS__)

/* yeah this is kinda broken. it need to mov from one reg to another */
static inline compile_result emit_code_write(compile_ctx *ctx, ir_op_t op) {
  if (op.arg > INT32_MAX || op.arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  return emit_syscall(ctx, SYS_WRITE, STDOUT_FILENO, 0, (int32_t)op.arg);
}

static inline compile_result emit_code_read(compile_ctx *ctx, ir_op_t op) {
  if (op.arg > INT32_MAX || op.arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  return emit_syscall(ctx, SYS_READ, STDIN_FILENO, 0, (int32_t)op.arg);
}

static inline compile_result emit_code_trap(compile_ctx *ctx, ir_op_t op) {
  unused(op);
  ctx_push_code(ctx, 0xcd, 0x03);
  return COMPILE_OK;
}

static inline compile_result emit_code_exit(compile_ctx *ctx, ir_op_t op) {
  if (op.arg > INT32_MAX || op.arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  return emit_syscall(ctx, SYS_EXIT, (int32_t)op.arg);
}

size_t ir_ctx_compile(ir_ctx *ir_ctx, compile_ctx *ctx) {
  static compile_result (*code_fn[])(compile_ctx *, ir_op_t) = {
      [IR_OP_TAPE] = emit_code_tape,  [IR_OP_CELL] = emit_code_cell,
      [IR_OP_LOOP] = emit_code_loop,  [IR_OP_WRITE] = emit_code_write,
      [IR_OP_READ] = emit_code_read,  [IR_OP_SET] = emit_code_trap,
      [IR_OP_PATCH] = emit_code_trap, [IR_OP_MAX] = emit_code_exit,
  };
  /* patch code to have a halt instruction */
  vec_push(ir_ctx, ((ir_op_t){.kind = IR_OP_MAX, .arg = 0}));

  char err_buf[500];
  compile_result err = COMPILE_OK;
  vec_for(ir_ctx, opcode, i) {
    /* remove LOOP flag */
    if ((err = code_fn[iter.opcode.kind & IR_OP_MAX](ctx, iter.opcode) !=
               COMPILE_OK)) {
      ir_fmt_op(iter.opcode, err_buf);
      fprintf(stderr, "compiler error: [%s] %s\n", err_buf,
              compile_result_str[err]);
      return -1;
    }
  }

  return 0;
}
