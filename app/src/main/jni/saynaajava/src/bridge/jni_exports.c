#include "saynaa_exports.h"
#include "saynaa_internal.h"
#include "saynaa_jni.h"

static Result run_string_pcall(VM* vm, const char* code) {
  if (vm == NULL || code == NULL)
    return RESULT_RUNTIME_ERROR;
  return RunStringPcall(vm, code);
}

saynaa_function(_debug, "debug(msg:Var) -> Null", "Print the string representation of msg to logcat with INFO level.") {
  const char* s;
  if (!ValidateSlotString(vm, 1, &s, NULL))
    return;
  __android_log_print(ANDROID_LOG_INFO, "saynaadebug", "%s", s == NULL ? "" : s);
}

JNIEXPORT jlong JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1open(JNIEnv* env, jobject thiz) {
  JavaVM* jvm = NULL;
  if ((*env)->GetJavaVM(env, &jvm) != JNI_OK) {
    return 0;
  }

  Configuration config = NewConfiguration();
  config.stdout_write = android_stdout_write;
  config.stderr_write = android_stderr_write;

  VM* vm = NewVM(&config);
  if (vm == NULL) {
    return 0;
  }

  RegisterBuiltinFn(vm, "debug", _debug, 1, DOCSTRING(_debug));

  BridgeState* bridge = (BridgeState*) calloc(1, sizeof(BridgeState));
  if (bridge == NULL) {
    FreeVM(vm);
    return 0;
  }

  bridge->jvm = jvm;
  bridge->saynaaObject = (*env)->NewGlobalRef(env, thiz);
  if (bridge->saynaaObject == NULL) {
    free(bridge);
    FreeVM(vm);
    return 0;
  }

  jclass localBridgeClass = (*env)->FindClass(env, "com/android/saynaa/saynaajava/JavaBridge");
  if (localBridgeClass == NULL) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    free(bridge);
    FreeVM(vm);
    return 0;
  }

  bridge->javaBridgeClass = (jclass) (*env)->NewGlobalRef(env, localBridgeClass);
  (*env)->DeleteLocalRef(env, localBridgeClass);

  bridge->mFindClass = (*env)->GetStaticMethodID(
      env, bridge->javaBridgeClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
  bridge->mCreateJavaObject = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createJavaObject", "(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
  bridge->mCallJavaMethod = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "callJavaMethod",
      "(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
  bridge->mCallStaticJavaMethod = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "callStaticJavaMethod", "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
  bridge->mGetFieldValue = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "getFieldValue",
      "(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;");
  bridge->mSetFieldValue = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "setFieldValue",
      "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/Object;)Z");
  bridge->mResolveCallbackInterface = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "resolveCallbackInterface", "(Ljava/lang/Object;Ljava/lang/String;II)Ljava/lang/String;");
  bridge->mCreateProxy = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createProxy", "(Lcom/android/saynaa/saynaajava/Saynaa;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;");
  bridge->mCreateNativeCallbackProxy = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createNativeCallbackProxy", "(Lcom/android/saynaa/saynaajava/Saynaa;Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/Object;");
  bridge->mGetDefaultInterfaceMethodName = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "getDefaultInterfaceMethodName", "(Ljava/lang/String;)Ljava/lang/String;");
  bridge->mPushToSlot = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "pushToSlot",
      "(Lcom/android/saynaa/saynaajava/Saynaa;ILjava/lang/Object;)Z");
  bridge->mSlotToJava = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "slotToJava",
      "(Lcom/android/saynaa/saynaajava/Saynaa;I)Ljava/lang/Object;");
  bridge->mArgsArrayFromSlots = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "argsFromSlots", "(Lcom/android/saynaa/saynaajava/Saynaa;II)[Ljava/lang/Object;");

  jclass saynaaClass = (*env)->GetObjectClass(env, thiz);
  bridge->mOnNativeError = (*env)->GetMethodID(env, saynaaClass, "onNativeError", "(Ljava/lang/String;)V");
  (*env)->DeleteLocalRef(env, saynaaClass);

  if (bridge->mFindClass == NULL || bridge->mCreateJavaObject == NULL || bridge->mCallJavaMethod == NULL
      || bridge->mCallStaticJavaMethod == NULL || bridge->mGetFieldValue == NULL || bridge->mSetFieldValue == NULL
      || bridge->mResolveCallbackInterface == NULL || bridge->mCreateProxy == NULL
      || bridge->mCreateNativeCallbackProxy == NULL || bridge->mGetDefaultInterfaceMethodName == NULL
      || bridge->mPushToSlot == NULL || bridge->mSlotToJava == NULL
      || bridge->mArgsArrayFromSlots == NULL || bridge->mOnNativeError == NULL) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    (*env)->DeleteGlobalRef(env, bridge->javaBridgeClass);
    free(bridge);
    FreeVM(vm);
    return 0;
  }

  SetUserData(vm, bridge);
  if (!register_java_wrapper_classes(vm)) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    (*env)->DeleteGlobalRef(env, bridge->javaBridgeClass);
    free(bridge);
    SetUserData(vm, NULL);
    FreeVM(vm);
    return 0;
  }

  return (jlong) vm;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1chdir(
    JNIEnv* env, jobject thiz, jstring path) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return -1;

  const char* pathChars = (*env)->GetStringUTFChars(env, path, NULL);
  if (pathChars == NULL)
    return -1;

  int result = chdir(pathChars);

  (*env)->ReleaseStringUTFChars(env, path, pathChars);

  return result;
}

// saynaa_getSlotCount
JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getSlotCount(JNIEnv* env, jobject thiz) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return 0;

  return (jint) (vm->fiber->sp - vm->fiber->ret);
}

JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getGlobal(
    JNIEnv* env, jobject thiz, jint handleId, jstring name) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || name == NULL)
    return NULL;

  BridgeState* bridge = bridge_from_vm(vm);
  const char* key = (*env)->GetStringUTFChars(env, name, NULL);
  if (key == NULL)
    return NULL;
  LOGI("saynaa_getGlobal: handleId=%d, name=%s", handleId, key);

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return NULL;
  }
  if (!IS_OBJ_TYPE(handle->value, OBJ_MODULE)) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return NULL;
  }
  Module* module = (Module*) AS_OBJ(handle->value);

  int idx = moduleGetGlobalIndex(module, key, (uint32_t) strlen(key));
  (*env)->ReleaseStringUTFChars(env, name, key);
  if (idx < 0)
    return NULL;

  reserveSlots(vm, 2);
  int slot1 = nextSlot(vm, true);

  Handle* handle2 = newHandle(vm, module->context->globals.data[idx]);
  if (handle2 == NULL)
    return NULL;

  setSlotHandle(vm, slot1, handle2);
  jobject resultValue = slot_to_java(env, vm, bridge, slot1);
  freeSlot(vm, slot1, 1);
  return resultValue;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getGlobalId(
    JNIEnv* env, jobject thiz, jint handleId, jstring name) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || name == NULL)
    return (jint) -1;

  BridgeState* bridge = bridge_from_vm(vm);
  const char* key = (*env)->GetStringUTFChars(env, name, NULL);
  if (key == NULL)
    return (jint) -1;

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return (jint) -1;
  }
  if (!IS_OBJ_TYPE(handle->value, OBJ_MODULE)) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return (jint) -1;
  }
  Module* module = (Module*) AS_OBJ(handle->value);

  int idx = moduleGetGlobalIndex(module, key, (uint32_t) strlen(key));
  (*env)->ReleaseStringUTFChars(env, name, key);
  find_pinned_handle(vm, handleId);
  return (jint) idx;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getGlobalFunctionId(
    JNIEnv* env, jobject thiz, jint handleId, jstring name) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || name == NULL)
    return (jint) -1;

  BridgeState* bridge = bridge_from_vm(vm);
  const char* key = (*env)->GetStringUTFChars(env, name, NULL);
  if (key == NULL)
    return (jint) -1;

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return (jint) -1;
  }

  /*
    08-29 10:56:45.653 45529 45529 F DEBUG   : Cmdline: com.android.saynaa
    08-29 10:56:45.653 45529 45529 F DEBUG   : pid: 45529, tid: 45529, name: .android.saynaa  >>> com.android.saynaa <<<
    08-29 10:56:45.653 45529 45529 F DEBUG   :       #00 pc 000000000005d7f9 /data/app/~~urhwRxRqISZ--iCP1OBjTQ==/com.android.saynaa-D81I3IhmIM-iInTzsFK_DQ==/lib/x86_64/libsaynaajava.so (Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getGlobalFunctionId+265) (BuildId: 90ffa0777f7b8447a2385d289b021e5ec98541bd)
    08-29 10:56:45.654 45529 45529 F DEBUG   :       #47 pc 00000000000e1e11 /system/lib64/libandroid_runtime.so (android::AndroidRuntime::start(char const*, android::Vector<android::String8> const&, bool)+897) (BuildId: e21d037b5951e3febdd9cd88307c86ae)
  */

  LOGI("[TRACE] BEFORE value");

  Var value = handle->value;
  LOGI("[TRACE] AFTER value");
  LOGI("[TRACE] BEFORE value is NULL %d, %p", value == NULL, AS_OBJ(value));

  LOGI("ad: handleId=%d, name=%s", handleId, key);
  LOGI("ad: handle: %p", (void*) handle);

  LOGI("[TRACE] AFTER value %s", varTypeName(value));

  TRACE("getGlobalFunctionId ENTER vm=%p handleId=%d", (void*) vm, handleId);

  if (!IS_OBJ_TYPE(handle->value, OBJ_MODULE)) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return (jint) -1;
  }

  Module* module = (Module*) AS_OBJ(handle->value);

  int idx = moduleGetGlobalIndex(module, key, (uint32_t) strlen(key));

  if (idx == -1) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return (jint) idx;
  }

  int slot1 = nextSlot(vm, true);

  Var var = module->context->globals.data[idx];
  vm->fiber->ret[slot1] = var;

  idx = IS_OBJ_TYPE(var, OBJ_CLOSURE) ? idx : -1;

  LOGI("varTypeName: %s, %d", varTypeName(var), idx);

  freeSlot(vm, slot1, 1);

  (*env)->ReleaseStringUTFChars(env, name, key);
  return (jint) idx;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1callFunctionById(JNIEnv* env,
    jobject thiz, jint handleId, jint functionId, jint argStart, jint argCount, jint retSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || functionId < 0)
    return JNI_FALSE;
  if (argStart < 0 || argCount < 0)
    return JNI_FALSE;

  LOGD("saynaa_callFunctionById: handleId=%d, functionId=%d, argStart=%d, argCount=%d, retSlot=%d",
      handleId, functionId, argStart, argCount, retSlot);

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL) {
    LOGE("saynaa_callFunctionById: pinned handle not found: id: %d", handleId);
    return JNI_FALSE;
  }
  if (!IS_OBJ_TYPE(handle->value, OBJ_MODULE)) {
    LOGE("saynaa_callFunctionById: pinned handle is not a module, id: %d, its %s", handleId,
        varTypeName(handle->value));
    return JNI_FALSE;
  }
  Module* module = (Module*) AS_OBJ(handle->value);

  if (functionId >= (jint) module->context->globals.count)
    return JNI_FALSE;

  int needed = argStart + argCount;
  if (retSlot >= needed)
    needed = retSlot + 1;
  if (needed < 1)
    needed = 1;
  reserveSlots(vm, needed);

  int slot1 = nextSlot(vm, false);

  vm->fiber->ret[slot1] = module->context->globals.data[functionId];
  jboolean ok = CallFunction(vm, slot1, (int) argCount, (int) argStart, (int) retSlot) ? JNI_TRUE : JNI_FALSE;
  freeSlot(vm, slot1, 1);
  return ok;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1newInstance(
    JNIEnv* env, jobject thiz, jint pinnedHandleId, jint argStart, jint argCount, jint retSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;
  if (argStart < 0 || argCount < 0)
    return JNI_FALSE;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return JNI_FALSE;

  Handle* handle = find_pinned_handle(vm, pinnedHandleId);
  if (handle == NULL) {
    LOGE("saynaa_newInstance: pinned handle not found: id: %d", pinnedHandleId);
    return JNI_FALSE;
  }

  if (!IS_OBJ_TYPE(handle->value, OBJ_CLASS)) {
    LOGE("saynaa_newInstance: pinned handle is not a class, id: %d, its %s", pinnedHandleId,
        varTypeName(handle->value));
    return JNI_FALSE;
  }

  int needed = argStart + argCount;
  if (retSlot >= needed)
    needed = retSlot + 1;
  if (needed < 1)
    needed = 1;
  reserveSlots(vm, needed);

  int slot1 = nextSlot(vm, false);

  vm->fiber->ret[slot1] = handle->value;
  jboolean ok = NewInstance(vm, slot1, (int) retSlot, (int) argCount, (int) argStart) ? JNI_TRUE : JNI_FALSE;

  LOGD("saynaa_newInstance: NewInstance returned %d, retSlot=%d, object type: %d", ok, retSlot,
      AS_OBJ(vm->fiber->ret[retSlot])->type);
  freeSlot(vm, slot1, 1);
  return ok;
}

// CallMethod
JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1callMethod(JNIEnv* env,
    jobject thiz, jint handleId, jstring methodName, jint argStart, jint argCount, jint retSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;
  if (argStart < 0 || argCount < 0)
    return JNI_FALSE;
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return JNI_FALSE;

  LOGD("saynaa_callMethod: handleId=%d, methodName=%s, argStart=%d, argCount=%d, retSlot=%d", handleId,
      methodName ? (*env)->GetStringUTFChars(env, methodName, NULL) : "NULL", argStart, argCount, retSlot);

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL) {
    LOGE("saynaa_callMethod: pinned handle not found: id: %d", handleId);
    return JNI_FALSE;
  }

  if (methodName == NULL) {
    LOGE("saynaa_callMethod: methodName is NULL");
    return JNI_FALSE;
  }

  const char* methodNameChars = (*env)->GetStringUTFChars(env, methodName, NULL);
  if (methodNameChars == NULL) {
    LOGE("saynaa_callMethod: methodNameChars is NULL");
    return JNI_FALSE;
  }

  int needed = argStart + argCount;
  if (retSlot >= needed)
    needed = retSlot + 1;
  if (needed < 1)
    needed = 1;
  reserveSlots(vm, needed);

  int slot1 = nextSlot(vm, false);

  vm->fiber->ret[slot1] = handle->value;
  jboolean ok = CallMethod(vm, slot1, methodNameChars, (int) argCount, (int) argStart, (int) retSlot)
                    ? JNI_TRUE
                    : JNI_FALSE;

  freeSlot(vm, slot1, 1);
  (*env)->ReleaseStringUTFChars(env, methodName, methodNameChars);
  return ok;
}
JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1objGetattr(
    JNIEnv* env, jobject thiz, jint handleId, jstring name, jboolean skipGetter) {
  VM* vm = vm_from_saynaa(env, thiz);
  LOGD("saynaa_objGetattr called with handleId=%d, name=%s, skipGetter=%d", handleId,
      name ? (*env)->GetStringUTFChars(env, name, NULL) : "NULL", skipGetter);
  if (vm == NULL || name == NULL)
    return NULL;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return NULL;

  LOGD("saynaa_objGetattr: handleId=%d, name=%s, skipGetter=%d", handleId,
      name ? (*env)->GetStringUTFChars(env, name, NULL) : "NULL", skipGetter);

  reserveSlots(vm, 2);
  int slot1 = nextSlot(vm, true);

  const char* key = (*env)->GetStringUTFChars(env, name, NULL);
  if (key == NULL)
    return NULL;

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL) {
    LOGE("saynaa_objGetattr: pinned handle not found: id: %d", handleId);
    (*env)->ReleaseStringUTFChars(env, name, key);
    return NULL;
  }
  Var objVar = handle->value;
  Var resultVar = varGetAttrib(vm, objVar, newStringLength(vm, key, strlen(key)), skipGetter == JNI_TRUE, false);
  LOGD("saynaa_objGetattr: result type: %s", varTypeName(resultVar));

  (*env)->ReleaseStringUTFChars(env, name, key);

  vm->fiber->ret[slot1] = resultVar;
  jobject resultValue = slot_to_java(env, vm, bridge, slot1);
  freeSlot(vm, slot1, 1);
  return resultValue;
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1reserveSlots(
    JNIEnv* env, jobject thiz, jint count) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;
  if (count < 0)
    return;
  reserveSlots(vm, count);
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setSlotNull(
    JNIEnv* env, jobject thiz, jint slot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;
  reserveSlots(vm, slot + 1);
  setSlotNull(vm, slot);
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setSlotBool(
    JNIEnv* env, jobject thiz, jint slot, jboolean value) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;
  reserveSlots(vm, slot + 1);
  setSlotBool(vm, slot, value == JNI_TRUE);
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setSlotNumber(
    JNIEnv* env, jobject thiz, jint slot, jdouble value) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;
  reserveSlots(vm, slot + 1);
  setSlotNumber(vm, slot, (double) value);
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setSlotString(
    JNIEnv* env, jobject thiz, jint slot, jstring value) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;
  reserveSlots(vm, slot + 1);
  if (value == NULL) {
    setSlotNull(vm, slot);
    return;
  }
  const char* text = (*env)->GetStringUTFChars(env, value, NULL);
  if (text == NULL)
    return;
  setSlotString(vm, slot, text);
  (*env)->ReleaseStringUTFChars(env, value, text);
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setSlotHandle(
    JNIEnv* env, jobject thiz, jint slot, jint handleId) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;
  reserveSlots(vm, slot + 1);

  Handle* handle = GetSlotHandle(vm, handleId);
  if (handle == NULL)
    return;
  setSlotHandle(vm, slot, handle);
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1captureSlotHandle(
    JNIEnv* env, jobject thiz, jint slot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || slot < 0)
    return 0;

  reserveSlots(vm, slot + 1);
  Handle* handle = GetSlotHandle(vm, slot);
  if (handle == NULL) {
    LOGE("saynaa_captureSlotHandle: no handle found in slot %d", slot);
    return 0;
  }

  int handleId = register_pinned_handle(vm, handle);
  LOGD("saynaa_captureSlotHandle: captured handle %p, id: %d, from slot %d, type: %s",
      (void*) handle, handleId, slot, varTypeName(handle->value));
  return (jint) handleId;
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setSlotPinnedHandle(
    JNIEnv* env, jobject thiz, jint slot, jint pinnedHandleId) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || pinnedHandleId <= 0)
    return;

  reserveSlots(vm, slot + 1);
  Handle* handle = find_pinned_handle(vm, (int) pinnedHandleId);
  if (handle == NULL)
    return;

  setSlotHandle(vm, slot, handle);
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1newList(JNIEnv* env, jobject thiz) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return -1;
  Handle* handle = newHandle(vm, VAR_OBJ(newList(vm, 0)));
  if (handle == NULL)
    return -1;
  return (jint) register_pinned_handle(vm, handle);
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1nextSlot(JNIEnv* env, jobject thiz) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return -1;
  int slot = nextSlot(vm, false);
  if (slot < 0)
    return -1;
  return slot;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1allocSlot(
    JNIEnv* env, jobject thiz, jint count) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return -1;
  if (count <= 0)
    return -1;
  int slot = allocSlot(vm, count);
  if (slot < 0)
    return -1;
  return slot;
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1freeSlot(
    JNIEnv* env, jobject thiz, jint slot, jint count) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;

  freeSlot(vm, slot, count);
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1newMap(JNIEnv* env, jobject thiz) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return -1;
  Handle* handle = newHandle(vm, VAR_OBJ(newMap(vm)));
  if (handle == NULL)
    return -1;

  return (jint) register_pinned_handle(vm, handle);
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1newModule(
    JNIEnv* env, jobject thiz, jstring name) {
  VM* vm = vm_from_saynaa(env, thiz);
  BridgeState* bridge = bridge_from_vm(vm);

  if (vm == NULL || name == NULL)
    return -1;
  const char* nameChars = (*env)->GetStringUTFChars(env, name, NULL);
  if (nameChars == NULL)
    return -1;
  Handle* module = NewModule(vm, nameChars);
  (*env)->ReleaseStringUTFChars(env, name, nameChars);
  if (module == NULL)
    return -1;

  LOGD("saynaa_newModule: created module %p, name: %s", (void*) module, nameChars);
  LOGI("saynaa_newModule: module %p", (void*) AS_OBJ(module->value));

  int result = register_pinned_handle(vm, module);
  LOGD("saynaa_newModule: registered module handle %p, id: %d", (void*) module, result);
  return result;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1registerModule(
    JNIEnv* env, jobject thiz, jint pinnedHandleId) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;

  Handle* handle = find_pinned_handle(vm, (int) pinnedHandleId);
  if (handle == NULL)
    return JNI_FALSE;

  registerModule(vm, handle);
  return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1moduleSetGlobal(
    JNIEnv* env, jobject thiz, jint pinnedHandleId, jstring name, jobject value) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || name == NULL)
    return JNI_FALSE;

  BridgeState* bridge = bridge_from_vm(vm);
  const char* key = (*env)->GetStringUTFChars(env, name, NULL);
  if (key == NULL)
    return JNI_FALSE;

  Handle* handle = find_pinned_handle(vm, (int) pinnedHandleId);
  if (handle == NULL) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return JNI_FALSE;
  }

  Module* module = (Module*) AS_OBJ(handle->value);
  if (module == NULL) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return JNI_FALSE;
  }

  int slot1 = nextSlot(vm, true);

  if (!object_to_slot(env, vm, bridge, slot1, value, "Failed to wrap Java value object.")) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    freeSlot(vm, slot1, 1);
    return JNI_FALSE;
  }

  moduleSetGlobal(vm, module, key, (uint32_t) strlen(key), SLOT(slot1));

  (*env)->ReleaseStringUTFChars(env, name, key);
  freeSlot(vm, slot1, 1);
  return JNI_TRUE;
}

// saynaa_addSearchPath
JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1addSearchPath(
    JNIEnv* env, jobject thiz, jstring path) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || path == NULL)
    return JNI_FALSE;

  const char* pathChars = (*env)->GetStringUTFChars(env, path, NULL);
  if (pathChars == NULL)
    return JNI_FALSE;

  AddSearchPath(vm, pathChars);
  (*env)->ReleaseStringUTFChars(env, path, pathChars);

  return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1runFile(
    JNIEnv* env, jobject thiz, jint handleId, jstring path) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || path == NULL)
    return RESULT_RUNTIME_ERROR;

  const char* pathChars = (*env)->GetStringUTFChars(env, path, NULL);
  if (pathChars == NULL)
    return RESULT_RUNTIME_ERROR;

  LOGD("saynaa_runFile: handleId=%d, path=%s", handleId, pathChars);

  Handle* handle = find_pinned_handle(vm, (int) handleId);
  if (handle == NULL) {
    (*env)->ReleaseStringUTFChars(env, path, pathChars);
    return JNI_FALSE;
  }
  Module* module = (Module*) AS_OBJ(handle->value);
  jint result = (jint) RunFileWithModule(vm, module, pathChars);
  (*env)->ReleaseStringUTFChars(env, path, pathChars);
  return result;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1listInsert(
    JNIEnv* env, jobject thiz, jint handleId, jint index, jint valueSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL)
    return JNI_FALSE;

  int slot1 = nextSlot(vm, true);
  SLOT(slot1) = handle->value;

  jboolean result = ListInsert(vm, handleId, index, valueSlot) ? JNI_TRUE : JNI_FALSE;
  freeSlot(vm, slot1, 1);
  return result;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1listReplace(
    JNIEnv* env, jobject thiz, jint handleId, jint index, jint valueSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL)
    return JNI_FALSE;

  if (!IS_OBJ_TYPE(handle->value, OBJ_LIST))
    return JNI_FALSE;

  List* list = (List*) AS_OBJ(handle->value);
  VarBuffer* elems = &list->elements;

  // Normalize index.
  if (index < 0)
    index = elems->count + index;
  if (index < 0) {
    VM_SET_ERROR(vm, newString(vm, "List index out of bound."));
    return JNI_FALSE;
  }

  if (index >= elems->count) {
    VM_SET_ERROR(vm, newString(vm, "List index out of bound."));
    return JNI_FALSE;
  }

  elems->data[index] = SLOT(valueSlot);

  return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1mapSet(
    JNIEnv* env, jobject thiz, jint handleId, jint keySlot, jint valueSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL) {
    return JNI_FALSE;
  }

  // Validate slots
  if (keySlot < 0 || valueSlot < 0) {
    return JNI_FALSE;
  }

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL) {
    return JNI_FALSE;
  }
  if (!IS_OBJ_TYPE(handle->value, OBJ_MAP)) {
    return JNI_FALSE;
  }
  Map* map = (Map*) AS_OBJ(handle->value);
  int mapSlot = nextSlot(vm, true);
  SLOT(mapSlot) = handle->value;

  LOGI("saynaa_mapSet called with Slot=%d, type=%s", mapSlot, varTypeName(SLOT(mapSlot)));

  return MapSet(vm, mapSlot, keySlot, valueSlot) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1bindJavaClass(
    JNIEnv* env, jobject thiz, jint slot, jobject value) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;
  reserveSlots(vm, slot + 1);

  if (value == NULL) {
    setSlotNull(vm, slot);
    return JNI_TRUE;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL || bridge->jvm == NULL || bridge->clsJavaClass == NULL)
    return JNI_FALSE;

  JavaRef* ref = make_java_ref(env, bridge->jvm, value);
  if (ref == NULL)
    return JNI_FALSE;

  LOGD("saynaa_bindJavaClass: created JavaRef %p for Java class %p", (void*) ref, (void*) value);

  if (!create_java_instance(vm, &bridge->clsJavaClass, ref, slot)) {
    java_ref_destructor(ref);
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1bindJavaObject(
    JNIEnv* env, jobject thiz, jint slot, jobject value) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;
  reserveSlots(vm, slot + 1);

  if (value == NULL) {
    setSlotNull(vm, slot);
    return JNI_TRUE;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL || bridge->jvm == NULL || bridge->clsJavaObject == NULL)
    return JNI_FALSE;

  find_pinned_handle(vm, 1);
  LOGD("testing the where bug come from");
  JavaRef* ref = make_java_ref(env, bridge->jvm, value);
  find_pinned_handle(vm, 1);
  if (ref == NULL)
    return JNI_FALSE;

  LOGD("saynaa_bindJavaObject: created JavaRef %p for Java class %p", (void*) ref, (void*) value);

  if (!create_java_instance(vm, &bridge->clsJavaObject, ref, slot)) {
    java_ref_destructor(ref);
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1bindJavaMethod(
    JNIEnv* env, jobject thiz, jint slot, jobject target, jstring methodName, jboolean isStatic) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;
  reserveSlots(vm, slot + 1);

  if (methodName == NULL) {
    setSlotNull(vm, slot);
    return JNI_TRUE;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL || bridge->jvm == NULL || bridge->clsJavaMethod == NULL)
    return JNI_FALSE;

  JavaRef* targetRef = NULL;
  if (target != NULL) {
    targetRef = make_java_ref(env, bridge->jvm, target);
    if (targetRef == NULL)
      return JNI_FALSE;
  }

  const char* methodNameChars = (*env)->GetStringUTFChars(env, methodName, NULL);
  if (methodNameChars == NULL) {
    if (targetRef != NULL)
      java_ref_destructor(targetRef);
    return JNI_FALSE;
  }

  bool success = create_java_method_instance(vm, targetRef, methodNameChars, isStatic == JNI_TRUE, slot);

  (*env)->ReleaseStringUTFChars(env, methodName, methodNameChars);

  if (!success) {
    if (targetRef != NULL)
      java_ref_destructor(targetRef);
    return JNI_FALSE;
  }

  return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getSlotType(
    JNIEnv* env, jobject thiz, jint slot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || slot < 0)
    return -1;
  reserveSlots(vm, slot + 1);
  return (jint) GetSlotType(vm, slot);
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getSlotBool(
    JNIEnv* env, jobject thiz, jint slot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;
  reserveSlots(vm, slot + 1);
  return GetSlotBool(vm, slot) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jdouble JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getSlotNumber(
    JNIEnv* env, jobject thiz, jint slot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return 0.0;
  reserveSlots(vm, slot + 1);
  return (jdouble) GetSlotNumber(vm, slot);
}

JNIEXPORT jstring JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getSlotString(
    JNIEnv* env, jobject thiz, jint slot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return NULL;
  reserveSlots(vm, slot + 1);
  const char* text = GetSlotString(vm, slot, NULL);
  if (text == NULL)
    return NULL;
  return (*env)->NewStringUTF(env, text);
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1isSlotJava(
    JNIEnv* env, jobject thiz, jint slot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;
  reserveSlots(vm, slot + 1);
  VarType type = GetSlotType(vm, slot);
  if (type == vPOINTER) {
    JavaRef* ref = (JavaRef*) GetSlotPointer(vm, slot, NULL, NULL);
    if (ref != NULL && ref->magic == JAVA_REF_MAGIC && ref->global != NULL)
      return JNI_TRUE;
  }
  if (type == vINSTANCE) {
    BridgeState* bridge = bridge_from_vm(vm);
    if (bridge != NULL) {
      bool result = varIsType(vm, SLOT(slot), bridge->clsJavaBase->value);
      if (result) {
        return JNI_TRUE;
      }
    }
  }
  return JNI_FALSE;
}

JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getSlotJavaObject(
    JNIEnv* env, jobject thiz, jint slot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return NULL;
  BridgeState* bridge = bridge_from_vm(vm);
  reserveSlots(vm, slot + 1);

  VarType type = GetSlotType(vm, slot);
  if (type == vPOINTER) {
    JavaRef* ref = (JavaRef*) GetSlotPointer(vm, slot, NULL, NULL);
    if (ref == NULL || ref->magic != JAVA_REF_MAGIC || ref->global == NULL)
      return NULL;
    return (*env)->NewLocalRef(env, ref->global);
  }

  if (type != vINSTANCE || bridge == NULL) {
    LOGE("saynaa_getSlotJavaObject: type isn't instance or bridge is null");
    return NULL;
  }

  bool result = varIsType(vm, SLOT(slot), bridge->clsJavaBase->value);
  if (!result) {
    LOGE("saynaa_getSlotJavaObject: isn't instance of clsJavaBase");
    return NULL;
  }

  JavaNativeBase* javainstance = (JavaNativeBase*) GetSlotNativeInstance(vm, slot);
  if (javainstance != NULL && javainstance->reference != NULL && javainstance->reference->global != NULL)
    return (*env)->NewLocalRef(env, javainstance->reference->global);

  return NULL;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getListSize(
    JNIEnv* env, jobject thiz, jint handleId) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return 0;

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL)
    return 0;
  if (!IS_OBJ_TYPE(handle->value, OBJ_LIST))
    return 0;
  List* list = (List*) AS_OBJ(handle->value);

  return (jint) list->elements.count;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1listGetToSlot(
    JNIEnv* env, jobject thiz, jint handleId, jint index, jint valueSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || index < 0)
    return JNI_FALSE;

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL)
    return JNI_FALSE;

  if (!IS_OBJ_TYPE(handle->value, OBJ_LIST))
    return JNI_FALSE;

  List* list = (List*) AS_OBJ(handle->value);
  if ((uint32_t) index >= list->elements.count)
    return JNI_FALSE;

  Var value = list->elements.data[index];
  LOGD("saynaa_listGetToSlot: value type: %s", varTypeName(value));
  if (value == NULL)
    return JNI_FALSE;

  SLOT(valueSlot) = value;
  return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getMapSize(
    JNIEnv* env, jobject thiz, jint handleId) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return 0;

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL)
    return 0;
  if (!IS_OBJ_TYPE(handle->value, OBJ_MAP))
    return 0;
  Map* map = (Map*) AS_OBJ(handle->value);
  return (jint) map->count;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1mapGetToSlots(
    JNIEnv* env, jobject thiz, jint handleId, jint keySlot, jint valueSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;

  Handle* handle = find_pinned_handle(vm, handleId);
  if (handle == NULL)
    return JNI_FALSE;
  if (!IS_OBJ_TYPE(handle->value, OBJ_MAP))
    return JNI_FALSE;
  Map* map = (Map*) AS_OBJ(handle->value);

  Var key = SLOT(keySlot);

  Var value = mapGet(map, key);
  if (value == NULL)
    return JNI_FALSE;

  int valueHandleSlot = nextSlot(vm, true);
  setSlotHandle(vm, valueHandleSlot, newHandle(vm, value));
  SLOT(valueSlot) = SLOT(valueHandleSlot);
  freeSlot(vm, valueHandleSlot, 1);

  return JNI_TRUE;
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackMethodNative(
    JNIEnv* env, jobject thiz, jint callbackId, jstring methodName, jobjectArray args) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || callbackId <= 0)
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return;

  CallbackEntry* entry = find_callback(vm, (int) callbackId);
  if (entry == NULL)
    return;

  if (!ensure_wrapper_classes(vm))
    return;

  if (vm->fiber != NULL)
    vm->fiber->error = NULL;

  const char* runtimeMethodName = NULL;
  if (methodName != NULL)
    runtimeMethodName = (*env)->GetStringUTFChars(env, methodName, NULL);

  bool ok = invoke_registered_callback(env, vm, bridge, entry, runtimeMethodName, args, NULL);

  if (methodName != NULL && runtimeMethodName != NULL)
    (*env)->ReleaseStringUTFChars(env, methodName, runtimeMethodName);

  if (!ok) {
    const char* err = (vm->fiber != NULL && vm->fiber->error != NULL) ? vm->fiber->error->data : "<unknown>";
    LOGE("invokeCallbackNative failed for callbackId=%d err=%s", (int) callbackId, err);
    if (vm->fiber != NULL)
      vm->fiber->error = NULL;
  } else {
    LOGI("invokeCallbackNative succeeded for callbackId=%d", (int) callbackId);
  }
}

JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackMethodWithResultNative(
    JNIEnv* env, jobject thiz, jint callbackId, jstring methodName, jobjectArray args) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || callbackId <= 0)
    return NULL;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return NULL;

  CallbackEntry* entry = find_callback(vm, (int) callbackId);
  if (entry == NULL)
    return NULL;

  if (!ensure_wrapper_classes(vm))
    return NULL;

  if (vm->fiber != NULL)
    vm->fiber->error = NULL;

  const char* runtimeMethodName = NULL;
  if (methodName != NULL)
    runtimeMethodName = (*env)->GetStringUTFChars(env, methodName, NULL);

  jobject result = NULL;
  bool ok = invoke_registered_callback(env, vm, bridge, entry, runtimeMethodName, args, &result);

  if (methodName != NULL && runtimeMethodName != NULL)
    (*env)->ReleaseStringUTFChars(env, methodName, runtimeMethodName);

  if (!ok) {
    const char* err = (vm->fiber != NULL && vm->fiber->error != NULL) ? vm->fiber->error->data : "<unknown>";
    LOGE("invokeCallbackMethodWithResultNative failed for callbackId=%d err=%s", (int) callbackId, err);
    if (vm->fiber != NULL)
      vm->fiber->error = NULL;
    return NULL;
  }

  return result;
}

JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackWithResultFromSlots(
    JNIEnv* env, jobject thiz, jint callbackId, jstring methodName, jint argStart, jint argCount) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || callbackId <= 0)
    return NULL;

  if (argStart < 0 || argCount < 0)
    return NULL;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return NULL;

  CallbackEntry* entry = find_callback(vm, (int) callbackId);
  if (entry == NULL)
    return NULL;

  if (!ensure_wrapper_classes(vm))
    return NULL;

  if (vm->fiber != NULL)
    vm->fiber->error = NULL;

  const char* runtimeMethodName = NULL;
  if (methodName != NULL)
    runtimeMethodName = (*env)->GetStringUTFChars(env, methodName, NULL);

  jobject result = NULL;
  bool ok = invoke_registered_callback_from_slots(
      env, vm, bridge, entry, runtimeMethodName, (int) argStart, (int) argCount, &result);

  if (methodName != NULL && runtimeMethodName != NULL)
    (*env)->ReleaseStringUTFChars(env, methodName, runtimeMethodName);

  if (!ok) {
    const char* err = (vm->fiber != NULL && vm->fiber->error != NULL) ? vm->fiber->error->data : "<unknown>";
    LOGE("invokeCallbackWithResultFromSlots failed for callbackId=%d err=%s", (int) callbackId, err);
    if (vm->fiber != NULL)
      vm->fiber->error = NULL;
    return NULL;
  }

  return result;
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1close(JNIEnv* env, jobject thiz) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;

  // Clear the Java-side pointer early to avoid re-entrant close calls during teardown.
  set_vm_ptr_on_saynaa(env, thiz, (jlong) 0);

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL) {
    if (bridge->closing)
      return;
    bridge->closing = true;
    clear_callbacks(vm);
    clear_pinned_handles(vm);
    release_bridge_handle(vm, &bridge->javaWrapperModule);
    release_bridge_handle(vm, &bridge->clsJavaMethod);
    release_bridge_handle(vm, &bridge->clsJavaObject);
    release_bridge_handle(vm, &bridge->clsJavaClass);
    if (bridge->saynaaObject != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
      bridge->saynaaObject = NULL;
    }

    if (bridge->javaBridgeClass != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->javaBridgeClass);
      bridge->javaBridgeClass = NULL;
    }
    free(bridge);
    SetUserData(vm, NULL);
  }

  // Best-effort cleanup: ensure no dangling handles block VM shutdown.
  while (vm->handles != NULL) {
    releaseHandle(vm, vm->handles);
  }

  FreeVM(vm);
}
