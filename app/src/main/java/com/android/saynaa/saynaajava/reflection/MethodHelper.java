package com.android.saynaa.saynaajava.reflection;

import android.util.Log;
import com.android.saynaa.saynaajava.JavaBridge;
import com.android.saynaa.saynaajava.JavaFunction;
import com.android.saynaa.saynaajava.JavaModule;
import com.android.saynaa.saynaajava.SaynaaContext;
import com.android.saynaa.saynaajava.SaynaaException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

// Target                   Meaning           Dispatch
// Class<?>	                static call	      Class.method
// Object instance	        instance call	    obj.method

public class MethodHelper {
  private static final String TAG = "MethodHelper";

  private static Object callJavaMethod(Class<?> cls, String methodName, Object... args) {
    if (cls == null || methodName == null) {
      Log.e(TAG, "Class or method name is null for callJavaMethod.");
      return null;
    }
    Object[] normalized = ReflectionNormalizer.normalizeArgs(args);
    JavaBridge.logArgsDebug("callJavaMethod: " + cls.getName() + "." + methodName, normalized);
    Method method = ReflectionFinder.findMethod(cls, methodName, normalized);
    if (method == null) {
      JavaBridge.logMethodMismatch(cls, methodName, normalized);
      return null;
    }
    try {
      Object[] coercedArgs = method.isVarArgs() ? JavaBridge.buildVarArgs(method.getParameterTypes(), normalized)
                              : JavaBridge.coerceArgs(method.getParameterTypes(), normalized);
      Object result = method.invoke(null, coercedArgs);
      return result;
    } catch (IllegalAccessException | InvocationTargetException | IllegalArgumentException e) {
      Log.e(TAG, "Failed to invoke static method: " + method, e);
      return null;
    }
  }

  private static Object callJavaMethod(Object instance, String methodName, Object... args) {
    if (instance == null || methodName == null) {
      Log.e(TAG, "Instance or method name is null for callJavaMethod.");
      return null;
    }
    Object[] normalized = ReflectionNormalizer.normalizeArgs(args);
    JavaBridge.logArgsDebug("callJavaMethod: " + instance.getClass().getName() + "." + methodName, normalized);
    Method method = ReflectionFinder.findMethod(instance.getClass(), methodName, normalized);
    if (method == null) {
      JavaBridge.logMethodMismatch(instance.getClass(), methodName, normalized);
      return null;
    }
    try {
      Object[] coercedArgs = method.isVarArgs() ? JavaBridge.buildVarArgs(method.getParameterTypes(), normalized)
                              : JavaBridge.coerceArgs(method.getParameterTypes(), normalized);
      Object result = method.invoke(instance, coercedArgs);
      return result;

    } catch (IllegalAccessException | InvocationTargetException | IllegalArgumentException e) {
      Log.e(TAG, "Failed to invoke instance method: " + method, e);
      return null;
    }
  }

  public static Object call(Object target, String methodName, Object... args) {
    if (target instanceof Class) {
      return callJavaMethod((Class<?>) target, methodName, args);
    }
    if (target != null) {
      return callJavaMethod(target, methodName, args);
    }
    Log.e(TAG, "Unsupported target for callJavaMethodFlexible: null");
    return null;
  }
}