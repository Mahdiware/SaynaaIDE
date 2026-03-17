/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#include "../shared/saynaa_internal.h"
#include "../shared/saynaa_value.h"

#ifdef __cplusplus
extern "C" {
#endif

// Literal strings used in various places. For now these are
// defined as macros so that it'll be easier in the future to refactor or
// restructre. The names of the macros are begin with LIST_ and the string.
#define LITS__init "_init"
#define LITS__str "_str"
#define LITS__repr "_repr"
#define LITS__getter "_getter"
#define LITS__setter "_setter"
#define LITS__call "_call"
#define LITS__next "_next"
#define LITS__value "_value"

// Functions, methods, classes and  other names which are intrenal / special to
// starts with the following character (ex: @main, @literalFn).
// When importing all (*) from a module, if the name of an entry starts with
// this character it'll be skipped.
#define SPECIAL_NAME_CHAR '@'

// Name of the implicit function for a module. When a module is parsed all of
// it's statements are wrapped around an implicit function with this name.
#define IMPLICIT_MAIN_NAME "@main"

// Name of a literal function. All literal function will have the same name but
// they're uniquely identified by their index in the script's function buffer.
#define LITERAL_FN_NAME "@anonymous"

// Initialize core language, builtin function and core libs.
void initializeCore(VM* vm);

// Initialize a module. If the script has path, it'll define __file__ global
// as an absolute path of the module. [path] will be the normalized absolute
// path of the module. If the module's path is NULL, it's name is used.
//
// Also define __name__ as the name of the module, assuming all the modules
// have name excpet for main which. for main the name will be defined as
// '__main__' just like python.
void initializeModule(VM* vm, Module* module, bool is_main);

// Create a new module with the given [name] and returns as a Module*.
// This is function is a wrapper around `NewModule()` function to create
// native modules for core and public native api.
Module* newModuleInternal(VM* vm, const char* name);

// Adds a function to the module with the give properties and add the function
// to the module's globals variables.
void moduleAddFunctionInternal(VM* vm, Module* module, const char* name,
                               nativeFn fptr, int arity, const char* docstring);

// Bind a method to a class and deal with magic methods.
void bindMethod(VM* vm, Class* cls, Closure* method);

// Get the specified magic method or NULL. Cache the method if possible.
Closure* getMagicMethod(Class* cls, MagicMethod mm);

/*****************************************************************************/
/* OPERATORS                                                                 */
/*****************************************************************************/

// This method is called just before constructing a type to initialize this
// and after that the constructor will be called. For builtin types this
// function will return VAR_NULL and the constructor will override this to
// it's instance (because for some classes we cannot create without argument
// example Fiber(fn), Range(from, to) etc). If the class cannot be
// instanciated (ex: Class 'Module') it'll set an error and return VAR_NULL.
// For other classes the return value will be an Instance.
Var preConstructThis(VM* vm, Class* cls);

// Returns the class of the [instance].
Class* getClass(VM* vm, Var instance);

// Returns the method (closure) in the instance [thiz]. If it's not an method
// but just an attribute the [is_method] pointer will be set to false and
// returns the value.
// If the method / attribute not found, it'll set a runtime error on the VM.
Var getMethod(VM* vm, Var thiz, String* name, bool* is_method);

// Returns the method (closure) from the instance's super class. If the method
// doesn't exists, it'll set an error on the VM.
Closure* getSuperMethod(VM* vm, Var thiz, String* name);

// Unlike getMethod this will not set error and will not try to get attribute
// with the same name. It'll return true if the method exists on [thiz], false
// otherwise and if the [method] argument is not NULL, method will be set.
bool hasMethod(VM* vm, Var thiz, String* name, Closure** method);

// Returns the string value of the variable, a wrapper of toString() function
// but for instances it'll try to calll "_to_string" function and on error
// it'll return NULL.
// if parameter [repr] is true it'll return repr string of the value and for
// instances it'll call "_repr()" method.
// Note that if _str method does not exists it'll use _repr method for to
// string.
String* varToString(VM* vm, Var thiz, bool repr);

Var varPositive(VM* vm, Var v); // Returns +v.
Var varNegative(VM* vm, Var v); // Returns -v.
Var varNot(VM* vm, Var v);      // Returns !v.
Var varBitNot(VM* vm, Var v);   // Returns ~v.

Var varAdd(VM* vm, Var v1, Var v2, bool inplace);      // Returns v1 + v2.
Var varSubtract(VM* vm, Var v1, Var v2, bool inplace); // Returns v1 - v2.
Var varMultiply(VM* vm, Var v1, Var v2, bool inplace); // Returns v1 * v2.
Var varDivide(VM* vm, Var v1, Var v2, bool inplace);   // Returns v1 / v2.
Var varExponent(VM* vm, Var v1, Var v2, bool inplace); // Returns v1 ** v2.
Var varModulo(VM* vm, Var v1, Var v2, bool inplace);   // Returns v1 % v2.

Var varBitAnd(VM* vm, Var v1, Var v2, bool inplace);    // Returns v1 & v2.
Var varBitOr(VM* vm, Var v1, Var v2, bool inplace);     // Returns v1 | v2.
Var varBitXor(VM* vm, Var v1, Var v2, bool inplace);    // Returns v1 ^ v2.
Var varBitLshift(VM* vm, Var v1, Var v2, bool inplace); // Returns v1 << v2.
Var varBitRshift(VM* vm, Var v1, Var v2, bool inplace); // Returns v1 >> v2.

Var varEqals(VM* vm, Var v1, Var v2);   // Returns v1 == v2.
Var varGreater(VM* vm, Var v1, Var v2); // Returns v1 > v2.
Var varLesser(VM* vm, Var v1, Var v2);  // Returns v1 < v2.

Var varOpRange(VM* vm, Var v1, Var v2); // Returns v1 .. v2.

// Returns [elem] in [container]. Sets an error if the [container] is not an
// iterable.
bool varContains(VM* vm, Var elem, Var container);

// Returns [inst] is [type]. Sets an error if the [type] is not a class.
bool varIsType(VM* vm, Var inst, Var type);

// Returns the attribute named [attrib] on the variable [on].
Var varGetAttrib(VM* vm, Var on, String* attrib, bool skipGetter, bool callable);

// Set the attribute named [attrib] on the variable [on] with the given
// [value].
void varSetAttrib(VM* vm, Var on, String* name, Var value, bool skipSetter);

// Returns the subscript value (ie. on[key]).
Var varGetSubscript(VM* vm, Var on, Var key);

// Set subscript [value] with the [key] (ie. on[key] = value).
void varsetSubscript(VM* vm, Var on, Var key, Var value);

// Iterate over [seq] store as [value], [iterator] start with null.
// Returns ture to continue loop, false to break.
bool varIterate(VM* vm, Var seq, Var* iterator, Var* value);

#ifdef __cplusplus
} // extern "C"
#endif
