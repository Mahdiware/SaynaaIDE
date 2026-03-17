#include "internal.h"

void* new_java_class_instance(VM* vm) {
  (void) vm;
  return calloc(1, sizeof(JavaClassNative));
}

void delete_java_class_instance(VM* vm, void* ptr) {
  (void) vm;
  JavaClassNative* inst = (JavaClassNative*) ptr;
  if (inst != NULL) {
    if (inst->class_ref != NULL)
      java_ref_destructor(inst->class_ref);
    free(inst);
  }
}

void* new_java_object_instance(VM* vm) {
  (void) vm;
  return calloc(1, sizeof(JavaObjectNative));
}

void delete_java_object_instance(VM* vm, void* ptr) {
  (void) vm;
  JavaObjectNative* inst = (JavaObjectNative*) ptr;
  if (inst != NULL) {
    if (inst->object_ref != NULL)
      java_ref_destructor(inst->object_ref);
    free(inst);
  }
}

void* new_java_method_instance(VM* vm) {
  (void) vm;
  return calloc(1, sizeof(JavaMethodNative));
}

void delete_java_method_instance(VM* vm, void* ptr) {
  (void) vm;
  JavaMethodNative* inst = (JavaMethodNative*) ptr;
  if (inst != NULL) {
    if (inst->target_ref != NULL)
      java_ref_destructor(inst->target_ref);
    if (inst->method_name != NULL)
      free(inst->method_name);
    free(inst);
  }
}

void java_class_init(VM* vm) {
  JavaClassNative* thiz = (JavaClassNative*) GetThis(vm);
  if (thiz == NULL)
    return;
  if (!ValidateSlotType(vm, 1, vPOINTER))
    return;
  thiz->class_ref = (JavaRef*) GetSlotPointer(vm, 1, NULL, NULL);
}

void java_object_init(VM* vm) {
  JavaObjectNative* thiz = (JavaObjectNative*) GetThis(vm);
  if (thiz == NULL)
    return;
  if (!ValidateSlotType(vm, 1, vPOINTER))
    return;
  thiz->object_ref = (JavaRef*) GetSlotPointer(vm, 1, NULL, NULL);
}

void java_method_init(VM* vm) {
  JavaMethodNative* thiz = (JavaMethodNative*) GetThis(vm);
  if (thiz == NULL)
    return;
  if (!ValidateSlotType(vm, 1, vPOINTER))
    return;
  if (!ValidateSlotString(vm, 2, NULL, NULL))
    return;

  thiz->target_ref = (JavaRef*) GetSlotPointer(vm, 1, NULL, NULL);
  const char* method_name = GetSlotString(vm, 2, NULL);
  thiz->method_name = str_dup_c(method_name);

  if (GetArgc(vm) >= 3) {
    thiz->is_static = GetSlotBool(vm, 3);
  }
}

void java_class_getter(VM* vm) {
  JavaClassNative* thiz = (JavaClassNative*) GetThis(vm);
  if (thiz == NULL || thiz->class_ref == NULL)
    return;
  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;

  const char* name = GetSlotString(vm, 1, NULL);
  JNIEnv* env = env_from_jvm(thiz->class_ref->jvm);
  if (env == NULL) {
    SetRuntimeError(vm, "Invalid JNI Environment.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL && bridge->javaBridgeClass != NULL && bridge->mGetFieldValue != NULL) {
    jobject classObj = (*env)->NewLocalRef(env, thiz->class_ref->global);
    jstring jField = (*env)->NewStringUTF(env, name == NULL ? "" : name);

    jobject fieldValue = (*env)->CallStaticObjectMethod(
        env, bridge->javaBridgeClass, bridge->mGetFieldValue, classObj, jField);

    (*env)->DeleteLocalRef(env, jField);
    if (classObj != NULL)
      (*env)->DeleteLocalRef(env, classObj);

    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "JavaClass._getter field access failed");
      return;
    }

    if (fieldValue != NULL) {
      put_java_result(vm, env, bridge, fieldValue, 0);
      (*env)->DeleteLocalRef(env, fieldValue);
      return;
    }
  }

  // Try resolving nested class references like View.OnClickListener -> android.view.View$OnClickListener
  if (bridge != NULL && bridge->javaBridgeClass != NULL && bridge->mFindClass != NULL) {
    jobject classObj = (*env)->NewLocalRef(env, thiz->class_ref->global);
    jstring ownerNameObj = get_java_object_name(env, vm, bridge, classObj,
        "JavaClass._getter getName() failed", "JavaClass._getter failed to resolve class name.");
    if (classObj != NULL)
      (*env)->DeleteLocalRef(env, classObj);

    if (ownerNameObj != NULL) {
      const char* ownerName = (*env)->GetStringUTFChars(env, ownerNameObj, NULL);
      if (ownerName != NULL && name != NULL && name[0] != '\0') {
        size_t ownerLen = strlen(ownerName);
        size_t childLen = strlen(name);
        char* nestedName = (char*) malloc(ownerLen + 1 + childLen + 1);
        if (nestedName != NULL) {
          memcpy(nestedName, ownerName, ownerLen);
          nestedName[ownerLen] = '$';
          memcpy(nestedName + ownerLen + 1, name, childLen);
          nestedName[ownerLen + 1 + childLen] = '\0';

          jstring jNested = (*env)->NewStringUTF(env, nestedName);
          jobject nestedClass = NULL;
          if (jNested != NULL) {
            nestedClass = (*env)->CallStaticObjectMethod(
                env, bridge->javaBridgeClass, bridge->mFindClass, jNested);
            (*env)->DeleteLocalRef(env, jNested);
          }

          if ((*env)->ExceptionCheck(env)) {
            free(nestedName);
            (*env)->ReleaseStringUTFChars(env, ownerNameObj, ownerName);
            (*env)->DeleteLocalRef(env, ownerNameObj);
            throw_if_exception(vm, env, "JavaClass._getter nested class lookup failed");
            return;
          }

          if (nestedClass != NULL) {
            JavaRef* ref = make_java_ref(env, bridge->jvm, nestedClass);
            (*env)->DeleteLocalRef(env, nestedClass);
            free(nestedName);
            (*env)->ReleaseStringUTFChars(env, ownerNameObj, ownerName);
            (*env)->DeleteLocalRef(env, ownerNameObj);
            if (ref == NULL) {
              SetRuntimeError(vm, "Failed to wrap nested Java class reference.");
              return;
            }
            create_java_instance(vm, &bridge->clsJavaClass, ref, 0);
            return;
          }

          free(nestedName);
        }
      }

      if (ownerName != NULL)
        (*env)->ReleaseStringUTFChars(env, ownerNameObj, ownerName);
      (*env)->DeleteLocalRef(env, ownerNameObj);
    }
  }

  JavaRef* target = clone_java_ref(env, thiz->class_ref);
  if (target == NULL) {
    SetRuntimeError(vm, "Failed to clone Java class reference.");
    return;
  }

  create_java_method_instance(vm, target, name, true, 0);
}

void java_class_str(VM* vm) {
  (void) vm;
  setSlotString(vm, 0, "<JavaClass>");
}

void java_class_call(VM* vm) {
  JavaClassNative* thiz = (JavaClassNative*) GetThis(vm);
  if (thiz == NULL || thiz->class_ref == NULL) {
    SetRuntimeError(vm, "Invalid JavaClass instance.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);

  jobject classObj = (*env)->NewLocalRef(env, thiz->class_ref->global);
  if (classObj == NULL) {
    SetRuntimeError(vm, "JavaClass._call failed to access class reference.");
    return;
  }

  bool isInterface = false;
  jclass clsClass = (*env)->FindClass(env, "java/lang/Class");
  if (clsClass != NULL) {
    jmethodID midIsInterface = (*env)->GetMethodID(env, clsClass, "isInterface", "()Z");
    if (midIsInterface != NULL) {
      jboolean result = (*env)->CallBooleanMethod(env, classObj, midIsInterface);
      if ((*env)->ExceptionCheck(env)) {
        (*env)->DeleteLocalRef(env, clsClass);
        (*env)->DeleteLocalRef(env, classObj);
        throw_if_exception(vm, env, "JavaClass._call isInterface failed");
        return;
      }
      isInterface = (result == JNI_TRUE);
    }
    (*env)->DeleteLocalRef(env, clsClass);
  }

  jstring classNameObj = get_java_object_name(env, vm, bridge, classObj,
      "JavaClass._call getName() failed", "JavaClass._call failed to resolve class name.");
  if (classNameObj == NULL) {
    (*env)->DeleteLocalRef(env, classObj);
    return;
  }

  int argc = GetArgc(vm);

  // AndLua-compatible sugar:
  // InterfaceClass{ onEvent = fn } / InterfaceClass(function...) / InterfaceClass("script")
  // should create a Java proxy instead of invoking constructor.
  if (isInterface && argc == 1) {
    VarType callbackType = GetSlotType(vm, 1);
    if (callbackType == vMAP || callbackType == vCLOSURE || callbackType == vSTRING) {
      const char* methodName = "*";

      if (bridge->mGetDefaultInterfaceMethodName != NULL) {
        jobject inferredObj = (*env)->CallStaticObjectMethod(
            env, bridge->javaBridgeClass, bridge->mGetDefaultInterfaceMethodName, classNameObj);
        if ((*env)->ExceptionCheck(env)) {
          (*env)->DeleteLocalRef(env, classNameObj);
          (*env)->DeleteLocalRef(env, classObj);
          throw_if_exception(vm, env, "JavaClass._call infer method name failed");
          return;
        }

        if (inferredObj != NULL) {
          const char* inferred = (*env)->GetStringUTFChars(env, (jstring) inferredObj, NULL);
          if (inferred != NULL && inferred[0] != '\0')
            methodName = inferred;

          if (callbackType == vCLOSURE && (methodName == NULL || strcmp(methodName, "*") == 0)) {
            if (inferred != NULL)
              (*env)->ReleaseStringUTFChars(env, (jstring) inferredObj, inferred);
            (*env)->DeleteLocalRef(env, inferredObj);
            (*env)->DeleteLocalRef(env, classNameObj);
            (*env)->DeleteLocalRef(env, classObj);
            SetRuntimeError(vm, "InterfaceClass(function) requires SAM interface or explicit "
                                "createProxy(interface, method, fn).");
            return;
          }

          int callbackId = 0;
          if (callbackType == vCLOSURE) {
            callbackId = register_callback(vm, 1);
          } else if (callbackType == vMAP) {
            callbackId = register_map_callback(vm, 1, methodName);
          }

          if (callbackType == vSTRING) {
            const char* script = GetSlotString(vm, 1, NULL);
            jobject saynaaObj = (*env)->NewLocalRef(env, bridge->saynaaObject);
            if (saynaaObj == NULL) {
              if (inferred != NULL)
                (*env)->ReleaseStringUTFChars(env, (jstring) inferredObj, inferred);
              (*env)->DeleteLocalRef(env, inferredObj);
              (*env)->DeleteLocalRef(env, classNameObj);
              (*env)->DeleteLocalRef(env, classObj);
              SetRuntimeError(vm, "Failed to access Saynaa object.");
              return;
            }

            jstring jMethod = (*env)->NewStringUTF(env, methodName == NULL ? "" : methodName);
            jstring jScript = (*env)->NewStringUTF(env, script == NULL ? "" : script);
            jobject proxy = (*env)->CallStaticObjectMethod(env, bridge->javaBridgeClass,
                bridge->mCreateProxy, saynaaObj, classNameObj, jMethod, jScript);

            (*env)->DeleteLocalRef(env, jScript);
            (*env)->DeleteLocalRef(env, jMethod);
            (*env)->DeleteLocalRef(env, saynaaObj);

            if ((*env)->ExceptionCheck(env)) {
              if (inferred != NULL)
                (*env)->ReleaseStringUTFChars(env, (jstring) inferredObj, inferred);
              (*env)->DeleteLocalRef(env, inferredObj);
              (*env)->DeleteLocalRef(env, classNameObj);
              (*env)->DeleteLocalRef(env, classObj);
              throw_if_exception(vm, env, "JavaClass._call createProxy failed");
              return;
            }

            put_java_result(vm, env, bridge, proxy, 0);
            if (proxy != NULL)
              (*env)->DeleteLocalRef(env, proxy);

            if (inferred != NULL)
              (*env)->ReleaseStringUTFChars(env, (jstring) inferredObj, inferred);
            (*env)->DeleteLocalRef(env, inferredObj);
            (*env)->DeleteLocalRef(env, classNameObj);
            (*env)->DeleteLocalRef(env, classObj);
            return;
          }

          if (callbackId == 0) {
            if (inferred != NULL)
              (*env)->ReleaseStringUTFChars(env, (jstring) inferredObj, inferred);
            (*env)->DeleteLocalRef(env, inferredObj);
            (*env)->DeleteLocalRef(env, classNameObj);
            (*env)->DeleteLocalRef(env, classObj);
            return;
          }

          jobject proxy = create_native_callback_proxy(env, vm, bridge, classNameObj, methodName, callbackId);
          if (proxy == NULL) {
            if (inferred != NULL)
              (*env)->ReleaseStringUTFChars(env, (jstring) inferredObj, inferred);
            (*env)->DeleteLocalRef(env, inferredObj);
            (*env)->DeleteLocalRef(env, classNameObj);
            (*env)->DeleteLocalRef(env, classObj);
            return;
          }

          put_java_result(vm, env, bridge, proxy, 0);
          (*env)->DeleteLocalRef(env, proxy);
          if (inferred != NULL)
            (*env)->ReleaseStringUTFChars(env, (jstring) inferredObj, inferred);
          (*env)->DeleteLocalRef(env, inferredObj);
          (*env)->DeleteLocalRef(env, classNameObj);
          (*env)->DeleteLocalRef(env, classObj);
          return;
        }
      }

      // Fallback if method inference API is unavailable.
      if (callbackType == vCLOSURE) {
        (*env)->DeleteLocalRef(env, classNameObj);
        (*env)->DeleteLocalRef(env, classObj);
        SetRuntimeError(vm, "InterfaceClass(function) requires method inference support.");
        return;
      }

      int callbackId = register_map_callback(vm, 1, "*");
      if (callbackType == vMAP && callbackId > 0) {
        jobject proxy = create_native_callback_proxy(env, vm, bridge, classNameObj, "*", callbackId);
        if (proxy != NULL) {
          put_java_result(vm, env, bridge, proxy, 0);
          (*env)->DeleteLocalRef(env, proxy);
          (*env)->DeleteLocalRef(env, classNameObj);
          (*env)->DeleteLocalRef(env, classObj);
          return;
        }
      }
    }
  }

  jobjectArray args = make_args_array(env, vm, bridge, 1, argc);
  if (VM_HAS_ERROR(vm) || (args == NULL && argc > 0)) {
    if (!VM_HAS_ERROR(vm) && argc > 0)
      SetRuntimeError(vm, "JavaClass._call argument conversion failed.");
    if (args != NULL)
      (*env)->DeleteLocalRef(env, args);
    if (classNameObj != NULL)
      (*env)->DeleteLocalRef(env, classNameObj);
    (*env)->DeleteLocalRef(env, classObj);
    return;
  }

  jobject obj = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mCreateJavaObject, classNameObj, args);

  if (args != NULL)
    (*env)->DeleteLocalRef(env, args);
  (*env)->DeleteLocalRef(env, classNameObj);
  (*env)->DeleteLocalRef(env, classObj);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "JavaClass._call constructor failed");
    return;
  }

  put_java_result(vm, env, bridge, obj, 0);
  if (obj != NULL)
    (*env)->DeleteLocalRef(env, obj);
}

void java_object_getter(VM* vm) {
  JavaObjectNative* thiz = (JavaObjectNative*) GetThis(vm);
  if (thiz == NULL || thiz->object_ref == NULL)
    return;
  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;

  const char* name = GetSlotString(vm, 1, NULL);
  JNIEnv* env = env_from_jvm(thiz->object_ref->jvm);
  if (env == NULL) {
    SetRuntimeError(vm, "Invalid JNI Environment.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL && bridge->javaBridgeClass != NULL && bridge->mGetFieldValue != NULL) {
    jobject obj = (*env)->NewLocalRef(env, thiz->object_ref->global);
    jstring jField = (*env)->NewStringUTF(env, name == NULL ? "" : name);

    jobject fieldValue = (*env)->CallStaticObjectMethod(
        env, bridge->javaBridgeClass, bridge->mGetFieldValue, obj, jField);

    (*env)->DeleteLocalRef(env, jField);
    if (obj != NULL)
      (*env)->DeleteLocalRef(env, obj);

    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "JavaObject._getter field access failed");
      return;
    }

    if (fieldValue != NULL) {
      put_java_result(vm, env, bridge, fieldValue, 0);
      (*env)->DeleteLocalRef(env, fieldValue);
      return;
    }
  }

  JavaRef* target = clone_java_ref(env, thiz->object_ref);
  if (target == NULL) {
    SetRuntimeError(vm, "Failed to clone Java object reference.");
    return;
  }

  create_java_method_instance(vm, target, name, false, 0);
}

void java_object_setter(VM* vm) {
  JavaObjectNative* thiz = (JavaObjectNative*) GetThis(vm);
  if (thiz == NULL || thiz->object_ref == NULL)
    return;
  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);
  jobject target = (*env)->NewLocalRef(env, thiz->object_ref->global);
  const char* fieldName = GetSlotString(vm, 1, NULL);
  jobject value = slot_to_java(env, vm, bridge, 2);

  jstring jFieldName = (*env)->NewStringUTF(env, fieldName);
  jboolean ok = (*env)->CallStaticBooleanMethod(
      env, bridge->javaBridgeClass, bridge->mSetFieldValue, target, jFieldName, value);

  if (value != NULL)
    (*env)->DeleteLocalRef(env, value);
  (*env)->DeleteLocalRef(env, target);
  (*env)->DeleteLocalRef(env, jFieldName);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "JavaObject._setter failed");
    return;
  }

  setSlotBool(vm, 0, ok == JNI_TRUE);
}

void java_object_str(VM* vm) {
  (void) vm;
  setSlotString(vm, 0, "<JavaObject>");
}

void java_method_call(VM* vm) {
  JavaMethodNative* thiz = (JavaMethodNative*) GetThis(vm);
  if (thiz == NULL || thiz->target_ref == NULL || thiz->method_name == NULL) {
    SetRuntimeError(vm, "Invalid JavaMethod instance.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);
  int argc = GetArgc(vm);
  jobjectArray args = make_args_array(env, vm, bridge, 1, argc);
  jobject ret = NULL;

  if (thiz->is_static) {
    jobject classObj = (*env)->NewLocalRef(env, thiz->target_ref->global);
    jstring classNameObj = get_java_object_name(env, vm, bridge, classObj,
        "JavaMethod._call static getName failed", "JavaMethod._call static getName failed");
    (*env)->DeleteLocalRef(env, classObj);

    if (classNameObj == NULL) {
      if (args != NULL)
        (*env)->DeleteLocalRef(env, args);
      return;
    }

    jstring jMethod = (*env)->NewStringUTF(env, thiz->method_name);
    ret = (*env)->CallStaticObjectMethod(
        env, bridge->javaBridgeClass, bridge->mCallStaticJavaMethod, classNameObj, jMethod, args);

    (*env)->DeleteLocalRef(env, jMethod);
    (*env)->DeleteLocalRef(env, classNameObj);
  } else {
    jobject target = (*env)->NewLocalRef(env, thiz->target_ref->global);
    jstring jMethod = (*env)->NewStringUTF(env, thiz->method_name);
    ret = (*env)->CallStaticObjectMethod(
        env, bridge->javaBridgeClass, bridge->mCallJavaMethod, target, jMethod, args);
    (*env)->DeleteLocalRef(env, jMethod);
    (*env)->DeleteLocalRef(env, target);
  }

  if (args != NULL)
    (*env)->DeleteLocalRef(env, args);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "JavaMethod._call failed");
    return;
  }

  put_java_result(vm, env, bridge, ret, 0);
  if (ret != NULL)
    (*env)->DeleteLocalRef(env, ret);
}

void java_method_str(VM* vm) {
  JavaMethodNative* thiz = (JavaMethodNative*) GetThis(vm);
  if (thiz != NULL && thiz->method_name != NULL) {
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "<JavaMethod %s>", thiz->method_name);
    setSlotString(vm, 0, buffer);
    return;
  }
  setSlotString(vm, 0, "<JavaMethod>");
}

void fn_activity(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL) {
    setSlotNull(vm, 0);
    return;
  }
  wrap_bridge_global(vm, bridge->activity, 0);
}

void fn_eventView(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL) {
    setSlotNull(vm, 0);
    return;
  }
  wrap_bridge_global(vm, bridge->lastEventView, 0);
}
