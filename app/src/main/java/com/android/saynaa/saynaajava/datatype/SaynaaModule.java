package com.android.saynaa.saynaajava.datatype;

import android.content.*;
import com.android.saynaa.saynaajava.*;
import java.util.*;

public final class SaynaaModule extends SaynaaObject {
  public SaynaaModule(Saynaa saynaa, int type, int handleId) {
    super(saynaa, type, handleId);
  }

  public boolean setGlobal(String name, Object value) {
    return saynaa.moduleSetGlobal(handleId, name, value);
  }

  public boolean setGlobal(String name, Object clazz, String methodName) {
    return saynaa.moduleSetGlobal(handleId, name, new JavaMethodBinding(clazz, methodName));
  }

  public boolean register() {
    return saynaa.registerModule(handleId);
  }
}