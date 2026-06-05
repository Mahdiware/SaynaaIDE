package com.android.saynaa.saynaajava;

import android.content.Context;
import android.util.Log;
import android.view.View;
import com.android.saynaa.activity.SaynaaActivity;
import com.android.saynaa.saynaajava.PCallResult;

public class Saynaa {

  public static final int SLOT_TYPE_OBJECT = 0;
  public static final int SLOT_TYPE_NULL = 1;
  public static final int SLOT_TYPE_BOOL = 2;
  public static final int SLOT_TYPE_NUMBER = 3;
  public static final int SLOT_TYPE_STRING = 4;
  public static final int SLOT_TYPE_LIST = 5;
  public static final int SLOT_TYPE_MAP = 6;
  public static final int SLOT_TYPE_RANGE = 7;
  public static final int SLOT_TYPE_MODULE = 8;
  public static final int SLOT_TYPE_CLOSURE = 9;
  public static final int SLOT_TYPE_METHOD_BIND = 10;
  public static final int SLOT_TYPE_FIBER = 11;
  public static final int SLOT_TYPE_CLASS = 12;
  public static final int SLOT_TYPE_POINTER = 13;
  public static final int SLOT_TYPE_INSTANCE = 14;
  Context context;

  static {
    System.loadLibrary("saynaajava");
  }

  public Saynaa(Context context) {
    this.context = context;
    this.vm = saynaa_open();
  }

  public synchronized int runFile(String fileName) {
    return saynaa_doFile(fileName);
  }

  public synchronized int runString(String code) {
    return saynaa_doString(code);
  }

  public synchronized int runStringPcall(String code) {
    return saynaa_doStringPcall(code);
  }

  public synchronized void invokeCallback(int callbackId, Object arg0) {
    invokeCallbackNative(callbackId, arg0);
  }

  public synchronized void invokeCallbackMethod(int callbackId, String methodName, Object[] args) {
    invokeCallbackMethodNative(callbackId, methodName, args);
  }

  public synchronized Object invokeCallbackMethodWithResult(int callbackId, String methodName, Object[] args) {
    return invokeCallbackMethodWithResultNative(callbackId, methodName, args);
  }

  public synchronized Object invokeCallbackMethodWithResultFromSlots(
      int callbackId, String methodName, int argStart, int argCount) {
    return invokeCallbackMethodWithResultFromSlotsNative(callbackId, methodName, argStart, argCount);
  }

  public synchronized PCallResult pcall(String functionName, Object... args) {
    return saynaa_pcall(functionName, args);
  }

  public synchronized Object getGlobal(String name) {
    return saynaa_getGlobal(name);
  }

  public synchronized int getGlobalFunctionId(String name) {
    return saynaa_getGlobalFunctionId(name);
  }

  public synchronized boolean callFunctionById(int functionId, int argStart, int argCount, int retSlot) {
    return saynaa_callFunctionById(functionId, argStart, argCount, retSlot);
  }

  public synchronized Object callFunctionByIdWithArgs(int functionId, Object... args) {
    if (isClosed() || functionId < 0) {
      return null;
    }

    int argc = args == null ? 0 : args.length;
    int argStart = 1;
    int retSlot = 0;
    reserveSlots(argStart + Math.max(argc, 0) + 2);

    for (int i = 0; i < argc; i++) {
      if (!JavaBridge.pushToSlot(this, argStart + i, args[i])) {
        return null;
      }
    }

    if (!callFunctionById(functionId, argStart, argc, retSlot)) {
      return null;
    }
    return JavaBridge.slotToJava(this, retSlot);
  }

  public synchronized Object callGlobalFunction(String name, Object... args) {
    int functionId = getGlobalFunctionId(name);
    return callFunctionByIdWithArgs(functionId, args);
  }

  public synchronized boolean setGlobal(String name, Object value) {
    return saynaa_setGlobal(name, value);
  }

  public synchronized boolean setGlobalValue(String name, Object value, boolean asSaynaa) {
    if (isClosed() || name == null)
      return false;
    int slot = 0;
    boolean ok = asSaynaa ? JavaBridge.pushToSlotAsSaynaa(this, slot, value)
                          : JavaBridge.pushToSlot(this, slot, value);
    if (!ok)
      return false;
    return saynaa_setGlobalFromSlot(name, slot);
  }

  synchronized void reserveSlots(int count) {
    saynaa_reserveSlots(count);
  }

  synchronized void setSlotNull(int slot) {
    saynaa_setSlotNull(slot);
  }

  synchronized void setSlotBool(int slot, boolean value) {
    saynaa_setSlotBool(slot, value);
  }

  synchronized void setSlotNumber(int slot, double value) {
    saynaa_setSlotNumber(slot, value);
  }

  synchronized void setSlotString(int slot, String value) {
    saynaa_setSlotString(slot, value);
  }

  synchronized void setSlotHandle(int slot, int handleId) {
    saynaa_setSlotHandle(slot, handleId);
  }

  synchronized void newList(int slot) {
    saynaa_newList(slot);
  }

  synchronized void newMap(int slot) {
    saynaa_newMap(slot);
  }

  synchronized boolean listInsert(int listSlot, int index, int valueSlot) {
    return saynaa_listInsert(listSlot, index, valueSlot);
  }

  synchronized boolean mapSet(int mapSlot, int keySlot, int valueSlot) {
    return saynaa_mapSet(mapSlot, keySlot, valueSlot);
  }

  synchronized boolean wrapJavaObject(int slot, Object value) {
    return saynaa_wrapJavaObject(slot, value);
  }

  synchronized int getSlotType(int slot) {
    return saynaa_getSlotType(slot);
  }

  synchronized boolean getSlotBool(int slot) {
    return saynaa_getSlotBool(slot);
  }

  synchronized double getSlotNumber(int slot) {
    return saynaa_getSlotNumber(slot);
  }

  synchronized String getSlotString(int slot) {
    return saynaa_getSlotString(slot);
  }

  synchronized Object getSlotJavaObject(int slot) {
    return saynaa_getSlotJavaObject(slot);
  }

  synchronized int getListSize(int listSlot) {
    return saynaa_getListSize(listSlot);
  }

  synchronized boolean listGetToSlot(int listSlot, int index, int valueSlot) {
    return saynaa_listGetToSlot(listSlot, index, valueSlot);
  }

  synchronized int getMapSize(int mapSlot) {
    return saynaa_getMapSize(mapSlot);
  }

  synchronized boolean mapEntryToSlots(int mapSlot, int entryIndex, int keySlot, int valueSlot) {
    return saynaa_mapEntryToSlots(mapSlot, entryIndex, keySlot, valueSlot);
  }

  public synchronized int doFile(String fileName) {
    return runFile(fileName);
  }

  public synchronized int doString(String code) {
    return runString(code);
  }

  public synchronized Object getModule() {
    return saynaa_getModule();
  }

  public synchronized void close() {
    if (this.vm != null && this.vm.getPointer() != 0) {
      try {
        saynaa_close();
      } finally {
        this.vm.setPointer(0);
        this.vm = null;
      }
    }
  }

  public synchronized boolean isClosed() {
    return this.vm == null || this.vm.getPointer() == 0;
  }

  // Called from native bridge when VM writes to stderr.
  public synchronized void onNativeError(String message) {
    if (message == null || message.trim().isEmpty())
      return;

    Log.e("saynaajava", message);

    if (context instanceof SaynaaActivity) {
      ((SaynaaActivity) context).onNativeError(message);
    }
  }

  public synchronized long getCPtrPeer() {
    return this.vm == null ? 0 : this.vm.getPointer();
  }

  private synchronized native void execute(Context context);
  private synchronized native int saynaa_doStringPcall(String code);
  private synchronized native void invokeCallbackNative(int callbackId, Object arg0);
  private synchronized native CPtr saynaa_open();
  private synchronized native Object saynaa_getModule();
  private synchronized native PCallResult saynaa_pcall(String functionName, Object[] args);
  private synchronized native int saynaa_doFile(String fileName);
  private synchronized native int saynaa_doString(String code);
  private synchronized native Object saynaa_getGlobal(String name);
  private synchronized native int saynaa_getGlobalFunctionId(String name);
  private synchronized native boolean saynaa_callFunctionById(
      int functionId, int argStart, int argCount, int retSlot);
  private synchronized native boolean saynaa_setGlobal(String name, Object value);
  private synchronized native boolean saynaa_setGlobalFromSlot(String name, int slot);
  private synchronized native void saynaa_reserveSlots(int count);
  private synchronized native void saynaa_setSlotNull(int slot);
  private synchronized native void saynaa_setSlotBool(int slot, boolean value);
  private synchronized native void saynaa_setSlotNumber(int slot, double value);
  private synchronized native void saynaa_setSlotString(int slot, String value);
  private synchronized native void saynaa_setSlotHandle(int slot, int handleId);
  private synchronized native void saynaa_newList(int slot);
  private synchronized native void saynaa_newMap(int slot);
  private synchronized native boolean saynaa_listInsert(int listSlot, int index, int valueSlot);
  private synchronized native boolean saynaa_mapSet(int mapSlot, int keySlot, int valueSlot);
  private synchronized native boolean saynaa_wrapJavaObject(int slot, Object value);
  private synchronized native int saynaa_getSlotType(int slot);
  private synchronized native boolean saynaa_getSlotBool(int slot);
  private synchronized native double saynaa_getSlotNumber(int slot);
  private synchronized native String saynaa_getSlotString(int slot);
  private synchronized native Object saynaa_getSlotJavaObject(int slot);
  private synchronized native int saynaa_getListSize(int listSlot);
  private synchronized native boolean saynaa_listGetToSlot(int listSlot, int index, int valueSlot);
  private synchronized native int saynaa_getMapSize(int mapSlot);
  private synchronized native boolean saynaa_mapEntryToSlots(
      int mapSlot, int entryIndex, int keySlot, int valueSlot);
  private synchronized native void saynaa_close();
  @SuppressWarnings("unused") private String source;
  @SuppressWarnings("unused") private String scriptPath;
  private CPtr vm;
  private synchronized native void invokeCallbackMethodNative(int callbackId, String methodName, Object[] args);
  private synchronized native Object invokeCallbackMethodWithResultNative(
      int callbackId, String methodName, Object[] args);
  private synchronized native Object invokeCallbackMethodWithResultFromSlotsNative(
      int callbackId, String methodName, int argStart, int argCount);
}