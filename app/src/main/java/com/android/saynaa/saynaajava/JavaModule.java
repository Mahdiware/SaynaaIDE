package com.android.saynaa.saynaajava;

import android.util.Log;
import com.android.saynaa.saynaajava.reflection.FieldHelper;
import com.android.saynaa.saynaajava.reflection.MethodHelper;
import com.android.saynaa.saynaajava.reflection.ReflectionFinder;
import com.android.saynaa.saynaajava.reflection.ReflectionNormalizer;
import java.lang.reflect.Array;
import java.util.Collection;
import java.util.Map;

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

  public String javaToString(Object value) {
    return value == null ? "null" : String.valueOf(value);
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

  public boolean testing(Object... args) {
    for (Object arg : args) {
      Log.d(TAG, "testing arg: " + arg);
    }
    return true;
  }
}