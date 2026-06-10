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
}