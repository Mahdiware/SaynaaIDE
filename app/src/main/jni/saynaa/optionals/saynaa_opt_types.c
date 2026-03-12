/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_optionals.h"

#include <math.h>

saynaa_function(_typesHashable, "types.hashable(value:Var) -> Bool",
                "Returns true if the [value] is hashable.") {
  // Get argument 1 directly.
  ASSERT(vm->fiber != NULL, OOPS);
  ASSERT(1 < GetSlotsCount(vm), OOPS);
  Var value = vm->fiber->ret[1];

  if (!IS_OBJ(value))
    setSlotBool(vm, 0, true);
  else
    setSlotBool(vm, 0, isObjectHashable(AS_OBJ(value)->type));
}

saynaa_function(_typesHash, "types.hash(value:Var) -> Number", "Returns the hash of the [value]") {
  // Get argument 1 directly.
  ASSERT(vm->fiber != NULL, OOPS);
  ASSERT(1 < GetSlotsCount(vm), OOPS);
  Var value = vm->fiber->ret[1];

  if (IS_OBJ(value) && !isObjectHashable(AS_OBJ(value)->type)) {
    SetRuntimeErrorFmt(vm, "Type '%s' is not hashable.", varTypeName(value));
    return;
  }

  setSlotNumber(vm, 0, varHashValue(value));
}

/*****************************************************************************/
/* BYTE BUFFER                                                               */
/*****************************************************************************/

static void* _bytebuffNew(VM* vm) {
  ByteBuffer* thiz = Realloc(vm, NULL, sizeof(ByteBuffer));
  ByteBufferInit(thiz);
  return thiz;
}

static void _bytebuffDelete(VM* vm, void* buff) {
  Realloc(vm, buff, 0);
}

saynaa_function(
    _bytebuffReserve, "types.ByteBuffer.reserve(count:Number) -> Null",
    "Reserve [count] number of bytes internally. This is use full if the final "
    "size of the buffer is known beforehand to avoid reduce the number of "
    "re-allocations.") {
  double size;
  if (!ValidateSlotNumber(vm, 1, &size))
    return;

  ByteBuffer* thiz = GetThis(vm);
  ByteBufferReserve(thiz, vm, (size_t) size);
}

// buff.fill(data, count)
saynaa_function(
    _bytebuffFill, "types.ByteBuffer.fill(value:Number) -> Null",
    "Fill the buffer with the given byte value. Note that the value must be in "
    "between 0 and 0xff inclusive.") {
  uint32_t n;
  if (!ValidateSlotInteger(vm, 1, &n))
    return;
  if (n < 0x00 || n > 0xff) {
    SetRuntimeErrorFmt(vm,
                       "Expected integer in range "
                       "0x00 to 0xff, got %i.",
                       n);
    return;
  }

  double count;
  if (!ValidateSlotNumber(vm, 1, &count))
    return;

  ByteBuffer* thiz = GetThis(vm);
  ByteBufferFill(thiz, vm, (uint8_t) n, (int) count);
}

saynaa_function(_bytebuffClear, "types.ByteBuffer.clear() -> Null", "Clear the buffer values.") {
  // TODO: Should I also zero or reduce the capacity?
  ByteBuffer* thiz = GetThis(vm);
  thiz->count = 0;
}

// Returns the length of bytes were written.
saynaa_function(
    _bytebuffWrite, "types.ByteBuffer.write(data:Number|String) -> Null",
    "Writes the data to the buffer. If the [data] is a number that should be "
    "in "
    "between 0 and 0xff inclusively. If the [data] is a string all the bytes "
    "of the string will be written to the buffer.") {
  ByteBuffer* thiz = GetThis(vm);

  VarType type = GetSlotType(vm, 1);

  switch (type) {
    case vBOOL:
      ByteBufferWrite(thiz, vm, GetSlotBool(vm, 1) ? 1 : 0);
      setSlotNumber(vm, 0, 1);
      return;

    case vNUMBER:
      {
        uint32_t i;
        if (!ValidateSlotInteger(vm, 1, &i))
          return;
        if (i < 0x00 || i > 0xff) {
          SetRuntimeErrorFmt(vm,
                             "Expected integer in range "
                             "0x00 to 0xff, got %i.",
                             i);
          return;
        }

        ByteBufferWrite(thiz, vm, (uint8_t) i);
        setSlotNumber(vm, 0, 1);
        return;
      }

    case vSTRING:
      {
        uint32_t length;
        const char* str = GetSlotString(vm, 1, &length);
        ByteBufferAddString(thiz, vm, str, length);
        setSlotNumber(vm, 0, (double) length);
        return;
      }

    // TODO:
    case vLIST:
      {
      }

    default:
      break;
  }

  //<  internal function.
  const char* name = getVarTypeName(type);
  SetRuntimeErrorFmt(vm, "Object %s cannot be written to ByteBuffer.", name);
}

saynaa_function(_bytebuffSubscriptGet, "types.ByteBuffer.[](index:Number)", "") {
  double index;
  if (!ValidateSlotNumber(vm, 1, &index))
    return;
  if (floor(index) != index) {
    SetRuntimeError(vm, "Expected an integer but got number.");
    return;
  }

  ByteBuffer* thiz = GetThis(vm);

  if (index < 0 || index >= thiz->count) {
    SetRuntimeError(vm, "Index out of bound");
    return;
  }

  setSlotNumber(vm, 0, thiz->data[(uint32_t) index]);
}

saynaa_function(_bytebuffSubscriptSet,
                "types.ByteBuffer.[]=(index:Number, value:Number)", "") {
  double index, value;
  if (!ValidateSlotNumber(vm, 1, &index))
    return;
  if (!ValidateSlotNumber(vm, 2, &value))
    return;

  if (floor(index) != index) {
    SetRuntimeError(vm, "Expected an integer but got float.");
    return;
  }
  if (floor(value) != value) {
    SetRuntimeError(vm, "Expected an integer but got float.");
    return;
  }

  ByteBuffer* thiz = GetThis(vm);

  if (index < 0 || index >= thiz->count) {
    SetRuntimeError(vm, "Index out of bound");
    return;
  }

  if (value < 0x00 || value > 0xff) {
    SetRuntimeError(vm, "Value should be in range 0x00 to 0xff.");
    return;
  }

  thiz->data[(uint32_t) index] = (uint8_t) value;
}

saynaa_function(_bytebuffString, "types.ByteBuffer.string() -> String",
                "Returns the buffered values as String.") {
  ByteBuffer* thiz = GetThis(vm);
  setSlotStringLength(vm, 0, thiz->data, thiz->count);
}

saynaa_function(_bytebuffCount, "types.ByteBuffer.count() -> Number",
                "Returns the number of bytes that"
                " have been written to the buffer.") {
  ByteBuffer* thiz = GetThis(vm);
  setSlotNumber(vm, 0, thiz->count);
}

/*****************************************************************************/
/* VECTOR                                                                    */
/*****************************************************************************/

typedef struct {
  double x, y, z;
} Vector;

static void* _vectorNew(VM* vm) {
  Vector* vec = Realloc(vm, NULL, sizeof(Vector));
  memset(vec, 0, sizeof(Vector));
  return vec;
}

static void _vectorDelete(VM* vm, void* vec) {
  Realloc(vm, vec, 0);
}

saynaa_function(_vectorInit, "types.Vector._init()", "") {
  int argc = GetArgc(vm);
  if (!CheckArgcRange(vm, argc, 0, 3))
    return;

  double x, y, z;
  Vector* vec = GetThis(vm);

  if (argc == 0)
    return;
  if (argc >= 1) {
    if (!ValidateSlotNumber(vm, 1, &x))
      return;
    vec->x = x;
  }

  if (argc >= 2) {
    if (!ValidateSlotNumber(vm, 2, &y))
      return;
    vec->y = y;
  }

  if (argc == 3) {
    if (!ValidateSlotNumber(vm, 3, &z))
      return;
    vec->z = z;
  }
}

saynaa_function(_vectorGetter, "types.Vector._getter()", "") {
  const char* name;
  uint32_t length;
  if (!ValidateSlotString(vm, 1, &name, &length))
    return;

  Vector* vec = GetThis(vm);
  if (length == 1) {
    if (*name == 'x') {
      setSlotNumber(vm, 0, vec->x);
      return;
    } else if (*name == 'y') {
      setSlotNumber(vm, 0, vec->y);
      return;
    } else if (*name == 'z') {
      setSlotNumber(vm, 0, vec->z);
      return;
    }
  }
}

saynaa_function(_vectorSetter, "types.Vector._setter()", "") {
  const char* name;
  uint32_t length;
  if (!ValidateSlotString(vm, 1, &name, &length))
    return;

  Vector* vec = GetThis(vm);

  if (length == 1) {
    if (*name == 'x') {
      double val;
      if (!ValidateSlotNumber(vm, 2, &val))
        return;
      vec->x = val;
      return;
    } else if (*name == 'y') {
      double val;
      if (!ValidateSlotNumber(vm, 2, &val))
        return;
      vec->y = val;
      return;
    } else if (*name == 'z') {
      double val;
      if (!ValidateSlotNumber(vm, 2, &val))
        return;
      vec->z = val;
      return;
    }
  }
}

saynaa_function(_vectorRepr, "types.Vector._repr()", "") {
  Vector* vec = GetThis(vm);
  setSlotStringFmt(vm, 0, "[%g, %g, %g]", vec->x, vec->y, vec->z);
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleTypes(VM* vm) {
  Handle* types = NewModule(vm, "types");

  REGISTER_FN(types, "hashable", _typesHashable, 1);
  REGISTER_FN(types, "hash", _typesHash, 1);

  Handle* cls_byte_buffer = NewClass(
      vm, "ByteBuffer", NULL, types, _bytebuffNew, _bytebuffDelete,
      "A simple dynamically allocated byte buffer type. This can be used for "
      "constructing larger strings without allocating and adding smaller "
      "intermeidate strings.");

  ADD_METHOD(cls_byte_buffer, "[]", _bytebuffSubscriptGet, 1);
  ADD_METHOD(cls_byte_buffer, "[]=", _bytebuffSubscriptSet, 2);
  ADD_METHOD(cls_byte_buffer, "reserve", _bytebuffReserve, 1);
  ADD_METHOD(cls_byte_buffer, "fill", _bytebuffFill, 2);
  ADD_METHOD(cls_byte_buffer, "clear", _bytebuffClear, 0);
  ADD_METHOD(cls_byte_buffer, "write", _bytebuffWrite, 1);
  ADD_METHOD(cls_byte_buffer, "string", _bytebuffString, 0);
  ADD_METHOD(cls_byte_buffer, "count", _bytebuffCount, 0);

  releaseHandle(vm, cls_byte_buffer);

  // TODO: add move methods.
  Handle* cls_vector = NewClass(vm, "Vector", NULL, types, _vectorNew, _vectorDelete,
                                "A simple vector type "
                                "contains x, y, and z components.");

  ADD_METHOD(cls_vector, "_init", _vectorInit, -1);
  ADD_METHOD(cls_vector, "_getter", _vectorGetter, 1);
  ADD_METHOD(cls_vector, "_setter", _vectorSetter, 2);
  ADD_METHOD(cls_vector, "_repr", _vectorRepr, 0);

  releaseHandle(vm, cls_vector);

  registerModule(vm, types);
  releaseHandle(vm, types);
}
