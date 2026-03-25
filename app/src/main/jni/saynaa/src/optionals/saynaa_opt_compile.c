/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_optionals.h"

#include "../shared/saynaa_bytecode.h"

#include <time.h>

typedef struct {
  char* source_path;
  char* bytecode_path;
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
  SaynaaBytecodeStatus status = saynaa_bytecode_decode_header(
      source, SAYNAA_BYTECODE_HEADER_SIZE, &header);
  if (status != SAYNAA_BC_OK) {
    SetRuntimeError(vm, "Invalid bytecode header.");
    return false;
  }

  status = saynaa_bytecode_validate_header(&header, 0);
  if (status != SAYNAA_BC_OK) {
    SetRuntimeError(vm, "Invalid bytecode header.");
    return false;
  }

  const uint8_t* payload = source + SAYNAA_BYTECODE_HEADER_SIZE;
  status = saynaa_bytecode_validate_checksum(&header, payload,
                                             header.bytecode_size);
  if (status != SAYNAA_BC_OK) {
    SetRuntimeError(vm, "Bytecode checksum mismatch.");
    return false;
  }

  return saynaa_bytecode_set_payload(vm, &info->bytecode, payload,
                                     header.bytecode_size,
                                     header.flags, header.timestamp) == SAYNAA_BC_OK;
}

static bool compileSourceToPayload(VM* vm, CompileInfo* info,
                                   const char* source, const char* path) {
  Result result = CompileStringToBytecode(vm, source, &info->bytecode);
  if (result != RESULT_SUCCESS) {
    SetRuntimeError(vm, "Failed to compile file to bytecode.");
    return false;
  }

  (void) path;
  return true;
}

void* _compileNew(VM* vm) {
  CompileInfo* info = (CompileInfo*) Realloc(vm, NULL, sizeof(CompileInfo));
  ASSERT(info != NULL, "Realloc failed.");
  info->source_path = NULL;
  info->bytecode_path = NULL;
  saynaa_bytecode_init(&info->bytecode);
  return info;
}

void _compileDelete(VM* vm, void* ptr) {
  CompileInfo* info = (CompileInfo*) ptr;
  if (info == NULL)
    return;
  if (info->source_path)
    Realloc(vm, info->source_path, 0);
  if (info->bytecode_path)
    Realloc(vm, info->bytecode_path, 0);
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

static bool compileEnsureBytecode(VM* vm, CompileInfo* info, const char* path) {
  bool is_bytecode = false;
  char* loaded = LoadScriptAutoDetect(vm, path, &is_bytecode);
  if (loaded == NULL) {
    SetRuntimeError(vm, "Failed to load source file.");
    return false;
  }

  if (is_bytecode) {
    bool ok = loadBytecodeFromBuffer(vm, info, (const uint8_t*) loaded);
    Realloc(vm, loaded, 0);
    if (!ok)
      return false;
    if (info->bytecode_path)
      Realloc(vm, info->bytecode_path, 0);
    info->bytecode_path = copyCString(vm, path);
    return (info->bytecode_path != NULL);
  }

  bool ok = compileSourceToPayload(vm, info, loaded, path);
  Realloc(vm, loaded, 0);
  if (!ok)
    return false;

  if (info->bytecode_path) {
    Realloc(vm, info->bytecode_path, 0);
    info->bytecode_path = NULL;
  }
  return true;
}

saynaa_function(_compileInit, "Compile._init(path:String) -> Null",
                "Initialize and compile a source path to bytecode.") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 1, 1))
    return;

  const char* path = NULL;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  CompileInfo* info = compileGetInfo(vm);
  if (info == NULL)
    return;

  if (info->source_path)
    Realloc(vm, info->source_path, 0);
  info->source_path = copyCString(vm, path);
  if (info->source_path == NULL) {
    SetRuntimeError(vm, "Failed to store compile path.");
    return;
  }

  if (!compileEnsureBytecode(vm, info, path))
    return;
}

saynaa_function(_compileSave, "Compile.save([out:String]) -> String",
                "Return bytecode path or copy it to [out].") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 0, 1))
    return;

  CompileInfo* info = compileGetInfo(vm);
  if (info == NULL)
    return;
  if (info->bytecode.data == NULL || info->bytecode.size == 0) {
    SetRuntimeError(vm, "No bytecode available to save.");
    return;
  }

  if (argc == 0) {
    if (info->bytecode_path == NULL && info->source_path != NULL) {
      info->bytecode_path = saynaa_bytecode_build_path(vm, info->source_path);
      if (info->bytecode_path == NULL) {
        SetRuntimeError(vm, "Failed to build output bytecode path.");
        return;
      }
    }
    if (info->bytecode_path == NULL) {
      SetRuntimeError(vm, "Missing output path.");
      return;
    }
    if (saynaa_bytecode_save(&info->bytecode, info->bytecode_path)
      != SAYNAA_BC_OK) {
      SetRuntimeError(vm, "Failed to write bytecode output.");
      return;
    }
    setSlotString(vm, 0, info->bytecode_path);
    return;
  }

  const char* out = NULL;
  if (!ValidateSlotString(vm, 1, &out, NULL))
    return;

  if (saynaa_bytecode_save(&info->bytecode, out) != SAYNAA_BC_OK) {
    SetRuntimeError(vm, "Failed to write bytecode output.");
    return;
  }

  if (info->bytecode_path == NULL) {
    info->bytecode_path = copyCString(vm, out);
  }
  setSlotString(vm, 0, out);
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
    SetRuntimeError(vm, "Failed to run bytecode file.");
}

saynaa_function(
    _compileBuiltin, "Compile(path:String) -> Compile",
    "Create a Compile instance for [path].") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 1, 1))
    return;

  const char* path = NULL;
  if (!ValidateSlotString(vm, 1, &path, NULL))
    return;

  reserveSlots(vm, 2);

  if (!ImportModule(vm, "compile", 0))
    return; // slot[0] = compile module
  if (!GetAttribute(vm, 0, "Compile", 0))
    return; // slot[0] = Compile class

  setSlotString(vm, 1, path); // slot[1] = path
  if (!NewInstance(vm, 0, 0, 1, 1))
    return; // slot[0] = Compile(path)
}

void registerModuleCompile(VM* vm) {
  if (vm == NULL || vm->builtin_classes[vOBJECT] == NULL)
    return;

  Handle* module = NewModule(vm, "compile");
  Handle* cls = NewClass(vm, "Compile", NULL, module,
                         _compileNew, _compileDelete,
                         "Compile source file to bytecode and run it.");

  ADD_METHOD(cls, "_init", _compileInit, 1);
  ADD_METHOD(cls, "save", _compileSave, -1);
  ADD_METHOD(cls, "run", _compileRun, 0);

  RegisterBuiltinFn(vm, "Compile", _compileBuiltin, 1, DOCSTRING(_compileBuiltin));

  registerModule(vm, module);
  releaseHandle(vm, cls);
  releaseHandle(vm, module);
}
