#pragma once

#include "common.h"
#include "ir_gen.h"
#include <stdint.h>

typedef struct {
  vec_t(uint8_t);
} interpret_ctx_t;

size_t ir_interpret(ir_ctx *ir_ctx, interpret_ctx_t *ctx);
