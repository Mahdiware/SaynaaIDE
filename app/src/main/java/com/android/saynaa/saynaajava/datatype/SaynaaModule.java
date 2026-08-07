package com.android.saynaa.saynaajava.datatype;

import android.content.*;
import com.android.saynaa.saynaajava.*;
import java.util.*;

public final class SaynaaModule extends SaynaaObject {
  public SaynaaModule(Saynaa saynaa, int slot) {
    super(saynaa, slot);
  }

  public SaynaaModule(Saynaa saynaa, int slot, int handleId) {
    super(saynaa, slot, handleId);
  }

  public int getSlot() {
    return slot;
  }
}