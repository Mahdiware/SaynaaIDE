/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_import.h"

#include "../shared/saynaa_bytecode.h"
#include "../shared/saynaa_path.h"
#include "../shared/saynaa_value.h"
#include "saynaa_vm.h"

#if defined(_MSC_VER) || (defined(_WIN32) && defined(__TINYC__))
#include <direct.h>
#include <io.h>

// access() function flag defines for windows.
#define F_OK 0
#define W_OK 2
#define R_OK 4

#define access _access
#define getcwd _getcwd
#define lstat stat
#define stat _stat

#else
#include <unistd.h>
#endif

static inline size_t pathAbs(const char* path, char* buff, size_t buffsz) {
  char cwd[MAX_PATH_LEN];

  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    // TODO: handle error.
  }

  return path_get_absolute(cwd, path, buff, buffsz);
}

static char *checkFileExists(VM *vm, const char *path) {
  if (path == NULL || *path == '\0')
    return NULL;

  if (path_is_file(path)) {
    size_t len = strlen(path);
    char *ret = Realloc(vm, NULL, len + 1);

    if (ret != NULL)
      memcpy(ret, path, len + 1);

    return ret;
  }

  const char *import_suffixes[] = {
    SAYNAA_FILE_EXT,
    SAYNAA_BYTECODE_EXT,
#ifdef _WIN32
    "\\_init" SAYNAA_FILE_EXT,
    "\\_init" SAYNAA_BYTECODE_EXT,
#else
    "/_init" SAYNAA_FILE_EXT,
    "/_init" SAYNAA_BYTECODE_EXT,
#endif
  };

  // Check Saynaa files
  for (size_t i = 0;
       i < sizeof(import_suffixes) / sizeof(import_suffixes[0]);
       i++) {
    char tmp[MAX_PATH_LEN];

    int n = snprintf(
      tmp,
      sizeof(tmp),
      "%s%s",
      path,
      import_suffixes[i]
    );

    if (n < 0 || (size_t)n >= sizeof(tmp))
      continue;

    if (path_is_file(tmp)) {
      char *ret = Realloc(vm, NULL, (size_t)n + 1);

      if (ret != NULL)
        memcpy(ret, tmp, (size_t)n + 1);

      return ret;
    }
  }

#ifndef NO_DL

  // Check native library
#if defined(_WIN32)
  const char *ext = ".dll";
#elif defined(__APPLE__)
  const char *ext = ".dylib";
#elif defined(__linux__)
  const char *ext = ".so";
#else
  const char *ext = NULL;
#endif

  if (ext != NULL) {
    char tmp[MAX_PATH_LEN];

    // name.so / name.dylib / name.dll
    int n = snprintf(
      tmp,
      sizeof(tmp),
      "%s%s",
      path,
      ext
    );

    if (n >= 0 &&
        (size_t)n < sizeof(tmp) &&
        path_is_file(tmp)) {
      char *ret = Realloc(vm, NULL, (size_t)n + 1);

      if (ret != NULL)
        memcpy(ret, tmp, (size_t)n + 1);

      return ret;
    }

#if defined(__linux__) || defined(__APPLE__)

    // libname.so / libname.dylib
    const char *slash = strrchr(path, '/');
    const char *name = slash ? slash + 1 : path;
    size_t dir_len = slash ? (size_t)(slash - path) + 1 : 0;

    n = snprintf(
      tmp,
      sizeof(tmp),
      "%.*slib%s%s",
      (int)dir_len,
      path,
      name,
      ext
    );

    if (n >= 0 &&
        (size_t)n < sizeof(tmp) &&
        path_is_file(tmp)) {
      char *ret = Realloc(vm, NULL, (size_t)n + 1);

      if (ret != NULL)
        memcpy(ret, tmp, (size_t)n + 1);

      return ret;
    }

#endif
  }

#endif

  return NULL;
}

char* resolvePath(VM* vm, const char* from, const char* path) {
  // Buffers to store intermediate path results.
  char buff1[FILENAME_MAX];
  char buff2[FILENAME_MAX];

  char* result = NULL;

  // If the path is absolute, Just normalize and return it. Resolve path will
  // only be absolute when the path is provided from the command line.
  if (path_is_absolute(path)) {
    // buff1 = normalized path. +1 for null terminator.
    path_normalize(path, buff1, sizeof(buff1));

    return checkFileExists(vm, buff1);
  }

  if (from == NULL) { //< [path] is relative to cwd.

    // buff1 = absolute path of [path].
    pathAbs(path, buff1, sizeof(buff1));
    // buff2 = normalized path. +1 for null terminator.
    path_normalize(buff1, buff2, sizeof(buff2));

    result = checkFileExists(vm, buff2);
    if (result != NULL)
      return result;
  } else {
    // Regardless of the platform both '/' and '\\' will be used
    // to indicate its the path of a directory.
    char last = from[strlen(from) - 1];

    // buff1 = absolute path of [from].
    pathAbs(from, buff1, sizeof(buff1));

    // If the [from] path isn't a directory we use the dirname of the from
    // script.
    if (last != '/' && last != '\\' && !path_is_dir(buff1)) {
      size_t from_dir_length = 0;
      path_dirname(buff1, &from_dir_length);
      if (from_dir_length == 0)
        return NULL;
      buff1[from_dir_length] = '\0';
    }

    // buff2 = absolute joined path.
    path_join(buff1, path, buff2, sizeof(buff2));

    // buff1 = normalized absolute path. +1 for null terminator
    path_normalize(buff2, buff1, sizeof(buff1));
    result = checkFileExists(vm, buff2);
    if (result != NULL)
      return result;
  }

  const uint32_t candidates = (uint32_t) vm->search_paths->elements.count;
  for (uint32_t idx = 0; idx < candidates; idx++) {
    const char* from_path = NULL;

    Var sp = vm->search_paths->elements.data[idx];
    ASSERT(IS_OBJ_TYPE(sp, OBJ_STRING), OOPS);
    from_path = AS_STRING(sp)->data;

    // buff1 = absolute joined path.
    path_join(from_path, path, buff1, sizeof(buff1));
    // buff2 = normalized path. +1 for null terminator.
    path_normalize(buff1, buff2, sizeof(buff2));

    result = checkFileExists(vm, buff2);
    if (result != NULL)
      return result;
  }

  return NULL;
}

bool importScript(VM* vm, Module* module, String* path_resolved, bool is_runtime, bool is_main) {
  LoadScriptResult load_result = vm->config.load_script_fn(vm, path_resolved->data);
  char* source = load_result.content;
  if (source == NULL || load_result.status != RESULT_SUCCESS) {
    VM_SET_ERROR(vm, stringFormat(vm, "Error loading module at \"@\"", path_resolved));
    if (source != NULL)
      Realloc(vm, source, 0);
    return false;
  }

  bool is_bytecode = load_result.is_bytecode;
  Result result = RESULT_SUCCESS;
  if (is_bytecode) {
    SaynaaBytecodeHeader header;
    Result status = saynaa_bytecode_decode_header(
        (const uint8_t*) source, SAYNAA_BYTECODE_HEADER_SIZE, &header);
    if (status == RESULT_SUCCESS) {
      const uint8_t* payload = (const uint8_t*) source + SAYNAA_BYTECODE_HEADER_SIZE;
      status = saynaa_bytecode_deserialize_module(vm, module, payload, header.bytecode_size);
    }

    if (status != RESULT_SUCCESS) {
      result = RESULT_COMPILE_ERROR;
      VM_SET_ERROR(vm, stringFormat(vm, "Error compiling module at \"@\"", path_resolved));
    } else {
      initializeModule(vm, module, is_main);
    }
  } else {
    initializeModule(vm, module, is_main);
    CompileOptions options = newCompilerOptions();
    options.runtime = is_runtime;
    result = compile(vm, module, source, &options);
  }

  Realloc(vm, source, 0);

  if (result != RESULT_SUCCESS) {
    if (!VM_HAS_ERROR(vm)) {
      VM_SET_ERROR(vm, stringFormat(vm, "Error compiling module at \"@\"", path_resolved));
    }
    return false;
  }

  return true;
}

#ifndef NO_DL

struct NativeLibCacheEntry {
  NativeLibCacheEntry* prev;
  NativeLibCacheEntry* next;
  char* path;
  void* os_handle;
  uint32_t refs;
};

static void* _dlCacheAlloc(VM* vm, size_t size) {
  return vm->config.realloc_fn(NULL, size, vm->config.user_data);
}

static void _dlCacheFree(VM* vm, void* ptr) {
  if (ptr != NULL) {
    vm->config.realloc_fn(ptr, 0, vm->config.user_data);
  }
}

static NativeLibCacheEntry* _dlCacheFind(VM* vm, const char* resolved_path) {
  for (NativeLibCacheEntry* entry = vm->native_dl_cache; entry != NULL;
       entry = entry->next) {
    if (strcmp(entry->path, resolved_path) == 0) {
      return entry;
    }
  }
  return NULL;
}

static NativeLibCacheEntry* _dlCacheAcquire(VM* vm, String* resolved) {
  NativeLibCacheEntry* entry = _dlCacheFind(vm, resolved->data);
  if (entry != NULL) {
    entry->refs++;
    return entry;
  }

  ASSERT(vm->config.load_dl_fn != NULL, OOPS);
  void* os_handle = vm->config.load_dl_fn(vm, resolved->data);
  if (os_handle == NULL)
    return NULL;

  entry = (NativeLibCacheEntry*) _dlCacheAlloc(vm, sizeof(NativeLibCacheEntry));
  if (entry == NULL) {
    if (vm->config.unload_dl_fn)
      vm->config.unload_dl_fn(vm, os_handle);
    return NULL;
  }

  char* path = (char*) _dlCacheAlloc(vm, (size_t) resolved->length + 1);
  if (path == NULL) {
    _dlCacheFree(vm, entry);
    if (vm->config.unload_dl_fn)
      vm->config.unload_dl_fn(vm, os_handle);
    return NULL;
  }

  memcpy(path, resolved->data, (size_t) resolved->length);
  path[resolved->length] = '\0';

  entry->prev = NULL;
  entry->next = vm->native_dl_cache;
  if (entry->next != NULL)
    entry->next->prev = entry;
  entry->path = path;
  entry->os_handle = os_handle;
  entry->refs = 1;
  vm->native_dl_cache = entry;

  return entry;
}

static void _dlCacheRelease(VM* vm, NativeLibCacheEntry* entry) {
  ASSERT(entry != NULL, OOPS);
  ASSERT(entry->refs > 0, OOPS);

  entry->refs--;
  if (entry->refs > 0)
    return;

  if (entry->prev != NULL) {
    entry->prev->next = entry->next;
  } else {
    vm->native_dl_cache = entry->next;
  }

  if (entry->next != NULL)
    entry->next->prev = entry->prev;

  if (vm->config.unload_dl_fn != NULL)
    vm->config.unload_dl_fn(vm, entry->os_handle);

  _dlCacheFree(vm, entry->path);
  _dlCacheFree(vm, entry);
}

// Returns true if the path ends with ".dll" or ".so".
bool isPathDL(String* path) {
  const char* dlext[] = {
      ".so",
      ".dll",
      NULL,
  };

  for (const char** ext = dlext; *ext != NULL; ext++) {
    size_t ext_len = strlen(*ext);
    if ((size_t) path->length < ext_len)
      continue;

    const char* start = path->data + (path->length - ext_len);
    if (!strncmp(start, *ext, ext_len))
      return true;
  }

  return false;
}

Module* importDL(VM* vm, String* resolved, String* name) {
  if (vm->config.import_dl_fn == NULL) {
    VM_SET_ERROR(vm, newString(vm, "Dynamic library importer not provided."));
    return NULL;
  }

  NativeLibCacheEntry* lib_entry = _dlCacheAcquire(vm, resolved);
  if (lib_entry == NULL) {
    if (!VM_HAS_ERROR(vm))
      VM_SET_ERROR(vm, stringFormat(vm, "Error loading module at \"@\"", resolved));
    return NULL;
  }

  // Since the DL library can use stack via slots api, we need to update
  // ret and then restore it back. We're using offset instead of a pointer
  // because the stack might be reallocated if it grows.
  uintptr_t ret_offset = vm->fiber->ret - vm->fiber->stack;
  vm->fiber->ret = vm->fiber->sp;
  Handle* lhandle = vm->config.import_dl_fn(vm, lib_entry->os_handle);
  vm->fiber->ret = vm->fiber->stack + ret_offset;

  if (lhandle == NULL) {
    vmUnloadDlHandle(vm, lib_entry);
    VM_SET_ERROR(vm, stringFormat(vm, "Error loading module at \"@\"", resolved));
    return NULL;
  }

  if (!IS_OBJ_TYPE(lhandle->value, OBJ_MODULE)) {
    releaseHandle(vm, lhandle);
    vmUnloadDlHandle(vm, lib_entry);
    VM_SET_ERROR(vm, stringFormat(vm,
                                  "Returned handle wasn't a "
                                  "module at \"@\"",
                                  resolved));
    return NULL;
  }

  Module* module = (Module*) AS_OBJ(lhandle->value);
  module->name = name;
  module->path = resolved;
  module->handle = lib_entry;
  vmRegisterModule(vm, module, resolved);

  releaseHandle(vm, lhandle);
  return module;
}

void vmUnloadDlHandle(VM* vm, void* handle) {
  if (handle == NULL)
    return;
  _dlCacheRelease(vm, (NativeLibCacheEntry*) handle);
}
#endif // NO_DL
