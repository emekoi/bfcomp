#pragma once

#include "common.h"
#include "ir_gen.h"
#include <stdint.h>

typedef struct {
  vec_t(uint8_t);
} compile_ctx;

size_t ir_ctx_compile(ir_ctx *ir_ctx, compile_ctx *ctx);
