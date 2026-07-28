#include "saynaa_internal.h"
#include "saynaa_jni.h"

VM* vm_from_saynaa(JNIEnv* env, jobject saynaaObject) {
  jclass saynaaCls = (*env)->GetObjectClass(env, saynaaObject);
  jfieldID ptrField = (*env)->GetFieldID(env, saynaaCls, "vm", "J");
  jlong ptr = (*env)->GetLongField(env, saynaaObject, ptrField);
  (*env)->DeleteLocalRef(env, saynaaCls);

  return (VM*) (intptr_t) ptr;
}


void set_vm_ptr_on_saynaa(JNIEnv* env, jobject saynaaObject, jlong ptr) {
  jclass saynaaCls = (*env)->GetObjectClass(env, saynaaObject);
  jfieldID ptrField = (*env)->GetFieldID(env, saynaaCls, "vm", "J");
  (*env)->SetLongField(env, saynaaObject, ptrField, ptr);
  (*env)->DeleteLocalRef(env, saynaaCls);
}
