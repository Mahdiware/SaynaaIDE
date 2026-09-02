/*
 * Copyright (c) 2022-2026 Mohamed Abdifatah. All rights reserved.
 * Distributed Under The MIT License
 */

#include "saynaa_core.h"

#include "../builtins/saynaa_built_functions.h"
#include "../shared/saynaa_bytecode.h"
#include "../shared/saynaa_validate.h"
#include "../utils/saynaa_debug.h"
#include "../utils/saynaa_utils.h"
#include "saynaa_vm.h"

#include <limits.h>
#include <math.h>

// A convenient macro to get the nth (1 based) argument of the current
// function.
#define ARG(n) (vm->fiber->ret[n])

// Evaluates to the current function's argument count.
#define ARGC ((int) (vm->fiber->sp - vm->fiber->ret) - 1)

// Set return value for the current native function and return.
#define RET(value) \
  do { \
    *(vm->fiber->ret) = value; \
    return; \
  } while (false)

#define RET_ERR(err) \
  do { \
    VM_SET_ERROR(vm, err); \
    RET(VAR_NULL); \
  } while (false)

static Var _instanceRemoveAttribFast(VM* vm, Instance* inst, String* attrib);

static inline Closure* _resolveMagicMethod(Class* cls, MagicMethod method) {
  Closure* closure = cls->magic_methods[method];
  if (closure == (Closure*) -1)
    closure = getMagicMethod(cls, method);
  return closure;
}

/*****************************************************************************/
/* SHARED FUNCTIONS                                                          */
/*****************************************************************************/

static void initializePrimitiveClasses(VM* vm);

void initializeCore(VM* vm) {
  register_builtins(vm);
}

void initializeModule(VM* vm, Module* module, bool is_main) {
  String *path = module->path, *name = NULL;

  if (is_main) {
    // TODO: consider static string "@main" stored in VM. to reduce
    // allocations everytime here.
    name = newString(vm, "@main");
    module->name = name;
    vmPushTempRef(vm, &name->_super); // @main.
  } else {
    ASSERT(module->name != NULL, OOPS);
    name = module->name;
  }

  ASSERT(name != NULL, OOPS);

  // A script's path will always the absolute normalized path (the path
  // resolving function would do take care of it) which is something that
  // was added after python 3.9.
  if (path != NULL) {
    moduleSetGlobal(vm, module, "__file__", 8, VAR_OBJ(path));
  }

  moduleSetGlobal(vm, module, "_name", 5, VAR_OBJ(name));
  if (is_main)
    vmPopTempRef(vm); // _main.
  moduleSetGlobal(vm, module, "_module", 7, VAR_OBJ(module));
}

/*****************************************************************************/
/* INTERNAL FUNCTIONS                                                        */
/*****************************************************************************/

String* varToString(VM* vm, Var thiz, bool repr) {
  if (IS_OBJ_TYPE(thiz, OBJ_INST)) {
    // The closure is retrieved from [thiz] thus, it doesn't need to be push
    // on the VM's temp references (since [thiz] should already be protected
    // from GC).
    Closure* closure = NULL;

    if (!repr) {
      closure = getMagicMethod(getClass(vm, thiz), METHOD_STR);
    }

    if (closure == NULL) {
      closure = getMagicMethod(getClass(vm, thiz), METHOD_REPR);
    }

    if (closure != NULL) {
      Var ret = VAR_NULL;
      Result result = vmCallMethod(vm, thiz, closure, 0, NULL, &ret);
      if (result != RESULT_SUCCESS)
        return NULL;

      if (!IS_OBJ_TYPE(ret, OBJ_STRING)) {
        VM_SET_ERROR(vm, newString(vm, "method " LITS__str " returned "
                                       "non-string type."));
        return NULL;
      }

      return (String*) AS_OBJ(ret);
    }

    // If we reached here, it doesn't have a to string override. just
    // "fall throught" and call 'toString()' bellow.
  }

  if (repr)
    return toRepr(vm, thiz);
  return toString(vm, thiz);
}

Var varSprintf(VM* vm, String* string, List* args) {
  ByteBuffer retbuff;
  ByteBufferInit(&retbuff);

  ByteBuffer fmtbuff;
  ByteBufferInit(&fmtbuff);
  ByteBufferReserve(&fmtbuff, vm, 32);

  ByteBuffer outbuff;
  ByteBufferInit(&outbuff);
  ByteBufferReserve(&outbuff, vm, 64);

  uint32_t index = 0; // index of args
  char* cur = string->data;
  char* percent = NULL;

  while (cur < string->data + string->length) {
    if (percent == NULL) {
      if (*cur == '%') {
        percent = cur++;
      } else {
        ByteBufferWrite(&retbuff, vm, *cur++);
      }
      continue;
    }

    char specifier;
    switch (*cur++) {
      case '%':
        ByteBufferWrite(&retbuff, vm, '%');
        percent = NULL;
        continue;

      case 'f':
      case 'F':
      case 'e':
      case 'E':
      case 'g':
      case 'G':
        specifier = 'f';
        break;

      case 'd':
      case 'i':
      case 'u':
      case 'x':
      case 'X':
      case 'o':
      case 'b':
        specifier = 'i';
        break;

      case 'c':
        specifier = 'c';
        break;

      case 's':
        specifier = 's';
        break;

      default:
        continue;
    }

    fmtbuff.count = 0;
    while (percent < cur) {
      char c = *percent++;
      if (c == 'c')
        c = 's'; // support encode to utf8 later
      if (c != '*')
        ByteBufferWrite(&fmtbuff, vm, c); // don't support '*'
    }
    ByteBufferWrite(&fmtbuff, vm, 0);
    percent = NULL;

    double num = 0;
    String* str = NULL;

    if (index < args->elements.count) {
      if (specifier == 's') {
        str = varToString(vm, args->elements.data[index], false);

      } else {
        if (!isNumeric(args->elements.data[index], &num)) {
          if (IS_OBJ_TYPE(args->elements.data[index], OBJ_STRING)) {
            str = (String*) AS_OBJ(args->elements.data[index]);
            utilToNumber(str->data, &num);
          }
        }
      }
      index++;
    }

    uint32_t len = 0;
    uint8_t utf8[4];
    for (;;) {
      switch (specifier) {
        case 'f':
          len = snprintf((char*) outbuff.data, outbuff.capacity, (char*) fmtbuff.data, num);
          break;
        case 'i':
          len = snprintf((char*) outbuff.data, outbuff.capacity,
                         (char*) fmtbuff.data, (int32_t) num);
          break;
        case 'c':
          utf8[utf8_encodeValue((int) num, utf8)] = 0;
          len = snprintf((char*) outbuff.data, outbuff.capacity, (char*) fmtbuff.data, utf8);
          break;
        case 's':
          if (str != NULL) {
            len = snprintf((char*) outbuff.data, outbuff.capacity,
                           (char*) fmtbuff.data, str->data);
          }
          break;
        default:
          UNREACHABLE();
      }

      if (len + 1 <= outbuff.capacity)
        break;
      ByteBufferReserve(&outbuff, vm, len + 1);
    }

    ByteBufferAddString(&retbuff, vm, (char*) outbuff.data, len);
  }

  String* str = newStringLength(vm, (const char*) retbuff.data, retbuff.count);
  ByteBufferClear(&retbuff, vm);
  ByteBufferClear(&outbuff, vm);
  ByteBufferClear(&fmtbuff, vm);
  return VAR_OBJ(str);
}

// Calls a unary operator overload method. If the method does not exists it'll
// return false, otherwise it'll call the method and return true. If any error
// occures it'll set an error.
static inline bool _callUnaryOpMethod(VM* vm, Var thiz, const char* method_name, Var* ret) {
  Closure* closure = NULL;
  String* name = newString(vm, method_name);
  vmPushTempRef(vm, &name->_super); // name.
  bool has_method = hasMethod(vm, thiz, name, &closure);
  vmPopTempRef(vm); // name.

  if (!has_method)
    return false;

  vmCallMethod(vm, thiz, closure, 0, NULL, ret);
  return true;
}

// Calls a binary operator overload method. If the method does not exists it'll
// return false, otherwise it'll call the method and return true. If any error
// occures it'll set an error.
static inline bool _callBinaryOpMethod(VM* vm, Var thiz, Var other,
                                       const char* method_name, Var* ret) {
  Closure* closure = NULL;
  String* name = newString(vm, method_name);
  vmPushTempRef(vm, &name->_super); // name.
  bool has_method = hasMethod(vm, thiz, name, &closure);
  vmPopTempRef(vm); // name.

  if (!has_method)
    return false;

  vmCallMethod(vm, thiz, closure, 1, &other, ret);
  return true;
}

// Delete an attribute from an object. If [skipDelattr] is true, _delattr is skipped.
void varDelAttrib(VM* vm, Var on, String* attrib, bool skipDelattr) {
#define ERR_NO_ATTRIB(vm, on, attrib) \
  VM_SET_ERROR(vm, stringFormat(vm, "'$' object has no attribute named '$'", \
                                varTypeName(on), attrib->data))

  if (!IS_OBJ(on)) {
    ERR_NO_ATTRIB(vm, on, attrib);
    return;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_MODULE:
      if (!moduleDeleteGlobal(vm, (Module*) obj, attrib->data, attrib->length)) {
        ERR_NO_ATTRIB(vm, on, attrib);
      }
      return;

    case OBJ_CLASS:
      {
        Class* cls = (Class*) obj;
        Var removed = mapRemoveKey(vm, cls->static_attribs, VAR_OBJ(attrib));
        if (IS_UNDEF(removed))
          ERR_NO_ATTRIB(vm, on, attrib);
      }
      return;

    case OBJ_MAP:
      {
        Map* map = (Map*) obj;
        Var removed = mapRemoveKey(vm, map, VAR_OBJ(attrib));
        if (IS_UNDEF(removed))
          ERR_NO_ATTRIB(vm, on, attrib);
      }
      return;

    case OBJ_INST:
      {
        Instance* inst = (Instance*) obj;

        if (!skipDelattr) {
          Closure* delattr = getMagicMethod(inst->cls, METHOD_DELATTR);
          if (delattr != NULL) {
            Var arg = VAR_OBJ(attrib);
            vmCallMethod(vm, on, delattr, 1, &arg, NULL);
            return;
          }
        }

        Var removed = _instanceRemoveAttribFast(vm, inst, attrib);
        if (IS_UNDEF(removed))
          ERR_NO_ATTRIB(vm, on, attrib);
      }
      return;

    default:
      break;
  }

  ERR_NO_ATTRIB(vm, on, attrib);

#undef ERR_NO_ATTRIB
}

/*****************************************************************************/
/* OPERATORS                                                                 */
/*****************************************************************************/

Var preConstructThis(VM* vm, Class* cls) {
#define NO_INSTANCE(type_name) \
  VM_SET_ERROR(vm, \
               newString(vm, "Class '" type_name "' cannot be instanciated."))

  switch (cls->class_of) {
    case vOBJECT:
      NO_INSTANCE("Object");
      return VAR_NULL;

    case vNULL:
    case vBOOL:
    case vNUMBER:
    case vSTRING:
    case vLIST:
    case vMAP:
    case vPOINTER:
    case vRANGE:
      return VAR_NULL; // Constructor will override the null.

    case vMODULE:
      NO_INSTANCE("Module");
      return VAR_NULL;

    case vCLOSURE:
      NO_INSTANCE("Closure");
      return VAR_NULL;

    case vFIBER:
      return VAR_NULL;

    case vCLASS:
      NO_INSTANCE("Class");
      return VAR_NULL;

    case vINSTANCE:
      return VAR_OBJ(newInstance(vm, cls));
  }

  UNREACHABLE();
  return VAR_NULL;
}

void bindMethod(VM* vm, Class* cls, Closure* method) {
  ASSERT(vm != NULL && cls != NULL && method != NULL, OOPS);

  vmInvalidateInlineCaches(vm);

  if (vm->method_cache_class == cls) {
    vm->method_cache_class = NULL;
    vm->method_cache_name = NULL;
    vm->method_cache_closure = NULL;
  }

  // TODO: check hash instead of using strcmp?
  if (strcmp(method->fn->name, LITS__init) == 0) {
    cls->magic_methods[METHOD_INIT] = method;
  } else if (strcmp(method->fn->name, LITS__new) == 0) {
    cls->magic_methods[METHOD_NEW] = method;
  } else if (strcmp(method->fn->name, LITS__del) == 0) {
    cls->magic_methods[METHOD_DEL] = method;
  } else if (strcmp(method->fn->name, LITS__str) == 0) {
    cls->magic_methods[METHOD_STR] = method;
  } else if (strcmp(method->fn->name, LITS__repr) == 0) {
    cls->magic_methods[METHOD_REPR] = method;
  } else if (strcmp(method->fn->name, LITS__getattribute) == 0) {
    cls->magic_methods[METHOD_GETATTRIBUTE] = method;
  } else if (strcmp(method->fn->name, LITS__getattr) == 0) {
    cls->magic_methods[METHOD_GETATTR] = method;
  } else if (strcmp(method->fn->name, LITS__getter) == 0) {
    cls->magic_methods[METHOD_GETTER] = method;
  } else if (strcmp(method->fn->name, LITS__setattr) == 0) {
    cls->magic_methods[METHOD_SETATTR] = method;
  } else if (strcmp(method->fn->name, LITS__setter) == 0) {
    cls->magic_methods[METHOD_SETTER] = method;
  } else if (strcmp(method->fn->name, LITS__delattr) == 0) {
    cls->magic_methods[METHOD_DELATTR] = method;
  } else if (strcmp(method->fn->name, LITS__call) == 0) {
    cls->magic_methods[METHOD_CALL] = method;
  }

  if (cls->method_lookup == NULL) {
    cls->method_lookup = newMap(vm);
  }

  if (method->fn->name != NULL) {
    String* method_name = newInternedString(vm, method->fn->name);
    vmPushTempRef(vm, &method_name->_super); // method_name

    if (IS_UNDEF(mapGetStringKey(cls->method_lookup, method_name))) {
      mapSetStringKey(vm, cls->method_lookup, method_name, VAR_OBJ(method));
    }

    vmPopTempRef(vm); // method_name
  }

  ClosureBufferWrite(&cls->methods, vm, method);
}

Closure* getMagicMethod(Class* cls, MagicMethod m) {
  ASSERT(cls != NULL, OOPS);

  // magic method
  //   -1: find the method from ancestor
  //   NULL: not found and don't find again
  if (cls->magic_methods[m] == (Closure*) -1) {
    cls->magic_methods[m] = NULL;

    Class* super = cls->super_class;
    while (super != NULL) {
      if (super->magic_methods[m] != NULL && super->magic_methods[m] != (Closure*) -1) {
        cls->magic_methods[m] = super->magic_methods[m];
        break;
      }
      super = super->super_class;
    }
  }
  // printf("%d %p\n", m, cls->magic_methods[m]);
  return cls->magic_methods[m];
}

Class* getClass(VM* vm, Var instance) {
  VarType type = getVarType(instance);
  if (0 <= type && type < vINSTANCE) {
    return vm->builtin_classes[type];
  }
  ASSERT(IS_OBJ_TYPE(instance, OBJ_INST), OOPS);
  Instance* inst = (Instance*) AS_OBJ(instance);
  return inst->cls;
}

// Returns a method on a class (it'll walk up the inheritance tree to search
// and if the method not found, it'll return NULL.
static inline Closure* clsGetMethod(VM* vm, Class* cls, String* name) {
  Class* cls_ = cls;
  do {
    if (cls_->method_lookup != NULL) {
      Var method = mapGetStringKey(cls_->method_lookup, name);
      if (IS_OBJ_TYPE(method, OBJ_CLOSURE)) {
        Closure* closure = (Closure*) AS_OBJ(method);
        ASSERT(closure->fn->is_method, OOPS);
        return closure;
      }
    }

    // Fallback for classes that may have direct method-buffer writes.
    for (int i = 0; i < (int) cls_->methods.count; i++) {
      Closure* method_ = cls_->methods.data[i];
      ASSERT(method_->fn->is_method, OOPS);
      const char* method_name = method_->fn->name;
      if (IS_CSTR_EQ(name, method_name, strlen(method_name))) {
        if (cls_->method_lookup != NULL) {
          String* cached_name = newInternedString(vm, method_name);
          vmPushTempRef(vm, &cached_name->_super); // cached_name
          mapSetStringKey(vm, cls_->method_lookup, cached_name, VAR_OBJ(method_));
          vmPopTempRef(vm); // cached_name
        }
        return method_;
      }
    }

    cls_ = cls_->super_class;
  } while (cls_ != NULL);
  return NULL;
}

bool hasMethod(VM* vm, Var thiz, String* name, Closure** _method) {
  Class* cls = getClass(vm, thiz);
  ASSERT(cls != NULL, OOPS);

  if (vm->method_cache_class == cls && vm->method_cache_name == name
      && vm->method_cache_closure != NULL) {
    *_method = vm->method_cache_closure;
    return true;
  }

  Closure* method_ = clsGetMethod(vm, cls, name);
  if (method_ != NULL) {
    vm->method_cache_class = cls;
    vm->method_cache_name = name;
    vm->method_cache_closure = method_;

    *_method = method_;
    return true;
  }

  return false;
}

Var getMethod(VM* vm, Var thiz, String* name, bool* is_method) {
  Closure* method;
  if (hasMethod(vm, thiz, name, &method)) {
    if (is_method)
      *is_method = true;
    return VAR_OBJ(method);
  }

  // If the attribute not found it'll set an error.
  if (is_method)
    *is_method = false;

  return varGetAttrib(vm, thiz, name, false, true);
}

Closure* getSuperMethod(VM* vm, Var thiz, String* name) {
  Class* super = getClass(vm, thiz)->super_class;
  if (super == NULL) {
    VM_SET_ERROR(vm, stringFormat(vm, "'$' object has no parent class.", varTypeName(thiz)));
    return NULL;
  };

  Closure* method = clsGetMethod(vm, super, name);
  if (method == NULL) {
    VM_SET_ERROR(vm, stringFormat(vm, "'@' class has no method named '@'.",
                                  super->name, name));
  }
  return method;
}

#define UNSUPPORTED_UNARY_OP(op) \
  VM_SET_ERROR(vm, stringFormat(vm, \
                                "Unsupported operand ($) for " \
                                "unary operator " op ".", \
                                varTypeName(v)))

#define UNSUPPORTED_BINARY_OP(op) \
  VM_SET_ERROR(vm, stringFormat(vm, \
                                "Unsupported operand types for " \
                                "operator '" op "' $ and $", \
                                varTypeName(v1), varTypeName(v2)))

#define RIGHT_OPERAND "Right operand"

#define CHECK_NUMERIC_OP_AS(op, as) \
  do { \
    double n1, n2; \
    if (isNumeric(v1, &n1)) { \
      if (validateNumeric(vm, v2, &n2, RIGHT_OPERAND)) { \
        return as(n1 op n2); \
      } \
      return VAR_NULL; \
    } \
  } while (false)

#define CHECK_STRING_OP_AS(op, as) \
  do { \
    if (IS_OBJ_TYPE(v1, OBJ_STRING) && IS_OBJ_TYPE(v2, OBJ_STRING)) { \
      String *s1 = (String*) AS_OBJ(v1), *s2 = (String*) AS_OBJ(v2); \
      int l1 = s1->length, l2 = s2->length, min = (l1 < l2 ? l1 : l2); \
      int result = memcmp(s1->data, s2->data, min); \
      if (result == 0) \
        return as((l1 - l2) op 0); \
      else \
        return as(result op 0); \
    } \
  } while (false)

#define CHECK_NUMERIC_OP(op) CHECK_NUMERIC_OP_AS(op, VAR_NUM)

#define CHECK_BITWISE_OP(op) \
  do { \
    int64_t i1, i2; \
    if (isInteger(v1, &i1)) { \
      if (validateInteger(vm, v2, &i2, RIGHT_OPERAND)) { \
        return VAR_NUM((double) (i1 op i2)); \
      } \
      return VAR_NULL; \
    } \
  } while (false)

#define CHECK_INST_UNARY_OP(name) \
  do { \
    if (IS_OBJ_TYPE(v, OBJ_INST)) { \
      Var result; \
      if (_callUnaryOpMethod(vm, v, name, &result)) { \
        return result; \
      } \
    } \
  } while (false)

#define CHECK_INST_BINARY_OP(name) \
  do { \
    if (IS_OBJ_TYPE(v1, OBJ_INST)) { \
      Var result; \
      if (inplace) { \
        if (_callBinaryOpMethod(vm, v1, v2, name "=", &result)) { \
          return result; \
        } \
      } \
      if (_callBinaryOpMethod(vm, v1, v2, name, &result)) { \
        return result; \
      } \
    } \
  } while (false)

Var varPositive(VM* vm, Var v) {
  double n;
  if (isNumeric(v, &n))
    return v;
  CHECK_INST_UNARY_OP("+thiz");
  UNSUPPORTED_UNARY_OP("unary +");
  return VAR_NULL;
}

Var varNegative(VM* vm, Var v) {
  double n;
  if (isNumeric(v, &n))
    return VAR_NUM(-AS_NUM(v));
  CHECK_INST_UNARY_OP("-thiz");
  UNSUPPORTED_UNARY_OP("unary -");
  return VAR_NULL;
}

Var varNot(VM* vm, Var v) {
  CHECK_INST_UNARY_OP("!thiz");
  return VAR_BOOL(!toBool(v));
}

Var varBitNot(VM* vm, Var v) {
  int64_t i;
  if (isInteger(v, &i))
    return VAR_NUM((double) (~i));
  CHECK_INST_UNARY_OP("~thiz");
  UNSUPPORTED_UNARY_OP("unary ~");
  return VAR_NULL;
}

Var varAdd(VM* vm, Var v1, Var v2, bool inplace) {
  CHECK_NUMERIC_OP(+);

  if (IS_OBJ(v1)) {
    Object* o1 = AS_OBJ(v1);
    switch (o1->type) {
      case OBJ_STRING:
        {
          if (!IS_OBJ(v2))
            break;
          Object* o2 = AS_OBJ(v2);
          if (o2->type == OBJ_STRING) {
            return VAR_OBJ(stringJoin(vm, (String*) o1, (String*) o2));
          }
        }
        break;

      case OBJ_LIST:
        {
          if (!IS_OBJ(v2))
            break;
          Object* o2 = AS_OBJ(v2);
          if (o2->type == OBJ_LIST) {
            if (inplace) {
              VarBufferConcat(&((List*) o1)->elements, vm, &((List*) o2)->elements);
              return v1;
            } else {
              return VAR_OBJ(listAdd(vm, (List*) o1, (List*) o2));
            }
          }
        }
        break;

      default:
        break;
    }
  }
  CHECK_INST_BINARY_OP("+");
  UNSUPPORTED_BINARY_OP("+");
  return VAR_NULL;
}

Var varModulo(VM* vm, Var v1, Var v2, bool inplace) {
  double n1, n2;
  if (isNumeric(v1, &n1)) {
    if (validateNumeric(vm, v2, &n2, RIGHT_OPERAND)) {
      if (n2 == 0) {
        VM_SET_ERROR(vm, newString(vm, "Division by zero."));
        return VAR_NULL;
      }
      return VAR_NUM(fmod(n1, n2));
    }
    return VAR_NULL;
  }

  if (IS_OBJ_TYPE(v1, OBJ_STRING)) {
    Var result;
    if (IS_OBJ_TYPE(v2, OBJ_LIST)) {
      result = varSprintf(vm, (String*) AS_OBJ(v1), (List*) AS_OBJ(v2));

    } else {
      List* args = newList(vm, 1);
      vmPushTempRef(vm, &args->_super);
      listAppend(vm, args, v2);
      result = varSprintf(vm, (String*) AS_OBJ(v1), args);
      vmPopTempRef(vm);
    }
    return result;
  }

  CHECK_INST_BINARY_OP("%");
  UNSUPPORTED_BINARY_OP("%");
  return VAR_NULL;
}

// TODO: the bellow function definitions can be written as macros.

Var varSubtract(VM* vm, Var v1, Var v2, bool inplace) {
  CHECK_NUMERIC_OP(-);
  CHECK_INST_BINARY_OP("-");
  UNSUPPORTED_BINARY_OP("-");
  return VAR_NULL;
}

Var varMultiply(VM* vm, Var v1, Var v2, bool inplace) {
  CHECK_NUMERIC_OP(*);
  CHECK_INST_BINARY_OP("*");

  if (IS_OBJ_TYPE(v1, OBJ_STRING)) {
    String* left = (String*) AS_OBJ(v1);
    int64_t right;
    if (isInteger(v2, &right)) {
      if (left->length == 0)
        return VAR_OBJ(left);
      if (right == 0)
        return VAR_OBJ(newString(vm, ""));

      // In python multiplying with negative number will result an empty
      // string so we're following the same rule here.
      if (right < 0)
        return VAR_OBJ(newString(vm, ""));

      if ((uint64_t) left->length * (uint64_t) right > UINT32_MAX) {
        VM_SET_ERROR(vm, newString(vm, "String repetition result too large."));
        return VAR_NULL;
      }

      String* str = newStringLength(vm, "", left->length * (uint32_t) right);
      char* buff = str->data;
      for (int i = 0; i < (int) right; i++) {
        memcpy(buff, left->data, left->length);
        buff += left->length;
      }
      ASSERT(buff == str->data + str->length, OOPS);
      str->hash = utilHashString(str->data);
      return VAR_OBJ(str);
    } else {
      VM_SET_ERROR(
          vm, stringFormat(
                  vm, "can't multiply sequence by non-int of type 'float'"));
      return VAR_NULL;
    }
  }

  UNSUPPORTED_BINARY_OP("*");
  return VAR_NULL;
}

Var varDivide(VM* vm, Var v1, Var v2, bool inplace) {
  double n1, n2;
  if (isNumeric(v1, &n1)) {
    if (validateNumeric(vm, v2, &n2, RIGHT_OPERAND)) {
      if (n2 == 0) {
        VM_SET_ERROR(vm, newString(vm, "Division by zero."));
        return VAR_NULL;
      }
      return VAR_NUM(n1 / n2);
    }
    return VAR_NULL;
  }

  CHECK_INST_BINARY_OP("/");
  UNSUPPORTED_BINARY_OP("/");
  return VAR_NULL;
}

Var varExponent(VM* vm, Var v1, Var v2, bool inplace) {
  double n1, n2;
  if (isNumeric(v1, &n1)) {
    if (validateNumeric(vm, v2, &n2, RIGHT_OPERAND)) {
      return VAR_NUM(pow(n1, n2));
    }
    return VAR_NULL;
  }

  CHECK_INST_BINARY_OP("**");
  UNSUPPORTED_BINARY_OP("**");
  return VAR_NULL;
}

Var varBitAnd(VM* vm, Var v1, Var v2, bool inplace) {
  CHECK_BITWISE_OP(&);
  CHECK_INST_BINARY_OP("&");
  UNSUPPORTED_BINARY_OP("&");
  return VAR_NULL;
}

Var varBitOr(VM* vm, Var v1, Var v2, bool inplace) {
  CHECK_BITWISE_OP(|);
  CHECK_INST_BINARY_OP("|");
  UNSUPPORTED_BINARY_OP("|");
  return VAR_NULL;
}

Var varBitXor(VM* vm, Var v1, Var v2, bool inplace) {
  CHECK_BITWISE_OP(^);
  CHECK_INST_BINARY_OP("^");
  UNSUPPORTED_BINARY_OP("^");
  return VAR_NULL;
}

Var varBitLshift(VM* vm, Var v1, Var v2, bool inplace) {
  CHECK_BITWISE_OP(<<);
  CHECK_INST_BINARY_OP("<<");
  UNSUPPORTED_BINARY_OP("<<");
  return VAR_NULL;
}

Var varBitRshift(VM* vm, Var v1, Var v2, bool inplace) {
  CHECK_BITWISE_OP(>>);
  CHECK_INST_BINARY_OP(">>");
  UNSUPPORTED_BINARY_OP(">>");
  return VAR_NULL;
}

Var varEqals(VM* vm, Var v1, Var v2) {
  const bool inplace = false;
  CHECK_INST_BINARY_OP("==");
  return VAR_BOOL(isValuesEqual(v1, v2));
}

Var varGreater(VM* vm, Var v1, Var v2) {
  CHECK_NUMERIC_OP_AS(>, VAR_BOOL);
  CHECK_STRING_OP_AS(>, VAR_BOOL);
  const bool inplace = false;
  CHECK_INST_BINARY_OP(">");
  UNSUPPORTED_BINARY_OP(">");
  return VAR_NULL;
}

Var varLesser(VM* vm, Var v1, Var v2) {
  CHECK_NUMERIC_OP_AS(<, VAR_BOOL);
  CHECK_STRING_OP_AS(<, VAR_BOOL);
  const bool inplace = false;
  CHECK_INST_BINARY_OP("<");
  UNSUPPORTED_BINARY_OP("<");
  return VAR_NULL;
}

Var varOpRange(VM* vm, Var v1, Var v2) {
  if (IS_NUM(v1) && IS_NUM(v2)) {
    return VAR_OBJ(newRange(vm, AS_NUM(v1), AS_NUM(v2)));
  }

  if (IS_OBJ_TYPE(v1, OBJ_STRING)) {
    String* str = varToString(vm, v2, false);
    if (str == NULL)
      return VAR_NULL;
    String* concat = stringJoin(vm, (String*) AS_OBJ(v1), str);
    return VAR_OBJ(concat);
  }

  const bool inplace = false;
  CHECK_INST_BINARY_OP("..");
  UNSUPPORTED_BINARY_OP("..");
  return VAR_NULL;
}

#undef RIGHT_OPERAND
#undef CHECK_NUMERIC_OP
#undef CHECK_BITWISE_OP
#undef UNSUPPORTED_UNARY_OP
#undef UNSUPPORTED_BINARY_OP

bool varContains(VM* vm, Var elem, Var container) {
  if (!IS_OBJ(container)) {
    VM_SET_ERROR(vm, stringFormat(vm, "'$' is not iterable.", varTypeName(container)));
    return false;
  }
  Object* obj = AS_OBJ(container);

  switch (obj->type) {
    case OBJ_STRING:
      {
        if (!IS_OBJ_TYPE(elem, OBJ_STRING)) {
          VM_SET_ERROR(vm, stringFormat(vm, "Expected a string operand."));
          return false;
        }

        String* sub = (String*) AS_OBJ(elem);
        String* str = (String*) AS_OBJ(container);
        if (sub->length > str->length)
          return false;

        const char* match = (const char*) utilMemMem(str->data, str->length,
                                                     sub->data, sub->length);
        return match != NULL;
      }
      break;

    case OBJ_LIST:
      {
        List* list = (List*) AS_OBJ(container);
        for (uint32_t i = 0; i < list->elements.count; i++) {
          if (isValuesEqual(elem, list->elements.data[i]))
            return true;
        }
        return false;
      }
      break;

    case OBJ_MAP:
      {
        Map* map = (Map*) AS_OBJ(container);
        return !IS_UNDEF(mapGet(map, elem));
      }
      break;

    default:
      break;
  }

#define v1 container
#define v2 elem
  const bool inplace = false;
  CHECK_INST_BINARY_OP("in");
#undef v1
#undef v2

  VM_SET_ERROR(vm, stringFormat(vm, "Argument of type $ is not iterable.",
                                varTypeName(container)));
  return VAR_NULL;
}

bool varIsType(VM* vm, Var inst, Var type) {
  if (!IS_OBJ_TYPE(type, OBJ_CLASS)) {
    VM_SET_ERROR(vm, newString(vm, "Right operand must be a class."));
    return false;
  }

  Class* cls = (Class*) AS_OBJ(type);
  Class* cls_inst = getClass(vm, inst);

  do {
    if (cls_inst == cls || PTR_EQ(cls_inst, cls))
      return true;
    cls_inst = cls_inst->super_class;
  } while (cls_inst != NULL);

  return false;
}

static int _instanceInlineAttribIndex(const Instance* inst, const String* attrib) {
  for (uint8_t i = 0; i < inst->inline_attrib_count; i++) {
    String* name = inst->inline_attrib_names[i];
    if (name == attrib)
      return (int) i;
    if (name != NULL && name->hash == attrib->hash && name->length == attrib->length
        && memcmp(name->data, attrib->data, attrib->length) == 0) {
      return (int) i;
    }
  }
  return -1;
}

static Var _instanceGetAttribFast(Instance* inst, String* attrib) {
  int index = _instanceInlineAttribIndex(inst, attrib);
  if (index >= 0)
    return inst->inline_attrib_values[index];

  if (inst->attribs != NULL)
    return mapGetStringKey(inst->attribs, attrib);

  return VAR_UNDEFINED;
}

static void _instanceSetAttribFast(VM* vm, Instance* inst, String* attrib, Var value) {
  int index = _instanceInlineAttribIndex(inst, attrib);
  if (index >= 0) {
    inst->inline_attrib_values[index] = value;
    return;
  }

  if (inst->attribs != NULL) {
    Var existing = mapGetStringKey(inst->attribs, attrib);
    if (!IS_UNDEF(existing)) {
      mapSetStringKey(vm, inst->attribs, attrib, value);
      return;
    }
  }

  if (inst->inline_attrib_count < INSTANCE_INLINE_ATTR_CAPACITY) {
    uint8_t i = inst->inline_attrib_count++;
    inst->inline_attrib_names[i] = attrib;
    inst->inline_attrib_values[i] = value;
    return;
  }

  if (inst->attribs == NULL)
    inst->attribs = newMap(vm);
  mapSetStringKey(vm, inst->attribs, attrib, value);
}

static Var _instanceRemoveAttribFast(VM* vm, Instance* inst, String* attrib) {
  int index = _instanceInlineAttribIndex(inst, attrib);
  if (index >= 0) {
    uint8_t i = (uint8_t) index;
    Var removed = inst->inline_attrib_values[i];

    if (i + 1 < inst->inline_attrib_count) {
      memmove(&inst->inline_attrib_names[i], &inst->inline_attrib_names[i + 1],
              (inst->inline_attrib_count - i - 1) * sizeof(String*));
      memmove(&inst->inline_attrib_values[i], &inst->inline_attrib_values[i + 1],
              (inst->inline_attrib_count - i - 1) * sizeof(Var));
    }

    inst->inline_attrib_count--;
    return removed;
  }

  if (inst->attribs != NULL)
    return mapRemoveKey(vm, inst->attribs, VAR_OBJ(attrib));

  return VAR_UNDEFINED;
}

static Var _newBoundMethod(VM* vm, Var instance, Closure* method) {
  MethodBind* mb = newMethodBind(vm, method);
  vmPushTempRef(vm, &mb->_super); // mb.
  mb->instance = instance;
  vmPopTempRef(vm); // mb.
  return VAR_OBJ(mb);
}

static bool _lookupAndBindMethod(VM* vm, Var instance, String* name, Var* out) {
  bool pushed_instance = false;
  if (IS_OBJ(instance)) {
    vmPushTempRef(vm, AS_OBJ(instance)); // instance.
    pushed_instance = true;
  }

  Closure* method = NULL;
  bool found = hasMethod(vm, instance, name, &method);
  if (found) {
    *out = _newBoundMethod(vm, instance, method);
  }

  if (pushed_instance) {
    vmPopTempRef(vm); // instance.
  }

  return found;
}

Var varGetAttrib(VM* vm, Var on, String* attrib, bool skipGetter, bool callable) {
#define ERR_NO_ATTRIB(vm, on, attrib) \
  VM_SET_ERROR(vm, stringFormat(vm, "'$' object has no attribute named '$'.", \
                                varTypeName(on), attrib->data))

  if (attrib->hash == CHECK_HASH("_class", 0xa2d93eae)) {
    return VAR_OBJ(getClass(vm, on));
  }

  if (!IS_OBJ(on)) {
    ERR_NO_ATTRIB(vm, on, attrib);
    return VAR_NULL;
  }

  if (IS_OBJ_TYPE(on, OBJ_INST) && !skipGetter) {
    Instance* inst = (Instance*) AS_OBJ(on);
    Closure* getattribute = _resolveMagicMethod(inst->cls, METHOD_GETATTRIBUTE);
    if (getattribute != NULL) {
      Var attrib_name = VAR_OBJ(attrib);
      Var value = VAR_NULL;
      vmCallMethod(vm, on, getattribute, 1, &attrib_name, &value);
      return value;
    }
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
      {
        String* str = (String*) obj;
        switch (attrib->hash) {
          case CHECK_HASH("length", 0x83d03615):
            return VAR_NUM(utf8_length(str->data));
        }
      }
      break;

    case OBJ_LIST:
      {
        List* list = (List*) obj;
        switch (attrib->hash) {
          case CHECK_HASH("length", 0x83d03615):
            return VAR_NUM((double) (list->elements.count));
        }
      }
      break;

    case OBJ_MAP:
      {
        Map* map = (Map*) obj;
        switch (attrib->hash) {
          case CHECK_HASH("length", 0x83d03615):
            return VAR_NUM((double) (map->count));

          case CHECK_HASH("keys", 0xF94A08CD):
            {
              List* list = newList(vm, map->count);
              vmPushTempRef(vm, &list->_super); // list.
              for (uint32_t i = 0; i < map->order_keys.count; i++) {
                listAppend(vm, list, map->order_keys.data[i]);
              }
              vmPopTempRef(vm); // list.
              return VAR_OBJ(list);
            }

          case CHECK_HASH("values", 0x34474C3B):
            {
              List* list = newList(vm, map->count);
              vmPushTempRef(vm, &list->_super); // list.
              for (uint32_t i = 0; i < map->order_keys.count; i++) {
                Var key = map->order_keys.data[i];
                listAppend(vm, list, mapGet(map, key));
              }
              vmPopTempRef(vm); // list.
              return VAR_OBJ(list);
            }

        } // switch

        Var value = mapGetStringKey(map, attrib);
        if (!IS_UNDEF(value))
          return value;
      }
      break;

    case OBJ_RANGE:
      {
        Range* range = (Range*) obj;
        switch (attrib->hash) {
          case CHECK_HASH("as_list", 0x1562c22):
            return VAR_OBJ(rangeAsList(vm, range));

          case CHECK_HASH("length", 0x83d03615):
            return VAR_NUM(rangeLength(vm, range));

            // We can't use 'start', 'end' since 'end' is a
            // keyword. Also we can't use 'from', 'to' since 'from' is a keyword
            // too. So, we're using 'first' and 'last' to access the range limits.

          case CHECK_HASH("first", 0x4881d841):
            return VAR_NUM(range->from);

          case CHECK_HASH("last", 0x63e1d819):
            return VAR_NUM(range->to);
        }
      }
      break;

    case OBJ_MODULE:
      {
        Module* module = (Module*) obj;

        switch (attrib->hash) {
          case CHECK_HASH("globals", 0x1577cde7):
            {
              Map* map = newMap(vm);
              vmPushTempRef(vm, &map->_super); // map.
              printf("count: %d\n", module->context->globals.count);
              for (int i = 0; i < (int) module->context->globals.count; i++) {
                String* name = moduleGetStringAt(
                    module, module->context->global_names.data[i]);
                if (name->data[0] == SPECIAL_NAME_CHAR) {
                  continue;
                }
                mapSet(vm, map, VAR_OBJ(name), module->context->globals.data[i]);
              }
              vmPopTempRef(vm); // map.

              return VAR_OBJ(map);
            }
        }

        // For generic attribute access, prefer module methods over globals.
        // Callable path already resolved methods in getMethod().
        if (!callable) {
          Var bound = VAR_UNDEFINED;
          if (_lookupAndBindMethod(vm, on, attrib, &bound)) {
            return bound;
          }
        }

        // Search in globals.
        int index = moduleGetGlobalIndexByName(vm, module, attrib);
        if (index != -1) {
          ASSERT_INDEX((uint32_t) index, module->context->globals.count);
          return module->context->globals.data[index];
        }
      }
      break;

    case OBJ_FUNC:
      break;

    case OBJ_CLOSURE:
      {
        Closure* closure = (Closure*) obj;
        switch (attrib->hash) {
          case CHECK_HASH("name", 0x8d39bde6):
            return VAR_OBJ(newString(vm, closure->fn->name));

          case CHECK_HASH("_docs", 0x8fb536a9):
            if (closure->fn->docstring) {
              return VAR_OBJ(newString(vm, closure->fn->docstring));
            } else {
              return VAR_OBJ(newString(vm, ""));
            }

          case CHECK_HASH("arity", 0x3e96bd7a):
            return VAR_NUM((double) (closure->fn->arity));
        }
      }
      break;

    case OBJ_METHOD_BIND:
      {
        MethodBind* mb = (MethodBind*) obj;

        switch (attrib->hash) {
          case CHECK_HASH("_docs", 0x8fb536a9):
            if (mb->method->fn->docstring) {
              return VAR_OBJ(newString(vm, mb->method->fn->docstring));
            } else {
              return VAR_OBJ(newString(vm, ""));
            }

          case CHECK_HASH("name", 0x8d39bde6):
            return VAR_OBJ(newString(vm, mb->method->fn->name));

          case CHECK_HASH("instance", 0xb86d992):
            if (IS_UNDEF(mb->instance))
              return VAR_NULL;
            return mb->instance;

          case CHECK_HASH("arity", 0x3e96bd7a):
            return VAR_NUM((double) (mb->method->fn->arity));
        }
      }
      break;

    case OBJ_UPVALUE:
      UNREACHABLE(); // Upvalues aren't first class objects.
      break;

    case OBJ_FIBER:
      {
        Fiber* fb = (Fiber*) obj;
        switch (attrib->hash) {
          case CHECK_HASH("is_done", 0x789c2706):
            return VAR_BOOL(fb->state == FIBER_DONE);

          case CHECK_HASH("function", 0x9ed64249):
            return VAR_OBJ(fb->closure);
        }
      }
      break;

    case OBJ_CLASS:
      {
        Class* cls = (Class*) obj;

        switch (attrib->hash) {
          case CHECK_HASH("_docs", 0x8fb536a9):
            if (cls->docstring) {
              return VAR_OBJ(newString(vm, cls->docstring));
            } else {
              return VAR_OBJ(newString(vm, ""));
            }

          case CHECK_HASH("name", 0x8d39bde6):
            return VAR_OBJ(newString(vm, cls->name->data));

          case CHECK_HASH("parent", 0xeacdfcfd):
            if (cls->super_class != NULL) {
              return VAR_OBJ(cls->super_class);
            } else {
              return VAR_NULL;
            }
        }

        Var value = mapGetStringKey(cls->static_attribs, attrib);
        if (!IS_UNDEF(value))
          return value;

        bool pushed_on = false;
        if (IS_OBJ(on)) {
          vmPushTempRef(vm, AS_OBJ(on)); // on.
          pushed_on = true;
        }

        Closure* method_ = clsGetMethod(vm, cls, attrib);
        if (method_ != NULL) {
          Var bound = _newBoundMethod(vm, on, method_);
          if (pushed_on)
            vmPopTempRef(vm); // on.
          return bound;
        }

        if (pushed_on)
          vmPopTempRef(vm); // on.
      }
      break;

    case OBJ_POINTER:
      break;

    case OBJ_INST:
      {
        Instance* inst = (Instance*) obj;
        Var value = _instanceGetAttribFast(inst, attrib);
        if (!IS_UNDEF(value))
          return value;
      }
      break;
  }

  if (!callable) {
    Var bound = VAR_UNDEFINED;
    if (_lookupAndBindMethod(vm, on, attrib, &bound)) {
      return bound;
    }
  }

  if (IS_OBJ_TYPE(on, OBJ_INST) && !skipGetter) {
    Instance* inst = (Instance*) AS_OBJ(on);
    Closure* getattr = _resolveMagicMethod(inst->cls, METHOD_GETATTR);
    if (getattr != NULL) {
      Var attrib_name = VAR_OBJ(attrib);
      Var value = VAR_NULL;
      vmCallMethod(vm, on, getattr, 1, &attrib_name, &value);
      return value;
    }

    Closure* getter = _resolveMagicMethod(inst->cls, METHOD_GETTER);
    if (getter != NULL) {
      Var attrib_name = VAR_OBJ(attrib);
      Var value = VAR_NULL;
      vmCallMethod(vm, on, getter, 1, &attrib_name, &value);
      return value;
    }
  }

  ERR_NO_ATTRIB(vm, on, attrib);
  return VAR_NULL;

#undef ERR_NO_ATTRIB
}

void varSetAttrib(VM* vm, Var on, String* attrib, Var value, bool skipSetter) {
// Set error for accessing non-existed attribute.
#define ERR_NO_ATTRIB(vm, on, attrib) \
  VM_SET_ERROR(vm, stringFormat(vm, "'$' object has no mutable attribute named '$'", \
                                varTypeName(on), attrib->data))

  if (!IS_OBJ(on)) {
    ERR_NO_ATTRIB(vm, on, attrib);
    return;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_MODULE:
      {
        moduleSetGlobal(vm, (Module*) obj, attrib->data, attrib->length, value);
      }
      return;

    case OBJ_FUNC:
    case OBJ_UPVALUE:
      UNREACHABLE(); // Functions aren't first class objects.
      break;

    case OBJ_CLASS:
      {
        Class* cls = (Class*) obj;
        mapSetStringKey(vm, cls->static_attribs, attrib, value);
        return;
      }

    case OBJ_MAP:
      {
        Map* map = (Map*) obj;
        mapSetStringKey(vm, map, attrib, value);
        return;
      }

    case OBJ_INST:
      {
        Instance* inst = (Instance*) obj;

        if (!skipSetter) {
          Closure* setattr = _resolveMagicMethod(inst->cls, METHOD_SETATTR);
          if (setattr != NULL) {
            Var args[2] = {VAR_OBJ(attrib), value};
            vmCallMethod(vm, VAR_OBJ(inst), setattr, 2, args, NULL);
            return; // If any error occure, it was already set.
          }

          Closure* setter = _resolveMagicMethod(inst->cls, METHOD_SETTER);
          if (setter != NULL) {
            // FIXME: Optimize argument passing to `_setter`.
            // Once values can be retrieved directly from the stack, pass a
            // pointer to the stack slots instead of creating a temporary `args` array.
            Var args[2] = {VAR_OBJ(attrib), value};

            vmCallMethod(vm, VAR_OBJ(inst), setter, 2, args, NULL);
            return; // If any error occure, it was already set.
          }
        }

        _instanceSetAttribFast(vm, inst, attrib, value);
        return;
      }
      break;

    default:
      break;
  }
  ERR_NO_ATTRIB(vm, on, attrib);
  return;

#undef ATTRIB_IMMUTABLE
#undef ERR_NO_ATTRIB
}

// Given a range. It'll "normalize" the range to slice an object (string or
// list) set the [start] index [length] and [reversed]. On success it'll return
// true.
static bool _normalizeSliceRange(VM* vm, Range* range, uint32_t count,
                                 int32_t* start, int32_t* length, bool* reversed) {
  if ((floor(range->from) != range->from) || (floor(range->to) != range->to)) {
    VM_SET_ERROR(vm, newString(vm, "Expected a whole number."));
    return false;
  }

  int32_t from = (int32_t) range->from;
  int32_t to = (int32_t) range->to;

  if (from < 0)
    from = count + from;
  if (to < 0)
    to = count + to;

  *reversed = false;
  if (to < from) {
    int32_t tmp = to;
    to = from;
    from = tmp;
    *reversed = true;
  }

  // lenient slicing (clamp values).
  if (from < 0)
    from = 0;
  if (to >= (int32_t) count)
    to = (int32_t) count - 1;

  if (from > to) {
    *start = 0;
    *length = 0;
    *reversed = false;
    return true;
  }

  *start = from;
  *length = to - from + 1;

  return true;
}

// Slice the string with the [range] and reutrn it. On error it'll set
// an error and return NULL.
static String* _sliceString(VM* vm, String* str, Range* range) {
  int32_t start;
  int32_t length;
  bool reversed;

  int char_length = utf8_length(str->data);

  if (char_length < 0) {
    VM_SET_ERROR(vm, newString(vm, "Invalid UTF-8 string."));
    return NULL;
  }

  if (!_normalizeSliceRange(vm, range, char_length, &start, &length, &reversed)) {
    return NULL;
  }

  String* slice = utf8_slice(vm, str->data, start, length, reversed);

  if (slice == NULL) {
    VM_SET_ERROR(vm, newString(vm, "Invalid UTF-8 string."));
    return NULL;
  }

  return slice;
}

// Slice the list with the [range] and reutrn it. On error it'll set
// an error and return NULL.
static List* _sliceList(VM* vm, List* list, Range* range) {
  int32_t start, length;
  bool reversed;
  if (!_normalizeSliceRange(vm, range, list->elements.count, &start, &length, &reversed)) {
    return NULL;
  }

  List* slice = newList(vm, length);
  vmPushTempRef(vm, &slice->_super); // slice.

  for (int32_t i = 0; i < length; i++) {
    int32_t ind = (reversed) ? start + length - 1 - i : start + i;
    listAppend(vm, slice, list->elements.data[ind]);
  }

  vmPopTempRef(vm); // slice.
  return slice;
}

Var varGetSubscript(VM* vm, Var on, Var key) {
  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.", varTypeName(on)));
    return VAR_NULL;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
      {
        int64_t index;
        String* str = (String*) obj;

        if (isInteger(key, &index)) {
          // str->length is BYTE length.
          // We need CHARACTER length here.
          size_t char_length = utf8_length(str->data);

          // Normalize negative index.
          if (index < 0)
            index = (int64_t) char_length + index;

          // Bounds check against Unicode characters.
          if (index < 0 || index >= (int64_t) char_length) {
            VM_SET_ERROR(vm, newString(vm, "String index out of bound."));
            return VAR_NULL;
          }

          int value;

          int byte_index = utf8_charAt(str->data, (size_t) index, &value);

          if (byte_index < 0) {
            VM_SET_ERROR(vm, newString(vm, "Invalid UTF-8 string."));
            return VAR_NULL;
          }

          int byte_count = utf8_encodeBytesCount(value);

          if (byte_count <= 0) {
            VM_SET_ERROR(vm, newString(vm, "Invalid Unicode code point."));
            return VAR_NULL;
          }

          String* c = newStringLength(vm, str->data + byte_index, byte_count);

          return VAR_OBJ(c);
        }

        if (IS_OBJ_TYPE(key, OBJ_RANGE)) {
          String* subs = _sliceString(vm, str, (Range*) AS_OBJ(key));

          if (subs != NULL)
            return VAR_OBJ(subs);

          return VAR_NULL;
        }
      }
      break;

    case OBJ_LIST:
      {
        int64_t index;
        VarBuffer* elems = &((List*) obj)->elements;

        if (isInteger(key, &index)) {
          // Normalize index.
          if (index < 0)
            index = elems->count + index;
          if (index >= elems->count || index < 0) {
            VM_SET_ERROR(vm, newString(vm, "List index out of bound."));
            return VAR_NULL;
          }
          return elems->data[index];
        }

        if (IS_OBJ_TYPE(key, OBJ_RANGE)) {
          List* sublist = _sliceList(vm, (List*) obj, (Range*) AS_OBJ(key));
          if (sublist != NULL)
            return VAR_OBJ(sublist);
          return VAR_NULL;
        }
      }
      break;

    case OBJ_MAP:
      {
        Var value = mapGet((Map*) obj, key);
        if (IS_UNDEF(value)) {
          if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
            VM_SET_ERROR(vm, stringFormat(vm, "Unhashable key '$'.", varTypeName(key)));
          } else {
            String* key_repr = varToString(vm, key, true);
            vmPushTempRef(vm, &key_repr->_super); // key_repr.
            VM_SET_ERROR(vm, stringFormat(vm, "Key '@' not exists", key_repr));
            vmPopTempRef(vm); // key_repr.
          }
          return VAR_NULL;
        }
        return value;
      }
      break;

    case OBJ_FUNC:
    case OBJ_UPVALUE:
      UNREACHABLE(); // Not first class objects.

    case OBJ_INST:
      {
        Var ret;
        if (_callBinaryOpMethod(vm, on, key, "[]", &ret)) {
          return ret;
        }
      }
      break;

    default:
      break;
  }

  VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.", varTypeName(on)));
  return VAR_NULL;
}

void varsetSubscript(VM* vm, Var on, Var key, Var value) {
  if (!IS_OBJ(on)) {
    VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.", varTypeName(on)));
    return;
  }

  Object* obj = AS_OBJ(on);
  switch (obj->type) {
    case OBJ_STRING:
      {
        // TODO: Simplify This String Subscript
        // FIXME: A new string cannot be added to its hash
        //        already contains the previous String's hash.
        int64_t index;
        String* str = ((String*) obj);

        if (isInteger(key, &index)) {
          // Normalize index.
          if (index < 0)
            index = str->length + index;
          if (index >= str->length || index < 0) {
            VM_SET_ERROR(vm, newString(vm, "String index out of bound."));
            return;
          }
        }

        if (!IS_OBJ(value)) {
          VM_SET_ERROR(vm, stringFormat(vm, "String subscript type $ is not allowed.",
                                        varTypeName(value)));
          return;
        }

        Object* objValue = AS_OBJ(value);
        if (objValue->type == OBJ_STRING) {
          String* strReplace = ((String*) objValue);
          str = replaceSubstring(vm, index, str, strReplace);
          str->hash = utilHashString(str->data);

          on = VAR_OBJ(str);

          return;
        }
      }
      break;

    case OBJ_LIST:
      {
        int64_t index;
        VarBuffer* elems = &((List*) obj)->elements;
        if (!validateInteger(vm, key, &index, "List index"))
          return;

        // Normalize index.
        if (index < 0)
          index = elems->count + index;
        if (index < 0) {
          VM_SET_ERROR(vm, newString(vm, "List index out of bound."));
          return;
        }

        if (index >= elems->count) {
          VarBufferFill(elems, vm, VAR_NULL, (index + 1) - elems->count);
        }

        elems->data[index] = value;
        return;
      }
      break;

    case OBJ_MAP:
      {
        if (IS_OBJ(key) && !isObjectHashable(AS_OBJ(key)->type)) {
          VM_SET_ERROR(vm, stringFormat(vm, "$ type is not hashable.", varTypeName(key)));
        } else {
          mapSet(vm, (Map*) obj, key, value);
        }
        return;
      }
      break;

    case OBJ_FUNC:
    case OBJ_UPVALUE:
      UNREACHABLE();

    case OBJ_INST:
      {
        Closure* closure = NULL;
        String* name = newString(vm, "[]=");
        vmPushTempRef(vm, &name->_super); // name.
        bool has_method = hasMethod(vm, on, name, &closure);
        vmPopTempRef(vm); // name.

        if (has_method) {
          Var args[2] = {key, value};
          vmCallMethod(vm, on, closure, 2, args, NULL);
          return;
        }
      }
      break;

    default:
      break;
  }

  VM_SET_ERROR(vm, stringFormat(vm, "$ type is not subscriptable.", varTypeName(on)));
  return;
}

bool varIterate(VM* vm, Var seq, Var* iterator, Var* value) {
  Object* obj = AS_OBJ(seq);
  switch (obj->type) {
    case OBJ_STRING:
      {
        if (IS_NULL(*iterator))
          *iterator = VAR_NUM((double) 0);
        uint32_t iter = (uint32_t) AS_NUM(*iterator);

        // TODO: Need to consider utf8.
        String* str = ((String*) obj);
        if (iter >= str->length)
          return false;

        // TODO: vm's char (and reusable) strings.
        *value = VAR_OBJ(newStringLength(vm, str->data + iter, 1));
        *iterator = VAR_NUM((double) iter + 1);
        return true;
      }

    case OBJ_LIST:
      {
        if (IS_NULL(*iterator))
          *iterator = VAR_NUM((double) 0);
        uint32_t iter = (uint32_t) AS_NUM(*iterator);

        VarBuffer* elems = &((List*) obj)->elements;
        if (iter >= elems->count)
          return false;
        *value = elems->data[iter];
        *iterator = VAR_NUM((double) iter + 1);
        return true;
      }

    case OBJ_MAP:
      {
        if (IS_NULL(*iterator))
          *iterator = VAR_NUM((double) 0);
        uint32_t iter = (uint32_t) AS_NUM(*iterator);

        Map* map = (Map*) obj;
        if (map->order_keys.count == 0)
          return false;
        if (iter >= map->order_keys.count)
          return false;

        *value = map->order_keys.data[iter];
        *iterator = VAR_NUM((double) iter + 1);
        return true;
      }

    case OBJ_RANGE:
      {
        if (IS_NULL(*iterator))
          *iterator = VAR_NUM((double) 0);
        double iter = AS_NUM(*iterator);
        double from = ((Range*) obj)->from;
        double to = ((Range*) obj)->to;
        if (from == to)
          return false;

        double current;
        if (from <= to) { //< Straight range.
          current = from + iter;
        } else { //< Reversed range.
          current = from - iter;
        }
        if (current == to)
          return false;
        *value = VAR_NUM(current);
        *iterator = VAR_NUM(iter + 1);
        return true;
      }

    case OBJ_INST:
      {
        for (;;) {
          if (!_callBinaryOpMethod(vm, seq, *iterator, LITS__next, iterator))
            break;
          if (IS_NULL(*iterator))
            return false;

          if (!_callBinaryOpMethod(vm, seq, *iterator, LITS__value, value))
            break;
          return true;
        }
        goto _default;
      }

    case OBJ_FIBER:
    case OBJ_CLOSURE:
    case OBJ_MODULE:
    case OBJ_FUNC:
    case OBJ_METHOD_BIND:
    case OBJ_UPVALUE:
    case OBJ_CLASS:

    default:
    _default:
      VM_SET_ERROR(vm, stringFormat(vm, "$ is not iterable.", varTypeName(seq)));
  }
  return false;
}
