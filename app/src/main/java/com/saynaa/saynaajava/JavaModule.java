package com.saynaa.saynaajava;

import android.content.Context;
import android.util.Log;
import com.saynaa.saynaajava.datatype.*;
import com.saynaa.saynaajava.reflection.*;
import java.io.File;
import java.lang.reflect.Array;
import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.function.Function;

public class JavaModule {
  protected final Saynaa saynaa;
  protected String module_name = "java";
  protected String TAG = "JavaModule";

  public JavaModule(Saynaa saynaa) {
    this.saynaa = saynaa;
  }

  public boolean create() {
    try {
      SaynaaModule module = saynaa.newModule(module_name);
      module.setGlobal("context", saynaa.getContext());
      module.setGlobal("saynaadir", saynaa.getSaynaaDir().getAbsolutePath());
      module.setGlobal("mainModule", saynaa.getMainModule());
      module.setGlobal("application", saynaa.getContext().getApplicationContext());
      module.setGlobal("bindClass", ReflectionFinder.class, "findClass");
      module.setGlobal("new", this, "newJavaObject");
      module.setGlobal("getField", FieldHelper.class, "getFieldValue");
      module.setGlobal("setField", FieldHelper.class, "setFieldValue");
      module.setGlobal("tostring", this, "javaToString");
      module.setGlobal("call", MethodHelper.class, "call");
      module.setGlobal("instanceof", this, "instanceOf");
      module.setGlobal("length", this, "lengthOf");
      module.setGlobal("loadDex", this, "loadDex");
      module.setGlobal("getClassName", this, "getClassName");
      module.setGlobal("getPackageName", this, "getPackageName");
      module.setGlobal("getSimpleClassName", this, "getSimpleClassName");
      module.setGlobal("setDebugMode", this, "setDebugMode");
      module.register();
    } catch (Exception e) {
      return false;
    }
    return true;
  }

  public void setDebugMode(boolean mode) {
    saynaa.setDebugMode(mode);
  }

  public void loadDex(String path) throws SaynaaException {
    ReflectionFinder.setExtraClassLoaders(saynaa.getDexLoader().loadDex(path));
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

  public String getClassName(Object obj) {
    if (obj instanceof Class) {
      return ((Class<?>) obj).getName();
    }
    if (obj == null)
      return "Null";
    return obj.getClass().getName();
  }

  public String getPackageName(Object obj) {
    if (obj instanceof Class) {
      Package pkg = ((Class<?>) obj).getPackage();
      return pkg != null ? pkg.getName() : "";
    }
    Package pkg = obj.getClass().getPackage();
    return pkg != null ? pkg.getName() : "";
  }

  public String getSimpleClassName(Object obj) {
    if (obj instanceof Class) {
      return ((Class<?>) obj).getSimpleName();
    }
    return obj.getClass().getSimpleName();
  }

  public String getCanonicalClassName(Object obj) {
    if (obj instanceof Class) {
      return ((Class<?>) obj).getCanonicalName();
    }
    return obj.getClass().getCanonicalName();
  }
}