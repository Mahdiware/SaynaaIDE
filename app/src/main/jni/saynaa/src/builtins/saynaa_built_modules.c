/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_built_modules.h"

#include "../runtime/saynaa_import.h"
#include "../shared/saynaa_validate.h"
#include "../utils/saynaa_debug.h"

// Create a module and add it to the vm's core modules, returns the module.
Module* newModuleInternal(VM* vm, const char* name) {
  String* _name = newString(vm, name);
  vmPushTempRef(vm, &_name->_super); // _name

  // Check if any module with the same name already exists and assert to the
  // hosting application.
  if (vmGetModule(vm, _name) != NULL) {
    ASSERT(false, stringFormat(vm, "A module named '$' already exists", name)->data);
  }

  Module* module = newModule(vm);
  module->context = newContext(vm);
  module->name = _name;
  module->initialized = true;
  vmPopTempRef(vm); // _name

  initializeModule(vm, module, false);
  return module;
}

// An internal function to add a function to the given [module].
void moduleAddFunctionInternal(VM* vm, Module* module, const char* name,
                               nativeFn fptr, int arity, const char* docstring) {
  Function* fn = newFunction(vm, name, (int) strlen(name), module, true, docstring, NULL);
  fn->native = fptr;
  fn->arity = arity;

  vmPushTempRef(vm, &fn->_super); // fn.
  Closure* closure = newClosure(vm, fn);
  moduleSetGlobal(vm, module, name, (uint32_t) strlen(name), VAR_OBJ(closure));
  vmPopTempRef(vm); // fn.
}

// 'lang' library methods.

saynaa_function(stdLangGC, "lang.gc() -> Number",
                "Trigger garbage collection and"
                " return the amount of bytes cleaned.") {
  size_t bytes_before = vm->bytes_allocated;
  vmCollectGarbage(vm);
  size_t garbage = bytes_before - vm->bytes_allocated;
  RET(VAR_NUM((double) garbage));
}

saynaa_function(stdLangDisas, "lang.disas([fn:Closure]) -> String",
                "Returns the disassembled opcode of [fn]. "
                "If omitted, disassembles the current module main body.") {
  int argc = ARGC;
  if (argc > 1) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  Function* fn = NULL;
  if (argc == 0) {
    if (vm->fiber == NULL || vm->fiber->frame_count == 0) {
      RET_ERR(
          newString(vm, "Cannot disassemble without an active call frame."));
    }

    CallFrame* frame = &vm->fiber->frames[vm->fiber->frame_count - 1];
    Module* module = frame->closure->fn->owner;
    ASSERT(module != NULL, OOPS);
    ASSERT(module->body != NULL, OOPS);
    fn = module->body->fn;

  } else {
    Closure* closure;
    if (!validateArgClosure(vm, 1, &closure))
      return;
    fn = closure->fn;
  }

  if (!validateCond(vm, !fn->is_native, "Cannot disassemble native functions."))
    return;

  String* out = dumpFunctionCode(vm, fn);
  if (out == NULL)
    return;
  RET(VAR_OBJ(out));
}

saynaa_function(stdLangBackTrace, "lang.backtrace() -> String",
                "Returns the backtrace as a string, each line is formated as "
                "'<function>;<file>;<line>\n'.") {
  // FIXME:
  // All of the bellow code were copied from "debug.c" file, consider
  // refactor the functionality in a way that it's possible to re use them.

  ByteBuffer bb;
  ByteBufferInit(&bb);

  Fiber* fiber = vm->fiber;
  ASSERT(fiber != NULL, OOPS);

  while (fiber) {
    for (int i = fiber->frame_count - 1; i >= 0; i--) {
      CallFrame* frame = &fiber->frames[i];
      const Function* fn = frame->closure->fn;

      // After fetching the instruction the ip will be inceased so we're
      // reducing it by 1. But stack overflows are occure before executing
      // any instruction of that function, so the instruction_index
      // possibly be -1 (set it to zero in that case).
      int instruction_index = (int) (frame->ip - fn->fn->opcodes.data) - 1;
      if (instruction_index == -1)
        instruction_index = 0;
      int line = fn->fn->oplines.data[instruction_index];

      // Note that path can be null.
      const char* path = (fn->owner->path) ? fn->owner->path->data : "<?>";
      const char* fn_name = (fn->name) ? fn->name : "<?>";

      ByteBufferAddStringFmt(&bb, vm, "%s;%s;%i\n", fn_name, path, line);
    }

    if (fiber->caller)
      fiber = fiber->caller;
    else
      fiber = fiber->native;
  }

  // bb.count not including the null byte and which is the length.
  String* bt = newStringLength(vm, (char*) bb.data, bb.count);
  vmPushTempRef(vm, &bt->_super); // bt.
  ByteBufferClear(&bb, vm);
  vmPopTempRef(vm); // bt.

  RET(VAR_OBJ(bt));
}

saynaa_function(stdLangModules, "lang.modules() -> List",
                "Returns the list of all registered modules.") {
  List* list = newList(vm, 8);
  vmPushTempRef(vm, &list->_super); // list.
  for (uint32_t i = 0; i < vm->modules->capacity; i++) {
    if (!IS_UNDEF(vm->modules->entries[i].key)) {
      Var entry = vm->modules->entries[i].value;
      ASSERT(IS_OBJ_TYPE(entry, OBJ_MODULE), OOPS);
      Module* module = (Module*) AS_OBJ(entry);
      ASSERT(module->name != NULL, OOPS);
      if (module->name->data[0] == SPECIAL_NAME_CHAR) {
        continue;
      }
      listAppend(vm, list, entry);
    }
  }
  vmPopTempRef(vm); // list.
  RET(VAR_OBJ(list));
}

#ifdef DEBUG
saynaa_function(stdLangDebugBreak, "lang.debug_break() -> Null",
                "A debug function for development (will be removed).") {
  DEBUG_BREAK();
}
#endif

saynaa_function(stdModuleLoad, "package.load(name:String) -> Module",
                "Load import the module with [name] and returns it. "
                "It won't be imported to the current scope.") {
  String* name;
  if (!validateArgString(vm, 1, &name))
    return;

  String* from = NULL;
  if (vm->fiber->frame_count > 0) {
    CallFrame* frame = &vm->fiber->frames[vm->fiber->frame_count - 1];
    from = frame->closure->fn->owner->path;
  }

  Var module = vmImportModule(vm, from, name);
  if (VM_HAS_ERROR(vm))
    return;

  RET(module);
}

void initializeBuiltinModules(VM* vm) {
#define MODULE_ADD_FN(module, name, fn, argc) \
  moduleAddFunctionInternal(vm, module, name, fn, argc, DOCSTRING(fn))

#define NEW_MODULE(module, name_string) \
  Module* module = newModuleInternal(vm, name_string); \
  vmPushTempRef(vm, &module->_super); /* module */ \
  vmRegisterModule(vm, module, module->name); \
  vmPopTempRef(vm) /* module */

  NEW_MODULE(lang, "lang");
  MODULE_ADD_FN(lang, "gc", stdLangGC, 0);
  MODULE_ADD_FN(lang, "disas", stdLangDisas, -1);
  MODULE_ADD_FN(lang, "backtrace", stdLangBackTrace, 0);
  MODULE_ADD_FN(lang, "modules", stdLangModules, 0);
#ifdef DEBUG
  MODULE_ADD_FN(lang, "debug_break", stdLangDebugBreak, 0);
#endif

  NEW_MODULE(package, "package");
  MODULE_ADD_FN(package, "load", stdModuleLoad, 1);
  moduleSetGlobal(vm, package, "path", 4, VAR_OBJ(vm->search_paths));

  moduleSetGlobal(vm, package, "searchers", 9, VAR_OBJ(vm->searchers));
  Closure* stdSearcher = newNativeClosure(vm, "standardSearcher", vmStandardSearcher,
                                          1, "standard searcher");
  listAppend(vm, vm->searchers, VAR_OBJ(stdSearcher));

#undef MODULE_ADD_FN
#undef NEW_MODULE
}
