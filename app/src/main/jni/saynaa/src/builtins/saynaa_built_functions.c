/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_built_functions.h"

#include "../runtime/saynaa_import.h"
#include "../shared/saynaa_bytecode.h"

static void _listJoinImpl(VM* vm, List* list, String* sep) {
  ByteBuffer buff;
  ByteBufferInit(&buff);

  for (uint32_t i = 0; i < list->elements.count; i++) {
    String* str = varToString(vm, list->elements.data[i], false);
    if (str == NULL)
      RET(VAR_NULL);
    vmPushTempRef(vm, &str->_super); // elem
    if (sep != NULL && i != 0) {
      ByteBufferAddString(&buff, vm, sep->data, sep->length);
    }
    ByteBufferAddString(&buff, vm, str->data, str->length);
    vmPopTempRef(vm); // elem
  }

  String* str = newStringLength(vm, (const char*) buff.data, buff.count);
  ByteBufferClear(&buff, vm);
  RET(VAR_OBJ(str));
}

// Add all the methods recursively to the lits used for generating a list of
// attributes for the 'dir()' function.
static void _collectMethods(VM* vm, List* list, Class* cls) {
  if (cls == NULL)
    return;

  for (uint32_t i = 0; i < cls->methods.count; i++) {
    listAppend(vm, list, VAR_OBJ(newString(vm, cls->methods.data[i]->fn->name)));
  }
  _collectMethods(vm, list, cls->super_class);
}

saynaa_function(coreHelp, "help([value:Closure|MethodBind|Class]) -> Null",
                "It'll print the docstring the object and return.") {
  int argc = ARGC;
  if (argc != 0 && argc != 1) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  if (argc == 0) {
    // If there ins't an io function callback, we're done.
    if (vm->config.stdout_write == NULL)
      RET(VAR_NULL);
    vm->config.stdout_write(vm, "Saynaa Language\nA simple, embedded scripting "
                                "language.\nUsage: help(object)\n");

  } else if (argc == 1) {
    if (vm->config.stdout_write == NULL)
      RET(VAR_NULL);
    Var value = ARG(1);

    if (IS_OBJ_TYPE(value, OBJ_CLOSURE)) {
      Closure* closure = (Closure*) AS_OBJ(value);
      // If there ins't an io function callback, we're done.

      if (closure->fn->docstring != NULL) {
        vm->config.stdout_write(vm, closure->fn->docstring);
        vm->config.stdout_write(vm, "\n\n");
      } else {
        vm->config.stdout_write(vm, "function '");
        vm->config.stdout_write(vm, closure->fn->name);
        vm->config.stdout_write(vm, "()' doesn't have a docstring.\n");
      }
    } else if (IS_OBJ_TYPE(value, OBJ_METHOD_BIND)) {
      MethodBind* mb = (MethodBind*) AS_OBJ(value);
      // If there ins't an io function callback, we're done.

      if (mb->method->fn->docstring != NULL) {
        vm->config.stdout_write(vm, mb->method->fn->docstring);
        vm->config.stdout_write(vm, "\n\n");
      } else {
        vm->config.stdout_write(vm, "method '");
        vm->config.stdout_write(vm, mb->method->fn->name);
        vm->config.stdout_write(vm, "()' doesn't have a docstring.\n");
      }
    } else if (IS_OBJ_TYPE(value, OBJ_CLASS)) {
      Class* cls = (Class*) AS_OBJ(value);
      if (cls->docstring != NULL) {
        vm->config.stdout_write(vm, cls->docstring);
        vm->config.stdout_write(vm, "\n\n");
      } else {
        vm->config.stdout_write(vm, "class '");
        vm->config.stdout_write(vm, cls->name->data);
        vm->config.stdout_write(vm, "' doesn't have a docstring.\n");
      }
    } else {
      RET_ERR(newString(vm, "Expected a Closure, MethodBind or "
                            "Class to get help."));
    }
  }
}

saynaa_function(coreDir, "dir(v:Var) -> List[String]",
                "It'll return all the elements of the variable [v]. "
                "If [v] is a module it'll return the names of globals, "
                "functions, and classes. If it's an instance it'll "
                "return all the attributes and methods.") {
  Var v = ARG(1);
  switch (getVarType(v)) {
    case vNULL:
    case vBOOL:
    case vNUMBER:
    case vSTRING:
    case vLIST:
    case vMAP:
    case vRANGE:
    case vCLOSURE:
    case vFIBER:
    case vMETHOD_BIND:
    case vPOINTER:
      {
        List* list = newList(vm, 8);
        vmPushTempRef(vm, &list->_super); // list.
        _collectMethods(vm, list, getClass(vm, v));
        vmPopTempRef(vm); // list.
        RET(VAR_OBJ(list));
      }

    case vMODULE:
      {
        Module* m = (Module*) AS_OBJ(v);
        List* list = newList(vm, 8);
        vmPushTempRef(vm, &list->_super); // list.
        for (uint32_t i = 0; i < m->context->globals.count; i++) {
          Var name = m->context->constants.data[m->context->global_names.data[i]];
          ASSERT(IS_OBJ_TYPE(name, OBJ_STRING), OOPS);
          if (((String*) AS_OBJ(name))->data[0] == SPECIAL_NAME_CHAR) {
            continue;
          }
          listAppend(vm, list, name);
        }
        vmPopTempRef(vm); // list.
        RET(VAR_OBJ(list));
      }
      break;

    case vCLASS:
      {
        Class* cls = (Class*) AS_OBJ(v);
        List* list = newList(vm, 8);
        vmPushTempRef(vm, &list->_super); // list.
        _collectMethods(vm, list, cls);
        // TODO: if we add static variables to classes it should be
        // added here as well.
        vmPopTempRef(vm); // list.
        RET(VAR_OBJ(list));
      }
      break;

    case vINSTANCE:
      {
        Instance* inst = (Instance*) AS_OBJ(v);
        List* list = newList(vm, 8);
        vmPushTempRef(vm, &list->_super); // list.
        for (uint8_t i = 0; i < inst->inline_attrib_count; i++) {
          if (inst->inline_attrib_names[i] != NULL) {
            listAppend(vm, list, VAR_OBJ(inst->inline_attrib_names[i]));
          }
        }
        if (inst->attribs != NULL) {
          for (uint32_t i = 0; i < inst->attribs->capacity; i++) {
            Var key = (inst->attribs->entries + i)->key;
            if (!IS_UNDEF(key)) {
              ASSERT(IS_OBJ_TYPE(key, OBJ_STRING), OOPS);
              listAppend(vm, list, key);
            }
          }
        }
        _collectMethods(vm, list, inst->cls);
        vmPopTempRef(vm); // list.
        RET(VAR_OBJ(list));
      }
      break;
  }

  UNREACHABLE();
}

saynaa_function(
    coreAssert, "assert(condition:Bool [, msg:String]) -> Null",
    "If the condition is false it'll terminate the current fiber with the "
    "optional error message") {
  int argc = ARGC;
  if (argc != 1 && argc != 2) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  if (!toBool(ARG(1))) {
    String* msg = NULL;

    if (argc == 2) {
      if (!IS_OBJ_TYPE(ARG(2), OBJ_STRING)) {
        msg = varToString(vm, ARG(2), false);
        if (msg == NULL)
          return; //< Error at _to_string override.

      } else {
        msg = (String*) AS_OBJ(ARG(2));
      }

      vmPushTempRef(vm, &msg->_super); // msg.
      VM_SET_ERROR(vm, stringFormat(vm, "Assertion failed: '@'.", msg));
      vmPopTempRef(vm); // msg.
    } else {
      VM_SET_ERROR(vm, newString(vm, "Assertion failed."));
    }
  }
}

saynaa_function(coreError, "error(value:var) -> Null",
                "Terminates the current fiber with the given error value.") {
  String* msg;
  if (!IS_OBJ_TYPE(ARG(1), OBJ_STRING)) {
    msg = varToString(vm, ARG(1), false);
    if (msg == NULL)
      return; // Error from _to_string
  } else {
    msg = (String*) AS_OBJ(ARG(1));
  }
  vmPushTempRef(vm, &msg->_super); // msg.
  VM_SET_ERROR(vm, msg);
  vmPopTempRef(vm); // msg.
}

saynaa_function(coreBin, "bin(value:Number) -> String",
                "Returns as a binary value string with '0b' prefix.") {
  int64_t value;
  if (!validateInteger(vm, ARG(1), &value, "Argument 1"))
    return;

  char buff[STR_BIN_BUFF_SIZE];

  bool negative = (value < 0) ? true : false;
  if (negative)
    value = -value;

  char* ptr = buff + STR_BIN_BUFF_SIZE - 1;
  *ptr-- = '\0'; // NULL byte at the end of the string.

  if (value != 0) {
    while (value > 0) {
      *ptr-- = '0' + (value & 1);
      value >>= 1;
    }
  } else {
    *ptr-- = '0';
  }

  *ptr-- = 'b';
  *ptr-- = '0';
  if (negative)
    *ptr-- = '-';

  uint32_t length = (uint32_t) ((buff + STR_BIN_BUFF_SIZE - 1) - (ptr + 1));
  RET(VAR_OBJ(newStringLength(vm, ptr + 1, length)));
}

saynaa_function(coreHex, "hex(value:Number) -> String",
                "Returns as a hexadecimal value string with '0x' prefix.") {
  int64_t value;
  if (!validateInteger(vm, ARG(1), &value, "Argument 1"))
    return;

  char buff[STR_HEX_BUFF_SIZE];

  char* ptr = buff;
  if (value < 0)
    *ptr++ = '-';
  *ptr++ = '0';
  *ptr++ = 'x';

  if (value > UINT32_MAX || value < -(int64_t) (UINT32_MAX)) {
    VM_SET_ERROR(vm, newString(vm, "Integer is too large."));
    RET(VAR_NULL);
  }

  // TODO: sprintf limits only to 8 character hex value, we need to do it
  // outthiz for a maximum of 16 character long (see bin() for reference).
  uint32_t _x = (uint32_t) ((value < 0) ? -value : value);
  int length = sprintf(ptr, "%x", _x);

  RET(VAR_OBJ(newStringLength(vm, buff, (uint32_t) ((ptr + length) - (char*) (buff)))));
}

saynaa_function(
    coreYield, "yield([value:Var]) -> Var",
    "Return the current function with the yield [value] to current running "
    "fiber. If the fiber is resumed, it'll run from the next statement of the "
    "yield() call. If the fiber resumed with with a value, the return value of "
    "the yield() would be that value otherwise null.") {
  int argc = ARGC;
  if (argc > 1) { // yield() or yield(val).
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  vmYieldFiber(vm, (argc == 1) ? &ARG(1) : NULL);
}

saynaa_function(coreToString, "str(valueVar) -> String",
                "Returns the string representation of the value.") {
  String* str = varToString(vm, ARG(1), false);
  if (str == NULL)
    RET(VAR_NULL);
  RET(VAR_OBJ(str));
}

saynaa_function(coreType, "type(value:Var) -> String", "Returns the type of the value.") {
  const char* type_name = varTypeName(ARG(1));
  RET(VAR_OBJ(newString(vm, type_name)));
}

saynaa_function(coreToInt, "int(value:Num) -> Integer",
                "Returns the integer value"
                " of the number argument without decimal.") {
  double num;
  if (!validateNumeric(vm, ARG(1), &num, "Argument 1"))
    return;

  RET(VAR_NUM((int) num));
}

saynaa_function(coreChr, "chr(value:Num) -> String",
                "Returns the ASCII string value of the integer argument.") {
  int64_t num;
  if (!validateInteger(vm, ARG(1), &num, "Argument 1"))
    return;

  if (!(0 <= num && num <= 0xff)) {
    RET_ERR(newString(vm, "The number should be in range 0x00 to 0xff."));
  }

  char c = (char) num;
  RET(VAR_OBJ(newStringLength(vm, &c, 1)));
}

saynaa_function(coreOrd, "ord(value:String) -> Number",
                "Returns integer value of the given ASCII character.") {
  String* c;
  if (!validateArgString(vm, 1, &c))
    return;
  if (c->length != 1) {
    RET_ERR(newString(vm, "Expected a string of length 1."));

  } else {
    RET(VAR_NUM((double) c->data[0]));
  }
}

saynaa_function(coreMin, "min(a:Var, b:Var) -> Bool", "Returns minimum of [a] and [b].") {
  Var a = ARG(1), b = ARG(2);
  Var islesser = varLesser(vm, a, b);
  if (VM_HAS_ERROR(vm))
    RET(VAR_NULL);

  if (toBool(islesser))
    RET(a);
  RET(b);
}

saynaa_function(coreMax, "max(a:var, b:var) -> Bool", "Returns maximum of [a] and [b].") {
  Var a = ARG(1), b = ARG(2);
  Var islesser = varLesser(vm, a, b);
  if (VM_HAS_ERROR(vm))
    RET(VAR_NULL);

  if (toBool(islesser))
    RET(b);
  RET(a);
}

saynaa_function(
    corePrint, "print(...) -> Null",
    "Write each argument as space seperated, to the stdout and ends with a "
    "newline.") {
  // If the host application doesn't provide any write function, discard the
  // output.
  if (vm->config.stdout_write == NULL)
    return;

  for (int i = 1; i <= ARGC; i++) {
    if (i != 1)
      vm->config.stdout_write(vm, " ");
    String* str = varToString(vm, ARG(i), false);
    if (str == NULL)
      RET(VAR_NULL);
    vm->config.stdout_write(vm, str->data);
  }

  vm->config.stdout_write(vm, "\n");
}

saynaa_function(
    coreInput, "input([msg:Var]) -> String",
    "Read a line from stdin and returns it without the line ending. Accepting "
    "an optional argument [msg] and prints it before reading.") {
  int argc = ARGC;
  if (argc > 1) { // input() or input(str).
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  // If the host application doesn't provide any write function, return.
  if (vm->config.stdin_read == NULL)
    return;

  if (argc == 1) {
    String* str = varToString(vm, ARG(1), false);
    if (str == NULL)
      RET(VAR_NULL);
    vm->config.stdout_write(vm, str->data);
  }

  char* str = vm->config.stdin_read(vm);
  if (str == NULL) { //< Input failed !?
    RET_ERR(newString(vm, "Input function failed."));
  }

  String* line = newString(vm, str);
  Realloc(vm, str, 0);
  RET(VAR_OBJ(line));
}

saynaa_function(
    coreExit, "exit([value:Number]) -> Null",
    "Exit the process with an optional exit code provided by the argument "
    "[value]. The default exit code is would be 0.") {
  int argc = ARGC;
  if (argc > 1) { // exit() or exit(val).
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  int64_t value = 0;
  if (argc == 1) {
    if (!validateInteger(vm, ARG(1), &value, "Argument 1"))
      return;
  }

  // FreeVM(vm);
  //  TODO: this actually needs to be the VM fiber being set to null though.
  exit((int) value);
}

saynaa_function(coreEval, "eval(expression:String) -> Var",
                "Evaluate an expression and returns the result.\n"
                "Only global variables can be used in the expression.") {
  String* expr;
  if (!validateArgString(vm, 1, &expr))
    return;

  String* code = stringFormat(vm, "return (@)", expr);
  vmPushTempRef(vm, &code->_super); // code.
  {
    CallFrame* frame = NULL;
    if (vm->fiber->frame_count > 0) {
      frame = &vm->fiber->frames[vm->fiber->frame_count - 1];
    } else if (vm->fiber->native && vm->fiber->native->frame_count > 0) {
      frame = &vm->fiber->native->frames[vm->fiber->native->frame_count - 1];
    }

    if (frame == NULL) {
      VM_SET_ERROR(
          vm, newString(vm, "Cannot eval without an active module context."));
      vmPopTempRef(vm); // code.
      RET(VAR_NULL);
    }

    Module* current_module = frame->closure->fn->owner;

    Module* new_module = newModule(vm);
    new_module->context = newContext(vm);
    vmPushTempRef(vm, &new_module->_super); // new_module.
    {
      // let global variables become available
      VarBufferConcat(&new_module->context->constants, vm,
                      &current_module->context->constants);
      VarBufferConcat(&new_module->context->globals, vm,
                      &current_module->context->globals);
      UintBufferConcat(&new_module->context->global_names, vm,
                       &current_module->context->global_names);

      CompileOptions options = newCompilerOptions();
      options.runtime = true;
      Result result = compile(vm, new_module, code->data, &options);

      if (result == RESULT_SUCCESS) {
        Var ret = VAR_NULL;
        vmCallFunction(vm, new_module->body, 0, NULL, &ret);
        ARG(0) = ret;
      }
    }
    vmPopTempRef(vm); // new_module.
  }
  vmPopTempRef(vm); // code.
}

/*
0:	CLONE     Create child module from parent context
1:	SHARED    Execute inside parent module, changes affect parent
2:	NEW       Create empty module, no parent context
*/
saynaa_function(
    coreLoadFile, "loadfile([module:Module], path:String, [mode:Number=0]) -> Var",
    "Load a script file from the given [path] and returns the module object. "
    "[mode] specifies the loading mode.\n"
    "0: CLONE - Create child module from parent context\n"
    "1: SHARED - Execute inside parent module, changes affect parent\n"
    "2: NEW - Create empty module, no parent context") {
  int argc = ARGC;
  if (argc > 3 || argc < 1) {
    RET_ERR(
        newString(vm, "Invalid argument count. Expected 1 to 3 arguments."));
  }

  String* path;
  Module* current_module;
  int64_t mode = 0;
  if (argc == 1) {
    // argc == 1, the path is the first argument
    // and Get the current module from the current call frame.
    if (!validateArgString(vm, 1, &path))
      return;

    CallFrame* frame = &vm->fiber->frames[vm->fiber->frame_count - 1];
    current_module = frame->closure->fn->owner;
  } else if (argc == 2) {
    // argc == 2, the first argument is either a module or a path,
    // and the second is either a path or a mode.

    Var v1 = ARG(1);
    Var v2 = ARG(2);

    if (IS_OBJ_TYPE(v1, OBJ_MODULE) && IS_OBJ_TYPE(v2, OBJ_STRING)) {
      // arg1 is module, arg2 is path
      current_module = (Module*) AS_OBJ(v1);
      path = (String*) AS_OBJ(v2);
    } else if (IS_OBJ_TYPE(v1, OBJ_STRING) && IS_NUM(v2)) {
      // arg1 is path, arg2 is mode
      CallFrame* frame = &vm->fiber->frames[vm->fiber->frame_count - 1];
      current_module = frame->closure->fn->owner;
      path = (String*) AS_OBJ(v1);
      mode = (int64_t) AS_NUM(v2);
    } else {
      RET_ERR(newString(
          vm, "loadfile: Invalid argument types. Expected (Module, String) "
              "or (String, Number)."));
    }
  } else if (argc == 3) {
    // argc == 3, the first argument is the module, the second is the path, and the third is the mode.
    if (!validateArgModule(vm, 1, &current_module))
      return;
    if (!validateArgString(vm, 2, &path))
      return;

    if (!validateInteger(vm, ARG(3), &mode, "Argument 3"))
      return;

  } else {
    RET_ERR(newString(
        vm, "loadfile: Invalid argument count. Expected 1 to 3 arguments."));
  }

  if (mode < 0 || mode > 2) {
    RET_ERR(newString(
        vm, "loadfile: invalid mode. Expected 0: CLONE, 1: SHARED or 2: NEW."));
  }

  Module* new_module;
  if (mode == 1) {
    new_module = newModule(vm);
    new_module->context = current_module->context;
  } else {
    new_module = newModule(vm);
    new_module->context = newContext(vm);
  }

  vmPushTempRef(vm, &new_module->_super); // new_module.
  {
    if (mode == 0) {
      VarBufferConcat(&new_module->context->constants, vm,
                      &current_module->context->constants);
      VarBufferConcat(&new_module->context->globals, vm,
                      &current_module->context->globals);
      UintBufferConcat(&new_module->context->global_names, vm,
                       &current_module->context->global_names);
    }

    if (vm->config.resolve_path_fn == NULL) {
      new_module->context = NULL;
      vmPopTempRef(vm); // new_module.
      return;
    }

    char* from_path = NULL;
    if (vm->fiber->frame_count > 0) {
      CallFrame* frame = &vm->fiber->frames[vm->fiber->frame_count - 1];
      from_path = frame->closure->fn->owner->path == NULL
                      ? NULL
                      : frame->closure->fn->owner->path->data;
    }

    char* resolve_path = vm->config.resolve_path_fn(vm, from_path, path->data);

    if (resolve_path == NULL) {
      vmPopTempRef(vm); // new_module.
      return;
    }

    // Create the `resolve` string for importScript in all modes so the
    // resolver is always provided. Only create `_name` for newly created
    // modules (CLONE/NEW) where we want to set the module's name/path.
    String* _name = NULL;
    String* resolve = newString(vm, resolve_path);
    vmPushTempRef(vm, &resolve->_super);

    // Convert the resolved path to a module name by replacing '/' with '.'
    _name = newString(vm, resolve_path);
    vmPushTempRef(vm, &_name->_super);
    for (char* c = _name->data; c < _name->data + _name->length; c++) {
      if (*c == '/')
        *c = '.';
    }
    _name->hash = utilHashString(_name->data);

    new_module->name = new_module->name == NULL ? _name : new_module->name;
    new_module->path = new_module->path == NULL ? resolve : new_module->path;

    if (!importScript(vm, new_module, resolve, true, false)) {
      if (resolve != NULL)
        vmPopTempRef(vm); // resolve.
      if (_name != NULL)
        vmPopTempRef(vm); // _name.
      vmPopTempRef(vm);   // new_module.
      return;
    }

    Var ret = VAR_NULL;
    vmCallFunction(vm, new_module->body, 0, NULL, &ret);

    if (ret == VAR_NULL) {
      ARG(0) = VAR_OBJ(new_module);
    } else {
      ARG(0) = ret;
    }
    if (resolve != NULL)
      vmPopTempRef(vm); // resolve.
    if (_name != NULL)
      vmPopTempRef(vm); // _name.
  }
  vmPopTempRef(vm); // new_module.
}

saynaa_function(coreDefine, "define(variable:String, value:Var) -> Null",
                "Define a global variable with the name [variable] and value "
                "[value] in the current module.") {
  String* variable;
  if (!validateArgString(vm, 1, &variable))
    return;

  Var valua = ARG(2);

  CallFrame* frame = &vm->fiber->frames[vm->fiber->frame_count - 1];
  Module* current_module = frame->closure->fn->owner;

  moduleSetGlobal(vm, current_module, variable->data, variable->length, valua);

  RET(VAR_NULL);
}

saynaa_function(
    coreDelete, "delete(variable:String|instance:Var) -> Null",
    "If [variable] is a String, delete the global variable with that name. "
    "If it's an instance, call _del() when defined.") {
  Var target = ARG(1);
  if (IS_OBJ_TYPE(target, OBJ_STRING)) {
    String* variable = (String*) AS_OBJ(target);

    CallFrame* frame = &vm->fiber->frames[vm->fiber->frame_count - 1];
    Module* current_module = frame->closure->fn->owner;

    if (!moduleDeleteGlobal(vm, current_module, variable->data, variable->length)) {
      VM_SET_ERROR(vm, stringFormat(vm, "Name '@' is not defined.", variable));
    }
    RET(VAR_NULL);
  }

  if (IS_OBJ_TYPE(target, OBJ_INST)) {
    Instance* inst = (Instance*) AS_OBJ(target);
    Closure* del = getMagicMethod(inst->cls, METHOD_DEL);
    if (del != NULL) {
      vmCallMethod(vm, target, del, 0, NULL, NULL);
    }
    RET(VAR_NULL);
  }

  RET_ERR(newString(vm, "delete() expects a String or an instance."));
}

saynaa_function(corePcall, "pcall(fn:Closure, ...args) -> List",
                "Calls function in protected mode."
                " Returns [success, result/error].") {
  int arg_count = ARGC;
  if (arg_count < 1) {
    RET_ERR(newString(vm, "Expected at least 1 argument (the function)."));
  }

  Closure* closure;
  if (!validateArgClosure(vm, 1, &closure))
    return;

  // Prepare arguments
  int call_argc = arg_count - 1;
  Var* call_argv = NULL;
  if (call_argc > 0) {
    call_argv = &vm->fiber->ret[2];
  }

  // Create fiber
  Fiber* fiber = newFiber(vm, closure);
  fiber->native = vm->fiber;
  vmPushTempRef(vm, &fiber->_super); // fiber.
  {
    bool success = vmPrepareFiber(vm, fiber, call_argc, call_argv);

    List* ret_list = newList(vm, 2);
    vmPushTempRef(vm, &ret_list->_super); // ret_list.
    {
      if (!success) {
        String* err = vm->fiber->error;
        vm->fiber->error = NULL; // clear error

        listAppend(vm, ret_list, VAR_FALSE);
        listAppend(vm, ret_list, VAR_OBJ(err));

      } else {
        // Suppress error reporting
        WriteFn old_stderr = vm->config.stderr_write;
        vm->config.stderr_write = NULL;

        Result result;
        Fiber* last = vm->fiber;

        if (fiber->closure->fn->is_native) {
          ASSERT(fiber->closure->fn->native != NULL,
                 "Native function was NULL");
          vm->fiber = fiber;
          fiber->closure->fn->native(vm);
          if (VM_HAS_ERROR(vm)) {
            result = RESULT_RUNTIME_ERROR;
          } else {
            result = RESULT_SUCCESS;
          }
        } else {
          result = vmRunFiber(vm, fiber);
        }

        // Restore stderr
        vm->config.stderr_write = old_stderr;

        // Restore fiber
        vm->fiber = last;

        if (result == RESULT_SUCCESS) {
          listAppend(vm, ret_list, VAR_TRUE);
          listAppend(vm, ret_list, *fiber->ret);
        } else {
          listAppend(vm, ret_list, VAR_FALSE);
          if (fiber->error) {
            listAppend(vm, ret_list, VAR_OBJ(fiber->error));
          } else {
            listAppend(vm, ret_list, VAR_OBJ(newString(vm, "Unknown Error")));
          }
          fiber->error = NULL;
        }
      }
    }
    vmPopTempRef(vm); // ret_list.
    RET(VAR_OBJ(ret_list));
  }
  vmPopTempRef(vm); // fiber.
}

saynaa_function(_coreHashable, "hashable(value:Var) -> Bool",
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

saynaa_function(_coreHash, "hash(value:Var) -> Number", "Returns the hash of the [value]") {
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

saynaa_function(
    coreListJoin,
    "list_join(thiz:List [, sep:String="
    "]) -> String",
    "Concatinate the elements of the list and return as a string.") {
  int argc = ARGC;
  if (argc != 1 && argc != 2) {
    RET_ERR(newString(vm, "Invalid argument count."));
  }

  List* list;
  String* sep = NULL;

  if (!validateArgList(vm, 1, &list))
    return;
  if (argc == 2)
    sep = varToString(vm, ARG(2), false);

  _listJoinImpl(vm, list, sep);
}

static void initializeBuiltinFN(VM* vm, Closure** bfn, const char* name, int length,
                                int arity, nativeFn ptr, const char* docstring) {
  Function* fn = newFunction(vm, name, length, NULL, true, docstring, NULL);
  fn->arity = arity;
  fn->native = ptr;
  vmPushTempRef(vm, &fn->_super); // fn.
  *bfn = newClosure(vm, fn);
  vmPopTempRef(vm); // fn.
}

void initializeBuiltinFunctions(VM* vm) {
#define INITIALIZE_BUILTIN_FN(name, fn, argc) \
  initializeBuiltinFN(vm, &vm->builtins_funcs[vm->builtins_count++], name, \
                      (int) strlen(name), argc, fn, DOCSTRING(fn));

  // General functions.
  INITIALIZE_BUILTIN_FN("help", coreHelp, -1);
  INITIALIZE_BUILTIN_FN("dir", coreDir, 1);
  INITIALIZE_BUILTIN_FN("assert", coreAssert, -1);
  INITIALIZE_BUILTIN_FN("hash", _coreHash, 1);
  INITIALIZE_BUILTIN_FN("hashable", _coreHashable, 1);
  INITIALIZE_BUILTIN_FN("bin", coreBin, 1);
  INITIALIZE_BUILTIN_FN("hex", coreHex, 1);
  INITIALIZE_BUILTIN_FN("yield", coreYield, -1);
  INITIALIZE_BUILTIN_FN("str", coreToString, 1);
  INITIALIZE_BUILTIN_FN("type", coreType, 1);
  INITIALIZE_BUILTIN_FN("int", coreToInt, 1);
  INITIALIZE_BUILTIN_FN("chr", coreChr, 1);
  INITIALIZE_BUILTIN_FN("ord", coreOrd, 1);
  INITIALIZE_BUILTIN_FN("min", coreMin, 2);
  INITIALIZE_BUILTIN_FN("max", coreMax, 2);
  INITIALIZE_BUILTIN_FN("print", corePrint, -1);
  INITIALIZE_BUILTIN_FN("input", coreInput, -1);
  INITIALIZE_BUILTIN_FN("exit", coreExit, -1);
  INITIALIZE_BUILTIN_FN("eval", coreEval, 1);
  INITIALIZE_BUILTIN_FN("loadfile", coreLoadFile, -1);
  INITIALIZE_BUILTIN_FN("define", coreDefine, 2);
  INITIALIZE_BUILTIN_FN("delete", coreDelete, 1);
  INITIALIZE_BUILTIN_FN("pcall", corePcall, -1);
  INITIALIZE_BUILTIN_FN("error", coreError, 1);

  INITIALIZE_BUILTIN_FN("list_join", coreListJoin, -1);

#undef INITIALIZE_BUILTIN_FN
}
