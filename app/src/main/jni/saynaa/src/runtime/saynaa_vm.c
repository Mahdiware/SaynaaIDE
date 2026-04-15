/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_vm.h"

#include "../shared/saynaa_bytecode.h"
#include "../utils/saynaa_debug.h"
#include "../utils/saynaa_utils.h"

#include <math.h>

#define INTERN_POOL_INITIAL_CAPACITY 1024u
#define INTERN_POOL_LOAD_PERCENT 75u

static uint32_t vmStringPoolTargetCapacity(uint32_t live_count) {
  uint32_t capacity = INTERN_POOL_INITIAL_CAPACITY;
  while (capacity < UINT32_MAX / 2
         && live_count * 100 > capacity * INTERN_POOL_LOAD_PERCENT) {
    capacity *= 2;
  }
  return capacity;
}

static void vmResizeStringPool(VM* vm, uint32_t new_capacity) {
  ASSERT(vm != NULL, OOPS);
  ASSERT(new_capacity >= INTERN_POOL_INITIAL_CAPACITY, OOPS);

  String** new_entries = (String**) vm->config.realloc_fn(
      NULL, sizeof(String*) * new_capacity, vm->config.user_data);
  ASSERT(new_entries != NULL, "Out of memory.");
  memset(new_entries, 0, sizeof(String*) * new_capacity);

  uint32_t new_count = 0;
  if (vm->interned_strings != NULL) {
    uint32_t mask = new_capacity - 1;
    for (uint32_t i = 0; i < vm->interned_strings_capacity; i++) {
      String* str = vm->interned_strings[i];
      if (str == NULL)
        continue;

      uint32_t index = str->hash & mask;
      while (new_entries[index] != NULL) {
        index = (index + 1) & mask;
      }

      new_entries[index] = str;
      new_count++;
    }

    vm->config.realloc_fn(vm->interned_strings, 0, vm->config.user_data);
  }

  vm->interned_strings = new_entries;
  vm->interned_strings_capacity = new_capacity;
  vm->interned_strings_count = new_count;
}

// Rebuild the interned string pool keeping only live strings.
// The pool does not keep strings alive by itself.
static void vmSweepStringPool(VM* vm) {
  ASSERT(vm != NULL, OOPS);

  if (vm->interned_strings == NULL || vm->interned_strings_capacity == 0) {
    return;
  }

  uint32_t live_count = 0;
  bool has_dead = false;
  for (uint32_t i = 0; i < vm->interned_strings_capacity; i++) {
    String* str = vm->interned_strings[i];
    if (str == NULL)
      continue;

    if (str->_super.is_marked)
      live_count++;
    else
      has_dead = true;
  }

  // Nothing to reclaim on this cycle.
  if (!has_dead)
    return;

  uint32_t new_capacity = vmStringPoolTargetCapacity(live_count);
  String** new_entries = (String**) vm->config.realloc_fn(
      NULL, sizeof(String*) * new_capacity, vm->config.user_data);
  ASSERT(new_entries != NULL, "Out of memory.");
  memset(new_entries, 0, sizeof(String*) * new_capacity);

  uint32_t mask = new_capacity - 1;
  for (uint32_t i = 0; i < vm->interned_strings_capacity; i++) {
    String* str = vm->interned_strings[i];
    if (str == NULL || !str->_super.is_marked)
      continue;

    uint32_t index = str->hash & mask;
    while (new_entries[index] != NULL) {
      index = (index + 1) & mask;
    }
    new_entries[index] = str;
  }

  vm->config.realloc_fn(vm->interned_strings, 0, vm->config.user_data);
  vm->interned_strings = new_entries;
  vm->interned_strings_capacity = new_capacity;
  vm->interned_strings_count = live_count;
}

String* vmFindInternedString(VM* vm, const char* text, uint32_t length,
                             uint32_t hash) {
  ASSERT(vm != NULL, OOPS);
  ASSERT(length == 0 || text != NULL, OOPS);

  if (vm->interned_strings == NULL || vm->interned_strings_capacity == 0)
    return NULL;
  if (vm->interned_strings_count == 0)
    return NULL;

  uint32_t mask = vm->interned_strings_capacity - 1;
  uint32_t index = hash & mask;

  for (uint32_t scanned = 0; scanned < vm->interned_strings_count; scanned++) {
    String* candidate = vm->interned_strings[index];
    if (candidate == NULL)
      return NULL;

    if (candidate->hash == hash && candidate->length == length
        && (length == 0 || memcmp(candidate->data, text, length) == 0)) {
      return candidate;
    }

    index = (index + 1) & mask;
  }

  return NULL;
}

void vmInternString(VM* vm, String* string) {
  ASSERT(vm != NULL && string != NULL, OOPS);

  if (vm->interned_strings == NULL || vm->interned_strings_capacity == 0) {
    vmResizeStringPool(vm, INTERN_POOL_INITIAL_CAPACITY);
  }

  if ((vm->interned_strings_count + 1) * 100
      > vm->interned_strings_capacity * INTERN_POOL_LOAD_PERCENT) {
    vmResizeStringPool(vm, vm->interned_strings_capacity * 2);
  }

  if (vmFindInternedString(vm, string->data, string->length, string->hash) != NULL)
    return;

  uint32_t mask = vm->interned_strings_capacity - 1;
  uint32_t index = string->hash & mask;
  while (vm->interned_strings[index] != NULL) {
    index = (index + 1) & mask;
  }

  vm->interned_strings[index] = string;
  vm->interned_strings_count++;
}

typedef struct {
  VM* vm;
  String* base_ptr;
  String* current_ptr;
  VarBuffer* modules;
  Module* target_module;
} WildcardImportRuntimeData;

#define VM_METHOD_INLINE_CACHE_MASK (VM_METHOD_INLINE_CACHE_SIZE - 1u)
#define VM_ATTRIB_INLINE_CACHE_MASK (VM_ATTRIB_INLINE_CACHE_SIZE - 1u)

static inline VMMethodInlineCacheEntry* vmMethodInlineCacheAt(VM* vm,
                                                               const uint8_t* site) {
  uintptr_t key = (uintptr_t) site;
  return &vm->method_inline_cache[((key >> 2) ^ (key >> 9))
                                  & VM_METHOD_INLINE_CACHE_MASK];
}

static inline VMAttribInlineCacheEntry* vmAttribInlineCacheAt(VM* vm,
                                                               const uint8_t* site) {
  uintptr_t key = (uintptr_t) site;
  return &vm->attrib_inline_cache[((key >> 2) ^ (key >> 10))
                                  & VM_ATTRIB_INLINE_CACHE_MASK];
}

/*****************************************************************************/
/* IMPORT HELPERS                                                            */
/*****************************************************************************/

// Build a dotted module name from wildcard base path and filename.
// Example: base="foo.bar.*", name="file1.sa" -> "foo.bar.file1.sa"
static void buildWildcardModuleName(const String* base, const char* name,
                                    char* out, size_t out_size) {
  ASSERT(base != NULL && name != NULL && out != NULL, OOPS);
  ASSERT(out_size > 0, OOPS);

  int base_len = (int) base->length;
  if (base_len >= 2 && base->data[base_len - 2] == '.' && base->data[base_len - 1] == '*') {
    base_len -= 2;
  }

  if (base_len <= 0) {
    snprintf(out, out_size, "%s", name);
    return;
  }

  size_t copy_len = (size_t) base_len;
  if (copy_len >= out_size)
    copy_len = out_size - 1;

  memcpy(out, base->data, copy_len);
  out[copy_len] = '\0';
  snprintf(out + copy_len, out_size - copy_len, ".%s", name);
}

// Convert dotted import path to filesystem path for directory walking.
// Example: "foo.bar.*" -> "foo/bar"
static void wildcardImportToFsPath(const String* import_path, char* out, size_t out_size) {
  ASSERT(import_path != NULL && out != NULL, OOPS);
  ASSERT(out_size > 0, OOPS);

  int fs_len = (int) import_path->length;
  if (fs_len >= 2 && import_path->data[fs_len - 2] == '.'
      && import_path->data[fs_len - 1] == '*') {
    fs_len -= 2;
  }

  if ((size_t) fs_len >= out_size)
    fs_len = (int) out_size - 1;

  for (int i = 0; i < fs_len; i++) {
    char c = import_path->data[i];
    out[i] = (c == '.') ? '/' : c;
  }
  out[fs_len] = '\0';
}

static void wildcardRuntimeCallback(const char* name, void* user_data) {
  WildcardImportRuntimeData* data = (WildcardImportRuntimeData*) user_data;
  VM* vm = data->vm;

  char full_name[MAX_PATH_LEN];
  buildWildcardModuleName(data->base_ptr, name, full_name, sizeof(full_name));

  String* mod_name = newString(vm, full_name);
  vmPushTempRef(vm, &mod_name->_super);

  // Import relative to current module path
  Var imported = vmImportModule(vm, data->current_ptr, mod_name);
  if (IS_OBJ_TYPE(imported, OBJ_MODULE)) {
    VarBufferWrite(data->modules, vm, imported);

    // Bind the module to a variable in the current module with the name of the
    // file without extension.
    // e.g. from test import * (where test contains debug.sa)
    // debug = import("test/debug")
    const char* ext = strrchr(name, '.');
    size_t name_len = (ext != NULL) ? (size_t) (ext - name) : strlen(name);

    if (name_len > 0) {
      moduleSetGlobal(vm, data->target_module, name, (int) name_len, imported);
    }
  }
  vmPopTempRef(vm);
}

static String* importNameToPath(VM* vm, String* name, bool* needs_pop) {
  if (needs_pop)
    *needs_pop = false;

  if (name == NULL)
    return NULL;

  uint32_t len = name->length;
  bool has_dot = false;
  bool is_wildcard = false;

  if (len >= 2 && name->data[len - 2] == '.' && name->data[len - 1] == '*') {
    len -= 2;
    is_wildcard = true;
  }

  for (uint32_t i = 0; i < len; i++) {
    if (name->data[i] == '.') {
      has_dot = true;
      break;
    }
  }

  if (!has_dot && !is_wildcard)
    return name;

  String* path = newStringLength(vm, name->data, len);
  for (uint32_t i = 0; i < path->length; i++) {
    if (path->data[i] == '.')
      path->data[i] = '/';
  }
  path->hash = utilHashString(path->data);
  vmPushTempRef(vm, &path->_super);
  if (needs_pop)
    *needs_pop = true;
  return path;
}

Handle* vmNewHandle(VM* vm, Var value) {
  Handle* handle = (Handle*) ALLOCATE(vm, Handle);
  handle->value = value;
  handle->prev = NULL;
  handle->next = vm->handles;
  if (handle->next != NULL)
    handle->next->prev = handle;
  vm->handles = handle;
  return handle;
}

void* vmRealloc(VM* vm, void* memory, size_t old_size, size_t new_size) {
  // Track heap delta to trigger GC on growth. During sweep we keep accounting
  // frozen and recalculate bytes_allocated from marked objects.
  if (!vm->collecting_garbage) {
    if (new_size >= old_size) {
      vm->bytes_allocated += new_size - old_size;
    } else {
      size_t released = old_size - new_size;
      vm->bytes_allocated = (released >= vm->bytes_allocated)
                                ? 0
                                : vm->bytes_allocated - released;
    }
  }

  // If we're garbage collecting no new allocation is allowed.
  ASSERT(!vm->collecting_garbage || new_size == 0,
         "No new allocation is allowed while garbage collection is running.");

  // Trigger GC only when an operation grows memory.
  if (new_size > old_size && vm->bytes_allocated > vm->next_gc) {
    ASSERT(vm->collecting_garbage == false, OOPS);
    vm->collecting_garbage = true;
    vmCollectGarbage(vm);
    vm->collecting_garbage = false;
  }

  return vm->config.realloc_fn(memory, new_size, vm->config.user_data);
}

void vmPushTempRef(VM* vm, Object* obj) {
  ASSERT(obj != NULL, "Cannot reference to NULL.");
  ASSERT(vm->temp_reference_count < MAX_TEMP_REFERENCE,
         "Too many temp references");
  vm->temp_reference[vm->temp_reference_count++] = obj;
}

void vmInvalidateInlineCaches(VM* vm) {
  ASSERT(vm != NULL, OOPS);

  vm->inline_cache_epoch++;
  if (vm->inline_cache_epoch == 0) {
    // Rare wrap-around: fully clear cache metadata.
    memset(vm->method_inline_cache, 0, sizeof(vm->method_inline_cache));
    memset(vm->attrib_inline_cache, 0, sizeof(vm->attrib_inline_cache));
    vm->inline_cache_epoch = 1;
  }
}

void vmPopTempRef(VM* vm) {
  ASSERT(vm->temp_reference_count > 0, "Temporary reference is empty to pop.");
  vm->temp_reference_count--;
}

void vmRegisterModule(VM* vm, Module* module, String* key) {
  ASSERT((((module->name != NULL) && IS_STR_EQ(module->name, key))
          || IS_STR_EQ(module->path, key)),
         OOPS);

  // FIXME:
  // Not sure what to do, if a module the the same key already exists. Should
  // I override or assert.
  mapSet(vm, vm->modules, VAR_OBJ(key), VAR_OBJ(module));
}

Module* vmGetModule(VM* vm, String* key) {
  Var module = mapGet(vm->modules, VAR_OBJ(key));
  if (IS_UNDEF(module))
    return NULL;
  ASSERT(AS_OBJ(module)->type == OBJ_MODULE, OOPS);
  return (Module*) AS_OBJ(module);
}

void vmCollectGarbage(VM* vm) {
  // Drop transient caches before mark/sweep to avoid stale raw pointers.
  vm->method_cache_class = NULL;
  vm->method_cache_name = NULL;
  vm->method_cache_closure = NULL;
  vmInvalidateInlineCaches(vm);

  // Mark builtin functions.
  for (int i = 0; i < vm->builtins_count; i++) {
    markObject(vm, &vm->builtins_funcs[i]->_super);
  }

  // Mark primitive types' classes.
  for (int i = 0; i < vINSTANCE; i++) {
    // It's possible that a garbage collection could be triggered while
    // we're building the primitives and the class could be NULL.
    if (vm->builtin_classes[i] == NULL)
      continue;

    markObject(vm, &vm->builtin_classes[i]->_super);
  }

  // Mark the modules and search path.
  markObject(vm, &vm->modules->_super);
  markObject(vm, &vm->search_paths->_super);
  markObject(vm, &vm->import_resolve_cache->_super);

  // Mark temp references.
  for (int i = 0; i < vm->temp_reference_count; i++) {
    markObject(vm, vm->temp_reference[i]);
  }

  // Mark the handles.
  for (Handle* h = vm->handles; h != NULL; h = h->next) {
    markValue(vm, h->value);
  }

  // Garbage collection triggered at the middle of a compilation.
  if (vm->compiler != NULL) {
    compilerMarkObjects(vm, vm->compiler);
  }

  if (vm->fiber != NULL) {
    markObject(vm, &vm->fiber->_super);
  }

  // Reset VM's bytes_allocated value and count it again so that we don't
  // required to know the size of each object that'll be freeing.
  vm->bytes_allocated = 0;

  // Pop the marked objects from the working set and push all of it's
  // referenced objects. This will repeat till no more objects left in the
  // working set.
  popMarkedObjects(vm);

  // Interned string pool is weak: keep only strings marked through real roots.
  vmSweepStringPool(vm);

  // Now [vm->bytes_allocated] is equal to the number of bytes allocated for
  // the root objects which are marked above. Since we're garbage collecting
  // freeObject() shouldn't modify vm->bytes_allocated. We ensure this by
  // copying the value to [bytes_allocated] and check after freeing.
#ifdef DEBUG
  size_t bytes_allocated = vm->bytes_allocated;
#endif

  // Now sweep all the un-marked objects in then link list and remove them
  // from the chain.

  // [ptr] is an Object* reference that should be equal to the next
  // non-garbage Object*.
  Object** ptr = &vm->first;
  while (*ptr != NULL) {
    // If the object the pointer points to wasn't marked it's unreachable.
    // Clean it. And update the pointer points to the next object.
    if (!(*ptr)->is_marked) {
      Object* garbage = *ptr;
      *ptr = garbage->next;
      freeObject(vm, garbage);

    } else {
      // Unmark the object for the next garbage collection.
      (*ptr)->is_marked = false;
      ptr = &(*ptr)->next;
    }
  }

#ifdef DEBUG
  // Safety check: during GC sweep, freeObject() must not mutate
  // vm->bytes_allocated. This assert helps catch accounting bugs that can
  // break GC trigger thresholds (too frequent or too late collections).
  ASSERT(bytes_allocated == vm->bytes_allocated, OOPS);
#endif

  // Next GC heap size will be change depends on the byte we've left with now,
  // and the [heap_fill_percent].
  vm->next_gc = vm->bytes_allocated + ((vm->bytes_allocated * vm->heap_fill_percent) / 100);
  if (vm->next_gc < vm->min_heap_size)
    vm->next_gc = vm->min_heap_size;
}

#define _ERR_FAIL(msg) \
  do { \
    if (vm->fiber != NULL) \
      VM_SET_ERROR(vm, msg); \
    return false; \
  } while (false)

bool vmPrepareFiber(VM* vm, Fiber* fiber, int argc, Var* argv) {
  ASSERT(fiber->closure->fn->arity >= -1,
         OOPS " (Forget to initialize arity.)");

  // Current call semantics: extra arguments are dropped; missing ones become null.
  int nulls = 0;
  if (fiber->closure->fn->arity != -1) {
    if (argc > fiber->closure->fn->arity) {
      argc = fiber->closure->fn->arity;
    } else {
      nulls = fiber->closure->fn->arity - argc;
    }
  }

  if (fiber->state != FIBER_NEW) {
    switch (fiber->state) {
      case FIBER_NEW:
        UNREACHABLE();
      case FIBER_RUNNING:
        _ERR_FAIL(newString(vm, "The fiber has already been running."));
      case FIBER_YIELDED:
        _ERR_FAIL(newString(vm, "Cannot run a fiber which is yielded, use "
                                "fiber_resume() instead."));
      case FIBER_DONE:
        _ERR_FAIL(newString(vm, "The fiber has done running."));
    }
    UNREACHABLE();
  }

  ASSERT(fiber->stack != NULL && fiber->sp == fiber->stack + 1, OOPS);
  ASSERT(fiber->ret == fiber->stack, OOPS);

  vmEnsureStackSize(vm, fiber, (int) (fiber->sp - fiber->stack) + argc + nulls);
  ASSERT((fiber->stack + fiber->stack_size) - fiber->sp >= argc + nulls, OOPS);

  // Pass the function arguments.

  // ARG1 is fiber, function arguments are ARG(2), ARG(3), ... ARG(argc).
  // And ret[0] is the return value, parameters starts at ret[1], ...
  for (int i = 0; i < argc; i++) {
    fiber->ret[1 + i] = *(argv + i); // +1: ret[0] is return value.
  }

  for (int i = 0; i < nulls; i++) {
    fiber->ret[1 + i + argc] = VAR_NULL;
  }
  fiber->sp += argc + nulls; // Parameters.

  // Native functions doesn't own a stack frame so, we're done here.
  if (fiber->closure->fn->is_native)
    return true;

  // Assert we have the first frame (to push the arguments). And assert we
  // have enough stack space for parameters.
  ASSERT(fiber->frame_count == 1, OOPS);
  ASSERT(fiber->frames[0].rbp == fiber->ret, OOPS);

  // Capture thiz.
  fiber->frames[0].thiz = fiber->thiz;
  fiber->thiz = VAR_UNDEFINED;

  // On success return true.
  return true;
}

bool vmSwitchFiber(VM* vm, Fiber* fiber, Var* value) {
  if (fiber->state != FIBER_YIELDED) {
    switch (fiber->state) {
      case FIBER_NEW:
        _ERR_FAIL(newString(vm, "The fiber hasn't started. call fiber_run() "
                                "to start."));
      case FIBER_RUNNING:
        _ERR_FAIL(newString(vm, "The fiber has already been running."));
      case FIBER_YIELDED:
        UNREACHABLE();
      case FIBER_DONE:
        _ERR_FAIL(newString(vm, "The fiber has done running."));
    }
    UNREACHABLE();
  }

  // Pass the resume argument if it has any.

  // Assert if we have a call frame and the stack size enough for the return
  // value and the resumed value.
  ASSERT(fiber->frame_count != 0, OOPS);
  ASSERT((fiber->stack + fiber->stack_size) - fiber->sp >= 2, OOPS);

  // fb->ret will points to the return value of the 'yield()' call.
  if (value == NULL)
    *fiber->ret = VAR_NULL;
  else
    *fiber->ret = *value;

  // Switch fiber.
  fiber->caller = vm->fiber;
  vm->fiber = fiber;

  // On success return true.
  return true;
}

#undef _ERR_FAIL

void vmYieldFiber(VM* vm, Var* value) {
  Fiber* caller = vm->fiber->caller;

  // Return the yield value to the caller fiber.
  if (caller != NULL) {
    if (value == NULL)
      *caller->ret = VAR_NULL;
    else
      *caller->ret = *value;
  }

  // Can be resumed by another caller fiber.
  vm->fiber->caller = NULL;
  vm->fiber->state = FIBER_YIELDED;
  vm->fiber = caller;
}

Result vmCallMethod(VM* vm, Var thiz, Closure* fn, int argc, Var* argv, Var* ret) {
  ASSERT(argc >= 0, "argc cannot be negative.");
  ASSERT(argc == 0 || argv != NULL, "argv was NULL when argc > 0.");

  Fiber* fiber = newFiber(vm, fn);
  fiber->thiz = thiz;
  fiber->native = vm->fiber;
  vmPushTempRef(vm, &fiber->_super); // fiber.
  bool success = vmPrepareFiber(vm, fiber, argc, argv);

  if (!success) {
    vmPopTempRef(vm); // fiber.
    return RESULT_RUNTIME_ERROR;
  }

  Result result;

  Fiber* last = vm->fiber;
  if (last != NULL)
    vmPushTempRef(vm, &last->_super); // last.
  {
    if (fiber->closure->fn->is_native) {
      ASSERT(fiber->closure->fn->native != NULL, "Native function was NULL");
      vm->fiber = fiber;
      fiber->closure->fn->native(vm);
      if (VM_HAS_ERROR(vm)) {
        if (last != NULL)
          last->error = vm->fiber->error;
        result = RESULT_RUNTIME_ERROR;
      } else {
        result = RESULT_SUCCESS;
      }

    } else {
      result = vmRunFiber(vm, fiber);
    }
  }

  if (last != NULL)
    vmPopTempRef(vm); // last.
  vmPopTempRef(vm);   // fiber.

  vm->fiber = last;

  if (ret != NULL)
    *ret = *fiber->ret;

  return result;
}

Result vmCallFunction(VM* vm, Closure* fn, int argc, Var* argv, Var* ret) {
  // Calling functions and methods are the same, except for the methods have
  // this defined, and for functions it'll be VAR_UNDEFINED.
  return vmCallMethod(vm, VAR_UNDEFINED, fn, argc, argv, ret);
}

#ifndef NO_DL

struct NativeLibCacheEntry {
  NativeLibCacheEntry* prev;
  NativeLibCacheEntry* next;
  char* path;
  void* os_handle;
  uint32_t refs;
};

static void* _dlCacheAlloc(VM* vm, size_t size) {
  return vm->config.realloc_fn(NULL, size, vm->config.user_data);
}

static void _dlCacheFree(VM* vm, void* ptr) {
  if (ptr != NULL) {
    vm->config.realloc_fn(ptr, 0, vm->config.user_data);
  }
}

static NativeLibCacheEntry* _dlCacheFind(VM* vm, const char* resolved_path) {
  for (NativeLibCacheEntry* entry = vm->native_dl_cache; entry != NULL; entry = entry->next) {
    if (strcmp(entry->path, resolved_path) == 0) {
      return entry;
    }
  }
  return NULL;
}

static NativeLibCacheEntry* _dlCacheAcquire(VM* vm, String* resolved) {
  NativeLibCacheEntry* entry = _dlCacheFind(vm, resolved->data);
  if (entry != NULL) {
    entry->refs++;
    return entry;
  }

  ASSERT(vm->config.load_dl_fn != NULL, OOPS);
  void* os_handle = vm->config.load_dl_fn(vm, resolved->data);
  if (os_handle == NULL)
    return NULL;

  entry = (NativeLibCacheEntry*) _dlCacheAlloc(vm, sizeof(NativeLibCacheEntry));
  if (entry == NULL) {
    if (vm->config.unload_dl_fn)
      vm->config.unload_dl_fn(vm, os_handle);
    return NULL;
  }

  char* path = (char*) _dlCacheAlloc(vm, (size_t) resolved->length + 1);
  if (path == NULL) {
    _dlCacheFree(vm, entry);
    if (vm->config.unload_dl_fn)
      vm->config.unload_dl_fn(vm, os_handle);
    return NULL;
  }

  memcpy(path, resolved->data, (size_t) resolved->length);
  path[resolved->length] = '\0';

  entry->prev = NULL;
  entry->next = vm->native_dl_cache;
  if (entry->next != NULL)
    entry->next->prev = entry;
  entry->path = path;
  entry->os_handle = os_handle;
  entry->refs = 1;
  vm->native_dl_cache = entry;

  return entry;
}

static void _dlCacheRelease(VM* vm, NativeLibCacheEntry* entry) {
  ASSERT(entry != NULL, OOPS);
  ASSERT(entry->refs > 0, OOPS);

  entry->refs--;
  if (entry->refs > 0)
    return;

  if (entry->prev != NULL) {
    entry->prev->next = entry->next;
  } else {
    vm->native_dl_cache = entry->next;
  }

  if (entry->next != NULL)
    entry->next->prev = entry->prev;

  if (vm->config.unload_dl_fn != NULL)
    vm->config.unload_dl_fn(vm, entry->os_handle);

  _dlCacheFree(vm, entry->path);
  _dlCacheFree(vm, entry);
}

// Returns true if the path ends with ".dll" or ".so".
static bool _isPathDL(String* path) {
  const char* dlext[] = {
      ".so",
      ".dll",
      NULL,
  };

  for (const char** ext = dlext; *ext != NULL; ext++) {
    size_t ext_len = strlen(*ext);
    if ((size_t) path->length < ext_len)
      continue;

    const char* start = path->data + (path->length - ext_len);
    if (!strncmp(start, *ext, ext_len))
      return true;
  }

  return false;
}

static Module* _importDL(VM* vm, String* resolved, String* name) {
  if (vm->config.import_dl_fn == NULL) {
    VM_SET_ERROR(vm, newString(vm, "Dynamic library importer not provided."));
    return NULL;
  }

  NativeLibCacheEntry* lib_entry = _dlCacheAcquire(vm, resolved);
  if (lib_entry == NULL) {
    VM_SET_ERROR(vm, stringFormat(vm, "Error loading module at \"@\"", resolved));
    return NULL;
  }

  // Since the DL library can use stack via slots api, we need to update
  // ret and then restore it back. We're using offset instead of a pointer
  // because the stack might be reallocated if it grows.
  uintptr_t ret_offset = vm->fiber->ret - vm->fiber->stack;
  vm->fiber->ret = vm->fiber->sp;
  Handle* lhandle = vm->config.import_dl_fn(vm, lib_entry->os_handle);
  vm->fiber->ret = vm->fiber->stack + ret_offset;

  if (lhandle == NULL) {
    vmUnloadDlHandle(vm, lib_entry);
    VM_SET_ERROR(vm, stringFormat(vm, "Error loading module at \"@\"", resolved));
    return NULL;
  }

  if (!IS_OBJ_TYPE(lhandle->value, OBJ_MODULE)) {
    releaseHandle(vm, lhandle);
    vmUnloadDlHandle(vm, lib_entry);
    VM_SET_ERROR(vm, stringFormat(vm,
                                  "Returned handle wasn't a "
                                  "module at \"@\"",
                                  resolved));
    return NULL;
  }

  Module* module = (Module*) AS_OBJ(lhandle->value);
  module->name = name;
  module->path = resolved;
  module->handle = lib_entry;
  vmRegisterModule(vm, module, resolved);

  releaseHandle(vm, lhandle);
  return module;
}

void vmUnloadDlHandle(VM* vm, void* handle) {
  if (handle == NULL)
    return;
  _dlCacheRelease(vm, (NativeLibCacheEntry*) handle);
}
#endif // NO_DL

/*****************************************************************************/
/* VM INTERNALS                                                              */
/*****************************************************************************/

static Module* _importScript(VM* vm, String* resolved, String* name) {
  char* source = vm->config.load_script_fn(vm, resolved->data);
  if (source == NULL) {
    VM_SET_ERROR(vm, stringFormat(vm, "Error loading module at \"@\"", resolved));
    return NULL;
  }

  // Make a new module, compile and cache it.
  Module* module = newModule(vm);
  module->path = resolved;
  module->name = name;

  vmPushTempRef(vm, &module->_super); // module.
  {
    bool is_bytecode = false;
    bool header_seen = false;
    SaynaaBytecodeHeader header;
    Result status = saynaa_bytecode_decode_header(
        (const uint8_t*) source, SAYNAA_BYTECODE_HEADER_SIZE, &header);
    if (status == RESULT_SUCCESS) {
      status = saynaa_bytecode_validate_header(&header, 0);
      if (status == RESULT_BYTECODE_INVALID_MAGIC) {
        header_seen = false;
      } else if (status == RESULT_SUCCESS) {
        header_seen = true;
        const uint8_t* payload = (const uint8_t*) source
                                 + SAYNAA_BYTECODE_HEADER_SIZE;
        status = saynaa_bytecode_validate_checksum(&header, payload,
                                                   header.bytecode_size);
        if (status == RESULT_SUCCESS) {
          status = saynaa_bytecode_validate_payload(payload, header.bytecode_size);
          if (status == RESULT_SUCCESS) {
            status = saynaa_bytecode_deserialize_module(vm, module, payload,
                                                        header.bytecode_size);
          }
          if (status == RESULT_SUCCESS) {
            is_bytecode = true;
          }
        }
      } else {
        header_seen = true;
      }
    }

    Result result = RESULT_SUCCESS;
    if (header_seen && !is_bytecode) {
      result = RESULT_COMPILE_ERROR;
    } else if (!is_bytecode) {
      initializeModule(vm, module, false);
      result = compile(vm, module, source, NULL);
    } else {
      initializeModule(vm, module, false);
    }

    Realloc(vm, source, 0);

    if (result == RESULT_SUCCESS) {
      vmRegisterModule(vm, module, resolved);
    } else {
      VM_SET_ERROR(vm, stringFormat(vm, "Error compiling module at \"@\"", resolved));
      module = NULL; //< set to null to indicate error.
    }
  }
  vmPopTempRef(vm); // module.

  return module;
}

static Module* _importResolved(VM* vm, String* resolved, String* name) {
  // If the script already imported and cached, return it.
  Var entry = mapGet(vm->modules, VAR_OBJ(resolved));
  if (!IS_UNDEF(entry)) {
    ASSERT(AS_OBJ(entry)->type == OBJ_MODULE, OOPS);
    return (Module*) AS_OBJ(entry); // We're done.
  }

  // The script not exists in the VM, make sure we have the script loading
  // api function.

#ifndef NO_DL
  bool isdl = _isPathDL(resolved);
  if (isdl && vm->config.load_dl_fn == NULL || vm->config.load_script_fn == NULL) {
#else
  if (vm->config.load_script_fn == NULL) {
#endif

    VM_SET_ERROR(vm,
                 newString(vm, "Cannot import. The hosting application "
                               "haven't registered the module loading API"));
    return NULL;
  }

  Module* module = NULL;

  vmPushTempRef(vm, &resolved->_super); // resolved.
  {
    // FIXME:
    // stringReplace() function expect 2 strings old, and new to replace but
    // we cannot afford to allocate new strings for single char, so we're
    // replaceing and rehashing the string here. I should add some update
    // string function that update a string after it's data was modified.
    //
    // The path of the module contain '/' which was replacement of '.' in
    // the import syntax, this is done so that path resolving can be done
    // easily. However it needs to be '.' for the name of the module.
    /*
     * Note: String name is likely const or shared?
     * No, _importResolved takes 'name'.
     * In original code, _name was created inside vmImportModule.
     * I should probably clone it if I modify it?
     * Or pass already modified name.
     * The original code created new string `_name` from `path->data`.
     * Let's create `_name` here using `name->data`.
     */

    String* _name = newStringLength(vm, name->data, name->length);
    for (char* c = _name->data; c < _name->data + _name->length; c++) {
      if (*c == '/')
        *c = '.';
    }
    _name->hash = utilHashString(_name->data);
    vmPushTempRef(vm, &_name->_super); // _name.

#ifndef NO_DL
    if (isdl)
      module = _importDL(vm, resolved, _name);
    else /* ... */
#endif
      module = _importScript(vm, resolved, _name);

    vmPopTempRef(vm); // _name.
  }
  vmPopTempRef(vm); // resolved.

  if (module == NULL) {
    ASSERT(VM_HAS_ERROR(vm), OOPS);
    return NULL;
  }
  return module;
}

static Map* _getImportResolveBucket(VM* vm, Var from_key, bool create) {
  Var bucket_var = mapGet(vm->import_resolve_cache, from_key);
  if (!IS_UNDEF(bucket_var)) {
    ASSERT(IS_OBJ_TYPE(bucket_var, OBJ_MAP), OOPS);
    return (Map*) AS_OBJ(bucket_var);
  }

  if (!create)
    return NULL;

  Map* bucket = newMap(vm);
  vmPushTempRef(vm, &bucket->_super); // bucket.
  mapSet(vm, vm->import_resolve_cache, from_key, VAR_OBJ(bucket));
  vmPopTempRef(vm); // bucket.
  return bucket;
}

static bool _resolvePathCacheGet(VM* vm, Var from_key, String* path,
                                 String** resolved) {
  Map* bucket = _getImportResolveBucket(vm, from_key, false);
  if (bucket == NULL)
    return false;

  Var cached = mapGet(bucket, VAR_OBJ(path));
  if (IS_UNDEF(cached))
    return false;

  if (IS_FALSE(cached)) {
    *resolved = NULL;
    return true;
  }

  ASSERT(IS_OBJ_TYPE(cached, OBJ_STRING), OOPS);
  *resolved = AS_STRING(cached);
  return true;
}

static void _resolvePathCacheSet(VM* vm, Var from_key, String* path,
                                 String* resolved) {
  if (vm->import_resolve_cache_entries >= vm->import_resolve_cache_limit) {
    ClearImportResolveCache(vm);
  }

  Map* bucket = _getImportResolveBucket(vm, from_key, true);
  ASSERT(bucket != NULL, OOPS);

  Var existing = mapGet(bucket, VAR_OBJ(path));
  bool is_new = IS_UNDEF(existing);

  if (resolved != NULL)
    mapSet(vm, bucket, VAR_OBJ(path), VAR_OBJ(resolved));
  else
    mapSet(vm, bucket, VAR_OBJ(path), VAR_FALSE);

  if (is_new)
    vm->import_resolve_cache_entries++;
}

static String* _resolvePathWithCache(VM* vm, Var from_key, const char* from_path,
                                     String* path) {
  String* resolved = NULL;
  if (_resolvePathCacheGet(vm, from_key, path, &resolved)) {
    return resolved;
  }

  char* raw = vm->config.resolve_path_fn(vm, from_path, path->data);
  if (raw == NULL) {
    _resolvePathCacheSet(vm, from_key, path, NULL);
    return NULL;
  }

  resolved = newString(vm, raw);
  Realloc(vm, raw, 0);

  vmPushTempRef(vm, &resolved->_super); // resolved.
  _resolvePathCacheSet(vm, from_key, path, resolved);
  vmPopTempRef(vm); // resolved.

  return resolved;
}

static Var _importPathSearch(VM* vm, String* path) {
  // Try resolver from default root first, then each configured search path.
  const uint32_t candidates = (uint32_t) vm->search_paths->elements.count + 1;
  for (uint32_t idx = 0; idx < candidates; idx++) {
    Var from_key = VAR_NULL;
    const char* from_path = NULL;

    if (idx > 0) {
      Var sp = vm->search_paths->elements.data[idx - 1];
      ASSERT(IS_OBJ_TYPE(sp, OBJ_STRING), OOPS);
      from_key = sp;
      from_path = AS_STRING(sp)->data;
    }

    String* resolved = _resolvePathWithCache(vm, from_key, from_path, path);
    if (resolved == NULL)
      continue;

    Module* module = _importResolved(vm, resolved, path);
    if (module == NULL)
      return VAR_NULL;
    return VAR_OBJ(module);
  }

  return VAR_NULL;
}

void vmStandardSearcher(VM* vm) {
  if (!IS_OBJ_TYPE(vm->fiber->ret[1], OBJ_STRING)) {
    vm->fiber->ret[0] = VAR_NULL;
    return;
  }
  String* name = AS_STRING(vm->fiber->ret[1]);

  // Convert dots to slashes for file path search. A copy is made to avoid
  // mutating the original import name.
  bool needs_pop = false;
  String* path = importNameToPath(vm, name, &needs_pop);
  Var result = _importPathSearch(vm, path);
  if (needs_pop)
    vmPopTempRef(vm);

  vm->fiber->ret[0] = result;
}

// Import and return the Module object with the [path] string. If the path
// starts with with './' or '../' we'll only try relative imports, otherwise
// we'll search native modules first and then at relative path.
Var vmImportModule(VM* vm, String* from, String* path) {
  ASSERT((path != NULL) && (path->length > 0), OOPS);

  bool is_relative = path->data[0] == '.';

  // If not relative check the [path] in the modules cache with the name
  // (before resolving the path).
  if (!is_relative) {
    // If not relative path we first search in modules cache. It'll find the
    // native module or the already imported cache of the script.
    Var entry = mapGet(vm->modules, VAR_OBJ(path));
    if (!IS_UNDEF(entry)) {
      ASSERT(AS_OBJ(entry)->type == OBJ_MODULE, OOPS);
      return entry; // We're done.
    }
  } else {
    // Relative Import Logic
    if (vm->config.resolve_path_fn == NULL) {
      VM_SET_ERROR(vm,
                   newString(vm, "Cannot import. The hosting application "
                                 "haven't registered the module loading API"));
      return VAR_NULL;
    }

    const char* from_path = (from) ? from->data : NULL;
    Var from_key = (from != NULL) ? VAR_OBJ(from) : VAR_NULL;
    String* resolved = _resolvePathWithCache(vm, from_key, from_path, path);
    if (resolved == NULL) {
      VM_SET_ERROR(vm, stringFormat(vm, "Cannot import module '@'", path));
      return VAR_NULL;
    }

    // We use _importResolved which handles cache check for resolved path
    Module* mod = _importResolved(vm, resolved, path);
    if (mod != NULL)
      return VAR_OBJ(mod);
    return VAR_NULL;
  }

  // Searchers Logic
  for (int i = 0; i < vm->searchers->elements.count; i++) {
    Var searcher = vm->searchers->elements.data[i];
    if (!IS_OBJ(searcher))
      continue; // TODO: Warning?

    // Prepare call
    Closure* closure = NULL;
    Var thiz = VAR_UNDEFINED;

    if (IS_OBJ_TYPE(searcher, OBJ_CLOSURE)) {
      closure = (Closure*) AS_OBJ(searcher);
    } // TODO: Handle MethodBind/Class? For now assume closure (native or script).

    if (closure) {
      // Fast path for the default searcher to avoid fiber setup overhead.
      if (closure->fn->is_native && closure->fn->native == vmStandardSearcher) {
        bool needs_pop = false;
        String* search_path = importNameToPath(vm, path, &needs_pop);
        Var result = _importPathSearch(vm, search_path);
        if (needs_pop)
          vmPopTempRef(vm);

        if (!IS_NULL(result)) {
          return result;
        }

        continue;
      }

      Var args[1] = {VAR_OBJ(path)};
      Var result;
      // Call searcher(path)
      if (vmCallMethod(vm, thiz, closure, 1, args, &result) != RESULT_SUCCESS) {
        return VAR_NULL; // Error in searcher
      }

      if (!IS_NULL(result)) {
        return result;
      }
    }
  }

  VM_SET_ERROR(vm, stringFormat(vm, "Cannot import module '@'", path));
  return VAR_NULL;
}

void vmEnsureStackSize(VM* vm, Fiber* fiber, int size) {
  if (size >= (MAX_STACK_SIZE / sizeof(Var))) {
    VM_SET_ERROR(vm, newString(vm, "Maximum stack limit reached."));
    return;
  }

  if (fiber->stack_size >= size)
    return;

  int new_size = utilPowerOf2Ceil(size);

  Var* old_rbp = fiber->stack; //< Old stack base pointer.
  fiber->stack = (Var*) vmRealloc(vm, fiber->stack, sizeof(Var) * fiber->stack_size,
                                  sizeof(Var) * new_size);
  fiber->stack_size = new_size;

  // If the old stack base pointer is the same as the current, that means the
  // stack hasn't been moved by the reallocation. In that case we're done.
  if (old_rbp == fiber->stack)
    return;

  // If we reached here that means the stack is moved by the reallocation and
  // we have to update all the pointers that pointing to the old stack slots.

  //
  //                                     '        '
  //             '        '              '        '
  //             '        '              |        | <new_rsp
  //    old_rsp> |        |              |        |
  //             |        |       .----> | value  | <new_ptr
  //             |        |       |      |        |
  //    old_ptr> | value  | ------'      |________| <new_rbp
  //             |        | ^            new stack
  //    old_rbp> |________| | height
  //             old stack
  //
  //            new_ptr = new_rbp      + height
  //                    = fiber->stack + ( old_ptr  - old_rbp )
#define MAP_PTR(old_ptr) (fiber->stack + ((old_ptr) - old_rbp))

  // Update the stack top pointer and the return pointer.
  fiber->sp = MAP_PTR(fiber->sp);
  fiber->ret = MAP_PTR(fiber->ret);

  // Update the stack base pointer of the call frames.
  for (int i = 0; i < fiber->frame_count; i++) {
    CallFrame* frame = fiber->frames + i;
    frame->rbp = MAP_PTR(frame->rbp);
  }
}

// The return address for the next call frame (rbp) has to be set to the
// fiber's ret (fiber->ret == next rbp).
static inline void pushCallFrame(VM* vm, const Closure* closure) {
  ASSERT(!closure->fn->is_native, OOPS);
  ASSERT(vm->fiber->ret != NULL, OOPS);

  // Grow the stack frame if needed.
  if (vm->fiber->frame_count + 1 > vm->fiber->frame_capacity) {
    // Native functions doesn't allocate a frame initially.
    int new_capacity = vm->fiber->frame_capacity << 1;
    if (new_capacity == 0)
      new_capacity = 1;

    vm->fiber->frames = (CallFrame*) vmRealloc(vm, vm->fiber->frames,
                                               sizeof(CallFrame) * vm->fiber->frame_capacity,
                                               sizeof(CallFrame) * new_capacity);
    vm->fiber->frame_capacity = new_capacity;
  }

  // Grow the stack if needed.
  int current_stack_slots = (int) (vm->fiber->sp - vm->fiber->stack) + 1;
  int needed = closure->fn->fn->stack_size + current_stack_slots;
  vmEnsureStackSize(vm, vm->fiber, needed);

  CallFrame* frame = vm->fiber->frames + vm->fiber->frame_count++;
  frame->rbp = vm->fiber->ret;
  frame->closure = closure;
  frame->ip = closure->fn->fn->opcodes.data;

  // Capture thiz.
  frame->thiz = vm->fiber->thiz;
  vm->fiber->thiz = VAR_UNDEFINED;
}

static inline void reuseCallFrame(VM* vm, const Closure* closure) {
  ASSERT(!closure->fn->is_native, OOPS);
  ASSERT(closure->fn->arity >= 0, OOPS);
  ASSERT(vm->fiber->frame_count > 0, OOPS);

  Fiber* fb = vm->fiber;

  CallFrame* frame = fb->frames + fb->frame_count - 1;
  frame->closure = closure;
  frame->ip = closure->fn->fn->opcodes.data;

  // Capture thiz.
  frame->thiz = vm->fiber->thiz;
  vm->fiber->thiz = VAR_UNDEFINED;

  ASSERT(*frame->rbp == VAR_NULL, OOPS);

  // Move all the argument(s) to the base of the current frame.
  Var* arg = fb->sp - closure->fn->arity;
  Var* target = frame->rbp + 1;
  for (; arg < fb->sp; arg++, target++) {
    *target = *arg;
  }

  // At this point target points to the stack pointer of the next call.
  fb->sp = target;

  // Grow the stack if needed (least probably).
  int needed = (closure->fn->fn->stack_size + (int) (vm->fiber->sp - vm->fiber->stack));
  vmEnsureStackSize(vm, vm->fiber, needed);
}

// Capture the [local] into an upvalue and return it. If the upvalue already
// exists on the fiber, it'll return it.
static Upvalue* captureUpvalue(VM* vm, Fiber* fiber, Var* local) {
  // If the fiber doesn't have any upvalues yet, create new one and add it.
  if (fiber->open_upvalues == NULL) {
    Upvalue* upvalue = newUpvalue(vm, local);
    fiber->open_upvalues = upvalue;
    return upvalue;
  }

  // In the bellow diagram 'u0' is the head of the open upvalues of the fiber.
  // We'll walk through the upvalues to see if any of it's value is similar
  // to the [local] we want to capture.
  //
  // This can be optimized with binary search since the upvalues are sorted
  // but it's not a frequent task neither the number of upvalues would be very
  // few and the local mostly located at the stack top.
  //
  // 1. If say 'l3' is what we want to capture, that local already has an
  //    upavlue 'u1' return it.
  // 2. If say 'l4' is what we want to capture, It doesn't have an upvalue yet.
  //    Create a new upvalue and insert to the link list (ie. u1.next = u3,
  //    u3.next = u2) and return it.
  //
  //           |      |
  //           |  l1  | <-- u0 (u1.value = l3)
  //           |  l2  |     |
  //           |  l3  | <-- u1 (u1.value = l3)
  //           |  l4  |     |
  //           |  l5  | <-- u2 (u2.value = l5)
  //           '------'     |
  //            stack       NULL

  // Edge case: if the local is located higher than all the open upvalues, we
  // cannot walk the chain, it's going to be the new head of the open upvalues.
  if (fiber->open_upvalues->ptr < local) {
    Upvalue* head = newUpvalue(vm, local);
    head->next = fiber->open_upvalues;
    fiber->open_upvalues = head;
    return head;
  }

  // Now we walk the chain of open upvalues and if we find an upvalue for the
  // local return it, otherwise insert it in the chain.
  Upvalue* last = NULL;
  Upvalue* current = fiber->open_upvalues;

  while (current->ptr > local) {
    last = current;
    current = current->next;

    // If the current is NULL, we've walked all the way to the end of the
    // open upvalues, and there isn't one upvalue for the local.
    if (current == NULL) {
      last->next = newUpvalue(vm, local);
      return last->next;
    }
  }

  // If [current] is the upvalue that captured [local] then return it.
  if (current->ptr == local)
    return current;

  ASSERT(last != NULL, OOPS);

  // If we've reached here, the upvalue isn't found, create a new one and
  // insert it to the chain.
  Upvalue* upvalue = newUpvalue(vm, local);
  last->next = upvalue;
  upvalue->next = current;
  return upvalue;
}

// Close all the upvalues for the locals including [top] and higher in the
// stack.
static void closeUpvalues(Fiber* fiber, Var* top) {
  while (fiber->open_upvalues != NULL && fiber->open_upvalues->ptr >= top) {
    Upvalue* upvalue = fiber->open_upvalues;
    upvalue->closed = *upvalue->ptr;
    upvalue->ptr = &upvalue->closed;

    fiber->open_upvalues = upvalue->next;
  }
}

static void vmReportError(VM* vm) {
  ASSERT(VM_HAS_ERROR(vm), "runtimeError() should be called after an error.");

  // TODO: pass the error to the caller of the fiber.

  if (vm->config.stderr_write == NULL)
    return;
  reportRuntimeError(vm, vm->fiber);
}

// Fast numeric check for VM hot paths.
static inline bool vmIsNumeric(Var v, double* out) {
  if (IS_NUM(v)) {
    *out = AS_NUM(v);
    return true;
  }
  if (IS_INT(v)) {
    *out = (double) AS_INT(v);
    return true;
  }
  return false;
}

/******************************************************************************
 * RUNTIME                                                                    *
 *****************************************************************************/

Result vmRunFiber(VM* vm, Fiber* fiber_) {
  // Set the fiber as the VM's current fiber (another root object) to prevent
  // it from garbage collection and get the reference from native functions.
  // If this is being called when running another fiber, that'll be garbage
  // collected, if protected with vmPushTempRef() by the caller otherwise.
  vm->fiber = fiber_;

  ASSERT(fiber_->state == FIBER_NEW || fiber_->state == FIBER_YIELDED, OOPS);
  fiber_->state = FIBER_RUNNING;

  // The instruction pointer.
  register const uint8_t* ip;

  register Var* rbp;         //< Stack base pointer register.
  register Var* thiz;        //< Points to the this in the current call frame.
  register CallFrame* frame; //< Current call frame.
  register Module* module;   //< Currently executing module.
  register Fiber* fiber = fiber_;

#if DEBUG
#define PUSH(value) \
  do { \
    ASSERT(fiber->sp < (fiber->stack + ((intptr_t) fiber->stack_size - 1)), OOPS); \
    (*fiber->sp++ = (value)); \
  } while (false)
#else
#define PUSH(value) (*fiber->sp++ = (value))
#endif

#define POP() (*(--fiber->sp))
#define DROP() (--fiber->sp)
#define PEEK(off) (*(fiber->sp + (off)))
#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t) ((ip[-2] << 8) | ip[-1]))

// Switch back to the caller of the current fiber, will be called when we're
// done with the fiber or aborting it for runtime errors.
#define FIBER_SWITCH_BACK() \
  do { \
    Fiber* caller = fiber->caller; \
    ASSERT(caller == NULL || caller->state == FIBER_RUNNING, OOPS); \
    fiber->state = FIBER_DONE; \
    fiber->caller = NULL; \
    fiber = caller; \
    vm->fiber = fiber; \
  } while (false)

// Check if any runtime error exists and if so returns RESULT_RUNTIME_ERROR.
#define CHECK_ERROR() \
  do { \
    if (VM_HAS_ERROR(vm)) { \
      UPDATE_FRAME(); \
      vmReportError(vm); \
      FIBER_SWITCH_BACK(); \
      return RESULT_RUNTIME_ERROR; \
    } \
  } while (false)

// [err_msg] must be of type String.
#define RUNTIME_ERROR(err_msg) \
  do { \
    VM_SET_ERROR(vm, err_msg); \
    UPDATE_FRAME(); \
    vmReportError(vm); \
    FIBER_SWITCH_BACK(); \
    return RESULT_RUNTIME_ERROR; \
  } while (false)

// Load the last call frame to vm's execution variables to resume/run the
// function.
#define LOAD_FRAME() \
  do { \
    frame = &fiber->frames[fiber->frame_count - 1]; \
    ip = frame->ip; \
    rbp = frame->rbp; \
    thiz = &frame->thiz; \
    module = frame->closure->fn->owner; \
  } while (false)

// Update the frame's execution variables before pushing another call frame.
#define UPDATE_FRAME() frame->ip = ip

#ifdef OPCODE
#error "OPCODE" should not be deifined here.
#endif

#if (defined(__GNUC__) || defined(__clang__)) && !defined(NO_COMPUTED_GOTO)
    #define USE_COMPUTED_GOTO 1
#else
    #define USE_COMPUTED_GOTO 0
#endif

  Opcode instruction;

#if USE_COMPUTED_GOTO
#define OPCODE(CODE) L_##CODE
#define DISPATCH() \
  do { \
    instruction = (Opcode) READ_BYTE(); \
    goto *dispatch_table[instruction]; \
  } while (false)
#else
#define OPCODE(CODE) case OP_##CODE
#define DISPATCH() goto L_vm_main_loop
#endif

  // Load the fiber's top call frame to the vm's execution variables.
  LOAD_FRAME();

L_vm_main_loop:
  // This NO_OP is required since Labels can only be followed by statements
  // and, declarations are not statements, If the macro DUMP_STACK isn't
  // defined, the next line become a declaration (Opcode instruction;).
  NO_OP;

#define _DUMP_STACK() \
  do { \
    system("clear"); \
    dumpGlobalValues(vm); \
    dumpStackFrame(vm); \
    DEBUG_BREAK(); \
  } while (false)

#if DUMP_STACK && defined(DEBUG)
  _DUMP_STACK();
#endif
#undef _DUMP_STACK

#if USE_COMPUTED_GOTO
#undef OPCODE
#define OPCODE(name, params, stack) &&L_##name,
  static void* dispatch_table[] = {
#include "../shared/saynaa_opcodes.h"
  };
#undef OPCODE
#define OPCODE(CODE) L_##CODE
  DISPATCH();
#else
  switch (instruction = (Opcode) READ_BYTE()) {
#endif
    OPCODE(PUSH_CONSTANT) : {
      uint16_t index = READ_SHORT();
      ASSERT_INDEX(index, module->constants.count);
      PUSH(module->constants.data[index]);
      DISPATCH();
    }

    OPCODE(PUSH_NULL) : PUSH(VAR_NULL);
    DISPATCH();

    OPCODE(PUSH_0) : PUSH(VAR_NUM(0));
    DISPATCH();

    OPCODE(PUSH_TRUE) : PUSH(VAR_TRUE);
    DISPATCH();

    OPCODE(PUSH_FALSE) : PUSH(VAR_FALSE);
    DISPATCH();

    OPCODE(SWAP) : {
      Var tmp = *(fiber->sp - 1);
      *(fiber->sp - 1) = *(fiber->sp - 2);
      *(fiber->sp - 2) = tmp;
      DISPATCH();
    }

    OPCODE(DUP) : {
      PUSH(*(fiber->sp - 1));
      DISPATCH();
    }

    OPCODE(PUSH_LIST) : {
      List* list = newList(vm, (uint32_t) READ_SHORT());
      PUSH(VAR_OBJ(list));
      DISPATCH();
    }

    OPCODE(PUSH_MAP) : {
      Map* map = newMap(vm);
      PUSH(VAR_OBJ(map));
      DISPATCH();
    }

    OPCODE(PUSH_THIS) : {
      PUSH(*thiz);
      DISPATCH();
    }

    OPCODE(LIST_APPEND) : {
      Var elem = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var list = PEEK(-2);
      ASSERT(IS_OBJ_TYPE(list, OBJ_LIST), OOPS);
      VarBufferWrite(&((List*) AS_OBJ(list))->elements, vm, elem);
      DROP(); // elem
      DISPATCH();
    }

    OPCODE(MAP_INSERT) : {
      Var value = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var key = PEEK(-2);   // Don't pop yet, we need the reference for gc.
      Var on = PEEK(-3);

      ASSERT(IS_OBJ_TYPE(on, OBJ_MAP), OOPS);

      if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
        RUNTIME_ERROR(stringFormat(vm, "$ type is not hashable.", varTypeName(key)));
      }
      mapSet(vm, (Map*) AS_OBJ(on), key, value);

      DROP(); // value
      DROP(); // key

      DISPATCH();
    }

    OPCODE(MAP_APPEND) : {
      Var value = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var on = PEEK(-2);

      ASSERT(IS_OBJ_TYPE(on, OBJ_MAP), OOPS);

      Map* map = (Map*) AS_OBJ(on);
      Var key = VAR_NUM((double) map->next_index);
      mapSet(vm, map, key, value);

      DROP(); // value

      DISPATCH();
    }

    OPCODE(PUSH_LOCAL_0) :
        OPCODE(PUSH_LOCAL_1) :
        OPCODE(PUSH_LOCAL_2) :
        OPCODE(PUSH_LOCAL_3) :
        OPCODE(PUSH_LOCAL_4) :
        OPCODE(PUSH_LOCAL_5) :
        OPCODE(PUSH_LOCAL_6) : OPCODE(PUSH_LOCAL_7) : OPCODE(PUSH_LOCAL_8) : {
      int index = (int) (instruction - OP_PUSH_LOCAL_0);
      PUSH(rbp[index + 1]); // +1: rbp[0] is return value.
      DISPATCH();
    }
    OPCODE(PUSH_LOCAL_N) : {
      uint16_t index = READ_SHORT();
      PUSH(rbp[index + 1]); // +1: rbp[0] is return value.
      DISPATCH();
    }

    OPCODE(STORE_LOCAL_0) :
        OPCODE(STORE_LOCAL_1) :
        OPCODE(STORE_LOCAL_2) :
        OPCODE(STORE_LOCAL_3) :
        OPCODE(STORE_LOCAL_4) :
        OPCODE(STORE_LOCAL_5) :
        OPCODE(STORE_LOCAL_6) :
        OPCODE(STORE_LOCAL_7) : OPCODE(STORE_LOCAL_8) : {
      int index = (int) (instruction - OP_STORE_LOCAL_0);
      rbp[index + 1] = PEEK(-1); // +1: rbp[0] is return value.
      DISPATCH();
    }
    OPCODE(STORE_LOCAL_N) : {
      uint16_t index = READ_SHORT();
      rbp[index + 1] = PEEK(-1); // +1: rbp[0] is return value.
      DISPATCH();
    }

    OPCODE(PUSH_GLOBAL) : {
      uint16_t index = READ_SHORT();
      ASSERT_INDEX(index, module->globals.count);
      PUSH(module->globals.data[index]);
      DISPATCH();
    }

    OPCODE(PUSH_GLOBAL_NAME) : {
      uint16_t name_index = READ_SHORT();
      String* name = moduleGetStringAt(module, (int) name_index);
      if (name == NULL) {
        RUNTIME_ERROR(stringFormat(vm, "Invalid global name in module data."));
      }

      int g_index = moduleGetGlobalIndexByName(vm, module, name);
      if (g_index == -1) {
        int missing_index = moduleGetGlobalIndex(module, LITS__missing,
                                                 (uint32_t) strlen(LITS__missing));
        if (missing_index != -1) {
          Var missing = module->globals.data[missing_index];
          if (IS_OBJ_TYPE(missing, OBJ_CLOSURE)) {
            Var args[1] = { VAR_OBJ(name) };
            Var result = VAR_NULL;
            Result call_result = vmCallFunction(vm, (Closure*) AS_OBJ(missing),
                                                1, args, &result);
            if (call_result != RESULT_SUCCESS) {
              CHECK_ERROR();
            }
            if (!IS_NULL(result) && !IS_UNDEF(result)) {
              PUSH(result);
              DISPATCH();
            }
          }
        }

        RUNTIME_ERROR(stringFormat(vm, "Name '@' is not defined.", name));
      }

      PUSH(module->globals.data[g_index]);
      DISPATCH();
    }

    OPCODE(STORE_GLOBAL) : {
      uint16_t index = READ_SHORT();
      ASSERT_INDEX(index, module->globals.count);
      module->globals.data[index] = PEEK(-1);
      DISPATCH();
    }

    OPCODE(STORE_GLOBAL_NAME) : {
      uint16_t name_index = READ_SHORT();
      String* name = moduleGetStringAt(module, (int) name_index);
      if (name == NULL) {
        RUNTIME_ERROR(stringFormat(vm, "Invalid global name in module data."));
      }
      moduleSetGlobal(vm, module, name->data, name->length, PEEK(-1));
      DISPATCH();
    }

    OPCODE(PUSH_BUILTIN_FN) : {
      uint8_t index = READ_BYTE();
      ASSERT_INDEX(index, vm->builtins_count);
      Closure* closure = vm->builtins_funcs[index];
      PUSH(VAR_OBJ(closure));
      DISPATCH();
    }

    OPCODE(PUSH_BUILTIN_TY) : {
      uint8_t index = READ_BYTE();
      ASSERT_INDEX(index, vINSTANCE);
      Class* cls = vm->builtin_classes[index];
      PUSH(VAR_OBJ(cls));
      DISPATCH();
    }

    OPCODE(PUSH_UPVALUE) : {
      uint16_t index = READ_SHORT();
      PUSH(*(frame->closure->upvalues[index]->ptr));
      DISPATCH();
    }

    OPCODE(STORE_UPVALUE) : {
      uint16_t index = READ_SHORT();
      *(frame->closure->upvalues[index]->ptr) = PEEK(-1);
      DISPATCH();
    }

    OPCODE(PUSH_CLOSURE) : {
      uint16_t index = READ_SHORT();
      ASSERT_INDEX(index, module->constants.count);
      ASSERT(IS_OBJ_TYPE(module->constants.data[index], OBJ_FUNC), OOPS);
      Function* fn = (Function*) AS_OBJ(module->constants.data[index]);

      Closure* closure = newClosure(vm, fn);
      vmPushTempRef(vm, &closure->_super); // closure.

      // Capture the vaupes.
      for (int i = 0; i < fn->upvalue_count; i++) {
        uint8_t is_immediate = READ_BYTE();
        uint16_t idx = READ_SHORT();

        if (is_immediate) {
          // rbp[0] is the return value, rbp + 1 is the first local and so on.
          closure->upvalues[i] = captureUpvalue(vm, fiber, (rbp + 1 + idx));
        } else {
          // The upvalue is already captured by the current function, reuse it.
          closure->upvalues[i] = frame->closure->upvalues[idx];
        }
      }

      PUSH(VAR_OBJ(closure));
      vmPopTempRef(vm); // closure.

      DISPATCH();
    }

    OPCODE(CREATE_CLASS) : {
      Var cls = POP();
      if (!IS_OBJ_TYPE(cls, OBJ_CLASS)) {
        RUNTIME_ERROR(newString(vm, "Cannot inherit a non class object."));
      }

      Class* base = (Class*) AS_OBJ(cls);

      // All Builtin type class except for Object are "final" ie. cannot
      // be inherited from.
      if (base->class_of != vINSTANCE && base->class_of != vOBJECT) {
        RUNTIME_ERROR(stringFormat(vm, "$ type cannot be inherited.",
                                   getVarTypeName(base->class_of)));
      }

      uint16_t index = READ_SHORT();
      ASSERT_INDEX(index, module->constants.count);
      ASSERT(IS_OBJ_TYPE(module->constants.data[index], OBJ_CLASS), OOPS);

      Class* drived = (Class*) AS_OBJ(module->constants.data[index]);
      drived->super_class = base;

      PUSH(VAR_OBJ(drived));
      DISPATCH();
    }

    OPCODE(BIND_METHOD) : {
      ASSERT(IS_OBJ_TYPE(PEEK(-1), OBJ_CLOSURE), OOPS);
      ASSERT(IS_OBJ_TYPE(PEEK(-2), OBJ_CLASS), OOPS);

      Closure* method = (Closure*) AS_OBJ(PEEK(-1));
      Class* cls = (Class*) AS_OBJ(PEEK(-2));

      bindMethod(vm, cls, method);

      DROP();
      DISPATCH();
    }

    OPCODE(CLOSE_UPVALUE) : {
      closeUpvalues(fiber, fiber->sp - 1);
      DROP();
      DISPATCH();
    }

    OPCODE(POP) : DROP();
    DISPATCH();

    OPCODE(IMPORT_WILDCARD) : {
      uint16_t index = READ_SHORT();
      String* import_path = moduleGetStringAt(module, (int) index);
      ASSERT(import_path != NULL, OOPS);

      String* current_path = module->path;
      // In REPL or some contexts, current_path might be NULL.
      // Default to "." if NULL.
      const char* base_path = (current_path != NULL) ? current_path->data : ".";

      char search_path[MAX_PATH_LEN];

      // Convert import path to a filesystem path for directory walking only.
      // Keep `import_path` unchanged so custom searchers still receive
      // the original dotted module path.
      char import_path_fs[MAX_PATH_LEN];
      wildcardImportToFsPath(import_path, import_path_fs, sizeof(import_path_fs));

      utilResolvePath(search_path, MAX_PATH_LEN, base_path, import_path_fs);

      VarBuffer modules;
      VarBufferInit(&modules);

      WildcardImportRuntimeData data;
      data.vm = vm;
      data.base_ptr = import_path;
      data.current_ptr = current_path; // Keep as is, vmImportModule handles NULL?
      data.modules = &modules;
      data.target_module = module;

      // Update frame before external call to ensure IP is saved (for GC safety).
      UPDATE_FRAME();

      bool success = utilWalkDirectory(search_path, SAYNAA_FILE_EXT,
                                       wildcardRuntimeCallback, &data);
      if (!success) {
        // Not a directory? Try single module import.
        // Pass the ORIGINAL import path (e.g. "path.to.module"), NOT proper path.
        Var imported = vmImportModule(vm, current_path, import_path);
        if (IS_OBJ_TYPE(imported, OBJ_MODULE)) {
          VarBufferWrite(&modules, vm, imported);
        } else {
          // If failed, an error is already set by vmImportModule.
          // We should bail out to report it.
          CHECK_ERROR();
        }
      }

      // 1. Check for uninitialized modules and run them ONE BY ONE.
      // This uses a "rewind and retry" strategy to handle recursion and stack safely.
      for (int i = 0; i < modules.count; i++) {
        Module* imported = (Module*) AS_OBJ(modules.data[i]);
        if (!imported->initialized) {
          imported->initialized = true;
          ASSERT(imported->body != NULL, OOPS);

          // Push the module to reserve a slot for the return value.
          PUSH(VAR_OBJ(imported));
          fiber->ret = fiber->sp - 1;

          pushCallFrame(vm, imported->body);

          // Rewind IP of the current frame so we execute this IMPORT_WILDCARD instruction
          // again when the module returns.
          // 3 bytes = 1 (OP_IMPORT_WILDCARD) + 2 (SHORT index)
          frame->ip -= 3;

          // Load the new frame and run it.
          LOAD_FRAME();
          CHECK_ERROR();

          VarBufferClear(&modules, vm);
          DISPATCH();
          // The VM will now execute the imported module.
          // When it returns, it will resume THIS frame at the rewound IP.
        }
      }

      // 2. If we reach here, all modules are initialized. Now import their symbols.
      for (int i = 0; i < modules.count; i++) {
        Module* imported = (Module*) AS_OBJ(modules.data[i]);

        // Copy public globals from imported module to current module
        for (uint32_t j = 0; j < imported->global_names.count; j++) {
          uint32_t name_idx = imported->global_names.data[j];
          ASSERT(name_idx < imported->constants.count, OOPS);

          String* name = moduleGetStringAt(imported, (int) name_idx);
          if (name == NULL) {
            continue;
          }

          // Skip internal/private names (starting with @ or _)
          if (name->length > 0 && (name->data[0] == '@' || name->data[0] == '_')) {
            continue;
          }

          // Re-fetch the global value from the module's globals buffer
          // Note: The index in 'globals' matches the index in 'global_names' (j)
          Var value = imported->globals.data[j];
          moduleSetGlobal(vm, module, name->data, name->length, value);
        }
      }

      // Reload frame (just in case)
      LOAD_FRAME();

      VarBufferClear(&modules, vm);
      DISPATCH();
    }

    OPCODE(IMPORT) : {
      uint16_t index = READ_SHORT();
      String* name = moduleGetStringAt(module, (int) index);
      if (name == NULL) {
        RUNTIME_ERROR(stringFormat(vm, "Invalid import name in module data."));
      }

      // Regular import bytecode is followed by STORE_GLOBAL for the imported symbol.
      // For handled imports (true), we bind from that target global slot.
      bool has_target_global = false;
      uint16_t target_global_index = 0;
      if ((Opcode) (*ip) == OP_STORE_GLOBAL) {
        target_global_index = (uint16_t) ((ip[1] << 8) | ip[2]);
        ASSERT_INDEX(target_global_index, module->globals.count);
        has_target_global = true;
      }

      Var _imported = vmImportModule(vm, module->path, name);
      CHECK_ERROR();

      // If a searcher returned true, it means "handled".
      // In this mode, true is only a sentinel and import value is read from
      // the target global slot (supports null values set by define()).
      if (IS_BOOL(_imported) && AS_BOOL(_imported) == true) {
        if (!has_target_global && (Opcode) (*ip) != OP_STORE_GLOBAL_NAME) {
          RUNTIME_ERROR(stringFormat(vm,
                                     "Invalid import instruction state for '@': "
                                     "missing STORE_GLOBAL target.",
                                     name));
        }

        if ((Opcode) (*ip) == OP_STORE_GLOBAL) {
          _imported = module->globals.data[target_global_index];

        } else if ((Opcode) (*ip) == OP_STORE_GLOBAL_NAME) {
          uint16_t name_index = (uint16_t) ((ip[1] << 8) | ip[2]);
          String* gname = moduleGetStringAt(module, (int) name_index);
          if (gname == NULL) {
            RUNTIME_ERROR(stringFormat(vm, "Invalid import target name in module data."));
          }

          int g_index = moduleGetGlobalIndexByName(vm, module, gname);
          if (g_index == -1) {
            _imported = VAR_NULL;
          } else {
            _imported = module->globals.data[g_index];
          }
        }
      }

      // NOTE: _imported could be any value (module, class, function or just true).
      // We only execute module body if it's a module.
      // ASSERT(IS_OBJ_TYPE(_imported, OBJ_MODULE), OOPS);

      PUSH(_imported);

      if (IS_OBJ_TYPE(_imported, OBJ_MODULE)) {
        Module* imported = (Module*) AS_OBJ(_imported);
        if (!imported->initialized) {
          imported->initialized = true;

          ASSERT(imported->body != NULL, OOPS);

          UPDATE_FRAME(); //< Update the current frame's ip.

          // Note that we're setting the main function's return address to the
          // module itself (for every other function we'll push a null at the
          // rbp before calling them and it'll be returned without modified if
          // the function doesn't returned anything). Also We can't return from
          // the body of the module, so the main function will return what's at
          // the rbp without modifying it. So at the end of the main function
          // the stack top would be the module itself.
          fiber->ret = fiber->sp - 1;
          pushCallFrame(vm, imported->body);

          LOAD_FRAME();  //< Load the top frame to vm's execution variables.
          CHECK_ERROR(); //< Stack overflow.
        }
      }

      DISPATCH();
    }

    {
      uint8_t argc;
      Var callable;
      const Closure* closure;

      uint16_t index; //< To get the method name.
      String* name;   //< The method name.

      OPCODE(SUPER_CALL) : argc = READ_BYTE();
      fiber->ret = (fiber->sp - argc - 1);
      fiber->thiz = *fiber->ret; //< This for the next call.
      index = READ_SHORT();
      name = moduleGetStringAt(module, (int) index);
      Closure* super_method = getSuperMethod(vm, fiber->thiz, name);
      CHECK_ERROR(); // Will return if super_method is NULL.
      callable = VAR_OBJ(super_method);
      goto L_do_call;

      OPCODE(METHOD_CALL) : {
        const uint8_t* call_site = ip - 1;
        argc = READ_BYTE();
        fiber->ret = (fiber->sp - argc - 1);
        fiber->thiz = *fiber->ret; //< This for the next call.

        index = READ_SHORT();
        name = moduleGetStringAt(module, (int) index);

        Class* recv_cls = getClass(vm, fiber->thiz);
        VMMethodInlineCacheEntry* mic = vmMethodInlineCacheAt(vm, call_site);
        if (mic->site == call_site
            && mic->epoch == vm->inline_cache_epoch
            && mic->cls == recv_cls
            && mic->name == name
            && mic->method != NULL) {
          callable = VAR_OBJ(mic->method);
          goto L_do_call;
        }

        Closure* resolved_method = NULL;
        if (hasMethod(vm, fiber->thiz, name, &resolved_method)) {
          callable = VAR_OBJ(resolved_method);

          mic->site = call_site;
          mic->epoch = vm->inline_cache_epoch;
          mic->cls = recv_cls;
          mic->name = name;
          mic->method = resolved_method;
          goto L_do_call;
        }

        callable = varGetAttrib(vm, fiber->thiz, name, false, true);
        CHECK_ERROR();
        goto L_do_call;
      }

      OPCODE(CALL) : OPCODE(TAIL_CALL) : {
        argc = READ_BYTE();
        fiber->ret = fiber->sp - argc - 1;
        callable = *fiber->ret;
      }

    L_do_call:
      // Raw functions cannot be on the stack, since they're not first
      // class citizens.
      ASSERT(!IS_OBJ_TYPE(callable, OBJ_FUNC), OOPS);

      *(fiber->ret) = VAR_NULL; //< Set the return value to null.

      if (IS_OBJ_TYPE(callable, OBJ_CLOSURE)) {
        closure = (const Closure*) AS_OBJ(callable);

      } else if (IS_OBJ_TYPE(callable, OBJ_METHOD_BIND)) {
        const MethodBind* mb = (const MethodBind*) AS_OBJ(callable);
        /*if (IS_UNDEF(mb->instance)) {
          RUNTIME_ERROR(newString(vm, "Cannot call an unbound
        method.")); CHECK_ERROR();
        }*/
        fiber->thiz = mb->instance;
        closure = mb->method;

      } else if (IS_OBJ_TYPE(callable, OBJ_CLASS)) {
        Class* cls = (Class*) AS_OBJ(callable);

        Closure* new_method = getMagicMethod(cls, METHOD_NEW);
        if (new_method != NULL) {
          Var new_instance = VAR_NULL;
          Result new_result = vmCallMethod(vm, VAR_OBJ(cls), new_method, argc,
                                           fiber->ret + 1, &new_instance);
          if (new_result != RESULT_SUCCESS) {
            CHECK_ERROR();
          }

          if (IS_NULL(new_instance) || IS_UNDEF(new_instance)) {
            // Fall back to default allocation and call _init.
            fiber->thiz = preConstructThis(vm, cls);
            CHECK_ERROR();
            *fiber->ret = fiber->thiz;
            closure = (const Closure*) getMagicMethod(cls, METHOD_INIT);
            if (closure == NULL) {
              if (argc != 0) {
                String* msg = stringFormat(vm,
                                           "Expected exactly 0 argument(s) "
                                           "for constructor $.",
                                           cls->name->data);
                RUNTIME_ERROR(msg);
              }
              fiber->thiz = VAR_UNDEFINED;
              DISPATCH();
            }

          } else {
            *fiber->ret = new_instance;
            fiber->thiz = new_instance;

            closure = (const Closure*) getMagicMethod(cls, METHOD_INIT);
            if (closure == NULL || !IS_OBJ_TYPE(new_instance, OBJ_INST)) {
              fiber->sp = fiber->ret + 1; // Pop args, leave return value.
              fiber->thiz = VAR_UNDEFINED;
              DISPATCH();
            }
          }

        } else {
          // Allocate / create a new this before calling constructor on it.
          fiber->thiz = preConstructThis(vm, cls);
          CHECK_ERROR();

          // Note:
          // For instance the constructor will update this and return
          // the instance (which might not be necessary since we're
          // setting it here).
          *fiber->ret = fiber->thiz;

          closure = (const Closure*) getMagicMethod(cls, METHOD_INIT);

          // No constructor is defined on the class. Just return thiz.
          if (closure == NULL) {
            if (argc != 0) {
              String* msg = stringFormat(vm,
                                         "Expected exactly 0 argument(s) "
                                         "for constructor $.",
                                         cls->name->data);
              RUNTIME_ERROR(msg);
            }

            fiber->thiz = VAR_UNDEFINED;
            DISPATCH();
          }
        }

      } else {
        closure = NULL;

        // try to call a "callable instance" via "_call".
        if (IS_OBJ_TYPE(callable, OBJ_INST)) {
          Instance* inst = (Instance*) AS_OBJ(callable);
          closure = getMagicMethod(inst->cls, METHOD_CALL);
        }

        if (closure == NULL) {
          RUNTIME_ERROR(stringFormat(vm, "$ '$'.",
                                     "Expected a callable to "
                                     "call, instead got",
                                     varTypeName(callable)));

        } else {
          fiber->thiz = callable;
        }
      }

      // If we reached here it's a valid callable.
      ASSERT(closure != NULL, OOPS);

      // Current call semantics: extra arguments are dropped; missing ones become null.
      if (closure->fn->arity != -1) {
        if (argc > closure->fn->arity) { // adjust stack
          fiber->sp -= argc - closure->fn->arity;
        }
        while (closure->fn->arity > argc) {
          PUSH(VAR_NULL);
          argc++;
        }
      }

      if (closure->fn->is_native) {
        if (closure->fn->native == NULL) {
          RUNTIME_ERROR(stringFormat(vm, "Native function pointer of $ was NULL.",
                                     closure->fn->name));
        }

        // Update the current frame's ip.
        UPDATE_FRAME();

        closure->fn->native(vm); //< Call the native function.

        // Calling yield() will change vm->fiber to it's caller fiber, which
        // would be null if we're not running the function with a fiber.
        if (vm->fiber == NULL)
          return RESULT_SUCCESS;

        // Pop function arguments except for the return value.
        // Note that calling fiber_new() and yield() would change the
        // vm->fiber so we're using fiber.
        fiber->sp = fiber->ret + 1;

        // If the fiber has changed, Load the top frame to vm's
        // execution variables.
        if (vm->fiber != fiber) {
          fiber = vm->fiber;

          // A switched fiber must have at least one VM frame to resume.
          // Native-only fibers (frame_count == 0) cannot be resumed here.
          if (fiber == NULL || fiber->frame_count == 0) {
            RUNTIME_ERROR(newString(
                vm, "Cannot continue: switched fiber has no VM call frame. "
                    "Start it with fiber_run() before resuming."));
          }

          LOAD_FRAME();
        }

        CHECK_ERROR();

      } else {
        if (instruction == OP_TAIL_CALL) {
          reuseCallFrame(vm, closure);
          LOAD_FRAME(); //< Re-load the frame to vm's execution variables.

        } else {
          ASSERT((instruction == OP_CALL) || (instruction == OP_METHOD_CALL)
                     || (instruction == OP_SUPER_CALL),
                 OOPS);

          UPDATE_FRAME(); //< Update the current frame's ip.
          pushCallFrame(vm, closure);
          LOAD_FRAME();  //< Load the top frame to vm's execution variables.
          CHECK_ERROR(); //< Stack overflow.
        }
      }

      DISPATCH();
    }

    OPCODE(ITER_TEST) : {
      Var seq = PEEK(-3);

      // Primitive types are not iterable.
      if (!IS_OBJ(seq)) {
        if (IS_NULL(seq)) {
          RUNTIME_ERROR(newString(vm, "Null is not iterable."));
        } else if (IS_BOOL(seq)) {
          RUNTIME_ERROR(newString(vm, "Boolenan is not iterable."));
        } else if (IS_NUM(seq)) {
          RUNTIME_ERROR(newString(vm, "Number is not iterable."));
        } else {
          UNREACHABLE();
        }
      }

      DISPATCH();
    }

    OPCODE(ITER) : {
      Var* value = (fiber->sp - 1);
      Var* iterator = (fiber->sp - 2);
      Var seq = PEEK(-3);
      uint16_t jump_offset = READ_SHORT();

#define JUMP_ITER_EXIT() \
  do { \
    ip += jump_offset; \
    DISPATCH(); \
  } while (false)

      bool cont = varIterate(vm, seq, iterator, value);
      CHECK_ERROR();
      if (!cont)
        JUMP_ITER_EXIT();
      DISPATCH();
    }

    OPCODE(JUMP) : {
      uint16_t offset = READ_SHORT();
      ip += offset;
      DISPATCH();
    }

    OPCODE(LOOP) : {
      uint16_t offset = READ_SHORT();
      ip -= offset;
      DISPATCH();
    }

    OPCODE(JUMP_IF) : {
      Var cond = POP();
      uint16_t offset = READ_SHORT();
      if (toBool(cond)) {
        ip += offset;
      }
      DISPATCH();
    }

    OPCODE(JUMP_IF_NOT) : {
      Var cond = POP();
      uint16_t offset = READ_SHORT();
      if (!toBool(cond)) {
        ip += offset;
      }
      DISPATCH();
    }

    OPCODE(OR) : {
      Var cond = PEEK(-1);
      uint16_t offset = READ_SHORT();
      if (toBool(cond)) {
        ip += offset;
      } else {
        DROP();
      }
      DISPATCH();
    }

    OPCODE(AND) : {
      Var cond = PEEK(-1);
      uint16_t offset = READ_SHORT();
      if (!toBool(cond)) {
        ip += offset;
      } else {
        DROP();
      }
      DISPATCH();
    }

    OPCODE(RETURN) : {
      // Close all the locals of the current frame.
      closeUpvalues(fiber, rbp + 1);

      // Set the return value.
      Var ret_value = POP();

      // Guard against corrupted return/base pointers to avoid native crashes.
      if (fiber->ret == NULL || fiber->stack == NULL || fiber->ret < fiber->stack
          || fiber->ret >= (fiber->stack + fiber->stack_size)) {
        RUNTIME_ERROR(newString(vm, "Invalid fiber return pointer state."));
      }

      // Pop the last frame, and if no more call frames, we're done with
      // the current fiber.
      if (--fiber->frame_count == 0) {
        // TODO: if we're evaluating an expression we need to set it's
        // value on the stack.
        // fiber->sp = fiber->stack; ??

        if (fiber->caller == NULL) {
          *fiber->ret = ret_value;
          return RESULT_SUCCESS;

        } else {
          FIBER_SWITCH_BACK();
          *fiber->ret = ret_value;
        }

      } else {
        Var* return_slot = rbp;
        if (return_slot == NULL || return_slot < fiber->stack
            || return_slot >= (fiber->stack + fiber->stack_size)) {
          // Fallback for rare stale frame-base states.
          return_slot = fiber->ret;
        }

        if (return_slot == NULL || return_slot < fiber->stack
            || return_slot >= (fiber->stack + fiber->stack_size)) {
          RUNTIME_ERROR(newString(vm, "Invalid frame base pointer state."));
        }

        *return_slot = ret_value;
        // Pop the params (locals should have popped at this point) and
        // update stack pointer.
        fiber->sp = return_slot + 1; // +1: return slot is returned value.
      }

      LOAD_FRAME();
      DISPATCH();
    }

    OPCODE(GET_ATTRIB) : {
      const uint8_t* attrib_site = ip - 1;
      Var on = PEEK(-1); // Don't pop yet, we need the reference for gc.
      String* name = moduleGetStringAt(module, READ_SHORT());
      ASSERT(name != NULL, OOPS);

      VMAttribInlineCacheEntry* aic = vmAttribInlineCacheAt(vm, attrib_site);
      Var value = VAR_UNDEFINED;
      bool cache_hit = false;

      if (aic->site == attrib_site
          && aic->epoch == vm->inline_cache_epoch
          && aic->name == name) {
        switch (aic->kind) {
          case VM_ATTRIB_IC_INST_INLINE:
            if (IS_OBJ_TYPE(on, OBJ_INST)) {
              Instance* inst = (Instance*) AS_OBJ(on);
              uint32_t slot = aic->slot_or_index;
              if (inst->cls == aic->receiver_cls
                  && slot < inst->inline_attrib_count) {
                String* slot_name = inst->inline_attrib_names[slot];
                if (slot_name == name) {
                  value = inst->inline_attrib_values[slot];
                  cache_hit = true;
                }
              }
            }
            break;

          case VM_ATTRIB_IC_METHOD_BIND:
            if (aic->method != NULL
                && getClass(vm, on) == aic->receiver_cls
                && (aic->receiver_obj == NULL
                    || (IS_OBJ(on) && AS_OBJ(on) == aic->receiver_obj))) {
              MethodBind* mb = newMethodBind(vm, aic->method);
              vmPushTempRef(vm, &mb->_super); // mb.
              mb->instance = on;
              value = VAR_OBJ(mb);
              vmPopTempRef(vm); // mb.
              cache_hit = true;
            }
            break;

          case VM_ATTRIB_IC_NONE:
          default:
            break;
        }
      }

      if (!cache_hit) {
        value = varGetAttrib(vm, on, name, false, false);
        CHECK_ERROR();

        aic->site = attrib_site;
        aic->epoch = vm->inline_cache_epoch;
        aic->kind = VM_ATTRIB_IC_NONE;
        aic->name = name;
        aic->receiver_cls = NULL;
        aic->receiver_obj = NULL;
        aic->slot_or_index = 0;
        aic->method = NULL;

        if (IS_OBJ_TYPE(on, OBJ_INST)) {
          Instance* inst = (Instance*) AS_OBJ(on);
          for (uint8_t i = 0; i < inst->inline_attrib_count; i++) {
            String* slot_name = inst->inline_attrib_names[i];
            if (slot_name == name) {
              aic->kind = VM_ATTRIB_IC_INST_INLINE;
              aic->receiver_cls = inst->cls;
              aic->slot_or_index = i;
              break;
            }
          }
        }

        if (IS_OBJ_TYPE(value, OBJ_METHOD_BIND)) {
          MethodBind* mb = (MethodBind*) AS_OBJ(value);
          if (mb->method != NULL && !IS_OBJ_TYPE(on, OBJ_CLASS)) {
            aic->kind = VM_ATTRIB_IC_METHOD_BIND;
            aic->receiver_cls = getClass(vm, on);
            aic->receiver_obj = NULL;
            aic->method = mb->method;
          }
        }
      }

      DROP(); // on
      PUSH(value);
      DISPATCH();
    }

    OPCODE(GET_ATTRIB_KEEP) : {
      const uint8_t* attrib_site = ip - 1;
      Var on = PEEK(-1);
      String* name = moduleGetStringAt(module, READ_SHORT());
      ASSERT(name != NULL, OOPS);

      VMAttribInlineCacheEntry* aic = vmAttribInlineCacheAt(vm, attrib_site);
      Var value = VAR_UNDEFINED;
      bool cache_hit = false;

      if (aic->site == attrib_site
          && aic->epoch == vm->inline_cache_epoch
          && aic->name == name) {
        switch (aic->kind) {
          case VM_ATTRIB_IC_INST_INLINE:
            if (IS_OBJ_TYPE(on, OBJ_INST)) {
              Instance* inst = (Instance*) AS_OBJ(on);
              uint32_t slot = aic->slot_or_index;
              if (inst->cls == aic->receiver_cls
                  && slot < inst->inline_attrib_count) {
                String* slot_name = inst->inline_attrib_names[slot];
                if (slot_name == name) {
                  value = inst->inline_attrib_values[slot];
                  cache_hit = true;
                }
              }
            }
            break;

          case VM_ATTRIB_IC_METHOD_BIND:
            if (aic->method != NULL
                && getClass(vm, on) == aic->receiver_cls
                && (aic->receiver_obj == NULL
                    || (IS_OBJ(on) && AS_OBJ(on) == aic->receiver_obj))) {
              MethodBind* mb = newMethodBind(vm, aic->method);
              vmPushTempRef(vm, &mb->_super); // mb.
              mb->instance = on;
              value = VAR_OBJ(mb);
              vmPopTempRef(vm); // mb.
              cache_hit = true;
            }
            break;

          case VM_ATTRIB_IC_NONE:
          default:
            break;
        }
      }

      if (!cache_hit) {
        value = varGetAttrib(vm, on, name, false, false);
        CHECK_ERROR();

        aic->site = attrib_site;
        aic->epoch = vm->inline_cache_epoch;
        aic->kind = VM_ATTRIB_IC_NONE;
        aic->name = name;
        aic->receiver_cls = NULL;
        aic->receiver_obj = NULL;
        aic->slot_or_index = 0;
        aic->method = NULL;

        if (IS_OBJ_TYPE(on, OBJ_INST)) {
          Instance* inst = (Instance*) AS_OBJ(on);
          for (uint8_t i = 0; i < inst->inline_attrib_count; i++) {
            String* slot_name = inst->inline_attrib_names[i];
            if (slot_name == name) {
              aic->kind = VM_ATTRIB_IC_INST_INLINE;
              aic->receiver_cls = inst->cls;
              aic->slot_or_index = i;
              break;
            }
          }
        }

        if (IS_OBJ_TYPE(value, OBJ_METHOD_BIND)) {
          MethodBind* mb = (MethodBind*) AS_OBJ(value);
          if (mb->method != NULL && !IS_OBJ_TYPE(on, OBJ_CLASS)) {
            aic->kind = VM_ATTRIB_IC_METHOD_BIND;
            aic->receiver_cls = getClass(vm, on);
            aic->receiver_obj = NULL;
            aic->method = mb->method;
          }
        }
      }

      PUSH(value);
      DISPATCH();
    }

    OPCODE(SET_ATTRIB) : {
      Var value = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var on = PEEK(-2);    // Don't pop yet, we need the reference for gc.
      String* name = moduleGetStringAt(module, READ_SHORT());
      ASSERT(name != NULL, OOPS);
      varSetAttrib(vm, on, name, value, false);

      DROP(); // value
      DROP(); // on
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GET_SUBSCRIPT) : {
      Var key = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var on = PEEK(-2);  // Don't pop yet, we need the reference for gc.
      Var value = varGetSubscript(vm, on, key);
      DROP(); // key
      DROP(); // on
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GET_SUBSCRIPT_KEEP) : {
      Var key = PEEK(-1);
      Var on = PEEK(-2);
      PUSH(varGetSubscript(vm, on, key));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(SET_SUBSCRIPT) : {
      Var value = PEEK(-1); // Don't pop yet, we need the reference for gc.
      Var key = PEEK(-2);   // Don't pop yet, we need the reference for gc.
      Var on = PEEK(-3);    // Don't pop yet, we need the reference for gc.
      varsetSubscript(vm, on, key, value);
      DROP(); // value
      DROP(); // key
      DROP(); // on
      PUSH(value);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(POSITIVE) : {
      // Don't pop yet, we need the reference for gc.
      Var thiz_ = PEEK(-1);
      Var result = varPositive(vm, thiz_);
      DROP(); // this
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(NEGATIVE) : {
      // Don't pop yet, we need the reference for gc.
      Var v = PEEK(-1);
      Var result = varNegative(vm, v);
      DROP(); // this
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(NOT) : {
      // Don't pop yet, we need the reference for gc.
      Var v = PEEK(-1);
      Var result = varNot(vm, v);
      DROP(); // this
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_NOT) : {
      // Don't pop yet, we need the reference for gc.
      Var v = PEEK(-1);
      Var result = varBitNot(vm, v);
      DROP(); // this
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    // Do not ever use PUSH(binaryOp(vm, POP(), POP()));
    // Function parameters are not evaluated in a defined order in C.

    OPCODE(ADD) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE();
      ASSERT(inplace <= 1, OOPS);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        Var result = VAR_NUM(n1 + n2);
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varAdd(vm, l, r, inplace);
      DROP();
      DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(SUBTRACT) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE();
      ASSERT(inplace <= 1, OOPS);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        Var result = VAR_NUM(n1 - n2);
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varSubtract(vm, l, r, inplace);
      DROP();
      DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(MULTIPLY) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE();
      ASSERT(inplace <= 1, OOPS);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        Var result = VAR_NUM(n1 * n2);
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varMultiply(vm, l, r, inplace);
      DROP();
      DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(DIVIDE) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE();
      ASSERT(inplace <= 1, OOPS);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        if (n2 == 0) {
          VM_SET_ERROR(vm, newString(vm, "Division by zero."));
          CHECK_ERROR();
        }
        Var result = VAR_NUM(n1 / n2);
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varDivide(vm, l, r, inplace);
      DROP();
      DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(EXPONENT) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE();
      ASSERT(inplace <= 1, OOPS);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        Var result = VAR_NUM(pow(n1, n2));
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varExponent(vm, l, r, inplace);
      DROP();
      DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(MOD) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE();
      ASSERT(inplace <= 1, OOPS);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        if (n2 == 0) {
          VM_SET_ERROR(vm, newString(vm, "Division by zero."));
          CHECK_ERROR();
        }
        Var result = VAR_NUM(fmod(n1, n2));
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varModulo(vm, l, r, inplace);
      DROP();
      DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_AND) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE();
      ASSERT(inplace <= 1, OOPS);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)
          && floor(n1) == n1 && floor(n2) == n2) {
        Var result = VAR_NUM((double) (((int64_t) n1) & ((int64_t) n2)));
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varBitAnd(vm, l, r, inplace);
      DROP();
      DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_OR) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE();
      ASSERT(inplace <= 1, OOPS);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)
          && floor(n1) == n1 && floor(n2) == n2) {
        Var result = VAR_NUM((double) (((int64_t) n1) | ((int64_t) n2)));
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varBitOr(vm, l, r, inplace);
      DROP();
      DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_XOR) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE();
      ASSERT(inplace <= 1, OOPS);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)
          && floor(n1) == n1 && floor(n2) == n2) {
        Var result = VAR_NUM((double) (((int64_t) n1) ^ ((int64_t) n2)));
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varBitXor(vm, l, r, inplace);
      DROP();
      DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_LSHIFT) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE();
      ASSERT(inplace <= 1, OOPS);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)
          && floor(n1) == n1 && floor(n2) == n2) {
        Var result = VAR_NUM((double) (((int64_t) n1) << ((int64_t) n2)));
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varBitLshift(vm, l, r, inplace);
      DROP();
      DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(BIT_RSHIFT) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      uint8_t inplace = READ_BYTE();
      ASSERT(inplace <= 1, OOPS);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)
          && floor(n1) == n1 && floor(n2) == n2) {
        Var result = VAR_NUM((double) (((int64_t) n1) >> ((int64_t) n2)));
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varBitRshift(vm, l, r, inplace);
      DROP();
      DROP(); // r, l
      PUSH(result);

      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(EQEQ) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        Var result = VAR_BOOL(n1 == n2);
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      if (IS_OBJ_TYPE(l, OBJ_STRING) && IS_OBJ_TYPE(r, OBJ_STRING)) {
        String* ls = AS_STRING(l);
        String* rs = AS_STRING(r);
        Var result = VAR_BOOL(IS_STR_EQ(ls, rs));
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varEqals(vm, l, r);
      DROP();
      DROP(); // r, l
      PUSH(result);
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(NOTEQ) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        Var result = VAR_BOOL(n1 != n2);
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      if (IS_OBJ_TYPE(l, OBJ_STRING) && IS_OBJ_TYPE(r, OBJ_STRING)) {
        String* ls = AS_STRING(l);
        String* rs = AS_STRING(r);
        Var result = VAR_BOOL(!IS_STR_EQ(ls, rs));
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varEqals(vm, l, r);
      DROP();
      DROP(); // r, l
      PUSH(VAR_BOOL(!toBool(result)));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(LT) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        Var result = VAR_BOOL(n1 < n2);
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varLesser(vm, l, r);
      DROP();
      DROP(); // r, l
      PUSH(result);
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(LTEQ) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        Var result = VAR_BOOL(n1 <= n2);
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }

      Var result = varLesser(vm, l, r);
      CHECK_ERROR();
      bool lteq = toBool(result);

      if (!lteq)
        result = varEqals(vm, l, r);
      CHECK_ERROR();

      DROP();
      DROP(); // r, l
      PUSH(result);
      DISPATCH();
    }

    OPCODE(GT) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        Var result = VAR_BOOL(n1 > n2);
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varGreater(vm, l, r);
      DROP();
      DROP(); // r, l
      PUSH(result);
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(GTEQ) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      double n1, n2;
      if (vmIsNumeric(l, &n1) && vmIsNumeric(r, &n2)) {
        Var result = VAR_BOOL(n1 >= n2);
        DROP();
        DROP(); // r, l
        PUSH(result);
        DISPATCH();
      }
      Var result = varGreater(vm, l, r);
      CHECK_ERROR();
      bool gteq = toBool(result);

      if (!gteq)
        result = varEqals(vm, l, r);
      CHECK_ERROR();

      DROP();
      DROP(); // r, l
      PUSH(result);
      DISPATCH();
    }

    OPCODE(RANGE) : {
      // Don't pop yet, we need the reference for gc.
      Var r = PEEK(-1), l = PEEK(-2);
      Var result = varOpRange(vm, l, r);
      DROP();
      DROP(); // r, l
      PUSH(result);
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(IN) : {
      // Don't pop yet, we need the reference for gc.
      Var container = PEEK(-1), elem = PEEK(-2);
      bool contains = varContains(vm, elem, container);
      DROP();
      DROP(); // container, elem
      PUSH(VAR_BOOL(contains));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(IS) : {
      // Don't pop yet, we need the reference for gc.
      Var type = PEEK(-1), inst = PEEK(-2);
      bool is = varIsType(vm, inst, type);
      DROP();
      DROP(); // container, elem
      PUSH(VAR_BOOL(is));
      CHECK_ERROR();
      DISPATCH();
    }

    OPCODE(REPL_PRINT) : {
      if (vm->config.stdout_write != NULL) {
        Var tmp = PEEK(-1);
        if (!IS_NULL(tmp)) {
          vm->config.stdout_write(vm, varToString(vm, tmp, true)->data);
          vm->config.stdout_write(vm, "\n");
        }
      }
      DISPATCH();
    }

    OPCODE(END) : UNREACHABLE();
#if !USE_COMPUTED_GOTO
    break;

    default:
      UNREACHABLE();
#endif
#if !USE_COMPUTED_GOTO
  }
#endif

  UNREACHABLE();
  return RESULT_RUNTIME_ERROR;
}