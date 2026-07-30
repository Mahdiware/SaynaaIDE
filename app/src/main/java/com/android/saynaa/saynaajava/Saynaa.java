package com.android.saynaa.saynaajava;

import android.content.Context;
import android.util.Log;
import android.view.View;
import com.android.saynaa.activity.SaynaaActivity;
import com.android.saynaa.saynaajava.JavaMethodBinding;
import com.android.saynaa.saynaajava.PCallResult;
import com.android.saynaa.saynaajava.datatype.*;

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
  public Context context;

  static {
    System.loadLibrary("saynaajava");
  }

  public Saynaa(Context context) {
    this.context = context;
    this.vm = saynaa_open();
  }

  public synchronized String getSaynaaDir() {
    return SaynaaApplication.getInstance().getSaynaaDir();
  }

  public synchronized Context getContext() {
    return this.context;
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

  public synchronized void invokeCallbackMethod(int callbackId, String methodName, Object[] args) {
    invokeCallbackMethodNative(callbackId, methodName, args);
  }

  public synchronized Object invokeCallbackMethodWithResult(int callbackId, String methodName, Object[] args) {
    return invokeCallbackMethodWithResultNative(callbackId, methodName, args);
  }

  public synchronized Object invokeCallbackMethodWithResultFromSlots(
      int callbackId, String methodName, int argStart, int argCount) {
    return invokeCallbackWithResultFromSlots(callbackId, methodName, argStart, argCount);
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

  public synchronized int getGlobalId(String name) {
    return saynaa_getGlobalId(name);
  }

  public synchronized boolean callFunctionById(int functionId, int argStart, int argCount, int retSlot) {
    return saynaa_callFunctionById(functionId, argStart, argCount, retSlot);
  }

  public synchronized Object callGlobalFunction(String name, Object... args) {
    int functionId = getGlobalFunctionId(name);
    return callFunctionByIdWithArgs(functionId, args);
  }

  public synchronized Object callFunctionById(int functionId, Object... args) {
    return callFunctionByIdWithArgs(functionId, args);
  }

  public synchronized Object callFunctionByIdWithArgs(int functionId, Object... args) {
    if (isClosed() || functionId < 0) {
      return null;
    }

    if (functionId < 0) {
      return null;
    }

    int argc = args == null ? 0 : args.length;
    int argStart = allocSlot(argc + 4);

    int retSlot = nextSlot();
    reserveSlots(argStart + Math.max(argc, 0) + 2);
    for (int i = 0; i < argc; i++) {
      if (!JavaBridge.pushToSlot(this, argStart + i, args[i])) {
        // throw new SaynaaException("Failed to push argument at index " + i + ".");
        freeSlot(argStart, argc + 4);
        return null;
      }
    }

    boolean ok = callFunctionById(functionId, argStart, argc, retSlot);
    if (!ok) {
      freeSlot(argStart, argc + 4);
      throw null;
    }
    freeSlot(argStart, argc + 4);
    return JavaBridge.slotToJava(this, retSlot);
  }

  public synchronized int getSlotCount() {
    return saynaa_getSlotCount();
  }

  public synchronized boolean setGlobal(String name, Object value) {
    return saynaa_setGlobal(name, value);
  }

  public synchronized void reserveSlots(int count) {
    saynaa_reserveSlots(count);
  }

  public synchronized int allocSlot(int count) {
    return saynaa_allocSlot(count);
  }

  public synchronized int nextSlot() {
    return saynaa_nextSlot();
  }

  public synchronized void freeSlot(int slot) {
    saynaa_freeSlot(slot, 1);
  }

  public synchronized void freeSlot(int slot, int count) {
    saynaa_freeSlot(slot, count);
  }

  public synchronized void addSearchPath(String path) {
    saynaa_addSearchPath(path);
  }

  public synchronized int chdir(String path) {
    return saynaa_chdir(path);
  }

  public synchronized void setSlotNull(int slot) {
    saynaa_setSlotNull(slot);
  }

  public synchronized void setSlotBool(int slot, boolean value) {
    saynaa_setSlotBool(slot, value);
  }

  public synchronized void setSlotNumber(int slot, double value) {
    saynaa_setSlotNumber(slot, value);
  }

  public synchronized void setSlotString(int slot, String value) {
    saynaa_setSlotString(slot, value);
  }

  public synchronized void setSlotHandle(int slot, int handleId) {
    saynaa_setSlotHandle(slot, handleId);
  }

  public synchronized void newList(int slot) {
    saynaa_newList(slot);
  }

  public synchronized void newMap(int slot) {
    saynaa_newMap(slot);
  }

  public synchronized SaynaaModule newModule(String name) {
    Object module = saynaa_newModule(name);
    // module is istance of SaynaaModule
    if (module instanceof SaynaaModule) {
      return (SaynaaModule) module;
    } else {
      return null;
    }
  }

  public synchronized boolean moduleSetGlobal(SaynaaModule module, String name, Object value) {
    return saynaa_moduleSetGlobal(module.getSlot(), name, value);
  }

  public synchronized boolean moduleSetGlobal(SaynaaModule module, String name, Object clazz, String methodName) {
    return saynaa_moduleSetGlobal(module.getSlot(), name, new JavaMethodBinding(clazz, methodName));
  }

  public synchronized boolean registerModule(SaynaaModule module) {
    return saynaa_registerModule(module.getSlot());
  }

  public synchronized int runFile(SaynaaModule module, String path) {
    return saynaa_runFile(module.getSlot(), path);
  }

  public synchronized boolean listInsert(int listSlot, int index, int valueSlot) {
    return saynaa_listInsert(listSlot, index, valueSlot);
  }

  public synchronized boolean mapSet(int mapSlot, int keySlot, int valueSlot) {
    return saynaa_mapSet(mapSlot, keySlot, valueSlot);
  }

  public synchronized boolean bindJavaObject(int slot, Object value) {
    return saynaa_bindJavaObject(slot, value);
  }

  public synchronized boolean bindJavaClass(int slot, Class<?> clazz) {
    return saynaa_bindJavaClass(slot, clazz);
  }

  public synchronized boolean bindJavaMethod(int slot, Object target, String methodName) {
    if (target instanceof Class) {
      // Static method
      return saynaa_bindJavaMethod(slot, target, methodName, true);
    } else {
      // Instance method
      return saynaa_bindJavaMethod(slot, target, methodName, false);
    }
  }

  public synchronized int getSlotType(int slot) {
    return saynaa_getSlotType(slot);
  }

  public synchronized boolean getSlotBool(int slot) {
    return saynaa_getSlotBool(slot);
  }

  public synchronized double getSlotNumber(int slot) {
    return saynaa_getSlotNumber(slot);
  }

  public synchronized String getSlotString(int slot) {
    return saynaa_getSlotString(slot);
  }

  public synchronized Object getSlotJavaObject(int slot) {
    return saynaa_getSlotJavaObject(slot);
  }

  public synchronized int getListSize(int listSlot) {
    return saynaa_getListSize(listSlot);
  }

  public synchronized boolean listGetToSlot(int listSlot, int index, int valueSlot) {
    return saynaa_listGetToSlot(listSlot, index, valueSlot);
  }

  public synchronized int getMapSize(int mapSlot) {
    return saynaa_getMapSize(mapSlot);
  }

  public synchronized boolean mapEntryToSlots(int mapSlot, int entryIndex, int keySlot, int valueSlot) {
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
    if (this.vm != 0) {
      try {
        saynaa_close();
      } finally {
        this.vm = 0;
      }
    }
  }

  public synchronized boolean isClosed() {
    return this.vm == 0;
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
    return this.vm;
  }

  private synchronized native int saynaa_doStringPcall(String code);
  private synchronized native long saynaa_open();
  private synchronized native Object saynaa_getModule();
  private synchronized native PCallResult saynaa_pcall(String functionName, Object[] args);
  private synchronized native int saynaa_doFile(String fileName);
  private synchronized native int saynaa_doString(String code);
  private synchronized native Object saynaa_getGlobal(String name);
  private synchronized native int saynaa_getGlobalFunctionId(String name);
  private synchronized native int saynaa_getGlobalId(String name);
  private synchronized native boolean saynaa_callFunctionById(
      int functionId, int argStart, int argCount, int retSlot);
  private synchronized native boolean saynaa_setGlobal(String name, Object value);
  private synchronized native boolean saynaa_setGlobalFromSlot(String name, int slot);
  private synchronized native void saynaa_reserveSlots(int count);
  private synchronized native int saynaa_nextSlot();
  private synchronized native void saynaa_freeSlot(int slot, int count);
  private synchronized native int saynaa_allocSlot(int count);
  private synchronized native int saynaa_getSlotCount();
  private synchronized native int saynaa_chdir(String path);
  private synchronized native void saynaa_setSlotNull(int slot);
  private synchronized native void saynaa_setSlotBool(int slot, boolean value);
  private synchronized native void saynaa_setSlotNumber(int slot, double value);
  private synchronized native void saynaa_setSlotString(int slot, String value);
  private synchronized native void saynaa_setSlotHandle(int slot, int handleId);
  private synchronized native void saynaa_addSearchPath(String path);
  private synchronized native void saynaa_newList(int slot);
  private synchronized native void saynaa_newMap(int slot);
  private synchronized native Object saynaa_newModule(String name);
  private synchronized native boolean saynaa_moduleSetGlobal(int moduleSlot, String name, Object value);
  private synchronized native boolean saynaa_registerModule(int moduleSlot);
  private synchronized native int saynaa_runFile(int moduleSlot, String path);
  private synchronized native boolean saynaa_listInsert(int listSlot, int index, int valueSlot);
  private synchronized native boolean saynaa_mapSet(int mapSlot, int keySlot, int valueSlot);
  private synchronized native boolean saynaa_bindJavaObject(int slot, Object value);
  private synchronized native boolean saynaa_bindJavaClass(int slot, Class<?> clazz);
  private synchronized native boolean saynaa_bindJavaMethod(
      int slot, Object target, String methodName, boolean isStatic);
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
  private long vm;
  private synchronized native void invokeCallbackMethodNative(int callbackId, String methodName, Object[] args);
  private synchronized native Object invokeCallbackMethodWithResultNative(
      int callbackId, String methodName, Object[] args);
  private synchronized native Object invokeCallbackWithResultFromSlots(
      int callbackId, String methodName, int argStart, int argCount);
}