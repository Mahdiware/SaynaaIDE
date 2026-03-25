/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_optionals.h"

static bool debugGetFrame(VM* vm, int level, CallFrame** out_frame, Var** out_end) {
  if (vm == NULL || vm->fiber == NULL || out_frame == NULL || out_end == NULL)
    return false;

  Fiber* fiber = vm->fiber;
  if (fiber->frame_count <= 0) {
    SetRuntimeError(vm, "No VM call frames available.");
    return false;
  }
  if (level < 0) {
    SetRuntimeError(vm, "Level must be >= 0.");
    return false;
  }

  int frame_index = fiber->frame_count - 1 - level;
  if (frame_index < 0) {
    SetRuntimeError(vm, "Invalid call frame level.");
    return false;
  }

  CallFrame* frame = &fiber->frames[frame_index];
  Var* start = frame->rbp + 1; // rbp[0] is return value.
  Var* end = NULL;
  if (frame_index == fiber->frame_count - 1) {
    end = fiber->sp;
  } else {
    end = fiber->frames[frame_index + 1].rbp;
  }

  if (end < start) {
    SetRuntimeError(vm, "Invalid local frame range.");
    return false;
  }

  *out_frame = frame;
  *out_end = end;
  return true;
}

saynaa_function(debugLocals, "debug.locals([level:Number]) -> List",
                "Return a list of locals for the call frame at [level].") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 0, 1))
    return;

  int32_t level = 0;
  if (argc == 1) {
    if (!ValidateSlotInteger(vm, 1, &level))
      return;
  }

  CallFrame* frame = NULL;
  Var* end = NULL;
  if (!debugGetFrame(vm, level, &frame, &end))
    return;

  List* list = newList(vm, 0);
  Var* cursor = frame->rbp + 1;
  for (; cursor < end; cursor++) {
    listAppend(vm, list, *cursor);
  }

  RET(VAR_OBJ(list));
}

saynaa_function(debugGetLocal, "debug.getlocal(level:Number, index:Number) -> Var",
                "Return a local value by [level] and 1-based [index].") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 2, 2))
    return;

  int32_t level = 0;
  if (!ValidateSlotInteger(vm, 1, &level))
    return;

  int32_t index = 0;
  if (!ValidateSlotInteger(vm, 2, &index))
    return;

  if (index <= 0) {
    SetRuntimeError(vm, "Index must be >= 1.");
    return;
  }

  CallFrame* frame = NULL;
  Var* end = NULL;
  if (!debugGetFrame(vm, level, &frame, &end))
    return;

  int32_t count = (int32_t) (end - (frame->rbp + 1));
  if (index > count) {
    SetRuntimeError(vm, "Local index out of range.");
    return;
  }

  RET(frame->rbp[index]);
}

void registerModuleDebug(VM* vm) {
  Handle* debug = NewModule(vm, "debug");

  REGISTER_FN(debug, "locals", debugLocals, -1);
  REGISTER_FN(debug, "getlocal", debugGetLocal, 2);

  registerModule(vm, debug);
  releaseHandle(vm, debug);
}
