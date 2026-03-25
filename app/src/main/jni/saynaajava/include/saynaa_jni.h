#pragma once

#include <jni.h>

VM* vm_from_saynaa(JNIEnv* env, jobject saynaaObject);
void set_vm_ptr_on_saynaa(JNIEnv* env, jobject saynaaObject, jlong ptr);
void ensure_activity_and_paths(JNIEnv* env, VM* vm, jobject thiz);

jclass saynaa_get_pcall_result_class(void);
jmethodID saynaa_get_pcall_result_ctor(void);
