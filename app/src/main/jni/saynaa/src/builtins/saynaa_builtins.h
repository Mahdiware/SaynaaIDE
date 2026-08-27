/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include "../runtime/saynaa_core.h"
#include "../runtime/saynaa_vm.h"
#include "../saynaa/saynaa.h"
#include "../shared/saynaa_common.h"
#include "../shared/saynaa_value.h"
#include "../utils/saynaa_utils.h"

// A convenient macro to get the nth (1 based) argument of the current
// function.
#define ARG(n) (vm->fiber->ret[n])

// Evaluates to the current function's argument count.
#define ARGC ((int) (vm->fiber->sp - vm->fiber->ret) - 1)

// Set return value for the current native function and return.
#define RET(value) \
  do { \
    *(vm->fiber->ret) = value; \
    return; \
  } while (false)

#define RET_ERR(err) \
  do { \
    VM_SET_ERROR(vm, err); \
    RET(VAR_NULL); \
  } while (false)

void register_builtins(VM* vm);