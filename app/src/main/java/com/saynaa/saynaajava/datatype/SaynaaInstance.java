package com.saynaa.saynaajava.datatype;

import android.content.*;
import android.util.Log;
import com.saynaa.saynaajava.*;
import java.util.*;

public final class SaynaaInstance extends SaynaaObject {
  private static final String TAG = "SaynaaInstance";

  public SaynaaInstance(Saynaa saynaa, int type, int handleId) {
    super(saynaa, type, handleId);
  }

  public Object call(Object methodName, Object... args) {
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

    boolean ok = saynaa.callMethod(handleId, methodName.toString(), argStart, argCount, retSlot);
    saynaa.freeSlot(argStart, argCount + 4);
    if (!ok) {
      Log.e(TAG, "Failed to call method: " + methodName + ", handleId=" + handleId
                     + ", args=" + Arrays.toString(args));
      return null;
    }
    Object ret = JavaBridge.slotToJava(saynaa, retSlot);
    return ret;
  }

  public String testing() {
    return "testing";
  }
}