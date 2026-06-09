package com.android.saynaa.saynaajava;

public final class JavaMethodBinding {
  private final Object target;
  private final String methodName;

  public JavaMethodBinding(Object target, String methodName) {
    this.target = target;
    this.methodName = methodName;
  }

  public Object getTarget() {
    return target;
  }

  public String getMethodName() {
    return methodName;
  }

  public boolean isStatic() {
    return target instanceof Class<?>;
  }
}