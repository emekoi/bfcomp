#pragma once

typedef struct {
  char *input_name;
  char *output_name;
  int dump_ir;
} options_t;

int parse_options(options_t *options, int argc, char *const argv[]);
