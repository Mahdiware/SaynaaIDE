/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_builtins.h"

#include "saynaa_built_functions.h"
#include "saynaa_built_modules.h"
#include "saynaa_built_classes.h"

void register_builtins(VM* vm) {
  initializeBuiltinFunctions(vm);
  initializeBuiltinModules(vm);
  initializeBuiltinClasses(vm);
}