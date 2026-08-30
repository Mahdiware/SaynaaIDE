package com.saynaa.saynaajava;

import android.content.Context;
import android.util.Log;
import android.view.View;
import com.saynaa.activity.SaynaaActivity;
import com.saynaa.saynaajava.JavaMethodBinding;
import com.saynaa.saynaajava.PCallResult;
import com.saynaa.saynaajava.datatype.*;
import java.util.ArrayList;
import java.util.List;

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
  public static final int SLOT_TYPE_CONTEXT = 14;
  public static final int SLOT_TYPE_INSTANCE = 15;

  private String saynaadir;
  private Context context;
  private SaynaaModule mainModule;

  /**
   * Array with all mainModule's instances.
   */
  private static final List<SaynaaModule> mainModules = new ArrayList<>();

  static {
    System.loadLibrary("saynaajava");
  }

  public Saynaa(Context context) {
    this.context = context;
    this.vm = saynaa_open();
    this.mainModule = newModule("main");
    this.saynaadir = context.getApplicationContext().getFilesDir().getAbsolutePath();
  }

  // future will use these: when added multiple modules per VM
  public synchronized SaynaaModule newMainModule(String name) {
    SaynaaModule module = newModule("main");
    mainModules.add(module);
    return module;
  }

  public synchronized SaynaaModule getMainModule(int index) {
    if (index < 0 || index >= mainModules.size())
      return null;
    return mainModules.get(index);
  }

  public synchronized void removeMainModule(int index) {
    if (index < 0 || index >= mainModules.size())
      return;
    mainModules.remove(index);
  }

  public synchronized int insertMainModule(SaynaaModule module) {
    mainModules.add(module);
    return mainModules.size() - 1;
  }

  public synchronized void setSaynaaDir(String dir) {
    this.saynaadir = dir;
  }

  public synchronized String getSaynaaDir() {
    return this.saynaadir;
  }

  public synchronized Context getContext() {
    return this.context;
  }

  public synchronized SaynaaModule getMainModule() {
    return this.mainModule;
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

  public synchronized Object getGlobal(String name) {
    return getGlobal(mainModule.getHandleId(), name);
  }

  public synchronized Object getGlobal(int handleId, String name) {
    return saynaa_getGlobal(handleId, name);
  }

  public synchronized int getGlobalFunctionId(String name) {
    return getGlobalFunctionId(mainModule.getHandleId(), name);
  }

  public synchronized int getGlobalFunctionId(int handleId, String name) {
    return saynaa_getGlobalFunctionId(handleId, name);
  }

  public synchronized int getGlobalId(int handleId, String name) {
    return saynaa_getGlobalId(handleId, name);
  }

  public synchronized boolean callFunctionById(int functionId, int argStart, int argCount, int retSlot) {
    return saynaa_callFunctionById(mainModule.getHandleId(), functionId, argStart, argCount, retSlot);
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

  public synchronized int captureSlotHandle(int slot) {
    return saynaa_captureSlotHandle(slot);
  }

  public synchronized void setSlotPinnedHandle(int slot, int pinnedHandleId) {
    saynaa_setSlotPinnedHandle(slot, pinnedHandleId);
  }

  public synchronized SaynaaList newList() {
    return new SaynaaList(this, SLOT_TYPE_LIST, saynaa_newList());
  }

  public synchronized SaynaaMap newMap() {
    return new SaynaaMap(this, SLOT_TYPE_MAP, saynaa_newMap());
  }

  public synchronized boolean newInstance(int classHandleId, int argStart, int argCount, int retSlot) {
    return saynaa_newInstance(classHandleId, argStart, argCount, retSlot);
  }

  public synchronized Object objGetattr(int handleId, String attrName, boolean skipGetter) {
    return saynaa_objGetattr(handleId, attrName, skipGetter);
  }

  public synchronized boolean callMethod(int handleId, String methodName, int argStart, int argCount, int retSlot) {
    return saynaa_callMethod(handleId, methodName, argStart, argCount, retSlot);
  }

  public synchronized boolean isSlotJava(int slot) {
    return saynaa_isSlotJava(slot);
  }

  public synchronized SaynaaModule newModule(String name) {
    int handleId = saynaa_newModule(name);
    if (handleId < 0) {
      Log.e("Saynaa", "Failed to create new module: " + name);
      return null;
    }
    return new SaynaaModule(this, SLOT_TYPE_MODULE, handleId);
  }

  public synchronized boolean moduleSetGlobal(int handleId, String name, Object value) {
    return saynaa_moduleSetGlobal(handleId, name, value);
  }

  public synchronized boolean moduleSetGlobal(int handleId, String name, Object clazz, String methodName) {
    return saynaa_moduleSetGlobal(handleId, name, new JavaMethodBinding(clazz, methodName));
  }

  public synchronized boolean registerModule(int handleId) {
    return saynaa_registerModule(handleId);
  }

  public synchronized boolean setGlobal(String name, Object value) {
    return this.mainModule.setGlobal(name, value);
  }

  public synchronized boolean setGlobal(String name, Object clazz, String methodName) {
    return this.mainModule.setGlobal(name, clazz, methodName);
  }

  public synchronized int runFile(String path) {
    return saynaa_runFile(this.mainModule.getHandleId(), path);
  }

  public synchronized int runFile(SaynaaModule module, String path) {
    return saynaa_runFile(module.getHandleId(), path);
  }

  public synchronized boolean listInsert(int handleId, int index, int valueSlot) {
    return saynaa_listInsert(handleId, index, valueSlot);
  }

  public synchronized boolean listReplace(int handleId, int index, int valueSlot) {
    return saynaa_listReplace(handleId, index, valueSlot);
  }

  public synchronized boolean mapSet(int handleId, int keySlot, int valueSlot) {
    return saynaa_mapSet(handleId, keySlot, valueSlot);
  }

  public synchronized boolean bindJavaObject(int slot, Object value) {
    Log.d("Saynaa", "Binding Java object to slot " + slot + ": " + value);
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

  public synchronized int getListSize(int handleId) {
    return saynaa_getListSize(handleId);
  }

  public synchronized boolean listGetToSlot(int handleId, int index, int valueSlot) {
    return saynaa_listGetToSlot(handleId, index, valueSlot);
  }

  public synchronized int getMapSize(int handleId) {
    return saynaa_getMapSize(handleId);
  }

  public synchronized boolean mapGetToSlots(int handleId, int keySlot, int valueSlot) {
    return saynaa_mapGetToSlots(handleId, keySlot, valueSlot);
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

  private synchronized native long saynaa_open();
  private synchronized native int saynaa_getGlobalFunctionId(int handleId, String name);
  private synchronized native Object saynaa_getGlobal(int handleId, String name);
  private synchronized native int saynaa_getGlobalId(int handleId, String name);
  private synchronized native boolean saynaa_callFunctionById(
      int handleId, int functionId, int argStart, int argCount, int retSlot);
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
  private synchronized native int saynaa_captureSlotHandle(int slot);
  private synchronized native void saynaa_setSlotPinnedHandle(int slot, int pinnedHandleId);
  private synchronized native void saynaa_addSearchPath(String path);
  private synchronized native int saynaa_newList();
  private synchronized native int saynaa_newMap();
  private synchronized native boolean saynaa_isSlotJava(int slot);
  private synchronized native int saynaa_newModule(String name);
  private synchronized native boolean saynaa_callMethod(
      int handleId, String methodName, int argStart, int argCount, int retSlot);
  private synchronized native Object saynaa_objGetattr(int handleId, String attrName, boolean skipGetter);
  private synchronized native boolean saynaa_newInstance(
      int classHandleId, int argStart, int argCount, int retSlot);
  private synchronized native boolean saynaa_moduleSetGlobal(int moduleSlot, String name, Object value);
  private synchronized native boolean saynaa_registerModule(int handleId);
  private synchronized native int saynaa_runFile(int moduleSlot, String path);
  private synchronized native boolean saynaa_listReplace(int handleId, int index, int valueSlot);
  private synchronized native boolean saynaa_listInsert(int handleId, int index, int valueSlot);
  private synchronized native boolean saynaa_mapSet(int handleId, int keySlot, int valueSlot);
  private synchronized native boolean saynaa_bindJavaObject(int slot, Object value);
  private synchronized native boolean saynaa_bindJavaClass(int slot, Class<?> clazz);
  private synchronized native boolean saynaa_bindJavaMethod(
      int slot, Object target, String methodName, boolean isStatic);
  private synchronized native int saynaa_getSlotType(int slot);
  private synchronized native boolean saynaa_getSlotBool(int slot);
  private synchronized native double saynaa_getSlotNumber(int slot);
  private synchronized native String saynaa_getSlotString(int slot);
  private synchronized native Object saynaa_getSlotJavaObject(int slot);
  private synchronized native int saynaa_getListSize(int handleId);
  private synchronized native boolean saynaa_listGetToSlot(int handleId, int index, int valueSlot);
  private synchronized native int saynaa_getMapSize(int mapSlot);
  private synchronized native boolean saynaa_mapGetToSlots(int handleId, int keySlot, int valueSlot);
  private synchronized native void saynaa_close();
  private long vm;
  private synchronized native void invokeCallbackMethodNative(int callbackId, String methodName, Object[] args);
  private synchronized native Object invokeCallbackMethodWithResultNative(
      int callbackId, String methodName, Object[] args);
  private synchronized native Object invokeCallbackWithResultFromSlots(
      int callbackId, String methodName, int argStart, int argCount);
}