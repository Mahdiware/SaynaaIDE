#pragma once

#include "../saynaa/src/cli/saynaa.h"
#include "../saynaa/src/runtime/saynaa_vm.h"

#include <android/log.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAYNAAJAVA_TAG "saynaajava"
#define JAVA_REF_MAGIC 0x534A5246u /* SJRF */

typedef struct JavaRef {
  unsigned int magic;
  JavaVM* jvm;
  jobject global;
} JavaRef;

typedef struct CallbackEntry CallbackEntry;
typedef struct JavaPackageEntry JavaPackageEntry;

struct JavaPackageEntry {
  char* prefix;
  JavaPackageEntry* next;
};

typedef struct BridgeState {
  JavaVM* jvm;
  jclass javaBridgeClass;
  jobject activity;
  jobject saynaaObject;
  jobject lastEventView;

  Handle* javaModule;
  Handle* javaWrapperModule;
  Handle* clsJavaClass;
  Handle* clsJavaObject;
  Handle* clsJavaMethod;

  jmethodID mFindClass;
  jmethodID mCreateJavaObject;
  jmethodID mCallJavaMethod;
  jmethodID mCallStaticJavaMethod;
  jmethodID mGetFieldValue;
  jmethodID mSetFieldValue;
  jmethodID mCreateProxy;
  jmethodID mCreateNativeCallbackProxy;
  jmethodID mGetDefaultInterfaceMethodName;

  int nextCallbackId;
  CallbackEntry* callbacks;
  JavaPackageEntry* packages;
  bool filesSearchPathAdded;
} BridgeState;

struct CallbackEntry {
  int id;
  Handle* fnHandle;
  Handle* mapHandle;
  char* methodName;
  CallbackEntry* next;
};

typedef struct JavaClassNative {
  JavaRef* class_ref;
} JavaClassNative;

typedef struct JavaObjectNative {
  JavaRef* object_ref;
} JavaObjectNative;

typedef struct JavaMethodNative {
  JavaRef* target_ref;
  char* method_name;
  bool is_static;
} JavaMethodNative;

typedef struct JavaSimpleClasses {
  jclass stringClass;
  jclass booleanClass;
  jclass numberClass;
} JavaSimpleClasses;

typedef struct JavaExport {
  const char* name;
  nativeFn fn;
  int arity;
  const char* docstring;
} JavaExport;

extern BridgeState* bridge_from_vm(VM* vm);
extern void throw_if_exception(VM* vm, JNIEnv* env, const char* prefix);
extern char* str_dup_c(const char* s);
extern char* normalize_java_package_prefix(const char* prefix);
extern char* massage_java_classname(const char* name);
extern Module* current_module_from_vm(VM* vm);
extern JNIEnv* env_from_jvm(JavaVM* jvm);
extern JavaRef* make_java_ref(JNIEnv* env, JavaVM* jvm, jobject obj);
extern bool create_java_instance(VM* vm, Handle** clsHandlePtr, JavaRef* ref, int outSlot);
extern bool clear_jni_exception_with_log(JNIEnv* env, const char* where);
extern jclass safe_find_class(VM* vm, JNIEnv* env, const char* className, const char* where);
extern jmethodID safe_get_static_method_id(
    VM* vm, JNIEnv* env, jclass cls, const char* name, const char* sig, const char* where);
extern void fn_bindClass(VM* vm);
extern void fn_new(VM* vm);
extern void fn_createProxy(VM* vm);
extern void fn_loadLib(VM* vm);
extern void fn_astable(VM* vm);
extern void fn_instanceof(VM* vm);
extern void fn_javaToString(VM* vm);
extern void fn_activity(VM* vm);
extern void fn_eventView(VM* vm);

extern void* new_java_class_instance(VM* vm);
extern void delete_java_class_instance(VM* vm, void* ptr);
extern void* new_java_object_instance(VM* vm);
extern void delete_java_object_instance(VM* vm, void* ptr);
extern void* new_java_method_instance(VM* vm);
extern void delete_java_method_instance(VM* vm, void* ptr);

extern void java_class_init(VM* vm);
extern void java_object_init(VM* vm);
extern void java_method_init(VM* vm);

extern void java_class_getter(VM* vm);
extern void java_class_call(VM* vm);
extern void java_object_getter(VM* vm);
extern void java_object_setter(VM* vm);
extern void java_method_call(VM* vm);

extern void java_class_str(VM* vm);
extern void java_object_str(VM* vm);
extern void java_method_str(VM* vm);
extern bool java_to_slot(JNIEnv* env, VM* vm, BridgeState* bridge, int slot, jobject obj);
extern jobject slot_to_java(JNIEnv* env, VM* vm, BridgeState* bridge, int slot);
extern bool ensure_java_module(VM* vm);
extern bool ensure_wrapper_classes(VM* vm);

extern void ensure_default_java_packages(BridgeState* bridge);
extern void clear_java_packages(BridgeState* bridge);
extern void android_stdout_write(VM* vm, const char* text);
extern void android_stderr_write(VM* vm, const char* text);

extern bool call_java_method(VM* vm, int num_args, bool is_static);

extern bool register_java_wrapper_classes(VM* vm);
extern void register_java_api(VM* vm);
extern void ensure_files_search_path(VM* vm, BridgeState* bridge, JNIEnv* env, jobject context);
extern void clear_callbacks(VM* vm);
extern int register_callback(VM* vm, int slot);
extern int register_map_callback(VM* vm, int mapSlot, const char* methodName);
extern CallbackEntry* find_callback(VM* vm, int callbackId);
extern bool invoke_registered_callback(JNIEnv* env, VM* vm, BridgeState* bridge,
    CallbackEntry* entry, const char* runtimeMethodName, jobject args, jobject* outResult);
extern jobject create_native_callback_proxy(JNIEnv* env, VM* vm, BridgeState* bridge, jstring jInterface,
  const char* methodName, int callbackId);
extern void release_bridge_handle(VM* vm, Handle** handlePtr);

extern void java_ref_destructor(void* ptr);
extern JavaRef* clone_java_ref(JNIEnv* env, JavaRef* src);
extern bool create_java_method_instance(
    VM* vm, JavaRef* target_ref, const char* method_name, bool is_static, int outSlot);
extern bool put_java_result(VM* vm, JNIEnv* env, BridgeState* bridge, jobject obj, int slot);
extern jobjectArray make_args_array(JNIEnv* env, VM* vm, BridgeState* bridge, int startSlot, int argc);
extern bool wrap_bridge_global(VM* vm, jobject globalRef, int outSlot);
extern jstring get_java_object_name(JNIEnv* env, VM* vm, BridgeState* bridge, jobject target,
    const char* errorPrefix, const char* nullMessage);
extern VM* vm_from_saynaa(JNIEnv* env, jobject saynaaObject);
extern void set_vm_ptr_on_saynaa(JNIEnv* env, jobject saynaaObject, jlong ptr);

extern jclass safe_find_class(VM* vm, JNIEnv* env, const char* className, const char* where);
extern jmethodID safe_get_static_method_id(
    VM* vm, JNIEnv* env, jclass cls, const char* name, const char* sig, const char* where);
extern bool clear_jni_exception_with_log(JNIEnv* env, const char* where);
