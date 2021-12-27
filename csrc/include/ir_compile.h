#pragma once

#include "common.h"
#include "ir_gen.h"
#include <stdint.h>

typedef struct {
  vec_t(uint8_t);
} ir_compile_buf;

size_t ir_ctx_compile(ir_ctx *ctx, char *mem);
