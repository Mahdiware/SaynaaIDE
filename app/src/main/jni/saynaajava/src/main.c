#include "saynaa_internal.h"

// Entry translation unit for the saynaajava module.

jstring get_java_object_name(JNIEnv* env, VM* vm, BridgeState* bridge, jobject target,
    const char* errorPrefix, const char* nullMessage) {
  if (target == NULL) {
    SetRuntimeError(vm, nullMessage);
    return NULL;
  }

  jstring jGetName = (*env)->NewStringUTF(env, "getName");
  if (jGetName == NULL) {
    clear_jni_exception_with_log(env, "get_java_object_name:NewStringUTF");
    SetRuntimeError(vm, "Failed to allocate JNI method name string.");
    return NULL;
  }

  jclass objClass = safe_find_class(vm, env, "java/lang/Object", "get_java_object_name:Object");
  if (objClass == NULL) {
    (*env)->DeleteLocalRef(env, jGetName);
    return NULL;
  }
  jobjectArray noArgs = (*env)->NewObjectArray(env, 0, objClass, NULL);
  (*env)->DeleteLocalRef(env, objClass);
  if (noArgs == NULL) {
    clear_jni_exception_with_log(env, "get_java_object_name:NewObjectArray");
    (*env)->DeleteLocalRef(env, jGetName);
    SetRuntimeError(vm, "Failed to allocate JNI argument array.");
    return NULL;
  }

  jobject classNameObj = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mCallJavaMethod, target, jGetName, noArgs);

  (*env)->DeleteLocalRef(env, noArgs);
  (*env)->DeleteLocalRef(env, jGetName);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, errorPrefix);
    return NULL;
  }

  if (classNameObj == NULL) {
    SetRuntimeError(vm, nullMessage);
    return NULL;
  }

  return (jstring) classNameObj;
}

bool wrap_bridge_global(VM* vm, jobject globalRef, int outSlot) {
  if (!ensure_wrapper_classes(vm)) {
    SetRuntimeError(vm, "Java wrappers are not initialized.");
    return false;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL || globalRef == NULL) {
    setSlotNull(vm, outSlot);
    return true;
  }

  JNIEnv* env = env_from_jvm(bridge->jvm);
  if (env == NULL) {
    SetRuntimeError(vm, "Invalid JNI Environment.");
    return false;
  }

  jobject localRef = (*env)->NewLocalRef(env, globalRef);
  if (localRef == NULL) {
    setSlotNull(vm, outSlot);
    return true;
  }

  JavaRef* ref = make_java_ref(env, bridge->jvm, localRef);
  if (ref == NULL) {
    setSlotNull(vm, outSlot);
  } else {
    create_java_instance(vm, &bridge->clsJavaObject, ref, outSlot);
  }

  (*env)->DeleteLocalRef(env, localRef);
  return true;
}

void clear_callbacks(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return;

  CallbackEntry* it = bridge->callbacks;
  while (it != NULL) {
    CallbackEntry* next = it->next;
    if (it->fnHandle != NULL)
      releaseHandle(vm, it->fnHandle);
    if (it->mapHandle != NULL)
      releaseHandle(vm, it->mapHandle);
    if (it->methodName != NULL)
      free(it->methodName);
    free(it);
    it = next;
  }

  bridge->callbacks = NULL;
  bridge->nextCallbackId = 1;
}

int register_callback(VM* vm, int slot) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return 0;

  if (GetSlotType(vm, slot) != vCLOSURE) {
    SetRuntimeError(vm, "Callback must be a function.");
    return 0;
  }

  Handle* fnHandle = GetSlotHandle(vm, slot);
  if (fnHandle == NULL) {
    SetRuntimeError(vm, "Failed to capture callback function.");
    return 0;
  }

  CallbackEntry* entry = (CallbackEntry*) calloc(1, sizeof(CallbackEntry));
  if (entry == NULL) {
    releaseHandle(vm, fnHandle);
    SetRuntimeError(vm, "Out of memory while registering callback.");
    return 0;
  }

  if (bridge->nextCallbackId <= 0)
    bridge->nextCallbackId = 1;
  entry->id = bridge->nextCallbackId++;
  entry->fnHandle = fnHandle;
  entry->mapHandle = NULL;
  entry->methodName = NULL;
  entry->next = bridge->callbacks;
  bridge->callbacks = entry;

  LOGI("Registered callback id=%d from slot=%d", entry->id, slot);

  return entry->id;
}

int register_map_callback(VM* vm, int mapSlot, const char* methodName) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return 0;

  if (GetSlotType(vm, mapSlot) != vMAP) {
    SetRuntimeError(vm, "Callback map must be a map object.");
    return 0;
  }

  Handle* mapHandle = GetSlotHandle(vm, mapSlot);
  if (mapHandle == NULL) {
    SetRuntimeError(vm, "Failed to capture callback map.");
    return 0;
  }

  CallbackEntry* entry = (CallbackEntry*) calloc(1, sizeof(CallbackEntry));
  if (entry == NULL) {
    releaseHandle(vm, mapHandle);
    SetRuntimeError(vm, "Out of memory while registering callback map.");
    return 0;
  }

  entry->methodName = str_dup_c(methodName == NULL ? "*" : methodName);
  if (entry->methodName == NULL) {
    releaseHandle(vm, mapHandle);
    free(entry);
    SetRuntimeError(vm, "Out of memory while registering callback method.");
    return 0;
  }

  if (bridge->nextCallbackId <= 0)
    bridge->nextCallbackId = 1;
  entry->id = bridge->nextCallbackId++;
  entry->fnHandle = NULL;
  entry->mapHandle = mapHandle;
  entry->next = bridge->callbacks;
  bridge->callbacks = entry;

  LOGI("Registered map callback id=%d method=%s", entry->id, entry->methodName == NULL ? "" : entry->methodName);

  return entry->id;
}

CallbackEntry* find_callback(VM* vm, int callbackId) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return NULL;

  CallbackEntry* it = bridge->callbacks;
  while (it != NULL) {
    if (it->id == callbackId)
      return it;
    it = it->next;
  }

  return NULL;
}

// Invoke a registered callback entry.
// - function callback  : call directly with all Java args converted to Saynaa slots.
// - map/table callback : resolve by runtime method name (exact match), then call if function
// exists. Missing map key is intentionally treated as a no-op.
bool invoke_registered_callback(JNIEnv* env, VM* vm, BridgeState* bridge, CallbackEntry* entry,
    const char* runtimeMethodName, jobjectArray argsArray, jobject* outResult) {
  if (vm == NULL || bridge == NULL || entry == NULL)
    return false;

  if (outResult != NULL)
    *outResult = NULL;

  int argc = 0;
  if (argsArray != NULL)
    argc = (int) (*env)->GetArrayLength(env, argsArray);

  reserveSlots(vm, argc + 8);

  for (int i = 0; i < argc; i++) {
    jobject arg = (*env)->GetObjectArrayElement(env, argsArray, (jsize) i);
    bool ok = object_to_slot(env, vm, bridge, 2 + i,
        create_singleton_array(env, arg, bridge->JavaObjectClass), "Failed to wrap Java argument object.");
    if (arg != NULL)
      (*env)->DeleteLocalRef(env, arg);
    if (!ok)
      return false;
  }

  bool ok = false;
  int argStart = 2;
  int argEnd = 1 + argc;
  int resultSlot = 0;

  if (entry->fnHandle != NULL) {
    setSlotHandle(vm, 1, entry->fnHandle);
    ok = CallFunction(vm, 1, argc, argStart, argEnd);
    resultSlot = 1;
  } else if (entry->mapHandle != NULL) {
    const char* methodKey = runtimeMethodName;
    if (methodKey == NULL || methodKey[0] == '\0')
      methodKey = entry->methodName;

    if (methodKey == NULL || methodKey[0] == '\0') {
      SetRuntimeError(vm, "callback method name is missing.");
      return false;
    }

    int keySlot = argEnd + 1;
    int fnSlot = argEnd + 2;
    setSlotHandle(vm, 1, entry->mapHandle);
    setSlotString(vm, keySlot, methodKey);

    if (CallMethod(vm, 1, "get", 1, keySlot, fnSlot) && GetSlotType(vm, fnSlot) == vCLOSURE) {
      ok = CallFunction(vm, fnSlot, argc, argStart, argEnd);
      resultSlot = fnSlot;
    } else {
      // If the callback method is absent in the map/table, do nothing.
      ok = true;
    }
  }

  if (ok && outResult != NULL && resultSlot > 0) {
    *outResult = slot_to_java(env, vm, bridge, resultSlot);
  }

  return ok;
}

bool invoke_registered_callback_from_slots(JNIEnv* env, VM* vm, BridgeState* bridge, CallbackEntry* entry,
    const char* runtimeMethodName, int argStart, int argCount, jobject* outResult) {
  if (vm == NULL || bridge == NULL || entry == NULL)
    return false;

  if (outResult != NULL)
    *outResult = NULL;

  if (argCount < 0 || argStart < 0) {
    SetRuntimeError(vm, "Invalid callback argument range.");
    return false;
  }

  int argEnd = argCount > 0 ? (argStart + argCount - 1) : (argStart - 1);
  reserveSlots(vm, argEnd + 8);

  bool ok = false;
  int resultSlot = 0;

  if (entry->fnHandle != NULL) {
    setSlotHandle(vm, 1, entry->fnHandle);
    ok = CallFunction(vm, 1, argCount, argStart, argEnd);
    resultSlot = 1;
  } else if (entry->mapHandle != NULL) {
    const char* methodKey = runtimeMethodName;
    if (methodKey == NULL || methodKey[0] == '\0')
      methodKey = entry->methodName;

    if (methodKey == NULL || methodKey[0] == '\0') {
      SetRuntimeError(vm, "callback method name is missing.");
      return false;
    }

    int keySlot = argEnd + 1;
    int fnSlot = argEnd + 2;
    setSlotHandle(vm, 1, entry->mapHandle);
    setSlotString(vm, keySlot, methodKey);

    if (CallMethod(vm, 1, "get", 1, keySlot, fnSlot) && GetSlotType(vm, fnSlot) == vCLOSURE) {
      ok = CallFunction(vm, fnSlot, argCount, argStart, argEnd);
      resultSlot = fnSlot;
    } else {
      ok = true;
    }
  }

  if (ok && outResult != NULL && resultSlot > 0) {
    *outResult = slot_to_java(env, vm, bridge, resultSlot);
  }

  return ok;
}

jobject create_native_callback_proxy(JNIEnv* env, VM* vm, BridgeState* bridge, jstring jInterface,
    const char* methodName, int callbackId) {
  if (env == NULL || vm == NULL || bridge == NULL || bridge->saynaaObject == NULL
      || bridge->mCreateNativeCallbackProxy == NULL || jInterface == NULL) {
    SetRuntimeError(vm, "Native callback proxy bridge is not initialized.");
    return NULL;
  }

  jobject saynaaObj = (*env)->NewLocalRef(env, bridge->saynaaObject);
  if (saynaaObj == NULL) {
    SetRuntimeError(vm, "Failed to access Saynaa object.");
    return NULL;
  }

  // methodName can be a concrete name (SAM/explicit callback) or wildcard "*" for map callbacks.
  jstring jMethod = (*env)->NewStringUTF(env, methodName == NULL ? "*" : methodName);
  jobject proxy = (*env)->CallStaticObjectMethod(env, bridge->javaBridgeClass,
      bridge->mCreateNativeCallbackProxy, saynaaObj, jInterface, jMethod, (jint) callbackId);

  (*env)->DeleteLocalRef(env, jMethod);
  (*env)->DeleteLocalRef(env, saynaaObj);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "native callback proxy creation failed");
    return NULL;
  }

  if (proxy == NULL) {
    SetRuntimeError(vm, "Failed to create native callback proxy.");
    return NULL;
  }

  return proxy;
}

void android_stdout_write(VM* vm, const char* text) {
  (void) vm;
  LOGI("%s", text == NULL ? "" : text);
}

void android_stderr_write(VM* vm, const char* text) {
  LOGE("%s", text == NULL ? "" : text);
  if (vm == NULL || text == NULL || text[0] == '\0')
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL || bridge->jvm == NULL || bridge->saynaaObject == NULL || bridge->mOnNativeError == NULL)
    return;

  JNIEnv* env = env_from_jvm(bridge->jvm);
  if (env == NULL)
    return;

  jobject saynaaObj = (*env)->NewLocalRef(env, bridge->saynaaObject);
  if (saynaaObj == NULL)
    return;

  jstring jText = (*env)->NewStringUTF(env, text);
  if (jText != NULL) {
    (*env)->CallVoidMethod(env, saynaaObj, bridge->mOnNativeError, jText);
    (*env)->DeleteLocalRef(env, jText);
    if ((*env)->ExceptionCheck(env)) {
      clear_jni_exception_with_log(env, "android_stderr_write:onNativeError");
    }
  }

  (*env)->DeleteLocalRef(env, saynaaObj);
}

char* str_dup_c(const char* s) {
  if (s == NULL)
    return NULL;
  size_t n = strlen(s);
  char* out = (char*) malloc(n + 1);
  if (out == NULL)
    return NULL;
  memcpy(out, s, n + 1);
  return out;
}

char* massage_java_classname(const char* name) {
  char* out = str_dup_c(name == NULL ? "" : name);
  if (out == NULL)
    return NULL;

  for (char* p = out; *p != '\0'; p++) {
    if (*p == '_')
      *p = '$';
  }
  return out;
}

jobject bridge_find_class_exact(JNIEnv* env, VM* vm, BridgeState* bridge, const char* className) {
  jstring jName = (*env)->NewStringUTF(env, className == NULL ? "" : className);
  jobject cls = (*env)->CallStaticObjectMethod(env, bridge->javaBridgeClass, bridge->mFindClass, jName);
  (*env)->DeleteLocalRef(env, jName);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "java class resolution failed");
    return NULL;
  }

  return cls;
}

jobject bridge_find_class(JNIEnv* env, VM* vm, BridgeState* bridge, const char* requestedName,
    bool searchPackages, char** resolvedNameOut) {
  if (resolvedNameOut != NULL)
    *resolvedNameOut = NULL;

  if (bridge == NULL || requestedName == NULL || requestedName[0] == '\0')
    return NULL;

  char* massaged = massage_java_classname(requestedName);
  if (massaged == NULL) {
    SetRuntimeError(vm, "Out of memory while resolving Java class name.");
    return NULL;
  }

  bool qualified = strchr(massaged, '.') != NULL;
  if (!searchPackages || qualified) {
    jobject cls = bridge_find_class_exact(env, vm, bridge, massaged);
    if (cls != NULL && resolvedNameOut != NULL)
      *resolvedNameOut = str_dup_c(massaged);
    free(massaged);
    return cls;
  }

  // Package registry support removed; only exact class names are allowed.
  jobject cls = bridge_find_class_exact(env, vm, bridge, massaged);
  if (cls != NULL && resolvedNameOut != NULL)
    *resolvedNameOut = str_dup_c(massaged);
  free(massaged);
  return cls;
}

bool inject_java_global(VM* vm, const char* alias, int slot) {
  if (alias == NULL || alias[0] == '\0')
    return true;

  Module* module = current_module_from_vm(vm);
  Handle* imported = GetSlotHandle(vm, slot);
  if (module == NULL || imported == NULL)
    return false;

  moduleSetGlobal(vm, module, alias, (uint32_t) strlen(alias), imported->value);
  return true;
}

JNIEnv* env_from_jvm(JavaVM* jvm) {
  if (jvm == NULL)
    return NULL;

  JNIEnv* env = NULL;
  if ((*jvm)->GetEnv(jvm, (void**) &env, JNI_VERSION_1_6) == JNI_OK) {
    return env;
  }

  if ((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK) {
    return NULL;
  }

  return env;
}

void throw_if_exception(VM* vm, JNIEnv* env, const char* prefix) {
  if (!(*env)->ExceptionCheck(env))
    return;

  (*env)->ExceptionDescribe(env);
  (*env)->ExceptionClear(env);
  SetRuntimeErrorFmt(vm, "%s (JNI exception).", prefix);
}

void java_ref_destructor(void* ptr) {
  JavaRef* ref = (JavaRef*) ptr;
  if (ref == NULL)
    return;
  if (ref->magic != JAVA_REF_MAGIC) {
    free(ref);
    return;
  }

  JNIEnv* env = env_from_jvm(ref->jvm);
  if (env != NULL && ref->global != NULL) {
    (*env)->DeleteGlobalRef(env, ref->global);
  }

  free(ref);
}

JavaRef* make_java_ref(JNIEnv* env, JavaVM* jvm, jobject obj) {
  if (obj == NULL)
    return NULL;
  JavaRef* ref = (JavaRef*) malloc(sizeof(JavaRef));
  if (ref == NULL)
    return NULL;

  ref->magic = JAVA_REF_MAGIC;
  ref->jvm = jvm;
  ref->global = (*env)->NewGlobalRef(env, obj);

  if (ref->global == NULL) {
    free(ref);
    return NULL;
  }

  return ref;
}

JavaRef* clone_java_ref(JNIEnv* env, JavaRef* src) {
  if (src == NULL || src->global == NULL)
    return NULL;
  jobject local = (*env)->NewLocalRef(env, src->global);
  if (local == NULL)
    return NULL;
  JavaRef* out = make_java_ref(env, src->jvm, local);
  (*env)->DeleteLocalRef(env, local);
  return out;
}

bool ensure_wrapper_classes(VM* vm);

bool create_java_instance(VM* vm, Handle** clsHandlePtr, JavaRef* ref, int outSlot) {
  if (clsHandlePtr == NULL) {
    if (ref != NULL)
      java_ref_destructor(ref);
    SetRuntimeError(vm, "Internal error: class handle pointer is null.");
    return false;
  }

  if (*clsHandlePtr == NULL) {
    if (!ensure_wrapper_classes(vm)) {
      if (ref != NULL)
        java_ref_destructor(ref);
      return false;
    }
  }

  if (*clsHandlePtr == NULL || ref == NULL) {
    SetRuntimeError(vm, "Internal error: Java class handle or ref is null.");
    if (ref != NULL)
      java_ref_destructor(ref);
    return false;
  }

  reserveSlots(vm, 8);
  setSlotHandle(vm, 1, *clsHandlePtr);
  setSlotPointer(vm, 2, ref, NULL);

  if (!NewInstance(vm, 1, outSlot, 1, 2)) {
    java_ref_destructor(ref);
    return false;
  }

  return true;
}

bool create_java_method_instance(VM* vm, JavaRef* target, const char* method_name, bool is_static, int outSlot) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL && bridge->clsJavaMethod == NULL) {
    if (!ensure_wrapper_classes(vm)) {
      if (target != NULL)
        java_ref_destructor(target);
      return false;
    }
  }
  if (bridge == NULL || bridge->clsJavaMethod == NULL || target == NULL || method_name == NULL) {
    SetRuntimeError(vm, "Internal error creating JavaMethod instance.");
    if (target != NULL)
      java_ref_destructor(target);
    return false;
  }

  reserveSlots(vm, 6);
  setSlotHandle(vm, 1, bridge->clsJavaMethod);
  setSlotPointer(vm, 2, target, NULL);
  setSlotString(vm, 3, method_name);
  setSlotBool(vm, 4, is_static);

  if (!NewInstance(vm, 1, outSlot, 3, 2)) {
    java_ref_destructor(target);
    return false;
  }

  return true;
}

jobject slot_to_java(JNIEnv* env, VM* vm, BridgeState* bridge, int slot) {
  if (env == NULL || vm == NULL || bridge == NULL || bridge->javaBridgeClass == NULL
      || bridge->mSlotToJava == NULL || bridge->saynaaObject == NULL) {
    return NULL;
  }

  jobject saynaaObj = (*env)->NewLocalRef(env, bridge->saynaaObject);
  if (saynaaObj == NULL)
    return NULL;

  jobject result = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mSlotToJava, saynaaObj, (jint) slot);
  (*env)->DeleteLocalRef(env, saynaaObj);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "slot_to_java failed");
    return NULL;
  }

  return result;
}

jobject make_args_array(JNIEnv* env, VM* vm, BridgeState* bridge, int startSlot, int argc) {
  if (env == NULL || vm == NULL || bridge == NULL || bridge->javaBridgeClass == NULL
      || bridge->mArgsArrayFromSlots == NULL || bridge->saynaaObject == NULL) {
    return NULL;
  }

  jobject saynaaObj = (*env)->NewLocalRef(env, bridge->saynaaObject);
  if (saynaaObj == NULL)
    return NULL;

  jobjectArray args = (jobjectArray) (*env)->CallStaticObjectMethod(env, bridge->javaBridgeClass,
      bridge->mArgsArrayFromSlots, saynaaObj, (jint) startSlot, (jint) argc);
  (*env)->DeleteLocalRef(env, saynaaObj);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "make_args_array failed");
    return NULL;
  }

  return args;
}

const char* java_simple_name(const char* className) {
  if (className == NULL)
    return NULL;

  const char* lastDot = strrchr(className, '.');
  const char* base = (lastDot == NULL) ? className : (lastDot + 1);

  const char* lastDollar = strrchr(base, '$');
  if (lastDollar != NULL && *(lastDollar + 1) != '\0')
    return lastDollar + 1;

  return base;
}

// Best-effort resolve of the currently executing module for global injection.
Module* current_module_from_vm(VM* vm) {
  if (vm == NULL || vm->fiber == NULL)
    return NULL;

  if (vm->fiber->frame_count <= 0) {
    if (vm->fiber->closure != NULL && vm->fiber->closure->fn != NULL)
      return vm->fiber->closure->fn->owner;
    return NULL;
  }

  CallFrame* frame = &vm->fiber->frames[vm->fiber->frame_count - 1];
  if (frame == NULL || frame->closure == NULL || frame->closure->fn == NULL)
    return NULL;

  return frame->closure->fn->owner;
}

bool ensure_slot_java_class(JNIEnv* env, VM* vm, BridgeState* bridge, int slot, jobject* classObj) {
  if (classObj == NULL)
    return false;

  *classObj = NULL;

  if (GetSlotType(vm, slot) == vSTRING) {
    const char* className = GetSlotString(vm, slot, NULL);
    jobject resolved = bridge_find_class(env, vm, bridge, className, true, NULL);
    if (VM_HAS_ERROR(vm))
      return false;
    if (resolved == NULL) {
      SetRuntimeErrorFmt(vm, "Java class not found: %s", className == NULL ? "" : className);
      return false;
    }
    *classObj = resolved;
    return true;
  }

  if (GetSlotType(vm, slot) != vPOINTER) {
    SetRuntimeError(vm, "Expected a Java class string or class object.");
    return false;
  }

  jobject candidate = slot_to_java(env, vm, bridge, slot);
  if (candidate == NULL)
    return false;

  jclass clsClass = (*env)->FindClass(env, "java/lang/Class");
  if (clsClass == NULL) {
    (*env)->DeleteLocalRef(env, candidate);
    return false;
  }

  jboolean isClass = (*env)->IsInstanceOf(env, candidate, clsClass);
  (*env)->DeleteLocalRef(env, clsClass);
  if (isClass != JNI_TRUE) {
    (*env)->DeleteLocalRef(env, candidate);
    SetRuntimeError(vm, "Expected a Java Class object.");
    return false;
  }

  *classObj = candidate;
  return true;
}

jstring class_name_from_slot(JNIEnv* env, VM* vm, BridgeState* bridge, int slot) {
  if (GetSlotType(vm, slot) == vSTRING) {
    const char* className = GetSlotString(vm, slot, NULL);
    return (*env)->NewStringUTF(env, className == NULL ? "" : className);
  }

  if (GetSlotType(vm, slot) != vPOINTER) {
    SetRuntimeError(vm, "Expected a Java class string or class object.");
    return NULL;
  }

  jobject classObj = slot_to_java(env, vm, bridge, slot);
  if (classObj == NULL)
    return NULL;
  jstring className = get_java_object_name(
      env, vm, bridge, classObj, "class.getName() failed", "Failed to resolve Java class name.");
  (*env)->DeleteLocalRef(env, classObj);
  return className;
}

static jobject make_empty_args(JNIEnv* env) {
  jclass objClass = (*env)->FindClass(env, "java/lang/Object");
  if (objClass == NULL)
    return NULL;
  jobjectArray args = (*env)->NewObjectArray(env, 0, objClass, NULL);
  (*env)->DeleteLocalRef(env, objClass);
  return args;
}

static bool build_java_array_from_list(
    JNIEnv* env, VM* vm, BridgeState* bridge, jobject classObj, int listSlot, jobject* outArray) {
  if (outArray == NULL)
    return false;
  *outArray = NULL;

  if (GetSlotType(vm, listSlot) != vLIST) {
    SetRuntimeError(vm, "java.create array expects a Saynaa List.");
    return false;
  }

  Handle* listHandle = GetSlotHandle(vm, listSlot);
  if (listHandle == NULL)
    return false;

  List* list = (List*) AS_OBJ(listHandle->value);
  releaseHandle(vm, listHandle);

  jclass classClass = (*env)->FindClass(env, "java/lang/Class");
  jmethodID mGetComponentType = (*env)->GetMethodID(env, classClass, "getComponentType", "()Ljava/lang/Class;");
  (*env)->DeleteLocalRef(env, classClass);
  if (mGetComponentType == NULL) {
    SetRuntimeError(vm, "Failed to resolve Class.getComponentType.");
    return false;
  }

  jobject componentType = (*env)->CallObjectMethod(env, classObj, mGetComponentType);
  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "Class.getComponentType failed");
    return false;
  }
  if (componentType == NULL) {
    SetRuntimeError(vm, "Failed to resolve array component type.");
    return false;
  }

  jclass arrayClass = (*env)->FindClass(env, "java/lang/reflect/Array");
  if (arrayClass == NULL) {
    (*env)->DeleteLocalRef(env, componentType);
    return false;
  }

  jmethodID mNewInstance = (*env)->GetStaticMethodID(
      env, arrayClass, "newInstance", "(Ljava/lang/Class;I)Ljava/lang/Object;");
  jmethodID mSet = (*env)->GetStaticMethodID(env, arrayClass, "set", "(Ljava/lang/Object;ILjava/lang/Object;)V");
  if (mNewInstance == NULL || mSet == NULL) {
    (*env)->DeleteLocalRef(env, arrayClass);
    (*env)->DeleteLocalRef(env, componentType);
    SetRuntimeError(vm, "Failed to resolve Array.newInstance/set.");
    return false;
  }

  jint size = (jint) list->elements.count;
  jobject arrayObj = (*env)->CallStaticObjectMethod(env, arrayClass, mNewInstance, componentType, size);
  if ((*env)->ExceptionCheck(env) || arrayObj == NULL) {
    if ((*env)->ExceptionCheck(env))
      throw_if_exception(vm, env, "Array.newInstance failed");
    (*env)->DeleteLocalRef(env, arrayClass);
    (*env)->DeleteLocalRef(env, componentType);
    return false;
  }

  reserveSlots(vm, listSlot + 2);
  int elemSlot = listSlot + 1;
  for (jint i = 0; i < size; i++) {
    Handle* elemHandle = vmNewHandle(vm, list->elements.data[i]);
    if (elemHandle == NULL)
      continue;
    setSlotHandle(vm, elemSlot, elemHandle);
    jobject elemObj = slot_to_java(env, vm, bridge, elemSlot);
    releaseHandle(vm, elemHandle);

    (*env)->CallStaticVoidMethod(env, arrayClass, mSet, arrayObj, i, elemObj);
    if ((*env)->ExceptionCheck(env)) {
      if (elemObj != NULL)
        (*env)->DeleteLocalRef(env, elemObj);
      (*env)->DeleteLocalRef(env, arrayObj);
      (*env)->DeleteLocalRef(env, arrayClass);
      (*env)->DeleteLocalRef(env, componentType);
      throw_if_exception(vm, env, "Array.set failed");
      return false;
    }

    if (elemObj != NULL)
      (*env)->DeleteLocalRef(env, elemObj);
  }

  (*env)->DeleteLocalRef(env, arrayClass);
  (*env)->DeleteLocalRef(env, componentType);
  *outArray = arrayObj;
  return true;
}

static bool populate_java_list(JNIEnv* env, VM* vm, BridgeState* bridge, jobject target, int listSlot) {
  if (GetSlotType(vm, listSlot) != vLIST)
    return true;

  Handle* listHandle = GetSlotHandle(vm, listSlot);
  if (listHandle == NULL)
    return false;
  List* list = (List*) AS_OBJ(listHandle->value);
  releaseHandle(vm, listHandle);

  jclass targetClass = (*env)->GetObjectClass(env, target);
  jmethodID mAdd = (*env)->GetMethodID(env, targetClass, "add", "(Ljava/lang/Object;)Z");
  if (mAdd == NULL) {
    (*env)->DeleteLocalRef(env, targetClass);
    SetRuntimeError(vm, "java.create list missing add(Object).");
    return false;
  }

  reserveSlots(vm, listSlot + 2);
  int elemSlot = listSlot + 1;
  for (uint32_t i = 0; i < list->elements.count; i++) {
    Handle* elemHandle = vmNewHandle(vm, list->elements.data[i]);
    if (elemHandle == NULL)
      continue;
    setSlotHandle(vm, elemSlot, elemHandle);
    jobject elemObj = slot_to_java(env, vm, bridge, elemSlot);
    releaseHandle(vm, elemHandle);

    (*env)->CallBooleanMethod(env, target, mAdd, elemObj);
    if ((*env)->ExceptionCheck(env)) {
      if (elemObj != NULL)
        (*env)->DeleteLocalRef(env, elemObj);
      (*env)->DeleteLocalRef(env, targetClass);
      throw_if_exception(vm, env, "java.create list add failed");
      return false;
    }

    if (elemObj != NULL)
      (*env)->DeleteLocalRef(env, elemObj);
  }

  (*env)->DeleteLocalRef(env, targetClass);
  return true;
}

static bool populate_java_map(JNIEnv* env, VM* vm, BridgeState* bridge, jobject target, int mapSlot) {
  if (GetSlotType(vm, mapSlot) != vMAP)
    return true;

  Handle* mapHandle = GetSlotHandle(vm, mapSlot);
  if (mapHandle == NULL)
    return false;
  Map* map = (Map*) AS_OBJ(mapHandle->value);
  releaseHandle(vm, mapHandle);

  jclass targetClass = (*env)->GetObjectClass(env, target);
  jmethodID mPut = (*env)->GetMethodID(
      env, targetClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
  if (mPut == NULL) {
    (*env)->DeleteLocalRef(env, targetClass);
    SetRuntimeError(vm, "java.create map missing put(Object,Object).");
    return false;
  }

  reserveSlots(vm, mapSlot + 3);
  int keySlot = mapSlot + 1;
  int valueSlot = mapSlot + 2;
  for (uint32_t i = 0; i < map->capacity; i++) {
    MapEntry* entry = &map->entries[i];
    if (IS_UNDEF(entry->key))
      continue;

    Handle* keyHandle = vmNewHandle(vm, entry->key);
    Handle* valueHandle = vmNewHandle(vm, entry->value);
    if (keyHandle == NULL || valueHandle == NULL) {
      if (keyHandle != NULL)
        releaseHandle(vm, keyHandle);
      if (valueHandle != NULL)
        releaseHandle(vm, valueHandle);
      continue;
    }

    setSlotHandle(vm, keySlot, keyHandle);
    setSlotHandle(vm, valueSlot, valueHandle);
    jobject keyObj = slot_to_java(env, vm, bridge, keySlot);
    jobject valueObj = slot_to_java(env, vm, bridge, valueSlot);
    releaseHandle(vm, keyHandle);
    releaseHandle(vm, valueHandle);

    (*env)->CallObjectMethod(env, target, mPut, keyObj, valueObj);
    if ((*env)->ExceptionCheck(env)) {
      if (keyObj != NULL)
        (*env)->DeleteLocalRef(env, keyObj);
      if (valueObj != NULL)
        (*env)->DeleteLocalRef(env, valueObj);
      (*env)->DeleteLocalRef(env, targetClass);
      throw_if_exception(vm, env, "java.create map put failed");
      return false;
    }

    if (keyObj != NULL)
      (*env)->DeleteLocalRef(env, keyObj);
    if (valueObj != NULL)
      (*env)->DeleteLocalRef(env, valueObj);
  }

  (*env)->DeleteLocalRef(env, targetClass);
  return true;
}




// Resolve interface slot into Java class name string expected by JavaBridge APIs.
// Accepts either:
// - string class name: "android.view.View$OnClickListener"
// - Java class object wrapper: bind("android.view.View$OnClickListener")
jstring resolve_proxy_interface_name(
    VM* vm, JNIEnv* env, BridgeState* bridge, int interfaceSlot, const char* errorPrefix) {
  VarType interfaceType = GetSlotType(vm, interfaceSlot);
  if (interfaceType == vSTRING) {
    const char* interfaceName = GetSlotString(vm, interfaceSlot, NULL);
    char* resolvedName = NULL;
    jobject cls = bridge_find_class(env, vm, bridge, interfaceName, true, &resolvedName);
    if (VM_HAS_ERROR(vm))
      return NULL;

    if (cls != NULL) {
      if (cls != NULL)
        (*env)->DeleteLocalRef(env, cls);
      jstring result = (*env)->NewStringUTF(env, resolvedName == NULL ? interfaceName : resolvedName);
      if (resolvedName != NULL)
        free(resolvedName);
      return result;
    }

    if (resolvedName != NULL)
      free(resolvedName);
    return (*env)->NewStringUTF(env, interfaceName == NULL ? "" : interfaceName);
  }

  if (interfaceType != vPOINTER) {
    SetRuntimeError(vm, "createProxy interface must be class object or string.");
    return NULL;
  }

  jobject ifaceObj = slot_to_java(env, vm, bridge, interfaceSlot);
  if (ifaceObj == NULL)
    return NULL;

  jstring classNameObj = get_java_object_name(
      env, vm, bridge, ifaceObj, errorPrefix, "createProxy failed to resolve interface name.");
  (*env)->DeleteLocalRef(env, ifaceObj);
  if (classNameObj == NULL)
    return NULL;

  return classNameObj;
}

bool register_java_wrapper_classes(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return false;

  if (bridge->javaWrapperModule != NULL && bridge->clsJavaClass != NULL
      && bridge->clsJavaObject != NULL && bridge->clsJavaMethod != NULL) {
    return true;
  }

  Handle* mod = NewModule(vm, "java_wrappers");
  if (mod == NULL)
    return false;
  bridge->javaWrapperModule = mod;

  Handle* clsJavaClass = NewClass(vm, "JavaClass", NULL, mod, new_java_class_instance,
      delete_java_class_instance, "Java class wrapper");
  if (clsJavaClass == NULL)
    return false;
  ClassAddMethod(vm, clsJavaClass, "_init", java_class_init, 1, "");
  ClassAddMethod(vm, clsJavaClass, "_call", java_class_call, -1, "");
  ClassAddMethod(vm, clsJavaClass, "_getter", java_class_getter, 1, "");
  ClassAddMethod(vm, clsJavaClass, "_str", java_class_str, 0, "");
  bridge->clsJavaClass = clsJavaClass;

  Handle* clsJavaObject = NewClass(vm, "JavaObject", NULL, mod, new_java_object_instance,
      delete_java_object_instance, "Java object wrapper");
  if (clsJavaObject == NULL)
    return false;
  ClassAddMethod(vm, clsJavaObject, "_init", java_object_init, 1, "");
  ClassAddMethod(vm, clsJavaObject, "_getter", java_object_getter, 1, "");
  ClassAddMethod(vm, clsJavaObject, "_setter", java_object_setter, 2, "");
  ClassAddMethod(vm, clsJavaObject, "_str", java_object_str, 0, "");
  bridge->clsJavaObject = clsJavaObject;

  Handle* clsJavaMethod = NewClass(vm, "JavaMethod", NULL, mod, new_java_method_instance,
      delete_java_method_instance, "Java method wrapper");
  if (clsJavaMethod == NULL)
    return false;
  ClassAddMethod(vm, clsJavaMethod, "_init", java_method_init, 3, "");
  ClassAddMethod(vm, clsJavaMethod, "_call", java_method_call, -1, "");
  ClassAddMethod(vm, clsJavaMethod, "_str", java_method_str, 0, "");
  bridge->clsJavaMethod = clsJavaMethod;

  registerModule(vm, mod);
  return true;
}

bool ensure_wrapper_classes(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return false;
  if (bridge->javaWrapperModule != NULL && bridge->clsJavaClass != NULL
      && bridge->clsJavaObject != NULL && bridge->clsJavaMethod != NULL) {
    return true;
  }
  return register_java_wrapper_classes(vm);
}
