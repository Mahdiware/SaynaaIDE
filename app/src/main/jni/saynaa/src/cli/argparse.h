/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum { ARG_BOOL, ARG_STRING, ARG_INT, ARG_FLOAT } ArgType;

typedef struct {
  const char* name;
  char short_name;
  const char* help;
  ArgType type;
  void* value;
} ArgOption;

typedef struct {
  const char* name;
  const char* description;
  ArgOption* options;
  int count;
  int capacity;
} ArgParser;

// Create a new parser
static inline ArgParser* ap_new(const char* name, const char* description) {
  ArgParser* parser = (ArgParser*) malloc(sizeof(ArgParser));
  parser->name = name;
  parser->description = description;
  parser->count = 0;
  parser->capacity = 16;
  parser->options = (ArgOption*) malloc(sizeof(ArgOption) * parser->capacity);
  return parser;
}

static inline void ap_free(ArgParser* parser) {
  free(parser->options);
  free(parser);
}

static inline void ap_add(ArgParser* parser, const char* name, char short_name,
                          ArgType type, void* value, const char* help) {
  if (parser->count >= parser->capacity) {
    parser->capacity *= 2;
    parser->options = (ArgOption*) realloc(parser->options,
                                           sizeof(ArgOption) * parser->capacity);
  }
  ArgOption* opt = &parser->options[parser->count++];
  opt->name = name;
  opt->short_name = short_name;
  opt->type = type;
  opt->value = value;
  opt->help = help;
}

#define ap_add_bool(p, name, s, val, help) \
  ap_add(p, name, s, ARG_BOOL, val, help)
#define ap_add_str(p, name, s, val, help) \
  ap_add(p, name, s, ARG_STRING, val, help)
#define ap_add_int(p, name, s, val, help) ap_add(p, name, s, ARG_INT, val, help)
#define ap_add_float(p, name, s, val, help) \
  ap_add(p, name, s, ARG_FLOAT, val, help)

static inline void ap_print_help(ArgParser* parser) {
  printf("Usage: %s [options] [file] [arguments...]\n\n", parser->name);
  if (parser->description) {
    printf("%s\n\n", parser->description);
  }
  printf("Options:\n");
  for (int i = 0; i < parser->count; i++) {
    ArgOption* opt = &parser->options[i];
    printf("  ");
    if (opt->short_name) {
      printf("-%c, ", opt->short_name);
    } else {
      printf("    ");
    }
    printf("--%-15s %s\n", opt->name, opt->help);
  }
  printf("\n");
}

static inline int ap_parse(ArgParser* parser, int argc, const char** argv) {
  for (int i = 1; i < argc; i++) {
    const char* arg = argv[i];

    if (arg[0] != '-') {
      return i; // Found positional argument (command or file)
    }

    if (strcmp(arg, "--") == 0) {
      return i + 1; // Stop at delimiter
    }

    bool matched = false;
    for (int j = 0; j < parser->count; j++) {
      ArgOption* opt = &parser->options[j];

      bool is_short = (arg[1] != '-' && opt->short_name == arg[1]);
      bool is_long = (arg[1] == '-' && strcmp(arg + 2, opt->name) == 0);

      if (is_short || is_long) {
        matched = true;
        if (opt->type == ARG_BOOL) {
          *(bool*) opt->value = true;
        } else {
          if (i + 1 >= argc) {
            fprintf(stderr, "Error: Option %s requires a value.\n", arg);
            exit(1);
          }
          const char* val = argv[++i];
          if (opt->type == ARG_STRING) {
            *(const char**) opt->value = val;
          } else if (opt->type == ARG_INT) {
            *(int*) opt->value = atoi(val);
          } else if (opt->type == ARG_FLOAT) {
            *(double*) opt->value = atof(val);
          }
        }
        break;
      }
    }

    if (!matched) {
      // Check if it's a combined short option like -dv
      if (arg[1] != '-' && strlen(arg) > 2) {
        // TODO: Implement combined short flags if needed
        // For now, treat as unknown
      }
      fprintf(stderr, "Error: Unknown option %s\n", arg);
      ap_print_help(parser);
      exit(1);
    }
  }
  return argc; // All args parsed as options
}
