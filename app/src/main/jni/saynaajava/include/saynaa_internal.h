#pragma once

#include "saynaa.h"
#include "saynaa_config.h"
#include "saynaa_vm.h"

#include <android/log.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// chdir
#include <unistd.h>

#define JAVA_REF_MAGIC 0x534A5246u /* SJRF */

// Macro to access a slot in the fiber's return array
#define SLOT(n) (vm->fiber->ret[n])

typedef struct JavaRef {
  unsigned int magic;
  JavaVM* jvm;
  jobject global;
} JavaRef;

typedef struct CallbackEntry CallbackEntry;
typedef struct PinnedHandleEntry PinnedHandleEntry;

typedef struct BridgeState {
  JavaVM* jvm;
  jclass javaBridgeClass;
  jobject saynaaObject;

  Handle* javaWrapperModule;
  Handle* clsJavaBase;
  Handle* clsJavaClass;
  Handle* clsJavaObject;
  Handle* clsJavaMethod;
  Handle* mainModule;

  jmethodID mFindClass;
  jmethodID mCreateJavaObject;
  jmethodID mCallJavaMethod;
  jmethodID mCallStaticJavaMethod;
  jmethodID mGetFieldValue;
  jmethodID mSetFieldValue;
  jmethodID mResolveCallbackInterface;
  jmethodID mCreateProxy;
  jmethodID mCreateNativeCallbackProxy;
  jmethodID mGetDefaultInterfaceMethodName;
  jmethodID mPushToSlot;
  jmethodID mSlotToJava;
  jmethodID mArgsArrayFromSlots;
  jmethodID mOnNativeError;

  int nextCallbackId;
  CallbackEntry* callbacks;
  int nextPinnedHandleId;
  PinnedHandleEntry* pinnedHandles;
  bool filesSearchPathAdded;
  bool closing;
} BridgeState;

struct CallbackEntry {
  int id;
  Handle* fnHandle;
  Handle* mapHandle;
  char* methodName;
  CallbackEntry* next;
};

struct PinnedHandleEntry {
  int id;
  Handle* handle;
  PinnedHandleEntry* next;
};

typedef enum { JAVA_CLASS, JAVA_OBJECT, JAVA_METHOD } JavaNativeType;

typedef struct {
  JavaNativeType type;
  JavaRef* reference;

  char* method_name;
  bool is_static;

} JavaNativeBase;

// typedef struct JavaExport {
//   const char* name;
//   nativeFn fn;
//   int arity;
//   const char* docstring;
// } JavaExport;

extern BridgeState* bridge_from_vm(VM* vm);
extern void throw_if_exception(VM* vm, JNIEnv* env, const char* prefix);
extern char* str_dup_c(const char* s);
extern Module* current_module_from_vm(VM* vm);
extern JNIEnv* env_from_jvm(JavaVM* jvm);
extern JavaRef* make_java_ref(JNIEnv* env, JavaVM* jvm, jobject obj);
extern bool create_java_instance(VM* vm, Handle** clsHandlePtr, JavaRef* ref, int outSlot);
extern bool clear_jni_exception_with_log(JNIEnv* env, const char* where);
extern jclass safe_find_class(VM* vm, JNIEnv* env, const char* className, const char* where);
extern jmethodID safe_get_static_method_id(
    VM* vm, JNIEnv* env, jclass cls, const char* name, const char* sig, const char* where);

extern void* new_java_class_instance(VM* vm);
extern void* new_java_object_instance(VM* vm);
extern void* new_java_method_instance(VM* vm);
extern void delete_java_instance(VM* vm, void* ptr);

extern void java_init(VM* vm);

extern void java_class_getter(VM* vm);
extern void java_class_call(VM* vm);
extern void java_object_getter(VM* vm);
extern void java_object_setter(VM* vm);
extern void java_method_call(VM* vm);

extern void java_class_str(VM* vm);
extern void java_object_str(VM* vm);
extern void java_method_str(VM* vm);
extern void java_method_getter(VM* vm);
extern jobject slot_to_java(JNIEnv* env, VM* vm, BridgeState* bridge, int slot);
extern bool ensure_wrapper_classes(VM* vm);

extern void android_stdout_write(VM* vm, const char* text);
extern void android_stderr_write(VM* vm, const char* text);

extern bool call_java_method(VM* vm, int num_args, bool is_static);

extern bool register_java_wrapper_classes(VM* vm);
extern void clear_callbacks(VM* vm);
extern int register_callback(VM* vm, int slot);
extern int register_map_callback(VM* vm, int mapSlot, const char* methodName);
extern CallbackEntry* find_callback(VM* vm, int callbackId);
extern int register_pinned_handle(VM* vm, Handle* handle);
extern Handle* find_pinned_handle(VM* vm, int handleId);
extern void clear_pinned_handles(VM* vm);
extern bool invoke_registered_callback(JNIEnv* env, VM* vm, BridgeState* bridge,
    CallbackEntry* entry, const char* runtimeMethodName, jobject args, jobject* outResult);
extern bool invoke_registered_callback_from_slots(JNIEnv* env, VM* vm, BridgeState* bridge,
    CallbackEntry* entry, const char* runtimeMethodName, int argStart, int argCount, jobject* outResult);
extern jobject create_native_callback_proxy(JNIEnv* env, VM* vm, BridgeState* bridge,
    jstring jInterface, const char* methodName, int callbackId);
extern void release_bridge_handle(VM* vm, Handle** handlePtr);
extern bool object_to_slot(JNIEnv* env, VM* vm, BridgeState* bridge, int startSlot, jobject object,
    const char* wrapErrorMessage);

extern void java_ref_destructor(void* ptr);
extern JavaRef* clone_java_ref(JNIEnv* env, JavaRef* src);
extern bool create_java_method_instance(
    VM* vm, JavaRef* target_ref, const char* method_name, bool is_static, int outSlot);
extern jobjectArray make_args_array(JNIEnv* env, VM* vm, BridgeState* bridge, int startSlot, int argc);
extern jstring get_java_object_name(JNIEnv* env, VM* vm, BridgeState* bridge, jobject target,
    const char* errorPrefix, const char* nullMessage);
extern Result saynaa_run_in_main_module(VM* vm, const char* source, const char* path_label);
extern Result saynaa_run_file_in_main_module(VM* vm, const char* path);
