/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_optionals.h"

#include <math.h>

// M_PI is non standard. For a portable solution, we're defining it ourselves.
#define PI 3.14159265358979323846

saynaa_function(stdMathFloor, "math.floor(value:Numberber) -> Numberber",
                "Return the floor value.") {
  double num;
  if (!ValidateSlotNumber(vm, 1, &num))
    return;
  setSlotNumber(vm, 0, floor(num));
}

saynaa_function(stdMathCeil, "math.ceil(value:Number) -> Number", "Returns the ceiling value.") {
  double num;
  if (!ValidateSlotNumber(vm, 1, &num))
    return;
  setSlotNumber(vm, 0, ceil(num));
}

saynaa_function(stdMathPow, "math.pow(a:Number, b:Number) -> Number",
                "Returns the power 'b' of 'a' similler to a**b.") {
  double num, ex;
  if (!ValidateSlotNumber(vm, 1, &num))
    return;
  if (!ValidateSlotNumber(vm, 2, &ex))
    return;
  setSlotNumber(vm, 0, pow(num, ex));
}

saynaa_function(stdMathSqrt, "math.sqrt(value:Number) -> Number",
                "Returns the square root of the value") {
  double num;
  if (!ValidateSlotNumber(vm, 1, &num))
    return;
  setSlotNumber(vm, 0, sqrt(num));
}

saynaa_function(stdMathAbs, "math.abs(value:Number) -> Number", "Returns the absolute value.") {
  double num;
  if (!ValidateSlotNumber(vm, 1, &num))
    return;
  if (num < 0)
    num = -num;
  setSlotNumber(vm, 0, num);
}

saynaa_function(stdMathSign, "math.sign(value:Number) -> Number",
                "return the sign of the which is one of (+1, 0, -1).") {
  double num;
  if (!ValidateSlotNumber(vm, 1, &num))
    return;
  if (num < 0)
    num = -1;
  else if (num > 0)
    num = +1;
  else
    num = 0;
  setSlotNumber(vm, 0, num);
}

saynaa_function(
    stdMathSine, "math.sin(rad:Number) -> Number",
    "Return the sine value of the argument [rad] which is an angle expressed "
    "in radians.") {
  double rad;
  if (!ValidateSlotNumber(vm, 1, &rad))
    return;
  setSlotNumber(vm, 0, sin(rad));
}

saynaa_function(
    stdMathCosine, "math.cos(rad:Number) -> Number",
    "Return the cosine value of the argument [rad] which is an angle expressed "
    "in radians.") {
  double rad;
  if (!ValidateSlotNumber(vm, 1, &rad))
    return;
  setSlotNumber(vm, 0, cos(rad));
}

saynaa_function(stdMathTangent, "math.tan(rad:Number) -> Number",
                "Return the tangent value of the argument [rad] which is an "
                "angle expressed "
                "in radians.") {
  double rad;
  if (!ValidateSlotNumber(vm, 1, &rad))
    return;
  setSlotNumber(vm, 0, tan(rad));
}

saynaa_function(stdMathSinh, "math.sinh(val:Number) -> Number",
                "Return the hyperbolic sine value of the argument [val].") {
  double val;
  if (!ValidateSlotNumber(vm, 1, &val))
    return;
  setSlotNumber(vm, 0, sinh(val));
}

saynaa_function(stdMathCosh, "math.cosh(val:Number) -> Number",
                "Return the hyperbolic cosine value of the argument [val].") {
  double val;
  if (!ValidateSlotNumber(vm, 1, &val))
    return;
  setSlotNumber(vm, 0, cosh(val));
}

saynaa_function(stdMathTanh, "math.tanh(val:Number) -> Number",
                "Return the hyperbolic tangent value of the argument [val].") {
  double val;
  if (!ValidateSlotNumber(vm, 1, &val))
    return;
  setSlotNumber(vm, 0, tanh(val));
}

saynaa_function(
    stdMathArcSine, "math.asin(num:Number) -> Number",
    "Return the arcsine value of the argument [num] which is an angle "
    "expressed in radians.") {
  double num;
  if (!ValidateSlotNumber(vm, 1, &num))
    return;

  if (num < -1 || 1 < num) {
    SetRuntimeError(vm, "Argument should be between -1 and +1");
  }

  setSlotNumber(vm, 0, asin(num));
}

saynaa_function(stdMathArcCosine, "math.acos(num:Number) -> Number",
                "Return the arc cosine value of the argument [num] which is "
                "an angle expressed in radians.") {
  double num;
  if (!ValidateSlotNumber(vm, 1, &num))
    return;

  if (num < -1 || 1 < num) {
    SetRuntimeError(vm, "Argument should be between -1 and +1");
  }

  setSlotNumber(vm, 0, acos(num));
}

saynaa_function(stdMathArcTangent, "math.atan(num:Number) -> Number",
                "Return the arc tangent value of the argument [num] which is "
                "an angle expressed in radians.") {
  double num;
  if (!ValidateSlotNumber(vm, 1, &num))
    return;
  setSlotNumber(vm, 0, atan(num));
}

saynaa_function(
    stdMathArcTan2, "math.atan2(y:Number, x:Number) -> Number",
    "These functions calculate the principal value of the arc tangent "
    "of y / x, using the signs of the two arguments to determine the "
    "quadrant of the result") {
  double y, x;
  if (!ValidateSlotNumber(vm, 1, &y))
    return;
  if (!ValidateSlotNumber(vm, 2, &x))
    return;

  setSlotNumber(vm, 0, atan2(y, x));
}

saynaa_function(stdMathLog10, "math.log10(value:Number) -> Number",
                "Return the logarithm to base 10 of argument [value]") {
  double num;
  if (!ValidateSlotNumber(vm, 1, &num))
    return;
  setSlotNumber(vm, 0, log10(num));
}

saynaa_function(stdMathRound, "math.round(value:Number) -> Number",
                "Round to nearest integer,"
                " away from zero and return the number.") {
  double num;
  if (!ValidateSlotNumber(vm, 1, &num))
    return;
  setSlotNumber(vm, 0, round(num));
}

saynaa_function(stdMathRand, "math.rand() -> Number",
                "Return a random number in the range of 0..0x7fff.") {
  // RAND_MAX is implementation dependent but is guaranteed to be at least
  // 0x7fff on any standard library implementation.
  // https://www.cplusplus.com/reference/cstdlib/RAND_MAX/
  setSlotNumber(vm, 0, rand() % 0x7fff);
}

saynaa_function(stdMathRandom, "math.random(value:Number, value:Number) -> Number",
                "Return a random number.") {
  double num1, num2;
  if (!ValidateSlotNumber(vm, 1, &num1))
    return;
  if (!ValidateSlotNumber(vm, 2, &num2))
    return;

  double result;
  // if num1 is lower, consider it min, otherwise, num2 is min
  if (num1 < num2) {
    // returns a random integer between num1 and num2 inclusive
    result = (double) ((rand() % (int) (num2 - num1 + 1)) + num1);
  } else if (num1 > num2) {
    result = (double) ((rand() % (int) (num1 - num2 + 1)) + num2);
  } else {
    result = (double) num1;
  }

  setSlotNumber(vm, 0, result);
}

/*****************************************************************************/
/* MODULE REGISTER                                                           */
/*****************************************************************************/

void registerModuleMath(VM* vm) {
  Handle* math = NewModule(vm, "math");

  // Set global value PI.
  moduleSetGlobal(vm, ((Module*) AS_OBJ(math->value)), "pi", 2, VAR_NUM(PI));

  REGISTER_FN(math, "floor", stdMathFloor, 1);
  REGISTER_FN(math, "ceil", stdMathCeil, 1);
  REGISTER_FN(math, "pow", stdMathPow, 2);
  REGISTER_FN(math, "sqrt", stdMathSqrt, 1);
  REGISTER_FN(math, "abs", stdMathAbs, 1);
  REGISTER_FN(math, "sign", stdMathSign, 1);
  REGISTER_FN(math, "sin", stdMathSine, 1);
  REGISTER_FN(math, "cos", stdMathCosine, 1);
  REGISTER_FN(math, "tan", stdMathTangent, 1);
  REGISTER_FN(math, "sinh", stdMathSinh, 1);
  REGISTER_FN(math, "cosh", stdMathCosh, 1);
  REGISTER_FN(math, "tanh", stdMathTanh, 1);
  REGISTER_FN(math, "asin", stdMathArcSine, 1);
  REGISTER_FN(math, "acos", stdMathArcCosine, 1);
  REGISTER_FN(math, "atan", stdMathArcTangent, 1);
  REGISTER_FN(math, "atan2", stdMathArcTan2, 2);
  REGISTER_FN(math, "log10", stdMathLog10, 1);
  REGISTER_FN(math, "round", stdMathRound, 1);
  REGISTER_FN(math, "rand", stdMathRand, 0);
  REGISTER_FN(math, "random", stdMathRandom, 2);

  registerModule(vm, math);
  releaseHandle(vm, math);
}