/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_optionals.h"

// A DUMMY MODULE TO TEST NATIVE INTERFACE AND CLASSES.

typedef struct {
  double val;
} Dummy;

void* _newDummy(VM* vm) {
  Dummy* dummy = Realloc(vm, NULL, sizeof(Dummy));
  ASSERT(dummy != NULL, "Realloc failed.");
  dummy->val = 0;
  return dummy;
}

void _deleteDummy(VM* vm, void* ptr) {
  Realloc(vm, ptr, 0);
}

saynaa_function(_dummyInit, "dummy.Dummy._init(n:Number)",
                "Initialize a dummy instance with [n].") {
  double val;
  if (!ValidateSlotNumber(vm, 1, &val))
    return;

  Dummy* thiz = (Dummy*) GetThis(vm);
  thiz->val = val;
}

saynaa_function(_dummyGetter, "dummy.Dummy._getter()", "") {
  const char* name = GetSlotString(vm, 1, NULL);
  Dummy* thiz = (Dummy*) GetThis(vm);
  if (strcmp("val", name) == 0) {
    setSlotNumber(vm, 0, thiz->val);
    return;
  }
}

saynaa_function(_dummySetter, "dummy.Dummy._setter()", "") {
  const char* name = GetSlotString(vm, 1, NULL);
  Dummy* thiz = (Dummy*) GetThis(vm);
  if (strcmp("val", name) == 0) {
    double val;
    if (!ValidateSlotNumber(vm, 2, &val))
      return;
    thiz->val = val;
    return;
  }
}

saynaa_function(_dummyAdd, "dummy.Dummy.+(other:dummy.Dummy) -> dummy.Dummy",
                "Adds two dummy instances.") {
  Dummy* thiz = (Dummy*) GetThis(vm);

  reserveSlots(vm, 4); // Now we have slots [0, 1, 2, 3].

  PlaceThis(vm, 2);   // slot[2] = this
  GetClass(vm, 2, 2); // slot[2] = Dummy class.

  // slots[1] = other.
  if (!ValidateSlotInstanceOf(vm, 1, 2))
    return;
  Dummy* other = (Dummy*) GetSlotNativeInstance(vm, 1);

  // slot[3] = thiz.val + other.val
  setSlotNumber(vm, 3, thiz->val + other->val);

  // slot[0] = Dummy(slot[3]) => return value.
  if (!NewInstance(vm, 2, 0, 1, 3))
    return;
}

saynaa_function(_dummyEq, "dummy.Dummy.==(other:dummy.Dummy) -> Bool",
                "Check if two dummy instances are the equal.") {
  // TODO: Currently there is no way of getting another native instance
  // So, it's impossible to check this == other. So for now checking with
  // number.
  double value;
  if (!ValidateSlotNumber(vm, 1, &value))
    return;

  Dummy* thiz = (Dummy*) GetThis(vm);
  setSlotBool(vm, 0, value == thiz->val);
}

saynaa_function(_dummyGt, "dummy.Dummy.>(other:dummy.Dummy) -> Bool",
                "Check if the dummy instance is greater than [other].") {
  // TODO: Currently there is no way of getting another native instance
  // So, it's impossible to check this == other. So for now checking with
  // number.
  double value;
  if (!ValidateSlotNumber(vm, 1, &value))
    return;

  Dummy* thiz = (Dummy*) GetThis(vm);
  setSlotBool(vm, 0, thiz->val > value);
}

saynaa_function(
    _dummyMethod, "Dummy.a_method(n1:Number, n2:Number) -> Number",
    "A dummy method to check dummy method calls. Will take 2 number arguments "
    "and return the multiplication.") {
  double n1, n2;
  if (!ValidateSlotNumber(vm, 1, &n1))
    return;
  if (!ValidateSlotNumber(vm, 2, &n2))
    return;
  setSlotNumber(vm, 0, n1 * n2);
}

saynaa_function(_dummyFunction, "dummy.afunc(s1:String, s2:String) -> String",
                "A dummy function the'll return s2 + s1.") {
  const char *s1, *s2;
  if (!ValidateSlotString(vm, 1, &s1, NULL))
    return;
  if (!ValidateSlotString(vm, 2, &s2, NULL))
    return;

  setSlotStringFmt(vm, 0, "%s%s", s2, s1);
}

saynaa_function(_dummyCallNative, "dummy.call_native(fn:Closure) -> Null",
                "Calls the function 'fn' with arguments 'foo', 42, false.") {
  if (!ValidateSlotType(vm, 1, vCLOSURE))
    return;

  reserveSlots(vm, 5); // Now we have slots [0, 1, 2, 3, 4].
  setSlotString(vm, 2, "foo");
  setSlotNumber(vm, 3, 42);
  setSlotBool(vm, 4, false);

  // slot[0] = slot[1](slot[2], slot[3], slot[4])
  if (!CallFunction(vm, 1, 3, 2, 0))
    return;
}

saynaa_function(
    _dummyCallMethod, "dummy.call_method(o:Var, method:String, a1:Var, a2:Var) -> Var",
    "Calls the method int the object [o] with two arguments [a1] and [a2].") {
  const char* method;
  if (!ValidateSlotString(vm, 2, &method, NULL))
    return;

  // slots = [null, o, method, a1, a2]

  if (!CallMethod(vm, 1, method, 2, 3, 0))
    return;
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleDummy(VM* vm) {
  Handle* dummy = NewModule(vm, "dummy");

  REGISTER_FN(dummy, "afunc", _dummyFunction, 2);
  REGISTER_FN(dummy, "call_native", _dummyCallNative, 1);
  REGISTER_FN(dummy, "call_method", _dummyCallMethod, 4);

  Handle* cls_dummy = NewClass(vm, "Dummy", NULL, dummy, _newDummy, _deleteDummy, NULL);
  ADD_METHOD(cls_dummy, "_init", _dummyInit, 1);
  ADD_METHOD(cls_dummy, "_getter", _dummyGetter, 1);
  ADD_METHOD(cls_dummy, "_setter", _dummySetter, 2);
  ADD_METHOD(cls_dummy, "+", _dummyAdd, 1);
  ADD_METHOD(cls_dummy, "==", _dummyEq, 1);
  ADD_METHOD(cls_dummy, ">", _dummyGt, 1);
  ADD_METHOD(cls_dummy, "a_method", _dummyMethod, 2);

  releaseHandle(vm, cls_dummy);

  registerModule(vm, dummy);
  releaseHandle(vm, dummy);
}