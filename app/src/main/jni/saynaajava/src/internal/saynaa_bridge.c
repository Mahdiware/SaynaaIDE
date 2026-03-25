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

bool load_java_simple_classes(JNIEnv* env, JavaSimpleClasses* classes) {
  if (env == NULL || classes == NULL)
    return false;

  classes->stringClass = safe_find_class(NULL, env, "java/lang/String", "load_java_simple_classes:string");
  classes->booleanClass = safe_find_class(NULL, env, "java/lang/Boolean", "load_java_simple_classes:boolean");
  classes->numberClass = safe_find_class(NULL, env, "java/lang/Number", "load_java_simple_classes:number");
  classes->integerClass = safe_find_class(NULL, env, "java/lang/Integer", "load_java_simple_classes:integer");
  classes->longClass = safe_find_class(NULL, env, "java/lang/Long", "load_java_simple_classes:long");
  classes->shortClass = safe_find_class(NULL, env, "java/lang/Short", "load_java_simple_classes:short");
  classes->byteClass = safe_find_class(NULL, env, "java/lang/Byte", "load_java_simple_classes:byte");
  classes->floatClass = safe_find_class(NULL, env, "java/lang/Float", "load_java_simple_classes:float");
  classes->doubleClass = safe_find_class(NULL, env, "java/lang/Double", "load_java_simple_classes:double");
  classes->charClass = safe_find_class(NULL, env, "java/lang/Character", "load_java_simple_classes:char");
  classes->listClass = safe_find_class(NULL, env, "java/util/List", "load_java_simple_classes:list");
  classes->mapClass = safe_find_class(NULL, env, "java/util/Map", "load_java_simple_classes:map");
  classes->classClass = safe_find_class(NULL, env, "java/lang/Class", "load_java_simple_classes:class");

  return classes->stringClass != NULL && classes->booleanClass != NULL
      && (classes->numberClass != NULL
          || classes->integerClass != NULL
          || classes->longClass != NULL
          || classes->shortClass != NULL
          || classes->byteClass != NULL
          || classes->floatClass != NULL
          || classes->doubleClass != NULL
          || classes->charClass != NULL);
}

void release_java_simple_classes(JNIEnv* env, JavaSimpleClasses* classes) {
  if (env == NULL || classes == NULL)
    return;

  if (classes->stringClass != NULL)
    (*env)->DeleteLocalRef(env, classes->stringClass);
  if (classes->booleanClass != NULL)
    (*env)->DeleteLocalRef(env, classes->booleanClass);
  if (classes->numberClass != NULL)
    (*env)->DeleteLocalRef(env, classes->numberClass);
  if (classes->integerClass != NULL)
    (*env)->DeleteLocalRef(env, classes->integerClass);
  if (classes->longClass != NULL)
    (*env)->DeleteLocalRef(env, classes->longClass);
  if (classes->shortClass != NULL)
    (*env)->DeleteLocalRef(env, classes->shortClass);
  if (classes->byteClass != NULL)
    (*env)->DeleteLocalRef(env, classes->byteClass);
  if (classes->floatClass != NULL)
    (*env)->DeleteLocalRef(env, classes->floatClass);
  if (classes->doubleClass != NULL)
    (*env)->DeleteLocalRef(env, classes->doubleClass);
  if (classes->charClass != NULL)
    (*env)->DeleteLocalRef(env, classes->charClass);
  if (classes->listClass != NULL)
    (*env)->DeleteLocalRef(env, classes->listClass);
  if (classes->mapClass != NULL)
    (*env)->DeleteLocalRef(env, classes->mapClass);
  if (classes->classClass != NULL)
    (*env)->DeleteLocalRef(env, classes->classClass);

  classes->stringClass = NULL;
  classes->booleanClass = NULL;
  classes->numberClass = NULL;
  classes->integerClass = NULL;
  classes->longClass = NULL;
  classes->shortClass = NULL;
  classes->byteClass = NULL;
  classes->floatClass = NULL;
  classes->doubleClass = NULL;
  classes->charClass = NULL;
  classes->listClass = NULL;
  classes->mapClass = NULL;
  classes->classClass = NULL;
}

static bool try_unbox_value(JNIEnv* env, VM* vm, jobject obj, int slot, JavaSimpleClasses* classes) {
  if (env == NULL || obj == NULL || classes == NULL)
    return false;

  if (classes->booleanClass != NULL
      && (*env)->IsInstanceOf(env, obj, classes->booleanClass) == JNI_TRUE) {
    jmethodID mBool = (*env)->GetMethodID(env, classes->booleanClass, "booleanValue", "()Z");
    if (mBool == NULL) {
      throw_if_exception(vm, env, "booleanValue() lookup failed");
      return false;
    }
    jboolean bv = (*env)->CallBooleanMethod(env, obj, mBool);
    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "booleanValue() failed");
      return false;
    }
    setSlotBool(vm, slot, bv == JNI_TRUE);
    return true;
  }

  if (classes->integerClass != NULL
      && (*env)->IsInstanceOf(env, obj, classes->integerClass) == JNI_TRUE) {
    jmethodID mInt = (*env)->GetMethodID(env, classes->integerClass, "intValue", "()I");
    if (mInt == NULL) {
      throw_if_exception(vm, env, "intValue() lookup failed");
      return false;
    }
    jint iv = (*env)->CallIntMethod(env, obj, mInt);
    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "intValue() failed");
      return false;
    }
    setSlotNumber(vm, slot, (double) iv);
    return true;
  }

  if (classes->longClass != NULL
      && (*env)->IsInstanceOf(env, obj, classes->longClass) == JNI_TRUE) {
    jmethodID mLong = (*env)->GetMethodID(env, classes->longClass, "longValue", "()J");
    if (mLong == NULL) {
      throw_if_exception(vm, env, "longValue() lookup failed");
      return false;
    }
    jlong lv = (*env)->CallLongMethod(env, obj, mLong);
    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "longValue() failed");
      return false;
    }
    setSlotNumber(vm, slot, (double) lv);
    return true;
  }

  if (classes->shortClass != NULL
      && (*env)->IsInstanceOf(env, obj, classes->shortClass) == JNI_TRUE) {
    jmethodID mShort = (*env)->GetMethodID(env, classes->shortClass, "shortValue", "()S");
    if (mShort == NULL) {
      throw_if_exception(vm, env, "shortValue() lookup failed");
      return false;
    }
    jshort sv = (*env)->CallShortMethod(env, obj, mShort);
    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "shortValue() failed");
      return false;
    }
    setSlotNumber(vm, slot, (double) sv);
    return true;
  }

  if (classes->byteClass != NULL
      && (*env)->IsInstanceOf(env, obj, classes->byteClass) == JNI_TRUE) {
    jmethodID mByte = (*env)->GetMethodID(env, classes->byteClass, "byteValue", "()B");
    if (mByte == NULL) {
      throw_if_exception(vm, env, "byteValue() lookup failed");
      return false;
    }
    jbyte bv = (*env)->CallByteMethod(env, obj, mByte);
    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "byteValue() failed");
      return false;
    }
    setSlotNumber(vm, slot, (double) bv);
    return true;
  }

  if (classes->floatClass != NULL
      && (*env)->IsInstanceOf(env, obj, classes->floatClass) == JNI_TRUE) {
    jmethodID mFloat = (*env)->GetMethodID(env, classes->floatClass, "floatValue", "()F");
    if (mFloat == NULL) {
      throw_if_exception(vm, env, "floatValue() lookup failed");
      return false;
    }
    jfloat fv = (*env)->CallFloatMethod(env, obj, mFloat);
    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "floatValue() failed");
      return false;
    }
    setSlotNumber(vm, slot, (double) fv);
    return true;
  }

  if (classes->doubleClass != NULL
      && (*env)->IsInstanceOf(env, obj, classes->doubleClass) == JNI_TRUE) {
    jmethodID mDouble = (*env)->GetMethodID(env, classes->doubleClass, "doubleValue", "()D");
    if (mDouble == NULL) {
      throw_if_exception(vm, env, "doubleValue() lookup failed");
      return false;
    }
    jdouble dv = (*env)->CallDoubleMethod(env, obj, mDouble);
    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "doubleValue() failed");
      return false;
    }
    setSlotNumber(vm, slot, (double) dv);
    return true;
  }

  if (classes->charClass != NULL
      && (*env)->IsInstanceOf(env, obj, classes->charClass) == JNI_TRUE) {
    jmethodID mChar = (*env)->GetMethodID(env, classes->charClass, "charValue", "()C");
    if (mChar == NULL) {
      throw_if_exception(vm, env, "charValue() lookup failed");
      return false;
    }
    jchar cv = (*env)->CallCharMethod(env, obj, mChar);
    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "charValue() failed");
      return false;
    }
    setSlotNumber(vm, slot, (double) cv);
    return true;
  }

  if (classes->numberClass != NULL
      && (*env)->IsInstanceOf(env, obj, classes->numberClass) == JNI_TRUE) {
    jmethodID mDouble = (*env)->GetMethodID(env, classes->numberClass, "doubleValue", "()D");
    if (mDouble == NULL) {
      throw_if_exception(vm, env, "doubleValue() lookup failed");
      return false;
    }
    jdouble dv = (*env)->CallDoubleMethod(env, obj, mDouble);
    if ((*env)->ExceptionCheck(env)) {
      throw_if_exception(vm, env, "doubleValue() failed");
      return false;
    }
    setSlotNumber(vm, slot, (double) dv);
    return true;
  }

  return false;
}

bool object_to_slot(JNIEnv* env, VM* vm, BridgeState* bridge, int slot, jobject obj, const char* wrapErrorMessage) {
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
      env, bridge->javaBridgeClass, bridge->mPushToSlot, saynaaObj, (jint) slot, obj);
  (*env)->DeleteLocalRef(env, saynaaObj);

  if ((*env)->ExceptionCheck(env)) {
    throw_if_exception(vm, env, "JavaBridge.pushToSlot failed");
    return false;
  }

  if (ok != JNI_TRUE) {
    SetRuntimeError(vm, wrapErrorMessage == NULL ? "Failed to convert Java value." : wrapErrorMessage);
    return false;
  }

  return true;
}
