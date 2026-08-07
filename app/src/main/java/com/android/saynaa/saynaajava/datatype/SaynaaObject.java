package com.android.saynaa.saynaajava.datatype;

import com.android.saynaa.saynaajava.*;

public class SaynaaObject {
  // private final Object value;

  protected final Saynaa saynaa;
  protected final int slot;
  protected final int handleId;

  public SaynaaObject(Saynaa saynaa, int slot) {
    this(saynaa, slot, 0);
  }

  public SaynaaObject(Saynaa saynaa, int slot, int handleId) {
    this.saynaa = saynaa;
    this.slot = slot;
    this.handleId = handleId;
  }

  public int getSlot() {
    return slot;
  }

  public int getHandleId() {
    return handleId;
  }
}
