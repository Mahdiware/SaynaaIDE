package com.android.saynaa.saynaajava.reflection;

import android.content.Context;
import com.android.saynaa.saynaajava.JavaBridge;
import com.android.saynaa.saynaajava.JavaFunction;
import com.android.saynaa.saynaajava.JavaModule;
import com.android.saynaa.saynaajava.SaynaaContext;
import com.android.saynaa.saynaajava.SaynaaState;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;

public class ReflectionNormalizer {
  public static Object normalizeArg(Object arg) {
    if (arg == null)
      return null;
    // if (arg instanceof SaynaaObject) {
    //   return ((SaynaaObject) arg).getObject();
    // }
    if (arg instanceof SaynaaContext) {
      Context ctx = ((SaynaaContext) arg).getContext();
      return ctx != null ? ctx : arg;
    }
    return arg;
  }

  public static Object[] normalizeArgs(Object... args) {
    if (args == null || args.length == 0)
      return args;
    Object[] out = new Object[args.length];
    for (int i = 0; i < args.length; i++) {
      out[i] = normalizeArg(args[i]);
    }
    return out;
  }

  public static Object normalizeReturn(Object value) {
    if (value == null)
      return null;
    // if (value instanceof SaynaaObject)
    //   return ((SaynaaObject) value).getObject();
    if (value instanceof CharSequence)
      return value.toString();
    if (value instanceof Character)
      return (int) ((Character) value).charValue();
    if (value instanceof Boolean)
      return value;
    if (value instanceof Number)
      return value;
    return value;
  }

  public static Object defaultReturnFor(Class<?> returnType) {
    if (returnType == void.class || returnType == Void.class)
      return null;
    if (Number.class.isAssignableFrom(returnType))
      return 0;
    if (returnType == boolean.class || returnType == Boolean.class)
      return false;
    if (returnType == byte.class || returnType == Byte.class)
      return (byte) 0;
    if (returnType == short.class || returnType == Short.class)
      return (short) 0;
    if (returnType == int.class || returnType == Integer.class)
      return 0;
    if (returnType == long.class || returnType == Long.class)
      return 0L;
    if (returnType == float.class || returnType == Float.class)
      return 0f;
    if (returnType == double.class || returnType == Double.class)
      return 0d;
    if (returnType == char.class || returnType == Character.class)
      return (char) 0;
    return null;
  }

  private static Object defaultArgFor(Class<?> paramType) {
    if (paramType == null)
      return null;
    if (paramType.isPrimitive())
      return defaultReturnFor(paramType);
    if (paramType == Boolean.class)
      return Boolean.FALSE;
    if (paramType == Character.class)
      return Character.valueOf((char) 0);
    if (Number.class.isAssignableFrom(paramType))
      return Integer.valueOf(0);
    return null;
  }

  public static Object[] normalizeCallbackArgs(Method method, Object[] args) {
    if (method == null)
      return args;
    Class<?>[] paramTypes = method.getParameterTypes();
    if (paramTypes == null || paramTypes.length == 0)
      return args == null ? new Object[0] : args;

    Object[] out = new Object[paramTypes.length];
    int copyCount = args == null ? 0 : Math.min(args.length, paramTypes.length);
    for (int i = 0; i < copyCount; i++) {
      Object arg = args[i];
      if (arg == null && paramTypes[i].isPrimitive()) {
        out[i] = defaultArgFor(paramTypes[i]);
      } else {
        out[i] = arg;
      }
    }
    for (int i = copyCount; i < paramTypes.length; i++) {
      out[i] = defaultArgFor(paramTypes[i]);
    }
    return out;
  }
}