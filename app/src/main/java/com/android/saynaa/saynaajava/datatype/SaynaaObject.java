package com.android.saynaa.saynaajava.datatype;

import com.android.saynaa.saynaajava.*;

public class SaynaaObject {
  protected final Saynaa saynaa;
  protected final int handleId;
  protected final int type;
  protected Object tag;

  public SaynaaObject(Saynaa saynaa, int type, int handleId) {
    this.saynaa = saynaa;
    this.type = type;
    this.handleId = handleId;
  }

  public int getHandleId() {
    return handleId;
  }

  public String toString() {
    Object result = saynaa.callGlobalFunction("str", this);
    if(result instanceof String)
      return (String)result;
    
    return String.valueOf(result);
  }

  public Object getattr(String attrName) {
    return getattr(attrName, true);
  }


  public Object getattr(String attrName, boolean skipGetter) {
    return saynaa.objGetattr(handleId, attrName, skipGetter);
  }
}
