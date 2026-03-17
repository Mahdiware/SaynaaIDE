/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa.h"

#include "../shared/saynaa_common.h"
#include "../utils/saynaa_utils.h"
#include "argparse.h"

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

  // Setup parser
  ArgParser* parser = ap_new("saynaa", "The Saynaa Programming Language");
  ap_add_str(parser, "cmd", 'c', &cmd, "Evaluate and run the passed string.");
  ap_add_bool(parser, "debug", 'd', &debug, "Compile and run the debug version.");
  ap_add_bool(parser, "help", 'h', &help, "Prints this help message and exit.");
  ap_add_bool(parser, "quiet", 'q', &quiet,
              "Don't print version and copyright statement on REPL startup.");
  ap_add_bool(parser, "version", 'v', &version, "Print version and exit.");
  ap_add_bool(parser, "ms", 'm', &millisecond, "Prints runtime millisecond.");

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
    // We have a script file or positional args
    vm_argc = argc - script_idx;
    vm_argv = (const char**) (argv + script_idx);
  } else {
    // REPL mode or -c mode
    vm_argc = 0;
    vm_argv = NULL;
  }

  // Create and initialize the VM.
  VM* vm = initializeVM(vm_argc, vm_argv);

  int exitcode = 0;

  if (cmd != NULL) { // -c "print('foo')"
    Result result = RunString(vm, cmd);
    exitcode = (int) result;

  } else if (script_idx >= argc) { // Run on REPL mode.
    if (!quiet) {
      printf("%s\n", COPYRIGHT);
    }
    exitcode = RunREPL(vm);

  } else { // file ...
    const char* file_path = argv[script_idx];

    // Add script directory to search path
    char script_dir[4096];
    utilResolvePath(script_dir, sizeof(script_dir), file_path, ".");

    size_t len = strlen(script_dir);
    if (len > 0 && script_dir[len - 1] != '/' && script_dir[len - 1] != '\\') {
      if (len + 1 < sizeof(script_dir)) {
        script_dir[len] = '/';
        script_dir[len + 1] = '\0';
      }
    }
    AddSearchPath(vm, script_dir);

    Result result = RunFile(vm, file_path);
    exitcode = (int) result;
  }

  if (millisecond)
    printf("runtime: %.4f ms\n", vm_time(vm));

  // Cleanup
  FreeVM(vm);
  ap_free(parser);
  return exitcode;
}
