/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

// This file contains all the public function implementations.

#include "../cli/saynaa.h"
#include "../runtime/saynaa_core.h"
#include "../runtime/saynaa_vm.h"
#include "../shared/saynaa_readline.h"
#include "../shared/saynaa_value.h"
#include "../utils/saynaa_utils.h"

#include <math.h>

// Path Resolving Functionality Documentation:
//
// The core does not implement path resolving functionality directly.
// Instead, this functionality is expected to be provided by the host
// application. By default, we use an implementation from the path library.
// However, the core cannot depend on external libraries to maintain
// encapsulation.
//
// Workaround:
// To address thiz, we declare the default path resolver here and use it.
// If someone wants to compile just the core without optional libraries,
// they must define NO_OPTIONAL. This prevents the compiler from attempting
// to link functions that are unavailable.
//
// Implications:
// - This approach ensures modularity and encapsulation.
// - Developers compiling the core without optional libraries must ensure
//   that NO_OPTIONAL is defined to avoid linking errors.
//
// Note:
// Future improvements may involve finding a better way to handle path
// resolution without relying on external libraries or requiring conditional
// compilation.
#ifndef NO_OPTIONAL
void registerLibs(VM* vm);
void cleanupLibs(VM* vm);
char* pathResolveImport(VM* vm, const char* from, const char* path);

#ifndef NO_DL
void* osLoadDL(VM* vm, const char* path);
Handle* osImportDL(VM* vm, void* handle);
void osUnloadDL(VM* vm, void* handle);

#endif // NO_DL
#endif // NO_OPTIONAL

#define CHECK_ARG_NULL(name) \
  ASSERT((name) != NULL, "Argument " #name " was NULL.");

#define CHECK_HANDLE_TYPE(handle, type) \
  do { \
    CHECK_ARG_NULL(handle); \
    ASSERT(IS_OBJ_TYPE(handle->value, type), \
           "Given handle is not of type " #type "."); \
  } while (false)

#define VALIDATE_SLOT_INDEX(index) \
  do { \
    ASSERT(index >= 0, "Slot index was negative."); \
    ASSERT(index < GetSlotsCount(vm), "Slot index is too large. Did you " \
                                      "forget to call reserveSlots()?."); \
  } while (false)

#define CHECK_FIBER_EXISTS(vm) \
  do { \
    ASSERT(vm->fiber != NULL, \
           "No fiber exists. Did you forget to call reserveSlots()?"); \
  } while (false)

// Macro to access the nth argument (0-based) of the current function.
#define ARG(n) (vm->fiber->ret[n])

// Alias for ARG(n). This macro is valid even if additional slots are allocated,
// but the caller must ensure the index is within bounds.
#define SLOT(n) ARG(n)

// Macro to set the value of the nth slot.
#define SET_SLOT(n, val) SLOT(n) = (val);

// Evaluates to the current function's argument count.
#define ARGC ((int) (vm->fiber->sp - vm->fiber->ret) - 1)

// Default allocator used to initialize the VM configuration if the host
// does not provide one.
static void* defaultRealloc(void* memory, size_t new_size, void* _);

static void stderrWrite(VM* vm, const char* text);
static void stdoutWrite(VM* vm, const char* text);
static char* stdinRead(VM* vm);
static char* loadScript(VM* vm, const char* path);

void* Realloc(VM* vm, void* ptr, size_t size) {
  ASSERT(vm->config.realloc_fn != NULL, "VM's allocator was NULL.");
  return vm->config.realloc_fn(ptr, size, vm->config.user_data);
}

Configuration NewConfiguration() {
  Configuration config;
  memset(&config, 0, sizeof(config));

  config.realloc_fn = defaultRealloc;

  config.stdout_write = stdoutWrite;
  config.stderr_write = stderrWrite;
  config.stdin_read = stdinRead;
#ifndef NO_OPTIONAL
  config.resolve_path_fn = pathResolveImport;

#ifndef NO_DL
  config.load_dl_fn = osLoadDL;
  config.import_dl_fn = osImportDL;
  config.unload_dl_fn = osUnloadDL;
#endif

#endif
  config.load_script_fn = loadScript;

  return config;
}

VM* NewVM(Configuration* config) {
  Configuration default_config = NewConfiguration();

  if (config == NULL)
    config = &default_config;

  VM* vm = (VM*) config->realloc_fn(NULL, sizeof(VM), config->user_data);
  memset(vm, 0, sizeof(VM));

  vm->config = *config;
  vm->working_set_count = 0;
  vm->working_set_capacity = MIN_CAPACITY;
  vm->working_set = (Object**) vm->config.realloc_fn(
      NULL, sizeof(Object*) * vm->working_set_capacity, NULL);
  vm->next_gc = INITIAL_GC_SIZE;
  vm->collecting_garbage = false;
  vm->min_heap_size = MIN_HEAP_SIZE;
  vm->heap_fill_percent = HEAP_FILL_PERCENT;

  vm->modules = newMap(vm);
  vm->search_paths = newList(vm, 8);
  vm->searchers = newList(vm, 8);

  vm->builtins_count = 0;
  vm->time = 0;

  // This is necessary to prevent garbage collection skip the entry in this
  // array while we're building it.
  for (int i = 0; i < vINSTANCE; i++) {
    vm->builtin_classes[i] = NULL;
  }

  initializeCore(vm);

#ifndef NO_OPTIONAL
  registerLibs(vm);
#endif

  return vm;
}

void FreeVM(VM* vm) {
#ifndef NO_OPTIONAL
  cleanupLibs(vm);
#endif

  Object* obj = vm->first;
  while (obj != NULL) {
    Object* next = obj->next;
    freeObject(vm, obj);
    obj = next;
  }

  vm->working_set = (Object**) vm->config.realloc_fn(vm->working_set, 0,
                                                     vm->config.user_data);

  // Validate that all handles have been released by the host application.
  // If handles remain, it indicates a resource leak in the host's usage of the VM.
  ASSERT(vm->handles == NULL, "Not all handles were released.");

  DEALLOCATE(vm, vm, VM);
}

void* GetUserData(const VM* vm) {
  return vm->config.user_data;
}

void SetUserData(VM* vm, void* user_data) {
  vm->config.user_data = user_data;
}

void RegisterBuiltinFn(VM* vm, const char* name, nativeFn fn, int arity, const char* docstring) {
  ASSERT(vm->builtins_count < BUILTIN_FN_CAPACITY,
         "Maximum builtin function limit reached, To increase the limit set "
         "BUILTIN_FN_CAPACITY and recompile.");

  // TODO: Optimize builtin function lookup during compilation.
  // If the functions are sorted, we can use binary search instead of linear scan.
  // Note that runtime lookup is already O(1) via index, so this only affects compile time.
  for (int i = 0; i < vm->builtins_count; i++) {
    Closure* bfn = vm->builtins_funcs[i];
    ASSERT(strcmp(bfn->fn->name, name) != 0,
           "Overriding existing function not supported yet.");
  }

  Function* fptr = newFunction(vm, name, (int) strlen(name), NULL, true, docstring, NULL);
  vmPushTempRef(vm, &fptr->_super); // fptr.
  fptr->native = fn;
  fptr->arity = arity;
  vm->builtins_funcs[vm->builtins_count++] = newClosure(vm, fptr);
  vmPopTempRef(vm); // fptr.
}

void AddSearchPath(VM* vm, const char* path) {
  CHECK_ARG_NULL(path);

  size_t length = strlen(path);
  ASSERT(length > 0, "Path size cannot be 0.");

  char last = path[length - 1];
  ASSERT(last == '/' || last == '\\', "Path should ends with "
                                      "either '/' or '\\'.");

  String* spath = newStringLength(vm, path, (uint32_t) length);
  vmPushTempRef(vm, &spath->_super); // spath.
  listAppend(vm, vm->search_paths, VAR_OBJ(spath));
  vmPopTempRef(vm); // spath.
}

Handle* NewModule(VM* vm, const char* name) {
  CHECK_ARG_NULL(name);
  Module* module = newModuleInternal(vm, name);

  vmPushTempRef(vm, &module->_super); // module.
  Handle* handle = vmNewHandle(vm, VAR_OBJ(module));
  vmPopTempRef(vm); // module.

  return handle;
}

void registerModule(VM* vm, Handle* module) {
  CHECK_HANDLE_TYPE(module, OBJ_MODULE);

  Module* module_ = (Module*) AS_OBJ(module->value);
  vmRegisterModule(vm, module_, module_->name);
}

void ModuleAddFunction(VM* vm, Handle* module, const char* name, nativeFn fptr,
                       int arity, const char* docstring) {
  CHECK_HANDLE_TYPE(module, OBJ_MODULE);
  CHECK_ARG_NULL(fptr);

  moduleAddFunctionInternal(vm, (Module*) AS_OBJ(module->value), name, fptr, arity, docstring);
}

Handle* NewClass(VM* vm, const char* name, Handle* base_class, Handle* module,
                 NewInstanceFn new_fn, DeleteInstanceFn delete_fn, const char* docstring) {
  CHECK_ARG_NULL(module);
  CHECK_ARG_NULL(name);
  CHECK_HANDLE_TYPE(module, OBJ_MODULE);

  Class* super = vm->builtin_classes[vOBJECT];
  if (base_class != NULL) {
    CHECK_HANDLE_TYPE(base_class, OBJ_CLASS);
    super = (Class*) AS_OBJ(base_class->value);
  }

  Class* class_ = newClass(vm, name, (int) strlen(name), super,
                           (Module*) AS_OBJ(module->value), docstring, NULL);
  class_->new_fn = new_fn;
  class_->delete_fn = delete_fn;

  vmPushTempRef(vm, &class_->_super); // class_.
  Handle* handle = vmNewHandle(vm, VAR_OBJ(class_));
  vmPopTempRef(vm); // class_.
  return handle;
}

Class* NewNativeClass(VM* vm, const char* name, NewInstanceFn new_fn,
                      DeleteInstanceFn delete_fn, const char* docstring) {
  CHECK_ARG_NULL(name);
  Class* super = vm->builtin_classes[vOBJECT];

  Class* class_ = newClass(vm, name, (int) strlen(name), super, NULL, docstring, NULL);
  class_->new_fn = new_fn;
  class_->delete_fn = delete_fn;
  return class_;
}

void NativeClassAddMethod(VM* vm, Class* cls, const char* name, nativeFn fptr,
                          int arity, const char* docstring) {
  CHECK_ARG_NULL(cls);
  CHECK_ARG_NULL(fptr);

  Class* class_ = cls;

  Function* fn = newFunction(vm, name, (int) strlen(name), class_->owner, true,
                             docstring, NULL);
  vmPushTempRef(vm, &fn->_super);

  fn->arity = arity;
  fn->is_method = true;
  fn->native = fptr;

  Closure* method = newClosure(vm, fn);
  vmPopTempRef(vm);

  vmPushTempRef(vm, &method->_super);
  bindMethod(vm, class_, method);
  vmPopTempRef(vm);
}

void ClassAddMethod(VM* vm, Handle* cls, const char* name, nativeFn fptr,
                    int arity, const char* docstring) {
  CHECK_ARG_NULL(cls);
  CHECK_ARG_NULL(fptr);
  CHECK_HANDLE_TYPE(cls, OBJ_CLASS);

  // TODO:
  // Check if the method name is valid, and validate argc for special
  // methods (like "_getter", "_call", "+", "-", etc).

  Class* class_ = (Class*) AS_OBJ(cls->value);

  Function* fn = newFunction(vm, name, (int) strlen(name), class_->owner, true,
                             docstring, NULL);
  vmPushTempRef(vm, &fn->_super); // fn.

  fn->arity = arity;
  fn->is_method = true;
  fn->native = fptr;

  // No need to push the function to the VM's temporary references because it is
  // written into the module's constant pool. The module itself is reachable via
  // the class handle, protecting it from garbage collection.

  Closure* method = newClosure(vm, fn);
  vmPopTempRef(vm); // fn.

  vmPushTempRef(vm, &method->_super); // method.
  bindMethod(vm, class_, method);
  vmPopTempRef(vm); // method.
}

void ModuleAddSource(VM* vm, Handle* module, const char* source) {
  CHECK_HANDLE_TYPE(module, OBJ_MODULE);
  CHECK_ARG_NULL(source);
  // TODO: compiler options, maybe set to the vm and reuse it here.
  compile(vm, (Module*) AS_OBJ(module->value), source, NULL);
}

void releaseHandle(VM* vm, Handle* handle) {
  ASSERT(handle != NULL, "Given handle was NULL.");

  // If the handle is the head of the vm's handle chain set it to the next one.
  if (handle == vm->handles) {
    vm->handles = handle->next;
  }

  // Remove the handle from the chain by connecting the both ends together.
  if (handle->next)
    handle->next->prev = handle->prev;
  if (handle->prev)
    handle->prev->next = handle->next;

  // Free the handle.
  DEALLOCATE(vm, handle, Handle);
}

double vm_time(VM* vm) {
  return vm->time;
}

Result RunString(VM* vm, const char* source) {
  Result result = RESULT_SUCCESS;

  // Create a temproary module for the source.
  Module* module = newModule(vm);
  vmPushTempRef(vm, &module->_super); // module.
  {
    module->path = newString(vm, "@(String)");
    result = compile(vm, module, source, NULL);
    if (result != RESULT_SUCCESS)
      return result;

    // Module initialized needs to be set to true just before executing
    // their main function to avoid cyclic inclusion crash the VM.
    module->initialized = true;

    Fiber* fiber = newFiber(vm, module->body);
    vmPushTempRef(vm, &fiber->_super); // fiber.
    vmPrepareFiber(vm, fiber, 0, NULL);
    vmPopTempRef(vm); // fiber.

    nanotime_t tstart = nanotime();
    result = vmRunFiber(vm, fiber);
    nanotime_t tend = nanotime();
    vm->time = millitime(tstart, tend);
  }
  vmPopTempRef(vm); // module.

  return result;
}

Result RunFile(VM* vm, const char* path) {
  // Note: Even if the file is already in the VM's script cache (e.g., via
  // import), we explicitly recompile it here to ensure the cache reflects the
  // latest content on disk.

  ASSERT(vm->config.load_script_fn != NULL,
         "No script loading functions defined.");

  Result result = RESULT_SUCCESS;
  Module* module = NULL;

  // Resolve the path.
  char* resolved_ = NULL;
  if (vm->config.resolve_path_fn != NULL) {
    resolved_ = vm->config.resolve_path_fn(vm, NULL, path);
  }

  if (resolved_ == NULL) {
    if (vm->config.stderr_write != NULL) {
      if (vm->config.use_ansi_escape) {
        vm->config.stderr_write(vm,
                                "\x1b[31mError\x1b[0m finding script at \"");
      } else {
        vm->config.stderr_write(vm, "Error finding script at \"");
      }
      vm->config.stderr_write(vm, path);
      vm->config.stderr_write(vm, "\"\n");
    }
    return RESULT_COMPILE_ERROR;
  }

  module = newModule(vm);
  vmPushTempRef(vm, &module->_super); // module.
  {
    // Set module path and and deallocate resolved.
    String* script_path = newString(vm, resolved_);
    vmPushTempRef(vm, &script_path->_super); // script_path.
    Realloc(vm, resolved_, 0);
    module->path = script_path;
    vmPopTempRef(vm); // script_path.

    initializeModule(vm, module, true);

    const char* _path = module->path->data;
    char* source = vm->config.load_script_fn(vm, _path);
    if (source == NULL) {
      result = RESULT_COMPILE_ERROR;
      if (vm->config.stderr_write != NULL) {
        if (vm->config.use_ansi_escape) {
          vm->config.stderr_write(vm,
                                  "\x1b[31mError\x1b[0m loading script at \"");
        } else {
          vm->config.stderr_write(vm, "Error loading script at \"");
        }
        vm->config.stderr_write(vm, _path);
        vm->config.stderr_write(vm, "\"\n");
      }
    } else {
      result = compile(vm, module, source, NULL);
      Realloc(vm, source, 0);
    }

    if (result == RESULT_SUCCESS) {
      vmRegisterModule(vm, module, module->path);
    }
  }
  vmPopTempRef(vm); // module.

  if (result != RESULT_SUCCESS)
    return result;

  // Module initialized needs to be set to true just before executing their
  // main function to avoid cyclic inclusion crash the VM.
  module->initialized = true;
  Fiber* fiber = newFiber(vm, module->body);
  vmPushTempRef(vm, &fiber->_super); // fiber.
  vmPrepareFiber(vm, fiber, 0, NULL);
  vmPopTempRef(vm); // fiber.

  nanotime_t tstart = nanotime();
  result = vmRunFiber(vm, fiber);
  nanotime_t tend = nanotime();
  vm->time = millitime(tstart, tend);

  return result;
}

// TODO: Consider moving to a shared location.
// Retrieves the implicit main function from a module for REPL execution.
Closure* moduleGetMainFunction(VM* vm, Module* module) {
  int main_index = moduleGetGlobalIndex(module, IMPLICIT_MAIN_NAME,
                                        (uint32_t) strlen(IMPLICIT_MAIN_NAME));
  if (main_index == -1)
    return NULL;
  ASSERT_INDEX(main_index, (int) module->globals.count);
  Var main_fn = module->globals.data[main_index];
  ASSERT(IS_OBJ_TYPE(main_fn, OBJ_CLOSURE), OOPS);
  return (Closure*) AS_OBJ(main_fn);
}

Result RunREPL(VM* vm) {
  WriteFn printfn = vm->config.stdout_write;
  WriteFn printerrfn = vm->config.stderr_write;
  ReadFn inputfn = vm->config.stdin_read;
  Result result = RESULT_SUCCESS;

  CompileOptions options = newCompilerOptions();
  options.repl_mode = true;

  if (inputfn == NULL) {
    if (printerrfn)
      printerrfn(vm, "REPL failed to input.");
    return RESULT_RUNTIME_ERROR;
  }

  // Create the main module used to compile and execute input source code.
  Handle* module = NewModule(vm, "@(REPL)");
  ASSERT(IS_OBJ_TYPE(module->value, OBJ_MODULE), OOPS);
  Module* _module = (Module*) AS_OBJ(module->value);
  initializeModule(vm, _module, true);

  // A buffer to accumulate multiple lines from stdin.
  ByteBuffer lines;
  ByteBufferInit(&lines);

  // Indicates if compilation failed due to an unexpected EOF, requiring
  // additional lines to be appended to the buffer.
  bool need_more_lines = false;

  bool done = false;
  char* line;
  do {
    const char* listening = (!need_more_lines) ? ">>> " : "... ";

    // Read a line from stdin.
#if defined(__linux) && defined(READLINE)
    line = saynaa_readline(vm, listening);

#else
    printfn(vm, listening);
    line = inputfn(vm);
#endif
    if (line == NULL) {
      if (printerrfn)
        printerrfn(vm, "REPL failed to input.\n");
      result = RESULT_RUNTIME_ERROR;
      break;
    }

    // Stop the REPL if EOF is encountered.
    size_t line_length = strlen(line);
    if (line_length >= 1 && *(line + line_length - 1) == EOF) {
      printfn(vm, "\n");
      result = RESULT_SUCCESS;
      Realloc(vm, line, 0);
      break;
    }

    // Skip compilation for empty lines.
    if (utilIsStringEmpty(line)) {
      if (need_more_lines)
        ASSERT(lines.count != 0, OOPS);
      Realloc(vm, line, 0);
      continue;
    }

    // Append the new line to the buffer.
    if (lines.count != 0)
      ByteBufferWrite(&lines, vm, '\n');
    ByteBufferAddString(&lines, vm, line, (uint32_t) line_length);
    Realloc(vm, line, 0);
    ByteBufferWrite(&lines, vm, '\0');

    // Attempt to compile the accumulated buffer.
    result = compile(vm, _module, (const char*) lines.data, &options);

    if (result == RESULT_UNEXPECTED_EOF) {
      ASSERT(lines.count > 0 && lines.data[lines.count - 1] == '\0', OOPS);
      lines.count -= 1; // Remove the null terminator to allow appending.
      need_more_lines = true;
      continue;
    }

    // If execution reaches here, compilation either succeeded or failed with a
    // different error. Clear the buffer for the next input cycle.
    need_more_lines = false;
    ByteBufferClear(&lines, vm);

    if (result != RESULT_SUCCESS)
      continue;

    // The compiled source becomes the "@main" function of the module. Execute it.
    Closure* _main = moduleGetMainFunction(vm, _module);
    ASSERT(_main != NULL, OOPS);
    result = vmCallFunction(vm, _main, 0, NULL, NULL);

  } while (!done);

  releaseHandle(vm, module);

  return result;
}

/*****************************************************************************/
/* RUNTIME                                                                   */
/*****************************************************************************/

void SetRuntimeError(VM* vm, const char* message) {
  CHECK_FIBER_EXISTS(vm);
  VM_SET_ERROR(vm, newString(vm, message));
}

void SetRuntimeErrorFmt(VM* vm, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  VM_SET_ERROR(vm, newStringVaArgs(vm, fmt, args));
  va_end(args);
}

void* GetThis(const VM* vm) {
  CHECK_FIBER_EXISTS(vm);
  ASSERT(IS_OBJ_TYPE(vm->fiber->thiz, OBJ_INST), OOPS);
  Instance* inst = (Instance*) AS_OBJ(vm->fiber->thiz);
  ASSERT(inst->native != NULL, OOPS);
  return inst->native;
}

int GetArgc(const VM* vm) {
  CHECK_FIBER_EXISTS(vm);
  return ARGC;
}

bool CheckArgcRange(VM* vm, int argc, int min, int max) {
  CHECK_FIBER_EXISTS(vm);
  ASSERT(min <= max, "invalid argc range (min > max).");

  if (argc < min) {
    char buff[STR_INT_BUFF_SIZE];
    sprintf(buff, "%d", min);
    VM_SET_ERROR(vm, stringFormat(vm, "Expected at least $ argument(s).", buff));
    return false;

  } else if (argc > max) {
    char buff[STR_INT_BUFF_SIZE];
    sprintf(buff, "%d", max);
    VM_SET_ERROR(vm, stringFormat(vm, "Expected at most $ argument(s).", buff));
    return false;
  }

  return true;
}

// Set error for incompatible type provided as an argument.
// TODO: Include the actual type received in the error message.
#define ERR_INVALID_SLOT_TYPE(slot, ty_name) \
  do { \
    char buff[STR_INT_BUFF_SIZE]; \
    sprintf(buff, "%d", slot); \
    VM_SET_ERROR(vm, stringFormat(vm, "Argument $ must be a $.", buff, ty_name)); \
  } while (false)

// FIXME: Encourage use of `GetSlotBool()` if strictly extracting boolean values.
bool ValidateSlotBool(VM* vm, int slot, bool* value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);

  Var val = ARG(slot);
  if (!IS_BOOL(val)) {
    ERR_INVALID_SLOT_TYPE(slot, "Boolean");
    return false;
  }

  if (value)
    *value = AS_BOOL(val);
  return true;
}

bool ValidateSlotNumber(VM* vm, int slot, double* value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);

  Var val = ARG(slot);
  if (!IS_NUM(val)) {
    ERR_INVALID_SLOT_TYPE(slot, "Number");
    return false;
  }

  if (value)
    *value = AS_NUM(val);
  return true;
}

bool ValidateSlotInteger(VM* vm, int slot, int32_t* value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);

  double n;
  if (!ValidateSlotNumber(vm, slot, &n))
    return false;

  if (floor(n) != n) {
    VM_SET_ERROR(vm, newString(vm, "Expected an integer got float."));
    return false;
  }

  if (value)
    *value = (int32_t) n;
  return true;
}

bool ValidateSlotString(VM* vm, int slot, const char** value, uint32_t* length) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);

  Var val = ARG(slot);
  if (!IS_OBJ_TYPE(val, OBJ_STRING)) {
    ERR_INVALID_SLOT_TYPE(slot, "String");
    return false;
  }
  String* str = (String*) AS_OBJ(val);
  if (value)
    *value = str->data;
  if (length)
    *length = str->length;
  return true;
}

bool ValidateSlotType(VM* vm, int slot, VarType type) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);
  if (getVarType(ARG(slot)) != type) {
    ERR_INVALID_SLOT_TYPE(slot, getVarTypeName(type));
    return false;
  }

  return true;
}

bool ValidateSlotInstanceOf(VM* vm, int slot, int cls) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(slot);
  VALIDATE_SLOT_INDEX(cls);

  Var instance = ARG(slot), class_ = SLOT(cls);
  if (!varIsType(vm, instance, class_)) {
    // If [class_] is not a valid class, it's already an error.
    if (VM_HAS_ERROR(vm))
      return false;
    ERR_INVALID_SLOT_TYPE(slot, ((Class*) AS_OBJ(class_))->name->data);
    return false;
  }

  return true;
}

bool IsSlotInstanceOf(VM* vm, int inst, int cls, bool* val) {
  CHECK_ARG_NULL(val);
  VALIDATE_SLOT_INDEX(inst);
  VALIDATE_SLOT_INDEX(cls);

  Var instance = ARG(inst), class_ = SLOT(cls);
  *val = varIsType(vm, instance, class_);
  return !VM_HAS_ERROR(vm);
}

void reserveSlots(VM* vm, int count) {
  if (vm->fiber == NULL)
    vm->fiber = newFiber(vm, NULL);
  int needed = (int) (vm->fiber->ret - vm->fiber->stack) + count;
  vmEnsureStackSize(vm, vm->fiber, needed);
}

int GetSlotsCount(VM* vm) {
  CHECK_FIBER_EXISTS(vm);
  return vm->fiber->stack_size - (int) (vm->fiber->ret - vm->fiber->stack);
}

VarType GetSlotType(VM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  return getVarType(SLOT(index));
}

bool GetSlotBool(VM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  Var value = SLOT(index);
  return toBool(value);
}

double GetSlotNumber(VM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  Var value = SLOT(index);
  ASSERT(IS_NUM(value), "Slot value wasn't a Number.");
  return AS_NUM(value);
}

const char* GetSlotString(VM* vm, int index, uint32_t* length) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  Var value = SLOT(index);
  ASSERT(IS_OBJ_TYPE(value, OBJ_STRING), "Slot value wasn't a String.");
  if (length != NULL)
    *length = ((String*) AS_OBJ(value))->length;
  return ((String*) AS_OBJ(value))->data;
}

void* GetSlotPointer(VM* vm, int index, void* native_ptr, Destructor destructor) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  Var value = SLOT(index);

  ASSERT(IS_OBJ_TYPE(value, OBJ_POINTER), "Slot value wasn't a Pointer.");

  return ((Pointer*) AS_OBJ(value))->native_ptr;
}

Handle* GetSlotHandle(VM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  return vmNewHandle(vm, SLOT(index));
}

void* GetSlotNativeInstance(VM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  Var value = SLOT(index);
  ASSERT(IS_OBJ_TYPE(value, OBJ_INST), "Slot value wasn't an Instance");

  // TODO: If the native initializer (NewInstanceFn()) returned NULL,
  // [inst->native] will be null - handle.
  Instance* inst = (Instance*) AS_OBJ(value);
  ASSERT(inst->native != NULL, "Slot value wasn't a Native Instance");

  return inst->native;
}

void setSlotNull(VM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_NULL);
}

void setSlotBool(VM* vm, int index, bool value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_BOOL(value));
}

void setSlotNumber(VM* vm, int index, double value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_NUM(value));
}

void setSlotString(VM* vm, int index, const char* value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_OBJ(newString(vm, value)));
}

void setSlotPointer(VM* vm, int index, void* native_ptr, Destructor destructor) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_OBJ(newPointer(vm, native_ptr, destructor)));
}

void setSlotClosure(VM* vm, int index, const char* name, nativeFn fptr,
                    int arity, const char* docstring) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_OBJ(newNativeClosure(vm, name, fptr, arity, docstring)));
}

void setSlotStringLength(VM* vm, int index, const char* value, uint32_t length) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_OBJ(newStringLength(vm, value, length)));
}

void setSlotStringFmt(VM* vm, int index, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  SET_SLOT(index, VAR_OBJ(newStringVaArgs(vm, fmt, args)));
  va_end(args);
}

void setSlotHandle(VM* vm, int index, Handle* handle) {
  CHECK_FIBER_EXISTS(vm);
  CHECK_ARG_NULL(handle);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, handle->value);
}

uint32_t GetSlotHash(VM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  Var value = SLOT(index);
  ASSERT(!IS_OBJ(value) || isObjectHashable(AS_OBJ(value)->type), OOPS);
  return varHashValue(value);
}

bool setAttribute(VM* vm, int instance, const char* name, int value) {
  CHECK_FIBER_EXISTS(vm);
  CHECK_ARG_NULL(name);
  VALIDATE_SLOT_INDEX(instance);
  VALIDATE_SLOT_INDEX(value);

  String* sname = newString(vm, name);
  vmPushTempRef(vm, &sname->_super); // sname.
  varSetAttrib(vm, SLOT(instance), sname, SLOT(value), true);
  vmPopTempRef(vm); // sname.

  return !VM_HAS_ERROR(vm);
}

bool GetAttribute(VM* vm, int instance, const char* name, int index) {
  CHECK_FIBER_EXISTS(vm);
  CHECK_ARG_NULL(name);
  VALIDATE_SLOT_INDEX(instance);
  VALIDATE_SLOT_INDEX(index);

  String* sname = newString(vm, name);
  vmPushTempRef(vm, &sname->_super); // sname.
  SET_SLOT(index, varGetAttrib(vm, SLOT(instance), sname, true, false));
  vmPopTempRef(vm); // sname.

  return !VM_HAS_ERROR(vm);
}

static Var _newInstance(VM* vm, Class* cls, int argc, Var* argv) {
  Var instance = preConstructThis(vm, cls);
  if (VM_HAS_ERROR(vm))
    return VAR_NULL;

  bool pushed = false;
  if (IS_OBJ(instance)) {
    vmPushTempRef(vm, AS_OBJ(instance)); // instance.
    pushed = true;
  }

  Closure* init = getMagicMethod(cls, METHOD_INIT);
  if (init != NULL) {
    // for builtin classes, preConstructThis returns null,
    // and instance is returned by _init.
    vmCallMethod(vm, instance, init, argc, argv, IS_NULL(instance) ? &instance : NULL);
  }
  if (pushed)
    vmPopTempRef(vm); // instance.

  return instance;
}

bool NewInstance(VM* vm, int cls, int index, int argc, int argv) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  if (argc != 0) {
    VALIDATE_SLOT_INDEX(argv);
    VALIDATE_SLOT_INDEX(argv + argc - 1);
  }

  ASSERT(IS_OBJ_TYPE(SLOT(cls), OBJ_CLASS), "Slot value wasn't a class.");

  Class* class_ = (Class*) AS_OBJ(SLOT(cls));

  SET_SLOT(index, _newInstance(vm, class_, argc, vm->fiber->ret + argv));
  return !VM_HAS_ERROR(vm);
}

void NewRange(VM* vm, int index, double first, double last) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(newRange(vm, first, last)));
}

void NewList(VM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(newList(vm, 0)));
}

void NewMap(VM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(newMap(vm)));
}

void NewString(VM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(newString(vm, "")));
}

void NewPointer(VM* vm, int index, void* native_ptr, Destructor destructor) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(newPointer(vm, native_ptr, destructor)));
}

void NewClosure(VM* vm, int index, const char* name, nativeFn fptr, int arity,
                const char* docstring) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, VAR_OBJ(newNativeClosure(vm, name, fptr, arity, docstring)));
}

bool ListInsert(VM* vm, int list, int32_t index, int value) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(list);
  VALIDATE_SLOT_INDEX(value);

  ASSERT(IS_OBJ_TYPE(SLOT(list), OBJ_LIST), "Slot value wasn't a List");
  List* l = (List*) AS_OBJ(SLOT(list));
  if (index < 0)
    index = l->elements.count + index + 1;

  if (index < 0 || (uint32_t) index > l->elements.count) {
    VM_SET_ERROR(vm, newString(vm, "Index out of bounds."));
    return false;
  }

  listInsert(vm, l, (uint32_t) index, SLOT(value));
  return true;
}

bool ListPop(VM* vm, int list, int32_t index, int popped) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(list);
  if (popped >= 0)
    VALIDATE_SLOT_INDEX(popped);

  ASSERT(IS_OBJ_TYPE(SLOT(list), OBJ_LIST), "Slot value wasn't a List");
  List* l = (List*) AS_OBJ(SLOT(list));
  if (index < 0)
    index += l->elements.count;

  if (index < 0 || (uint32_t) index >= l->elements.count) {
    VM_SET_ERROR(vm, newString(vm, "Index out of bounds."));
    return false;
  }

  Var p = listRemoveAt(vm, l, index);
  if (popped >= 0)
    SET_SLOT(popped, p);
  return true;
}

uint32_t ListLength(VM* vm, int list) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(list);

  ASSERT(IS_OBJ_TYPE(SLOT(list), OBJ_LIST), "Slot value wasn't a List");
  List* l = (List*) AS_OBJ(SLOT(list));

  return l->elements.count;
}

bool CallFunction(VM* vm, int fn, int argc, int argv, int ret) {
  CHECK_FIBER_EXISTS(vm);
  ASSERT(IS_OBJ_TYPE(SLOT(fn), OBJ_CLOSURE), "Slot value wasn't a function");
  if (argc != 0) {
    VALIDATE_SLOT_INDEX(argv);
    VALIDATE_SLOT_INDEX(argv + argc - 1);
  }
  if (ret >= 0)
    VALIDATE_SLOT_INDEX(ret);

  // Calls a class == construct.
  if (IS_OBJ_TYPE(SLOT(fn), OBJ_CLASS)) {
    Var inst = _newInstance(vm, (Class*) AS_OBJ(SLOT(fn)), argc, vm->fiber->ret + argv);
    if (ret >= 0)
      SET_SLOT(ret, inst);
    return !VM_HAS_ERROR(vm);
  }

  if (IS_OBJ_TYPE(SLOT(fn), OBJ_CLOSURE)) {
    Closure* func = (Closure*) AS_OBJ(SLOT(fn));

    // Methods are not first class. Accessing a method will return a method
    // bind instance which has a reference to an instance and invoking it
    // will calls the method with that instance.
    ASSERT(!func->fn->is_method, OOPS);

    Var retval;
    vmCallFunction(vm, func, argc, vm->fiber->ret + argv, &retval);
    if (ret >= 0)
      SET_SLOT(ret, retval);
    return !VM_HAS_ERROR(vm);
  }

  VM_SET_ERROR(vm, newString(vm, "Expected a Callable."));
  return false;
}

bool CallMethod(VM* vm, int instance, const char* method, int argc, int argv, int ret) {
  CHECK_FIBER_EXISTS(vm);
  CHECK_ARG_NULL(method);
  VALIDATE_SLOT_INDEX(instance);
  if (argc != 0) {
    VALIDATE_SLOT_INDEX(argv);
    VALIDATE_SLOT_INDEX(argv + argc - 1);
  }
  if (ret >= 0)
    VALIDATE_SLOT_INDEX(ret);

  bool is_method = false;
  String* smethod = newString(vm, method);
  vmPushTempRef(vm, &smethod->_super); // smethod.
  Var callable = getMethod(vm, SLOT(instance), smethod, &is_method);
  vmPopTempRef(vm); // smethod.

  if (VM_HAS_ERROR(vm))
    return false;

  // Calls a class == construct.
  if (IS_OBJ_TYPE(callable, OBJ_CLASS)) {
    Var inst = _newInstance(vm, (Class*) AS_OBJ(callable), argc, vm->fiber->ret + argv);
    if (ret >= 0)
      SET_SLOT(ret, inst);
    return !VM_HAS_ERROR(vm);
  }

  if (IS_OBJ_TYPE(callable, OBJ_CLOSURE)) {
    Var retval;
    vmCallMethod(vm, SLOT(instance), (Closure*) AS_OBJ(callable), argc,
                 vm->fiber->ret + argv, &retval);
    if (ret >= 0)
      SET_SLOT(ret, retval);
    return !VM_HAS_ERROR(vm);
  }

  VM_SET_ERROR(vm, stringFormat(vm, "Instance has no method named '$'.", method));
  return false;
}

void PlaceThis(VM* vm, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);
  SET_SLOT(index, vm->fiber->thiz);
}

bool ImportModule(VM* vm, const char* path, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(index);

  String* path_ = newString(vm, path);
  vmPushTempRef(vm, &path_->_super); // path_
  Var module = vmImportModule(vm, NULL, path_);
  vmPopTempRef(vm); // path_

  SET_SLOT(index, module);
  return !VM_HAS_ERROR(vm);
}

void GetClass(VM* vm, int instance, int index) {
  CHECK_FIBER_EXISTS(vm);
  VALIDATE_SLOT_INDEX(instance);
  VALIDATE_SLOT_INDEX(index);

  SET_SLOT(index, VAR_OBJ(getClass(vm, SLOT(instance))));
}

#undef ERR_INVALID_ARG_TYPE
#undef ARG
#undef SLOT
#undef SET_SLOT
#undef ARGC

/*****************************************************************************/
/* INTERNAL                                                                  */
/*****************************************************************************/

// The default allocator that will be used to initialize the vm's configuration
// if the host doesn't provided any allocators for us.
static void* defaultRealloc(void* memory, size_t new_size, void* _) {
  if (new_size == 0) {
    free(memory);
    return NULL;
  }
  return realloc(memory, new_size);
}

void stderrWrite(VM* vm, const char* text) {
  fprintf(stderr, "%s", text);
}

void stdoutWrite(VM* vm, const char* text) {
  fprintf(stdout, "%s", text);
}

static char* stdinRead(VM* vm) {
  ByteBuffer buff;
  ByteBufferInit(&buff);
  char c;
  do {
    c = (char) fgetc(stdin);
    if (c == '\n')
      break;
    ByteBufferWrite(&buff, vm, (uint8_t) c);
  } while (c != EOF);
  ByteBufferWrite(&buff, vm, '\0');

  char* str = (char*) Realloc(vm, NULL, buff.count);
  memcpy(str, buff.data, buff.count);
  ByteBufferClear(&buff, vm);
  return str;
}

static char* loadScript(VM* vm, const char* path) {
  FILE* file = fopen(path, "r");
  if (file == NULL)
    return NULL;

  // Get the source length. In windows the ftell will includes the cariage
  // return when using ftell with fseek. But that's not an issue since
  // we'll be allocating more memory than needed for fread().
  fseek(file, 0, SEEK_END);
  size_t file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  // Allocate string + 1 for the NULL terminator.
  char* buff = (char*) Realloc(vm, NULL, file_size + 1);
  ASSERT(buff != NULL, "Realloc failed.");

  clearerr(file);
  size_t read = fread(buff, sizeof(char), file_size, file);
  ASSERT(read <= file_size, "fread() failed.");
  buff[read] = '\0';
  fclose(file);

  return buff;
}