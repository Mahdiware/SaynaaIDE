#pragma once

bool clear_jni_exception_with_log(JNIEnv* env, const char* where);
jclass safe_find_class(VM* vm, JNIEnv* env, const char* className, const char* where);
jmethodID safe_get_static_method_id(
    VM* vm, JNIEnv* env, jclass cls, const char* name, const char* sig, const char* where);
