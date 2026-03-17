/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#if defined(__linux) && defined(READLINE)

#include "saynaa_readline.h"

#include "../runtime/saynaa_vm.h"

#include <ctype.h>

// `Function` is a typedef in readline and a struct in saynaa.
// We rename the readline function to avoid conflict.
#define Function ReadlineFunction
#include <readline/history.h>
#include <readline/readline.h>
#include <readline/tilde.h>
#undef Function

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*****************************************************************************/
/* INTERNAL STATE                                                            */
/*****************************************************************************/

static VM* g_vm = NULL;
static Var g_resolved_obj = VAR_UNDEFINED;
static bool g_is_member_completion = false;

/*****************************************************************************/
/* HISTORY MANAGEMENT                                                        */
/*****************************************************************************/

static void save_history_on_exit(void) {
  const char* home = getenv("HOME");
  char path[1024];
  if (home) {
    snprintf(path, sizeof(path), "%s/.saynaa_history", home);
  } else {
    snprintf(path, sizeof(path), "/tmp/.saynaa_history");
  }
  write_history(path);
}

/*****************************************************************************/
/* HELPER FUNCTIONS                                                          */
/*****************************************************************************/

static Module* find_module_safe(VM* vm, const char* name) {
  if (!vm || !vm->modules)
    return NULL;

  Map* map = vm->modules;
  if (map->count == 0 || !map->entries)
    return NULL;

  for (uint32_t i = 0; i < map->capacity; i++) {
    MapEntry* entry = &map->entries[i];
    if (IS_UNDEF(entry->key))
      continue;

    if (IS_OBJ_TYPE(entry->key, OBJ_STRING)) {
      String* s = AS_STRING(entry->key);
      if (s && s->data && strcmp(s->data, name) == 0) {
        if (IS_OBJ_TYPE(entry->value, OBJ_MODULE)) {
          return (Module*) AS_OBJ(entry->value);
        }
      }
    }
  }
  return NULL;
}

static Var resolve_repl_variable(VM* vm, const char* name) {
  Module* module = find_module_safe(vm, "@(REPL)");
  if (!module)
    return VAR_UNDEFINED;

  for (int i = 0; i < module->global_names.count; i++) {
    uint32_t name_idx = module->global_names.data[i];
    if (name_idx >= module->constants.count)
      continue;

    Var name_var = module->constants.data[name_idx];
    if (IS_OBJ_TYPE(name_var, OBJ_STRING)) {
      String* s = (String*) AS_OBJ(name_var);
      if (s && s->data && strcmp(s->data, name) == 0) {
        if (i < module->globals.count)
          return module->globals.data[i];
      }
    }
  }
  return VAR_UNDEFINED;
}

/*****************************************************************************/
/* COMPLETION GENERATORS                                                     */
/*****************************************************************************/

static char* generate_member_completion(const char* text, int state) {
  static int current_index = 0;
  static int text_len = 0;

  if (!state) {
    current_index = 0;
    text_len = strlen(text);
    if (!g_is_member_completion)
      return NULL;
  }

  if (IS_OBJ_TYPE(g_resolved_obj, OBJ_MODULE)) {
    Module* module = (Module*) AS_OBJ(g_resolved_obj);
    while (current_index < module->global_names.count) {
      uint32_t name_idx = module->global_names.data[current_index++];
      if (name_idx >= module->constants.count)
        continue;

      Var name_var = module->constants.data[name_idx];
      if (IS_OBJ_TYPE(name_var, OBJ_STRING)) {
        String* s = (String*) AS_OBJ(name_var);
        if (s && s->data && strncmp(s->data, text, text_len) == 0) {
          return strdup(s->data);
        }
      }
    }
  }
  return NULL;
}

static char* generate_global_completion(const char* text, int state) {
  static int keyword_idx = 0;
  static int builtin_idx = 0;
  static int repl_global_idx = 0;
  static int text_len = 0;
  static Module* repl_module_ref = NULL;

  static const char* keywords[] = {
      "class", "from", "import", "as",    "function", "fn",     "end",
      "null",  "in",   "is",     "and",   "or",       "not",    "true",
      "false", "this", "super",  "do",    "then",     "while",  "for",
      "if",    "elif", "else",   "break", "continue", "return", NULL};

  if (!state) {
    keyword_idx = builtin_idx = repl_global_idx = 0;
    text_len = strlen(text);
    repl_module_ref = g_vm ? find_module_safe(g_vm, "@(REPL)") : NULL;
  }

  // 1. Keywords
  const char* kw;
  while ((kw = keywords[keyword_idx])) {
    keyword_idx++;
    if (strncmp(kw, text, text_len) == 0)
      return strdup(kw);
  }

  // 2. Builtins
  if (g_vm) {
    while (builtin_idx < g_vm->builtins_count) {
      Closure* cl = g_vm->builtins_funcs[builtin_idx++];
      if (cl && cl->fn && cl->fn->name) {
        const char* name = cl->fn->name;
        if (strncmp(name, text, text_len) == 0) {
          char* buf = (char*) malloc(strlen(name) + 2);
          snprintf(buf, strlen(name) + 2, "%s(", name);
          return buf;
        }
      }
    }
  }

  // 3. User Globals
  if (repl_module_ref) {
    while (repl_global_idx < repl_module_ref->global_names.count) {
      uint32_t name_idx = repl_module_ref->global_names.data[repl_global_idx++];
      if (name_idx >= repl_module_ref->constants.count)
        continue;

      Var name_var = repl_module_ref->constants.data[name_idx];
      if (IS_OBJ_TYPE(name_var, OBJ_STRING)) {
        String* s = (String*) AS_OBJ(name_var);
        if (s && s->data && strncmp(s->data, text, text_len) == 0) {
          return strdup(s->data);
        }
      }
    }
  }

  return NULL;
}

/*****************************************************************************/
/* READLINE CALLBACK                                                         */
/*****************************************************************************/

static char** saynaa_completion_entry(const char* text, int start, int end) {
  rl_attempted_completion_over = 1;
  g_is_member_completion = false;
  g_resolved_obj = VAR_UNDEFINED;

  // Member completion: "obj."
  if (start > 0 && rl_line_buffer[start - 1] == '.') {
    int i = start - 2;
    while (i >= 0 && (isalnum(rl_line_buffer[i]) || rl_line_buffer[i] == '_'))
      i--;
    i++;
    int len = (start - 1) - i;
    if (len > 0) {
      char* name = (char*) malloc(len + 1);
      strncpy(name, rl_line_buffer + i, len);
      name[len] = '\0';

      Var obj = resolve_repl_variable(g_vm, name);
      free(name);

      if (!IS_UNDEF(obj)) {
        g_resolved_obj = obj;
        g_is_member_completion = true;
        return rl_completion_matches(text, generate_member_completion);
      }
    }
  }

  // Global completion
  return rl_completion_matches(text, generate_global_completion);
}

/*****************************************************************************/
/* PUBLIC API                                                                */
/*****************************************************************************/

char* saynaa_readline(VM* vm, const char* prompt) {
  g_vm = vm;
  static bool initialized = false;

  if (!initialized) {
    rl_readline_name = "saynaa";
    rl_completion_append_character = '\0';
    using_history();

    const char* home = getenv("HOME");
    char path[1024];
    if (home) {
      snprintf(path, sizeof(path), "%s/.saynaa_history", home);
    } else {
      snprintf(path, sizeof(path), "/tmp/.saynaa_history");
    }

    read_history(path);
    atexit(save_history_on_exit);

    rl_attempted_completion_function = saynaa_completion_entry;
    rl_bind_key('\t', rl_complete);

    initialized = true;
  }

  char* input = readline(prompt);
  if (input && *input)
    add_history(input);

  return input;
}

void saynaa_saveline(const char* input) {
  if (!input || *input == '\0')
    return;
  add_history(input);
}

#endif // defined(__linux) && defined(READLINE)
