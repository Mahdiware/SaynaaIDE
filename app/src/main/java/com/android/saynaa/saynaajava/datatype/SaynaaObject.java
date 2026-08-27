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

  public Object getme(String attrName) {
    return getme(attrName, true);
  }

  public void setTag(Object value) {
    this.tag = value;
  }

  public Object getTag() {
    return this.tag;
  }

  public Object getme(String attrName, boolean skipGetter) {
    return saynaa.objGetattr(handleId, attrName, skipGetter);
  }
}
