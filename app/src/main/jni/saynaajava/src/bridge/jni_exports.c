#include "saynaa_internal.h"
#include "saynaa_exports.h"
#include "saynaa_jni.h"

static Result run_string_pcall(VM* vm, const char* code) {
  if (vm == NULL || code == NULL)
    return RESULT_RUNTIME_ERROR;
  return RunStringPcall(vm, code);
}

JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1open(JNIEnv* env, jobject thiz) {
  JavaVM* jvm = NULL;
  if ((*env)->GetJavaVM(env, &jvm) != JNI_OK) {
    return NULL;
  }

  Configuration config = NewConfiguration();
  config.stdout_write = android_stdout_write;
  config.stderr_write = android_stderr_write;

  VM* vm = NewVM(&config);
  if (vm == NULL) {
    return NULL;
  }

  BridgeState* bridge = (BridgeState*) calloc(1, sizeof(BridgeState));
  if (bridge == NULL) {
    FreeVM(vm);
    return NULL;
  }

  bridge->jvm = jvm;
  bridge->saynaaObject = (*env)->NewGlobalRef(env, thiz);
  if (bridge->saynaaObject == NULL) {
    free(bridge);
    FreeVM(vm);
    return NULL;
  }

  jclass localBridgeClass = (*env)->FindClass(env, "com/android/saynaa/saynaajava/JavaBridge");
  if (localBridgeClass == NULL) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    free(bridge);
    FreeVM(vm);
    return NULL;
  }

  bridge->javaBridgeClass = (jclass) (*env)->NewGlobalRef(env, localBridgeClass);
  (*env)->DeleteLocalRef(env, localBridgeClass);

  bridge->mFindClass = (*env)->GetStaticMethodID(
      env, bridge->javaBridgeClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
  bridge->mCreateJavaObject = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createJavaObject", "(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
    bridge->mCreateFromSlots = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createFromSlots", "(Lcom/android/saynaa/saynaajava/Saynaa;IIII)Z");
    bridge->mNewFromSlots = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "newFromSlots", "(Lcom/android/saynaa/saynaajava/Saynaa;IIII)Z");
  bridge->mCallJavaMethod = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "callJavaMethod",
      "(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
  bridge->mCallStaticJavaMethod = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "callStaticJavaMethod", "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
    bridge->mCallFromSlots = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "callFromSlots", "(Lcom/android/saynaa/saynaajava/Saynaa;IIIII)Z");
    bridge->mCallStaticFromSlots = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "callStaticFromSlots", "(Lcom/android/saynaa/saynaajava/Saynaa;IIIII)Z");
  bridge->mGetFieldValue = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "getFieldValue",
      "(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;");
  bridge->mSetFieldValue = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "setFieldValue",
      "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/Object;)Z");
    bridge->mGetFieldFromSlots = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "getFieldFromSlots", "(Lcom/android/saynaa/saynaajava/Saynaa;III)Z");
    bridge->mSetFieldFromSlots = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "setFieldFromSlots", "(Lcom/android/saynaa/saynaajava/Saynaa;IIII)Z");
  bridge->mResolveCallbackInterface = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "resolveCallbackInterface", "(Ljava/lang/Object;Ljava/lang/String;II)Ljava/lang/String;");
  bridge->mCreateProxy = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createProxy", "(Lcom/android/saynaa/saynaajava/Saynaa;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;");
    bridge->mCreateProxyFromSlots = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createProxyFromSlots", "(Lcom/android/saynaa/saynaajava/Saynaa;IIIII)Z");
    bridge->mResolveInterfaceNameFromSlots = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "resolveInterfaceNameFromSlots", "(Lcom/android/saynaa/saynaajava/Saynaa;I)Ljava/lang/String;");
  bridge->mCreateNativeCallbackProxy = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createNativeCallbackProxy", "(Lcom/android/saynaa/saynaajava/Saynaa;Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/Object;");
  bridge->mGetDefaultInterfaceMethodName = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "getDefaultInterfaceMethodName", "(Ljava/lang/String;)Ljava/lang/String;");
    bridge->mPushToSlot = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "pushToSlot", "(Lcom/android/saynaa/saynaajava/Saynaa;ILjava/lang/Object;)Z");
  bridge->mSlotToJava = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "slotToJava", "(Lcom/android/saynaa/saynaajava/Saynaa;I)Ljava/lang/Object;");
  bridge->mArgsArrayFromSlots = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "argsFromSlots", "(Lcom/android/saynaa/saynaajava/Saynaa;II)[Ljava/lang/Object;");
    bridge->mJavaLength = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "lengthOf", "(Ljava/lang/Object;)D");
    bridge->mJavaToString = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "javaToString", "(Ljava/lang/Object;)Ljava/lang/String;");
    bridge->mAstableToSlot = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "astableToSlot", "(Lcom/android/saynaa/saynaajava/Saynaa;ILjava/lang/Object;)Z");
      bridge->mInstanceOf = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
        "instanceOf", "(Ljava/lang/Object;Ljava/lang/Object;)Z");

  jclass saynaaClass = (*env)->GetObjectClass(env, thiz);
  bridge->mOnNativeError = (*env)->GetMethodID(env, saynaaClass, "onNativeError", "(Ljava/lang/String;)V");
  (*env)->DeleteLocalRef(env, saynaaClass);

    if (bridge->mFindClass == NULL || bridge->mCreateJavaObject == NULL
      || bridge->mCreateFromSlots == NULL || bridge->mNewFromSlots == NULL
      || bridge->mCallJavaMethod == NULL
      || bridge->mCallStaticJavaMethod == NULL || bridge->mCallFromSlots == NULL
      || bridge->mCallStaticFromSlots == NULL || bridge->mGetFieldValue == NULL
      || bridge->mSetFieldValue == NULL || bridge->mGetFieldFromSlots == NULL
      || bridge->mSetFieldFromSlots == NULL || bridge->mResolveCallbackInterface == NULL
      || bridge->mCreateProxy == NULL || bridge->mCreateProxyFromSlots == NULL
      || bridge->mResolveInterfaceNameFromSlots == NULL || bridge->mCreateNativeCallbackProxy == NULL
      || bridge->mGetDefaultInterfaceMethodName == NULL || bridge->mPushToSlot == NULL
      || bridge->mSlotToJava == NULL || bridge->mArgsArrayFromSlots == NULL
      || bridge->mJavaLength == NULL || bridge->mJavaToString == NULL || bridge->mAstableToSlot == NULL
      || bridge->mInstanceOf == NULL
      || bridge->mOnNativeError == NULL) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    (*env)->DeleteGlobalRef(env, bridge->javaBridgeClass);
    free(bridge);
    FreeVM(vm);
    return NULL;
  }

  SetUserData(vm, bridge);
  if (!register_java_wrapper_classes(vm)) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    (*env)->DeleteGlobalRef(env, bridge->javaBridgeClass);
    free(bridge);
    SetUserData(vm, NULL);
    FreeVM(vm);
    return NULL;
  }
  register_java_api(vm);

  jclass cptrCls = (*env)->FindClass(env, "com/android/saynaa/saynaajava/CPtr");
  if (cptrCls == NULL) {
    return NULL;
  }

  jobject cptrObj = (*env)->AllocObject(env, cptrCls);
  jfieldID ptrField = (*env)->GetFieldID(env, cptrCls, "pointer", "J");
  (*env)->SetLongField(env, cptrObj, ptrField, (jlong) (intptr_t) vm);
  (*env)->DeleteLocalRef(env, cptrCls);

  return cptrObj;
}

JNIEXPORT jobject JNICALL
Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1pcall(
    JNIEnv* env, jobject thiz, jstring functionName, jobjectArray args) {

  VM* vm = vm_from_saynaa(env, thiz);
  BridgeState* bridge = vm ? bridge_from_vm(vm) : NULL;

  const char* fnNameChars = NULL;
  const char* message = "OK";
  jboolean success = JNI_TRUE;
  jclass pcallClass = NULL;
  jmethodID pcallCtor = NULL;
  jstring jmsg = NULL;
  jobject result = NULL;
  jobject resultValue = NULL;

  // ===== VALIDATION =====
  if (vm == NULL || bridge == NULL || functionName == NULL) {
    success = JNI_FALSE;
    message = "Invalid VM or arguments";
    goto L_return;
  }

  fnNameChars = (*env)->GetStringUTFChars(env, functionName, NULL);
  if (fnNameChars == NULL) {
    success = JNI_FALSE;
    message = "String conversion failed";
    goto L_return;
  }

  int argc = (args == NULL) ? 0 : (int)(*env)->GetArrayLength(env, args);

  Module* module = current_module_from_vm(vm);
  if (module == NULL && bridge != NULL && bridge->mainModule != NULL) {
    module = (Module*) AS_OBJ(bridge->mainModule->value);
  }
  if (module == NULL) {
    goto L_cleanup;
  }

  int fnIndex = moduleGetGlobalIndex(module, fnNameChars, (uint32_t) strlen(fnNameChars));
  if (fnIndex < 0) {
    goto L_cleanup;
  }

  // ===== PREPARE CALL =====
  reserveSlots(vm, argc + 1);
  vm->fiber->ret[0] = module->globals.data[fnIndex];

  for (int i = 0; i < argc; i++) {
    jobject arg = (*env)->GetObjectArrayElement(env, args, i);

    if (!java_to_slot(env, vm, bridge, i + 1, arg)) {
      (*env)->DeleteLocalRef(env, arg);
      success = JNI_FALSE;
      message = "Argument conversion failed";
      goto L_cleanup;
    }

    (*env)->DeleteLocalRef(env, arg);
  }

  // ===== EXECUTE =====
  if (!CallFunction(vm, 0, argc, 1, 0)) {
    success = JNI_FALSE;
    message = "CallFunction failed";
    goto L_cleanup;
  }

  // ===== VM ERROR CHECK =====
  if (VM_HAS_ERROR(vm)) {
    success = JNI_FALSE;

    const char* errMsg =
        (vm->fiber && vm->fiber->error)
        ? vm->fiber->error->data
        : "<unknown error>";

    LOGE("VM ERROR: %s", errMsg);

    message = errMsg;

    if (vm->fiber) {
      vm->fiber->error = NULL;
    }
  } else {
    resultValue = slot_to_java(env, vm, bridge, 0);
    if (VM_HAS_ERROR(vm)) {
      success = JNI_FALSE;
      message = "Return value conversion failed";
      if (vm->fiber) {
        vm->fiber->error = NULL;
      }
    }
  }

L_cleanup:
  if (fnNameChars != NULL) {
    (*env)->ReleaseStringUTFChars(env, functionName, fnNameChars);
  }

L_return:
  // ===== CREATE RESULT OBJECT (SAFE) =====
  pcallClass = saynaa_get_pcall_result_class();
  pcallCtor = saynaa_get_pcall_result_ctor();

  if (pcallClass == NULL || pcallCtor == NULL) {
    LOGE("pcall class/ctor NULL");
    return NULL;
  }

  jmsg = (*env)->NewStringUTF(env, message);
  if (jmsg == NULL) {
    return NULL;
  }

  result = (*env)->NewObject(
      env,
      pcallClass,
      pcallCtor,
      success,
      jmsg,
      resultValue);

  (*env)->DeleteLocalRef(env, jmsg);
  if (resultValue != NULL)
    (*env)->DeleteLocalRef(env, resultValue);

  return result;
}

static Module* get_or_create_main_module(VM* vm, BridgeState* bridge) {
  Module* module = current_module_from_vm(vm);
  if (module != NULL)
    return module;

  if (bridge == NULL)
    return NULL;

  if (bridge->mainModule == NULL)
    bridge->mainModule = NewModule(vm, "@(SAYNAA)");

  if (bridge->mainModule == NULL)
    return NULL;

  return (Module*) AS_OBJ(bridge->mainModule->value);
}

JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getGlobal(
    JNIEnv* env, jobject thiz, jstring name) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || name == NULL)
    return NULL;

  BridgeState* bridge = bridge_from_vm(vm);
  const char* key = (*env)->GetStringUTFChars(env, name, NULL);
  if (key == NULL)
    return NULL;

  Module* module = get_or_create_main_module(vm, bridge);

  if (module == NULL) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return NULL;
  }

  int idx = moduleGetGlobalIndex(module, key, (uint32_t) strlen(key));
  (*env)->ReleaseStringUTFChars(env, name, key);
  if (idx < 0)
    return NULL;

  reserveSlots(vm, 2);
  Handle* handle = vmNewHandle(vm, module->globals.data[idx]);
  if (handle == NULL)
    return NULL;

  setSlotHandle(vm, 1, handle);
  jobject resultValue = slot_to_java(env, vm, bridge, 1);
  releaseHandle(vm, handle);
  return resultValue;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getGlobalFunctionId(
    JNIEnv* env, jobject thiz, jstring name) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || name == NULL)
    return (jint) -1;

  BridgeState* bridge = bridge_from_vm(vm);
  const char* key = (*env)->GetStringUTFChars(env, name, NULL);
  if (key == NULL)
    return (jint) -1;

  Module* module = get_or_create_main_module(vm, bridge);
  if (module == NULL) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return (jint) -1;
  }

  int idx = moduleGetGlobalIndex(module, key, (uint32_t) strlen(key));
  (*env)->ReleaseStringUTFChars(env, name, key);
  return (jint) idx;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1callFunctionById(
    JNIEnv* env, jobject thiz, jint functionId, jint argStart, jint argCount, jint retSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || functionId < 0)
    return JNI_FALSE;
  if (argStart < 0 || argCount < 0)
    return JNI_FALSE;

  BridgeState* bridge = bridge_from_vm(vm);
  Module* module = get_or_create_main_module(vm, bridge);
  if (module == NULL)
    return JNI_FALSE;

  if (functionId >= (jint) module->globals.count)
    return JNI_FALSE;

  int needed = argStart + argCount;
  if (retSlot >= needed)
    needed = retSlot + 1;
  if (needed < 1)
    needed = 1;
  reserveSlots(vm, needed);

  vm->fiber->ret[0] = module->globals.data[functionId];
  if (!CallFunction(vm, 0, (int) argCount, (int) argStart, (int) retSlot))
    return JNI_FALSE;
  return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setGlobal(
    JNIEnv* env, jobject thiz, jstring name, jobject value) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || name == NULL)
    return JNI_FALSE;

  BridgeState* bridge = bridge_from_vm(vm);
  const char* key = (*env)->GetStringUTFChars(env, name, NULL);
  if (key == NULL)
    return JNI_FALSE;

  Module* module = get_or_create_main_module(vm, bridge);

  if (module == NULL) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return JNI_FALSE;
  }

  reserveSlots(vm, 2);
  if (!java_to_slot(env, vm, bridge, 1, value)) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return JNI_FALSE;
  }

  if (bridge != NULL && value != NULL && strcmp(key, "activity") == 0) {
    if (bridge->activity != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->activity);
      bridge->activity = NULL;
    }
    bridge->activity = (*env)->NewGlobalRef(env, value);
  }

  Handle* handle = GetSlotHandle(vm, 1);
  if (handle == NULL) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return JNI_FALSE;
  }

  moduleSetGlobal(vm, module, key, (uint32_t) strlen(key), handle->value);
  releaseHandle(vm, handle);
  (*env)->ReleaseStringUTFChars(env, name, key);
  return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setGlobalFromSlot(
    JNIEnv* env, jobject thiz, jstring name, jint slot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || name == NULL)
    return JNI_FALSE;

  BridgeState* bridge = bridge_from_vm(vm);
  const char* key = (*env)->GetStringUTFChars(env, name, NULL);
  if (key == NULL)
    return JNI_FALSE;

  Module* module = get_or_create_main_module(vm, bridge);

  if (module == NULL) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return JNI_FALSE;
  }

  if (slot < 0) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return JNI_FALSE;
  }

  reserveSlots(vm, slot + 1);
  Handle* handle = GetSlotHandle(vm, slot);
  if (handle == NULL) {
    (*env)->ReleaseStringUTFChars(env, name, key);
    return JNI_FALSE;
  }

  moduleSetGlobal(vm, module, key, (uint32_t) strlen(key), handle->value);
  releaseHandle(vm, handle);
  (*env)->ReleaseStringUTFChars(env, name, key);
  return JNI_TRUE;
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

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1newList(
    JNIEnv* env, jobject thiz, jint slot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;
  reserveSlots(vm, slot + 1);
  NewList(vm, slot);
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1newMap(
    JNIEnv* env, jobject thiz, jint slot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;
  reserveSlots(vm, slot + 1);
  NewMap(vm, slot);
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1listInsert(
    JNIEnv* env, jobject thiz, jint listSlot, jint index, jint valueSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;
  reserveSlots(vm, listSlot + 1);
  reserveSlots(vm, valueSlot + 1);
  return ListInsert(vm, listSlot, index, valueSlot) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1mapSet(
    JNIEnv* env, jobject thiz, jint mapSlot, jint keySlot, jint valueSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return JNI_FALSE;
  reserveSlots(vm, mapSlot + 1);
  reserveSlots(vm, keySlot + 1);
  reserveSlots(vm, valueSlot + 1);
  return MapSet(vm, mapSlot, keySlot, valueSlot) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1wrapJavaObject(
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

  JavaRef* ref = make_java_ref(env, bridge->jvm, value);
  if (ref == NULL)
    return JNI_FALSE;

  if (!create_java_instance(vm, &bridge->clsJavaObject, ref, slot)) {
    java_ref_destructor(ref);
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

  if (type != vINSTANCE || bridge == NULL)
    return NULL;

  int clsSlot = slot + 4;
  int objSlot = slot + 5;
  int methodSlot = slot + 6;
  reserveSlots(vm, methodSlot + 1);

  bool isClass = false, isObject = false, isMethod = false;
  if (bridge->clsJavaClass != NULL) {
    setSlotHandle(vm, clsSlot, bridge->clsJavaClass);
    IsSlotInstanceOf(vm, slot, clsSlot, &isClass);
  }
  if (bridge->clsJavaObject != NULL) {
    setSlotHandle(vm, objSlot, bridge->clsJavaObject);
    IsSlotInstanceOf(vm, slot, objSlot, &isObject);
  }
  if (bridge->clsJavaMethod != NULL) {
    setSlotHandle(vm, methodSlot, bridge->clsJavaMethod);
    IsSlotInstanceOf(vm, slot, methodSlot, &isMethod);
  }

  if (isClass) {
    JavaClassNative* jc = (JavaClassNative*) GetSlotNativeInstance(vm, slot);
    if (jc != NULL && jc->class_ref != NULL && jc->class_ref->global != NULL)
      return (*env)->NewLocalRef(env, jc->class_ref->global);
  }

  if (isObject) {
    JavaObjectNative* jo = (JavaObjectNative*) GetSlotNativeInstance(vm, slot);
    if (jo != NULL && jo->object_ref != NULL && jo->object_ref->global != NULL)
      return (*env)->NewLocalRef(env, jo->object_ref->global);
  }

  if (isMethod) {
    JavaMethodNative* jm = (JavaMethodNative*) GetSlotNativeInstance(vm, slot);
    if (jm != NULL && jm->target_ref != NULL && jm->target_ref->global != NULL)
      return (*env)->NewLocalRef(env, jm->target_ref->global);
  }

  return NULL;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getListSize(
    JNIEnv* env, jobject thiz, jint listSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return 0;
  reserveSlots(vm, listSlot + 1);
  if (GetSlotType(vm, listSlot) != vLIST)
    return 0;
  Handle* handle = GetSlotHandle(vm, listSlot);
  if (handle == NULL)
    return 0;
  List* list = (List*) AS_OBJ(handle->value);
  releaseHandle(vm, handle);
  return (jint) list->elements.count;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1listGetToSlot(
    JNIEnv* env, jobject thiz, jint listSlot, jint index, jint valueSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || index < 0)
    return JNI_FALSE;
  reserveSlots(vm, valueSlot + 1);
  reserveSlots(vm, listSlot + 1);
  if (GetSlotType(vm, listSlot) != vLIST)
    return JNI_FALSE;
  Handle* handle = GetSlotHandle(vm, listSlot);
  if (handle == NULL)
    return JNI_FALSE;
  List* list = (List*) AS_OBJ(handle->value);
  releaseHandle(vm, handle);
  if ((uint32_t) index >= list->elements.count)
    return JNI_FALSE;
  Handle* valueHandle = vmNewHandle(vm, list->elements.data[index]);
  if (valueHandle == NULL)
    return JNI_FALSE;
  setSlotHandle(vm, valueSlot, valueHandle);
  releaseHandle(vm, valueHandle);
  return JNI_TRUE;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1getMapSize(
    JNIEnv* env, jobject thiz, jint mapSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return 0;
  reserveSlots(vm, mapSlot + 1);
  if (GetSlotType(vm, mapSlot) != vMAP)
    return 0;
  Handle* handle = GetSlotHandle(vm, mapSlot);
  if (handle == NULL)
    return 0;
  Map* map = (Map*) AS_OBJ(handle->value);
  releaseHandle(vm, handle);
  return (jint) map->count;
}

JNIEXPORT jboolean JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1mapEntryToSlots(
    JNIEnv* env, jobject thiz, jint mapSlot, jint entryIndex, jint keySlot, jint valueSlot) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || entryIndex < 0)
    return JNI_FALSE;
  reserveSlots(vm, valueSlot + 1);
  reserveSlots(vm, keySlot + 1);
  reserveSlots(vm, mapSlot + 1);
  if (GetSlotType(vm, mapSlot) != vMAP)
    return JNI_FALSE;
  Handle* handle = GetSlotHandle(vm, mapSlot);
  if (handle == NULL)
    return JNI_FALSE;
  Map* map = (Map*) AS_OBJ(handle->value);
  releaseHandle(vm, handle);

  int found = -1;
  for (uint32_t i = 0; i < map->capacity; i++) {
    MapEntry* entry = &map->entries[i];
    if (IS_UNDEF(entry->key))
      continue;
    found++;
    if (found == entryIndex) {
      Handle* keyHandle = vmNewHandle(vm, entry->key);
      Handle* valueHandle = vmNewHandle(vm, entry->value);
      if (keyHandle == NULL || valueHandle == NULL) {
        if (keyHandle != NULL)
          releaseHandle(vm, keyHandle);
        if (valueHandle != NULL)
          releaseHandle(vm, valueHandle);
        return JNI_FALSE;
      }
      setSlotHandle(vm, keySlot, keyHandle);
      setSlotHandle(vm, valueSlot, valueHandle);
      releaseHandle(vm, keyHandle);
      releaseHandle(vm, valueHandle);
      return JNI_TRUE;
    }
  }

  return JNI_FALSE;
}
JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1doFile(
    JNIEnv* env, jobject thiz, jstring fileName) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || fileName == NULL)
    return (jint) RESULT_RUNTIME_ERROR;

  ensure_activity_and_paths(env, vm, thiz);

  const char* file = (*env)->GetStringUTFChars(env, fileName, NULL);
  if (file == NULL)
    return (jint) RESULT_RUNTIME_ERROR;

  Result ret = saynaa_run_file_in_main_module(vm, file);
  (*env)->ReleaseStringUTFChars(env, fileName, file);
  return (jint) ret;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1doString(
    JNIEnv* env, jobject thiz, jstring codeString) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || codeString == NULL)
    return (jint) RESULT_RUNTIME_ERROR;

  ensure_activity_and_paths(env, vm, thiz);

  const char* code = (*env)->GetStringUTFChars(env, codeString, NULL);
  if (code == NULL)
    return (jint) RESULT_RUNTIME_ERROR;

  Result ret = saynaa_run_in_main_module(vm, code, "@(String)");
  (*env)->ReleaseStringUTFChars(env, codeString, code);
  return (jint) ret;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1doStringPcall(
    JNIEnv* env, jobject thiz, jstring codeString) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || codeString == NULL)
    return (jint) RESULT_RUNTIME_ERROR;

  ensure_activity_and_paths(env, vm, thiz);

  const char* code = (*env)->GetStringUTFChars(env, codeString, NULL);
  if (code == NULL)
    return (jint) RESULT_RUNTIME_ERROR;

  Result ret = run_string_pcall(vm, code);
  (*env)->ReleaseStringUTFChars(env, codeString, code);
  return (jint) ret;
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1setLastEventView(
    JNIEnv* env, jobject thiz, jobject view) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return;

  if (bridge->lastEventView != NULL) {
    (*env)->DeleteGlobalRef(env, bridge->lastEventView);
    bridge->lastEventView = NULL;
  }
  if (view != NULL) {
    bridge->lastEventView = (*env)->NewGlobalRef(env, view);
  }
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_execute(
    JNIEnv* env, jobject thiz, jobject context) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL) {
    LOGE("VM not initialized.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL) {
    ensure_files_search_path(vm, bridge, env, context);

    clear_callbacks(vm);
    // Keep wrapper/module handles alive for the VM lifetime.
    // Releasing and recreating them on each execute() can re-register
    // module names (e.g. "java_wrappers") and trigger assertion failures
    // in DEBUG builds.

    if (bridge->activity != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->activity);
      bridge->activity = NULL;
    }
    if (bridge->lastEventView != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->lastEventView);
      bridge->lastEventView = NULL;
    }
    if (context != NULL) {
      bridge->activity = (*env)->NewGlobalRef(env, context);
    }
  }

  jclass saynaaCls = (*env)->GetObjectClass(env, thiz);
  jfieldID sourceField = (*env)->GetFieldID(env, saynaaCls, "source", "Ljava/lang/String;");
  jfieldID scriptPathField = (*env)->GetFieldID(env, saynaaCls, "scriptPath", "Ljava/lang/String;");
  jstring source = (jstring) (*env)->GetObjectField(env, thiz, sourceField);
  jstring scriptPath = scriptPathField == NULL ? NULL : (jstring) (*env)->GetObjectField(env, thiz, scriptPathField);
  (*env)->DeleteLocalRef(env, saynaaCls);

  const char* path = NULL;
  if (scriptPath != NULL) {
    path = (*env)->GetStringUTFChars(env, scriptPath, NULL);
  }

  const char* code = NULL;
  if (source != NULL) {
    code = (*env)->GetStringUTFChars(env, source, NULL);
  }

  Result result;
  if (path != NULL && path[0] != '\0') {
    LOGI("Running script file... path=%s", path);
    result = saynaa_run_file_in_main_module(vm, path);
  } else if (code != NULL) {
    LOGI("Running script... source_len=%d", (int) strlen(code));
    result = saynaa_run_in_main_module(vm, code, "@(String)");
  } else {
    LOGE("Neither scriptPath nor source is available.");
    if (source != NULL)
      (*env)->DeleteLocalRef(env, source);
    if (scriptPath != NULL)
      (*env)->DeleteLocalRef(env, scriptPath);
    return;
  }

  if (source != NULL) {
    if (code != NULL)
      (*env)->ReleaseStringUTFChars(env, source, code);
    (*env)->DeleteLocalRef(env, source);
  }
  if (scriptPath != NULL) {
    if (path != NULL)
      (*env)->ReleaseStringUTFChars(env, scriptPath, path);
    (*env)->DeleteLocalRef(env, scriptPath);
  }

  if (result != RESULT_SUCCESS) {
    LOGE("Saynaa execution failed with code: %d", (int) result);
    if (vm->fiber != NULL && vm->fiber->error != NULL && vm->fiber->error->data != NULL) {
      LOGE("VM error: %s", vm->fiber->error->data);
    }
  } else {
    LOGI("Saynaa execution finished successfully.");
  }
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackNative(
    JNIEnv* env, jobject thiz, jint callbackId, jobject arg0) {
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

  jclass objClass = (*env)->FindClass(env, "java/lang/Object");
  jobjectArray args = (*env)->NewObjectArray(env, arg0 == NULL ? 0 : 1, objClass, NULL);
  (*env)->DeleteLocalRef(env, objClass);
  if (arg0 != NULL)
    (*env)->SetObjectArrayElement(env, args, 0, arg0);

  bool ok = invoke_registered_callback(env, vm, bridge, entry, NULL, args, NULL);
  if (args != NULL)
    (*env)->DeleteLocalRef(env, args);

  if (!ok) {
    const char* err = (vm->fiber != NULL && vm->fiber->error != NULL) ? vm->fiber->error->data : "<unknown>";
    LOGE("invokeCallbackNative failed for callbackId=%d err=%s", (int) callbackId, err);
    if (vm->fiber != NULL)
      vm->fiber->error = NULL;
  } else {
    LOGI("invokeCallbackNative succeeded for callbackId=%d", (int) callbackId);
  }
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

JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackMethodWithResultFromSlotsNative(
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
    LOGE("invokeCallbackMethodWithResultFromSlotsNative failed for callbackId=%d err=%s", (int) callbackId, err);
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
    release_bridge_handle(vm, &bridge->mainModule);
    release_bridge_handle(vm, &bridge->javaWrapperModule);
    release_bridge_handle(vm, &bridge->javaModule);
    release_bridge_handle(vm, &bridge->clsJavaMethod);
    release_bridge_handle(vm, &bridge->clsJavaObject);
    release_bridge_handle(vm, &bridge->clsJavaClass);
    if (bridge->saynaaObject != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
      bridge->saynaaObject = NULL;
    }
    if (bridge->lastEventView != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->lastEventView);
      bridge->lastEventView = NULL;
    }
    if (bridge->activity != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->activity);
      bridge->activity = NULL;
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

