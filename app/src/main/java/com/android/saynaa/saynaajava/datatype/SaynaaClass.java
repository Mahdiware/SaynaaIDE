package com.android.saynaa.saynaajava.datatype;

import android.content.*;
import android.util.Log;
import com.android.saynaa.saynaajava.*;
import java.util.*;

public final class SaynaaClass extends SaynaaObject {
  private static final String TAG = "SaynaaClass";

  public SaynaaClass(Saynaa saynaa, int type, int handleId) {
    super(saynaa, type, handleId);
  }

  public SaynaaInstance newInstance(Object... args) {
    int retSlot = saynaa.nextSlot();
    int argCount = args == null ? 0 : args.length;
    int argStart = saynaa.allocSlot(argCount + 4);
    for (int i = 0; i < argCount; i++) {
      if (!JavaBridge.pushToSlot(saynaa, argStart + i, args[i])) {
        saynaa.freeSlot(argStart, argCount + 4);
        Log.e(TAG, "Failed to push argument to slot: " + i);
        return null;
      }
    }

    boolean ok = saynaa.newInstance(handleId, argStart, argCount, retSlot);
    saynaa.freeSlot(argStart, argCount + 4);
    if (!ok) {
      Log.e(TAG, "Failed to create new instance: handleId=" + handleId + ", args=" + Arrays.toString(args));
      return null;
    }
    Object ret = JavaBridge.slotToJava(saynaa, retSlot);
    if (ret instanceof SaynaaInstance) {
      return (SaynaaInstance) ret;
    } else {
      Log.e(TAG, "Failed to create new instance, returned object is not a SaynaaInstance its "
                     + (ret == null ? "null" : ret.getClass().getName()));
      return null;
    }
  }
}