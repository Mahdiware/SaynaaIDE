#include "internal.h"

VM* vm_from_saynaa(JNIEnv* env, jobject saynaaObject) {
  jclass saynaaCls = (*env)->GetObjectClass(env, saynaaObject);
  jfieldID vmField = (*env)->GetFieldID(env, saynaaCls, "vm", "Lcom/android/saynaa/saynaajava/CPtr;");
  jobject cptrObj = (*env)->GetObjectField(env, saynaaObject, vmField);
  (*env)->DeleteLocalRef(env, saynaaCls);

  if (cptrObj == NULL)
    return NULL;

  jclass cptrCls = (*env)->GetObjectClass(env, cptrObj);
  jfieldID ptrField = (*env)->GetFieldID(env, cptrCls, "pointer", "J");
  jlong ptr = (*env)->GetLongField(env, cptrObj, ptrField);
  (*env)->DeleteLocalRef(env, cptrCls);
  (*env)->DeleteLocalRef(env, cptrObj);

  return (VM*) (intptr_t) ptr;
}

void set_vm_ptr_on_saynaa(JNIEnv* env, jobject saynaaObject, jlong ptr) {
  jclass saynaaCls = (*env)->GetObjectClass(env, saynaaObject);
  jfieldID vmField = (*env)->GetFieldID(env, saynaaCls, "vm", "Lcom/android/saynaa/saynaajava/CPtr;");
  jobject cptrObj = (*env)->GetObjectField(env, saynaaObject, vmField);
  (*env)->DeleteLocalRef(env, saynaaCls);

  if (cptrObj == NULL)
    return;

  jclass cptrCls = (*env)->GetObjectClass(env, cptrObj);
  jfieldID ptrField = (*env)->GetFieldID(env, cptrCls, "pointer", "J");
  (*env)->SetLongField(env, cptrObj, ptrField, ptr);
  (*env)->DeleteLocalRef(env, cptrCls);
  (*env)->DeleteLocalRef(env, cptrObj);
}

static Result run_snippet_with_pcall(VM* vm, const char* code) {
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
  ensure_default_java_packages(bridge);
  bridge->saynaaObject = (*env)->NewGlobalRef(env, thiz);
  if (bridge->saynaaObject == NULL) {
    clear_java_packages(bridge);
    free(bridge);
    FreeVM(vm);
    return NULL;
  }

  jclass localBridgeClass = (*env)->FindClass(env, "com/android/saynaa/saynaajava/JavaBridge");
  if (localBridgeClass == NULL) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    clear_java_packages(bridge);
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
  bridge->mCallJavaMethod = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "callJavaMethod",
      "(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
  bridge->mCallStaticJavaMethod = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "callStaticJavaMethod", "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
  bridge->mGetFieldValue = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "getFieldValue",
      "(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;");
  bridge->mSetFieldValue = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "setFieldValue",
      "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/Object;)Z");
  bridge->mCreateProxy = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createProxy", "(Lcom/android/saynaa/saynaajava/Saynaa;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;");
  bridge->mCreateNativeCallbackProxy = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createNativeCallbackProxy", "(Lcom/android/saynaa/saynaajava/Saynaa;Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/Object;");
  bridge->mGetDefaultInterfaceMethodName = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "getDefaultInterfaceMethodName", "(Ljava/lang/String;)Ljava/lang/String;");

  if (bridge->mFindClass == NULL || bridge->mCreateJavaObject == NULL || bridge->mCallJavaMethod == NULL
      || bridge->mCallStaticJavaMethod == NULL || bridge->mGetFieldValue == NULL
      || bridge->mSetFieldValue == NULL || bridge->mCreateProxy == NULL
      || bridge->mCreateNativeCallbackProxy == NULL || bridge->mGetDefaultInterfaceMethodName == NULL) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    (*env)->DeleteGlobalRef(env, bridge->javaBridgeClass);
    clear_java_packages(bridge);
    free(bridge);
    FreeVM(vm);
    return NULL;
  }

  SetUserData(vm, bridge);
  if (!register_java_wrapper_classes(vm)) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    (*env)->DeleteGlobalRef(env, bridge->javaBridgeClass);
    clear_java_packages(bridge);
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

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1pcall(
    JNIEnv* env, jobject thiz, jstring functionName, jobjectArray args) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return -1;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return -1;

  if (functionName == NULL)
    return -3;

  const char* fnNameChars = (*env)->GetStringUTFChars(env, functionName, NULL);
  if (fnNameChars == NULL) {
    return -3;
  }

  int retCode = 0;
  int argc = (args == NULL) ? 0 : (int) (*env)->GetArrayLength(env, args);

  Module* module = current_module_from_vm(vm);
  if (module == NULL) {
    retCode = -4;
    goto L_cleanup;
  }

  int fnIndex = moduleGetGlobalIndex(module, fnNameChars, (uint32_t) strlen(fnNameChars));
  if (fnIndex < 0) {
    retCode = -5;
    goto L_cleanup;
  }

  reserveSlots(vm, argc + 1);
  vm->fiber->ret[0] = module->globals.data[fnIndex];

  for (int i = 0; i < argc; i++) {
    jobject arg = (*env)->GetObjectArrayElement(env, args, i);
    if (!java_to_slot(env, vm, bridge, i + 1, arg)) {
      if (arg != NULL)
        (*env)->DeleteLocalRef(env, arg);
      retCode = -6;
      goto L_cleanup;
    }
    if (arg != NULL)
      (*env)->DeleteLocalRef(env, arg);
  }

  if (!CallFunction(vm, 0, argc, argc > 0 ? 1 : 0, -1)) {
    retCode = 1;
    goto L_cleanup;
  }

L_cleanup:
  if (VM_HAS_ERROR(vm)) {
    const char* err = (vm->fiber != NULL && vm->fiber->error != NULL) ? vm->fiber->error->data : "<unknown>";
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, "saynaa_pcall failed: %s", err);
    vm->fiber->error = NULL;
    if (retCode == 0)
      retCode = 1;
  }

  if (fnNameChars != NULL)
    (*env)->ReleaseStringUTFChars(env, functionName, fnNameChars);

  return (jint) retCode;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1doFile(
    JNIEnv* env, jobject thiz, jstring fileName) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || fileName == NULL)
    return (jint) RESULT_RUNTIME_ERROR;

  const char* file = (*env)->GetStringUTFChars(env, fileName, NULL);
  if (file == NULL)
    return (jint) RESULT_RUNTIME_ERROR;

  Result ret = RunFile(vm, file);
  (*env)->ReleaseStringUTFChars(env, fileName, file);
  return (jint) ret;
}

JNIEXPORT jint JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1doString(
    JNIEnv* env, jobject thiz, jstring codeString) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || codeString == NULL)
    return (jint) RESULT_RUNTIME_ERROR;

  const char* code = (*env)->GetStringUTFChars(env, codeString, NULL);
  if (code == NULL)
    return (jint) RESULT_RUNTIME_ERROR;

  Result ret = RunString(vm, code);
  (*env)->ReleaseStringUTFChars(env, codeString, code);
  return (jint) ret;
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_executeSnippetWithViewNative(
    JNIEnv* env, jobject thiz, jstring snippet, jobject view) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || snippet == NULL)
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL) {
    if (bridge->lastEventView != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->lastEventView);
      bridge->lastEventView = NULL;
    }
    if (view != NULL)
      bridge->lastEventView = (*env)->NewGlobalRef(env, view);
  }

  const char* code = (*env)->GetStringUTFChars(env, snippet, NULL);
  if (code == NULL)
    return;

  Result result = run_snippet_with_pcall(vm, code);

  (*env)->ReleaseStringUTFChars(env, snippet, code);

  if (bridge != NULL && bridge->lastEventView != NULL) {
    (*env)->DeleteGlobalRef(env, bridge->lastEventView);
    bridge->lastEventView = NULL;
  }

  if (result != RESULT_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG,
        "Saynaa snippet-with-view execution failed with code: %d", (int) result);
  }
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_execute(
    JNIEnv* env, jobject thiz, jobject context) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL) {
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, "VM not initialized.");
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
  jstring source = (jstring) (*env)->GetObjectField(env, thiz, sourceField);
  (*env)->DeleteLocalRef(env, saynaaCls);

  if (source == NULL) {
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, "Source is null.");
    return;
  }

  const char* code = (*env)->GetStringUTFChars(env, source, NULL);
  if (code == NULL) {
    (*env)->DeleteLocalRef(env, source);
    return;
  }

  __android_log_print(ANDROID_LOG_INFO, SAYNAAJAVA_TAG,
      "Running script... source_len=%d preprocessor=disabled", (int) strlen(code));
  Result result = RunString(vm, code);

  (*env)->ReleaseStringUTFChars(env, source, code);
  (*env)->DeleteLocalRef(env, source);

  if (result != RESULT_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, "Saynaa execution failed with code: %d", (int) result);
    if (vm->fiber != NULL && vm->fiber->error != NULL && vm->fiber->error->data != NULL) {
      __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, "VM error: %s", vm->fiber->error->data);
    }
  } else {
    __android_log_print(ANDROID_LOG_INFO, SAYNAAJAVA_TAG, "Saynaa execution finished successfully.");
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
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG,
        "invokeCallbackNative failed for callbackId=%d err=%s", (int) callbackId, err);
    if (vm->fiber != NULL)
      vm->fiber->error = NULL;
  } else {
    __android_log_print(ANDROID_LOG_INFO, SAYNAAJAVA_TAG,
        "invokeCallbackNative succeeded for callbackId=%d", (int) callbackId);
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
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG,
        "invokeCallbackNative failed for callbackId=%d err=%s", (int) callbackId, err);
    if (vm->fiber != NULL)
      vm->fiber->error = NULL;
  } else {
    __android_log_print(ANDROID_LOG_INFO, SAYNAAJAVA_TAG,
        "invokeCallbackNative succeeded for callbackId=%d", (int) callbackId);
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
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG,
        "invokeCallbackMethodWithResultNative failed for callbackId=%d err=%s", (int) callbackId, err);
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

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL) {
    clear_callbacks(vm);
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
    clear_java_packages(bridge);
    free(bridge);
    SetUserData(vm, NULL);
  }

  FreeVM(vm);
  set_vm_ptr_on_saynaa(env, thiz, (jlong) 0);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  (void) vm;
  (void) reserved;
  return JNI_VERSION_1_6;
}
