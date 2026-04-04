package com.android.saynaa.saynaajava;

import android.content.Context;
import android.util.Log;
import android.view.Menu;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

public class JavaBridge {
  private static final String TAG = "JavaBridge";

  private static Saynaa unwrapState(SaynaaState state) {
    return state == null ? null : state.getSaynaa();
  }

  public static Object slotToJava(SaynaaState state, int slot) {
    return slotToJava(unwrapState(state), slot);
  }

  public static Object slotToJava(Saynaa saynaa, int slot) {
    if (saynaa == null || saynaa.isClosed())
      return null;

    int type = saynaa.getSlotType(slot);
    switch (type) {
    case Saynaa.SLOT_TYPE_NULL:
      return null;
    case Saynaa.SLOT_TYPE_BOOL:
      return Boolean.valueOf(saynaa.getSlotBool(slot));
    case Saynaa.SLOT_TYPE_NUMBER:
      return Double.valueOf(saynaa.getSlotNumber(slot));
    case Saynaa.SLOT_TYPE_STRING:
      return saynaa.getSlotString(slot);
    case Saynaa.SLOT_TYPE_POINTER:
    case Saynaa.SLOT_TYPE_INSTANCE:
      return saynaa.getSlotJavaObject(slot);
    case Saynaa.SLOT_TYPE_LIST: {
      int size = saynaa.getListSize(slot);
      ArrayList<Object> out = new ArrayList<>(Math.max(size, 0));
      int valueSlot = slot + 1;
      saynaa.reserveSlots(valueSlot + 1);
      for (int i = 0; i < size; i++) {
        if (saynaa.listGetToSlot(slot, i, valueSlot)) {
          out.add(slotToJava(saynaa, valueSlot));
        } else {
          out.add(null);
        }
      }
      return out;
    }
    case Saynaa.SLOT_TYPE_MAP: {
      int size = saynaa.getMapSize(slot);
      HashMap<Object, Object> out = new HashMap<>(Math.max(size, 0));
      int keySlot = slot + 1;
      int valueSlot = slot + 2;
      saynaa.reserveSlots(valueSlot + 1);
      for (int i = 0; i < size; i++) {
        if (saynaa.mapEntryToSlots(slot, i, keySlot, valueSlot)) {
          Object key = slotToJava(saynaa, keySlot);
          Object value = slotToJava(saynaa, valueSlot);
          out.put(key, value);
        }
      }
      return out;
    }
    default:
      return null;
    }
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

  public static Object[] argsFromSlots(SaynaaState state, int startSlot, int argc) {
    return argsFromSlots(unwrapState(state), startSlot, argc);
  }

  public static double lengthOf(Object value) {
    if (value == null)
      return 0;
    if (value instanceof CharSequence)
      return ((CharSequence) value).length();
    if (value instanceof Collection)
      return ((Collection<?>) value).size();
    if (value instanceof Map)
      return ((Map<?, ?>) value).size();
    if (value.getClass().isArray())
      return Array.getLength(value);
    return -1;
  }

  public static String javaToString(Object value) {
    return value == null ? "null" : String.valueOf(value);
  }

  public static boolean astableToSlot(Saynaa saynaa, int listSlot, Object value) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    saynaa.newList(listSlot);
    if (value == null)
      return true;

    int elemSlot = listSlot + 1;
    saynaa.reserveSlots(elemSlot + 1);

    if (value.getClass().isArray()) {
      int len = Array.getLength(value);
      for (int i = 0; i < len; i++) {
        Object item = Array.get(value, i);
        if (!pushToSlot(saynaa, elemSlot, item))
          return false;
        if (!saynaa.listInsert(listSlot, -1, elemSlot))
          return false;
      }
      return true;
    }

    if (value instanceof Iterable) {
      for (Object item : (Iterable<?>) value) {
        if (!pushToSlot(saynaa, elemSlot, item))
          return false;
        if (!saynaa.listInsert(listSlot, -1, elemSlot))
          return false;
      }
      return true;
    }

    if (value instanceof Iterator) {
      Iterator<?> it = (Iterator<?>) value;
      while (it.hasNext()) {
        Object item = it.next();
        if (!pushToSlot(saynaa, elemSlot, item))
          return false;
        if (!saynaa.listInsert(listSlot, -1, elemSlot))
          return false;
      }
      return true;
    }

    if (value instanceof Enumeration) {
      Enumeration<?> en = (Enumeration<?>) value;
      while (en.hasMoreElements()) {
        Object item = en.nextElement();
        if (!pushToSlot(saynaa, elemSlot, item))
          return false;
        if (!saynaa.listInsert(listSlot, -1, elemSlot))
          return false;
      }
      return true;
    }

    return false;
  }

  public static boolean astableToSlot(SaynaaState state, int listSlot, Object value) {
    return astableToSlot(unwrapState(state), listSlot, value);
  }

  public static boolean instanceOf(Object target, Object classOrName) {
    if (target == null || classOrName == null)
      return false;

    if (classOrName instanceof Class) {
      return ((Class<?>) classOrName).isInstance(target);
    }

    if (classOrName instanceof String) {
      Class<?> cls = findClass((String) classOrName);
      return cls != null && cls.isInstance(target);
    }

    return false;
  }

  private static Object normalizeArg(Object arg) {
    if (arg == null)
      return null;
    if (arg instanceof SaynaaObject) {
      return ((SaynaaObject) arg).getObject();
    }
    if (arg instanceof SaynaaContext) {
      Context ctx = ((SaynaaContext) arg).getContext();
      return ctx != null ? ctx : arg;
    }
    return arg;
  }

  private static Object[] normalizeArgs(Object... args) {
    if (args == null || args.length == 0)
      return args;
    Object[] out = new Object[args.length];
    for (int i = 0; i < args.length; i++) {
      out[i] = normalizeArg(args[i]);
    }
    return out;
  }

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
  // Cache for class method arrays to reduce repeated reflection scans.
  private static final Map<Class<?>, Method[]> classMethodsCache = new HashMap<>();
  // Cache for fast-path method lookup by single-argument type (String/int/double/bool/void).
  private static final Map<String, Method> stringMethodCache = new HashMap<>();
  private static final Map<String, Method> integerMethodCache = new HashMap<>();
  private static final Map<String, Method> doubleMethodCache = new HashMap<>();
  private static final Map<String, Method> boolMethodCache = new HashMap<>();
  private static final Map<String, Method> voidMethodCache = new HashMap<>();
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

  private static Method[] getMethodsCached(Class<?> cls) {
    Method[] methods = classMethodsCache.get(cls);
    if (methods != null) {
      return methods;
    }
    methods = cls.getMethods();
    classMethodsCache.put(cls, methods);
    return methods;
  }

  private static final List<ClassLoader> extraClassLoaders = new ArrayList<>();

  public static synchronized void setExtraClassLoaders(List<ClassLoader> loaders) {
    extraClassLoaders.clear();
    if (loaders != null) {
      extraClassLoaders.addAll(loaders);
    }
  }

  public static synchronized void addExtraClassLoader(ClassLoader loader) {
    if (loader != null) {
      extraClassLoaders.add(loader);
    }
  }

  private static synchronized List<ClassLoader> snapshotExtraLoaders() {
    return new ArrayList<>(extraClassLoaders);
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
      // Fall through to custom loaders.
    }

    for (ClassLoader loader : snapshotExtraLoaders()) {
      if (loader == null)
        continue;
      try {
        cls = Class.forName(className, false, loader);
        if (cls != null) {
          classCache.put(className, cls);
          Log.d(TAG, "Found and cached class (loader): " + className);
          return cls;
        }
      } catch (ClassNotFoundException ignored) {
        // Try next loader.
      }
    }

    if (Log.isLoggable(TAG, Log.DEBUG)) {
      Log.d(TAG, "Class not found: " + className);
    }
    return null;
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

  private static int matchScore(Class<?> paramType, Class<?> argType) {
    if (argType == null) {
      return paramType.isPrimitive() ? -1 : 4;
    }

    if (paramType == argType)
      return 0;

    if (paramType.isPrimitive()) {
      Class<?> prim = toPrimitive(argType);
      if (paramType == prim)
        return 1;
      if (Number.class.isAssignableFrom(argType)
          && (paramType == int.class || paramType == long.class || paramType == short.class
              || paramType == byte.class || paramType == float.class || paramType == double.class)) {
        return 2;
      }
      return -1;
    }

    if (Number.class.isAssignableFrom(paramType) && Number.class.isAssignableFrom(argType))
      return 2;

    if (paramType.isAssignableFrom(argType))
      return 3;

    return -1;
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
    Object[] normalized = normalizeArgs(args);
    Class<?>[] argTypes = new Class<?>[normalized.length];
    for (int i = 0; i < args.length; i++) {
      argTypes[i] = normalized[i] == null ? null : normalized[i].getClass();
    }
    ConstructorKey key = new ConstructorKey(cls, argTypes);
    if (constructorCache.containsKey(key)) {
      return constructorCache.get(key);
    }

    Constructor<?> best = null;
    int bestScore = Integer.MAX_VALUE;
    for (Constructor<?> ctor : cls.getConstructors()) {
      Class<?>[] paramTypes = ctor.getParameterTypes();
      if (paramTypes.length != normalized.length)
        continue;

      boolean match = true;
      int score = 0;
      for (int i = 0; i < paramTypes.length; i++) {
        int s = matchScore(paramTypes[i], argTypes[i]);
        if (s < 0) {
          match = false;
          break;
        }
        score += s;
      }

      if (match && score < bestScore) {
        best = ctor;
        bestScore = score;
      }
    }
    if (best != null) {
      constructorCache.put(key, best);
      Log.d(TAG, "Cached constructor: " + best);
      return best;
    }
    Log.e(TAG, "No matching constructor found for " + cls.getName());
    return null;
  }

  private static Object createJavaObject(Class<?> cls, Object... args) {
    if (cls == null) {
      Log.e(TAG, "Failed to resolve class for creation.");
      return null;
    }

    Object[] normalized = normalizeArgs(args);
    Constructor<?> ctor = findConstructor(cls, normalized);
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
    Log.d(TAG, "Creating Java object: " + fullClassName);
    logArgsDebug("createJavaObject", args);
    Class<?> cls = findClass(fullClassName);
    if (cls == null) {
      Log.e(TAG, "Failed to find class: " + fullClassName);
      return null;
    }
    return createJavaObject(cls, args);
  }

  private static Object createJavaObjectFlexible(Object classOrName, Object... args) {
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

  private static Class<?> resolveClass(Object classOrName) {
    if (classOrName instanceof Class)
      return (Class<?>) classOrName;
    if (classOrName instanceof String)
      return findClass((String) classOrName);
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

  // --- Find matching method ---
  public static Method findMethod(Class<?> cls, String methodName, Object... args) {
    Object[] normalized = normalizeArgs(args);
    Class<?>[] argTypes = new Class<?>[normalized.length];
    for (int i = 0; i < normalized.length; i++) {
      argTypes[i] = normalized[i] == null ? null : normalized[i].getClass();
    }

    String cacheName = cls.getName() + "#" + methodName;
    if (normalized.length == 0) {
      Method cached = voidMethodCache.get(cacheName);
      if (cached != null) {
        return cached;
      }
    } else if (normalized.length == 1) {
      Object arg = normalized[0];
      if (arg instanceof String) {
        Method cached = stringMethodCache.get(cacheName);
        if (cached != null) {
          return cached;
        }
      } else if (arg instanceof Boolean) {
        Method cached = boolMethodCache.get(cacheName);
        if (cached != null) {
          return cached;
        }
      } else if (arg instanceof Number) {
        Method cached = (arg instanceof Integer || arg instanceof Long || arg instanceof Short || arg instanceof Byte)
                            ? integerMethodCache.get(cacheName)
                            : doubleMethodCache.get(cacheName);
        if (cached != null) {
          return cached;
        }
      }
    }

    MethodKey key = new MethodKey(cls, methodName, argTypes);
    if (methodCache.containsKey(key)) {
      return methodCache.get(key);
    }
    if (missingMethodCache.containsKey(key)) {
      return null;
    }

    Method bestMatch = null;
    int bestScore = Integer.MAX_VALUE;
    for (Method method : getMethodsCached(cls)) {
      if (!method.getName().equals(methodName))
        continue;

      Class<?>[] paramTypes = method.getParameterTypes();
      boolean isVarArgs = method.isVarArgs();
      if (!isVarArgs && paramTypes.length != normalized.length)
        continue;

      boolean match = true;
      int score = 0;

      if (!isVarArgs) {
        for (int i = 0; i < paramTypes.length; i++) {
          int s = matchScore(paramTypes[i], argTypes[i]);
          if (s < 0) {
            match = false;
            break;
          }
          score += s;
        }
      } else {
        int fixedCount = paramTypes.length - 1;
        if (normalized.length < fixedCount) {
          match = false;
        } else {
          for (int i = 0; i < fixedCount; i++) {
            int s = matchScore(paramTypes[i], argTypes[i]);
            if (s < 0) {
              match = false;
              break;
            }
            score += s;
          }
          if (match) {
            Class<?> varType = paramTypes[fixedCount].getComponentType();
            for (int i = fixedCount; i < normalized.length; i++) {
              int s = matchScore(varType, argTypes[i]);
              if (s < 0) {
                match = false;
                break;
              }
              score += s;
            }
          }
        }
      }

      if (match && score < bestScore) {
        bestMatch = method;
        bestScore = score;
      }
    }

    if (bestMatch != null) {
      methodCache.put(key, bestMatch);
      Log.d(TAG, "Cached method: " + bestMatch);

      Class<?>[] params = bestMatch.getParameterTypes();
      if (params.length == 0) {
        voidMethodCache.put(cacheName, bestMatch);
      } else if (params.length == 1) {
        Class<?> p0 = params[0];
        if (p0 == String.class || CharSequence.class.isAssignableFrom(p0)) {
          stringMethodCache.put(cacheName, bestMatch);
        } else if (p0 == boolean.class || p0 == Boolean.class) {
          boolMethodCache.put(cacheName, bestMatch);
        } else if (p0 == int.class || p0 == Integer.class || p0 == long.class || p0 == Long.class
                   || p0 == short.class || p0 == Short.class || p0 == byte.class || p0 == Byte.class) {
          integerMethodCache.put(cacheName, bestMatch);
        } else if (p0 == float.class || p0 == Float.class || p0 == double.class
                   || p0 == Double.class || Number.class.isAssignableFrom(p0)) {
          doubleMethodCache.put(cacheName, bestMatch);
        }
      }
    } else {
      missingMethodCache.put(key, Boolean.TRUE);
    }
    return bestMatch;
  }

  public static String resolveCallbackInterface(Object target, String methodName, int argc, int argIndex) {
    if (target == null || methodName == null || methodName.trim().isEmpty())
      return null;
    if (argc < 0 || argIndex < 0)
      return null;

    Class<?> cls = (target instanceof Class) ? (Class<?>) target : target.getClass();
    for (Method method : getMethodsCached(cls)) {
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

  private static Object[] buildVarArgs(Class<?>[] paramTypes, Object[] normalized) {
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
  public static Object callJavaMethod(Object javaObject, String methodName, Object... args) {
    Object target = normalizeArg(javaObject);
    if (target == null) {
      Log.e(TAG, "Java object is null.");
      return null;
    }

    Object[] normalized = normalizeArgs(args);
    logArgsDebug("callJavaMethod " + methodName, normalized);

    Class<?> cls = target.getClass();
    Method method = findMethod(cls, methodName, normalized);
    if (method == null)
      return null;

    try {
      Object[] coercedArgs = method.isVarArgs() ? buildVarArgs(method.getParameterTypes(), normalized)
                                                : coerceArgs(method.getParameterTypes(), normalized);
      Object ret = method.invoke(target, coercedArgs);
      return normalizeReturn(ret);
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

    Object[] normalized = normalizeArgs(args);
    logArgsDebug("callStaticJavaMethod " + className + "." + methodName, normalized);
    Method method = findMethod(cls, methodName, normalized);
    if (method == null)
      return null;

    try {
      Object[] coercedArgs = method.isVarArgs() ? buildVarArgs(method.getParameterTypes(), normalized)
                                                : coerceArgs(method.getParameterTypes(), normalized);
      Object ret = method.invoke(null, coercedArgs);
      return normalizeReturn(ret);
    } catch (IllegalAccessException | InvocationTargetException | IllegalArgumentException e) {
      Log.e(TAG, "Error invoking static method: " + methodName, e);
      return null;
    }
  }

  public static boolean callFromSlots(
      Saynaa saynaa, int targetSlot, int methodNameSlot, int argsStart, int argc, int outSlot) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    String methodName = saynaa.getSlotString(methodNameSlot);
    Object target = slotToJava(saynaa, targetSlot);
    Object[] args = argsFromSlots(saynaa, argsStart, argc);
    Object ret = callJavaMethod(target, methodName, args);
    return pushToSlot(saynaa, outSlot, ret);
  }

  public static boolean callFromSlots(
      SaynaaState state, int targetSlot, int methodNameSlot, int argsStart, int argc, int outSlot) {
    return callFromSlots(unwrapState(state), targetSlot, methodNameSlot, argsStart, argc, outSlot);
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

  public static boolean createFromSlots(SaynaaState state, int classSlot, int valueSlot, int argc, int outSlot) {
    return createFromSlots(unwrapState(state), classSlot, valueSlot, argc, outSlot);
  }

  public static String resolveInterfaceNameFromSlots(Saynaa saynaa, int interfaceSlot) {
    if (saynaa == null || saynaa.isClosed())
      return null;
    Object interfaceOrName = slotToJava(saynaa, interfaceSlot);
    return resolveInterfaceName(interfaceOrName);
  }

  public static String resolveInterfaceNameFromSlots(SaynaaState state, int interfaceSlot) {
    return resolveInterfaceNameFromSlots(unwrapState(state), interfaceSlot);
  }

  public static boolean createProxyFromSlots(Saynaa saynaa, int interfaceSlot, int methodNameSlot,
      int callbackSlot, int argc, int outSlot) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    String interfaceName = resolveInterfaceNameFromSlots(saynaa, interfaceSlot);
    if (interfaceName == null || interfaceName.trim().isEmpty())
      return false;

    String methodName = "*";
    if (argc >= 3) {
      methodName = saynaa.getSlotString(methodNameSlot);
    } else {
      methodName = getDefaultInterfaceMethodName(interfaceName);
    }

    String script = saynaa.getSlotString(callbackSlot);
    Object proxy = createProxy(saynaa, interfaceName, methodName, script);
    return pushToSlot(saynaa, outSlot, proxy);
  }

  public static boolean createProxyFromSlots(SaynaaState state, int interfaceSlot,
      int methodNameSlot, int callbackSlot, int argc, int outSlot) {
    return createProxyFromSlots(unwrapState(state), interfaceSlot, methodNameSlot, callbackSlot, argc, outSlot);
  }

  public static boolean newFromSlots(Saynaa saynaa, int classSlot, int argsStart, int argc, int outSlot) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    Object classOrName = slotToJava(saynaa, classSlot);
    Object[] args = argsFromSlots(saynaa, argsStart, argc);
    Object ret = createJavaObjectFlexible(classOrName, args);
    return pushToSlot(saynaa, outSlot, ret);
  }

  public static boolean newFromSlots(SaynaaState state, int classSlot, int argsStart, int argc, int outSlot) {
    return newFromSlots(unwrapState(state), classSlot, argsStart, argc, outSlot);
  }

  public static boolean callStaticFromSlots(
      Saynaa saynaa, int classNameSlot, int methodNameSlot, int argsStart, int argc, int outSlot) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    String className = saynaa.getSlotString(classNameSlot);
    String methodName = saynaa.getSlotString(methodNameSlot);
    Object[] args = argsFromSlots(saynaa, argsStart, argc);
    Object ret = callStaticJavaMethod(className, methodName, args);
    return pushToSlot(saynaa, outSlot, ret);
  }

  public static boolean callStaticFromSlots(SaynaaState state, int classNameSlot,
      int methodNameSlot, int argsStart, int argc, int outSlot) {
    return callStaticFromSlots(unwrapState(state), classNameSlot, methodNameSlot, argsStart, argc, outSlot);
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

  private static void logArgsDebug(String prefix, Object[] args) {
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

  private static Object normalizeReturn(Object value) {
    if (value == null)
      return null;
    if (value instanceof SaynaaObject)
      return ((SaynaaObject) value).getObject();
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

  public static boolean pushToSlot(Saynaa saynaa, int slot, Object value) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    Object normalized = normalizeReturn(value);
    saynaa.reserveSlots(slot + 3);

    if (normalized == null) {
      saynaa.setSlotNull(slot);
      return true;
    }

    if (normalized instanceof Boolean) {
      saynaa.setSlotBool(slot, (Boolean) normalized);
      return true;
    }

    if (normalized instanceof Number) {
      saynaa.setSlotNumber(slot, ((Number) normalized).doubleValue());
      return true;
    }

    if (normalized instanceof CharSequence) {
      saynaa.setSlotString(slot, normalized.toString());
      return true;
    }

    return saynaa.wrapJavaObject(slot, normalized);
  }

  public static boolean pushToSlot(SaynaaState state, int slot, Object value) {
    return pushToSlot(unwrapState(state), slot, value);
  }

  public static boolean pushToSlotAsSaynaa(Saynaa saynaa, int slot, Object value) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    Object normalized = normalizeReturn(value);
    saynaa.reserveSlots(slot + 3);

    if (normalized == null) {
      saynaa.setSlotNull(slot);
      return true;
    }

    if (normalized instanceof Boolean) {
      saynaa.setSlotBool(slot, (Boolean) normalized);
      return true;
    }

    if (normalized instanceof Number) {
      saynaa.setSlotNumber(slot, ((Number) normalized).doubleValue());
      return true;
    }

    if (normalized instanceof CharSequence) {
      saynaa.setSlotString(slot, normalized.toString());
      return true;
    }

    if (normalized instanceof Map) {
      saynaa.newMap(slot);
      int keySlot = slot + 1;
      int valueSlot = slot + 2;
      for (Map.Entry<?, ?> entry : ((Map<?, ?>) normalized).entrySet()) {
        if (!pushToSlotAsSaynaa(saynaa, keySlot, entry.getKey()))
          return false;
        if (!pushToSlotAsSaynaa(saynaa, valueSlot, entry.getValue()))
          return false;
        if (!saynaa.mapSet(slot, keySlot, valueSlot))
          return false;
      }
      return true;
    }

    if (normalized instanceof Iterable) {
      saynaa.newList(slot);
      int elemSlot = slot + 1;
      for (Object elem : (Iterable<?>) normalized) {
        if (!pushToSlotAsSaynaa(saynaa, elemSlot, elem))
          return false;
        if (!saynaa.listInsert(slot, -1, elemSlot))
          return false;
      }
      return true;
    }

    Class<?> cls = normalized.getClass();
    if (cls.isArray()) {
      saynaa.newList(slot);
      int elemSlot = slot + 1;
      int len = Array.getLength(normalized);
      for (int i = 0; i < len; i++) {
        Object elem = Array.get(normalized, i);
        if (!pushToSlotAsSaynaa(saynaa, elemSlot, elem))
          return false;
        if (!saynaa.listInsert(slot, -1, elemSlot))
          return false;
      }
      return true;
    }

    return saynaa.wrapJavaObject(slot, normalized);
  }

  public static boolean pushToSlotAsSaynaa(SaynaaState state, int slot, Object value) {
    return pushToSlotAsSaynaa(unwrapState(state), slot, value);
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
      return normalizeReturn(field.get(isStaticAccess ? null : objOrClass));
    } catch (IllegalAccessException e) {
      Log.e(TAG, "Error accessing field: " + fieldName, e);
      return null;
    }
  }

  public static boolean getFieldFromSlots(Saynaa saynaa, int targetSlot, int fieldNameSlot, int outSlot) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    String fieldName = saynaa.getSlotString(fieldNameSlot);
    Object target = slotToJava(saynaa, targetSlot);
    Object ret = getFieldValue(target, fieldName);
    return pushToSlot(saynaa, outSlot, ret);
  }

  public static boolean getFieldFromSlots(SaynaaState state, int targetSlot, int fieldNameSlot, int outSlot) {
    return getFieldFromSlots(unwrapState(state), targetSlot, fieldNameSlot, outSlot);
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

  public static boolean setFieldFromSlots(
      Saynaa saynaa, int targetSlot, int fieldNameSlot, int valueSlot, int outSlot) {
    if (saynaa == null || saynaa.isClosed())
      return false;

    String fieldName = saynaa.getSlotString(fieldNameSlot);
    Object target = slotToJava(saynaa, targetSlot);
    Object value = slotToJava(saynaa, valueSlot);
    boolean ok = setFieldValue(target, fieldName, value);
    saynaa.reserveSlots(outSlot + 1);
    saynaa.setSlotBool(outSlot, ok);
    return true;
  }

  public static boolean setFieldFromSlots(
      SaynaaState state, int targetSlot, int fieldNameSlot, int valueSlot, int outSlot) {
    return setFieldFromSlots(unwrapState(state), targetSlot, fieldNameSlot, valueSlot, outSlot);
  }

  public static Object createProxy(Saynaa saynaa, String interfaceName, String methodName, String script) {
    return SaynaaProxyFactory.createProxy(saynaa, interfaceName, methodName, script);
  }

  public static Object createProxy(SaynaaState state, String interfaceName, String methodName, String script) {
    return createProxy(unwrapState(state), interfaceName, methodName, script);
  }

  private static Object defaultReturnFor(Class<?> returnType) {
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

  private static Object[] normalizeCallbackArgs(Method method, Object[] args) {
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

  private static Object invokeCallbackFromJava(
      Saynaa saynaa, int callbackId, String methodName, Method method, Object[] args) {
    if (saynaa == null || saynaa.isClosed() || callbackId <= 0)
      return null;

    Object[] safeArgs = normalizeCallbackArgs(method, args);
    int argc = safeArgs == null ? 0 : safeArgs.length;
    int argStart = 2;
    saynaa.reserveSlots(argStart + Math.max(argc, 0) + 4);

    for (int i = 0; i < argc; i++) {
      int slot = argStart + i;
      if (!pushToSlot(saynaa, slot, safeArgs[i]))
        return null;
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

    Object normalized = normalizeReturn(callbackResult);
    if (normalized == null)
      return defaultReturnFor(returnType);

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
        return text.isEmpty() ? defaultReturnFor(returnType) : text.charAt(0);
      }
      return defaultReturnFor(returnType);
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

    return defaultReturnFor(returnType);
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
        Class<?> iface = findClass(n);
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
              return defaultReturnFor(rt);
            }
          }

          Class<?> rt = method.getReturnType();
          return defaultReturnFor(rt);
        }
      };

      return Proxy.newProxyInstance(loader, ifaces, handler);
    } catch (Throwable t) {
      Log.e(TAG, "createNativeCallbackProxy failed: " + interfaceName + "." + methodName, t);
      return null;
    }
  }

  public static Object createNativeCallbackProxy(final SaynaaState state,
      final String interfaceName, final String methodName, final int callbackId) {
    return createNativeCallbackProxy(unwrapState(state), interfaceName, methodName, callbackId);
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

      Class<?> iface = findClass(names[0].trim());
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
