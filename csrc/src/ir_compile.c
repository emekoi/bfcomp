#include "ir_compile.h"
#include "common.h"
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

/* utilize recursion to lower loops.
 * when we hit a loop, write the prelude slice the remaining data, then encode
 * the body, then encode the epilogue */

/* we will be copying the technique llvm uses of linked list of arrays. but with
 * a twist: since the analysis we have to do is fairlt simply we can simply have
 * a separate array for the loops body and links to the code before and after it
 * (code -> loop -> code). we can then encode the loop body and worry about
 * relaxation later. */

/* or we could go with the recursive approach and merely allocate a new array,
 * pass it to the recursive call, then use its size to set up prologue and
 * epilogue. */

static const char *compile_result_str[] = {
    [COMPILE_OPERAND_SIZE] = "operand size too big",
    [COMPILE_MALFORMED_LOOP] = "malformed loop",
    [COMPILE_OK] = "ok",
};

#if 0
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
#endif

/* use inc for registers, add for mem locs. add is 1 uop less. */
static inline compile_result emit_add_sub(compile_ctx *ctx, mod_rm_mode mode,
                                          ir_op_t op) {
  if (op.arg == 0)
    return COMPILE_OK;
  if (op.arg > INT32_MAX || op.arg < INT32_MIN) {
    return COMPILE_OPERAND_SIZE;
  }
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

static inline compile_result emit_code_tape(compile_ctx *ctx, ir_ctx *ctx_ir,
                                            size_t idx) {
  return emit_add_sub(ctx, MODE_REG_DIRECT, ctx_ir->data[idx]);
}

static inline compile_result emit_code_cell(compile_ctx *ctx, ir_ctx *ctx_ir,
                                            size_t idx) {
  return emit_add_sub(ctx, MODE_REG_INDIRECT, ctx_ir->data[idx]);
}

#define ctx_patch_code(ctx, idx, ...)                                          \
  vec_insertarr(ctx, idx, ((uint8_t[]){__VA_ARGS__}),                          \
                macro_count_args(__VA_ARGS__))

static inline compile_result emit_code_loop_start(compile_ctx *ctx, ir_ctx *ir,
                                                  size_t size) {
  unused(ir);

  /* cmp (%SP_REG), $0 */
  ctx_push_code(ctx, 0x80, mod_rm(MODE_REG_INDIRECT, 0x7, SP_REG), 0x0);

  /* check if we can do a short jump (accounting for 2 byte jump) */
  if (size + 2 <= INT8_MAX) {
    /* je imm8 (start of loop) */
    ctx_push_code(ctx, 0x74, (int8_t)(size));
  } else {
    /* je imm16/imm32 (start of loop) */
    ctx_push_code(ctx, 0x0F, 0x84); /* 0x0F is a prefix byte */
    int arg = (int32_t)(size);
    vec_push_as_bytes(ctx, &arg);
  }

  return COMPILE_OK;
}

static inline compile_result emit_code_loop_end(compile_ctx *ctx, ir_ctx *ir,
                                                size_t idx) {
  unused(ir);
  unused(idx);

  /* cmp (%SP_REG), $0 */
  ctx_push_code(ctx, 0x80, mod_rm(MODE_REG_INDIRECT, 0x7, SP_REG), 0x0);

  /* we add 2 to account for encoding short jumps,
   * or the prefix to a long jump */
  int32_t offset = -(ctx->length + 2);

  /* check if we can do a short jump (accounting for 2 byte jump) */
  if (offset >= INT8_MIN) {
    /* jne imm8 (end of loop) */
    ctx_push_code(ctx, 0x75, (int8_t)(offset));
  } else {
    /* jne imm16/imm32 */
    ctx_push_code(ctx, 0x0F, 0x85); /* 0x0F is a prefix byte */
    int32_t arg = -(int32_t)(offset);
    /* jumps are relative to end of instruction,
     * so we must account for the size of the adress itself. */
    arg -= sizeof(arg);
    vec_push_as_bytes(ctx, &arg);
  }

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
static inline compile_result emit_code_write(compile_ctx *ctx, ir_ctx *ctx_ir,
                                             size_t idx) {
  ir_op_t op = ctx_ir->data[idx];
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
    /* we have to do this outside since the macro expansion is not hygenic :(
     */
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

static inline compile_result emit_code_read(compile_ctx *ctx, ir_ctx *ctx_ir,
                                            size_t idx) {
  ir_op_t op = ctx_ir->data[idx];
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

static inline compile_result emit_code_trap(compile_ctx *ctx, ir_ctx *ctx_ir,
                                            size_t idx) {
  unused(ctx_ir);
  unused(idx);
  ctx_push_code(ctx, 0xcd, 0x03);
  return COMPILE_OK;
}

static inline compile_result emit_code_exit(compile_ctx *ctx, ir_ctx *ctx_ir,
                                            size_t idx) {
  ir_op_t op = ctx_ir->data[idx];
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

/* lifted outside the function to avoid stack overflow */
char err_buf[500];

/* lifter outside since there is no reason to have this
 * inside the function body */
static compile_result (*code_fn[])(compile_ctx *, ir_ctx *, size_t) = {
    [IR_OP_TAPE] = emit_code_tape,
    [IR_OP_CELL] = emit_code_cell,
    [IR_OP_LOOP_START] = emit_code_loop_start,
    [IR_OP_LOOP_END] = emit_code_loop_end,
    [IR_OP_WRITE] = emit_code_write,
    [IR_OP_READ] = emit_code_read,
    [IR_OP_SET] = emit_code_trap,
    [IR_OP_MAX] = emit_code_exit,
};

size_t ir_ctx_compile__(compile_ctx *ctx, ir_ctx *ctx_ir, size_t idx) {
  compile_result err = COMPILE_OK;

  for (size_t i = idx; i < ctx_ir->length; i++) {
    ir_op_t opcode = ctx_ir->data[i];
    if (opcode.kind == IR_OP_LOOP_START) {
      compile_ctx loop_code = {0};
      /* an OK approximation maybe? */
      vec_reserve(&loop_code, ctx_ir->data[i].arg * 2);
      err = ir_ctx_compile__(&loop_code, ctx_ir, i + 1);
      if (err == COMPILE_OK) {
        err = emit_code_loop_start(ctx, ctx_ir, loop_code.length);
        vec_extend(ctx, &loop_code);
        /* skip to end of loop body */
        i += opcode.arg;
      }
    } else {
      err = code_fn[opcode.kind](ctx, ctx_ir, i);
    }

    if (err != COMPILE_OK) {
      ir_fmt_op(opcode, err_buf);
      fprintf(stderr, "compiler error: [%s] %s\n", err_buf,
              compile_result_str[err]);
      return err;
    }

    if (opcode.kind == IR_OP_LOOP_END) {
      break;
    }
  }
  return COMPILE_OK;
}

inline size_t ir_ctx_compile(compile_ctx *ctx, ir_ctx *ctx_ir) {
  /* patch code to have a halt instruction */
  vec_push(ctx_ir, ((ir_op_t){.kind = IR_OP_MAX, .arg = 0}));
  return ir_ctx_compile__(ctx, ctx_ir, 0) == COMPILE_OK;
}
