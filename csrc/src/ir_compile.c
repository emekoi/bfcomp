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

static const char *reg_names[] = {
    [REG_EAX] = "eax", [REG_ECX] = "ecx", [REG_EDX] = "edx", [REG_EBX] = "ebx",
    [REG_ESP] = "esp", [REG_EBP] = "ebp", [REG_ESI] = "esi", [REG_EDI] = "edi",
};

static const char *mod_rm_names[] = {
    [MODE_REG_INDIRECT] = "indirect",
    [MODE_SIB_1] = "sib_1",
    [MODE_SIB_2] = "sib_2",
    [MODE_REG_DIRECT] = "direct",
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
    /* inc %SP_REG */
    if (mode == MODE_REG_DIRECT)
      ctx_push_code(ctx, 0x40 + SP_REG);
    else
      ctx_push_code(ctx, 0xFF, mod_rm(mode, 0, SP_REG));
    break;
  case -1:
    /* dec %SP_REG */
    if (mode == MODE_REG_DIRECT)
      ctx_push_code(ctx, 0x48 + SP_REG);
    else
      ctx_push_code(ctx, 0xFF, mod_rm(mode, 1, SP_REG));
    break;
  default:
    if (op.arg > 0) {
      if (op.arg <= 0xFF) {
        /* add %SP_REG, imm8 */
        ctx_push_code(ctx, 0x83, mod_rm(mode, 0, SP_REG), (uint8_t)op.arg);
      } else {
        /* add %SP_REG, imm16/imm32 */
        uint32_t arg = (uint32_t)op.arg;
        ctx_push_code(ctx, 0x81, mod_rm(mode, 0, SP_REG));
        vec_push_as_bytes(ctx, &arg);
      }
    } else {
      if (op.arg <= 0xFF) {
        int8_t t = -op.arg;
        /* sub %SP_REG, imm8 */
        ctx_push_code(ctx, 0x83, mod_rm(mode, 5, SP_REG), (uint8_t)(-op.arg));
      } else {
        uint32_t arg = -op.arg;
        /* sub %SP_REG, imm16/imm32 */
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

#define ctx_patch_code(ctx, idx, ...)                                          \
  vec_insertarr(ctx, idx, ((uint8_t[]){__VA_ARGS__}),                          \
                macro_count_args(__VA_ARGS__))

static inline compile_result emit_code_loop(compile_ctx *ctx, ir_op_t op) {
  if (op.arg > INT32_MAX || op.arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }

  /* i'm not smart enough to think of another way that still allows our code to
   * contiguous in memory, so patching will have to do.  */
  if (op.kind == IR_OP_LOOP) {
    /* cmp (%SP_REG), $0 */
    ctx_push_code(ctx, 0x80, mod_rm(MODE_REG_INDIRECT, 0x7, SP_REG), 0x0);
    ir_patch_t *next = dmt_calloc(1, sizeof(ir_patch_t));
    *next = (ir_patch_t){.addr = ctx->length, .prev = ctx->patch};
    ctx->patch = next;
    return COMPILE_OK;
  }

  /* cmp (%SP_REG), $0 */
  ctx_push_code(ctx, 0x80, mod_rm(MODE_REG_INDIRECT, 0x7, SP_REG), 0x0);

  int64_t delta = ctx->length - ctx->patch->addr;

  /* check if we can do a short jump */
  if (delta <= INT8_MAX) {
    /* kind of a ugly hack but we want to skip over the code we just emitted */
    /* jne imm8 */
    ctx_push_code(ctx, 0x75, -(int8_t)(delta + 2));
    delta = ctx->length - ctx->patch->addr;
    /* je imm8 */
    ctx_patch_code(ctx, ctx->patch->addr, 0x74, (int8_t)(delta));
  } else {
    /* we must do a near jump */
    /* jne imm16/imm32 */
    ctx_push_code(ctx, 0x0F, 0x85); /* 0x0F is a prefix byte */
    int32_t arg = -(int32_t)(delta + 6);
    vec_push_as_bytes(ctx, &arg);

    /* je imm16/imm32 */
    /* ctx_push_code(ctx, 0x0F, 0x84); /\* 0x0F is a prefix byte *\/ */
    ctx_patch_code(ctx, ctx->patch->addr, 0x0F, 0x84);
    arg = (int32_t)(delta + 6);
    vec_insert_as_bytes(ctx, ctx->patch->addr + 2, &arg);
  }

  ir_patch_t *tmp = ctx->patch->prev;
  dmt_free(ctx->patch);
  ctx->patch = tmp;

  return COMPILE_OK;
}
static const reg_t syscall_arg_reg[] = {REG_EBX, REG_ECX, REG_EDX,
                                        REG_ESI, REG_EDI, REG_EBP};

#define syscall_const_arg(ctx, n, arg)                                         \
  do {                                                                         \
    ctx_push_code(ctx, 0xb8 + syscall_arg_reg[n]);                             \
    vec_push_as_bytes(ctx, arg);                                               \
  } while (0)

#define syscall_reg_arg(ctx, n, reg)                                           \
  ctx_push_code(ctx, 0x89, mod_rm(MODE_REG_DIRECT, reg, syscall_arg_reg[n]))

/* yeah this is kinda broken. it need to mov from one reg to another */
static inline compile_result emit_code_write(compile_ctx *ctx, ir_op_t op) {
  if (op.arg > INT32_MAX || op.arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }

  for (int64_t i = 0; i < op.arg; i++) {
    int32_t fd = STDOUT_FILENO;
    uint8_t argc = 0;

    /* mov SYS_WRITE, %eax */
    ctx_push_code(ctx, 0xb8 + REG_EAX, SYS_WRITE, 0x0, 0x0, 0x0);
    syscall_const_arg(ctx, argc++, &fd);
    /* mov %edi, %ecx */
    syscall_reg_arg(ctx, argc, SP_REG);
    /* we have to do this outside since the macro expansion is not hygenic :( */
    argc += 1;
    fd = 1;
    /* mov $1, %esi */
    syscall_const_arg(ctx, argc++, &fd);

    /* create a loop. i am feeling kinda of lazy so we just emit a bunch of
     * interrupts. */
    /* int $0x80 */
    ctx_push_code(ctx, 0xcd, 0x80);
  }

  return COMPILE_OK;
}

static inline compile_result emit_code_read(compile_ctx *ctx, ir_op_t op) {
  if (op.arg > INT32_MAX || op.arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
  int32_t fd = STDIN_FILENO;
  uint8_t argc = 0;

  /* mov SYS_READ, %eax */
  ctx_push_code(ctx, 0xb8 + REG_EAX, SYS_READ, 0x0, 0x0, 0x0);
  syscall_const_arg(ctx, argc++, &fd);
  /* mov %edi, %ecx */
  syscall_reg_arg(ctx, argc, SP_REG);
  /* we have to do this outside since the macro expansion is not hygenic :( */
  argc += 1;
  fd = 1;
  /* mov $1, %esi */
  syscall_const_arg(ctx, argc++, &fd);
  /* int $0x80 */
  ctx_push_code(ctx, 0xcd, 0x80);
  return COMPILE_OK;
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
  /* mov SYS_EXIT, %eax */
  ctx_push_code(ctx, 0xb8 + REG_EAX, SYS_EXIT, 0x0, 0x0, 0x0);
  int32_t arg = op.arg;
  syscall_const_arg(ctx, 0, &arg);
  /* int $0x80 */
  ctx_push_code(ctx, 0xcd, 0x80);
  return COMPILE_OK;
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
