/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_optionals.h"

#include "../shared/saynaa_bytecode.h"

#include <time.h>

typedef struct {
  SaynaaBytecode bytecode;
} CompileInfo;

static char* copyCString(VM* vm, const char* text) {
  size_t length = strlen(text);
  char* out = (char*) Realloc(vm, NULL, length + 1);
  if (out == NULL)
    return NULL;
  memcpy(out, text, length + 1);
  return out;
}

static bool loadBytecodeFromBuffer(VM* vm, CompileInfo* info,
                                   const uint8_t* source) {
  SaynaaBytecodeHeader header;
  Result status = saynaa_bytecode_decode_header(
      source, SAYNAA_BYTECODE_HEADER_SIZE, &header);
  if (status != RESULT_SUCCESS) {
    SetRuntimeError(vm, "Invalid bytecode header.");
    return false;
  }

  status = saynaa_bytecode_validate_header(&header, 0);
  if (status != RESULT_SUCCESS) {
    SetRuntimeError(vm, "Invalid bytecode header.");
    return false;
  }

  const uint8_t* payload = source + SAYNAA_BYTECODE_HEADER_SIZE;
  status = saynaa_bytecode_validate_checksum(&header, payload,
                                             header.bytecode_size);
  if (status != RESULT_SUCCESS) {
    SetRuntimeError(vm, "Bytecode checksum mismatch.");
    return false;
  }

  status = saynaa_bytecode_validate_payload(payload, header.bytecode_size);
  if (status != RESULT_SUCCESS) {
    SetRuntimeError(vm, "Invalid bytecode payload.");
    return false;
  }

  return saynaa_bytecode_set_payload(vm, &info->bytecode, payload,
                                     header.bytecode_size,
                                     header.flags, header.timestamp) == RESULT_SUCCESS;
}

static bool compileSourceToPayload(VM* vm, CompileInfo* info,
                                   const char* source) {
  Result result = CompileStringToBytecode(vm, source, &info->bytecode);
  if (result != RESULT_SUCCESS) {
    SetRuntimeError(vm, "Failed to compile code to bytecode.");
    return false;
  }
  return true;
}

void* _compileNew(VM* vm) {
  CompileInfo* info = (CompileInfo*) Realloc(vm, NULL, sizeof(CompileInfo));
  ASSERT(info != NULL, "Realloc failed.");
  saynaa_bytecode_init(&info->bytecode);
  return info;
}

void _compileDelete(VM* vm, void* ptr) {
  CompileInfo* info = (CompileInfo*) ptr;
  if (info == NULL)
    return;
  saynaa_bytecode_clear(vm, &info->bytecode);
  Realloc(vm, info, 0);
}

static CompileInfo* compileGetInfo(VM* vm) {
  CompileInfo* info = (CompileInfo*) GetThis(vm);
  if (info == NULL) {
    SetRuntimeError(vm, "Compile instance is NULL.");
    return NULL;
  }
  return info;
}

static bool compileSourceCode(VM* vm, CompileInfo* info, const char* code) {
  bool ok = compileSourceToPayload(vm, info, code);
  if (!ok)
    return false;
  return true;
}

saynaa_function(_compileInit, "Compile._init(code:String) -> Null",
                "Initialize and compile a source code to bytecode.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 1, 1))
    return;

  const char* code = NULL;
  if (!ValidateSlotString(vm, 1, &code, NULL))
    return;

  CompileInfo* info = compileGetInfo(vm);
  if (info == NULL)
    return;

  if (!compileSourceCode(vm, info, code))
    return;
}

saynaa_function(_compileSave, "Compile.save(out:String) -> Bool",
                "Return true if bytecode saved successfully into out.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 1, 1))
    return;

  CompileInfo* info = compileGetInfo(vm);
  if (info == NULL)
    return;
  if (info->bytecode.data == NULL || info->bytecode.size == 0) {
    SetRuntimeError(vm, "No bytecode available to save.");
    return;
  }

  const char* out = NULL;
  if (!ValidateSlotString(vm, 1, &out, NULL))
    return;

  if (saynaa_bytecode_save(&info->bytecode, out) != RESULT_SUCCESS) {
    SetRuntimeError(vm, "Failed to write bytecode output.");
    return;
  }

  setSlotBool(vm, 0, true);
}

saynaa_function(_compileRun, "Compile.run() -> Null",
                "Run the compiled bytecode.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 0, 0))
    return;

  CompileInfo* info = compileGetInfo(vm);
  if (info == NULL)
    return;
  if (info->bytecode.data == NULL || info->bytecode.size == 0) {
    SetRuntimeError(vm, "No bytecode available to run.");
    return;
  }
  Result result = saynaa_bytecode_run(vm, &info->bytecode);
  if (result != RESULT_SUCCESS && !VM_HAS_ERROR(vm))
    SetRuntimeError(vm, "Failed to run bytecode.");
}

saynaa_function(
    _compileBuiltin, "Compile(code:String) -> Compile",
    "Create a Compile instance for [path].") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 1, 1))
    return;

  const char* code = NULL;
  if (!ValidateSlotString(vm, 1, &code, NULL))
    return;

  reserveSlots(vm, 2);

  if (!ImportModule(vm, "compile", 0))
    return; // slot[0] = compile module
  if (!GetAttribute(vm, 0, "Compile", 0))
    return; // slot[0] = Compile class

  setSlotString(vm, 1, code); // slot[1] = code
  if (!NewInstance(vm, 0, 0, 1, 1))
    return; // slot[0] = Compile(path)
}

void registerModuleCompile(VM* vm) {
  if (vm == NULL || vm->builtin_classes[vOBJECT] == NULL)
    return;

  Handle* module = NewModule(vm, "compile");
  Handle* cls = NewClass(vm, "Compile", NULL, module,
                         _compileNew, _compileDelete,
                         "Compile source code to bytecode and run it.");

  ADD_METHOD(cls, "_init", _compileInit, 1);
  ADD_METHOD(cls, "save", _compileSave, 1);
  ADD_METHOD(cls, "run", _compileRun, 0);

  RegisterBuiltinFn(vm, "Compile", _compileBuiltin, 1, DOCSTRING(_compileBuiltin));

  registerModule(vm, module);
  releaseHandle(vm, cls);
  releaseHandle(vm, module);
}
