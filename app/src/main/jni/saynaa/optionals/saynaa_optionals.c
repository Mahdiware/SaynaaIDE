/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_optionals.h"

// Standard libraries.
void registerModuleMath(VM* vm);
void registerModuleTypes(VM* vm);
void registerModuleTime(VM* vm);
void registerModuleIO(VM* vm);
void registerModulePath(VM* vm);
void registerModuleOS(VM* vm);
void registerModuleJson(VM* vm);
void registerModuleDummy(VM* vm);
void registerModuleTerm(VM* vm);
void registerModuleRegex(VM* vm);

void cleanupModuleTerm(VM* vm);

// Registers the modules.
void registerLibs(VM* vm) {
  registerModuleMath(vm);
  registerModuleTypes(vm);
  registerModuleTime(vm);
  registerModuleIO(vm);
  registerModulePath(vm);
  registerModuleOS(vm);
  registerModuleJson(vm);
  registerModuleDummy(vm);
  registerModuleTerm(vm);
  registerModuleRegex(vm);
}

// Cleanup the modules.
void cleanupLibs(VM* vm) {
  cleanupModuleTerm(vm);
}