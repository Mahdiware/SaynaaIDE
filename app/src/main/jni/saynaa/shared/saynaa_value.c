/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_value.h"

#include "../runtime/saynaa_vm.h"
#include "../utils/saynaa_utils.h"

#include <ctype.h>
#include <math.h>

#if defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wpointer-to-int-cast"
#elif defined(__clang__)
#pragma clang diagnostic ignored "-Wpointer-to-int-cast"
#endif

// The maximum percentage of the map entries that can be filled before the map
// is grown. A lower percentage reduce collision which makes looks up faster
// but take more memory.
#define MAP_LOAD_PERCENT 75

// The factor a collection would grow by when it's exceeds the current
// capacity. The new capacity will be calculated by multiplying it's old
// capacity by the GROW_FACTOR.
#define GROW_FACTOR 2

#define _MAX(a, b) ((a) > (b) ? (a) : (b))
#define _MIN(a, b) ((a) < (b) ? (a) : (b))

// Buffer implementations.
DEFINE_BUFFER(Uint, uint32_t)
DEFINE_BUFFER(Byte, uint8_t)
DEFINE_BUFFER(Var, Var)
DEFINE_BUFFER(String, String*)
DEFINE_BUFFER(Closure, Closure*)

void ByteBufferAddString(ByteBuffer* thiz, VM* vm, const char* str, uint32_t length) {
  ByteBufferReserve(thiz, vm, (size_t) thiz->count + length);
  for (uint32_t i = 0; i < length; i++) {
    thiz->data[thiz->count++] = *(str++);
  }
}

void ByteBufferAddStringFmt(ByteBuffer* thiz, VM* vm, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);

  va_list copy;
  va_copy(copy, args);
  int length = vsnprintf(NULL, 0, fmt, copy);
  va_end(copy);

  ByteBufferReserve(thiz, vm, thiz->count + (size_t) length + 1);
  vsnprintf((char*) (thiz->data + thiz->count), thiz->capacity - thiz->count, fmt, args);
  thiz->count += length;
  va_end(args);
}

void varInitObject(Object* thiz, VM* vm, ObjectType type) {
  thiz->type = type;
  thiz->is_marked = false;
  thiz->next = vm->first;
  vm->first = thiz;
}

void markObject(VM* vm, Object* thiz) {
  if (thiz == NULL || thiz->is_marked)
    return;
  thiz->is_marked = true;

  // Add the object to the VM's working_set so that we can recursively mark
  // its referenced objects later.
  if (vm->working_set_count >= vm->working_set_capacity) {
    vm->working_set_capacity *= 2;
    vm->working_set = (Object**) vm->config.realloc_fn(
        vm->working_set, vm->working_set_capacity * sizeof(Object*), vm->config.user_data);
  }

  vm->working_set[vm->working_set_count++] = thiz;
}

void markValue(VM* vm, Var thiz) {
  if (!IS_OBJ(thiz))
    return;
  markObject(vm, AS_OBJ(thiz));
}

void markVarBuffer(VM* vm, VarBuffer* thiz) {
  if (thiz == NULL)
    return;
  for (uint32_t i = 0; i < thiz->count; i++) {
    markValue(vm, thiz->data[i]);
  }
}

void markStringBuffer(VM* vm, StringBuffer* thiz) {
  if (thiz == NULL)
    return;
  for (uint32_t i = 0; i < thiz->count; i++) {
    markObject(vm, &thiz->data[i]->_super);
  }
}

void markClosureBuffer(VM* vm, ClosureBuffer* thiz) {
  if (thiz == NULL)
    return;
  for (uint32_t i = 0; i < thiz->count; i++) {
    markObject(vm, &thiz->data[i]->_super);
  }
}

static void popMarkedObjectsInternal(Object* obj, VM* vm) {
  // TODO: trace here.

  switch (obj->type) {
    case OBJ_STRING:
      {
        vm->bytes_allocated += sizeof(String);
        vm->bytes_allocated += ((size_t) ((String*) obj)->capacity);
      }
      break;

    case OBJ_LIST:
      {
        List* list = (List*) obj;
        markVarBuffer(vm, &list->elements);
        vm->bytes_allocated += sizeof(List);
        vm->bytes_allocated += sizeof(Var) * list->elements.capacity;
      }
      break;

    case OBJ_MAP:
      {
        Map* map = (Map*) obj;
        for (uint32_t i = 0; i < map->capacity; i++) {
          if (IS_UNDEF(map->entries[i].key))
            continue;
          markValue(vm, map->entries[i].key);
          markValue(vm, map->entries[i].value);
        }
        vm->bytes_allocated += sizeof(Map);
        vm->bytes_allocated += sizeof(MapEntry) * map->capacity;
      }
      break;

    case OBJ_RANGE:
      {
        vm->bytes_allocated += sizeof(Range);
      }
      break;

    case OBJ_MODULE:
      {
        Module* module = (Module*) obj;
        vm->bytes_allocated += sizeof(Module);

        markObject(vm, &module->path->_super);
        markObject(vm, &module->name->_super);

        markVarBuffer(vm, &module->globals);
        vm->bytes_allocated += sizeof(Var) * module->globals.capacity;

        // Integer buffer has no mark call.
        vm->bytes_allocated += sizeof(uint32_t) * module->global_names.capacity;

        markVarBuffer(vm, &module->constants);
        vm->bytes_allocated += sizeof(Var) * module->constants.capacity;

        markObject(vm, &module->body->_super);
      }
      break;

    case OBJ_FUNC:
      {
        Function* func = (Function*) obj;
        vm->bytes_allocated += sizeof(Function);

        markObject(vm, &func->owner->_super);

        // If a garbage collection is triggered when allocating a name string
        // for this function, it's [fn] property will be NULL.
        if (!func->is_native && func->fn != NULL) {
          Fn* fn = func->fn;
          vm->bytes_allocated += sizeof(Fn);

          vm->bytes_allocated += sizeof(uint8_t) * fn->opcodes.capacity;
          vm->bytes_allocated += sizeof(uint32_t) * fn->oplines.capacity;
        }
      }
      break;

    case OBJ_CLOSURE:
      {
        Closure* closure = (Closure*) obj;
        markObject(vm, &closure->fn->_super);
        for (int i = 0; i < closure->fn->upvalue_count; i++) {
          markObject(vm, &(closure->upvalues[i]->_super));
        }

        vm->bytes_allocated += sizeof(Closure);
        vm->bytes_allocated += sizeof(Upvalue*) * closure->fn->upvalue_count;
      }
      break;

    case OBJ_METHOD_BIND:
      {
        MethodBind* mb = (MethodBind*) obj;
        markObject(vm, &mb->method->_super);
        markValue(vm, mb->instance);

        vm->bytes_allocated += sizeof(MethodBind);
      }
      break;

    case OBJ_UPVALUE:
      {
        Upvalue* upvalue = (Upvalue*) obj;

        // We don't have to mark upvalue->ptr since the [ptr] points to a local
        // in the stack, however we need to mark upvalue->closed incase if it's
        // closed.
        markValue(vm, upvalue->closed);

        vm->bytes_allocated += sizeof(Upvalue);
      }
      break;

    case OBJ_FIBER:
      {
        Fiber* fiber = (Fiber*) obj;
        vm->bytes_allocated += sizeof(Fiber);

        markObject(vm, &fiber->closure->_super);

        // Mark the stack.
        for (Var* local = fiber->stack; local < fiber->sp; local++) {
          markValue(vm, *local);
        }
        vm->bytes_allocated += sizeof(Var) * fiber->stack_size;

        // Mark call frames.
        for (int i = 0; i < fiber->frame_count; i++) {
          markObject(vm, (Object*) &fiber->frames[i].closure->_super);
          markValue(vm, fiber->frames[i].thiz);
        }
        vm->bytes_allocated += sizeof(CallFrame) * fiber->frame_capacity;

        markObject(vm, &fiber->caller->_super);
        markObject(vm, &fiber->native->_super);
        markObject(vm, &fiber->error->_super);

        markValue(vm, fiber->thiz);
      }
      break;

    case OBJ_CLASS:
      {
        Class* cls = (Class*) obj;
        vm->bytes_allocated += sizeof(Class);
        markObject(vm, &cls->owner->_super);
        markObject(vm, &cls->name->_super);
        markObject(vm, &cls->static_attribs->_super);
        // don't need to mark magic_methods, they are all in cls->methods.

        markClosureBuffer(vm, &cls->methods);
        vm->bytes_allocated += sizeof(Closure) * cls->methods.capacity;
      }
      break;

    case OBJ_INST:
      {
        Instance* inst = (Instance*) obj;
        markObject(vm, &inst->attribs->_super);
        markObject(vm, &inst->cls->_super);
        vm->bytes_allocated += sizeof(Instance);
      }
      break;
  }
}

void popMarkedObjects(VM* vm) {
  while (vm->working_set_count > 0) {
    Object* marked_obj = vm->working_set[--vm->working_set_count];
    popMarkedObjectsInternal(marked_obj, vm);
  }
}

Var doubleToVar(double value) {
#if VAR_NAN_TAGGING
  return utilDoubleToBits(value);
#else
  return VAR_NUM(value);
#endif // VAR_NAN_TAGGING
}

double varToDouble(Var value) {
#if VAR_NAN_TAGGING
  return utilDoubleFromBits(value);
#else
  return AS_NUM(value);
#endif // VAR_NAN_TAGGING
}

static String* _allocateString(VM* vm, size_t length) {
  String* string = ALLOCATE_DYNAMIC(vm, String, length + 1, char);
  varInitObject(&string->_super, vm, OBJ_STRING);
  string->length = (uint32_t) length;
  string->data[length] = '\0';
  string->capacity = (uint32_t) (length + 1);
  return string;
}

String* newStringLength(VM* vm, const char* text, uint32_t length) {
  ASSERT(length == 0 || text != NULL, "Unexpected NULL string.");

  String* string = _allocateString(vm, length);

  if (length != 0 && text != NULL)
    memcpy(string->data, text, length);
  string->hash = utilHashString(string->data);

  return string;
}

String* newStringVaArgs(VM* vm, const char* fmt, va_list args) {
  va_list copy;
  va_copy(copy, args);
  int length = vsnprintf(NULL, 0, fmt, copy);
  va_end(copy);

  String* string = _allocateString(vm, (size_t) length);
  vsnprintf(string->data, string->capacity, fmt, args);
  string->hash = utilHashString(string->data);

  return string;
}

List* newList(VM* vm, uint32_t size) {
  List* list = ALLOCATE(vm, List);
  vmPushTempRef(vm, &list->_super); // list.
  varInitObject(&list->_super, vm, OBJ_LIST);
  VarBufferInit(&list->elements);
  if (size > 0) {
    VarBufferFill(&list->elements, vm, VAR_NULL, size);
    list->elements.count = 0;
  }
  vmPopTempRef(vm); // list.
  return list;
}

Map* newMap(VM* vm) {
  Map* map = ALLOCATE(vm, Map);
  varInitObject(&map->_super, vm, OBJ_MAP);
  map->capacity = 0;
  map->count = 0;
  map->entries = NULL;
  return map;
}

Range* newRange(VM* vm, double from, double to) {
  Range* range = ALLOCATE(vm, Range);
  varInitObject(&range->_super, vm, OBJ_RANGE);
  range->from = from;
  range->to = to;
  return range;
}

Module* newModule(VM* vm) {
  Module* module = ALLOCATE(vm, Module);
  memset(module, 0, sizeof(Module));
  varInitObject(&module->_super, vm, OBJ_MODULE);

  VarBufferInit(&module->globals);
  UintBufferInit(&module->global_names);
  VarBufferInit(&module->constants);

  return module;
}

Function* newFunction(VM* vm, const char* name, int length, Module* owner,
                      bool is_native, const char* docstring, int* fn_index) {
  Function* func = ALLOCATE(vm, Function);
  memset(func, 0, sizeof(Function));
  varInitObject(&func->_super, vm, OBJ_FUNC);

  vmPushTempRef(vm, &func->_super); // func

  func->owner = owner;
  func->is_native = is_native;
  func->upvalue_count = 0;
  func->arity = ARITY_UNINITIALIZED;
  func->is_method = false;
  func->docstring = docstring;

  ASSERT(is_native || owner != NULL, OOPS);

  // Only builtin function does't have an owner module.
  if (is_native && owner == NULL) {
    func->name = name;
    func->native = NULL;

  } else {
    uint32_t _fn_index = moduleAddConstant(vm, owner, VAR_OBJ(func));
    if (fn_index)
      *fn_index = _fn_index;
    func->name = moduleAddString(owner, vm, name, length, NULL)->data;

    if (is_native) {
      func->native = NULL;

    } else {
      Fn* fn = ALLOCATE(vm, Fn);
      ByteBufferInit(&fn->opcodes);
      UintBufferInit(&fn->oplines);
      fn->stack_size = 0;
      func->fn = fn;
    }
  }

  vmPopTempRef(vm); // func
  return func;
}

Closure* newClosure(VM* vm, Function* fn) {
  Closure* closure = ALLOCATE_DYNAMIC(vm, Closure, fn->upvalue_count, Upvalue*);
  varInitObject(&closure->_super, vm, OBJ_CLOSURE);

  closure->fn = fn;
  memset(closure->upvalues, 0, sizeof(Upvalue*) * fn->upvalue_count);

  return closure;
}

MethodBind* newMethodBind(VM* vm, Closure* method) {
  MethodBind* mb = ALLOCATE(vm, MethodBind);
  varInitObject(&mb->_super, vm, OBJ_METHOD_BIND);

  mb->method = method;
  mb->instance = VAR_UNDEFINED;

  return mb;
}

Upvalue* newUpvalue(VM* vm, Var* value) {
  Upvalue* upvalue = ALLOCATE(vm, Upvalue);
  varInitObject(&upvalue->_super, vm, OBJ_UPVALUE);

  upvalue->ptr = value;
  upvalue->closed = VAR_NULL;
  upvalue->next = NULL;
  return upvalue;
}

Fiber* newFiber(VM* vm, Closure* closure) {
  ASSERT(closure == NULL || closure->fn->arity >= -1, OOPS);

  Fiber* fiber = ALLOCATE(vm, Fiber);

  // If a garbage collection is triggered here, and the fiber isn't fully
  // constructed -> it's fields are not intialized yet, would cause a crash
  // so we need to memset here.
  memset(fiber, 0, sizeof(Fiber));

  varInitObject(&fiber->_super, vm, OBJ_FIBER);

  vmPushTempRef(vm, &fiber->_super); // fiber.

  fiber->state = FIBER_NEW;
  fiber->closure = closure;

  if (closure == NULL || closure->fn->is_native) {
    // For native functions, we're only using stack for parameters,
    // there won't be any locals or temps (which are belongs to the
    // native "C" stack).

    int stack_size = (closure == NULL) ? 1 : closure->fn->arity + 1;
    stack_size = utilPowerOf2Ceil(stack_size);

    // We need at least 1 stack slot for the return value.
    if (stack_size == 0)
      stack_size++;

    fiber->stack = ALLOCATE_ARRAY(vm, Var, stack_size);
    ASSERT(fiber->stack != NULL, "Out of memory");
    fiber->stack_size = stack_size;
    fiber->ret = fiber->stack;
    fiber->sp = fiber->stack + 1;

  } else {
    // Calculate the stack size.
    int stack_size = utilPowerOf2Ceil(closure->fn->fn->stack_size + 1);
    if (stack_size < MIN_STACK_SIZE)
      stack_size = MIN_STACK_SIZE;
    fiber->stack = ALLOCATE_ARRAY(vm, Var, stack_size);
    fiber->stack_size = stack_size;
    fiber->ret = fiber->stack;
    fiber->sp = fiber->stack + 1;

    // Allocate call frames.
    fiber->frame_capacity = INITIAL_CALL_FRAMES;
    fiber->frames = ALLOCATE_ARRAY(vm, CallFrame, fiber->frame_capacity);
    fiber->frame_count = 1;

    // Initialize the first frame.
    fiber->frames[0].closure = closure;
    fiber->frames[0].ip = closure->fn->fn->opcodes.data;
    fiber->frames[0].rbp = fiber->ret;
  }

  fiber->open_upvalues = NULL;
  fiber->thiz = VAR_UNDEFINED;

  // Initialize the return value to null (doesn't really have to do that here
  // but if we're trying to debut it may crash when dumping the return value).
  *fiber->ret = VAR_NULL;

  vmPopTempRef(vm); // fiber.

  return fiber;
}

Class* newClass(VM* vm, const char* name, int length, Class* super,
                Module* module, const char* docstring, int* cls_index) {
  Class* cls = ALLOCATE(vm, Class);

  // If the garbage collection trigged bellow while allocating for
  // [cls->name] or other properties, the calss is in the root (temp ref)
  // and it's property [cls->name] is un initialized, which cause a crash.
  memset(cls, 0, sizeof(Class));

  varInitObject(&cls->_super, vm, OBJ_CLASS);

  vmPushTempRef(vm, &cls->_super); // class.

  ClosureBufferInit(&cls->methods);
  cls->static_attribs = newMap(vm);

  cls->class_of = vINSTANCE;
  cls->super_class = super;
  cls->docstring = docstring;

  // Initialize to -1 as undefined
  for (int i = 0; i < MAX_MAGIC_METHODS; i++) {
    cls->magic_methods[i] = (Closure*) -1;
  }

  // Builtin types doesn't belongs to a module.
  if (module != NULL) {
    cls->name = moduleAddString(module, vm, name, length, NULL);
    int _cls_index = moduleAddConstant(vm, module, VAR_OBJ(cls));
    if (cls_index)
      *cls_index = _cls_index;
    moduleSetGlobal(vm, module, name, length, VAR_OBJ(cls));
  } else {
    cls->name = newStringLength(vm, name, (uint32_t) length);
  }

  vmPopTempRef(vm); // class.
  return cls;
}

Pointer* newPointer(VM* vm, void* native_ptr, Destructor destructor) {
  Pointer* pointer = ALLOCATE(vm, Pointer);
  varInitObject((Object*) pointer, vm, OBJ_POINTER);
  pointer->native_ptr = native_ptr;
  pointer->destructor = destructor;
  return pointer;
}

Closure* newNativeClosure(VM* vm, const char* name, nativeFn fptr, int arity,
                          const char* docstring) {
  Function* fn = newFunction(vm, name, (int) strlen(name), NULL, true, docstring, NULL);
  fn->arity = arity;
  fn->native = fptr;

  vmPushTempRef(vm, &fn->_super); // fn.
  Closure* body = newClosure(vm, fn);
  vmPopTempRef(vm); // fn.
  return body;
}

Instance* newInstance(VM* vm, Class* cls) {
  ASSERT(cls->class_of == vINSTANCE, "Cannot create an instace of builtin "
                                     "class with newInstance() function.");

  Instance* inst = ALLOCATE(vm, Instance);
  memset(inst, 0, sizeof(Instance));
  varInitObject(&inst->_super, vm, OBJ_INST);

  vmPushTempRef(vm, &inst->_super); // inst.

  inst->cls = cls;
  inst->native = NULL;
  while (cls != NULL) {
    if (cls->new_fn != NULL) {
      inst->native = cls->new_fn(vm);
      break;
    }
    cls = cls->super_class;
  }

  inst->attribs = newMap(vm);

  vmPopTempRef(vm); // inst.
  return inst;
}

List* rangeAsList(VM* vm, Range* thiz) {
  if (thiz->from < thiz->to) {
    List* list = newList(vm, (uint32_t) (thiz->to - thiz->from));
    vmPushTempRef(vm, &list->_super); // list.
    for (double i = thiz->from; i < thiz->to; i++) {
      VarBufferWrite(&list->elements, vm, VAR_NUM(i));
    }
    vmPopTempRef(vm); // list.

    return list;
  } else {
    List* list = newList(vm, (uint32_t) (thiz->from - thiz->to));
    vmPushTempRef(vm, &list->_super); // list.
    for (double i = thiz->from; i > thiz->to; i--) {
      VarBufferWrite(&list->elements, vm, VAR_NUM(i));
    }
    vmPopTempRef(vm); // list.

    return list;
  }
}

double rangeLength(VM* vm, Range* thiz) {
  double length = 0;

  if (thiz->from < thiz->to) {
    for (double i = thiz->from; i < thiz->to; i++) {
      length++;
    }
  } else {
    for (double i = thiz->from; i > thiz->to; i--) {
      length++;
    }
  }

  return length;
}

String* stringLower(VM* vm, String* thiz) {
  // If the string itself is already lower, don't allocate new string.
  uint32_t index = 0;
  for (const char* c = thiz->data; *c != '\0'; c++, index++) {
    if (isupper(*c)) {
      // It contain upper case letters, allocate new lower case string .
      String* lower = newStringLength(vm, thiz->data, thiz->length);

      // Start where the first upper case letter found.
      char* _c = lower->data + (c - thiz->data);
      for (; *_c != '\0'; _c++)
        *_c = (char) tolower(*_c);

      // Since the string is modified re-hash it.
      lower->hash = utilHashString(lower->data);
      return lower;
    }
  }
  // If we reached here the string itself is lower, return it.
  return thiz;
}

String* stringUpper(VM* vm, String* thiz) {
  // If the string itself is already upper don't allocate new string.
  uint32_t index = 0;
  for (const char* c = thiz->data; *c != '\0'; c++, index++) {
    if (islower(*c)) {
      // It contain lower case letters, allocate new upper case string .
      String* upper = newStringLength(vm, thiz->data, thiz->length);

      // Start where the first lower case letter found.
      char* _c = upper->data + (c - thiz->data);
      for (; *_c != '\0'; _c++)
        *_c = (char) toupper(*_c);

      // Since the string is modified re-hash it.
      upper->hash = utilHashString(upper->data);
      return upper;
    }
  }
  // If we reached here the string itself is lower, return it.
  return thiz;
}

String* stringStrip(VM* vm, String* thiz) {
  // Implementation:
  //
  // "     a string with leading and trailing white space    "
  //  ^start >>                                       << end^
  //
  // These 'start' and 'end' pointers will move respectively right and left
  // while it's a white space and return an allocated string from 'start' with
  // length of (end - start + 1). For already trimmed string it'll not
  // allocate a new string, instead returns the same string provided.

  const char* start = thiz->data;
  while (*start && isspace(*start))
    start++;

  // If we reached the end of the string, it's all white space, return
  // an empty string.
  if (*start == '\0') {
    return newStringLength(vm, NULL, 0);
  }

  const char* end = thiz->data + thiz->length - 1;
  while (isspace(*end))
    end--;

  // If the string is already trimmed, return the same string.
  if (start == thiz->data && end == thiz->data + thiz->length - 1) {
    return thiz;
  }

  return newStringLength(vm, start, (uint32_t) (end - start + 1));
}

static const char* _memFind(const char* haystack, uint32_t haystack_len,
                            const char* needle, uint32_t needle_len) {
  if (needle_len == 0)
    return haystack;
  if (haystack_len < needle_len)
    return NULL;

  const char* end = haystack + haystack_len - needle_len;
  for (const char* p = haystack; p <= end; p++) {
    if (*p == *needle) {
      if (memcmp(p, needle, needle_len) == 0)
        return p;
    }
  }
  return NULL;
}

String* stringReplace(VM* vm, String* thiz, String* old, String* new_, int32_t count) {
  // The algorithm:
  //
  // We'll first deduce the maximum possible occurence of the old string.
  //
  //   max_count = floor(thiz.length / old.length)
  //
  // If count == -1 we'll set it to max_count, otherwise we can update our
  // count as follows.
  //
  //   count = min(count, min_count)
  //
  // Now we know the maximum possible length of the new string.
  //
  //   length = max(thiz.length,
  //                thiz.length + (new.length - old.length) * count)
  //
  // Finally we use "C" functions strstr() and memcpy() to find and replace.

  ASSERT(count >= 0 || count == -1, OOPS);

  // Optimize case.
  if (thiz->length == 0 || old->length == 0 || count == 0)
    return thiz;
  if (IS_STR_EQ(old, new_))
    return thiz;

  int32_t max_count = thiz->length / old->length;
  count = (count == -1) ? max_count : _MIN(count, max_count);

  // Validate that the new string length will not overflow.
  int64_t diff = (int64_t) new_->length - (int64_t) old->length;
  uint64_t est_len = (uint64_t) thiz->length + diff * count;
  ASSERT(est_len <= UINT32_MAX,
         "String replacement results in length overflow.");
  uint32_t length = _MAX(thiz->length, (uint32_t) est_len);

  String* replaced = thiz; // Will be allocated if any match found.
  int32_t replacedc = 0;   // Replaced count so far.

  const char* s = thiz->data; // Source: current position in thiz.
  char* d = NULL;             // Destination pointer in replaced.

  do {
    if (replacedc == count)
      break;

    uint32_t remaining = thiz->length - (uint32_t) (s - thiz->data);
    const char* match = _memFind(s, remaining, old->data, old->length);
    if (match == NULL)
      break;

    // Note that since we're not allocating anything else here, this string
    // doesn't needs to pushed to VM's temp references.
    if (replacedc == 0) {
      replaced = newStringLength(vm, "", length);
      d = replaced->data;
    }

    // Copy everything from [s] till [match].
    memcpy(d, s, match - s);
    d += match - s;
    s = match;

    // Copy the replace string.
    memcpy(d, new_->data, new_->length);
    d += new_->length;
    s += old->length;
    replacedc++;

  } while (true);

  // Copy the rest of the string from [s] till the end.
  if (d != NULL) {
    uint32_t tail_length = thiz->length - (int32_t) (s - thiz->data);
    memcpy(d, s, tail_length);
    d += tail_length;

    // Update the string.
    replaced->length = (int32_t) (d - replaced->data);
    ASSERT(replaced->length < replaced->capacity, OOPS);
    replaced->data[replaced->length] = '\0';
    replaced->hash = utilHashString(replaced->data);

  } else {
    ASSERT(thiz == replaced, OOPS);
  }

  return replaced;
}

void* _memmem(const void* l, size_t l_len, const void* s, size_t s_len) {
  register char *cur, *last;
  const char* cl = (const char*) l;
  const char* cs = (const char*) s;

  /* we need something to compare */
  if (l_len == 0 || s_len == 0)
    return NULL;

  /* "s" must be smaller or equal to "l" */
  if (l_len < s_len)
    return NULL;

  /* special case where s_len == 1 */
  if (s_len == 1)
    return (void*) memchr(l, (int) *cs, l_len);

  /* the last position where its possible to find "s" in "l" */
  last = (char*) cl + l_len - s_len;

  for (cur = (char*) cl; cur <= last; cur++)
    if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
      return cur;

  return NULL;
}

List* stringSplit(VM* vm, String* thiz, String* sep) {
  List* list = newList(vm, 0);
  vmPushTempRef(vm, &list->_super); // list.

  if (sep == NULL || sep->length == 0) {
    for (uint32_t i = 0; i < thiz->length; i++) {
      String* ch = newStringLength(vm, &thiz->data[i], 1);
      vmPushTempRef(vm, &ch->_super); // ch
      listAppend(vm, list, VAR_OBJ(ch));
      vmPopTempRef(vm); // ch
    }
  } else {
    const char* s = thiz->data; // Current position in thiz.
    do {
      const char* match = (char*) _memmem(s, thiz->length - (s - thiz->data),
                                          sep->data, sep->length);

      if (match == NULL) {
        // Add the tail string from [s] till the end. Optimize case: if
        // the string doesn't have any match we can reuse thiz.
        if (s == thiz->data) {
          listAppend(vm, list, VAR_OBJ(thiz));

        } else {
          String* tail = newStringLength(vm, s, (uint32_t) (thiz->length - (s - thiz->data)));
          vmPushTempRef(vm, &tail->_super); // tail.
          listAppend(vm, list, VAR_OBJ(tail));
          vmPopTempRef(vm); // tail.
        }

        break; // We're done.
      }

      String* split = newStringLength(vm, s, (uint32_t) (match - s));
      vmPushTempRef(vm, &split->_super); // split.
      listAppend(vm, list, VAR_OBJ(split));
      vmPopTempRef(vm); // split.

      s = match + sep->length;

    } while (true);
  }

  vmPopTempRef(vm); // list.
  return list;
}

String* stringFormat(VM* vm, const char* fmt, ...) {
  va_list arg_list;

  // Calculate the total length of the resulting string. This is required to
  // determine the final string size to allocate.
  va_start(arg_list, fmt);
  size_t total_length = 0;
  for (const char* c = fmt; *c != '\0'; c++) {
    switch (*c) {
      case '$':
        total_length += strlen(va_arg(arg_list, const char*));
        break;

      case '@':
        total_length += va_arg(arg_list, String*)->length;
        break;

      default:
        total_length++;
    }
  }
  va_end(arg_list);

  // Now build the new string.
  String* result = _allocateString(vm, total_length);
  va_start(arg_list, fmt);
  char* buff = result->data;
  for (const char* c = fmt; *c != '\0'; c++) {
    switch (*c) {
      case '$':
        {
          const char* string = va_arg(arg_list, const char*);
          size_t length = strlen(string);
          memcpy(buff, string, length);
          buff += length;
        }
        break;

      case '@':
        {
          String* string = va_arg(arg_list, String*);
          memcpy(buff, string->data, string->length);
          buff += string->length;
        }
        break;

      default:
        {
          *buff++ = *c;
        }
        break;
    }
  }
  va_end(arg_list);

  result->hash = utilHashString(result->data);
  return result;
}

String* stringJoin(VM* vm, String* str1, String* str2) {
  // Optimize end case.
  if (str1->length == 0)
    return str2;
  if (str2->length == 0)
    return str1;

  size_t length = (size_t) str1->length + (size_t) str2->length;
  String* string = _allocateString(vm, length);

  memcpy(string->data, str1->data, str1->length);
  memcpy(string->data + str1->length, str2->data, str2->length);
  // Null byte already existed. From _allocateString.

  string->hash = utilHashString(string->data);
  return string;
}

String* replaceSubstring(VM* vm, uint32_t index, String* str, String* replace) {
  char* stringValue = str->data;
  strncpy(stringValue + index, replace->data, replace->length);
  return str;
}

void listInsert(VM* vm, List* thiz, uint32_t index, Var value) {
  // Add an empty slot at the end of the buffer.
  if (IS_OBJ(value))
    vmPushTempRef(vm, AS_OBJ(value));
  VarBufferWrite(&thiz->elements, vm, VAR_NULL);
  if (IS_OBJ(value))
    vmPopTempRef(vm);

  // Shift the existing elements down.
  for (uint32_t i = thiz->elements.count - 1; i > index; i--) {
    thiz->elements.data[i] = thiz->elements.data[i - 1];
  }

  // Insert the new element.
  thiz->elements.data[index] = value;
}

void listShrink(VM* vm, List* thiz) {
  if (thiz->elements.capacity / GROW_FACTOR >= thiz->elements.count) {
    thiz->elements.data = (Var*) vmRealloc(
        vm, thiz->elements.data, sizeof(Var) * thiz->elements.capacity,
        sizeof(Var) * thiz->elements.capacity / GROW_FACTOR);
    thiz->elements.capacity /= GROW_FACTOR;
  }
}

Var listRemoveAt(VM* vm, List* thiz, uint32_t index) {
  ASSERT_INDEX(index, thiz->elements.count);

  Var removed = thiz->elements.data[index];
  if (IS_OBJ(removed))
    vmPushTempRef(vm, AS_OBJ(removed));

  // Shift the rest of the elements up.
  for (uint32_t i = index; i < thiz->elements.count - 1; i++) {
    thiz->elements.data[i] = thiz->elements.data[i + 1];
  }

  listShrink(vm, thiz);

  if (IS_OBJ(removed))
    vmPopTempRef(vm);

  thiz->elements.count--;
  return removed;
}

void listClear(VM* vm, List* thiz) {
  VarBufferClear(&thiz->elements, vm);
}

List* listAdd(VM* vm, List* l1, List* l2) {
  // Optimize end case.
  if (l1->elements.count == 0)
    return l2;
  if (l2->elements.count == 0)
    return l1;

  uint32_t size = l1->elements.count + l2->elements.count;
  List* list = newList(vm, size);

  vmPushTempRef(vm, &list->_super); // list.
  VarBufferConcat(&list->elements, vm, &l1->elements);
  VarBufferConcat(&list->elements, vm, &l2->elements);
  vmPopTempRef(vm); // list.

  return list;
}

// Return a hash value for the object. Only String and Range objects can be
// hashable.
static uint32_t _hashObject(Object* obj) {
  ASSERT(isObjectHashable(obj->type),
         "Check if it's hashable before calling this method.");

  switch (obj->type) {
    case OBJ_STRING:
      return ((String*) obj)->hash;

    case OBJ_RANGE:
      {
        Range* range = (Range*) obj;
        return utilHashNumber(range->from) ^ utilHashNumber(range->to);
      }

    case OBJ_CLASS:
      {
        return utilHashBits((uint64_t) obj);
      }

    default:
      break;
  }

  UNREACHABLE();
  return 0;
}

uint32_t varHashValue(Var v) {
  if (IS_OBJ(v))
    return _hashObject(AS_OBJ(v));

#if VAR_NAN_TAGGING
  return utilHashBits(v);
#else
  switch (v.type) {
    case VAL_NULL:
      return 0;
    case VAL_UNDEF:
      return 1;
    case VAL_VOID:
      return 2;
    case VAL_TRUE:
      return 3;
    case VAL_FALSE:
      return 4;
    case VAL_INT:
      return utilHashBits((uint64_t) v._int);
    case VAL_NUMBER:
      return utilHashBits(utilDoubleToBits(v._number));
    default:
      return 0;
  }
#endif
}

// Find the entry with the [key]. Returns true if found and set [result] to
// point to the entry, return false otherwise and points [result] to where
// the entry should be inserted.
static bool _mapFindEntry(Map* thiz, Var key, MapEntry** result) {
  // An empty map won't contain the key.
  if (thiz->capacity == 0)
    return false;

  // The [start_index] is where the entry supposed to be if there wasn't any
  // collision occurred. It'll be the start index for the linear probing.
  uint32_t start_index = varHashValue(key) % thiz->capacity;
  uint32_t index = start_index;

  // Keep track of the first tombstone after the [start_index] if we don't
  // find the key anywhere. The tombstone would be the entry at where we will
  // have to insert the key/value pair.
  MapEntry* tombstone = NULL;

  do {
    MapEntry* entry = &thiz->entries[index];

    if (IS_UNDEF(entry->key)) {
      ASSERT(IS_BOOL(entry->value), OOPS);

      if (IS_TRUE(entry->value)) {
        // We've found a tombstone, if we haven't found one [tombstone] should
        // be updated. We still need to keep search for if the key exists.
        if (tombstone == NULL)
          tombstone = entry;

      } else {
        // We've found a new empty slot and the key isn't found. If
        // we've found a tombstone along the sequence we could use that
        // entry otherwise the entry at the current index.

        *result = (tombstone != NULL) ? tombstone : entry;
        return false;
      }

    } else if (isValuesEqual(entry->key, key)) {
      // We've found the key.
      *result = entry;
      return true;
    }

    index = (index + 1) % thiz->capacity;

  } while (index != start_index);

  // If we reach here means the map is filled with tombstone. Set the first
  // tombstone as result for the next insertion and return false.
  ASSERT(tombstone != NULL, OOPS);
  *result = tombstone;
  return false;
}

// Add the key, value pair to the entries array of the map. Returns true if
// the entry added for the first time and false for replaced value.
static bool _mapInsertEntry(Map* thiz, Var key, Var value) {
  ASSERT(thiz->capacity != 0, "Should ensure the capacity before inserting.");

  MapEntry* result;
  if (_mapFindEntry(thiz, key, &result)) {
    // Key already found, just replace the value.
    result->value = value;
    return false;
  } else {
    result->key = key;
    result->value = value;
    return true;
  }
}

// Resize the map's size to the given [capacity].
static void _mapResize(VM* vm, Map* thiz, uint32_t capacity) {
  MapEntry* old_entries = thiz->entries;
  uint32_t old_capacity = thiz->capacity;

  thiz->entries = ALLOCATE_ARRAY(vm, MapEntry, capacity);
  thiz->capacity = capacity;
  for (uint32_t i = 0; i < capacity; i++) {
    thiz->entries[i].key = VAR_UNDEFINED;
    thiz->entries[i].value = VAR_FALSE;
  }

  // Insert the old entries to the new entries.
  for (uint32_t i = 0; i < old_capacity; i++) {
    // Skip the empty entries or tombstones.
    if (IS_UNDEF(old_entries[i].key))
      continue;

    _mapInsertEntry(thiz, old_entries[i].key, old_entries[i].value);
  }

  DEALLOCATE_ARRAY(vm, old_entries, MapEntry, old_capacity);
}

Var mapGet(Map* thiz, Var key) {
  MapEntry* entry;
  if (_mapFindEntry(thiz, key, &entry))
    return entry->value;
  return VAR_UNDEFINED;
}

void mapSet(VM* vm, Map* thiz, Var key, Var value) {
  // If map is about to fill, resize it first.
  if (thiz->count + 1 > thiz->capacity * MAP_LOAD_PERCENT / 100) {
    uint32_t capacity = thiz->capacity * GROW_FACTOR;
    if (capacity < MIN_CAPACITY)
      capacity = MIN_CAPACITY;
    _mapResize(vm, thiz, capacity);
  }

  if (_mapInsertEntry(thiz, key, value)) {
    thiz->count++; //< A new key added.
  }
}

void mapClear(VM* vm, Map* thiz) {
  DEALLOCATE_ARRAY(vm, thiz->entries, MapEntry, thiz->capacity);
  thiz->entries = NULL;
  thiz->capacity = 0;
  thiz->count = 0;
}

Var mapRemoveKey(VM* vm, Map* thiz, Var key) {
  MapEntry* entry;
  if (!_mapFindEntry(thiz, key, &entry))
    return VAR_UNDEFINED;

  // Set the key as VAR_UNDEFINED to mark is as an available slow and set it's
  // value to VAR_TRUE for tombstone.
  Var value = entry->value;
  entry->key = VAR_UNDEFINED;
  entry->value = VAR_TRUE;

  thiz->count--;

  if (IS_OBJ(value))
    vmPushTempRef(vm, AS_OBJ(value));

  if (thiz->count == 0) {
    // Clear the map if it's empty.
    mapClear(vm, thiz);

  } else if ((thiz->capacity > MIN_CAPACITY)
             && (thiz->capacity / (GROW_FACTOR * GROW_FACTOR))
                    > ((thiz->count * 100) / MAP_LOAD_PERCENT)) {
    // We grow the map when it's filled 75% (MAP_LOAD_PERCENT) by 2
    // (GROW_FACTOR) but we're not shrink the map when it's half filled (ie.
    // half of the capacity is 75%). Instead we wait till it'll become 1/4 is
    // filled (1/4 = 1/(GROW_FACTOR*GROW_FACTOR)) to minimize the
    // reallocations and which is more faster.

    uint32_t capacity = thiz->capacity / (GROW_FACTOR * GROW_FACTOR);
    if (capacity < MIN_CAPACITY)
      capacity = MIN_CAPACITY;

    _mapResize(vm, thiz, capacity);
  }

  if (IS_OBJ(value))
    vmPopTempRef(vm);

  return value;
}

bool fiberHasError(Fiber* fiber) {
  return fiber->error != NULL;
}

void freeObject(VM* vm, Object* thiz) {
  // TODO: Debug trace memory here.

  // First clean the object's references, but we're not recursively
  // deallocating them because they're not marked and will be cleaned later.
  // Example: List's `elements` is VarBuffer that contain a heap allocated
  // array of `var*` which will be cleaned below but the actual `var` elements
  // will won't be freed here instead they haven't marked at all, and will be
  // removed at the sweeping phase of the garbage collection.
  switch (thiz->type) {
    case OBJ_STRING:
      {
        String* str = (String*) thiz;
        DEALLOCATE_DYNAMIC(vm, str, String, str->capacity, char);
        return;
      };

    case OBJ_LIST:
      {
        VarBufferClear(&(((List*) thiz)->elements), vm);
        DEALLOCATE(vm, thiz, List);
        return;
      }

    case OBJ_MAP:
      {
        Map* map = (Map*) thiz;
        DEALLOCATE_ARRAY(vm, map->entries, MapEntry, map->capacity);
        DEALLOCATE(vm, thiz, Map);
        return;
      }

    case OBJ_RANGE:
      {
        DEALLOCATE(vm, thiz, Range);
        return;
      }

    case OBJ_MODULE:
      {
        Module* module = (Module*) thiz;
        VarBufferClear(&module->globals, vm);
        UintBufferClear(&module->global_names, vm);
        VarBufferClear(&module->constants, vm);
#ifndef NO_DL
        if (module->handle)
          vmUnloadDlHandle(vm, module->handle);
#endif
        DEALLOCATE(vm, thiz, Module);
        return;
      }

    case OBJ_FUNC:
      {
        Function* func = (Function*) thiz;
        if (!func->is_native) {
          ByteBufferClear(&func->fn->opcodes, vm);
          UintBufferClear(&func->fn->oplines, vm);
          DEALLOCATE(vm, func->fn, Fn);
        }
        DEALLOCATE(vm, thiz, Function);
        return;
      };

    case OBJ_CLOSURE:
      {
        DEALLOCATE_DYNAMIC(vm, thiz, Closure, ((Closure*) thiz)->fn->upvalue_count, Upvalue*);
        return;
      }

    case OBJ_METHOD_BIND:
      {
        DEALLOCATE(vm, thiz, MethodBind);
        return;
      }

    case OBJ_UPVALUE:
      {
        DEALLOCATE(vm, thiz, Upvalue);
        return;
      }

    case OBJ_FIBER:
      {
        Fiber* fiber = (Fiber*) thiz;
        DEALLOCATE_ARRAY(vm, fiber->stack, Var, fiber->stack_size);
        DEALLOCATE_ARRAY(vm, fiber->frames, CallFrame, fiber->frame_capacity);
        DEALLOCATE(vm, fiber, Fiber);
        return;
      }

    case OBJ_CLASS:
      {
        Class* cls = (Class*) thiz;
        ClosureBufferClear(&cls->methods, vm);
        DEALLOCATE(vm, cls, Class);
        return;
      }

    case OBJ_POINTER:
      {
        Pointer* pointer = (Pointer*) thiz;
        if (pointer->destructor && pointer->native_ptr) {
          pointer->destructor(pointer->native_ptr);
        }
        DEALLOCATE(vm, pointer, Pointer);
        return;
      }

    case OBJ_INST:
      {
        Instance* inst = (Instance*) thiz;
        Class* cls = inst->cls;
        while (cls != NULL) {
          if (cls->delete_fn != NULL) {
            cls->delete_fn(vm, inst->native);
            break;
          }
          cls = cls->super_class;
        }

        DEALLOCATE(vm, inst, Instance);
        return;
      }
  }

  UNREACHABLE();
}

uint32_t moduleAddConstant(VM* vm, Module* module, Var value) {
  for (uint32_t i = 0; i < module->constants.count; i++) {
    if (isValuesSame(module->constants.data[i], value)) {
      return i;
    }
  }
  VarBufferWrite(&module->constants, vm, value);
  return (int) module->constants.count - 1;
}

String* moduleAddString(Module* module, VM* vm, const char* name,
                        uint32_t length, int* index) {
  for (uint32_t i = 0; i < module->constants.count; i++) {
    if (!IS_OBJ_TYPE(module->constants.data[i], OBJ_STRING))
      continue;
    String* _name = (String*) AS_OBJ(module->constants.data[i]);
    if (_name->length == length && strncmp(_name->data, name, length) == 0) {
      // Name already exists in the buffer.
      if (index)
        *index = i;
      return _name;
    }
  }

  // If we reach here the name doesn't exists in the buffer, so add it and
  // return the index.
  String* new_name = newStringLength(vm, name, length);
  vmPushTempRef(vm, &new_name->_super); // new_name
  VarBufferWrite(&module->constants, vm, VAR_OBJ(new_name));
  vmPopTempRef(vm); // new_name
  if (index)
    *index = module->constants.count - 1;
  return new_name;
}

String* moduleGetStringAt(Module* module, int index) {
  ASSERT(index >= 0, OOPS);
  if (index >= (int) module->constants.count)
    return NULL;
  Var constant = module->constants.data[index];
  if (IS_OBJ_TYPE(constant, OBJ_STRING)) {
    return (String*) AS_OBJ(constant);
  }
  return NULL;
}

uint32_t moduleSetGlobal(VM* vm, Module* module, const char* name, uint32_t length, Var value) {
  ASSERT(name != NULL, OOPS);
  ASSERT(!IS_UNDEF(value), OOPS);

  if (IS_OBJ(value))
    vmPushTempRef(vm, AS_OBJ(value));

  // If already exists update the value.
  int g_index = moduleGetGlobalIndex(module, name, length);
  if (g_index != -1) {
    ASSERT(g_index < (int) module->globals.count, OOPS);
    module->globals.data[g_index] = value;
    if (IS_OBJ(value))
      vmPopTempRef(vm);
    return g_index;
  }

  // If we're reached here that means we don't already have a variable with
  // that name, create new one and set the value.
  int name_index = 0;
  moduleAddString(module, vm, name, length, &name_index);
  UintBufferWrite(&module->global_names, vm, name_index);
  VarBufferWrite(&module->globals, vm, value);

  if (IS_OBJ(value))
    vmPopTempRef(vm);
  return module->globals.count - 1;
}

int moduleGetGlobalIndex(Module* module, const char* name, uint32_t length) {
  for (uint32_t i = 0; i < module->global_names.count; i++) {
    uint32_t name_index = module->global_names.data[i];
    String* g_name = moduleGetStringAt(module, name_index);
    ASSERT(g_name != NULL, OOPS);
    if (g_name->length == length && strncmp(g_name->data, name, length) == 0) {
      return (int) i;
    }
  }
  return -1;
}

void moduleAddMain(VM* vm, Module* module) {
  ASSERT(module->body == NULL, OOPS);

  module->initialized = false;

  const char* fn_name = IMPLICIT_MAIN_NAME;
  Function* body_fn = newFunction(vm, fn_name, (int) strlen(fn_name), module,
                                  false, NULL, NULL);
  body_fn->arity = 0;

  vmPushTempRef(vm, &body_fn->_super); // body_fn.
  module->body = newClosure(vm, body_fn);
  vmPopTempRef(vm); // body_fn.

  moduleSetGlobal(vm, module, IMPLICIT_MAIN_NAME,
                  (uint32_t) strlen(IMPLICIT_MAIN_NAME), VAR_OBJ(module->body));
}

/*****************************************************************************/
/* UTILITY FUNCTIONS                                                         */
/*****************************************************************************/

VarType getObjVarType(ObjectType type) {
  switch (type) {
    case OBJ_STRING:
      return vSTRING;
    case OBJ_LIST:
      return vLIST;
    case OBJ_MAP:
      return vMAP;
    case OBJ_RANGE:
      return vRANGE;
    case OBJ_MODULE:
      return vMODULE;
    case OBJ_FUNC:
      UNREACHABLE();
    case OBJ_CLOSURE:
      return vCLOSURE;
    case OBJ_METHOD_BIND:
      return vMETHOD_BIND;
    case OBJ_UPVALUE:
      UNREACHABLE();
    case OBJ_FIBER:
      return vFIBER;
    case OBJ_CLASS:
      return vCLASS;
    case OBJ_POINTER:
      return vPOINTER;
    case OBJ_INST:
      return vINSTANCE;
  }

  UNREACHABLE();
  return (VarType) -1;
}

ObjectType getVarObjType(VarType type) {
  switch (type) {
    case vOBJECT:
    case vNULL:
    case vBOOL:
    case vNUMBER:
      UNREACHABLE();

    case vSTRING:
      return OBJ_STRING;
    case vLIST:
      return OBJ_LIST;
    case vMAP:
      return OBJ_MAP;
    case vRANGE:
      return OBJ_RANGE;
    case vMODULE:
      return OBJ_MODULE;
    case vCLOSURE:
      return OBJ_CLOSURE;
    case vMETHOD_BIND:
      return OBJ_METHOD_BIND;
    case vFIBER:
      return OBJ_FIBER;
    case vCLASS:
      return OBJ_CLASS;
    case vPOINTER:
      return OBJ_POINTER;
    case vINSTANCE:
      return OBJ_INST;
  }

  UNREACHABLE();
  return (ObjectType) -1;
}

const char* getVarTypeName(VarType type) {
  switch (type) {
    case vOBJECT:
      return "Object";
    case vNULL:
      return "Null";
    case vBOOL:
      return "Bool";
    case vNUMBER:
      return "Number";
    default:
      return getObjectTypeName(getVarObjType(type));
  }

  UNREACHABLE();
  return NULL;
}

const char* getObjectTypeName(ObjectType type) {
  switch (type) {
    case OBJ_STRING:
      return "String";
    case OBJ_LIST:
      return "List";
    case OBJ_MAP:
      return "Map";
    case OBJ_RANGE:
      return "Range";
    case OBJ_MODULE:
      return "Module";
    case OBJ_FUNC:
      return "Func";
    case OBJ_CLOSURE:
      return "Closure";
    case OBJ_METHOD_BIND:
      return "MethodBind";
    case OBJ_UPVALUE:
      return "Upvalue";
    case OBJ_FIBER:
      return "Fiber";
    case OBJ_CLASS:
      return "Class";
    case OBJ_POINTER:
      return "Pointer";
    case OBJ_INST:
      return "Inst";
  }
  UNREACHABLE();
  return NULL;
}

const char* varTypeName(Var v) {
  if (IS_NULL(v))
    return "Null";
  if (IS_BOOL(v))
    return "Bool";
  if (IS_NUM(v))
    return "Number";
  if (IS_UNDEF(v))
    return "Undefined";

  ASSERT(IS_OBJ(v), OOPS);
  Object* obj = AS_OBJ(v);

  if (obj->type == OBJ_INST) {
    return ((Instance*) obj)->cls->name->data;
  }

  return getObjectTypeName(obj->type);
}

VarType getVarType(Var v) {
  if (IS_NULL(v))
    return vNULL;
  if (IS_BOOL(v))
    return vBOOL;
  if (IS_NUM(v))
    return vNUMBER;

  ASSERT(IS_OBJ(v), OOPS);
  Object* obj = AS_OBJ(v);
  return getObjVarType(obj->type);
}

bool isValuesSame(Var v1, Var v2) {
#if VAR_NAN_TAGGING
  // Bit representation of each values are unique so just compare the bits.
  return v1 == v2;
#else
  if (v1.type != v2.type)
    return false;
  switch (v1.type) {
    case VAL_NUMBER:
      return utilDoubleToBits(v1._number) == utilDoubleToBits(v2._number);
    case VAL_INT:
      return v1._int == v2._int;
    case VAL_OBJECT:
      return v1._obj == v2._obj;
    default:
      return true;
  }
#endif
}

bool isValuesEqual(Var v1, Var v2) {
  if (isValuesSame(v1, v2))
    return true;

  // +0 and -0 have different bit value representations.
  if (IS_NUM(v1) && IS_NUM(v2)) {
    return AS_NUM(v1) == AS_NUM(v2);
  }

  // If we reach here only heap allocated objects could be compared.
  if (!IS_OBJ(v1) || !IS_OBJ(v2))
    return false;

  Object *o1 = AS_OBJ(v1), *o2 = AS_OBJ(v2);
  if (o1->type != o2->type)
    return false;

  switch (o1->type) {
    case OBJ_RANGE:
      return ((Range*) o1)->from == ((Range*) o2)->from
             && ((Range*) o1)->to == ((Range*) o2)->to;

    case OBJ_STRING:
      {
        String *s1 = (String*) o1, *s2 = (String*) o2;
        return s1->hash == s2->hash && s1->length == s2->length
               && memcmp(s1->data, s2->data, s1->length) == 0;
      }

    case OBJ_LIST:
      {
        /*
         * l1 = []; list_append(l1, l1) # [[...]]
         * l2 = []; list_append(l2, l2) # [[...]]
         * l1 == l2 ## This will cause a stack overflow but not handling that
         *          ## (in python too).
         */
        List *l1 = (List*) o1, *l2 = (List*) o2;
        if (l1->elements.count != l2->elements.count)
          return false;
        Var* _v1 = l1->elements.data;
        Var* _v2 = l2->elements.data;
        for (uint32_t i = 0; i < l1->elements.count; i++) {
          if (!isValuesEqual(*_v1, *_v2))
            return false;
          _v1++, _v2++;
        }
        return true;
      }

    case OBJ_MAP:
      {
        Map *m1 = (Map*) o1, *m2 = (Map*) o2;

        MapEntry* e = m1->entries;
        for (; e < m1->entries + m1->capacity; e++) {
          if (IS_UNDEF(e->key))
            continue;
          Var v = mapGet(m2, e->key);
          if (IS_UNDEF(v))
            return false;
          if (!isValuesEqual(e->value, v))
            return false;
        }
        return true;
      }

    default:
      return false;
  }
}

bool isObjectHashable(ObjectType type) {
  // Only String and Range are hashable (since they're immutable).
  return type == OBJ_STRING || type == OBJ_RANGE || type == OBJ_CLASS;
}

// This will prevent recursive list/map from crash when calling to_string, by
// checking if the current sequence is in the outer sequence linked list.
struct OuterSequence {
  struct OuterSequence* outer;
  // If false it'll be map. If we have multiple sequence we should use an enum
  // here but we only ever support list and map as builtin sequence (so bool).
  bool is_list;
  union {
    const List* list;
    const Map* map;
  };
};
typedef struct OuterSequence OuterSequence;

static void _toStringInternal(VM* vm, const Var v, ByteBuffer* buff,
                              OuterSequence* outer, bool repr) {
  ASSERT(outer == NULL || repr, OOPS);

  if (IS_NULL(v)) {
    ByteBufferAddString(buff, vm, "null", 4);
    return;

  } else if (IS_BOOL(v)) {
    if (AS_BOOL(v))
      ByteBufferAddString(buff, vm, "true", 4);
    else
      ByteBufferAddString(buff, vm, "false", 5);
    return;

  } else if (IS_NUM(v)) {
    double value = AS_NUM(v);

    if (isnan(value)) {
      ByteBufferAddString(buff, vm, "nan", 3);

    } else if (isinf(value)) {
      if (value > 0.0) {
        ByteBufferAddString(buff, vm, "+inf", 4);
      } else {
        ByteBufferAddString(buff, vm, "-inf", 4);
      }

    } else {
      char num_buff[STR_DBL_BUFF_SIZE];
      int length = sprintf(num_buff, DOUBLE_FMT, AS_NUM(v));
      ByteBufferAddString(buff, vm, num_buff, length);
    }

    return;

  } else if (IS_OBJ(v)) {
    const Object* obj = AS_OBJ(v);

    switch (obj->type) {
      case OBJ_STRING:
        {
          const String* str = (const String*) obj;
          if (outer == NULL && !repr) {
            ByteBufferAddString(buff, vm, str->data, str->length);
            return;
          } else {
            // If recursive return with quotes (ex: [42, "hello", 0..10]).
            ByteBufferWrite(buff, vm, '"');
            for (uint32_t i = 0; i < str->length; i++) {
              char c = str->data[i];
              switch (c) {
                case '"':
                  ByteBufferAddString(buff, vm, "\\\"", 2);
                  break;
                case '\\':
                  ByteBufferAddString(buff, vm, "\\\\", 2);
                  break;
                case '\n':
                  ByteBufferAddString(buff, vm, "\\n", 2);
                  break;
                case '\r':
                  ByteBufferAddString(buff, vm, "\\r", 2);
                  break;
                case '\t':
                  ByteBufferAddString(buff, vm, "\\t", 2);
                  break;

                default:
                  {
                    // if c isn't in range 0x00 to 0xff, isprintc()
                    // fail an assertion.
                    if (isprint(c)) {
                      ByteBufferWrite(buff, vm, c);
                    } else {
                      ByteBufferAddString(buff, vm, "\\x", 2);
                      uint8_t byte = (uint8_t) c;
                      ByteBufferWrite(buff, vm, utilHexDigit(((byte >> 4) & 0xf), false));
                      ByteBufferWrite(buff, vm, utilHexDigit(((byte >> 0) & 0xf), false));
                    }
                  }
                  break;
              }
            }
            ByteBufferWrite(buff, vm, '"');
            return;
          }
          UNREACHABLE();
        }

      case OBJ_LIST:
        {
          const List* list = (const List*) obj;
          if (list->elements.count == 0) {
            ByteBufferAddString(buff, vm, "[]", 2);
            return;
          }

          // Check if the list is recursive.
          OuterSequence* seq = outer;
          while (seq != NULL) {
            if (seq->is_list && seq->list == list) {
              ByteBufferAddString(buff, vm, "[...]", 5);
              return;
            }
            seq = seq->outer;
          }
          OuterSequence seq_list;
          seq_list.outer = outer;
          seq_list.is_list = true;
          seq_list.list = list;

          ByteBufferWrite(buff, vm, '[');
          for (uint32_t i = 0; i < list->elements.count; i++) {
            if (i != 0)
              ByteBufferAddString(buff, vm, ", ", 2);
            _toStringInternal(vm, list->elements.data[i], buff, &seq_list, true);
          }
          ByteBufferWrite(buff, vm, ']');
          return;
        }

      case OBJ_MAP:
        {
          const Map* map = (const Map*) obj;
          if (map->entries == NULL) {
            ByteBufferAddString(buff, vm, "{}", 2);
            return;
          }

          // Check if the map is recursive.
          OuterSequence* seq = outer;
          while (seq != NULL) {
            if (!seq->is_list && seq->map == map) {
              ByteBufferAddString(buff, vm, "{...}", 5);
              return;
            }
            seq = seq->outer;
          }
          OuterSequence seq_map;
          seq_map.outer = outer;
          seq_map.is_list = false;
          seq_map.map = map;

          ByteBufferWrite(buff, vm, '{');
          uint32_t i = 0;     // Index of the current entry to iterate.
          bool _first = true; // For first element no ',' required.
          do {
            // Get the next valid key index.
            bool _done = false;
            while (IS_UNDEF(map->entries[i].key)) {
              if (++i >= map->capacity) {
                _done = true;
                break;
              }
            }
            if (_done)
              break;

            if (!_first)
              ByteBufferAddString(buff, vm, ", ", 2);

            _toStringInternal(vm, map->entries[i].key, buff, &seq_map, true);
            ByteBufferWrite(buff, vm, ':');
            _toStringInternal(vm, map->entries[i].value, buff, &seq_map, true);

            i++;
            _first = false;
          } while (i < map->capacity);

          ByteBufferWrite(buff, vm, '}');
          return;
        }

      case OBJ_RANGE:
        {
          const Range* range = (const Range*) obj;

          char buff_from[STR_DBL_BUFF_SIZE];
          const int len_from = snprintf(buff_from, sizeof(buff_from),
                                        DOUBLE_FMT, range->from);
          char buff_to[STR_DBL_BUFF_SIZE];
          const int len_to = snprintf(buff_to, sizeof(buff_to), DOUBLE_FMT,
                                      range->to);

          ByteBufferAddString(buff, vm, "[Range:", 7);
          ByteBufferAddString(buff, vm, buff_from, len_from);
          ByteBufferAddString(buff, vm, "..", 2);
          ByteBufferAddString(buff, vm, buff_to, len_to);
          ByteBufferWrite(buff, vm, ']');
          return;
        }

      case OBJ_MODULE:
        {
          const Module* module = (const Module*) obj;
          ByteBufferAddString(buff, vm, "[Module:", 8);
          if (module->name != NULL) {
            ByteBufferAddString(buff, vm, module->name->data, module->name->length);
          } else {
            ByteBufferWrite(buff, vm, '"');
            ByteBufferAddString(buff, vm, module->path->data, module->path->length);
            ByteBufferWrite(buff, vm, '"');
          }
          /*
          ByteBufferAddString(buff, vm, " ", 1);

          char buff_addr[STR_HEX_BUFF_SIZE];
          char* ptr = (char*)buff_addr;
          (*ptr++) = '0'; (*ptr++) = 'x';
          const int len = snprintf(ptr, sizeof(buff_addr) - 2,
            "%08x", (unsigned int)(uintptr_t)module);
          ByteBufferAddString(buff, vm, buff_addr, (uint32_t)len);
          */
          ByteBufferWrite(buff, vm, ']');
          return;
        }

      case OBJ_FUNC:
        {
          const Function* fn = (const Function*) obj;
          ByteBufferAddString(buff, vm, "[Func:", 6);
          ByteBufferAddString(buff, vm, fn->name, (uint32_t) strlen(fn->name));
          /*
          ByteBufferAddString(buff, vm, " ", 1);

          char buff_addr[STR_HEX_BUFF_SIZE];
          char* ptr = (char*)buff_addr;
          (*ptr++) = '0'; (*ptr++) = 'x';
          const int len = snprintf(ptr, sizeof(buff_addr) - 2,
            "%08x", (unsigned int)(uintptr_t)fn);
          ByteBufferAddString(buff, vm, buff_addr, (uint32_t)len);
          */
          ByteBufferWrite(buff, vm, ']');
          return;
        }

      case OBJ_CLOSURE:
        {
          const Closure* closure = (const Closure*) obj;
          ByteBufferAddString(buff, vm, "[Closure:", 9);
          ByteBufferAddString(buff, vm, closure->fn->name,
                              (uint32_t) strlen(closure->fn->name));
          /*
          ByteBufferAddString(buff, vm, " ", 1);

          char buff_addr[STR_HEX_BUFF_SIZE];
          char* ptr = (char*)buff_addr;
          (*ptr++) = '0'; (*ptr++) = 'x';
          const int len = snprintf(ptr, sizeof(buff_addr) - 2,
            "%08x", (unsigned int)(uintptr_t)closure);
          ByteBufferAddString(buff, vm, buff_addr, (uint32_t)len);
          */
          ByteBufferWrite(buff, vm, ']');
          return;
        }
      case OBJ_METHOD_BIND:
        {
          const MethodBind* mb = (const MethodBind*) obj;
          ByteBufferAddString(buff, vm, "[MethodBind:", 12);
          ByteBufferAddString(buff, vm, mb->method->fn->name,
                              (uint32_t) strlen(mb->method->fn->name));
          /*
          ByteBufferAddString(buff, vm, " ", 1);

          char buff_addr[STR_HEX_BUFF_SIZE];
          char* ptr = (char*)buff_addr;
          (*ptr++) = '0'; (*ptr++) = 'x';
          const int len = snprintf(ptr, sizeof(buff_addr) - 2,
            "%08x", (unsigned int)(uintptr_t)mb);
          ByteBufferAddString(buff, vm, buff_addr, (uint32_t)len);
          */
          ByteBufferWrite(buff, vm, ']');
          return;
        }

      case OBJ_FIBER:
        {
          const Fiber* fb = (const Fiber*) obj;
          ByteBufferAddString(buff, vm, "[Fiber:", 7);
          ByteBufferAddString(buff, vm, fb->closure->fn->name,
                              (uint32_t) strlen(fb->closure->fn->name));
          ByteBufferWrite(buff, vm, ']');
          return;
        }

      case OBJ_UPVALUE:
        {
          ByteBufferAddString(buff, vm, "[Upvalue]", 9);
          return;
        }

      case OBJ_CLASS:
        {
          const Class* cls = (const Class*) obj;
          ByteBufferAddString(buff, vm, "[Class:", 7);
          ByteBufferAddString(buff, vm, cls->name->data, cls->name->length);
          /*
          ByteBufferAddString(buff, vm, " ", 1);

          char buff_addr[STR_HEX_BUFF_SIZE];
          char* ptr = (char*)buff_addr;
          (*ptr++) = '0'; (*ptr++) = 'x';
          const int len = snprintf(ptr, sizeof(buff_addr) - 2,
            "%08x", (unsigned int)(uintptr_t)cls);
          ByteBufferAddString(buff, vm, buff_addr, (uint32_t)len);
          */
          ByteBufferWrite(buff, vm, ']');
          return;
        }

      case OBJ_POINTER:
        {
          const Pointer* ptr = (const Pointer*) obj;

          ByteBufferAddString(buff, vm, "[Pointer:", 9);
          char addr_buff[STR_HEX_BUFF_SIZE];
          char* addr_ptr = addr_buff;
          (*addr_ptr++) = '0';
          (*addr_ptr++) = 'x';
          int addr_len = snprintf(addr_ptr, sizeof(addr_buff) - 2, "%08x",
                                  (unsigned int) (uintptr_t) ptr->native_ptr);
          ByteBufferAddString(buff, vm, addr_buff, (uint32_t) addr_len);
          ByteBufferWrite(buff, vm, ']');
          return;
        }

      case OBJ_INST:
        {
          const Instance* inst = (const Instance*) obj;
          ByteBufferWrite(buff, vm, '[');
          ByteBufferWrite(buff, vm, '\'');
          ByteBufferAddString(buff, vm, inst->cls->name->data, inst->cls->name->length);
          ByteBufferAddString(buff, vm, "' instance at ", 14);

          char buff_addr[STR_HEX_BUFF_SIZE];
          char* ptr = (char*) buff_addr;
          (*ptr++) = '0';
          (*ptr++) = 'x';
          const int len = snprintf(ptr, sizeof(buff_addr) - 2, "%08x",
                                   (unsigned int) (uintptr_t) inst);
          ByteBufferAddString(buff, vm, buff_addr, (uint32_t) len);
          ByteBufferWrite(buff, vm, ']');
          return;
        }
    }
  }
  UNREACHABLE();
  return;
}

String* toString(VM* vm, const Var value) {
  // If it's already a string don't allocate a new string.
  if (IS_OBJ_TYPE(value, OBJ_STRING)) {
    return (String*) AS_OBJ(value);
  }

  ByteBuffer buff;
  ByteBufferInit(&buff);
  _toStringInternal(vm, value, &buff, NULL, false);
  String* ret = newStringLength(vm, (const char*) buff.data, buff.count);
  ByteBufferClear(&buff, vm);
  return ret;
}

String* toRepr(VM* vm, const Var value) {
  ByteBuffer buff;
  ByteBufferInit(&buff);
  _toStringInternal(vm, value, &buff, NULL, true);
  String* ret = newStringLength(vm, (const char*) buff.data, buff.count);
  ByteBufferClear(&buff, vm);
  return ret;
}

bool toBool(Var v) {
  if (IS_BOOL(v))
    return AS_BOOL(v);
  if (IS_NULL(v))
    return false;
  if (IS_NUM(v))
    return AS_NUM(v) != 0;

  ASSERT(IS_OBJ(v), OOPS);
  Object* o = AS_OBJ(v);
  switch (o->type) {
    case OBJ_STRING:
      return ((String*) o)->length != 0;
    case OBJ_LIST:
      return ((List*) o)->elements.count != 0;
    case OBJ_MAP:
      return ((Map*) o)->count != 0;
    case OBJ_RANGE: // [[FALLTHROUGH]]
    case OBJ_MODULE:
    case OBJ_FUNC:
    case OBJ_CLOSURE:
    case OBJ_METHOD_BIND:
    case OBJ_UPVALUE:
    case OBJ_FIBER:
    case OBJ_CLASS:
    case OBJ_POINTER:
    case OBJ_INST:
      return true;
  }

  UNREACHABLE();
  return false;
}

// Undefining for amalgamation to let othre libraries also define _MIN, _MAX.
#undef _MAX
#undef _MIN