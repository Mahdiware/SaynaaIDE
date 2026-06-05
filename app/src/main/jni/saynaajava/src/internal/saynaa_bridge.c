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

  jboolean ok = (*env)->CallStaticBooleanMethod(
      env, bridge->javaBridgeClass, bridge->mPushToSlot, saynaaObj, (jint) slot, obj);
  (*env)->DeleteLocalRef(env, saynaaObj);

  if ((*env)->ExceptionCheck(env)) {
    jthrowable ex = (*env)->ExceptionOccurred(env);

    (*env)->ExceptionDescribe(env); // prints to logcat
    (*env)->ExceptionClear(env);

    SetRuntimeError(vm, "JavaBridge.pushToSlot failed (see logcat)");
    return false;
  }

  if (ok != JNI_TRUE) {
    SetRuntimeError(vm, wrapErrorMessage == NULL ? "Failed to convert Java value." : wrapErrorMessage);
    return false;
  }

  return true;
}
