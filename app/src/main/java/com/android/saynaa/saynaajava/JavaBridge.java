package com.android.saynaa.saynaajava;

import android.content.Context;
import android.util.Log;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.HashMap;
import java.util.Map;

public class JavaBridge {
  private static final String TAG = "JavaBridge";

  private static boolean classesEqual(Class<?> a, Class<?> b) {
    return a == b || (a != null && a.equals(b));
  }

  private static int classHash(Class<?> c) {
    return c == null ? 0 : c.hashCode();
  }

  // Cache for classes
  private static final Map<String, Class<?>> classCache = new HashMap<>();
  // Cache for methods
  private static final Map<MethodKey, Method> methodCache = new HashMap<>();
  // Cache for method misses to avoid repeated reflective scans during dynamic dispatch.
  private static final Map<MethodKey, Boolean> missingMethodCache = new HashMap<>();
  // Cache for constructors
  private static final Map<ConstructorKey, Constructor<?>> constructorCache = new HashMap<>();
  // Cache for fields
  private static final Map<FieldKey, Field> fieldCache = new HashMap<>();
  // Cache for missing fields to avoid repeated reflective exceptions on method-style access.
  private static final Map<FieldKey, Boolean> missingFieldCache = new HashMap<>();

  // --- Utility classes for cache keys ---
  private static class MethodKey {
    private final Class<?> cls;
    private final String methodName;
    private final Class<?>[] paramTypes;

    MethodKey(Class<?> cls, String methodName, Class<?>[] paramTypes) {
      this.cls = cls;
      this.methodName = methodName;
      this.paramTypes = paramTypes;
    }

    @Override
    public boolean equals(Object o) {
      if (!(o instanceof MethodKey))
        return false;
      MethodKey other = (MethodKey) o;
      if (!cls.equals(other.cls) || !methodName.equals(other.methodName))
        return false;
      if (paramTypes.length != other.paramTypes.length)
        return false;
      for (int i = 0; i < paramTypes.length; i++) {
        if (!classesEqual(paramTypes[i], other.paramTypes[i]))
          return false;
      }
      return true;
    }

    @Override
    public int hashCode() {
      int result = cls.hashCode();
      result = 31 * result + methodName.hashCode();
      for (Class<?> p : paramTypes) {
        result = 31 * result + classHash(p);
      }
      return result;
    }
  }

  private static class ConstructorKey {
    private final Class<?> cls;
    private final Class<?>[] paramTypes;

    ConstructorKey(Class<?> cls, Class<?>[] paramTypes) {
      this.cls = cls;
      this.paramTypes = paramTypes;
    }

    @Override
    public boolean equals(Object o) {
      if (!(o instanceof ConstructorKey))
        return false;
      ConstructorKey other = (ConstructorKey) o;
      if (!cls.equals(other.cls))
        return false;
      if (paramTypes.length != other.paramTypes.length)
        return false;
      for (int i = 0; i < paramTypes.length; i++) {
        if (!classesEqual(paramTypes[i], other.paramTypes[i]))
          return false;
      }
      return true;
    }

    @Override
    public int hashCode() {
      int result = cls.hashCode();
      for (Class<?> p : paramTypes) {
        result = 31 * result + classHash(p);
      }
      return result;
    }
  }

  private static class FieldKey {
    private final Class<?> cls;
    private final String fieldName;

    FieldKey(Class<?> cls, String fieldName) {
      this.cls = cls;
      this.fieldName = fieldName;
    }

    @Override
    public boolean equals(Object o) {
      if (!(o instanceof FieldKey))
        return false;
      FieldKey other = (FieldKey) o;
      return cls.equals(other.cls) && fieldName.equals(other.fieldName);
    }

    @Override
    public int hashCode() {
      return cls.hashCode() * 31 + fieldName.hashCode();
    }
  }

  private static Field findFieldQuietly(Class<?> cls, String fieldName) {
    FieldKey key = new FieldKey(cls, fieldName);
    if (missingFieldCache.containsKey(key)) {
      return null;
    }

    Field field = fieldCache.get(key);
    if (field != null) {
      return field;
    }

    try {
      field = cls.getField(fieldName);
      fieldCache.put(key, field);
      Log.d(TAG, "Cached field: " + field);
      return field;
    } catch (NoSuchFieldException e) {
      missingFieldCache.put(key, Boolean.TRUE);
      return null;
    }
  }

  // --- Class loading with caching ---
  public static Class<?> findClass(String className) {
    Class<?> cls = classCache.get(className);
    if (cls != null)
      return cls;

    try {
      cls = Class.forName(className);
      classCache.put(className, cls);
      Log.d(TAG, "Found and cached class: " + className);
      return cls;
    } catch (ClassNotFoundException e) {
      Log.e(TAG, "Class not found: " + className, e);
      return null;
    }
  }

  // --- Convert boxed to primitive types for matching ---
  private static Class<?> toPrimitive(Class<?> cls) {
    if (cls == Integer.class)
      return int.class;
    if (cls == Boolean.class)
      return boolean.class;
    if (cls == Byte.class)
      return byte.class;
    if (cls == Character.class)
      return char.class;
    if (cls == Double.class)
      return double.class;
    if (cls == Float.class)
      return float.class;
    if (cls == Long.class)
      return long.class;
    if (cls == Short.class)
      return short.class;
    return cls;
  }

  // --- Check if arg type matches parameter type ---
  private static boolean isAssignable(Class<?> paramType, Class<?> argType) {
    if (paramType.isPrimitive()) {
      if (argType == null)
        return false;
      Class<?> prim = toPrimitive(argType);
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

  private static Object[] coerceArgs(Class<?>[] paramTypes, Object... args) {
    Object[] out = new Object[args.length];
    for (int i = 0; i < args.length; i++) {
      out[i] = coerceArg(paramTypes[i], args[i]);
    }
    return out;
  }

  private static Object coerceFieldValue(Class<?> fieldType, Object value) {
    if (value == null)
      return null;

    if ((fieldType == byte.class || fieldType == Byte.class) && value instanceof Number)
      return ((Number) value).byteValue();
    if ((fieldType == short.class || fieldType == Short.class) && value instanceof Number)
      return ((Number) value).shortValue();
    if ((fieldType == int.class || fieldType == Integer.class) && value instanceof Number)
      return ((Number) value).intValue();
    if ((fieldType == long.class || fieldType == Long.class) && value instanceof Number)
      return ((Number) value).longValue();
    if ((fieldType == float.class || fieldType == Float.class) && value instanceof Number)
      return ((Number) value).floatValue();
    if ((fieldType == double.class || fieldType == Double.class) && value instanceof Number)
      return ((Number) value).doubleValue();
    if ((fieldType == boolean.class || fieldType == Boolean.class) && value instanceof Number)
      return ((Number) value).intValue() != 0;
    if ((fieldType == boolean.class || fieldType == Boolean.class) && value instanceof Boolean)
      return value;
    if ((fieldType == char.class || fieldType == Character.class) && value instanceof String) {
      String stringValue = (String) value;
      return stringValue.isEmpty() ? value : stringValue.charAt(0);
    }

    return value;
  }

  // --- Find matching constructor ---
  public static Constructor<?> findConstructor(Class<?> cls, Object... args) {
    Class<?>[] argTypes = new Class<?>[args.length];
    for (int i = 0; i < args.length; i++) {
      argTypes[i] = args[i] == null ? null : args[i].getClass();
    }
    ConstructorKey key = new ConstructorKey(cls, argTypes);
    if (constructorCache.containsKey(key)) {
      return constructorCache.get(key);
    }

    for (Constructor<?> ctor : cls.getConstructors()) {
      Class<?>[] paramTypes = ctor.getParameterTypes();
      if (paramTypes.length != args.length)
        continue;

      boolean match = true;
      for (int i = 0; i < paramTypes.length; i++) {
        if (!isAssignable(paramTypes[i], argTypes[i])) {
          match = false;
          break;
        }
      }

      if (match) {
        constructorCache.put(key, ctor);
        Log.d(TAG, "Cached constructor: " + ctor);
        return ctor;
      }
    }
    Log.e(TAG, "No matching constructor found for " + cls.getName());
    return null;
  }

  // --- Create Java object dynamically ---
  public static Object createJavaObject(String fullClassName, Object... args) {
    Log.d(TAG, "Creating Java object: " + fullClassName);
    Class<?> cls = findClass(fullClassName);
    if (cls == null) {
      Log.e(TAG, "Failed to find class: " + fullClassName);
      return null;
    }

    Constructor<?> ctor = findConstructor(cls, args);
    if (ctor == null) {
      return null;
    }

    try {
      Object[] coercedArgs = coerceArgs(ctor.getParameterTypes(), args);
      return ctor.newInstance(coercedArgs);
    } catch (InstantiationException | IllegalAccessException | InvocationTargetException
        | IllegalArgumentException e) {
      Log.e(TAG, "Failed to instantiate " + fullClassName, e);
      return null;
    }
  }

  // --- Find matching method ---
  public static Method findMethod(Class<?> cls, String methodName, Object... args) {
    Class<?>[] argTypes = new Class<?>[args.length];
    for (int i = 0; i < args.length; i++) {
      argTypes[i] = args[i] == null ? null : args[i].getClass();
    }
    MethodKey key = new MethodKey(cls, methodName, argTypes);
    if (methodCache.containsKey(key)) {
      return methodCache.get(key);
    }
    if (missingMethodCache.containsKey(key)) {
      return null;
    }

    Method bestMatch = null;
    for (Method method : cls.getMethods()) {
      if (!method.getName().equals(methodName))
        continue;

      Class<?>[] paramTypes = method.getParameterTypes();
      if (paramTypes.length != args.length)
        continue;

      boolean match = true;
      for (int i = 0; i < paramTypes.length; i++) {
        if (!isAssignable(paramTypes[i], argTypes[i])) {
          match = false;
          break;
        }
      }

      if (match) {
        bestMatch = method;
        break; // can enhance to choose best match, but first match is fine
      }
    }

    if (bestMatch != null) {
      methodCache.put(key, bestMatch);
      Log.d(TAG, "Cached method: " + bestMatch);
    } else {
      missingMethodCache.put(key, Boolean.TRUE);
    }
    return bestMatch;
  }

  // --- Call instance method ---
  public static Object callJavaMethod(Object javaObject, String methodName, Object... args) {
    if (javaObject == null) {
      Log.e(TAG, "Java object is null.");
      return null;
    }

    Class<?> cls = javaObject.getClass();
    Method method = findMethod(cls, methodName, args);
    if (method == null)
      return null;

    try {
      Object[] coercedArgs = coerceArgs(method.getParameterTypes(), args);
      return method.invoke(javaObject, coercedArgs);
    } catch (IllegalAccessException | InvocationTargetException | IllegalArgumentException e) {
      Log.e(TAG, "Error invoking method: " + methodName, e);
      return null;
    }
  }

  // --- Call static method ---
  public static Object callStaticJavaMethod(String className, String methodName, Object... args) {
    Class<?> cls = findClass(className);
    if (cls == null)
      return null;

    Method method = findMethod(cls, methodName, args);
    if (method == null)
      return null;

    try {
      Object[] coercedArgs = coerceArgs(method.getParameterTypes(), args);
      return method.invoke(null, coercedArgs);
    } catch (IllegalAccessException | InvocationTargetException | IllegalArgumentException e) {
      Log.e(TAG, "Error invoking static method: " + methodName, e);
      return null;
    }
  }

  // --- Get field value (instance or static) ---
  public static Object getFieldValue(Object objOrClass, String fieldName) {
    Class<?> cls;
    boolean isStaticAccess = false;
    if (objOrClass instanceof Class) {
      cls = (Class<?>) objOrClass;
      isStaticAccess = true;
    } else if (objOrClass != null) {
      cls = objOrClass.getClass();
    } else {
      Log.e(TAG, "Object or Class is null for getFieldValue");
      return null;
    }

    Field field = findFieldQuietly(cls, fieldName);
    if (field == null)
      return null;

    try {
      return field.get(isStaticAccess ? null : objOrClass);
    } catch (IllegalAccessException e) {
      Log.e(TAG, "Error accessing field: " + fieldName, e);
      return null;
    }
  }

  // --- Set field value (instance or static) ---
  public static boolean setFieldValue(Object objOrClass, String fieldName, Object value) {
    Class<?> cls;
    boolean isStaticAccess = false;
    if (objOrClass instanceof Class) {
      cls = (Class<?>) objOrClass;
      isStaticAccess = true;
    } else if (objOrClass != null) {
      cls = objOrClass.getClass();
    } else {
      Log.e(TAG, "Object or Class is null for setFieldValue");
      return false;
    }

    Field field = findFieldQuietly(cls, fieldName);
    if (field == null)
      return false;

    try {
      field.set(isStaticAccess ? null : objOrClass, coerceFieldValue(field.getType(), value));
      return true;
    } catch (IllegalAccessException | IllegalArgumentException e) {
      Log.e(TAG, "Error setting field: " + fieldName, e);
      return false;
    }
  }

  public static Object createProxy(
      Saynaa saynaa, String interfaceName, String methodName, String script) {
    return SaynaaProxyFactory.createProxy(saynaa, interfaceName, methodName, script);
  }

  public static Object createNativeCallbackProxy(
      final Saynaa saynaa, final String interfaceName, final String methodName, final int callbackId) {
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
        Class<?> iface = Class.forName(n);
        ifaces[i] = iface;
        if (loader == null)
          loader = iface.getClassLoader();
      }

      final boolean wildcard = "*".equals(methodName);
      InvocationHandler handler =
          new InvocationHandler() {
            @Override
            public Object invoke(Object proxy, Method method, Object[] args) {
              String m = method.getName();
              if ("toString".equals(m) && method.getParameterTypes().length == 0)
                return "SaynaaNativeCallbackProxy(" + interfaceName + ")";
              if ("hashCode".equals(m) && method.getParameterTypes().length == 0)
                return System.identityHashCode(proxy);
              if ("equals".equals(m) && method.getParameterTypes().length == 1)
                return proxy == (args == null ? null : args[0]);

              if (saynaa != null && !saynaa.isClosed() && (wildcard || (methodName != null && methodName.equals(m)))) {
                Object callbackResult = saynaa.invokeCallbackMethodWithResult(callbackId, m, args);

                Class<?> rt = method.getReturnType();
                if (rt == void.class)
                  return null;

                if (callbackResult != null) {
                  if (rt == boolean.class || rt == Boolean.class)
                    return (callbackResult instanceof Boolean) ? callbackResult : Boolean.TRUE;
                  if (rt == char.class || rt == Character.class) {
                    if (callbackResult instanceof Character)
                      return callbackResult;
                    if (callbackResult instanceof Number)
                      return (char) ((Number) callbackResult).intValue();
                  }
                  if (rt == byte.class || rt == Byte.class || rt == short.class || rt == Short.class
                      || rt == int.class || rt == Integer.class || rt == long.class || rt == Long.class
                      || rt == float.class || rt == Float.class || rt == double.class || rt == Double.class) {
                    return coerceArg(rt, callbackResult);
                  }
                  if (!rt.isPrimitive() && rt.isInstance(callbackResult))
                    return callbackResult;
                }

                if (!rt.isPrimitive())
                  return null;
                if (rt == boolean.class)
                  return true;
                if (rt == byte.class)
                  return (byte) 0;
                if (rt == short.class)
                  return (short) 0;
                if (rt == int.class)
                  return 0;
                if (rt == long.class)
                  return 0L;
                if (rt == float.class)
                  return 0f;
                if (rt == double.class)
                  return 0d;
                if (rt == char.class)
                  return (char) 0;
                return null;
              }

              Class<?> rt = method.getReturnType();
              if (rt == boolean.class)
                return false;
              if (rt == byte.class)
                return (byte) 0;
              if (rt == short.class)
                return (short) 0;
              if (rt == int.class)
                return 0;
              if (rt == long.class)
                return 0L;
              if (rt == float.class)
                return 0f;
              if (rt == double.class)
                return 0d;
              if (rt == char.class)
                return (char) 0;
              return null;
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

      Class<?> iface = Class.forName(names[0].trim());
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
