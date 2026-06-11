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

bool object_to_slot(JNIEnv* env, VM* vm, BridgeState* bridge, int startSlot, jobject object,
    const char* wrapErrorMessage) {
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
      env, bridge->javaBridgeClass, bridge->mPushToSlot, saynaaObj, (jint) startSlot, object);

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