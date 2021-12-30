#include "ir_compile.h"
#include "common.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

typedef enum {
  COMPILE_OPERAND_SIZE,
  COMPILE_OK,
} compile_result;

static const char *compile_result_str[] = {
    [COMPILE_OPERAND_SIZE] = "operand size too big",
    [COMPILE_OK] = "ok",
};

typedef enum {
  REG_EAX = 0x00,
  REG_ECX = 0x01,
  REG_EDX = 0x02,
  REG_EBX = 0x03,
  REG_ESP = 0x04, /* used as sp */
  REG_EBP = 0x05, /* used as ip */
  REG_ESI = 0x06,
  REG_EDI = 0x07,
} reg_t;

typedef enum {
  SYS_EXIT = 0x01,
  SYS_READ = 0x03,
  SYS_WRITE = 0x04,
} syscall_t;

typedef struct {
  struct {
    uint8_t mode : 2;
    uint8_t reg : 3;
    uint8_t rm : 3;
  } mod_rm;
} x86_instr;

typedef enum { MOV } instr_t;

#define syscall(n, ...) 0x89, n, ##__VA_ARGS__, 0xcd, 0x80

/* static instr_t sys_write = (instr_t){.code = {0}, .addr = -1}; */
/* static instr_t sys_read = (instr_t){.code = {0}, .addr = -1}; */
/* static instr_t sys_exit = (instr_t){.code = syscall(1), .addr = -1}; */

#define macro_count_args(...)                                                  \
  (sizeof((uint8_t[]){__VA_ARGS__}) / sizeof(uint8_t))

#define ctx_push_code(ctx, ...)                                                \
  vec_pusharr(ctx, ((uint8_t[]){__VA_ARGS__}), macro_count_args(__VA_ARGS__))

static inline compile_result emit_code_tape(compile_ctx *ctx, int64_t arg) {
  if (arg > INT32_MAX || arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  ctx_push_code(ctx, 0);
  return COMPILE_OK;
}

static inline compile_result emit_code_cell(compile_ctx *ctx, int64_t arg) {
  if (arg > INT32_MAX || arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  ctx_push_code(ctx, 0);
  return COMPILE_OK;
}

static inline compile_result emit_code_loop(compile_ctx *ctx, int64_t arg) {
  if (arg > INT32_MAX || arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  ctx_push_code(ctx, 0);
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
static inline compile_result emit_code_write(compile_ctx *ctx, int64_t arg) {
  if (arg > INT32_MAX || arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  return emit_syscall(ctx, SYS_WRITE, STDOUT_FILENO, 0, (int32_t)arg);
}

static inline compile_result emit_code_read(compile_ctx *ctx, int64_t arg) {
  if (arg > INT32_MAX || arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  return emit_syscall(ctx, SYS_READ, STDIN_FILENO, 0, (int32_t)arg);
}

static inline compile_result emit_code_trap(compile_ctx *ctx, int64_t arg) {
  unused(arg);
  ctx_push_code(ctx, 0xcd, 0x03);
  return COMPILE_OK;
}

static inline compile_result emit_code_exit(compile_ctx *ctx, int64_t arg) {
  if (arg > INT32_MAX || arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  return emit_syscall(ctx, SYS_EXIT, (int32_t)arg);
}

size_t ir_ctx_compile(ir_ctx *ir_ctx, compile_ctx *ctx) {
  static compile_result (*code_fn[])(compile_ctx *, int64_t) = {
      [IR_OP_TAPE] = emit_code_trap,  [IR_OP_CELL] = emit_code_trap,
      [IR_OP_LOOP] = emit_code_trap,  [IR_OP_WRITE] = emit_code_write,
      [IR_OP_READ] = emit_code_read,  [IR_OP_SET] = emit_code_trap,
      [IR_OP_PATCH] = emit_code_trap, [IR_OP_MAX] = emit_code_exit,
  };
  /* patch code to have a halt instruction */
  vec_push(ir_ctx, ((ir_op_t){.kind = IR_OP_MAX, .arg = 0}));
  char err_buf[500];
  compile_result err = COMPILE_OK;
  vec_for(ir_ctx, opcode, i) {
    /* remove LOOP flag */
    if ((err = code_fn[iter.opcode.kind & IR_OP_MAX](ctx, iter.opcode.arg) !=
               COMPILE_OK)) {
      ir_fmt_op(iter.opcode, err_buf);
      fprintf(stderr, "compiler error: [%s] %s\n", err_buf,
              compile_result_str[err]);
      return -1;
    }
  }
  return 0;
}
