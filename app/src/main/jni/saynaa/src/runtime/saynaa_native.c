/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#ifndef NO_DL

#define DL_IMPLEMENT
#include "saynaa_native.h"

#include <stdlib.h>

// Build NativeApi only once per process; each loaded module receives the same
// table pointer during InitApi().
static NativeApi* _nativeApiSingleton(void) {
  static bool initialized = false;
  static NativeApi api;

  if (!initialized) {
    api = MakeNativeAPI();
    initialized = true;
  }

  return &api;
}

#if defined(_WIN32) || defined(__NT__)
#include <windows.h>

typedef void (*CleanupModuleFn)(VM*);

typedef struct {
  HMODULE os_handle;
  ExportModuleFn export_fn;
  CleanupModuleFn cleanup_fn;
} NativeDlHandle;

void* osLoadDL(VM* vm, const char* path) {
  HMODULE handle = LoadLibraryA(path);
  if (handle == NULL)
    return NULL;

  InitApiFn init_fn = (InitApiFn) GetProcAddress(handle, API_INIT_FN_NAME);
  ExportModuleFn export_fn = (ExportModuleFn) GetProcAddress(handle, EXPORT_FN_NAME);
  CleanupModuleFn cleanup_fn = (CleanupModuleFn) GetProcAddress(handle, CLEANUP_FN_NAME);

  if (init_fn == NULL || export_fn == NULL) {
    FreeLibrary(handle);
    return NULL;
  }

  init_fn(_nativeApiSingleton());

  NativeDlHandle* native_handle = (NativeDlHandle*) malloc(sizeof(NativeDlHandle));
  if (native_handle == NULL) {
    FreeLibrary(handle);
    return NULL;
  }

  native_handle->os_handle = handle;
  native_handle->export_fn = export_fn;
  native_handle->cleanup_fn = cleanup_fn;

  return (void*) native_handle;
}

Handle* osImportDL(VM* vm, void* handle) {
  NativeDlHandle* native_handle = (NativeDlHandle*) handle;
  if (native_handle == NULL || native_handle->export_fn == NULL)
    return NULL;

  return native_handle->export_fn(vm);
}

void osUnloadDL(VM* vm, void* handle) {
  NativeDlHandle* native_handle = (NativeDlHandle*) handle;
  if (native_handle == NULL)
    return;

  if (native_handle->cleanup_fn != NULL)
    native_handle->cleanup_fn(vm);
  FreeLibrary(native_handle->os_handle);
  free(native_handle);
}

#elif defined(__linux__)
#include <dlfcn.h>

#ifndef SAYNAA_DL_FLAGS
#define SAYNAA_DL_FLAGS (RTLD_NOW | RTLD_LOCAL)
#endif

typedef void (*CleanupModuleFn)(VM*);

typedef struct {
  void* os_handle;
  ExportModuleFn export_fn;
  CleanupModuleFn cleanup_fn;
} NativeDlHandle;

void* osLoadDL(VM* vm, const char* path) {
  // Lua uses RTLD_NOW and local visibility by default. Allow host override.
  void* os_handle = dlopen(path, SAYNAA_DL_FLAGS);
  if (os_handle == NULL)
    return NULL;

  InitApiFn init_fn = (InitApiFn) dlsym(os_handle, API_INIT_FN_NAME);
  if (init_fn == NULL) {
    if (dlclose(os_handle)) { /* TODO: Handle error. */
    }
    return NULL;
  }

  ExportModuleFn export_fn = (ExportModuleFn) dlsym(os_handle, EXPORT_FN_NAME);
  if (export_fn == NULL) {
    if (dlclose(os_handle)) { /* TODO: Handle error. */
    }
    return NULL;
  }

  CleanupModuleFn cleanup_fn = (CleanupModuleFn) dlsym(os_handle, CLEANUP_FN_NAME);
  // Cleanup hook is optional.

  init_fn(_nativeApiSingleton());

  NativeDlHandle* native_handle = (NativeDlHandle*) malloc(sizeof(NativeDlHandle));
  if (native_handle == NULL) {
    if (dlclose(os_handle)) { /* TODO: Handle error. */
    }
    return NULL;
  }

  native_handle->os_handle = os_handle;
  native_handle->export_fn = export_fn;
  native_handle->cleanup_fn = cleanup_fn;
  return native_handle;
}

Handle* osImportDL(VM* vm, void* handle) {
  NativeDlHandle* native_handle = (NativeDlHandle*) handle;
  if (native_handle == NULL || native_handle->export_fn == NULL)
    return NULL;

  return native_handle->export_fn(vm);
}

void osUnloadDL(VM* vm, void* handle) {
  NativeDlHandle* native_handle = (NativeDlHandle*) handle;
  if (native_handle == NULL)
    return;

  if (native_handle->cleanup_fn != NULL)
    native_handle->cleanup_fn(vm);
  dlclose(native_handle->os_handle);
  free(native_handle);
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