#pragma once

#include "common.h"
#include "ir_gen.h"
#include <stdint.h>

typedef struct {
  vec_t(uint8_t);
  ir_patch_t *patch;
} compile_ctx;

typedef enum {
  COMPILE_OPERAND_SIZE,
  COMPILE_MALFORMED_LOOP,
  COMPILE_OK,
} compile_result;

/* since we know we never do syscalls with more than 3 arguments and we never
 * use the stack so we can use %edi as we see fit. it would make more sense to
 * use %ebp, but that makes encoding instructions a bit harder. e.g inc (%ebp)
 * needs 3 whereas only 2 are needed for other registers. */
#define SP_REG REG_EDI

typedef enum {
  REG_EAX = 0x00,
  REG_ECX = 0x01,
  REG_EDX = 0x02,
  REG_EBX = 0x03,
  REG_ESP = 0x04,
  REG_EBP = 0x05,
  REG_ESI = 0x06,
  REG_EDI = 0x07, /* used as sp */
} reg_t;

/* 32 bit linux syscalls */
typedef enum {
  SYS_EXIT = 0x01,
  SYS_READ = 0x03,
  SYS_WRITE = 0x04,
} syscall_t;

typedef enum {
  MODE_REG_INDIRECT = 0x0,
  MODE_SIB_1 = 0x1,
  MODE_SIB_2 = 0x2,
  MODE_REG_DIRECT = 0x3
} mod_rm_mode;

#define mod_rm(mode, reg, rm) ((mode << 6) | (reg << 3) | rm)

#define macro_count_args(...)                                                  \
  (sizeof((uint8_t[]){__VA_ARGS__}) / sizeof(uint8_t))

#define ctx_push_code(ctx, ...)                                                \
  vec_pusharr(ctx, ((uint8_t[]){__VA_ARGS__}), macro_count_args(__VA_ARGS__))

size_t ir_ctx_compile(compile_ctx *ctx, ir_ctx *ctx_ir);
