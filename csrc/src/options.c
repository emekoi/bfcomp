#include "options.h"
#include "common.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static struct option long_options[] = {{"output", required_argument, NULL, 'o'},
                                       {"compile", no_argument, NULL, 'c'},
                                       {"dump", no_argument, NULL, 'd'},
                                       {NULL, 0, NULL, 0}};

int parse_options(options_t *options, int argc, char *const argv[]) {
  int c = 0;
  int compiling = 0;
  while ((c = getopt_long(argc, argv, "dco:", long_options, NULL)) != -1) {
    switch (c) {
    case 'c':
      compiling = 1;
      break;
    case 'd':
      options->dump_ir = 1;
      break;
    case 'o': {
      size_t len = strlen(optarg);
      options->output_name = realloc(NULL, len + 1);
      strcpy(options->output_name, optarg)[len] = '\0';
      break;
    }
    default:
      fprintf(stderr, "error parsing arguments\n");
      goto error_ret;
    }
  }

  if (optind == argc) {
    fprintf(stderr, "missing file argument\n");
  } else {
    size_t len = strlen(argv[optind]);
    options->input_name = realloc(NULL, len + 1);
    strcpy(options->input_name, argv[optind++])[len] = '\0';
    if (compiling && (options->output_name == NULL)) {
      const char *start = strrchr(options->input_name, '/');
      start = start ? (start + 1) : options->input_name;
      const char *end = strrchr(options->input_name, '.');
      end =
          (end && end != options->input_name) ? end : options->input_name + len;
      options->output_name = realloc(NULL, 1 + (end - start));
      memcpy(options->output_name, start, end - start);
      options->output_name[end - start] = '\0';
    }

    if (optind == argc)
      return 0;

    fprintf(stderr, "extra arguments:");
    while (optind < argc) {
      fprintf(stderr, " '%s'", argv[optind++]);
    }
    fprintf(stderr, "\n");
  }

error_ret:
  if (options->output_name)
    free(options->output_name);
  if (options->input_name)
    free(options->input_name);
  return 1;
}
