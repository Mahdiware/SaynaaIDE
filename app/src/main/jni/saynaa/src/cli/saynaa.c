/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa.h"

#include "../shared/saynaa_common.h"
#include "../utils/saynaa_utils.h"
#include "argparse.h"

#include "../shared/saynaa_bytecode.h"

#include <stdio.h>

#if defined(__linux__)
#include <signal.h>
static bool typeAgain = 0;

void signalHandler(int signum) {
  ASSERT(signum == SIGINT || signum == SIGTSTP, "Unexpected signal");
  if (!typeAgain) {
    printf("\n\aTo exit, press ^C again or ^D or type exit();\n");
    typeAgain++;
    return;
  }
  exit(0);
}
#endif

// Initialize a new VM instance with default configuration.
static VM* initializeVM(int argc, const char** argv) {
  Configuration config = NewConfiguration();
  config.argument.argc = argc;
  config.argument.argv = argv;

  if (utilIsAtTy(stderr)) {
    config.use_ansi_escape = true;
  }

  VM* vm = NewVM(&config);
  return vm;
}

static void ensureTrailingSlash(char* path, size_t capacity) {
  size_t len = strlen(path);
  if (len == 0)
    return;
  if (path[len - 1] == '/' || path[len - 1] == '\\')
    return;
  if (len + 1 < capacity) {
    path[len] = '/';
    path[len + 1] = '\0';
  }
}

static void addScriptDirToSearchPath(VM* vm, const char* file_path) {
  char script_dir[4096];
  utilResolvePath(script_dir, sizeof(script_dir), file_path, ".");
  ensureTrailingSlash(script_dir, sizeof(script_dir));
  AddSearchPath(vm, script_dir);
}

static Result prepareBytecodeInput(VM* vm, const char* file_path,
                                   const char* output_path, bool bytecode,
                                   bool quiet, const char** run_path,
                                   char** bytecode_path) {
  *run_path = file_path;
  *bytecode_path = NULL;

  if (!bytecode)
    return RESULT_SUCCESS;

  bool is_bytecode = false;
  char* loaded = LoadScriptAutoDetect(vm, file_path, &is_bytecode, NULL);
  if (loaded != NULL)
    Realloc(vm, loaded, 0);

  if (is_bytecode) {
    if (!quiet) {
      fprintf(stderr, "Input is already bytecode.\n");
    }
    return RESULT_COMPILE_ERROR;
  }

  if (output_path != NULL) {
    *bytecode_path = (char*) output_path;
  } else {
    *bytecode_path = saynaa_bytecode_build_path(vm, file_path);
  }

  if (*bytecode_path == NULL) {
    fprintf(stderr, "Error building bytecode path for \"%s\"\n", file_path);
    return RESULT_COMPILE_ERROR;
  }

  SaynaaBytecode bytecode_data;
  saynaa_bytecode_init(&bytecode_data);
  Result result = CompileFileToBytecode(vm, file_path, &bytecode_data);
  if (result != RESULT_SUCCESS) {
    fprintf(stderr, "Error compiling bytecode from \"%s\"\n", file_path);
    if (*bytecode_path != output_path)
      Realloc(vm, *bytecode_path, 0);
    *bytecode_path = NULL;
    return result;
  }

  Result status = saynaa_bytecode_save(&bytecode_data, *bytecode_path);
  saynaa_bytecode_clear(vm, &bytecode_data);
  if (status != RESULT_SUCCESS) {
    fprintf(stderr, "Error writing bytecode to \"%s\"\n", *bytecode_path);
    if (*bytecode_path != output_path)
      Realloc(vm, *bytecode_path, 0);
    *bytecode_path = NULL;
    return RESULT_COMPILE_ERROR;
  }

  *run_path = *bytecode_path;
  return RESULT_SUCCESS;
}

int main(int argc, const char** argv) {
  // Register signal handlers
#if defined(__linux__)
  signal(SIGINT, signalHandler);
  signal(SIGTSTP, signalHandler);
#endif

  // Argument variables
  const char* cmd = NULL;
  bool debug = false;
  bool help = false;
  bool quiet = false;
  bool version = false;
  bool millisecond = false;
  bool bytecode = false;
  bool execute = false;
  const char* output_path = NULL;

  // Setup parser
  ArgParser* parser = ap_new("saynaa", "The Saynaa Programming Language");
  ap_add_str(parser, "cmd", 'c', &cmd, "Evaluate and run the passed string.");
  ap_add_bool(parser, "debug", 'd', &debug, "Compile and run the debug version.");
  ap_add_bool(parser, "help", 'h', &help, "Prints this help message and exit.");
  ap_add_bool(parser, "quiet", 'q', &quiet,
              "Don't print version and copyright statement on REPL startup.");
  ap_add_bool(parser, "version", 'v', &version, "Print version and exit.");
  ap_add_bool(parser, "ms", 'm', &millisecond, "Prints runtime millisecond.");
  ap_add_bool(parser, "bytecode", 'b', &bytecode,
              "Compile source to bytecode (no execution unless -x is set).");
  ap_add_bool(parser, "execute", 'x', &execute,
              "Execute the script (or bytecode if -b is set).");
  ap_add_str(parser, "output", 'o', &output_path,
             "Output path for bytecode when using -b.");

  // Parse arguments
  int script_idx = ap_parse(parser, argc, argv);

  if (help) {
    // Manually print usage since ap_print_help assumes we want to show it.
    // The user asked for "before test.sa its language args", handling help specially if needed.
    // But if help is set, we just print help and exit, ignoring everything else usually.
    // However, if the user ran `saynaa test.sa --help`, `script_idx` would be `test.sa`.
    // If the parser stops at `test.sa`, then `--help` after it is NOT parsed into `help`.
    // So `help` only becomes true if `-h` or `--help` is successfuly parsed BEFORE the script.
    ap_print_help(parser);
    ap_free(parser);
    return 0;
  }

  if (version) {
    fprintf(stdout, "%s %s\n", LANGUAGE, VERSION_STRING);
    ap_free(parser);
    return 0;
  }

  // Construct VM args
  int vm_argc = 0;
  const char** vm_argv = NULL;
  if (script_idx < argc) {
    vm_argc = argc - script_idx;
    vm_argv = (const char**) (argv + script_idx);
  }

  // Create and initialize the VM.
  VM* vm = initializeVM(vm_argc, vm_argv);

  if (!bytecode && !execute) {
    execute = true; // Default behavior: run source.
  }

  if (output_path != NULL && !bytecode) {
    fprintf(stderr, "-o requires -b to produce bytecode output.\n");
    FreeVM(vm);
    ap_free(parser);
    return (int) RESULT_COMPILE_ERROR;
  }

  int exitcode = 0;

  if (cmd != NULL) { // -c "print('foo')"
    if (bytecode) {
      fprintf(stderr, "-b is not supported with -c.\n");
      FreeVM(vm);
      ap_free(parser);
      return (int) RESULT_COMPILE_ERROR;
    }
    Result result = RunString(vm, cmd);
    exitcode = (int) result;

  } else if (script_idx >= argc) { // Run on REPL mode.
    if (!quiet) {
      printf("%s\n", COPYRIGHT);
    }
    exitcode = RunREPL(vm);

  } else { // file ...
    const char* file_path = argv[script_idx];
    addScriptDirToSearchPath(vm, file_path);

    const char* run_path = file_path;
    char* bytecode_path = NULL;
    Result bc_result = prepareBytecodeInput(vm, file_path, output_path, bytecode,
                                            quiet, &run_path, &bytecode_path);
    if (bc_result != RESULT_SUCCESS) {
      FreeVM(vm);
      ap_free(parser);
      return (int) bc_result;
    }

    if (execute) {
      Result result = RunFile(vm, run_path);
      exitcode = (int) result;
    }

    if (bytecode && bytecode_path != output_path) {
      Realloc(vm, bytecode_path, 0);
    }
  }

  if (millisecond)
    printf("runtime: %.4f ms\n", vm_time(vm));

  // Cleanup
  FreeVM(vm);
  ap_free(parser);
  return exitcode;
}
