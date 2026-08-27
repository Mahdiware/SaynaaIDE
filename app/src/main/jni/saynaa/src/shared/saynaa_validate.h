/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include "../runtime/saynaa_vm.h"
#include "saynaa_internal.h"
#include "saynaa_value.h"

// Check if [var] is string for argument at [arg]. If not set error and
// return false.
#define DEFINE_VALIDATE_OBJ(m_class, m_type, m_name) \
  bool validateArg##m_class(VM* vm, int arg, m_class** value) { \
    Var var = vm->fiber->ret[arg]; \
    int argc = ((int) (vm->fiber->sp - vm->fiber->ret) - 1); \
    ASSERT(arg > 0 && arg <= argc, OOPS); \
    if (!IS_OBJ(var) || AS_OBJ(var)->type != m_type) { \
      char buff[12]; \
      sprintf(buff, "%d", arg); \
      VM_SET_ERROR(vm, stringFormat(vm, "Expected a " m_name " at argument $.", buff, false)); \
      return false; \
    } \
    *value = (m_class*) AS_OBJ(var); \
    return true; \
  }

#define DECLARE_VALIDATE_OBJ(m_class, m_type, m_name) \
  bool validateArg##m_class(VM* vm, int arg, m_class** value);

DECLARE_VALIDATE_OBJ(String, OBJ_STRING, "string")
DECLARE_VALIDATE_OBJ(List, OBJ_LIST, "list")
DECLARE_VALIDATE_OBJ(Map, OBJ_MAP, "map")
DECLARE_VALIDATE_OBJ(Closure, OBJ_CLOSURE, "closure")
DECLARE_VALIDATE_OBJ(Fiber, OBJ_FIBER, "fiber")
DECLARE_VALIDATE_OBJ(Class, OBJ_CLASS, "class")
DECLARE_VALIDATE_OBJ(Module, OBJ_MODULE, "module")

// Check if [var] is a numeric value (bool/number) and set [value].
bool isNumeric(Var var, double* value);

// Check if [var] is an integer value and set [value].
bool isInteger(Var var, int64_t* value);

// Check if [var] is a number or boolean. If not, it sets an error and returns false.
bool validateNumeric(VM* vm, Var var, double* value, const char* name);

// Check if [var] is a 64-bit integer. If not, it sets an error and returns false.
bool validateInteger(VM* vm, Var var, int64_t* value, const char* name);

// Index could be larger than a 32-bit integer, but the size is
// limited to a 32-bit unsigned integer.
bool validateIndex(VM* vm, int64_t index, uint32_t size, const char* container);

// Check if the [condition] is true. If not, sets an error and returns false.
bool validateCond(VM* vm, bool condition, const char* err);