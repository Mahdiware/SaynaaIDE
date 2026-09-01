package com.saynaa.util;

import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.List;

public class ArrayBuilder {
  private final List<Object> values;

  public ArrayBuilder() {
    values = new ArrayList<>();
  }

  public ArrayBuilder(int initialCapacity) {
    values = new ArrayList<>(initialCapacity);
  }

  /**
   * Add a value to the array.
   */
  public ArrayBuilder add(Object value) {
    values.add(value);
    return this;
  }

  /**
   * Add multiple values.
   */
  public ArrayBuilder addAll(Object... values) {
    if (values != null) {
      for (Object value : values) {
        this.values.add(value);
      }
    }
    return this;
  }

  /**
   * Get a value.
   */
  public Object get(int index) {
    return values.get(index);
  }

  /**
   * Replace a value.
   */
  public ArrayBuilder set(int index, Object value) {
    values.set(index, value);
    return this;
  }

  /**
   * Remove a value.
   */
  public Object remove(int index) {
    return values.remove(index);
  }

  /**
   * Number of stored values.
   */
  public int size() {
    return values.size();
  }

  /**
   * Check whether the array is empty.
   */
  public boolean isEmpty() {
    return values.isEmpty();
  }

  /**
   * Remove all values.
   */
  public void clear() {
    values.clear();
  }

  /**
   * Convert to Object[].
   */
  public Object[] toObjectArray() {
    return values.toArray();
  }

  /**
   * Create a Java array using a datatype name.
   *
   * Examples:
   *
   *     toArray("java.lang.String")
   *     toArray("java.lang.Integer")
   *     toArray("int")
   *     toArray("long")
   *     toArray("boolean")
   *     toArray("double")
   *
   * Returns:
   *
   *     String[]
   *     Integer[]
   *     int[]
   *     long[]
   *     boolean[]
   *     double[]
   */
  public Object toArray(String datatype) {
    if (datatype == null || datatype.trim().isEmpty()) {
      throw new IllegalArgumentException("Datatype cannot be null or empty");
    }

    Class<?> componentType = resolveType(datatype.trim());

    Object array = Array.newInstance(componentType, values.size());

    for (int i = 0; i < values.size(); i++) {
      try {
        Array.set(array, i, values.get(i));
      } catch (IllegalArgumentException e) {
        throw new IllegalArgumentException("Cannot store value at index " + i + " ("
                                               + getTypeName(values.get(i)) + ")"
                                               + " in " + datatype + "[]",
            e);
      }
    }

    return array;
  }

  /**
   * Resolve a Java datatype name.
   */
  private static Class<?> resolveType(String datatype) {
    // Primitive types
    switch (datatype) {
    case "byte":
      return byte.class;

    case "short":
      return short.class;

    case "int":
      return int.class;

    case "long":
      return long.class;

    case "float":
      return float.class;

    case "double":
      return double.class;

    case "char":
      return char.class;

    case "boolean":
      return boolean.class;

    case "void":
      return void.class;
    }

    // Common Java aliases
    switch (datatype) {
    case "String":
      return String.class;

    case "Integer":
      return Integer.class;

    case "Long":
      return Long.class;

    case "Boolean":
      return Boolean.class;

    case "Double":
      return Double.class;

    case "Float":
      return Float.class;

    case "Short":
      return Short.class;

    case "Byte":
      return Byte.class;

    case "Character":
      return Character.class;

    case "Object":
      return Object.class;
    }

    // Fully-qualified Java class
    try {
      return Class.forName(datatype);
    } catch (ClassNotFoundException e) {
      throw new IllegalArgumentException("Unknown Java datatype: " + datatype, e);
    }
  }

  private static String getTypeName(Object value) {
    if (value == null) {
      return "null";
    }

    return value.getClass().getName();
  }
}
