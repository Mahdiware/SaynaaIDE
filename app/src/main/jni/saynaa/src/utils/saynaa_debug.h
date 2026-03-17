/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include "../shared/saynaa_internal.h"
#include "../shared/saynaa_value.h"

// Pretty print compile time error.
void reportCompileTimeError(VM* vm, const char* path, int line, const char* source,
                            const char* at, int length, const char* fmt, va_list args);

// Pretty print runtime error.
void reportRuntimeError(VM* vm, Fiber* fiber);

// Dump opcodes of the given function to the stdout.
void dumpFunctionCode(VM* vm, Function* func);

// Dump the all the global values of the script to the stdout.
void dumpGlobalValues(VM* vm);

// Dump the current (top most) stack call frame to the stdout.
void dumpStackFrame(VM* vm);
