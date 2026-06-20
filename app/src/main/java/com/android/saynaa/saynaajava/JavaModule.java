package com.android.saynaa.saynaajava;

import android.util.Log;
import com.android.saynaa.saynaajava.datatype.*;
import com.android.saynaa.saynaajava.reflection.*;
import java.lang.reflect.Array;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Function;
import com.android.saynaa.saynaajava.tests.EcjTest;

public class JavaModule {
  protected final SaynaaState state;
  protected String module_name = "java";
  protected String TAG = "JavaModule";

  public JavaModule(SaynaaState state) {
    this.state = state;
  }

  public boolean create() {
    try {
      SaynaaModule module = state.newModule(module_name);
      state.moduleSetGlobal(module, "context", state.getContext());
      state.moduleSetGlobal(module, "EjsTest", this, "EcjCompile");
      state.moduleSetGlobal(module, "saynaadir", state.getSaynaaDir());
      state.moduleSetGlobal(module, "bindClass", ReflectionFinder.class, "findClass");
      state.moduleSetGlobal(module, "new", this, "newJavaObject");
      state.moduleSetGlobal(module, "getField", FieldHelper.class, "getFieldValue");
      state.moduleSetGlobal(module, "setField", FieldHelper.class, "setFieldValue");
      state.moduleSetGlobal(module, "tostring", this, "javaToString");
      state.moduleSetGlobal(module, "call", MethodHelper.class, "call");
      state.moduleSetGlobal(module, "instanceof", this, "instanceOf");
      state.moduleSetGlobal(module, "length", this, "lengthOf");
      state.moduleSetGlobal(module, "testing", this, "testing");
      state.registerModule(module);
    } catch (Exception e) {
      return false;
    }
    return true;
  }

  public void EcjCompile() {
    EcjTest.compileHello(state.getSaynaaDir());
  }

  public Object newJavaObject(Object classOrName, Object... args) {
    try {
      Object result = JavaBridge.createJavaObjectFlexible(classOrName, args);
      return result;
    } catch (Exception e) {
      e.printStackTrace();
      throw new RuntimeException(e);
    }
  }

  public boolean instanceOf(Object target, Object classOrName) {
    if (target == null || classOrName == null)
      return false;

    if (classOrName instanceof Class) {
      return ((Class<?>) classOrName).isInstance(target);
    }

    if (classOrName instanceof String) {
      Class<?> cls = ReflectionFinder.findClass((String) classOrName);
      return cls != null && cls.isInstance(target);
    }

    return false;
  }

  private final Map<Class<?>, Function<Object, String>> stringConverters = new HashMap<>();

  {
    stringConverters.put(byte[].class, obj -> new String((byte[]) obj));
    stringConverters.put(char[].class, obj -> new String((char[]) obj));
    stringConverters.put(int[].class, obj -> {
      int[] arr = (int[]) obj;
      StringBuilder sb = new StringBuilder();
      sb.append("[");
      for (int i = 0; i < arr.length; i++) {
        sb.append(arr[i]);
        if (i < arr.length - 1) {
          sb.append(", ");
        }
      }
      sb.append("]");
      return sb.toString();
    });
  }

  public String javaToString(Object value) {
    if (value == null) {
      return new String("null");
    }

    Function<Object, String> converter = stringConverters.get(value.getClass());

    if (converter != null) {
      return new String(converter.apply(value));
    }

    return new String(String.valueOf(value));
  }

  public double lengthOf(Object value) {
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

  public byte[] testing() {
    return new byte[] {'H', 'e', 'l', 'l', 'o'};
  }
}