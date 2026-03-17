#pragma once

void throw_if_exception(VM* vm, JNIEnv* env, const char* prefix);
char* str_dup_c(const char* s);
char* normalize_java_package_prefix(const char* prefix);
char* massage_java_classname(const char* name);
Module* current_module_from_vm(VM* vm);
JNIEnv* env_from_jvm(JavaVM* jvm);
JavaRef* make_java_ref(JNIEnv* env, JavaVM* jvm, jobject obj);
bool create_java_instance(VM* vm, Handle** clsHandlePtr, JavaRef* ref, int outSlot);
bool clear_jni_exception_with_log(JNIEnv* env, const char* where);
jclass safe_find_class(VM* vm, JNIEnv* env, const char* className, const char* where);
jmethodID safe_get_static_method_id(
    VM* vm, JNIEnv* env, jclass cls, const char* name, const char* sig, const char* where);
void fn_bindClass(VM* vm);
void fn_new(VM* vm);
void fn_createProxy(VM* vm);
void fn_loadLib(VM* vm);
void fn_astable(VM* vm);
void fn_instanceof(VM* vm);
void fn_javaToString(VM* vm);
void java_class_str(VM* vm);
void java_object_str(VM* vm);
void java_method_str(VM* vm);
bool java_to_slot(JNIEnv* env, VM* vm, BridgeState* bridge, int slot, jobject obj);
jobject slot_to_java(JNIEnv* env, VM* vm, BridgeState* bridge, int slot);
bool ensure_java_module(VM* vm);
bool ensure_wrapper_classes(VM* vm);
