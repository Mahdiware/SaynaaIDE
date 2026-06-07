#include "saynaa_internal.h"

BridgeState* bridge_from_vm(VM* vm) {
  return (BridgeState*) GetUserData(vm);
}

void release_bridge_handle(VM* vm, Handle** handle) {
  if (vm == NULL || handle == NULL || *handle == NULL)
    return;

  releaseHandle(vm, *handle);
  *handle = NULL;
}

jobjectArray wrap_single_object(JNIEnv* env, jobject obj, jclass objClass) {
  if (objClass == NULL)
    return NULL;

  jobjectArray arr = (*env)->NewObjectArray(env, 1, objClass, NULL);
  if (arr == NULL)
    return NULL;

  (*env)->SetObjectArrayElement(env, arr, 0, obj);
  return arr;
}

bool object_to_slot(JNIEnv* env, VM* vm, BridgeState* bridge, int slot, jobject obj, const char* wrapErrorMessage) {
  if (env == NULL || vm == NULL || bridge == NULL) {
    SetRuntimeError(vm, "Invalid Java bridge state.");
    return false;
  }

  if (bridge->javaBridgeClass == NULL || bridge->mPushToSlot == NULL || bridge->saynaaObject == NULL) {
    SetRuntimeError(vm, "Java bridge not initialized.");
    return false;
  }

  jobject saynaaObj = (*env)->NewLocalRef(env, bridge->saynaaObject);
  if (saynaaObj == NULL) {
    SetRuntimeError(vm, "Failed to access Saynaa instance.");
    return false;
  }

  jobjectArray finalArray = wrap_single_object(env, obj, bridge->JavaObjectClass);

  if (finalArray == NULL) {
    (*env)->DeleteLocalRef(env, saynaaObj);
    SetRuntimeError(vm, "Failed to build argument array.");
    return false;
  }

  // Call Java static method
  jboolean ok = (*env)->CallStaticBooleanMethod(
      env, bridge->javaBridgeClass, bridge->mPushToSlot, saynaaObj, (jint) slot, finalArray);

  // Cleanup
  (*env)->DeleteLocalRef(env, saynaaObj);
  (*env)->DeleteLocalRef(env, finalArray);

  // Exception handling
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);

    SetRuntimeError(vm, "JavaBridge.pushToSlot failed (see logcat)");
    return false;
  }

  // Result check
  if (ok != JNI_TRUE) {
    SetRuntimeError(vm, wrapErrorMessage == NULL ? "Failed to convert Java value." : wrapErrorMessage);
    return false;
  }

  return true;
}

bool objects_to_slot(JNIEnv* env, VM* vm, BridgeState* bridge, int startSlot, jobjectArray arr, const char* wrapErrorMessage) {
  if (env == NULL || vm == NULL || bridge == NULL) {
    SetRuntimeError(vm, "Invalid Java bridge state.");
    return false;
  }

  if (bridge->javaBridgeClass == NULL || bridge->mPushToSlot == NULL || bridge->saynaaObject == NULL) {
    SetRuntimeError(vm, "Java bridge not initialized.");
    return false;
  }

  jobject saynaaObj = (*env)->NewLocalRef(env, bridge->saynaaObject);
  if (saynaaObj == NULL) {
    SetRuntimeError(vm, "Failed to access Saynaa instance.");
    return false;
  }

  jboolean ok = (*env)->CallStaticBooleanMethod(
      env, bridge->javaBridgeClass, bridge->mPushToSlot, saynaaObj, (jint) startSlot, arr);

  (*env)->DeleteLocalRef(env, saynaaObj);

  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionDescribe(env);
    (*env)->ExceptionClear(env);

    SetRuntimeError(vm, "JavaBridge.pushToSlot failed (see logcat)");
    return false;
  }

  if (ok != JNI_TRUE) {
    SetRuntimeError(vm, wrapErrorMessage == NULL ? "Failed to convert Java values." : wrapErrorMessage);
    return false;
  }

  return true;
}