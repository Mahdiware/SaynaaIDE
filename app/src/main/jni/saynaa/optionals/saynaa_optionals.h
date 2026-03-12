/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include <errno.h>
#include <stdio.h>
#include <string.h>

// FIXME:
// Since this are part of the "standard" libraries, we can include
// internals here using the relative path, however it'll make these
// libraries less "portable" in a sence that these files cannot be just drag
// and dropped into another embedded application where is cannot find the
// relative include.
//

#include "../cli/saynaa.h"
#include "../runtime/saynaa_core.h"
#include "../runtime/saynaa_vm.h"
#include "../shared/saynaa_common.h"
#include "../shared/saynaa_value.h"
#include "../utils/saynaa_utils.h"

#define REPORT_ERRNO(fn) \
  SetRuntimeErrorFmt(vm, "C." #fn " errno:%i - %s.", errno, strerror(errno))

#define REGISTER_FN(module, name, fn, argc) \
  ModuleAddFunction(vm, module, name, fn, argc, DOCSTRING(fn))

#define ADD_METHOD(cls, name, fn, argc) \
  ClassAddMethod(vm, cls, name, fn, argc, DOCSTRING(fn));

#define RET(value) *(vm->fiber->ret) = value;

/*****************************************************************************/
/* SHARED FUNCTIONS                                                          */
/*****************************************************************************/

// These are "public" module functions that can be shared. Since each source
// files in this modules doesn't have their own headers we're using this as
// a common header for every one.

// Register all the the libraries to the VM.
void registerLibs(VM* vm);

// Cleanup registered libraries call this only if the libraries were registered
// with registerLibs() function.
void cleanupLibs(VM* vm);

// The import statement path resolving function. This
// implementation is required by saynaa from it's hosting application
// inorder to use the import statements.
char* pathResolveImport(VM* vm, const char* from, const char* path);

// Write the executable's path to the buffer and return true, if it failed
// it'll return false.
bool osGetExeFilePath(char* buff, int size);
