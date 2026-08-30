/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include "../shared/saynaa_common.h"
#include "../shared/saynaa_internal.h"
#include "../shared/saynaa_value.h"

char* resolvePath(VM* vm, const char* from, const char* path);
bool importScript(VM* vm, Module* module, String* path_resolved, bool is_runtime, bool is_main);

#ifndef NO_DL

// Returns true if the path ends with ".dll" or ".so".
bool isPathDL(String* path);

Module* importDL(VM* vm, String* resolved, String* name);

// Release platform dependent native extension module handle. (*.dll, *.so).
void vmUnloadDlHandle(VM* vm, void* handle);

#endif