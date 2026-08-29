package com.android.saynaa.saynaajava;

import android.content.Context;
import android.util.Log;
import android.view.Menu;
import com.android.saynaa.saynaajava.datatype.*;
import com.android.saynaa.saynaajava.reflection.FieldHelper;
import com.android.saynaa.saynaajava.reflection.ReflectionFinder;
import com.android.saynaa.saynaajava.reflection.ReflectionKeys.ConstructorKey;
import com.android.saynaa.saynaajava.reflection.ReflectionKeys.FieldKey;
import com.android.saynaa.saynaajava.reflection.ReflectionKeys.MethodKey;
import com.android.saynaa.saynaajava.reflection.ReflectionNormalizer;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.IdentityHashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

public class JavaBridge {
  private static final String TAG = "JavaBridge";
  private static final int MAX_BRIDGE_RECURSION_DEPTH = 64;
  private static final long MAX_SAFE_INTEGER_LONG = 9007199254740991L;
  private static final long MIN_SAFE_INTEGER_LONG = -9007199254740991L;

  private static boolean isFiniteDouble(double value) {
    return !Double.isNaN(value) && !Double.isInfinite(value);
  }

  private static Object decodeSlotNumber(double value) {
    if (!isFiniteDouble(value)) {
      return Double.valueOf(value);
    }

    double rounded = Math.rint(value);
    if (rounded == value) {
      if (value >= Integer.MIN_VALUE && value <= Integer.MAX_VALUE) {
        return Integer.valueOf((int) value);
      }

      if (value >= MIN_SAFE_INTEGER_LONG && value <= MAX_SAFE_INTEGER_LONG) {
        return Long.valueOf((long) value);
      }
    }

    return Double.valueOf(value);
  }

  public static Object slotToJava(Saynaa saynaa, int slot) {
    if (saynaa == null || saynaa.isClosed()) {
      return null;
    }

    // FORCE the scratchpad baseline to start well clear of the current slot
    // and any active argument frame boundaries.
    int scratchStart = Math.max(slot, saynaa.getSlotCount());

    return slotToJavaInternal(saynaa, slot, scratchStart, 0);
  }

  private static Object slotToJavaInternal(Saynaa saynaa, int slot, int scratchSlot, int depth) {
    if (saynaa == null || saynaa.isClosed()) {
      return null;
    }

    if (depth >= MAX_BRIDGE_RECURSION_DEPTH) {
      Log.e(TAG, "slotToJava depth limit reached at slot " + slot);
      return null;
    }

    int type = saynaa.getSlotType(slot);

    switch (type) {
    case Saynaa.SLOT_TYPE_NULL:
      return null;

    case Saynaa.SLOT_TYPE_BOOL:
      return Boolean.valueOf(saynaa.getSlotBool(slot));

    case Saynaa.SLOT_TYPE_NUMBER:
      return decodeSlotNumber(saynaa.getSlotNumber(slot));

    case Saynaa.SLOT_TYPE_STRING:
      return saynaa.getSlotString(slot);

    case Saynaa.SLOT_TYPE_POINTER:
      return saynaa.getSlotJavaObject(slot);
    case Saynaa.SLOT_TYPE_INSTANCE: {
      if (saynaa.isSlotJava(slot)) {
        return saynaa.getSlotJavaObject(slot);
      } else {
        return new SaynaaInstance(saynaa, type, saynaa.captureSlotHandle(slot));
      }
    }
    case Saynaa.SLOT_TYPE_LIST: {
      return new SaynaaList(saynaa, type, saynaa.captureSlotHandle(slot));
    }

    case Saynaa.SLOT_TYPE_MAP: {
      return new SaynaaMap(saynaa, type, saynaa.captureSlotHandle(slot));
    }

    case Saynaa.SLOT_TYPE_CLASS:
      return new SaynaaClass(saynaa, type, saynaa.captureSlotHandle(slot));

    case Saynaa.SLOT_TYPE_MODULE:
      return new SaynaaModule(saynaa, type, saynaa.captureSlotHandle(slot));

    default:
      return new SaynaaObject(saynaa, type, saynaa.captureSlotHandle(slot));
    }
  }

  // findClass used in JNI
  public static Class<?> findClass(String className) {
    return ReflectionFinder.findClass(className);
  }

  // getFieldValue and setFieldValue used in JNI
  public static boolean setFieldValue(Object objOrClass, String fieldName, Object value) {
    return FieldHelper.setFieldValue(objOrClass, fieldName, value);
  }

  public static Object getFieldValue(Object objOrClass, String fieldName) {
    return FieldHelper.getFieldValue(objOrClass, fieldName);
  }

  public static Object[] argsFromSlots(Saynaa saynaa, int startSlot, int argc) {
    if (saynaa == null || saynaa.isClosed() || argc <= 0)
      return new Object[0];

    Object[] out = new Object[argc];
    for (int i = 0; i < argc; i++) {
      out[i] = slotToJava(saynaa, startSlot + i);
    }
    return out;
  }

  // --- Check if arg type matches parameter type ---
  private static boolean isAssignable(Class<?> paramType, Class<?> argType) {
    if (paramType.isPrimitive()) {
      if (argType == null)
        return false;
      Class<?> prim = ReflectionFinder.toPrimitive(argType);
      if (paramType.equals(prim))
        return true;

      // Allow numeric conversions (Double -> int, etc.)
      if ((paramType == int.class || paramType == long.class || paramType == short.class
              || paramType == byte.class || paramType == float.class || paramType == double.class)
          && Number.class.isAssignableFrom(argType)) {
        return true;
      }
      return false;
    }
    if (argType == null) {
      // null can match any non-primitive type
      return !paramType.isPrimitive();
    }

    // Allow numeric conversions for boxed numeric types.
    if (Number.class.isAssignableFrom(paramType) && Number.class.isAssignableFrom(argType)) {
      return true;
    }

    return paramType.isAssignableFrom(argType);
  }

  private static Object coerceArg(Class<?> paramType, Object arg) {
    if (arg == null)
      return null;

    if (paramType == int.class || paramType == Integer.class)
      return arg instanceof Number ? ((Number) arg).intValue() : arg;
    if (paramType == long.class || paramType == Long.class)
      return arg instanceof Number ? ((Number) arg).longValue() : arg;
    if (paramType == short.class || paramType == Short.class)
      return arg instanceof Number ? ((Number) arg).shortValue() : arg;
    if (paramType == byte.class || paramType == Byte.class)
      return arg instanceof Number ? ((Number) arg).byteValue() : arg;
    if (paramType == float.class || paramType == Float.class)
      return arg instanceof Number ? ((Number) arg).floatValue() : arg;
    if (paramType == double.class || paramType == Double.class)
      return arg instanceof Number ? ((Number) arg).doubleValue() : arg;

    return arg;
  }

  public static Object[] coerceArgs(Class<?>[] paramTypes, Object... args) {
    Object[] out = new Object[args.length];
    for (int i = 0; i < args.length; i++) {
      out[i] = coerceArg(paramTypes[i], args[i]);
    }
    return out;
  }

  private static Object createJavaObject(Class<?> cls, Object... args) {
    if (cls == null) {
      Log.e(TAG, "Failed to resolve class for creation.");
      return null;
    }

    Object[] normalized = ReflectionNormalizer.normalizeArgs(args);
    Constructor<?> ctor = ReflectionFinder.findConstructor(cls, normalized);
    if (ctor == null) {
      logConstructorMismatch(cls, normalized);
      return null;
    }

    try {
      Object[] coercedArgs = coerceArgs(ctor.getParameterTypes(), normalized);
      Object instance = ctor.newInstance(coercedArgs);
      if (instance == null) {
        Log.e(TAG, "Constructor returned null for " + cls.getName());
      }
      return instance;
    } catch (InstantiationException | IllegalAccessException | InvocationTargetException
             | IllegalArgumentException e) {
      Log.e(TAG, "Failed to instantiate " + cls.getName(), e);
      return null;
    }
  }

  // --- Create Java object dynamically ---
  public static Object createJavaObject(String fullClassName, Object... args) {
    logArgsDebug("createJavaObject", args);
    Class<?> cls = ReflectionFinder.findClass(fullClassName);
    if (cls == null) {
      Log.e(TAG, "Failed to find class: " + fullClassName);
      return null;
    }
    return createJavaObject(cls, args);
  }

  public static Object createJavaObjectFlexible(Object classOrName, Object... args) {
    if (classOrName instanceof Class) {
      return createJavaObject((Class<?>) classOrName, args);
    }
    if (classOrName instanceof String) {
      return createJavaObject((String) classOrName, args);
    }
    if (classOrName != null) {
      Log.e(TAG, "Unsupported class target: " + classOrName.getClass().getName());
    }
    return null;
  }

  public static void logMethodMismatch(Class<?> cls, String methodName, Object[] args) {
    StringBuilder sb = new StringBuilder();
    sb.append("No matching method found: ").append(cls.getName()).append(".").append(methodName).append("(");
    for (int i = 0; i < args.length; i++) {
      if (i > 0)
        sb.append(", ");
      sb.append(args[i] == null ? "null" : args[i].getClass().getSimpleName());
    }
    sb.append(")");
    Log.e(TAG, sb.toString());
  }

  private static Class<?> resolveClass(Object classOrName) {
    if (classOrName instanceof Class)
      return (Class<?>) classOrName;
    if (classOrName instanceof String)
      return ReflectionFinder.findClass((String) classOrName);
    if (classOrName != null)
      Log.e(TAG, "Unsupported class target: " + classOrName.getClass().getName());
    return null;
  }

  private static String resolveInterfaceName(Object interfaceOrName) {
    if (interfaceOrName instanceof String)
      return (String) interfaceOrName;
    if (interfaceOrName instanceof Class)
      return ((Class<?>) interfaceOrName).getName();
    if (interfaceOrName != null)
      Log.e(TAG, "Unsupported interface target: " + interfaceOrName.getClass().getName());
    return null;
  }

  public static String resolveCallbackInterface(Object target, String methodName, int argc, int argIndex) {
    if (target == null || methodName == null || methodName.trim().isEmpty())
      return null;
    if (argc < 0 || argIndex < 0)
      return null;

    Class<?> cls = (target instanceof Class) ? (Class<?>) target : target.getClass();
    for (Method method : ReflectionFinder.getMethodsCached(cls)) {
      if (!method.getName().equals(methodName))
        continue;

      Class<?>[] paramTypes = method.getParameterTypes();
      boolean isVarArgs = method.isVarArgs();

      if (!isVarArgs && paramTypes.length != argc)
        continue;
      if (isVarArgs && argc < paramTypes.length - 1)
        continue;

      Class<?> paramType;
      if (isVarArgs && argIndex >= paramTypes.length - 1) {
        paramType = paramTypes[paramTypes.length - 1].getComponentType();
      } else if (argIndex < paramTypes.length) {
        paramType = paramTypes[argIndex];
      } else {
        continue;
      }

      if (paramType != null && paramType.isInterface())
        return paramType.getName();
    }

    return null;
  }

  public static Object[] buildVarArgs(Class<?>[] paramTypes, Object[] normalized) {
    int fixedCount = paramTypes.length - 1;
    Class<?> varType = paramTypes[fixedCount].getComponentType();
    int varCount = Math.max(0, normalized.length - fixedCount);
    Object varArray = Array.newInstance(varType, varCount);

    for (int i = 0; i < varCount; i++) {
      Object coerced = coerceArg(varType, normalized[fixedCount + i]);
      Array.set(varArray, i, coerced);
    }

    Object[] out = new Object[paramTypes.length];
    for (int i = 0; i < fixedCount; i++) {
      out[i] = coerceArg(paramTypes[i], normalized[i]);
    }
    out[fixedCount] = varArray;
    return out;
  }

  // --- Call instance method ---
  // it used in JNI, so it must be public
  public static Object callJavaMethod(Object javaObject, String methodName, Object... args) {
    Object target = ReflectionNormalizer.normalizeArg(javaObject);
    if (target == null) {
      Log.e(TAG, "Java object is null.");
      return null;
    }

    Object[] normalized = ReflectionNormalizer.normalizeArgs(args);
    logArgsDebug("callJavaMethod " + methodName, normalized);

    Class<?> cls = target.getClass();
    Method method = ReflectionFinder.findMethod(cls, methodName, normalized);
    if (method == null)
      return null;

    try {
      Object[] coercedArgs = method.isVarArgs() ? buildVarArgs(method.getParameterTypes(), normalized)
                                                : coerceArgs(method.getParameterTypes(), normalized);
      Object ret = method.invoke(target, coercedArgs);
      return ReflectionNormalizer.normalizeReturn(ret);
    } catch (IllegalAccessException | InvocationTargetException | IllegalArgumentException e) {
      Log.e(TAG, "Error invoking method: " + methodName, e);
      return null;
    }
  }

  // --- Call static method ---
  public static Object callStaticJavaMethod(String className, String methodName, Object... args) {
    Class<?> cls = ReflectionFinder.findClass(className);
    if (cls == null)
      return null;

    Object[] normalized = ReflectionNormalizer.normalizeArgs(args);
    logArgsDebug("callStaticJavaMethod " + className + "." + methodName, normalized);

    Method method = ReflectionFinder.findMethod(cls, methodName, normalized);
    if (method == null)
      return null;

    try {
      Object[] coercedArgs = method.isVarArgs() ? buildVarArgs(method.getParameterTypes(), normalized)
                                                : coerceArgs(method.getParameterTypes(), normalized);
      Object ret = method.invoke(null, coercedArgs);
      return ReflectionNormalizer.normalizeReturn(ret);
    } catch (IllegalAccessException | InvocationTargetException | IllegalArgumentException e) {
      Log.e(TAG, "Error invoking static method: " + methodName, e);
      return null;
    }
  }

  public static boolean createFromSlots(Saynaa saynaa, int classSlot, int valueSlot, int argc, int outSlot) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    Object classOrName = slotToJava(saynaa, classSlot);
    Class<?> cls = resolveClass(classOrName);
    if (cls == null)
      return false;

    if (cls.isInterface())
      return false;

    Object created = null;
    if (cls.isArray()) {
      if (argc < 2)
        return false;
      Object value = slotToJava(saynaa, valueSlot);
      if (!(value instanceof List))
        return false;

      List<?> list = (List<?>) value;
      Class<?> component = cls.getComponentType();
      Object arrayObj = Array.newInstance(component, list.size());
      for (int i = 0; i < list.size(); i++) {
        Array.set(arrayObj, i, list.get(i));
      }
      created = arrayObj;
    } else if (List.class.isAssignableFrom(cls)) {
      ArrayList<Object> list = new ArrayList<>();
      if (argc >= 2 && saynaa.getSlotType(valueSlot) == Saynaa.SLOT_TYPE_LIST) {
        Object value = slotToJava(saynaa, valueSlot);
        if (value instanceof List)
          list.addAll((List<?>) value);
      }
      created = list;
    } else if (Map.class.isAssignableFrom(cls)) {
      HashMap<Object, Object> map = new HashMap<>();
      if (argc >= 2 && saynaa.getSlotType(valueSlot) == Saynaa.SLOT_TYPE_MAP) {
        Object value = slotToJava(saynaa, valueSlot);
        if (value instanceof Map)
          map.putAll((Map<?, ?>) value);
      }
      created = map;
    } else {
      Object[] args = argsFromSlots(saynaa, valueSlot, Math.max(argc - 1, 0));
      created = createJavaObject(cls, args);
    }

    return pushToSlot(saynaa, outSlot, created);
  }

  public static boolean newFromSlots(Saynaa saynaa, int classSlot, int argsStart, int argc, int outSlot) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    Object classOrName = slotToJava(saynaa, classSlot);
    Object[] args = argsFromSlots(saynaa, argsStart, argc);
    Object ret = createJavaObjectFlexible(classOrName, args);
    return pushToSlot(saynaa, outSlot, ret);
  }

  private static void logConstructorMismatch(Class<?> cls, Object[] args) {
    StringBuilder sb = new StringBuilder();
    sb.append("Constructor mismatch for ").append(cls.getName()).append(". Args=");
    if (args == null) {
      sb.append("null");
    } else {
      sb.append("[");
      for (int i = 0; i < args.length; i++) {
        if (i > 0)
          sb.append(", ");
        Object arg = args[i];
        sb.append(arg == null ? "null" : arg.getClass().getName());
      }
      sb.append("]");
    }
    Log.e(TAG, sb.toString());

    for (Constructor<?> ctor : cls.getConstructors()) {
      Log.e(TAG, "Available ctor: " + ctor.toString());
    }
  }

  public static void logArgsDebug(String prefix, Object[] args) {
    if (!Log.isLoggable(TAG, Log.DEBUG))
      return;
    StringBuilder sb = new StringBuilder();
    sb.append(prefix).append(" args=");
    if (args == null) {
      sb.append("null");
    } else {
      sb.append("[");
      for (int i = 0; i < args.length; i++) {
        if (i > 0)
          sb.append(", ");
        Object arg = args[i];
        if (arg == null) {
          sb.append("null");
        } else {
          sb.append(arg.getClass().getName());
          sb.append("=");
          sb.append(arg.toString());
        }
      }
      sb.append("]");
    }
    Log.d(TAG, sb.toString());
  }

  private static boolean pushNumberToSlot(Saynaa saynaa, int slot, Number numberValue) {
    if (numberValue == null)
      return false;

    if (numberValue instanceof BigInteger || numberValue instanceof BigDecimal) {
      return saynaa.bindJavaObject(slot, numberValue);
    }

    if (numberValue instanceof Long) {
      long longValue = numberValue.longValue();
      if (longValue < MIN_SAFE_INTEGER_LONG || longValue > MAX_SAFE_INTEGER_LONG) {
        return saynaa.bindJavaObject(slot, numberValue);
      }
      saynaa.setSlotNumber(slot, (double) longValue);
      return true;
    }

    double numeric = numberValue.doubleValue();
    if (!isFiniteDouble(numeric)) {
      return saynaa.bindJavaObject(slot, numberValue);
    }

    saynaa.setSlotNumber(slot, numeric);
    return true;
  }

  private static boolean pushScalarToSlot(Saynaa saynaa, int slot, Object normalized) {
    if (normalized == null) {
      saynaa.setSlotNull(slot);
      return true;
    }

    if (normalized instanceof Boolean) {
      saynaa.setSlotBool(slot, (Boolean) normalized);
      return true;
    }

    if (normalized instanceof Number) {
      return pushNumberToSlot(saynaa, slot, (Number) normalized);
    }

    if (normalized instanceof String) {
      saynaa.setSlotString(slot, normalized.toString());
      return true;
    }

    // SaynaaObject
    if (normalized instanceof SaynaaObject) {
      SaynaaObject object = (SaynaaObject) normalized;
      if (object.getHandleId() > 0) {
        saynaa.setSlotPinnedHandle(slot, object.getHandleId());
        return true;
      }
    }

    return false;
  }

  public static boolean pushToSlot(Saynaa saynaa, int slot, Object value) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    Object normalized = ReflectionNormalizer.normalizeReturn(value);
    // saynaa.reserveSlots(slot + 2);

    if (pushScalarToSlot(saynaa, slot, normalized)) {
      return true;
    }

    if (normalized instanceof Class<?>) {
      return saynaa.bindJavaClass(slot, (Class<?>) normalized);
    }

    // JavaMethodBinding
    if (normalized instanceof JavaMethodBinding) {
      JavaMethodBinding binding = (JavaMethodBinding) normalized;
      return saynaa.bindJavaMethod(slot, binding.getTarget(), binding.getMethodName());
    }

    return saynaa.bindJavaObject(slot, normalized);
  }

  private static IdentityHashMap<Object, Boolean> ensureVisitingMap(IdentityHashMap<Object, Boolean> visiting) {
    return visiting != null ? visiting : new IdentityHashMap<Object, Boolean>();
  }

  public static Object createProxy(Saynaa saynaa, String interfaceName, String methodName, String script) {
    return SaynaaProxyFactory.createProxy(saynaa, interfaceName, methodName, script);
  }

  private static Object invokeCallbackFromJava(
      Saynaa saynaa, int callbackId, String methodName, Method method, Object[] args) {
    if (saynaa == null || saynaa.isClosed() || callbackId <= 0)
      return null;

    Object[] safeArgs = ReflectionNormalizer.normalizeCallbackArgs(method, args);
    int argc = safeArgs == null ? 0 : safeArgs.length;
    int argStart = saynaa.allocSlot(argc + 4);

    for (int i = 0; i < argc; i++) {
      int slot = argStart + i;
      if (!pushToSlot(saynaa, slot, safeArgs[i])) {
        saynaa.freeSlot(argStart, argc + 4);
        return null;
      }
    }

    return saynaa.invokeCallbackMethodWithResultFromSlots(callbackId, methodName, argStart, argc);
  }

  private static void sendProxyError(Saynaa saynaa, String methodName, Throwable t) {
    if (saynaa != null && saynaa.context instanceof SaynaaContext) {
      Exception ex = t instanceof Exception ? (Exception) t : new SaynaaException(t);
      ((SaynaaContext) saynaa.context).sendError(methodName, ex);
      return;
    }
    Log.e(TAG, "Proxy error: " + methodName, t);
  }

  private static Object coerceCallbackResult(Class<?> returnType, Object callbackResult) {
    if (returnType == void.class || returnType == Void.class)
      return null;

    Object normalized = ReflectionNormalizer.normalizeReturn(callbackResult);
    if (normalized == null)
      return ReflectionNormalizer.defaultReturnFor(returnType);

    if (returnType == boolean.class || returnType == Boolean.class) {
      if (normalized instanceof Boolean)
        return normalized;
      if (normalized instanceof Number)
        return ((Number) normalized).intValue() != 0;
      if (normalized instanceof CharSequence) {
        String text = normalized.toString().trim();
        if ("true".equalsIgnoreCase(text))
          return true;
        if ("false".equalsIgnoreCase(text))
          return false;
      }
      return Boolean.TRUE;
    }

    if (returnType == char.class || returnType == Character.class) {
      if (normalized instanceof Character)
        return normalized;
      if (normalized instanceof Number)
        return (char) ((Number) normalized).intValue();
      if (normalized instanceof CharSequence) {
        String text = normalized.toString();
        return text.isEmpty() ? ReflectionNormalizer.defaultReturnFor(returnType) : text.charAt(0);
      }
      return ReflectionNormalizer.defaultReturnFor(returnType);
    }

    if (returnType == byte.class || returnType == Byte.class || returnType == short.class
        || returnType == Short.class || returnType == int.class || returnType == Integer.class
        || returnType == long.class || returnType == Long.class || returnType == float.class
        || returnType == Float.class || returnType == double.class || returnType == Double.class) {
      if (normalized instanceof Boolean)
        normalized = ((Boolean) normalized) ? 1 : 0;
      return coerceArg(returnType, normalized);
    }

    if (returnType == String.class && normalized instanceof CharSequence)
      return normalized.toString();

    if (!returnType.isPrimitive() && returnType.isInstance(normalized))
      return normalized;

    return ReflectionNormalizer.defaultReturnFor(returnType);
  }

  public static Object createNativeCallbackProxy(final Saynaa saynaa, final String interfaceName,
      final String methodName, final int callbackId) {
    try {
      if (interfaceName == null || interfaceName.trim().isEmpty()) {
        Log.e(TAG, "createNativeCallbackProxy failed: empty interfaceName");
        return null;
      }

      String[] names = interfaceName.split(",");
      Class<?>[] ifaces = new Class<?>[names.length];
      ClassLoader loader = null;
      for (int i = 0; i < names.length; i++) {
        String n = names[i] == null ? "" : names[i].trim();
        if (n.isEmpty()) {
          Log.e(TAG, "createNativeCallbackProxy failed: invalid interface list: " + interfaceName);
          return null;
        }
        Class<?> iface = ReflectionFinder.findClass(n);
        if (iface == null) {
          Log.e(TAG, "createNativeCallbackProxy failed: class not found: " + n);
          return null;
        }
        ifaces[i] = iface;
        if (loader == null)
          loader = iface.getClassLoader();
      }

      final boolean wildcard = "*".equals(methodName);
      InvocationHandler handler = new InvocationHandler() {
        @Override
        public Object invoke(Object proxy, Method method, Object[] args) {
          String m = method.getName();
          if ("toString".equals(m) && method.getParameterTypes().length == 0)
            return "SaynaaNativeCallbackProxy(" + interfaceName + ")";
          if ("hashCode".equals(m) && method.getParameterTypes().length == 0)
            return System.identityHashCode(proxy);
          if ("equals".equals(m) && method.getParameterTypes().length == 1)
            return proxy == (args == null ? null : args[0]);

          if (saynaa != null && !saynaa.isClosed()
              && (wildcard || (methodName != null && methodName.equals(m)))) {
            Class<?> rt = method.getReturnType();
            try {
              Object callbackResult = invokeCallbackFromJava(saynaa, callbackId, m, method, args);
              return coerceCallbackResult(rt, callbackResult);
            } catch (Throwable t) {
              sendProxyError(saynaa, m, t);
              return ReflectionNormalizer.defaultReturnFor(rt);
            }
          }

          Class<?> rt = method.getReturnType();
          return ReflectionNormalizer.defaultReturnFor(rt);
        }
      };

      return Proxy.newProxyInstance(loader, ifaces, handler);
    } catch (Throwable t) {
      Log.e(TAG, "createNativeCallbackProxy failed: " + interfaceName + "." + methodName, t);
      return null;
    }
  }

  public static String getDefaultInterfaceMethodName(String interfaceName) {
    try {
      if (interfaceName == null || interfaceName.trim().isEmpty())
        return "*";

      String[] names = interfaceName.split(",");
      if (names.length != 1) {
        // Multi-interface proxy has no single default method.
        return "*";
      }

      Class<?> iface = ReflectionFinder.findClass(names[0].trim());
      if (iface == null)
        return "*";
      Method[] methods = iface.getMethods();
      String found = null;
      for (Method m : methods) {
        if (m.getDeclaringClass() == Object.class)
          continue;
        int mod = m.getModifiers();
        if (!java.lang.reflect.Modifier.isAbstract(mod))
          continue;
        if (found != null && !found.equals(m.getName())) {
          // Non-SAM interface; wildcard is used by map/table callbacks.
          return "*";
        }
        found = m.getName();
      }
      return found == null ? "*" : found;
    } catch (Throwable t) {
      Log.e(TAG, "getDefaultInterfaceMethodName failed: " + interfaceName, t);
      return "*";
    }
  }
}
