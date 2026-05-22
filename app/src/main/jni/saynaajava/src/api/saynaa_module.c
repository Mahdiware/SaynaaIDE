#include "saynaa_internal.h"
#include "saynaa_bytecode.h"

extern void initializeModule(VM* vm, Module* module, bool is_main);

static bool set_module_global_from_bridge_ref(
    VM* vm, Module* module, const char* name, jobject global_ref) {
  if (vm == NULL || module == NULL || name == NULL || global_ref == NULL)
    return false;

  reserveSlots(vm, 1);
  if (!wrap_bridge_global(vm, global_ref, 0))
    return false;

  Handle* handle = GetSlotHandle(vm, 0);
  if (handle == NULL)
    return false;

  moduleSetGlobal(vm, module, name, (uint32_t) strlen(name), handle->value);
  releaseHandle(vm, handle);
  return true;
}

static void inject_context_globals(VM* vm, Module* module) {
  if (vm == NULL || module == NULL)
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL || bridge->activity == NULL)
    return;

  // Match AndLua behavior where both `activity` and `this` refer to context.
  set_module_global_from_bridge_ref(vm, module, "activity", bridge->activity);
  set_module_global_from_bridge_ref(vm, module, "this", bridge->activity);
}

static bool should_preserve_global_name(const String* name) {
  if (name == NULL || name->data == NULL)
    return false;

  // Runtime-reserved globals are refreshed per-run and should not be restored
  // from a previous module snapshot.
  if (strcmp(name->data, "__file__") == 0)
    return false;
  if (strcmp(name->data, "_name") == 0)
    return false;
  if (strcmp(name->data, "_module") == 0)
    return false;
  if (strcmp(name->data, "activity") == 0)
    return false;
  if (strcmp(name->data, "this") == 0)
    return false;

  return true;
}

static void snapshot_module_globals(VM* vm, Module* module, List* names, List* values) {
  if (vm == NULL || module == NULL || names == NULL || values == NULL)
    return;

  uint32_t limit = module->globals.count;
  if (module->global_names.count < limit)
    limit = module->global_names.count;

  for (uint32_t i = 0; i < limit; i++) {
    uint32_t name_index = module->global_names.data[i];
    if (name_index >= module->constants.count)
      continue;

    Var name_var = module->constants.data[name_index];
    if (!IS_OBJ_TYPE(name_var, OBJ_STRING))
      continue;

    String* name = (String*) AS_OBJ(name_var);
    if (!should_preserve_global_name(name))
      continue;

    listAppend(vm, names, name_var);
    listAppend(vm, values, module->globals.data[i]);
  }
}

static void restore_module_globals(VM* vm, Module* module, List* names, List* values) {
  if (vm == NULL || module == NULL || names == NULL || values == NULL)
    return;

  uint32_t count = names->elements.count;
  if (values->elements.count < count)
    count = values->elements.count;

  for (uint32_t i = 0; i < count; i++) {
    Var name_var = names->elements.data[i];
    if (!IS_OBJ_TYPE(name_var, OBJ_STRING))
      continue;

    String* name = (String*) AS_OBJ(name_var);
    if (!should_preserve_global_name(name))
      continue;

    // Keep bytecode-defined globals authoritative; only restore missing ones.
    if (moduleGetGlobalIndexByName(vm, module, name) != -1)
      continue;

    moduleSetGlobal(vm, module, name->data, name->length, values->elements.data[i]);
  }
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

  // Startup scripts rely on _module (and related module globals).
  initializeModule(vm, module, true);

  Result result = compile(vm, module, source, NULL);
  if (result != RESULT_SUCCESS)
    return result;

  inject_context_globals(vm, module);

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
  Result resultOut = RESULT_SUCCESS;
  char* source = LoadScriptAutoDetect(vm, module->path->data, &is_bytecode, &resultOut);
  if (source == NULL) {
    return (resultOut != RESULT_SUCCESS) ? resultOut : RESULT_COMPILE_ERROR;
  }

  if (is_bytecode) {
    List* preserved_names = NULL;
    List* preserved_values = NULL;
    bool has_preserved_globals = false;

    if (module->globals.count > 0) {
      preserved_names = newList(vm, module->globals.count);
      preserved_values = newList(vm, module->globals.count);
      if (preserved_names != NULL && preserved_values != NULL) {
        vmPushTempRef(vm, &preserved_names->_super);
        vmPushTempRef(vm, &preserved_values->_super);
        has_preserved_globals = true;
        snapshot_module_globals(vm, module, preserved_names, preserved_values);
      }
    }

    SaynaaBytecodeHeader header;
    Result status = saynaa_bytecode_decode_header(
        (const uint8_t*) source, SAYNAA_BYTECODE_HEADER_SIZE, &header);
    if (status == RESULT_SUCCESS) {
      const uint8_t* payload = (const uint8_t*) source + SAYNAA_BYTECODE_HEADER_SIZE;
      reset_module_for_bytecode(vm, module);
      status = saynaa_bytecode_deserialize_module(vm, module, payload, header.bytecode_size);
    }
    Realloc(vm, source, 0);

    if (status != RESULT_SUCCESS) {
      if (has_preserved_globals) {
        vmPopTempRef(vm); // preserved_values
        vmPopTempRef(vm); // preserved_names
      }
      return RESULT_COMPILE_ERROR;
    }

    // Bytecode deserialization restores module body/globals, then we must
    // re-initialize runtime module globals like _module.
    initializeModule(vm, module, true);

    inject_context_globals(vm, module);
    if (has_preserved_globals) {
      restore_module_globals(vm, module, preserved_names, preserved_values);
      vmPopTempRef(vm); // preserved_values
      vmPopTempRef(vm); // preserved_names
    }

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

  // Source compile path also requires module globals before script execution.
  initializeModule(vm, module, true);

  Result result = compile(vm, module, source, NULL);
  Realloc(vm, source, 0);
  if (result != RESULT_SUCCESS)
    return result;

  inject_context_globals(vm, module);

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
