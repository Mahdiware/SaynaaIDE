#include "saynaa_internal.h"
#include "saynaa_bytecode.h"

static int find_builtin_index(VM* vm, const char* name) {
  if (vm == NULL || name == NULL)
    return -1;

  for (int i = 0; i < vm->builtins_count; i++) {
    Closure* closure = vm->builtins_funcs[i];
    if (closure != NULL && closure->fn != NULL && strcmp(closure->fn->name, name) == 0) {
      return i;
    }
  }
  return -1;
}

static void inject_builtin_global(VM* vm, Module* module, const char* name) {
  if (vm == NULL || module == NULL || name == NULL)
    return;

  int index = find_builtin_index(vm, name);
  if (index < 0)
    return;

  moduleSetGlobal(vm, module, name, (uint32_t) strlen(name), VAR_OBJ(vm->builtins_funcs[index]));
}

static void reset_module_for_bytecode(VM* vm, Module* module) {
  if (vm == NULL || module == NULL)
    return;

  VarBufferClear(&module->globals, vm);
  UintBufferClear(&module->global_names, vm);
  VarBufferClear(&module->constants, vm);
  module->body = NULL;
  module->initialized = false;
}

static Handle* ensure_main_module(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return NULL;

  if (bridge->mainModule != NULL)
    return bridge->mainModule;

  bridge->mainModule = NewModule(vm, "@(SAYNAA)");
  return bridge->mainModule;
}

Result saynaa_run_in_main_module(VM* vm, const char* source, const char* path_label) {
  if (vm == NULL || source == NULL)
    return RESULT_RUNTIME_ERROR;

  Handle* module_handle = ensure_main_module(vm);
  if (module_handle == NULL)
    return RESULT_RUNTIME_ERROR;

  Module* module = (Module*) AS_OBJ(module_handle->value);
  module->path = newString(vm, (path_label != NULL) ? path_label : "@(String)");

  Result result = compile(vm, module, source, NULL);
  if (result != RESULT_SUCCESS)
    return result;

  inject_builtin_global(vm, module, "eventView");

  module->initialized = true;

  Fiber* fiber = newFiber(vm, module->body);
  if (fiber == NULL)
    return RESULT_RUNTIME_ERROR;

  vmPushTempRef(vm, &fiber->_super);
  if (!vmPrepareFiber(vm, fiber, 0, NULL)) {
    vmPopTempRef(vm);
    return RESULT_RUNTIME_ERROR;
  }
  vmPopTempRef(vm);

  return vmRunFiber(vm, fiber);
}

Result saynaa_run_file_in_main_module(VM* vm, const char* path) {
  if (vm == NULL || path == NULL)
    return RESULT_RUNTIME_ERROR;

  Handle* module_handle = ensure_main_module(vm);
  if (module_handle == NULL)
    return RESULT_RUNTIME_ERROR;

  char* resolved_ = NULL;
  if (vm->config.resolve_path_fn != NULL) {
    resolved_ = vm->config.resolve_path_fn(vm, NULL, path);
  }

  if (resolved_ == NULL) {
    if (vm->config.stderr_write != NULL) {
      if (vm->config.use_ansi_escape) {
        vm->config.stderr_write(vm, "\x1b[31mError\x1b[0m finding script at \"");
      } else {
        vm->config.stderr_write(vm, "Error finding script at \"");
      }
      vm->config.stderr_write(vm, path);
      vm->config.stderr_write(vm, "\"\n");
    }
    return RESULT_COMPILE_ERROR;
  }

  Module* module = (Module*) AS_OBJ(module_handle->value);
  module->path = newString(vm, resolved_);
  Realloc(vm, resolved_, 0);

  bool is_bytecode = false;
  char* source = LoadScriptAutoDetect(vm, module->path->data, &is_bytecode);
  if (source == NULL)
    return RESULT_COMPILE_ERROR;

  if (is_bytecode) {
    SaynaaBytecodeHeader header;
    SaynaaBytecodeStatus status = saynaa_bytecode_decode_header(
        (const uint8_t*) source, SAYNAA_BYTECODE_HEADER_SIZE, &header);
    if (status == SAYNAA_BC_OK) {
      const uint8_t* payload = (const uint8_t*) source + SAYNAA_BYTECODE_HEADER_SIZE;
      reset_module_for_bytecode(vm, module);
      status = saynaa_bytecode_deserialize_module(vm, module, payload, header.bytecode_size);
    }
    Realloc(vm, source, 0);

    if (status != SAYNAA_BC_OK)
      return RESULT_COMPILE_ERROR;

    inject_builtin_global(vm, module, "eventView");

    module->initialized = true;

    Fiber* fiber = newFiber(vm, module->body);
    if (fiber == NULL)
      return RESULT_RUNTIME_ERROR;

    vmPushTempRef(vm, &fiber->_super);
    if (!vmPrepareFiber(vm, fiber, 0, NULL)) {
      vmPopTempRef(vm);
      return RESULT_RUNTIME_ERROR;
    }
    vmPopTempRef(vm);

    return vmRunFiber(vm, fiber);
  }

  Result result = compile(vm, module, source, NULL);
  Realloc(vm, source, 0);
  if (result != RESULT_SUCCESS)
    return result;

  inject_builtin_global(vm, module, "eventView");

  module->initialized = true;

  Fiber* fiber = newFiber(vm, module->body);
  if (fiber == NULL)
    return RESULT_RUNTIME_ERROR;

  vmPushTempRef(vm, &fiber->_super);
  if (!vmPrepareFiber(vm, fiber, 0, NULL)) {
    vmPopTempRef(vm);
    return RESULT_RUNTIME_ERROR;
  }
  vmPopTempRef(vm);

  return vmRunFiber(vm, fiber);
}
