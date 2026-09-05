#include "saynaa_internal.h"

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

void clear_pinned_handles(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return;

  PinnedHandleEntry* it = bridge->pinnedHandles;
  while (it != NULL) {
    PinnedHandleEntry* next = it->next;
    if (it->handle != NULL)
      releaseHandle(vm, it->handle);
    free(it);
    it = next;
  }

  bridge->pinnedHandles = NULL;
  bridge->nextPinnedHandleId = 1;
}

int register_pinned_handle(VM* vm, Handle* handle) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL || handle == NULL)
    return 0;

  PinnedHandleEntry* entry = (PinnedHandleEntry*) calloc(1, sizeof(PinnedHandleEntry));
  if (entry == NULL) {
    releaseHandle(vm, handle);
    SetRuntimeError(vm, "Out of memory while pinning handle.");
    return 0;
  }

  if (bridge->nextPinnedHandleId <= 0)
    bridge->nextPinnedHandleId = 1;

  entry->id = bridge->nextPinnedHandleId++;
  entry->handle = handle;
  entry->next = bridge->pinnedHandles;
  bridge->pinnedHandles = entry;

  return entry->id;
}

Handle* find_pinned_handle(VM* vm, int handleId) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL || handleId <= 0)
    return NULL;

  PinnedHandleEntry* it = bridge->pinnedHandles;
  while (it != NULL) {
    if (it->id == handleId)
      return it->handle;
    it = it->next;
  }

  return NULL;
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

  int argStart = allocSlot(vm, argc);

  for (int i = 0; i < argc; i++) {
    jobject arg = (*env)->GetObjectArrayElement(env, argsArray, (jsize) i);
    bool ok = object_to_slot(env, vm, bridge, argStart + i, arg, "Failed to wrap Java argument object.");
    if (arg != NULL)
      (*env)->DeleteLocalRef(env, arg);
    if (!ok)
      return false;
  }

  bool ok = false;
  int resultSlot = nextSlot(vm, false);

  int slot1 = nextSlot(vm, false);

  if (entry->fnHandle != NULL) {
    setSlotHandle(vm, slot1, entry->fnHandle);
    ok = CallFunction(vm, slot1, argc, argStart, resultSlot);
  } else if (entry->mapHandle != NULL) {
    const char* methodKey = runtimeMethodName;
    if (methodKey == NULL || methodKey[0] == '\0')
      methodKey = entry->methodName;

    if (methodKey == NULL || methodKey[0] == '\0') {
      SetRuntimeError(vm, "callback method name is missing.");
      return false;
    }

    int keySlot = nextSlot(vm, false);
    int fnSlot = nextSlot(vm, false);

    int slot5 = nextSlot(vm, false);

    setSlotHandle(vm, slot5, entry->mapHandle);
    setSlotString(vm, keySlot, methodKey);

    if (CallMethod(vm, slot5, "get", 1, keySlot, fnSlot) && GetSlotType(vm, fnSlot) == vCLOSURE) {
      ok = CallFunction(vm, fnSlot, argc, argStart, resultSlot);
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
  int resultSlot = nextSlot(vm, false);

  if (entry->fnHandle != NULL) {
    int slot1 = nextSlot(vm, false);
    setSlotHandle(vm, slot1, entry->fnHandle);
    ok = CallFunction(vm, slot1, argCount, argStart, resultSlot);
  } else if (entry->mapHandle != NULL) {
    const char* methodKey = runtimeMethodName;
    if (methodKey == NULL || methodKey[0] == '\0')
      methodKey = entry->methodName;

    if (methodKey == NULL || methodKey[0] == '\0') {
      SetRuntimeError(vm, "callback method name is missing.");
      return false;
    }

    int keySlot = nextSlot(vm, false);
    int fnSlot = nextSlot(vm, false);
    int slot5 = nextSlot(vm, false);

    setSlotHandle(vm, slot5, entry->mapHandle);
    setSlotString(vm, keySlot, methodKey);

    if (CallMethod(vm, slot5, "get", 1, keySlot, fnSlot) && GetSlotType(vm, fnSlot) == vCLOSURE) {
      ok = CallFunction(vm, fnSlot, argCount, argStart, resultSlot);
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
  //LOGE("%s", text == NULL ? "" : text);
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
    LOGD("Deleting global reference for JavaRef %p", (void*) ref->global);
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
    SetRuntimeError(vm, "Internal error: class handle pointer is null.");
    return false;
  }

  if (*clsHandlePtr == NULL) {
    if (!ensure_wrapper_classes(vm)) {
      return false;
    }
  }

  if (*clsHandlePtr == NULL || ref == NULL) {
    SetRuntimeError(vm, "Internal error: Java class handle or ref is null.");
    return false;
  }

  int slot1 = nextSlot(vm, true);
  int slot2 = nextSlot(vm, true);

  LOGI("Creating Java instance: clsHandle=%p, ref=%p, outSlot=%d, slot1=%d, slot2=%d",
      (void*) *clsHandlePtr, (void*) ref, outSlot, slot1, slot2);

  setSlotHandle(vm, slot1, *clsHandlePtr);
  setSlotPointer(vm, slot2, ref, NULL);

  bool state = true;

  if (!NewInstance(vm, slot1, outSlot, 1, slot2)) {
    state = false;
    goto L_return;
  }

  newHandle(vm, SLOT(outSlot));

L_return:

  freeSlot(vm, slot1, 1);
  freeSlot(vm, slot2, 1);

  return state;
}

bool create_java_method_instance(VM* vm, JavaRef* target, const char* method_name, bool is_static, int outSlot) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL && bridge->clsJavaMethod == NULL) {
    if (!ensure_wrapper_classes(vm)) {
      return false;
    }
  }
  if (bridge == NULL || bridge->clsJavaMethod == NULL || target == NULL || method_name == NULL) {
    SetRuntimeError(vm, "Internal error creating JavaMethod instance.");
    return false;
  }

  reserveSlots(vm, 6);

  int slot1 = nextSlot(vm, true);
  int slot2 = nextSlot(vm, false);
  int slot3 = nextSlot(vm, false);
  int slot4 = nextSlot(vm, false);

  LOGI("Creating Java method instance: clsshndle: %p, target: %p, method: %s, is_static: %d, "
       "outSlot: %d, slot1: %d, slot2: %d, slot3: %d, slot4: %d",
      (void*) bridge->clsJavaMethod, (void*) target, method_name == NULL ? "" : method_name,
      is_static ? 1 : 0, outSlot, slot1, slot2, slot3, slot4);

  setSlotHandle(vm, slot1, bridge->clsJavaMethod);
  setSlotPointer(vm, slot2, target, NULL);
  setSlotString(vm, slot3, method_name);
  setSlotBool(vm, slot4, is_static);

  bool state = true;

  if (!NewInstance(vm, slot1, outSlot, 3, slot2)) {
    state = false;
    goto L_return;
  }
  LOGI("Java method instance: done");

  newHandle(vm, SLOT(outSlot));

L_return:

  freeSlot(vm, slot1, 1);
  freeSlot(vm, slot2, 1);
  freeSlot(vm, slot3, 1);
  freeSlot(vm, slot4, 1);

  return state;
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

  Handle* clsJavaBase = NewClass(vm, "JavaBase", NULL, mod, NULL, NULL, "Java base wrapper");
  if (clsJavaBase == NULL)
    return false;
  bridge->clsJavaBase = clsJavaBase;

  Handle* clsJavaClass = NewClass(vm, "JavaClass", clsJavaBase, mod, new_java_class_instance,
      delete_java_instance, "Java class wrapper");
  if (clsJavaClass == NULL)
    return false;
  ClassAddMethod(vm, clsJavaClass, "_init", java_init, 1, "");
  ClassAddMethod(vm, clsJavaClass, "_call", java_class_call, -1, "");
  ClassAddMethod(vm, clsJavaClass, "_getter", java_class_getter, 1, "");
  ClassAddMethod(vm, clsJavaClass, "_str", java_class_str, 0, "");
  bridge->clsJavaClass = clsJavaClass;

  Handle* clsJavaObject = NewClass(vm, "JavaObject", clsJavaBase, mod, new_java_object_instance,
      delete_java_instance, "Java object wrapper");
  if (clsJavaObject == NULL)
    return false;
  ClassAddMethod(vm, clsJavaObject, "_init", java_init, 1, "");
  ClassAddMethod(vm, clsJavaObject, "_getter", java_object_getter, 1, "");
  ClassAddMethod(vm, clsJavaObject, "_setter", java_object_setter, 2, "");
  ClassAddMethod(vm, clsJavaObject, "_str", java_object_str, 0, "");
  bridge->clsJavaObject = clsJavaObject;

  Handle* clsJavaMethod = NewClass(vm, "JavaMethod", clsJavaBase, mod, new_java_method_instance,
      delete_java_instance, "Java method wrapper");
  if (clsJavaMethod == NULL)
    return false;
  ClassAddMethod(vm, clsJavaMethod, "_init", java_init, 3, "");
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
