package com.android.saynaa.saynaajava.reflection;

import com.android.saynaa.saynaajava.JavaBridge;
import com.android.saynaa.saynaajava.JavaFunction;
import com.android.saynaa.saynaajava.JavaModule;
import com.android.saynaa.saynaajava.SaynaaContext;
import com.android.saynaa.saynaajava.SaynaaState;

public class ReflectionKeys {
  // --- Utility classes for cache keys ---
  public static final class MethodKey {
    private final Class<?> cls;
    private final String methodName;
    private final Class<?>[] paramTypes;

    public MethodKey(Class<?> cls, String methodName, Class<?>[] paramTypes) {
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

  public static final class ConstructorKey {
    private final Class<?> cls;
    private final Class<?>[] paramTypes;

    public ConstructorKey(Class<?> cls, Class<?>[] paramTypes) {
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

  public static final class FieldKey {
    private final Class<?> cls;
    private final String fieldName;

    public FieldKey(Class<?> cls, String fieldName) {
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

  private static int classHash(Class<?> c) {
    return c == null ? 0 : c.hashCode();
  }

  private static boolean classesEqual(Class<?> a, Class<?> b) {
    return a == b || (a != null && a.equals(b));
  }
}