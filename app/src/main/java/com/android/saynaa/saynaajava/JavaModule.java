package com.android.saynaa.saynaajava;

import android.util.Log;

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
      state.moduleSetGlobal(module, "bindClass", JavaBridge.class, "findClass");
      state.moduleSetGlobal(module, "new", this, "newJavaObject");
      state.moduleSetGlobal(module, "getField", JavaBridge.class, "getFieldValue");
      state.moduleSetGlobal(module, "setField", JavaBridge.class, "setFieldValue");
      state.moduleSetGlobal(module, "tostring", JavaBridge.class, "javaToString");
      state.moduleSetGlobal(module, "instanceof", JavaBridge.class, "instanceOf");
      state.moduleSetGlobal(module, "callStatic", JavaBridge.class, "callStaticJavaMethod");
      state.moduleSetGlobal(module, "length", JavaBridge.class, "lengthOf");
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

  public boolean testing(Object... args) {
    for (Object arg : args) {
      Log.d(TAG, "testing arg: " + arg);
    }
    return true;
  }

  // Target                   Meaning           Dispatch
  // String (class name)	    static call	      Class.method
  // Class<?>	                static call	      Class.method
  // Object instance	        instance call	    obj.method
}