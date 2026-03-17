/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#ifndef NO_DL

#define DL_IMPLEMENT
#include "saynaa_native.h"

#if defined(_WIN32) || defined(__NT__)
#include <windows.h>

void* osLoadDL(VM* vm, const char* path) {
  HMODULE handle = LoadLibraryA(path);
  if (handle == NULL)
    return NULL;

  InitApiFn init_fn = (InitApiFn) GetProcAddress(handle, API_INIT_FN_NAME);

  if (init_fn == NULL) {
    FreeLibrary(handle);
    return NULL;
  }

  NativeApi api = MakeNativeAPI();
  init_fn(&api);

  return (void*) handle;
}

Handle* osImportDL(VM* vm, void* handle) {
  ExportModuleFn export_fn = (ExportModuleFn) GetProcAddress((HMODULE) handle, EXPORT_FN_NAME);
  if (export_fn == NULL)
    return NULL;

  return export_fn(vm);
}

void osUnloadDL(VM* vm, void* handle) {
  ExportModuleFn cleanup_fn = (ExportModuleFn) GetProcAddress((HMODULE) handle, CLEANUP_FN_NAME);
  if (cleanup_fn != NULL)
    cleanup_fn(vm);
  FreeLibrary((HMODULE) handle);
}

#elif defined(__linux__)
#include <dlfcn.h>

void* osLoadDL(VM* vm, const char* path) {
  // https://man7.org/linux/man-pages/man3/dlopen.3.html
  void* handle = dlopen(path, RTLD_LAZY);
  if (handle == NULL)
    return NULL;

  InitApiFn init_fn = (InitApiFn) dlsym(handle, API_INIT_FN_NAME);
  if (init_fn == NULL) {
    if (dlclose(handle)) { /* TODO: Handle error. */
    }
    dlerror(); // Clear error.
    return NULL;
  }

  NativeApi api = MakeNativeAPI();
  init_fn(&api);

  return handle;
}

Handle* osImportDL(VM* vm, void* handle) {
  ExportModuleFn export_fn = (ExportModuleFn) dlsym(handle, EXPORT_FN_NAME);
  if (export_fn == NULL) {
    dlerror(); // Clear error.
    return NULL;
  }

  Handle* module = export_fn(vm);
  dlerror(); // Clear error.
  return module;
}

void osUnloadDL(VM* vm, void* handle) {
  dlclose(handle);
}

#else

void* osLoadDL(VM* vm, const char* path) {
  return NULL;
}
Handle* osImportDL(VM* vm, void* handle) {
  return NULL;
}
void osUnloadDL(VM* vm, void* handle) {
}

#endif

#endif // NO_DL