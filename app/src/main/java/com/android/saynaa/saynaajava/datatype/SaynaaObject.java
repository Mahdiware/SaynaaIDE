package com.android.saynaa.saynaajava.datatype;

import com.android.saynaa.saynaajava.*;

public class SaynaaObject {
  // private final Object value;

  // FIXME: use SaynaaState instead of Saynaa
  protected final Saynaa saynaa;
  protected final int slot;

  public SaynaaObject(Saynaa saynaa, int slot) {
    this.saynaa = saynaa;
    this.slot = slot;
  }

  public int getSlot() {
    return slot;
  }
}
