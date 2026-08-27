/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_validate.h"

#include <math.h>

DEFINE_VALIDATE_OBJ(String, OBJ_STRING, "string")
DEFINE_VALIDATE_OBJ(List, OBJ_LIST, "list")
DEFINE_VALIDATE_OBJ(Map, OBJ_MAP, "map")
DEFINE_VALIDATE_OBJ(Closure, OBJ_CLOSURE, "closure")
DEFINE_VALIDATE_OBJ(Fiber, OBJ_FIBER, "fiber")
DEFINE_VALIDATE_OBJ(Class, OBJ_CLASS, "class")
DEFINE_VALIDATE_OBJ(Module, OBJ_MODULE, "module")

// Check if [var] is a numeric value (bool/number) and set [value].
bool isNumeric(Var var, double* value) {
  if (IS_NUM(var)) {
    *value = AS_NUM(var);
    return true;
  }
  if (IS_BOOL(var)) {
    *value = AS_BOOL(var);
    return true;
  }
  return false;
}

// Check if [var] is an integer value and set [value].
bool isInteger(Var var, int64_t* value) {
  double number;
  if (isNumeric(var, &number)) {
    // Note: This check verifies if the double represents an integral value.
    if (floor(number) == number) {
      // Ensure the value fits within a 64-bit integer.
      ASSERT(INT64_MIN <= number && number <= INT64_MAX,
             "Value exceeds 64-bit integer range.");
      *value = (int64_t) (number);
      return true;
    }
  }
  return false;
}

// Check if [var] is a number or boolean. If not, it sets an error and returns false.
bool validateNumeric(VM* vm, Var var, double* value, const char* name) {
  if (isNumeric(var, value))
    return true;
  VM_SET_ERROR(vm, stringFormat(vm, "$ must be a numeric value.", name));
  return false;
}

// Check if [var] is a 64-bit integer. If not, it sets an error and returns false.
bool validateInteger(VM* vm, Var var, int64_t* value, const char* name) {
  if (isInteger(var, value))
    return true;
  VM_SET_ERROR(vm, stringFormat(vm, "$ must be a Number.", name));
  return false;
}

// Index could be larger than a 32-bit integer, but the size is
// limited to a 32-bit unsigned integer.
bool validateIndex(VM* vm, int64_t index, uint32_t size, const char* container) {
  if (index < 0 || size <= index) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ index out of bound.", container));
    return false;
  }
  return true;
}

// Check if the [condition] is true. If not, sets an error and returns false.
bool validateCond(VM* vm, bool condition, const char* err) {
  if (!condition) {
    VM_SET_ERROR(vm, newString(vm, err));
    return false;
  }
  return true;
}