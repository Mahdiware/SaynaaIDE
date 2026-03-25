#include "saynaa_internal.h"
#include "saynaa_jni.h"

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

void ensure_activity_and_paths(JNIEnv* env, VM* vm, jobject thiz) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL || env == NULL || thiz == NULL)
    return;

  jclass saynaaCls = (*env)->GetObjectClass(env, thiz);
  if (saynaaCls == NULL)
    return;

  jfieldID contextField = (*env)->GetFieldID(env, saynaaCls, "context", "Landroid/content/Context;");
  jobject context = NULL;
  if (contextField != NULL) {
    context = (*env)->GetObjectField(env, thiz, contextField);
  }
  (*env)->DeleteLocalRef(env, saynaaCls);

  if (context == NULL)
    return;

  ensure_files_search_path(vm, bridge, env, context);

  if (bridge->activity != NULL) {
    (*env)->DeleteGlobalRef(env, bridge->activity);
    bridge->activity = NULL;
  }
  bridge->activity = (*env)->NewGlobalRef(env, context);

  (*env)->DeleteLocalRef(env, context);
}
