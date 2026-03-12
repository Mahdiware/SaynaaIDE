/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include "../shared/saynaa_value.h"

typedef enum {
#define OPCODE(name, _, __) OP_##name,
#include "../shared/saynaa_opcodes.h"
#undef OPCODE
} Opcode;

// compiler is a one pass/single pass compiler, which means it
// doesn't go through the basic compilation pipeline such as lexing, parsing
// (AST), analyzing, intermediate code generation, and target codegeneration
// one by one. Instead it'll generate the target code as it reads the source
// (directly from lexing to codegen). Despite it's faster than multipass
// compilers, we're restricted syntax-wise and from compile-time optimizations.
typedef struct Compiler Compiler;

// The options to configure the compilation provided by the command line
// arguments (or other ways the host application provides). For now this
// struct is not publicily visible to the host for the sake of simplicity
// (like lua does) it needs to be a somehow addressed (TODO:).
typedef struct {
  // Compile debug version of the source. In release mode all the assertions
  // and debug informations will be stripped (TODO:) and wll be optimized.
  bool debug;

  // Set to true if compiling in REPL mode, This will print repr version of
  // each evaluated non-null values.
  bool repl_mode;

  // compile at runtime.
  bool runtime;

} CompileOptions;

// Create a new CompilerOptions with the default values and return it.
CompileOptions newCompilerOptions();

// This will take source code as a cstring, compiles it to bytecodes
// and append them to the module's implicit main function. On a successfull
// compilation it'll return RESULT_SUCCESS, otherwise it'll return
// RESULT_COMPILE_ERROR but if repl_mode set in the [options],  and we've
// reached and unexpected EOF it'll return RESULT_UNEXPECTED_EOF.
Result compile(VM* vm, Module* module, const char* source, const CompileOptions* options);

// Mark the heap allocated objects of the compiler at the garbage collection
// called at the marking phase of vmCollectGarbage().
void compilerMarkObjects(VM* vm, Compiler* compiler);