package com.android.saynaa.saynaajava;

public class SaynaaObject {
  private final Object value;

  public SaynaaObject(Object value) {
    this.value = value;
  }

  public Object getObject() {
    return value;
  }

  public Object call(String methodName, Object... args) {
    return JavaBridge.callJavaMethod(value, methodName, args);
  }

  public Object getField(String fieldName) {
    return JavaBridge.getFieldValue(value, fieldName);
  }

  public boolean setField(String fieldName, Object fieldValue) {
    return JavaBridge.setFieldValue(value, fieldName, fieldValue);
  }
}
