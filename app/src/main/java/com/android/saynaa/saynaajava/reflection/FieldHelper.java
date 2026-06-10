package com.android.saynaa.saynaajava.reflection;

import android.util.Log;
import com.android.saynaa.saynaajava.JavaBridge;
import com.android.saynaa.saynaajava.JavaFunction;
import com.android.saynaa.saynaajava.JavaModule;
import com.android.saynaa.saynaajava.SaynaaContext;
import com.android.saynaa.saynaajava.SaynaaState;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.concurrent.ConcurrentHashMap;

public class FieldHelper {
  private static final String TAG = "FieldHelper";

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

    Field field = ReflectionFinder.findFieldQuietly(cls, fieldName);
    if (field == null)
      return false;

    try {
      field.set(isStaticAccess ? null : objOrClass,
          ReflectionNormalizer.normalizeArg(coerceFieldValue(field.getType(), value)));
      return true;
    } catch (IllegalAccessException | IllegalArgumentException e) {
      Log.e(TAG, "Error setting field: " + fieldName, e);
      return false;
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

    Field field = ReflectionFinder.findFieldQuietly(cls, fieldName);
    if (field == null)
      return null;

    try {
      return ReflectionNormalizer.normalizeReturn(field.get(isStaticAccess ? null : objOrClass));
    } catch (IllegalAccessException e) {
      Log.e(TAG, "Error accessing field: " + fieldName, e);
      return null;
    }
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
}