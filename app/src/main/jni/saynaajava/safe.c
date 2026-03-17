#include "internal.h"

bool clear_jni_exception_with_log(JNIEnv* env, const char* where) {
  if (env == NULL)
    return false;
  if (!(*env)->ExceptionCheck(env))
    return false;

  __android_log_print(
      ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, "JNI exception at %s", where == NULL ? "<unknown>" : where);
  (*env)->ExceptionDescribe(env);
  (*env)->ExceptionClear(env);
  return true;
}

jclass safe_find_class(VM* vm, JNIEnv* env, const char* className, const char* where) {
  if (env == NULL)
    return NULL;

  jclass cls = (*env)->FindClass(env, className == NULL ? "" : className);
  if (cls == NULL) {
    clear_jni_exception_with_log(env, where);
    if (vm != NULL)
      SetRuntimeErrorFmt(vm, "JNI FindClass failed at %s for %s",
          where == NULL ? "<unknown>" : where, className == NULL ? "<null>" : className);
  }
  return cls;
}

jmethodID safe_get_static_method_id(
    VM* vm, JNIEnv* env, jclass cls, const char* name, const char* sig, const char* where) {
  if (env == NULL || cls == NULL)
    return NULL;

  jmethodID mid = (*env)->GetStaticMethodID(env, cls, name == NULL ? "" : name, sig == NULL ? "" : sig);
  if (mid == NULL) {
    clear_jni_exception_with_log(env, where);
    if (vm != NULL)
      SetRuntimeErrorFmt(vm, "JNI GetStaticMethodID failed at %s for %s%s",
          where == NULL ? "<unknown>" : where, name == NULL ? "<null>" : name, sig == NULL ? "" : sig);
  }
  return mid;
}
