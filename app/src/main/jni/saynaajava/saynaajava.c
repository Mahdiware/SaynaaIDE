#include "../saynaa/cli/saynaa.h"
#include "../saynaa/runtime/saynaa_vm.h"

#include <android/log.h>
#include <jni.h>
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

static BridgeState* bridge_from_vm(VM* vm) {
  return (BridgeState*) GetUserData(vm);
}

static void throw_if_exception(VM* vm, JNIEnv* env, const char* prefix);
static char* str_dup_c(const char* s);
static char* normalize_java_package_prefix(const char* prefix);
static char* massage_java_classname(const char* name);
static Module* current_module_from_vm(VM* vm);
static JNIEnv* env_from_jvm(JavaVM* jvm);
static JavaRef* make_java_ref(JNIEnv* env, JavaVM* jvm, jobject obj);
static bool create_java_instance(VM* vm, Handle** clsHandlePtr, JavaRef* ref, int outSlot);
static void fn_bindClass(VM* vm);
static void fn_new(VM* vm);
static void fn_createProxy(VM* vm);
static void fn_loadLib(VM* vm);
static void fn_astable(VM* vm);
static void fn_instanceof(VM* vm);
static void fn_javaToString(VM* vm);
static bool java_to_slot(JNIEnv* env, VM* vm, BridgeState* bridge, int slot, jobject obj);
static jobject slot_to_java(JNIEnv* env, VM* vm, BridgeState* bridge, int slot);
static bool ensure_java_module(VM* vm);
static bool ensure_wrapper_classes(VM* vm);

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

static void release_bridge_handle(VM* vm, Handle** handle) {
  if (vm == NULL || handle == NULL || *handle == NULL)
    return;

  releaseHandle(vm, *handle);
  *handle = NULL;
}

static bool load_java_simple_classes(JNIEnv* env, JavaSimpleClasses* classes) {
  if (env == NULL || classes == NULL)
    return false;

  classes->stringClass = (*env)->FindClass(env, "java/lang/String");
  classes->booleanClass = (*env)->FindClass(env, "java/lang/Boolean");
  classes->numberClass = (*env)->FindClass(env, "java/lang/Number");

  return classes->stringClass != NULL && classes->booleanClass != NULL && classes->numberClass != NULL;
}

static void release_java_simple_classes(JNIEnv* env, JavaSimpleClasses* classes) {
  if (env == NULL || classes == NULL)
    return;

  if (classes->stringClass != NULL)
    (*env)->DeleteLocalRef(env, classes->stringClass);
  if (classes->booleanClass != NULL)
    (*env)->DeleteLocalRef(env, classes->booleanClass);
  if (classes->numberClass != NULL)
    (*env)->DeleteLocalRef(env, classes->numberClass);

  classes->stringClass = NULL;
  classes->booleanClass = NULL;
  classes->numberClass = NULL;
}

static bool object_to_slot(
    JNIEnv* env, VM* vm, BridgeState* bridge, int slot, jobject obj, const char* wrapErrorMessage) {
  if (obj == NULL) {
    setSlotNull(vm, slot);
    return true;
  }

  JavaSimpleClasses classes = {0};
  if (!load_java_simple_classes(env, &classes))
    return false;

  if ((*env)->IsInstanceOf(env, obj, classes.stringClass) == JNI_TRUE) {
    const char* s = (*env)->GetStringUTFChars(env, (jstring) obj, NULL);
    setSlotString(vm, slot, s == NULL ? "" : s);
    if (s != NULL)
      (*env)->ReleaseStringUTFChars(env, (jstring) obj, s);
  } else if ((*env)->IsInstanceOf(env, obj, classes.booleanClass) == JNI_TRUE) {
    jmethodID mBooleanValue = (*env)->GetMethodID(env, classes.booleanClass, "booleanValue", "()Z");
    jboolean bv = (*env)->CallBooleanMethod(env, obj, mBooleanValue);
    setSlotBool(vm, slot, bv == JNI_TRUE);
  } else if ((*env)->IsInstanceOf(env, obj, classes.numberClass) == JNI_TRUE) {
    jmethodID mDoubleValue = (*env)->GetMethodID(env, classes.numberClass, "doubleValue", "()D");
    jdouble dv = (*env)->CallDoubleMethod(env, obj, mDoubleValue);
    setSlotNumber(vm, slot, (double) dv);
  } else {
    JavaRef* ref = make_java_ref(env, bridge->jvm, obj);
    if (ref == NULL) {
      release_java_simple_classes(env, &classes);
      SetRuntimeError(vm, wrapErrorMessage);
      return false;
    }

    if (!create_java_instance(vm, &bridge->clsJavaObject, ref, slot)) {
      release_java_simple_classes(env, &classes);
      return false;
    }
  }

  release_java_simple_classes(env, &classes);
  return true;
}

static jstring get_java_object_name(JNIEnv* env, VM* vm, BridgeState* bridge, jobject target,
    const char* errorPrefix, const char* nullMessage) {
  if (target == NULL) {
    SetRuntimeError(vm, nullMessage);
    return NULL;
  }

  jstring jGetName = (*env)->NewStringUTF(env, "getName");
  jclass objClass = (*env)->FindClass(env, "java/lang/Object");
  jobjectArray noArgs = (*env)->NewObjectArray(env, 0, objClass, NULL);
  (*env)->DeleteLocalRef(env, objClass);

  jobject classNameObj = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mCallJavaMethod, target, jGetName, noArgs);

  (*env)->DeleteLocalRef(env, noArgs);
  (*env)->DeleteLocalRef(env, jGetName);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, errorPrefix);
    return NULL;
  }

  if (classNameObj == NULL) {
    SetRuntimeError(vm, nullMessage);
    return NULL;
  }

  return (jstring) classNameObj;
}

static bool wrap_bridge_global(VM* vm, jobject globalRef, int outSlot) {
  if (!ensure_wrapper_classes(vm)) {
    SetRuntimeError(vm, "Java wrappers are not initialized.");
    return false;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL || globalRef == NULL) {
    setSlotNull(vm, outSlot);
    return true;
  }

  JNIEnv* env = env_from_jvm(bridge->jvm);
  if (env == NULL) {
    SetRuntimeError(vm, "Invalid JNI Environment.");
    return false;
  }

  jobject localRef = (*env)->NewLocalRef(env, globalRef);
  if (localRef == NULL) {
    setSlotNull(vm, outSlot);
    return true;
  }

  JavaRef* ref = make_java_ref(env, bridge->jvm, localRef);
  if (ref == NULL) {
    setSlotNull(vm, outSlot);
  } else {
    create_java_instance(vm, &bridge->clsJavaObject, ref, outSlot);
  }

  (*env)->DeleteLocalRef(env, localRef);
  return true;
}

static void add_java_exports(VM* vm, Handle* mod) {
  static const JavaExport exports[] = {
      {"bindClass", fn_bindClass, 1, "java.bindClass(className) -> Java Class object."},
      {"new", fn_new, -1, "java.new(classOrName, args...) -> Java object."},
      {"newInstance", fn_new, -1, "java.newInstance(classOrName, args...) -> Java object."},
      {"createProxy", fn_createProxy, -1, "java.createProxy(interface, callback) -> proxy."},
      {"loadLib", fn_loadLib, 2, "java.loadLib(className, methodName) -> result."},
      {"astable", fn_astable, 1, "java.astable(javaArrayOrIterable) -> Saynaa List."},
      {"instanceof", fn_instanceof, 2, "java.instanceof(javaObject, classOrName) -> boolean."},
      {"tostring", fn_javaToString, 1, "java.tostring(javaValue) -> string."},
  };

  for (size_t i = 0; i < sizeof(exports) / sizeof(exports[0]); i++) {
    ModuleAddFunction(vm, mod, exports[i].name, exports[i].fn, exports[i].arity, exports[i].docstring);
  }
}

static void clear_java_packages(BridgeState* bridge) {
  if (bridge == NULL)
    return;

  JavaPackageEntry* it = bridge->packages;
  while (it != NULL) {
    JavaPackageEntry* next = it->next;
    if (it->prefix != NULL)
      free(it->prefix);
    free(it);
    it = next;
  }

  bridge->packages = NULL;
}

static bool java_package_exists(BridgeState* bridge, const char* prefix) {
  if (bridge == NULL || prefix == NULL)
    return false;

  for (JavaPackageEntry* it = bridge->packages; it != NULL; it = it->next) {
    if (it->prefix != NULL && strcmp(it->prefix, prefix) == 0)
      return true;
  }

  return false;
}

static bool append_java_package(BridgeState* bridge, const char* prefix) {
  if (bridge == NULL || prefix == NULL)
    return false;

  char* normalized = normalize_java_package_prefix(prefix);
  if (normalized == NULL)
    return false;

  if (java_package_exists(bridge, normalized)) {
    free(normalized);
    return true;
  }

  JavaPackageEntry* entry = (JavaPackageEntry*) calloc(1, sizeof(JavaPackageEntry));
  if (entry == NULL) {
    free(normalized);
    return false;
  }

  entry->prefix = normalized;
  entry->next = NULL;

  if (bridge->packages == NULL) {
    bridge->packages = entry;
    return true;
  }

  JavaPackageEntry* tail = bridge->packages;
  while (tail->next != NULL)
    tail = tail->next;
  tail->next = entry;
  return true;
}

static void ensure_default_java_packages(BridgeState* bridge) {
  static const char* defaults[] = {
      "",
      "java.lang.",
      "java.util.",
      "android.view.",
      "android.widget.",
      "android.app.",
      "android.content.",
      "android.os.",
      "android.text.",
      "android.graphics.",
  };

  if (bridge == NULL)
    return;

  for (size_t i = 0; i < (sizeof(defaults) / sizeof(defaults[0])); i++) {
    append_java_package(bridge, defaults[i]);
  }
}

static void clear_callbacks(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return;

  CallbackEntry* it = bridge->callbacks;
  while (it != NULL) {
    CallbackEntry* next = it->next;
    if (it->fnHandle != NULL)
      releaseHandle(vm, it->fnHandle);
    if (it->mapHandle != NULL)
      releaseHandle(vm, it->mapHandle);
    if (it->methodName != NULL)
      free(it->methodName);
    free(it);
    it = next;
  }

  bridge->callbacks = NULL;
  bridge->nextCallbackId = 1;
}

static int register_callback(VM* vm, int slot) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return 0;

  if (GetSlotType(vm, slot) != vCLOSURE) {
    SetRuntimeError(vm, "Callback must be a function.");
    return 0;
  }

  Handle* fnHandle = GetSlotHandle(vm, slot);
  if (fnHandle == NULL) {
    SetRuntimeError(vm, "Failed to capture callback function.");
    return 0;
  }

  CallbackEntry* entry = (CallbackEntry*) calloc(1, sizeof(CallbackEntry));
  if (entry == NULL) {
    releaseHandle(vm, fnHandle);
    SetRuntimeError(vm, "Out of memory while registering callback.");
    return 0;
  }

  if (bridge->nextCallbackId <= 0)
    bridge->nextCallbackId = 1;
  entry->id = bridge->nextCallbackId++;
  entry->fnHandle = fnHandle;
  entry->mapHandle = NULL;
  entry->methodName = NULL;
  entry->next = bridge->callbacks;
  bridge->callbacks = entry;

  __android_log_print(
      ANDROID_LOG_INFO, SAYNAAJAVA_TAG, "Registered callback id=%d from slot=%d", entry->id, slot);

  return entry->id;
}

static int register_map_callback(VM* vm, int mapSlot, const char* methodName) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return 0;

  if (GetSlotType(vm, mapSlot) != vMAP) {
    SetRuntimeError(vm, "Callback map must be a map object.");
    return 0;
  }

  Handle* mapHandle = GetSlotHandle(vm, mapSlot);
  if (mapHandle == NULL) {
    SetRuntimeError(vm, "Failed to capture callback map.");
    return 0;
  }

  CallbackEntry* entry = (CallbackEntry*) calloc(1, sizeof(CallbackEntry));
  if (entry == NULL) {
    releaseHandle(vm, mapHandle);
    SetRuntimeError(vm, "Out of memory while registering callback map.");
    return 0;
  }

  entry->methodName = str_dup_c(methodName == NULL ? "*" : methodName);
  if (entry->methodName == NULL) {
    releaseHandle(vm, mapHandle);
    free(entry);
    SetRuntimeError(vm, "Out of memory while registering callback method.");
    return 0;
  }

  if (bridge->nextCallbackId <= 0)
    bridge->nextCallbackId = 1;
  entry->id = bridge->nextCallbackId++;
  entry->fnHandle = NULL;
  entry->mapHandle = mapHandle;
  entry->next = bridge->callbacks;
  bridge->callbacks = entry;

  __android_log_print(ANDROID_LOG_INFO, SAYNAAJAVA_TAG, "Registered map callback id=%d method=%s",
      entry->id, entry->methodName == NULL ? "" : entry->methodName);

  return entry->id;
}

static CallbackEntry* find_callback(VM* vm, int callbackId) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return NULL;

  CallbackEntry* it = bridge->callbacks;
  while (it != NULL) {
    if (it->id == callbackId)
      return it;
    it = it->next;
  }

  return NULL;
}

// Invoke a registered callback entry.
// - function callback  : call directly with all Java args converted to Saynaa slots.
// - map/table callback : resolve by runtime method name (exact match), then call if function
// exists. Missing map key is intentionally treated as a no-op.
static bool invoke_registered_callback(JNIEnv* env, VM* vm, BridgeState* bridge,
    CallbackEntry* entry, const char* runtimeMethodName, jobjectArray argsArray, jobject* outResult) {
  if (vm == NULL || bridge == NULL || entry == NULL)
    return false;

  if (outResult != NULL)
    *outResult = NULL;

  int argc = 0;
  if (argsArray != NULL)
    argc = (int) (*env)->GetArrayLength(env, argsArray);

  reserveSlots(vm, argc + 8);

  for (int i = 0; i < argc; i++) {
    jobject arg = (*env)->GetObjectArrayElement(env, argsArray, (jsize) i);
    bool ok = java_to_slot(env, vm, bridge, 2 + i, arg);
    if (arg != NULL)
      (*env)->DeleteLocalRef(env, arg);
    if (!ok)
      return false;
  }

  bool ok = false;
  int argStart = 2;
  int argEnd = 1 + argc;
  int resultSlot = 0;

  if (entry->fnHandle != NULL) {
    setSlotHandle(vm, 1, entry->fnHandle);
    ok = CallFunction(vm, 1, 1, argStart, argEnd);
    resultSlot = 1;
  } else if (entry->mapHandle != NULL) {
    const char* methodKey = runtimeMethodName;
    if (methodKey == NULL || methodKey[0] == '\0')
      methodKey = entry->methodName;

    if (methodKey == NULL || methodKey[0] == '\0') {
      SetRuntimeError(vm, "callback method name is missing.");
      return false;
    }

    int keySlot = argEnd + 1;
    int fnSlot = argEnd + 2;
    setSlotHandle(vm, 1, entry->mapHandle);
    setSlotString(vm, keySlot, methodKey);

    if (CallMethod(vm, 1, "get", 1, keySlot, fnSlot) && GetSlotType(vm, fnSlot) == vCLOSURE) {
      ok = CallFunction(vm, fnSlot, 1, argStart, argEnd);
      resultSlot = fnSlot;
    } else {
      // If the callback method is absent in the map/table, do nothing.
      ok = true;
    }
  }

  if (ok && outResult != NULL && resultSlot > 0) {
    *outResult = slot_to_java(env, vm, bridge, resultSlot);
  }

  return ok;
}

static jobject create_native_callback_proxy(JNIEnv* env, VM* vm, BridgeState* bridge,
    jstring jInterface, const char* methodName, int callbackId) {
  if (env == NULL || vm == NULL || bridge == NULL || bridge->saynaaObject == NULL
      || bridge->mCreateNativeCallbackProxy == NULL || jInterface == NULL) {
    SetRuntimeError(vm, "Native callback proxy bridge is not initialized.");
    return NULL;
  }

  jobject saynaaObj = (*env)->NewLocalRef(env, bridge->saynaaObject);
  if (saynaaObj == NULL) {
    SetRuntimeError(vm, "Failed to access Saynaa object.");
    return NULL;
  }

  // methodName can be a concrete name (SAM/explicit callback) or wildcard "*" for map callbacks.
  jstring jMethod = (*env)->NewStringUTF(env, methodName == NULL ? "*" : methodName);
  jobject proxy = (*env)->CallStaticObjectMethod(env, bridge->javaBridgeClass,
      bridge->mCreateNativeCallbackProxy, saynaaObj, jInterface, jMethod, (jint) callbackId);

  (*env)->DeleteLocalRef(env, jMethod);
  (*env)->DeleteLocalRef(env, saynaaObj);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "native callback proxy creation failed");
    return NULL;
  }

  if (proxy == NULL) {
    SetRuntimeError(vm, "Failed to create native callback proxy.");
    return NULL;
  }

  return proxy;
}

static void android_stdout_write(VM* vm, const char* text) {
  (void) vm;
  __android_log_print(ANDROID_LOG_INFO, SAYNAAJAVA_TAG, "%s", text == NULL ? "" : text);
}

static void android_stderr_write(VM* vm, const char* text) {
  (void) vm;
  __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, "%s", text == NULL ? "" : text);
}

static void ensure_files_search_path(VM* vm, BridgeState* bridge, JNIEnv* env, jobject context) {
  if (vm == NULL || bridge == NULL || env == NULL || context == NULL)
    return;
  if (bridge->filesSearchPathAdded)
    return;

  jclass contextCls = (*env)->GetObjectClass(env, context);
  if (contextCls == NULL)
    return;

  jmethodID mGetFilesDir = (*env)->GetMethodID(env, contextCls, "getFilesDir", "()Ljava/io/File;");
  if (mGetFilesDir == NULL) {
    (*env)->DeleteLocalRef(env, contextCls);
    return;
  }

  jobject filesDirObj = (*env)->CallObjectMethod(env, context, mGetFilesDir);
  (*env)->DeleteLocalRef(env, contextCls);
  if ((*env)->ExceptionCheck(env) || filesDirObj == NULL) {
    if ((*env)->ExceptionCheck(env))
      throw_if_exception(vm, env, "getFilesDir failed");
    if (filesDirObj != NULL)
      (*env)->DeleteLocalRef(env, filesDirObj);
    return;
  }

  jclass fileCls = (*env)->GetObjectClass(env, filesDirObj);
  jmethodID mGetAbsolutePath = NULL;
  if (fileCls != NULL)
    mGetAbsolutePath = (*env)->GetMethodID(env, fileCls, "getAbsolutePath", "()Ljava/lang/String;");

  jstring absPath = NULL;
  if (mGetAbsolutePath != NULL)
    absPath = (jstring) (*env)->CallObjectMethod(env, filesDirObj, mGetAbsolutePath);

  if (fileCls != NULL)
    (*env)->DeleteLocalRef(env, fileCls);
  (*env)->DeleteLocalRef(env, filesDirObj);

  if ((*env)->ExceptionCheck(env) || absPath == NULL) {
    if ((*env)->ExceptionCheck(env))
      throw_if_exception(vm, env, "getFilesDir.getAbsolutePath failed");
    if (absPath != NULL)
      (*env)->DeleteLocalRef(env, absPath);
    return;
  }

  const char* base = (*env)->GetStringUTFChars(env, absPath, NULL);
  if (base != NULL) {
    size_t n = strlen(base);
    bool hasSlash = (n > 0 && (base[n - 1] == '/' || base[n - 1] == '\\'));
    if (hasSlash) {
      AddSearchPath(vm, base);
      bridge->filesSearchPathAdded = true;
    } else {
      char* withSlash = (char*) malloc(n + 2);
      if (withSlash != NULL) {
        memcpy(withSlash, base, n);
        withSlash[n] = '/';
        withSlash[n + 1] = '\0';
        AddSearchPath(vm, withSlash);
        bridge->filesSearchPathAdded = true;
        free(withSlash);
      }
    }

    if (bridge->javaModule != NULL) {
      Module* java = (Module*) AS_OBJ(bridge->javaModule->value);
      if (hasSlash) {
        moduleSetGlobal(vm, java, "saynaadir", 9, VAR_OBJ(newString(vm, base)));
      } else {
        char* withSlash = (char*) malloc(n + 2);
        if (withSlash != NULL) {
          memcpy(withSlash, base, n);
          withSlash[n] = '/';
          withSlash[n + 1] = '\0';
          moduleSetGlobal(vm, java, "saynaadir", 9, VAR_OBJ(newString(vm, withSlash)));
          free(withSlash);
        }
      }
    }

    (*env)->ReleaseStringUTFChars(env, absPath, base);
  }

  (*env)->DeleteLocalRef(env, absPath);
}

static char* str_dup_c(const char* s) {
  if (s == NULL)
    return NULL;
  size_t n = strlen(s);
  char* out = (char*) malloc(n + 1);
  if (out == NULL)
    return NULL;
  memcpy(out, s, n + 1);
  return out;
}

static char* normalize_java_package_prefix(const char* prefix) {
  if (prefix == NULL)
    return str_dup_c("");

  size_t len = strlen(prefix);
  while (len > 0
         && (prefix[len - 1] == ' ' || prefix[len - 1] == '\t' || prefix[len - 1] == '\r'
             || prefix[len - 1] == '\n')) {
    len--;
  }

  if (len >= 2 && prefix[len - 2] == '.' && prefix[len - 1] == '*') {
    len -= 1;
  }

  if (len == 0)
    return str_dup_c("");

  bool needs_dot = prefix[len - 1] != '.';
  char* out = (char*) malloc(len + (needs_dot ? 2 : 1));
  if (out == NULL)
    return NULL;

  memcpy(out, prefix, len);
  if (needs_dot) {
    out[len] = '.';
    out[len + 1] = '\0';
  } else {
    out[len] = '\0';
  }
  return out;
}

static char* massage_java_classname(const char* name) {
  char* out = str_dup_c(name == NULL ? "" : name);
  if (out == NULL)
    return NULL;

  for (char* p = out; *p != '\0'; p++) {
    if (*p == '_')
      *p = '$';
  }
  return out;
}

static jobject bridge_find_class_exact(JNIEnv* env, VM* vm, BridgeState* bridge, const char* className) {
  jstring jName = (*env)->NewStringUTF(env, className == NULL ? "" : className);
  jobject cls = (*env)->CallStaticObjectMethod(env, bridge->javaBridgeClass, bridge->mFindClass, jName);
  (*env)->DeleteLocalRef(env, jName);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "java class resolution failed");
    return NULL;
  }

  return cls;
}

static jobject bridge_find_class(JNIEnv* env, VM* vm, BridgeState* bridge,
    const char* requestedName, bool searchPackages, char** resolvedNameOut) {
  if (resolvedNameOut != NULL)
    *resolvedNameOut = NULL;

  if (bridge == NULL || requestedName == NULL || requestedName[0] == '\0')
    return NULL;

  char* massaged = massage_java_classname(requestedName);
  if (massaged == NULL) {
    SetRuntimeError(vm, "Out of memory while resolving Java class name.");
    return NULL;
  }

  bool qualified = strchr(massaged, '.') != NULL;
  if (!searchPackages || qualified) {
    jobject cls = bridge_find_class_exact(env, vm, bridge, massaged);
    if (cls != NULL && resolvedNameOut != NULL)
      *resolvedNameOut = str_dup_c(massaged);
    free(massaged);
    return cls;
  }

  ensure_default_java_packages(bridge);
  for (JavaPackageEntry* it = bridge->packages; it != NULL; it = it->next) {
    size_t prefixLen = strlen(it->prefix == NULL ? "" : it->prefix);
    size_t nameLen = strlen(massaged);
    char* fullName = (char*) malloc(prefixLen + nameLen + 1);
    if (fullName == NULL) {
      free(massaged);
      SetRuntimeError(vm, "Out of memory while resolving Java class name.");
      return NULL;
    }

    memcpy(fullName, it->prefix == NULL ? "" : it->prefix, prefixLen);
    memcpy(fullName + prefixLen, massaged, nameLen + 1);

    jobject cls = bridge_find_class_exact(env, vm, bridge, fullName);
    if (VM_HAS_ERROR(vm)) {
      free(fullName);
      free(massaged);
      return NULL;
    }
    if (cls != NULL) {
      if (resolvedNameOut != NULL)
        *resolvedNameOut = fullName;
      else
        free(fullName);
      free(massaged);
      return cls;
    }

    free(fullName);
  }

  free(massaged);
  return NULL;
}

static bool is_java_wildcard_import(const char* name) {
  if (name == NULL)
    return false;
  size_t len = strlen(name);
  return len >= 2 && name[len - 2] == '.' && name[len - 1] == '*';
}

static bool inject_java_global(VM* vm, const char* alias, int slot) {
  if (alias == NULL || alias[0] == '\0')
    return true;

  Module* module = current_module_from_vm(vm);
  Handle* imported = GetSlotHandle(vm, slot);
  if (module == NULL || imported == NULL)
    return false;

  moduleSetGlobal(vm, module, alias, (uint32_t) strlen(alias), imported->value);
  return true;
}

static JNIEnv* env_from_jvm(JavaVM* jvm) {
  if (jvm == NULL)
    return NULL;

  JNIEnv* env = NULL;
  if ((*jvm)->GetEnv(jvm, (void**) &env, JNI_VERSION_1_6) == JNI_OK) {
    return env;
  }

  if ((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK) {
    return NULL;
  }

  return env;
}

static void throw_if_exception(VM* vm, JNIEnv* env, const char* prefix) {
  if (!(*env)->ExceptionCheck(env))
    return;

  (*env)->ExceptionDescribe(env);
  (*env)->ExceptionClear(env);
  SetRuntimeErrorFmt(vm, "%s (JNI exception).", prefix);
}

static void java_ref_destructor(void* ptr) {
  JavaRef* ref = (JavaRef*) ptr;
  if (ref == NULL)
    return;
  if (ref->magic != JAVA_REF_MAGIC) {
    free(ref);
    return;
  }

  JNIEnv* env = env_from_jvm(ref->jvm);
  if (env != NULL && ref->global != NULL) {
    (*env)->DeleteGlobalRef(env, ref->global);
  }

  free(ref);
}

static JavaRef* make_java_ref(JNIEnv* env, JavaVM* jvm, jobject obj) {
  if (obj == NULL)
    return NULL;
  JavaRef* ref = (JavaRef*) malloc(sizeof(JavaRef));
  if (ref == NULL)
    return NULL;

  ref->magic = JAVA_REF_MAGIC;
  ref->jvm = jvm;
  ref->global = (*env)->NewGlobalRef(env, obj);

  if (ref->global == NULL) {
    free(ref);
    return NULL;
  }

  return ref;
}

static JavaRef* clone_java_ref(JNIEnv* env, JavaRef* src) {
  if (src == NULL || src->global == NULL)
    return NULL;
  jobject local = (*env)->NewLocalRef(env, src->global);
  if (local == NULL)
    return NULL;
  JavaRef* out = make_java_ref(env, src->jvm, local);
  (*env)->DeleteLocalRef(env, local);
  return out;
}

static bool ensure_wrapper_classes(VM* vm);

static bool create_java_instance(VM* vm, Handle** clsHandlePtr, JavaRef* ref, int outSlot) {
  if (clsHandlePtr == NULL) {
    if (ref != NULL)
      java_ref_destructor(ref);
    SetRuntimeError(vm, "Internal error: class handle pointer is null.");
    return false;
  }

  if (*clsHandlePtr == NULL) {
    if (!ensure_wrapper_classes(vm)) {
      if (ref != NULL)
        java_ref_destructor(ref);
      return false;
    }
  }

  if (*clsHandlePtr == NULL || ref == NULL) {
    SetRuntimeError(vm, "Internal error: Java class handle or ref is null.");
    if (ref != NULL)
      java_ref_destructor(ref);
    return false;
  }

  reserveSlots(vm, 8);
  setSlotHandle(vm, 1, *clsHandlePtr);
  setSlotPointer(vm, 2, ref, NULL);

  if (!NewInstance(vm, 1, outSlot, 1, 2)) {
    java_ref_destructor(ref);
    return false;
  }

  return true;
}

static bool create_java_method_instance(
    VM* vm, JavaRef* target, const char* method_name, bool is_static, int outSlot) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL && bridge->clsJavaMethod == NULL) {
    if (!ensure_wrapper_classes(vm)) {
      if (target != NULL)
        java_ref_destructor(target);
      return false;
    }
  }
  if (bridge == NULL || bridge->clsJavaMethod == NULL || target == NULL || method_name == NULL) {
    SetRuntimeError(vm, "Internal error creating JavaMethod instance.");
    if (target != NULL)
      java_ref_destructor(target);
    return false;
  }

  reserveSlots(vm, 6);
  setSlotHandle(vm, 1, bridge->clsJavaMethod);
  setSlotPointer(vm, 2, target, NULL);
  setSlotString(vm, 3, method_name);
  setSlotBool(vm, 4, is_static);

  if (!NewInstance(vm, 1, outSlot, 3, 2)) {
    java_ref_destructor(target);
    return false;
  }

  return true;
}

static bool put_java_result(VM* vm, JNIEnv* env, BridgeState* bridge, jobject obj, int slot) {
  return object_to_slot(env, vm, bridge, slot, obj, "Failed to wrap Java result object.");
}

static jobject slot_to_java(JNIEnv* env, VM* vm, BridgeState* bridge, int slot) {
  VarType type = GetSlotType(vm, slot);
  switch (type) {
  case vNULL:
    return NULL;
  case vBOOL: {
    jclass cls = (*env)->FindClass(env, "java/lang/Boolean");
    jmethodID valueOf = (*env)->GetStaticMethodID(env, cls, "valueOf", "(Z)Ljava/lang/Boolean;");
    jobject v = (*env)->CallStaticObjectMethod(env, cls, valueOf, GetSlotBool(vm, slot) ? JNI_TRUE : JNI_FALSE);
    (*env)->DeleteLocalRef(env, cls);
    return v;
  }
  case vNUMBER: {
    jclass cls = (*env)->FindClass(env, "java/lang/Double");
    jmethodID valueOf = (*env)->GetStaticMethodID(env, cls, "valueOf", "(D)Ljava/lang/Double;");
    jobject v = (*env)->CallStaticObjectMethod(env, cls, valueOf, (jdouble) GetSlotNumber(vm, slot));
    (*env)->DeleteLocalRef(env, cls);
    return v;
  }
  case vSTRING: {
    const char* s = GetSlotString(vm, slot, NULL);
    return (*env)->NewStringUTF(env, s == NULL ? "" : s);
  }
  case vPOINTER: {
    JavaRef* ref = (JavaRef*) GetSlotPointer(vm, slot, NULL, NULL);
    if (ref == NULL || ref->magic != JAVA_REF_MAGIC || ref->global == NULL) {
      SetRuntimeError(vm, "Invalid Java object pointer.");
      return NULL;
    }
    return (*env)->NewLocalRef(env, ref->global);
  }
  case vINSTANCE: {
    if (bridge == NULL)
      return NULL;

    int base = GetSlotsCount(vm);
    reserveSlots(vm, base + 3);
    int clsSlot = base;
    int objSlot = base + 1;
    int methodSlot = base + 2;

    bool isClass = false, isObject = false, isMethod = false;
    if (bridge->clsJavaClass != NULL) {
      setSlotHandle(vm, clsSlot, bridge->clsJavaClass);
      IsSlotInstanceOf(vm, slot, clsSlot, &isClass);
    }
    if (bridge->clsJavaObject != NULL) {
      setSlotHandle(vm, objSlot, bridge->clsJavaObject);
      IsSlotInstanceOf(vm, slot, objSlot, &isObject);
    }
    if (bridge->clsJavaMethod != NULL) {
      setSlotHandle(vm, methodSlot, bridge->clsJavaMethod);
      IsSlotInstanceOf(vm, slot, methodSlot, &isMethod);
    }

    if (isClass) {
      JavaClassNative* jc = (JavaClassNative*) GetSlotNativeInstance(vm, slot);
      if (jc != NULL && jc->class_ref != NULL && jc->class_ref->global != NULL) {
        return (*env)->NewLocalRef(env, jc->class_ref->global);
      }
    }

    if (isObject) {
      JavaObjectNative* jo = (JavaObjectNative*) GetSlotNativeInstance(vm, slot);
      if (jo != NULL && jo->object_ref != NULL && jo->object_ref->global != NULL) {
        return (*env)->NewLocalRef(env, jo->object_ref->global);
      }
    }

    if (isMethod) {
      JavaMethodNative* jm = (JavaMethodNative*) GetSlotNativeInstance(vm, slot);
      if (jm != NULL && jm->target_ref != NULL && jm->target_ref->global != NULL) {
        return (*env)->NewLocalRef(env, jm->target_ref->global);
      }
    }

    SetRuntimeError(vm, "Unsupported Java wrapper instance conversion.");
    return NULL;
  }
  default:
    SetRuntimeError(vm, "Unsupported Saynaa type for Java bridge argument.");
    return NULL;
  }
}

static bool java_to_slot(JNIEnv* env, VM* vm, BridgeState* bridge, int slot, jobject obj) {
  return object_to_slot(env, vm, bridge, slot, obj, "Failed to wrap Java object.");
}

static jobject make_args_array(JNIEnv* env, VM* vm, BridgeState* bridge, int startSlot, int argc) {
  jclass objClass = (*env)->FindClass(env, "java/lang/Object");
  jobjectArray args = (*env)->NewObjectArray(env, (jsize) argc, objClass, NULL);
  (*env)->DeleteLocalRef(env, objClass);

  for (int i = 0; i < argc; i++) {
    jobject arg = slot_to_java(env, vm, bridge, startSlot + i);
    if (GetSlotType(vm, startSlot + i) != vNULL && arg == NULL && (*env)->ExceptionCheck(env) == JNI_FALSE) {
      return NULL;
    }
    (*env)->SetObjectArrayElement(env, args, (jsize) i, arg);
    if (arg != NULL)
      (*env)->DeleteLocalRef(env, arg);
  }
  return args;
}

static const char* java_simple_name(const char* className) {
  if (className == NULL)
    return NULL;

  const char* lastDot = strrchr(className, '.');
  const char* base = (lastDot == NULL) ? className : (lastDot + 1);

  const char* lastDollar = strrchr(base, '$');
  if (lastDollar != NULL && *(lastDollar + 1) != '\0')
    return lastDollar + 1;

  return base;
}

// Best-effort resolve of the currently executing module for global injection.
static Module* current_module_from_vm(VM* vm) {
  if (vm == NULL || vm->fiber == NULL || vm->fiber->frame_count <= 0)
    return NULL;

  CallFrame* frame = &vm->fiber->frames[vm->fiber->frame_count - 1];
  if (frame == NULL || frame->closure == NULL || frame->closure->fn == NULL)
    return NULL;

  return frame->closure->fn->owner;
}

static bool ensure_slot_java_class(JNIEnv* env, VM* vm, BridgeState* bridge, int slot, jobject* classObj) {
  if (classObj == NULL)
    return false;

  *classObj = NULL;

  if (GetSlotType(vm, slot) == vSTRING) {
    const char* className = GetSlotString(vm, slot, NULL);
    jobject resolved = bridge_find_class(env, vm, bridge, className, true, NULL);
    if (VM_HAS_ERROR(vm))
      return false;
    if (resolved == NULL) {
      SetRuntimeErrorFmt(vm, "Java class not found: %s", className == NULL ? "" : className);
      return false;
    }
    *classObj = resolved;
    return true;
  }

  if (GetSlotType(vm, slot) != vPOINTER) {
    SetRuntimeError(vm, "Expected a Java class string or class object.");
    return false;
  }

  jobject candidate = slot_to_java(env, vm, bridge, slot);
  if (candidate == NULL)
    return false;

  jclass clsClass = (*env)->FindClass(env, "java/lang/Class");
  if (clsClass == NULL) {
    (*env)->DeleteLocalRef(env, candidate);
    return false;
  }

  jboolean isClass = (*env)->IsInstanceOf(env, candidate, clsClass);
  (*env)->DeleteLocalRef(env, clsClass);
  if (isClass != JNI_TRUE) {
    (*env)->DeleteLocalRef(env, candidate);
    SetRuntimeError(vm, "Expected a Java Class object.");
    return false;
  }

  *classObj = candidate;
  return true;
}

static jstring class_name_from_slot(JNIEnv* env, VM* vm, BridgeState* bridge, int slot) {
  if (GetSlotType(vm, slot) == vSTRING) {
    const char* className = GetSlotString(vm, slot, NULL);
    return (*env)->NewStringUTF(env, className == NULL ? "" : className);
  }

  if (GetSlotType(vm, slot) != vPOINTER) {
    SetRuntimeError(vm, "Expected a Java class string or class object.");
    return NULL;
  }

  jobject classObj = slot_to_java(env, vm, bridge, slot);
  if (classObj == NULL)
    return NULL;
  jstring className = get_java_object_name(
      env, vm, bridge, classObj, "class.getName() failed", "Failed to resolve Java class name.");
  (*env)->DeleteLocalRef(env, classObj);
  return className;
}

static void fn_instanceof(VM* vm) {
  int argc = GetArgc(vm);
  if (argc != 2) {
    SetRuntimeError(vm, "instanceof expects (object, classOrName).");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);

  if (GetSlotType(vm, 1) != vPOINTER) {
    setSlotBool(vm, 0, false);
    return;
  }

  jobject target = slot_to_java(env, vm, bridge, 1);
  if (target == NULL) {
    setSlotBool(vm, 0, false);
    return;
  }

  jobject classObj = NULL;
  if (!ensure_slot_java_class(env, vm, bridge, 2, &classObj)) {
    (*env)->DeleteLocalRef(env, target);
    return;
  }

  jboolean result = (*env)->IsInstanceOf(env, target, (jclass) classObj);
  setSlotBool(vm, 0, result == JNI_TRUE);

  (*env)->DeleteLocalRef(env, classObj);
  (*env)->DeleteLocalRef(env, target);
}

static void fn_astable(VM* vm) {
  int argc = GetArgc(vm);
  if (argc != 1) {
    SetRuntimeError(vm, "astable expects exactly one Java value.");
    return;
  }

  NewList(vm, 0);

  if (GetSlotType(vm, 1) == vNULL)
    return;

  if (GetSlotType(vm, 1) != vPOINTER) {
    SetRuntimeError(vm, "astable expects a Java object, array, iterator, or enumeration.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);
  jobject target = slot_to_java(env, vm, bridge, 1);
  if (target == NULL)
    return;

  jclass clsClass = (*env)->FindClass(env, "java/lang/Class");
  jclass iterableClass = (*env)->FindClass(env, "java/lang/Iterable");
  jclass iteratorClass = (*env)->FindClass(env, "java/util/Iterator");
  jclass enumerationClass = (*env)->FindClass(env, "java/util/Enumeration");
  jclass reflectArrayClass = (*env)->FindClass(env, "java/lang/reflect/Array");
  jobject targetClass = (*env)->GetObjectClass(env, target);

  jmethodID mIsArray = (*env)->GetMethodID(env, clsClass, "isArray", "()Z");
  jboolean isArray = (*env)->CallBooleanMethod(env, targetClass, mIsArray);
  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "astable isArray failed");
    goto astable_cleanup;
  }

  if (isArray == JNI_TRUE) {
    jmethodID mGetLength = (*env)->GetStaticMethodID(env, reflectArrayClass, "getLength", "(Ljava/lang/Object;)I");
    jmethodID mGet = (*env)->GetStaticMethodID(
        env, reflectArrayClass, "get", "(Ljava/lang/Object;I)Ljava/lang/Object;");
    jint length = (*env)->CallStaticIntMethod(env, reflectArrayClass, mGetLength, target);
    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "astable getLength failed");
      goto astable_cleanup;
    }

    for (jint i = 0; i < length; i++) {
      jobject item = (*env)->CallStaticObjectMethod(env, reflectArrayClass, mGet, target, i);
      if ((*env)->ExceptionCheck(env)) {
        throw_if_exception(vm, env, "astable array access failed");
        if (item != NULL)
          (*env)->DeleteLocalRef(env, item);
        goto astable_cleanup;
      }

      if (!java_to_slot(env, vm, bridge, 1, item)) {
        if (item != NULL)
          (*env)->DeleteLocalRef(env, item);
        goto astable_cleanup;
      }
      if (!ListInsert(vm, 0, -1, 1)) {
        if (item != NULL)
          (*env)->DeleteLocalRef(env, item);
        goto astable_cleanup;
      }
      if (item != NULL)
        (*env)->DeleteLocalRef(env, item);
    }
    goto astable_cleanup;
  }

  if ((*env)->IsInstanceOf(env, target, iterableClass) == JNI_TRUE) {
    jmethodID mIterator = (*env)->GetMethodID(env, iterableClass, "iterator", "()Ljava/util/Iterator;");
    jobject iterator = (*env)->CallObjectMethod(env, target, mIterator);
    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "astable iterator() failed");
      if (iterator != NULL)
        (*env)->DeleteLocalRef(env, iterator);
      goto astable_cleanup;
    }

    jmethodID mHasNext = (*env)->GetMethodID(env, iteratorClass, "hasNext", "()Z");
    jmethodID mNext = (*env)->GetMethodID(env, iteratorClass, "next", "()Ljava/lang/Object;");
    while ((*env)->CallBooleanMethod(env, iterator, mHasNext) == JNI_TRUE) {
      jobject item = (*env)->CallObjectMethod(env, iterator, mNext);
      if ((*env)->ExceptionCheck(env)) {
        throw_if_exception(vm, env, "astable iterator next failed");
        if (item != NULL)
          (*env)->DeleteLocalRef(env, item);
        (*env)->DeleteLocalRef(env, iterator);
        goto astable_cleanup;
      }
      if (!java_to_slot(env, vm, bridge, 1, item) || !ListInsert(vm, 0, -1, 1)) {
        if (item != NULL)
          (*env)->DeleteLocalRef(env, item);
        (*env)->DeleteLocalRef(env, iterator);
        goto astable_cleanup;
      }
      if (item != NULL)
        (*env)->DeleteLocalRef(env, item);
    }
    (*env)->DeleteLocalRef(env, iterator);
    goto astable_cleanup;
  }

  if ((*env)->IsInstanceOf(env, target, iteratorClass) == JNI_TRUE) {
    jmethodID mHasNext = (*env)->GetMethodID(env, iteratorClass, "hasNext", "()Z");
    jmethodID mNext = (*env)->GetMethodID(env, iteratorClass, "next", "()Ljava/lang/Object;");
    while ((*env)->CallBooleanMethod(env, target, mHasNext) == JNI_TRUE) {
      jobject item = (*env)->CallObjectMethod(env, target, mNext);
      if ((*env)->ExceptionCheck(env)) {
        throw_if_exception(vm, env, "astable iterator next failed");
        if (item != NULL)
          (*env)->DeleteLocalRef(env, item);
        goto astable_cleanup;
      }
      if (!java_to_slot(env, vm, bridge, 1, item) || !ListInsert(vm, 0, -1, 1)) {
        if (item != NULL)
          (*env)->DeleteLocalRef(env, item);
        goto astable_cleanup;
      }
      if (item != NULL)
        (*env)->DeleteLocalRef(env, item);
    }
    goto astable_cleanup;
  }

  if ((*env)->IsInstanceOf(env, target, enumerationClass) == JNI_TRUE) {
    jmethodID mHasMore = (*env)->GetMethodID(env, enumerationClass, "hasMoreElements", "()Z");
    jmethodID mNext = (*env)->GetMethodID(env, enumerationClass, "nextElement", "()Ljava/lang/Object;");
    while ((*env)->CallBooleanMethod(env, target, mHasMore) == JNI_TRUE) {
      jobject item = (*env)->CallObjectMethod(env, target, mNext);
      if ((*env)->ExceptionCheck(env)) {
        throw_if_exception(vm, env, "astable enumeration next failed");
        if (item != NULL)
          (*env)->DeleteLocalRef(env, item);
        goto astable_cleanup;
      }
      if (!java_to_slot(env, vm, bridge, 1, item) || !ListInsert(vm, 0, -1, 1)) {
        if (item != NULL)
          (*env)->DeleteLocalRef(env, item);
        goto astable_cleanup;
      }
      if (item != NULL)
        (*env)->DeleteLocalRef(env, item);
    }
    goto astable_cleanup;
  }

  SetRuntimeError(vm, "astable only supports Java arrays, Iterable, Iterator, and Enumeration.");

astable_cleanup:
  if (targetClass != NULL)
    (*env)->DeleteLocalRef(env, targetClass);
  if (reflectArrayClass != NULL)
    (*env)->DeleteLocalRef(env, reflectArrayClass);
  if (enumerationClass != NULL)
    (*env)->DeleteLocalRef(env, enumerationClass);
  if (iteratorClass != NULL)
    (*env)->DeleteLocalRef(env, iteratorClass);
  if (iterableClass != NULL)
    (*env)->DeleteLocalRef(env, iterableClass);
  if (clsClass != NULL)
    (*env)->DeleteLocalRef(env, clsClass);
  (*env)->DeleteLocalRef(env, target);
}

static void fn_loadLib(VM* vm) {
  int argc = GetArgc(vm);
  if (argc != 2) {
    SetRuntimeError(vm, "loadLib expects (className, methodName).");
    return;
  }

  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;
  if (!ValidateSlotString(vm, 2, NULL, NULL))
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);
  const char* className = GetSlotString(vm, 1, NULL);
  const char* methodName = GetSlotString(vm, 2, NULL);

  jstring jClassName = (*env)->NewStringUTF(env, className == NULL ? "" : className);
  jstring jMethodName = (*env)->NewStringUTF(env, methodName == NULL ? "" : methodName);

  jclass objClass = (*env)->FindClass(env, "java/lang/Object");
  jobjectArray oneArg = (*env)->NewObjectArray(env, 1, objClass, NULL);
  jobjectArray noArgs = (*env)->NewObjectArray(env, 0, objClass, NULL);
  (*env)->DeleteLocalRef(env, objClass);

  if (bridge->saynaaObject != NULL)
    (*env)->SetObjectArrayElement(env, oneArg, 0, bridge->saynaaObject);

  jobject ret = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mCallStaticJavaMethod, jClassName, jMethodName, oneArg);
  if ((*env)->ExceptionCheck(env)) {
    (*env)->ExceptionClear(env);
    ret = (*env)->CallStaticObjectMethod(env, bridge->javaBridgeClass,
        bridge->mCallStaticJavaMethod, jClassName, jMethodName, noArgs);
  }

  (*env)->DeleteLocalRef(env, noArgs);
  (*env)->DeleteLocalRef(env, oneArg);
  (*env)->DeleteLocalRef(env, jMethodName);
  (*env)->DeleteLocalRef(env, jClassName);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "loadLib failed");
    return;
  }

  java_to_slot(env, vm, bridge, 0, ret);
  if (ret != NULL)
    (*env)->DeleteLocalRef(env, ret);
}

static void fn_javaToString(VM* vm) {
  if (GetArgc(vm) != 1) {
    SetRuntimeError(vm, "tostring expects exactly one value.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);

  jobject target = slot_to_java(env, vm, bridge, 1);
  if (target == NULL) {
    setSlotString(vm, 0, "null");
    return;
  }

  jstring jMethodName = (*env)->NewStringUTF(env, "toString");
  jclass objClass = (*env)->FindClass(env, "java/lang/Object");
  jobjectArray noArgs = (*env)->NewObjectArray(env, 0, objClass, NULL);
  (*env)->DeleteLocalRef(env, objClass);

  jobject ret = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mCallJavaMethod, target, jMethodName, noArgs);

  (*env)->DeleteLocalRef(env, noArgs);
  (*env)->DeleteLocalRef(env, jMethodName);
  (*env)->DeleteLocalRef(env, target);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "tostring failed");
    return;
  }

  java_to_slot(env, vm, bridge, 0, ret);
  if (ret != NULL)
    (*env)->DeleteLocalRef(env, ret);
}

static void fn_bindClass(VM* vm) {
  if (!ensure_wrapper_classes(vm)) {
    SetRuntimeError(vm, "Java wrappers are not initialized.");
    return;
  }

  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);
  const char* name = GetSlotString(vm, 1, NULL);

  jobject cls = bridge_find_class_exact(env, vm, bridge, name);
  if (VM_HAS_ERROR(vm))
    return;

  if (cls != NULL) {
    JavaRef* ref = make_java_ref(env, bridge->jvm, cls);
    if (ref == NULL) {
      SetRuntimeError(vm, "Failed to wrap Java class.");
    } else {
      create_java_instance(vm, &bridge->clsJavaClass, ref, 0);
    }
  } else {
    setSlotNull(vm, 0);
  }
  if (cls != NULL)
    (*env)->DeleteLocalRef(env, cls);
}

static void fn_javaAddPackage(VM* vm) {
  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  const char* prefix = GetSlotString(vm, 1, NULL);
  if (!append_java_package(bridge, prefix == NULL ? "" : prefix)) {
    SetRuntimeError(vm, "Failed to register Java package prefix.");
    return;
  }

  setSlotNull(vm, 0);
}

static void fn_jclass(VM* vm) {
  int argc = GetArgc(vm);
  if (argc != 1 && argc != 2) {
    SetRuntimeError(vm, "jclass expects (className) or (className, alias).");
    return;
  }

  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;
  if (argc == 2 && !ValidateSlotString(vm, 2, NULL, NULL))
    return;

  if (!ensure_wrapper_classes(vm)) {
    SetRuntimeError(vm, "Java wrappers are not initialized.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);
  const char* className = GetSlotString(vm, 1, NULL);

  char* resolvedName = NULL;
  jobject cls = bridge_find_class(env, vm, bridge, className, true, &resolvedName);
  if (VM_HAS_ERROR(vm))
    return;
  if (cls == NULL) {
    SetRuntimeErrorFmt(vm, "Java class not found: %s", className == NULL ? "" : className);
    return;
  }

  JavaRef* ref = make_java_ref(env, bridge->jvm, cls);
  if (ref == NULL) {
    if (resolvedName != NULL)
      free(resolvedName);
    (*env)->DeleteLocalRef(env, cls);
    SetRuntimeError(vm, "Failed to wrap Java class.");
    return;
  }

  if (!create_java_instance(vm, &bridge->clsJavaClass, ref, 0)) {
    if (resolvedName != NULL)
      free(resolvedName);
    (*env)->DeleteLocalRef(env, cls);
    return;
  }

  const char* alias = (argc == 2) ? GetSlotString(vm, 2, NULL)
                                  : java_simple_name(resolvedName != NULL ? resolvedName : className);
  inject_java_global(vm, alias, 0);

  if (resolvedName != NULL)
    free(resolvedName);
  (*env)->DeleteLocalRef(env, cls);
}

static void fn_importJava(VM* vm) {
  int argc = GetArgc(vm);
  if (argc != 1 && argc != 2) {
    SetRuntimeError(vm, "importJava expects (className) or (className, alias).");
    return;
  }

  if (!ensure_wrapper_classes(vm)) {
    SetRuntimeError(vm, "Java wrappers are not initialized.");
    return;
  }

  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;
  if (argc == 2 && !ValidateSlotString(vm, 2, NULL, NULL))
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);
  const char* className = GetSlotString(vm, 1, NULL);

  if (is_java_wildcard_import(className)) {
    if (argc == 2) {
      SetRuntimeError(vm, "Wildcard importJava does not accept an alias.");
      return;
    }
    if (!append_java_package(bridge, className)) {
      SetRuntimeError(vm, "Failed to register Java package prefix.");
      return;
    }
    setSlotNull(vm, 0);
    return;
  }

  char* resolvedName = NULL;
  jobject cls = bridge_find_class(env, vm, bridge, className, true, &resolvedName);
  if (VM_HAS_ERROR(vm))
    return;

  if (cls == NULL) {
    SetRuntimeErrorFmt(vm, "Java class not found: %s", className == NULL ? "" : className);
    return;
  }

  JavaRef* ref = make_java_ref(env, bridge->jvm, cls);
  if (ref == NULL) {
    (*env)->DeleteLocalRef(env, cls);
    SetRuntimeError(vm, "Failed to wrap Java class.");
    return;
  }

  if (!create_java_instance(vm, &bridge->clsJavaClass, ref, 0)) {
    (*env)->DeleteLocalRef(env, cls);
    return;
  }

  // Inject imported class into current module globals for import-like ergonomics.
  const char* alias = (argc == 2) ? GetSlotString(vm, 2, NULL)
                                  : java_simple_name(resolvedName != NULL ? resolvedName : className);
  inject_java_global(vm, alias, 0);

  if (resolvedName != NULL)
    free(resolvedName);
  (*env)->DeleteLocalRef(env, cls);
}

static void fn_new(VM* vm) {
  int argc = GetArgc(vm);
  if (argc < 1) {
    SetRuntimeError(vm, "java.new expects class name and optional arguments.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);

  jstring jClassName = class_name_from_slot(env, vm, bridge, 1);
  if (jClassName == NULL)
    return;

  jobjectArray args = make_args_array(env, vm, bridge, 2, argc - 1);
  if (args == NULL && argc > 1) {
    SetRuntimeError(vm, "java.new argument conversion failed.");
    (*env)->DeleteLocalRef(env, jClassName);
    return;
  }

  jobject obj = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mCreateJavaObject, jClassName, args);

  if (args != NULL)
    (*env)->DeleteLocalRef(env, args);
  (*env)->DeleteLocalRef(env, jClassName);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "java.new failed");
    return;
  }

  java_to_slot(env, vm, bridge, 0, obj);
  if (obj != NULL)
    (*env)->DeleteLocalRef(env, obj);
}

static void fn_call(VM* vm) {
  int argc = GetArgc(vm);
  if (argc < 2) {
    SetRuntimeError(vm, "java.call expects target, methodName and optional arguments.");
    return;
  }

  if (!ValidateSlotType(vm, 1, vPOINTER))
    return;
  if (!ValidateSlotString(vm, 2, NULL, NULL))
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);

  jobject target = slot_to_java(env, vm, bridge, 1);
  const char* methodName = GetSlotString(vm, 2, NULL);
  jstring jMethodName = (*env)->NewStringUTF(env, methodName);
  jobjectArray args = make_args_array(env, vm, bridge, 3, argc - 2);

  jobject ret = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mCallJavaMethod, target, jMethodName, args);

  if (args != NULL)
    (*env)->DeleteLocalRef(env, args);
  if (target != NULL)
    (*env)->DeleteLocalRef(env, target);
  (*env)->DeleteLocalRef(env, jMethodName);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "java.call failed");
    return;
  }

  java_to_slot(env, vm, bridge, 0, ret);
  if (ret != NULL)
    (*env)->DeleteLocalRef(env, ret);
}

static void fn_callStatic(VM* vm) {
  int argc = GetArgc(vm);
  if (argc < 2) {
    SetRuntimeError(vm, "java.callStatic expects className, methodName and optional arguments.");
    return;
  }

  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;
  if (!ValidateSlotString(vm, 2, NULL, NULL))
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);

  const char* className = GetSlotString(vm, 1, NULL);
  const char* methodName = GetSlotString(vm, 2, NULL);
  jstring jClassName = (*env)->NewStringUTF(env, className);
  jstring jMethodName = (*env)->NewStringUTF(env, methodName);
  jobjectArray args = make_args_array(env, vm, bridge, 3, argc - 2);

  jobject ret = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mCallStaticJavaMethod, jClassName, jMethodName, args);

  if (args != NULL)
    (*env)->DeleteLocalRef(env, args);
  (*env)->DeleteLocalRef(env, jMethodName);
  (*env)->DeleteLocalRef(env, jClassName);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "java.callStatic failed");
    return;
  }

  java_to_slot(env, vm, bridge, 0, ret);
  if (ret != NULL)
    (*env)->DeleteLocalRef(env, ret);
}

static void fn_getField(VM* vm) {
  if (!ValidateSlotType(vm, 1, vPOINTER))
    return;
  if (!ValidateSlotString(vm, 2, NULL, NULL))
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);

  jobject target = slot_to_java(env, vm, bridge, 1);
  const char* field = GetSlotString(vm, 2, NULL);
  jstring jField = (*env)->NewStringUTF(env, field);

  jobject ret = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mGetFieldValue, target, jField);

  if (target != NULL)
    (*env)->DeleteLocalRef(env, target);
  (*env)->DeleteLocalRef(env, jField);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "java.getField failed");
    return;
  }

  java_to_slot(env, vm, bridge, 0, ret);
  if (ret != NULL)
    (*env)->DeleteLocalRef(env, ret);
}

static void fn_setField(VM* vm) {
  int argc = GetArgc(vm);
  if (argc != 3) {
    SetRuntimeError(vm, "java.setField expects (target, fieldName, value).");
    return;
  }

  if (!ValidateSlotType(vm, 1, vPOINTER))
    return;
  if (!ValidateSlotString(vm, 2, NULL, NULL))
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);

  jobject target = slot_to_java(env, vm, bridge, 1);
  const char* fieldName = GetSlotString(vm, 2, NULL);
  jobject value = slot_to_java(env, vm, bridge, 3);

  jstring jFieldName = (*env)->NewStringUTF(env, fieldName);
  jboolean ok = (*env)->CallStaticBooleanMethod(
      env, bridge->javaBridgeClass, bridge->mSetFieldValue, target, jFieldName, value);

  if (value != NULL)
    (*env)->DeleteLocalRef(env, value);
  if (target != NULL)
    (*env)->DeleteLocalRef(env, target);
  (*env)->DeleteLocalRef(env, jFieldName);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "java.setField failed");
    return;
  }

  setSlotBool(vm, 0, ok == JNI_TRUE);
}

static void* new_java_class_instance(VM* vm) {
  (void) vm;
  return calloc(1, sizeof(JavaClassNative));
}

static void delete_java_class_instance(VM* vm, void* ptr) {
  (void) vm;
  JavaClassNative* inst = (JavaClassNative*) ptr;
  if (inst != NULL) {
    if (inst->class_ref != NULL)
      java_ref_destructor(inst->class_ref);
    free(inst);
  }
}

static void* new_java_object_instance(VM* vm) {
  (void) vm;
  return calloc(1, sizeof(JavaObjectNative));
}

static void delete_java_object_instance(VM* vm, void* ptr) {
  (void) vm;
  JavaObjectNative* inst = (JavaObjectNative*) ptr;
  if (inst != NULL) {
    if (inst->object_ref != NULL)
      java_ref_destructor(inst->object_ref);
    free(inst);
  }
}

static void* new_java_method_instance(VM* vm) {
  (void) vm;
  return calloc(1, sizeof(JavaMethodNative));
}

static void delete_java_method_instance(VM* vm, void* ptr) {
  (void) vm;
  JavaMethodNative* inst = (JavaMethodNative*) ptr;
  if (inst != NULL) {
    if (inst->target_ref != NULL)
      java_ref_destructor(inst->target_ref);
    if (inst->method_name != NULL)
      free(inst->method_name);
    free(inst);
  }
}

static void java_class_init(VM* vm) {
  JavaClassNative* thiz = (JavaClassNative*) GetThis(vm);
  if (thiz == NULL)
    return;
  if (!ValidateSlotType(vm, 1, vPOINTER))
    return;
  thiz->class_ref = (JavaRef*) GetSlotPointer(vm, 1, NULL, NULL);
}

static void java_object_init(VM* vm) {
  JavaObjectNative* thiz = (JavaObjectNative*) GetThis(vm);
  if (thiz == NULL)
    return;
  if (!ValidateSlotType(vm, 1, vPOINTER))
    return;
  thiz->object_ref = (JavaRef*) GetSlotPointer(vm, 1, NULL, NULL);
}

static void java_method_init(VM* vm) {
  JavaMethodNative* thiz = (JavaMethodNative*) GetThis(vm);
  if (thiz == NULL)
    return;
  if (!ValidateSlotType(vm, 1, vPOINTER))
    return;
  if (!ValidateSlotString(vm, 2, NULL, NULL))
    return;

  thiz->target_ref = (JavaRef*) GetSlotPointer(vm, 1, NULL, NULL);
  const char* method_name = GetSlotString(vm, 2, NULL);
  thiz->method_name = str_dup_c(method_name);

  if (GetArgc(vm) >= 3) {
    thiz->is_static = GetSlotBool(vm, 3);
  }
}

static void java_class_getter(VM* vm) {
  JavaClassNative* thiz = (JavaClassNative*) GetThis(vm);
  if (thiz == NULL || thiz->class_ref == NULL)
    return;
  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;

  const char* name = GetSlotString(vm, 1, NULL);
  JNIEnv* env = env_from_jvm(thiz->class_ref->jvm);
  if (env == NULL) {
    SetRuntimeError(vm, "Invalid JNI Environment.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL && bridge->javaBridgeClass != NULL && bridge->mGetFieldValue != NULL) {
    jobject classObj = (*env)->NewLocalRef(env, thiz->class_ref->global);
    jstring jField = (*env)->NewStringUTF(env, name == NULL ? "" : name);

    jobject fieldValue = (*env)->CallStaticObjectMethod(
        env, bridge->javaBridgeClass, bridge->mGetFieldValue, classObj, jField);

    (*env)->DeleteLocalRef(env, jField);
    if (classObj != NULL)
      (*env)->DeleteLocalRef(env, classObj);

    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "JavaClass._getter field access failed");
      return;
    }

    if (fieldValue != NULL) {
      put_java_result(vm, env, bridge, fieldValue, 0);
      (*env)->DeleteLocalRef(env, fieldValue);
      return;
    }
  }

  JavaRef* target = clone_java_ref(env, thiz->class_ref);
  if (target == NULL) {
    SetRuntimeError(vm, "Failed to clone Java class reference.");
    return;
  }

  create_java_method_instance(vm, target, name, true, 0);
}

static void java_class_call(VM* vm) {
  JavaClassNative* thiz = (JavaClassNative*) GetThis(vm);
  if (thiz == NULL || thiz->class_ref == NULL) {
    SetRuntimeError(vm, "Invalid JavaClass instance.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);

  jobject classObj = (*env)->NewLocalRef(env, thiz->class_ref->global);
  jstring classNameObj = get_java_object_name(env, vm, bridge, classObj,
      "JavaClass._call getName() failed", "JavaClass._call failed to resolve class name.");
  (*env)->DeleteLocalRef(env, classObj);
  if (classNameObj == NULL)
    return;

  int argc = GetArgc(vm);
  jobjectArray args = make_args_array(env, vm, bridge, 1, argc);
  if (VM_HAS_ERROR(vm) || (args == NULL && argc > 0)) {
    if (!VM_HAS_ERROR(vm) && argc > 0)
      SetRuntimeError(vm, "JavaClass._call argument conversion failed.");
    if (args != NULL)
      (*env)->DeleteLocalRef(env, args);
    if (classNameObj != NULL)
      (*env)->DeleteLocalRef(env, classNameObj);
    return;
  }

  jobject obj = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mCreateJavaObject, classNameObj, args);

  if (args != NULL)
    (*env)->DeleteLocalRef(env, args);
  (*env)->DeleteLocalRef(env, classNameObj);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "JavaClass._call constructor failed");
    return;
  }

  put_java_result(vm, env, bridge, obj, 0);
  if (obj != NULL)
    (*env)->DeleteLocalRef(env, obj);
}

static void java_object_getter(VM* vm) {
  JavaObjectNative* thiz = (JavaObjectNative*) GetThis(vm);
  if (thiz == NULL || thiz->object_ref == NULL)
    return;
  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;

  const char* name = GetSlotString(vm, 1, NULL);
  JNIEnv* env = env_from_jvm(thiz->object_ref->jvm);
  if (env == NULL) {
    SetRuntimeError(vm, "Invalid JNI Environment.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL && bridge->javaBridgeClass != NULL && bridge->mGetFieldValue != NULL) {
    jobject obj = (*env)->NewLocalRef(env, thiz->object_ref->global);
    jstring jField = (*env)->NewStringUTF(env, name == NULL ? "" : name);

    jobject fieldValue = (*env)->CallStaticObjectMethod(
        env, bridge->javaBridgeClass, bridge->mGetFieldValue, obj, jField);

    (*env)->DeleteLocalRef(env, jField);
    if (obj != NULL)
      (*env)->DeleteLocalRef(env, obj);

    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "JavaObject._getter field access failed");
      return;
    }

    if (fieldValue != NULL) {
      put_java_result(vm, env, bridge, fieldValue, 0);
      (*env)->DeleteLocalRef(env, fieldValue);
      return;
    }
  }

  JavaRef* target = clone_java_ref(env, thiz->object_ref);
  if (target == NULL) {
    SetRuntimeError(vm, "Failed to clone Java object reference.");
    return;
  }

  create_java_method_instance(vm, target, name, false, 0);
}

static void java_object_setter(VM* vm) {
  JavaObjectNative* thiz = (JavaObjectNative*) GetThis(vm);
  if (thiz == NULL || thiz->object_ref == NULL)
    return;
  if (!ValidateSlotString(vm, 1, NULL, NULL))
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);
  jobject target = (*env)->NewLocalRef(env, thiz->object_ref->global);
  const char* fieldName = GetSlotString(vm, 1, NULL);
  jobject value = slot_to_java(env, vm, bridge, 2);

  jstring jFieldName = (*env)->NewStringUTF(env, fieldName);
  jboolean ok = (*env)->CallStaticBooleanMethod(
      env, bridge->javaBridgeClass, bridge->mSetFieldValue, target, jFieldName, value);

  if (value != NULL)
    (*env)->DeleteLocalRef(env, value);
  (*env)->DeleteLocalRef(env, target);
  (*env)->DeleteLocalRef(env, jFieldName);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "JavaObject._setter failed");
    return;
  }

  setSlotBool(vm, 0, ok == JNI_TRUE);
}

static void java_method_call(VM* vm) {
  JavaMethodNative* thiz = (JavaMethodNative*) GetThis(vm);
  if (thiz == NULL || thiz->target_ref == NULL || thiz->method_name == NULL) {
    SetRuntimeError(vm, "Invalid JavaMethod instance.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  JNIEnv* env = env_from_jvm(bridge->jvm);
  int argc = GetArgc(vm);
  jobjectArray args = make_args_array(env, vm, bridge, 1, argc);
  jobject ret = NULL;

  if (thiz->is_static) {
    jobject classObj = (*env)->NewLocalRef(env, thiz->target_ref->global);
    jstring classNameObj = get_java_object_name(env, vm, bridge, classObj,
        "JavaMethod._call static getName failed", "JavaMethod._call static getName failed");
    (*env)->DeleteLocalRef(env, classObj);

    if (classNameObj == NULL) {
      if (args != NULL)
        (*env)->DeleteLocalRef(env, args);
      return;
    }

    jstring jMethod = (*env)->NewStringUTF(env, thiz->method_name);
    ret = (*env)->CallStaticObjectMethod(
        env, bridge->javaBridgeClass, bridge->mCallStaticJavaMethod, classNameObj, jMethod, args);

    (*env)->DeleteLocalRef(env, jMethod);
    (*env)->DeleteLocalRef(env, classNameObj);
  } else {
    jobject target = (*env)->NewLocalRef(env, thiz->target_ref->global);
    jstring jMethod = (*env)->NewStringUTF(env, thiz->method_name);
    ret = (*env)->CallStaticObjectMethod(
        env, bridge->javaBridgeClass, bridge->mCallJavaMethod, target, jMethod, args);
    (*env)->DeleteLocalRef(env, jMethod);
    (*env)->DeleteLocalRef(env, target);
  }

  if (args != NULL)
    (*env)->DeleteLocalRef(env, args);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "JavaMethod._call failed");
    return;
  }

  put_java_result(vm, env, bridge, ret, 0);
  if (ret != NULL)
    (*env)->DeleteLocalRef(env, ret);
}

static void fn_activity(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL) {
    setSlotNull(vm, 0);
    return;
  }
  wrap_bridge_global(vm, bridge->activity, 0);
}

static void fn_eventView(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL) {
    setSlotNull(vm, 0);
    return;
  }
  wrap_bridge_global(vm, bridge->lastEventView, 0);
}

// Resolve interface slot into Java class name string expected by JavaBridge APIs.
// Accepts either:
// - string class name: "android.view.View$OnClickListener"
// - Java class object wrapper: bind("android.view.View$OnClickListener")
static jstring resolve_proxy_interface_name(
    VM* vm, JNIEnv* env, BridgeState* bridge, int interfaceSlot, const char* errorPrefix) {
  VarType interfaceType = GetSlotType(vm, interfaceSlot);
  if (interfaceType == vSTRING) {
    const char* interfaceName = GetSlotString(vm, interfaceSlot, NULL);
    char* resolvedName = NULL;
    jobject cls = bridge_find_class(env, vm, bridge, interfaceName, true, &resolvedName);
    if (VM_HAS_ERROR(vm))
      return NULL;

    if (cls != NULL) {
      if (cls != NULL)
        (*env)->DeleteLocalRef(env, cls);
      jstring result = (*env)->NewStringUTF(env, resolvedName == NULL ? interfaceName : resolvedName);
      if (resolvedName != NULL)
        free(resolvedName);
      return result;
    }

    if (resolvedName != NULL)
      free(resolvedName);
    return (*env)->NewStringUTF(env, interfaceName == NULL ? "" : interfaceName);
  }

  if (interfaceType != vPOINTER) {
    SetRuntimeError(vm, "createProxy interface must be class object or string.");
    return NULL;
  }

  jobject ifaceObj = slot_to_java(env, vm, bridge, interfaceSlot);
  if (ifaceObj == NULL)
    return NULL;

  jstring classNameObj = get_java_object_name(
      env, vm, bridge, ifaceObj, errorPrefix, "createProxy failed to resolve interface name.");
  (*env)->DeleteLocalRef(env, ifaceObj);
  if (classNameObj == NULL)
    return NULL;

  return classNameObj;
}

static void fn_createProxy(VM* vm) {
  int argc = GetArgc(vm);
  if (argc != 2 && argc != 3) {
    SetRuntimeError(vm, "createProxy expects (interfaceOrName, callback) or (interfaceOrName, "
                        "methodName, callback).\n"
                        "callback can be script string, function, or callback map.");
    return;
  }

  const char* methodName = "*";
  int callbackArgIndex = 2;
  if (argc == 3) {
    if (!ValidateSlotString(vm, 2, NULL, NULL))
      return;
    methodName = GetSlotString(vm, 2, NULL);
    callbackArgIndex = 3;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL || bridge->saynaaObject == NULL || bridge->mCreateProxy == NULL) {
    SetRuntimeError(vm, "createProxy bridge is not initialized.");
    return;
  }

  JNIEnv* env = env_from_jvm(bridge->jvm);
  if (env == NULL) {
    SetRuntimeError(vm, "Invalid JNI Environment.");
    return;
  }

  if (argc == 2 && bridge->mGetDefaultInterfaceMethodName != NULL) {
    jstring jInterfaceName = resolve_proxy_interface_name(vm, env, bridge, 1, "createProxy class.getName failed");

    if (jInterfaceName != NULL) {
      jobject inferredObj = (*env)->CallStaticObjectMethod(
          env, bridge->javaBridgeClass, bridge->mGetDefaultInterfaceMethodName, jInterfaceName);

      if ((*env)->ExceptionCheck(env)) {
        (*env)->DeleteLocalRef(env, jInterfaceName);
        throw_if_exception(vm, env, "createProxy infer method name failed");
        return;
      }

      if (inferredObj != NULL) {
        const char* inferred = (*env)->GetStringUTFChars(env, (jstring) inferredObj, NULL);
        if (inferred != NULL && inferred[0] != '\0') {
          methodName = inferred;
        }
        if (inferred != NULL)
          (*env)->ReleaseStringUTFChars(env, (jstring) inferredObj, inferred);
        (*env)->DeleteLocalRef(env, inferredObj);
      }

      (*env)->DeleteLocalRef(env, jInterfaceName);
    }
  }

  VarType callbackType = GetSlotType(vm, callbackArgIndex);

  if (callbackType != vSTRING) {
    int callbackId = 0;
    if (callbackType == vCLOSURE) {
      if (methodName != NULL && strcmp(methodName, "*") == 0) {
        SetRuntimeError(vm, "createProxy(interface, function) requires explicit method for "
                            "multi-method interfaces.");
        return;
      }
      callbackId = register_callback(vm, callbackArgIndex);
    } else if (callbackType == vMAP) {
      callbackId = register_map_callback(vm, callbackArgIndex, methodName);
    } else {
      SetRuntimeError(vm, "createProxy callback must be script string, function, or map.");
      return;
    }

    if (callbackId == 0)
      return;

    jstring jInterface = resolve_proxy_interface_name(vm, env, bridge, 1, "createProxy class.getName failed");
    if (jInterface == NULL)
      return;

    jobject proxy = create_native_callback_proxy(env, vm, bridge, jInterface, methodName, callbackId);
    (*env)->DeleteLocalRef(env, jInterface);
    if (proxy == NULL)
      return;

    put_java_result(vm, env, bridge, proxy, 0);
    (*env)->DeleteLocalRef(env, proxy);
    return;
  }

  const char* script = GetSlotString(vm, callbackArgIndex, NULL);

  jobject saynaaObj = (*env)->NewLocalRef(env, bridge->saynaaObject);
  if (saynaaObj == NULL) {
    SetRuntimeError(vm, "Failed to access Saynaa object.");
    return;
  }

  jstring jInterface = resolve_proxy_interface_name(vm, env, bridge, 1, "createProxy class.getName failed");
  if (jInterface == NULL) {
    (*env)->DeleteLocalRef(env, saynaaObj);
    return;
  }

  jstring jMethod = (*env)->NewStringUTF(env, methodName == NULL ? "" : methodName);
  jstring jScript = (*env)->NewStringUTF(env, script == NULL ? "" : script);

  jobject proxy = (*env)->CallStaticObjectMethod(
      env, bridge->javaBridgeClass, bridge->mCreateProxy, saynaaObj, jInterface, jMethod, jScript);

  (*env)->DeleteLocalRef(env, jScript);
  (*env)->DeleteLocalRef(env, jMethod);
  (*env)->DeleteLocalRef(env, jInterface);
  (*env)->DeleteLocalRef(env, saynaaObj);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "createProxy failed");
    return;
  }

  put_java_result(vm, env, bridge, proxy, 0);
  if (proxy != NULL)
    (*env)->DeleteLocalRef(env, proxy);
}

static void register_java_api(VM* vm) {
  RegisterBuiltinFn(vm, "bindClass", fn_bindClass, 1, "bindClass(\"java.class.Name\") -> Java Class object.");
  RegisterBuiltinFn(vm, "jclass", fn_jclass,
      -1, "jclass(className[, alias]) -> resolve Java class using registered packages and inject into module globals.");
  RegisterBuiltinFn(vm, "java_add_package", fn_javaAddPackage,
      1, "java_add_package(prefix) -> register Java package prefix for jclass/importJava/createProxy.");
  RegisterBuiltinFn(vm, "importJava", fn_importJava,
      -1, "importJava(className[, alias]) or importJava(package.*) -> JavaClass or register package prefix.");
  RegisterBuiltinFn(vm, "jimport", fn_importJava, -1, "Alias of importJava(className[, alias]).");
  RegisterBuiltinFn(vm, "new", fn_new, -1, "new(classOrName, args...) -> Java object.");
  RegisterBuiltinFn(vm, "newInstance", fn_new, -1, "newInstance(classOrName, args...) -> Java object.");
  RegisterBuiltinFn(vm, "call", fn_call, -1, "call(javaObject, methodName, args...) -> return value.");

  RegisterBuiltinFn(vm, "java_bind_class", fn_bindClass, 1, "Bind Java class and return a Java pointer object.");
  RegisterBuiltinFn(vm, "java_new", fn_new, -1, "Construct Java object by class name.");
  RegisterBuiltinFn(vm, "java_call", fn_call, -1, "Invoke Java instance method.");
  RegisterBuiltinFn(vm, "java_call_static", fn_callStatic, -1, "Invoke Java static method.");
  RegisterBuiltinFn(vm, "java_get_field", fn_getField, 2, "Read Java field value.");
  RegisterBuiltinFn(vm, "java_set_field", fn_setField, 3, "Write Java field value.");
  RegisterBuiltinFn(vm, "activity", fn_activity, 0, "Return current Android Activity object.");
  RegisterBuiltinFn(vm, "getActivity", fn_activity, 0, "Return current Android Activity object.");
  RegisterBuiltinFn(vm, "eventView", fn_eventView, 0, "Return current event view object.");
  RegisterBuiltinFn(vm, "createProxy", fn_createProxy,
      -1, "createProxy(interface, callback) or createProxy(interface, method, callback) -> Java listener/proxy.");
  RegisterBuiltinFn(vm, "instanceof", fn_instanceof, 2, "instanceof(javaObject, classOrName) -> boolean.");
  RegisterBuiltinFn(vm, "astable", fn_astable, 1, "astable(javaArrayOrIterable) -> Saynaa List.");
  RegisterBuiltinFn(vm, "loadLib", fn_loadLib, 2, "loadLib(className, methodName).");
}

static bool ensure_java_module(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return false;

  if (bridge->javaModule != NULL)
    return true;

  Handle* mod = NewModule(vm, "java");
  if (mod == NULL)
    return false;

  add_java_exports(vm, mod);

  Module* java = (Module*) AS_OBJ(mod->value);
  moduleSetGlobal(vm, java, "ids", 3, VAR_OBJ(newMap(vm)));
  moduleSetGlobal(vm, java, "loaded", 6, VAR_OBJ(newMap(vm)));
  moduleSetGlobal(vm, java, "imported", 8, VAR_OBJ(newList(vm, 0)));
  moduleSetGlobal(vm, java, "saynaadir", 9, VAR_OBJ(newString(vm, "")));

  registerModule(vm, mod);
  bridge->javaModule = mod;
  return true;
}

static bool register_java_wrapper_classes(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return false;

  if (bridge->javaWrapperModule != NULL && bridge->clsJavaClass != NULL
      && bridge->clsJavaObject != NULL && bridge->clsJavaMethod != NULL && ensure_java_module(vm)) {
    return true;
  }

  Handle* mod = NewModule(vm, "java_wrappers");
  if (mod == NULL)
    return false;
  bridge->javaWrapperModule = mod;

  Handle* clsJavaClass = NewClass(vm, "JavaClass", NULL, mod, new_java_class_instance,
      delete_java_class_instance, "Java class wrapper");
  if (clsJavaClass == NULL)
    return false;
  ClassAddMethod(vm, clsJavaClass, "_init", java_class_init, 1, "");
  ClassAddMethod(vm, clsJavaClass, "_call", java_class_call, -1, "");
  ClassAddMethod(vm, clsJavaClass, "_getter", java_class_getter, 1, "");
  bridge->clsJavaClass = clsJavaClass;

  Handle* clsJavaObject = NewClass(vm, "JavaObject", NULL, mod, new_java_object_instance,
      delete_java_object_instance, "Java object wrapper");
  if (clsJavaObject == NULL)
    return false;
  ClassAddMethod(vm, clsJavaObject, "_init", java_object_init, 1, "");
  ClassAddMethod(vm, clsJavaObject, "_getter", java_object_getter, 1, "");
  ClassAddMethod(vm, clsJavaObject, "_setter", java_object_setter, 2, "");
  bridge->clsJavaObject = clsJavaObject;

  Handle* clsJavaMethod = NewClass(vm, "JavaMethod", NULL, mod, new_java_method_instance,
      delete_java_method_instance, "Java method wrapper");
  if (clsJavaMethod == NULL)
    return false;
  ClassAddMethod(vm, clsJavaMethod, "_init", java_method_init, 3, "");
  ClassAddMethod(vm, clsJavaMethod, "_call", java_method_call, -1, "");
  bridge->clsJavaMethod = clsJavaMethod;

  registerModule(vm, mod);
  return ensure_java_module(vm);
}

static bool ensure_wrapper_classes(VM* vm) {
  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return false;
  if (bridge->javaWrapperModule != NULL && bridge->clsJavaClass != NULL
      && bridge->clsJavaObject != NULL && bridge->clsJavaMethod != NULL) {
    return true;
  }
  return register_java_wrapper_classes(vm);
}

static VM* vm_from_saynaa(JNIEnv* env, jobject saynaaObject) {
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

static void set_vm_ptr_on_saynaa(JNIEnv* env, jobject saynaaObject, jlong ptr) {
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

JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1open(JNIEnv* env, jobject thiz) {
  JavaVM* jvm = NULL;
  if ((*env)->GetJavaVM(env, &jvm) != JNI_OK) {
    return NULL;
  }

  Configuration config = NewConfiguration();
  config.stdout_write = android_stdout_write;
  config.stderr_write = android_stderr_write;

  VM* vm = NewVM(&config);
  if (vm == NULL) {
    return NULL;
  }

  BridgeState* bridge = (BridgeState*) calloc(1, sizeof(BridgeState));
  if (bridge == NULL) {
    FreeVM(vm);
    return NULL;
  }

  bridge->jvm = jvm;
  ensure_default_java_packages(bridge);
  bridge->saynaaObject = (*env)->NewGlobalRef(env, thiz);
  if (bridge->saynaaObject == NULL) {
    clear_java_packages(bridge);
    free(bridge);
    FreeVM(vm);
    return NULL;
  }

  jclass localBridgeClass = (*env)->FindClass(env, "com/android/saynaa/saynaajava/JavaBridge");
  if (localBridgeClass == NULL) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    clear_java_packages(bridge);
    free(bridge);
    FreeVM(vm);
    return NULL;
  }

  bridge->javaBridgeClass = (jclass) (*env)->NewGlobalRef(env, localBridgeClass);
  (*env)->DeleteLocalRef(env, localBridgeClass);

  bridge->mFindClass = (*env)->GetStaticMethodID(
      env, bridge->javaBridgeClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
  bridge->mCreateJavaObject = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createJavaObject", "(Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
  bridge->mCallJavaMethod = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "callJavaMethod",
      "(Ljava/lang/Object;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
  bridge->mCallStaticJavaMethod = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "callStaticJavaMethod", "(Ljava/lang/String;Ljava/lang/String;[Ljava/lang/Object;)Ljava/lang/Object;");
  bridge->mGetFieldValue = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "getFieldValue",
      "(Ljava/lang/Object;Ljava/lang/String;)Ljava/lang/Object;");
  bridge->mSetFieldValue = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass, "setFieldValue",
      "(Ljava/lang/Object;Ljava/lang/String;Ljava/lang/Object;)Z");
  bridge->mCreateProxy = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createProxy", "(Lcom/android/saynaa/saynaajava/Saynaa;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/Object;");
  bridge->mCreateNativeCallbackProxy = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "createNativeCallbackProxy", "(Lcom/android/saynaa/saynaajava/Saynaa;Ljava/lang/String;Ljava/lang/String;I)Ljava/lang/Object;");
  bridge->mGetDefaultInterfaceMethodName = (*env)->GetStaticMethodID(env, bridge->javaBridgeClass,
      "getDefaultInterfaceMethodName", "(Ljava/lang/String;)Ljava/lang/String;");

  if (bridge->mFindClass == NULL || bridge->mCreateJavaObject == NULL || bridge->mCallJavaMethod == NULL
      || bridge->mCallStaticJavaMethod == NULL || bridge->mGetFieldValue == NULL
      || bridge->mSetFieldValue == NULL || bridge->mCreateProxy == NULL
      || bridge->mCreateNativeCallbackProxy == NULL || bridge->mGetDefaultInterfaceMethodName == NULL) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    (*env)->DeleteGlobalRef(env, bridge->javaBridgeClass);
    clear_java_packages(bridge);
    free(bridge);
    FreeVM(vm);
    return NULL;
  }

  SetUserData(vm, bridge);
  if (!register_java_wrapper_classes(vm)) {
    (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
    (*env)->DeleteGlobalRef(env, bridge->javaBridgeClass);
    clear_java_packages(bridge);
    free(bridge);
    SetUserData(vm, NULL);
    FreeVM(vm);
    return NULL;
  }
  register_java_api(vm);

  jclass cptrCls = (*env)->FindClass(env, "com/android/saynaa/saynaajava/CPtr");
  if (cptrCls == NULL) {
    return NULL;
  }

  jobject cptrObj = (*env)->AllocObject(env, cptrCls);
  jfieldID ptrField = (*env)->GetFieldID(env, cptrCls, "pointer", "J");
  (*env)->SetLongField(env, cptrObj, ptrField, (jlong) (intptr_t) vm);
  (*env)->DeleteLocalRef(env, cptrCls);

  return cptrObj;
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_executeSnippetNative(
    JNIEnv* env, jobject thiz, jstring snippet) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || snippet == NULL)
    return;

  const char* code = (*env)->GetStringUTFChars(env, snippet, NULL);
  if (code == NULL)
    return;

  Result result = RunString(vm, code);

  (*env)->ReleaseStringUTFChars(env, snippet, code);

  if (result != RESULT_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG,
        "Saynaa snippet execution failed with code: %d", (int) result);
  }
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_executeSnippetWithViewNative(
    JNIEnv* env, jobject thiz, jstring snippet, jobject view) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || snippet == NULL)
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL) {
    if (bridge->lastEventView != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->lastEventView);
      bridge->lastEventView = NULL;
    }
    if (view != NULL)
      bridge->lastEventView = (*env)->NewGlobalRef(env, view);
  }

  const char* code = (*env)->GetStringUTFChars(env, snippet, NULL);
  if (code == NULL)
    return;

  Result result = RunString(vm, code);

  (*env)->ReleaseStringUTFChars(env, snippet, code);

  if (bridge != NULL && bridge->lastEventView != NULL) {
    (*env)->DeleteGlobalRef(env, bridge->lastEventView);
    bridge->lastEventView = NULL;
  }

  if (result != RESULT_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG,
        "Saynaa snippet-with-view execution failed with code: %d", (int) result);
  }
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_execute(
    JNIEnv* env, jobject thiz, jobject context) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL) {
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, "VM not initialized.");
    return;
  }

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL) {
    ensure_files_search_path(vm, bridge, env, context);

    clear_callbacks(vm);
    release_bridge_handle(vm, &bridge->javaWrapperModule);
    release_bridge_handle(vm, &bridge->clsJavaMethod);
    release_bridge_handle(vm, &bridge->clsJavaObject);
    release_bridge_handle(vm, &bridge->clsJavaClass);

    if (bridge->activity != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->activity);
      bridge->activity = NULL;
    }
    if (bridge->lastEventView != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->lastEventView);
      bridge->lastEventView = NULL;
    }
    if (context != NULL) {
      bridge->activity = (*env)->NewGlobalRef(env, context);
    }
  }

  jclass saynaaCls = (*env)->GetObjectClass(env, thiz);
  jfieldID sourceField = (*env)->GetFieldID(env, saynaaCls, "source", "Ljava/lang/String;");
  jstring source = (jstring) (*env)->GetObjectField(env, thiz, sourceField);
  (*env)->DeleteLocalRef(env, saynaaCls);

  if (source == NULL) {
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, "Source is null.");
    return;
  }

  const char* code = (*env)->GetStringUTFChars(env, source, NULL);
  if (code == NULL) {
    (*env)->DeleteLocalRef(env, source);
    return;
  }

  __android_log_print(ANDROID_LOG_INFO, SAYNAAJAVA_TAG,
      "Running script... source_len=%d preprocessor=disabled", (int) strlen(code));
  Result result = RunString(vm, code);

  (*env)->ReleaseStringUTFChars(env, source, code);
  (*env)->DeleteLocalRef(env, source);

  if (result != RESULT_SUCCESS) {
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG, "Saynaa execution failed with code: %d", (int) result);
  } else {
    __android_log_print(ANDROID_LOG_INFO, SAYNAAJAVA_TAG, "Saynaa execution finished successfully.");
  }
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackNative(
    JNIEnv* env, jobject thiz, jint callbackId, jobject arg0) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || callbackId <= 0)
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return;

  CallbackEntry* entry = find_callback(vm, (int) callbackId);
  if (entry == NULL)
    return;

  if (!ensure_wrapper_classes(vm))
    return;

  if (vm->fiber != NULL)
    vm->fiber->error = NULL;

  jclass objClass = (*env)->FindClass(env, "java/lang/Object");
  jobjectArray args = (*env)->NewObjectArray(env, arg0 == NULL ? 0 : 1, objClass, NULL);
  (*env)->DeleteLocalRef(env, objClass);
  if (arg0 != NULL)
    (*env)->SetObjectArrayElement(env, args, 0, arg0);

  bool ok = invoke_registered_callback(env, vm, bridge, entry, NULL, args, NULL);
  if (args != NULL)
    (*env)->DeleteLocalRef(env, args);

  if (!ok) {
    const char* err = (vm->fiber != NULL && vm->fiber->error != NULL) ? vm->fiber->error->data : "<unknown>";
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG,
        "invokeCallbackNative failed for callbackId=%d err=%s", (int) callbackId, err);
    if (vm->fiber != NULL)
      vm->fiber->error = NULL;
  } else {
    __android_log_print(ANDROID_LOG_INFO, SAYNAAJAVA_TAG,
        "invokeCallbackNative succeeded for callbackId=%d", (int) callbackId);
  }
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackMethodNative(
    JNIEnv* env, jobject thiz, jint callbackId, jstring methodName, jobjectArray args) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || callbackId <= 0)
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return;

  CallbackEntry* entry = find_callback(vm, (int) callbackId);
  if (entry == NULL)
    return;

  if (!ensure_wrapper_classes(vm))
    return;

  if (vm->fiber != NULL)
    vm->fiber->error = NULL;

  const char* runtimeMethodName = NULL;
  if (methodName != NULL)
    runtimeMethodName = (*env)->GetStringUTFChars(env, methodName, NULL);

  bool ok = invoke_registered_callback(env, vm, bridge, entry, runtimeMethodName, args, NULL);

  if (methodName != NULL && runtimeMethodName != NULL)
    (*env)->ReleaseStringUTFChars(env, methodName, runtimeMethodName);

  if (!ok) {
    const char* err = (vm->fiber != NULL && vm->fiber->error != NULL) ? vm->fiber->error->data : "<unknown>";
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG,
        "invokeCallbackNative failed for callbackId=%d err=%s", (int) callbackId, err);
    if (vm->fiber != NULL)
      vm->fiber->error = NULL;
  } else {
    __android_log_print(ANDROID_LOG_INFO, SAYNAAJAVA_TAG,
        "invokeCallbackNative succeeded for callbackId=%d", (int) callbackId);
  }
}

JNIEXPORT jobject JNICALL Java_com_android_saynaa_saynaajava_Saynaa_invokeCallbackMethodWithResultNative(
    JNIEnv* env, jobject thiz, jint callbackId, jstring methodName, jobjectArray args) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL || callbackId <= 0)
    return NULL;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge == NULL)
    return NULL;

  CallbackEntry* entry = find_callback(vm, (int) callbackId);
  if (entry == NULL)
    return NULL;

  if (!ensure_wrapper_classes(vm))
    return NULL;

  if (vm->fiber != NULL)
    vm->fiber->error = NULL;

  const char* runtimeMethodName = NULL;
  if (methodName != NULL)
    runtimeMethodName = (*env)->GetStringUTFChars(env, methodName, NULL);

  jobject result = NULL;
  bool ok = invoke_registered_callback(env, vm, bridge, entry, runtimeMethodName, args, &result);

  if (methodName != NULL && runtimeMethodName != NULL)
    (*env)->ReleaseStringUTFChars(env, methodName, runtimeMethodName);

  if (!ok) {
    const char* err = (vm->fiber != NULL && vm->fiber->error != NULL) ? vm->fiber->error->data : "<unknown>";
    __android_log_print(ANDROID_LOG_ERROR, SAYNAAJAVA_TAG,
        "invokeCallbackMethodWithResultNative failed for callbackId=%d err=%s", (int) callbackId, err);
    if (vm->fiber != NULL)
      vm->fiber->error = NULL;
    return NULL;
  }

  return result;
}

JNIEXPORT void JNICALL Java_com_android_saynaa_saynaajava_Saynaa_saynaa_1close(JNIEnv* env, jobject thiz) {
  VM* vm = vm_from_saynaa(env, thiz);
  if (vm == NULL)
    return;

  BridgeState* bridge = bridge_from_vm(vm);
  if (bridge != NULL) {
    clear_callbacks(vm);
    release_bridge_handle(vm, &bridge->javaWrapperModule);
    release_bridge_handle(vm, &bridge->javaModule);
    release_bridge_handle(vm, &bridge->clsJavaMethod);
    release_bridge_handle(vm, &bridge->clsJavaObject);
    release_bridge_handle(vm, &bridge->clsJavaClass);
    if (bridge->saynaaObject != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->saynaaObject);
      bridge->saynaaObject = NULL;
    }
    if (bridge->lastEventView != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->lastEventView);
      bridge->lastEventView = NULL;
    }
    if (bridge->activity != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->activity);
      bridge->activity = NULL;
    }
    if (bridge->javaBridgeClass != NULL) {
      (*env)->DeleteGlobalRef(env, bridge->javaBridgeClass);
      bridge->javaBridgeClass = NULL;
    }
    clear_java_packages(bridge);
    free(bridge);
    SetUserData(vm, NULL);
  }

  FreeVM(vm);
  set_vm_ptr_on_saynaa(env, thiz, (jlong) 0);
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
  (void) vm;
  (void) reserved;
  return JNI_VERSION_1_6;
}
