#include "saynaa_bytecode.h"
#include "saynaa_internal.h"

extern void initializeModule(VM* vm, Module* module, bool is_main);

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

  // Startup scripts rely on _module (and related module globals).
  initializeModule(vm, module, true);

  Result result = compile(vm, module, source, NULL);
  if (result != RESULT_SUCCESS)
    return result;

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

  return RunFileWithModule(vm, module, path);
}
