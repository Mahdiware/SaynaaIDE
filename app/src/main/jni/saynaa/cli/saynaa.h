/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma comment(lib, "uuid.lib")
#endif

/*****************************************************************************/
/* SAYNAA DEFINES                                                            */
/*****************************************************************************/

// The version number macros.
// Major Version - Increment when changes break compatibility.
// Minor Version - Increment when new functionality added to public api.
// Patch Version - Increment when bug fixed or minor changes between releases.
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 2

// language name
#define LANGUAGE "saynaa"

// String representation of the version.
#define VERSION_STRING "1.0.2"
#define VERSION_NUMBER 102

#define VERSION LANGUAGE " " VERSION_STRING
#define COPYRIGHT VERSION " Copyright (C) 2022-2026 mahdiware.me"
#define AUTHORS "Mohamed Abdifatah"

//  visibility macros. define DLL for using
// shared library and define COMPILE to export symbols when compiling the
// it self as a shared library.

#ifdef _MSC_VER
#define EXPORT __declspec(dllexport)
#define IMPORT __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
#define EXPORT __attribute__((visibility("default")))
#define IMPORT
#else
#define EXPORT
#define IMPORT
#endif

#ifdef _DLL_
#ifdef _COMPILE_
#define PUBLIC EXPORT
#else
#define PUBLIC IMPORT
#endif
#else
#define PUBLIC
#endif

/*****************************************************************************/
/* SAYNAA TYPEDEFS & CALLBACKS                                           */
/*****************************************************************************/

//  Virtual Machine. It'll contain the state of the execution, stack,
// heap, and manage memory allocations.
typedef struct VM VM;

// A handle to the variables. It'll hold the reference to the
// variable and ensure that the variable it holds won't be garbage collected
// till it's released with releaseHandle().
typedef struct Handle Handle;

typedef struct Class Class;

typedef struct Configuration Configuration;

// C function pointer which is callable by native module
// functions.
typedef void (*nativeFn)(VM* vm);

// A function that'll be called for all the allocation calls by VM.
//
// - To allocate new memory it'll pass NULL to parameter [memory] and the
//   required size to [new_size]. On failure the return value would be NULL.
//
// - When reallocating an existing memory if it's grow in place the return
//   address would be the same as [memory] otherwise a new address.
//
// - To free an allocated memory pass [memory] and 0 to [new_size]. The
//   function will return NULL.
typedef void* (*ReallocFn)(void* memory, size_t new_size, void* user_data);

// Function callback to write [text] to stdout or stderr.
typedef void (*WriteFn)(VM* vm, const char* text);

// A function callback to read a line from stdin. The returned string shouldn't
// contain a line ending (\n or \r\n). The returned string **must** be
// allocated with Realloc() and the VM will claim the ownership of the
// string.
typedef char* (*ReadFn)(VM* vm);

// A generic function thiat could be used by the VM to signal something to
// the host application. The first argument is depend on the callback it's
// registered.
typedef void (*SignalFn)(void*);

// Load and return the script. Called by the compiler to fetch initial source
// code and source for import statements. Return NULL to indicate failure to
// load. Otherwise the string **must** be allocated with Realloc() and
// the VM will claim the ownership of the string.
typedef char* (*LoadScriptFn)(VM* vm, const char* path);

#ifndef NO_DL

// Load and return the native extension (*.dll, *.so) from the path, this will
// then used to import the module with the ImportImportDL function. On error
// the function should return NULL and shouldn't use any error api function.
typedef void* (*LoadDL)(VM* vm, const char* path);

// Native extension loader from the dynamic library. The handle should be vaiid
// as long as the module handle is alive. On error the function should return
// NULL and shouldn't use any error api function.
typedef Handle* (*ImportDL)(VM* vm, void* handle);

// Once the native module is gargage collected, the dl handle will be released
// with UnloadDL function.
typedef void (*UnloadDL)(VM* vm, void* handle);

#endif // NO_DL

// A function callback to resolve the import statement path. [from] path can
// be either path to a script or a directory or NULL if [path] is relative to
// cwd. If the path is a directory it'll always ends with a path separator
// which could be either '/' or '\\' regardless of the system. is
// un aware of the system, to indicate that the path is a directory.
//
// The return value should be a normalized absolute path of the [path]. Return
// NULL to indicate failure to resolve. Othrewise the string **must** be
// allocated with Realloc() and the VM will claim the ownership of the
// string.
typedef char* (*ResolvePathFn)(VM* vm, const char* from, const char* path);

// A function callback to allocate and return a new instance of the registered
// class. Which will be called when the instance is constructed. The returned/
// data is expected to be alive till the delete callback occurs.
typedef void* (*NewInstanceFn)(VM* vm);

// A function callback to de-allocate the allocated native instance of the
// registered class. This function is invoked at the GC execution. No object
// allocations are allowed during it, so **NEVER** allocate any objects
// inside them.
typedef void (*DeleteInstanceFn)(VM* vm, void*);

// A destructor function will de-allocate the native pointer
// when the handle is released. This function is invoked at the GC execution.
typedef void (*Destructor)(void*);

/*****************************************************************************/
/* SAYNAA TYPES                                                          */
/*****************************************************************************/

// Type enum of the first class types. Note that Object isn't
// instanciable (as of now) but they're considered first calss.
typedef enum VarType {
  vOBJECT = 0,

  vNULL,
  vBOOL,
  vNUMBER,
  vSTRING,
  vLIST,
  vMAP,
  vRANGE,
  vMODULE,
  vCLOSURE,
  vMETHOD_BIND,
  vFIBER,
  vCLASS,
  vPOINTER,
  vINSTANCE,
} VarType;

// Result that will return after a compilation or running a script
// or a function or evaluating an expression.
typedef enum Result {
  RESULT_SUCCESS = 0, // Successfully finished the execution.

  // Note that this result is internal and will not be returned to the host
  // anymore.
  //
  // Unexpected EOF while compiling the source. This is another compile time
  // error that will ONLY be returned if we're compiling in REPL mode. We need
  // this specific error to indicate the host application to add another line
  // to the last input. If REPL is not enabled this will be compile error.
  RESULT_UNEXPECTED_EOF,

  RESULT_COMPILE_ERROR, // Compilation failed.
  RESULT_RUNTIME_ERROR, // An error occurred at runtime.
} Result;

typedef struct Argument {
  // argument count: is number of value inside argv
  int argc;

  // argument value that passed file: main.c in function: int main(argc, argv)
  const char** argv;
} Argument;

typedef struct Configuration {
  // The callback used to allocate, reallocate, and free. If the function
  // pointer is NULL it defaults to the VM's realloc(), free() wrappers.
  ReallocFn realloc_fn;

  // I/O callbacks.
  WriteFn stderr_write;
  WriteFn stdout_write;
  ReadFn stdin_read;

  // Import system callbacks.
  ResolvePathFn resolve_path_fn;
  LoadScriptFn load_script_fn;

#ifndef NO_DL
  LoadDL load_dl_fn;
  ImportDL import_dl_fn;
  UnloadDL unload_dl_fn;
#endif

  // If true stderr calls will use ansi color codes.
  bool use_ansi_escape;

  // User defined data associated with VM.
  void* user_data;

  Argument argument;
} Configuration;

/*****************************************************************************/
/* SAYNAA PUBLIC API                                                     */
/*****************************************************************************/

// Create a new Configuration with the default values and return it.
// Override those default configuration to adopt to another hosting
// application.
PUBLIC Configuration NewConfiguration();

// Allocate, initialize and returns a new VM.
PUBLIC VM* NewVM(Configuration* config);

// Clean the VM and dispose all the resources allocated by the VM.
PUBLIC void FreeVM(VM* vm);

// Update the user data of the vm.
PUBLIC void SetUserData(VM* vm, void* user_data);

// Returns the associated user data.
PUBLIC void* GetUserData(const VM* vm);

// Register a new builtin function with the given [name]. [docstring] could be
// NULL or will always valid pointer since VM doesn't allocate a string for
// docstrings.
PUBLIC void RegisterBuiltinFn(VM* vm, const char* name, nativeFn fn, int arity,
                              const char* docstring);

// Adds a new search path to the VM, the path will be appended to the list of
// search paths. Search path orders are the same as the registered order.
// the last character of the path **must** be a path seperator '/' or '\\'.
PUBLIC void AddSearchPath(VM* vm, const char* path);

// Invoke allocator directly.  This function should be called
// when the host application want to send strings to the VM that are claimed
// by the VM once the caller returned it. For other uses you **should** call
// Realloc with [size] 0 to cleanup, otherwise there will be a memory leak.
//
// Internally it'll call `ReallocFn` function that was provided in the
// configuration.
PUBLIC void* Realloc(VM* vm, void* ptr, size_t size);

// Release the handle and allow its value to be garbage collected. Always call
// this for every handles before freeing the VM.
PUBLIC void releaseHandle(VM* vm, Handle* handle);

// Add a new module named [name] to the [vm]. Note that the module shouldn't
// already existed, otherwise an assertion will fail to indicate that.
PUBLIC Handle* NewModule(VM* vm, const char* name);

// Register the module to the VM's modules map, once after it can be
// imported in other modules.
PUBLIC void registerModule(VM* vm, Handle* module);

// Add a native function to the given module. If [arity] is -1 that means
// the function has variadic parameters and use GetArgc() to get the argc.
// Note that the function will be added as a global variable of the module.
// [docstring] is optional and could be omitted with NULL.
PUBLIC void ModuleAddFunction(VM* vm, Handle* module, const char* name,
                              nativeFn fptr, int arity, const char* docstring);

// Create a new class on the [module] with the [name] and return it.
// If the [base_class] is NULL by default it'll set to "Object" class.
// [docstring] is optional and could be omitted with NULL.
PUBLIC Handle* NewClass(VM* vm, const char* name, Handle* base_class,
                        Handle* module, NewInstanceFn new_fn,
                        DeleteInstanceFn delete_fn, const char* docstring);

// Add a native method to the given class. If the [arity] is -1 that means
// the method has variadic parameters and use GetArgc() to get the argc.
// [docstring] is optional and could be omitted with NULL.
PUBLIC void ClassAddMethod(VM* vm, Handle* cls, const char* name, nativeFn fptr,
                           int arity, const char* docstring);

PUBLIC Class* NewNativeClass(VM* vm, const char* name, NewInstanceFn new_fn,
                             DeleteInstanceFn delete_fn, const char* docstring);

PUBLIC void NativeClassAddMethod(VM* vm, Class* cls, const char* name,
                                 nativeFn fptr, int arity, const char* docstring);

// It'll compile the [source] for the module which result all the
// functions and classes in that [source] to register on the module.
PUBLIC void ModuleAddSource(VM* vm, Handle* module, const char* source);

// Run the source string. The [source] is expected to be valid till this
// function returns.
PUBLIC Result RunString(VM* vm, const char* source);

// Run the file at [path] relative to the current working directory.
PUBLIC Result RunFile(VM* vm, const char* path);

// time vm taked.
PUBLIC double vm_time(VM* vm);

// FIXME:
// Currently exit function will terminate the process which should exit from
// the function and return to the caller.
//
// Run REPL mode. If there isn't any stdin read function defined,
// or imput function ruturned NULL, it'll immediatly return a runtime error.
PUBLIC Result RunREPL(VM* vm);

/*****************************************************************************/
/* NATIVE / RUNTIME FUNCTION API                                             */
/*****************************************************************************/

// Set a runtime error to VM.
PUBLIC void SetRuntimeError(VM* vm, const char* message);

// Set a runtime error with C formated string.
PUBLIC void SetRuntimeErrorFmt(VM* vm, const char* fmt, ...);

// Returns native [this] of the current method as a void*.
PUBLIC void* GetThis(const VM* vm);

// Return the current functions argument count. This is needed for functions
// registered with -1 argument count (which means variadic arguments).
PUBLIC int GetArgc(const VM* vm);

// Check if the argc is in the range of (min <= argc <= max), if it's not, a
// runtime error will be set and return false, otherwise return true. Assuming
// that min <= max, and won't validate this in release binary.
PUBLIC bool CheckArgcRange(VM* vm, int argc, int min, int max);

// Helper function to check if the argument at the [slot] slot is Boolean and
// if not set a runtime error.
PUBLIC bool ValidateSlotBool(VM* vm, int slot, bool* value);

// Helper function to check if the argument at the [slot] slot is Number and
// if not set a runtime error.
PUBLIC bool ValidateSlotNumber(VM* vm, int slot, double* value);

// Helper function to check if the argument at the [slot] is an a whold number
// and if not set a runtime error.
PUBLIC bool ValidateSlotInteger(VM* vm, int slot, int32_t* value);

// Helper function to check if the argument at the [slot] slot is String and
// if not set a runtime error.
PUBLIC bool ValidateSlotString(VM* vm, int slot, const char** value, uint32_t* length);

// Helper function to check if the argument at the [slot] slot is of type
// [type] and if not sets a runtime error.
PUBLIC bool ValidateSlotType(VM* vm, int slot, VarType type);

// Helper function to check if the argument at the [slot] slot is an instance
// of the class which is at the [cls] index. If not set a runtime error.
PUBLIC bool ValidateSlotInstanceOf(VM* vm, int slot, int cls);

// Helper function to check if the instance at the [inst] slot is an instance
// of the class which is at the [cls] index. The value will be set to [val]
// if the object at [cls] slot isn't a valid class a runtime error will be set
// and return false.
PUBLIC bool IsSlotInstanceOf(VM* vm, int inst, int cls, bool* val);

// Make sure the fiber has [count] number of slots to work with (including the
// arguments).
PUBLIC void reserveSlots(VM* vm, int count);

// Returns the available number of slots to work with. It has at least the
// number argument the function is registered plus one for return value.
PUBLIC int GetSlotsCount(VM* vm);

// Returns the type of the variable at the [index] slot.
PUBLIC VarType GetSlotType(VM* vm, int index);

// Returns boolean value at the [index] slot. If the value at the [index]
// is not a boolean it'll be casted (only for booleans).
PUBLIC bool GetSlotBool(VM* vm, int index);

// Returns number value at the [index] slot. If the value at the [index]
// is not a boolean, an assertion will fail.
PUBLIC double GetSlotNumber(VM* vm, int index);

// Returns the string at the [index] slot. The returned pointer is only valid
// inside the native function that called this. Afterwards it may garbage
// collected and become demangled. If the [length] is not NULL the length of
// the string will be written.
PUBLIC const char* GetSlotString(VM* vm, int index, uint32_t* length);

PUBLIC void* GetSlotPointer(VM* vm, int index, void* native_ptr, Destructor destructor);

// Capture the variable at the [index] slot and return its handle. As long
// as the handle is not released with `releaseHandle()` the variable
// won't be garbage collected.
PUBLIC Handle* GetSlotHandle(VM* vm, int index);

// Returns the native instance at the [index] slot. If the value at the [index]
// is not a valid native instance, an assertion will fail.
PUBLIC void* GetSlotNativeInstance(VM* vm, int index);

// Set the [index] slot value as null.
PUBLIC void setSlotNull(VM* vm, int index);

// Set the [index] slot boolean value as the given [value].
PUBLIC void setSlotBool(VM* vm, int index, bool value);

// Set the [index] slot numeric value as the given [value].
PUBLIC void setSlotNumber(VM* vm, int index, double value);

// Create a new String copying the [value] and set it to [index] slot.
PUBLIC void setSlotString(VM* vm, int index, const char* value);

PUBLIC void setSlotPointer(VM* vm, int index, void* native_ptr, Destructor destructor);

PUBLIC void setSlotClosure(VM* vm, int index, const char* name, nativeFn fptr,
                           int arity, const char* docstring);

// Create a new String copying the [value] and set it to [index] slot.
// Unlike the above function it'll copy only the spicified length.
PUBLIC void setSlotStringLength(VM* vm, int index, const char* value, uint32_t length);

// Create a new string copying from the formated string and set it to [index]
// slot.
PUBLIC void setSlotStringFmt(VM* vm, int index, const char* fmt, ...);

// Set the [index] slot's value as the given [handle]. The function won't
// reclaim the ownership of the handle and you can still use it till
// it's released by yourself.
PUBLIC void setSlotHandle(VM* vm, int index, Handle* handle);

// Returns the hash of the [index] slot value. The value at the [index] must be
// hashable.
PUBLIC uint32_t GetSlotHash(VM* vm, int index);

/*****************************************************************************/
/* SAYNAA FFI                                                                */
/*****************************************************************************/

// Place the [this] instance at the [index] slot.
PUBLIC void PlaceThis(VM* vm, int index);

// Set the [index] slot's value as the class of the [instance].
PUBLIC void GetClass(VM* vm, int instance, int index);

// Creates a new instance of class at the [cls] slot, calls the constructor,
// and place it at the [index] slot. Returns true if the instance constructed
// successfully.
//
// [argc] is the argument count for the constructor, and [argv]
// is the first argument slot's index.
PUBLIC bool NewInstance(VM* vm, int cls, int index, int argc, int argv);

// Create a new Range object and place it at [index] slot.
PUBLIC void NewRange(VM* vm, int index, double first, double last);

// Create a new List object and place it at [index] slot.
PUBLIC void NewList(VM* vm, int index);

// Create a new Map object and place it at [index] slot.
PUBLIC void NewMap(VM* vm, int index);

// Create a new String object and place it at [index] slot.
PUBLIC void NewString(VM* vm, int index);

// Function to create a new Pointer object for API interaction.
PUBLIC void NewPointer(VM* vm, int index, void* native_ptr, Destructor destructor);

// Function to create new Closure
PUBLIC void NewClosure(VM* vm, int index, const char* name, nativeFn fptr,
                       int arity, const char* docstring);

// Insert [value] to the [list] at the [index], if the index is less than zero,
// it'll count from backwards. ie. insert[-1] == insert[list.length].
// Note that slot [list] must be a valid list otherwise it'll fail an
// assertion.
PUBLIC bool ListInsert(VM* vm, int list, int32_t index, int value);

// Pop an element from [list] at [index] and place it at the [popped] slot, if
// [popped] is negative, the popped value will be ignored.
PUBLIC bool ListPop(VM* vm, int list, int32_t index, int popped);

// Returns the length of the list at the [list] slot, it the slot isn't a list
// an assertion will fail.
PUBLIC uint32_t ListLength(VM* vm, int list);

// Calls a function at the [fn] slot, with [argc] argument where [argv] is the
// slot of the first argument. [ret] is the slot index of the return value. if
// [ret] < 0 the return value will be discarded.
PUBLIC bool CallFunction(VM* vm, int fn, int argc, int argv, int ret);

// Calls a [method] on the [instance] with [argc] argument where [argv] is the
// slot of the first argument. [ret] is the slot index of the return value. if
// [ret] < 0 the return value will be discarded.
PUBLIC bool CallMethod(VM* vm, int instance, const char* method, int argc, int argv, int ret);

// Get the attribute with [name] of the instance at the [instance] slot and
// place it at the [index] slot. Return true on success.
PUBLIC bool GetAttribute(VM* vm, int instance, const char* name, int index);

// Set the attribute with [name] of the instance at the [instance] slot to
// the value at the [value] index slot. Return true on success.
PUBLIC bool setAttribute(VM* vm, int instance, const char* name, int value);

// Import a module with the [path] and place it at [index] slot. The path
// sepearation should be '/'. Example: to import module "foo.bar" the [path]
// should be "foo/bar". On failure, it'll set an error and return false.
PUBLIC bool ImportModule(VM* vm, const char* path, int index);

#ifdef __cplusplus
} // extern "C"
#endif
