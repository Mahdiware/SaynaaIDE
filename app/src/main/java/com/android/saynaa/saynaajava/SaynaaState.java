package com.android.saynaa.saynaajava;

import android.content.Context;
import android.view.View;
import com.android.saynaa.saynaajava.reflection.ReflectionFinder;
import java.util.List;

public class SaynaaState {
  private final Saynaa saynaa;
  private final int stateId;
  private final Context context;

  SaynaaState(Context context, int stateId) {
    this.saynaa = new Saynaa(context);
    this.stateId = stateId;
    this.context = context;
  }

  public synchronized Context getContext() {
    return this.context;
  }

  public synchronized String getSaynaaDir() {
    return SaynaaApplication.getInstance().getSaynaaDir();
  }

  public synchronized void doString(String source) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    int result = saynaa.runString(source);
    if (result != 0) {
      throw new SaynaaException("SaynaaState execution failed with code: " + result);
    }
  }

  public synchronized int runFile(String fileName) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.runFile(fileName);
  }

  public synchronized int runString(String source) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.runString(source);
  }

  public synchronized void invokeCallback(int callbackId, Object arg0) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    saynaa.invokeCallback(callbackId, arg0);
  }

  public synchronized void invokeCallbackMethod(int callbackId, String methodName, Object[] args)
      throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    saynaa.invokeCallbackMethod(callbackId, methodName, args);
  }

  public synchronized Object invokeCallbackMethodWithResult(
      int callbackId, String methodName, Object[] args) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.invokeCallbackMethodWithResult(callbackId, methodName, args);
  }

  public synchronized Object invokeCallbackMethodWithResultFromSlots(
      int callbackId, String methodName, int argStart, int argCount) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.invokeCallbackMethodWithResultFromSlots(callbackId, methodName, argStart, argCount);
  }

  public synchronized PCallResult pcall(String functionName, Object... args) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.pcall(functionName, args);
  }

  public synchronized Object getGlobal(String name) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.getGlobal(name);
  }

  public synchronized int getGlobalFunctionId(String name) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.getGlobalFunctionId(name);
  }

  public synchronized Object callFunctionById(int functionId, Object... args) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    if (functionId < 0) {
      return null;
    }

    int argc = args == null ? 0 : args.length;
    int argStart = 1;
    int retSlot = 0;
    saynaa.reserveSlots(argStart + Math.max(argc, 0) + 2);
    for (int i = 0; i < argc; i++) {
      if (!JavaBridge.pushToSlot(saynaa, argStart + i, args[i])) {
        throw new SaynaaException("Failed to push argument at index " + i + ".");
      }
    }

    boolean ok = saynaa.callFunctionById(functionId, argStart, argc, retSlot);
    if (!ok) {
      throw new SaynaaException("CallFunction failed for id: " + functionId);
    }
    return JavaBridge.slotToJava(saynaa, retSlot);
  }

  public synchronized Object callGlobalFunction(String name, Object... args) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    int functionId = saynaa.getGlobalFunctionId(name);
    return callFunctionById(functionId, args);
  }

  public synchronized void setExtraClassLoaders(List<ClassLoader> loaders) {
    ReflectionFinder.setExtraClassLoaders(loaders);
  }

  public synchronized void addExtraClassLoader(ClassLoader loader) {
    ReflectionFinder.addExtraClassLoader(loader);
  }

  public synchronized void removeExtraClassLoader(ClassLoader loader) {
    ReflectionFinder.removeExtraClassLoader(loader);
  }

  public synchronized List<ClassLoader> getExtraClassLoaders() {
    return ReflectionFinder.getExtraClassLoaders();
  }

  public synchronized boolean setGlobal(String name, Object value) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.setGlobal(name, value);
  }

  public synchronized boolean setGlobalValue(String name, Object value, boolean asSaynaa) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.setGlobalValue(name, value, asSaynaa);
  }

  public synchronized void reserveSlots(int count) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    saynaa.reserveSlots(count);
  }

  public synchronized void setSlotNull(int slot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    saynaa.setSlotNull(slot);
  }

  public synchronized void setSlotBool(int slot, boolean value) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    saynaa.setSlotBool(slot, value);
  }

  public synchronized void setSlotNumber(int slot, double value) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    saynaa.setSlotNumber(slot, value);
  }

  public synchronized void setSlotString(int slot, String value) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    saynaa.setSlotString(slot, value);
  }

  public synchronized void setSlotHandle(int slot, int handleId) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    saynaa.setSlotHandle(slot, handleId);
  }

  public synchronized void setSlotModule(int slot, SaynaaModule module) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    saynaa.setSlotHandle(slot, module.getSlot());
  }

  public synchronized void newList(int slot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    saynaa.newList(slot);
  }

  public synchronized void newMap(int slot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    saynaa.newMap(slot);
  }

  public synchronized SaynaaModule newModule(String name) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    SaynaaModule module = saynaa.newModule(name);

    if (module == null) {
      throw new SaynaaException("Failed to create module with name: " + name);
    }

    return module;
  }

  public synchronized boolean moduleSetGlobal(SaynaaModule module, String name, Object value) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.moduleSetGlobal(module, name, value);
  }

  public synchronized boolean moduleSetGlobal(
      SaynaaModule module, String name, Object clazz, String methodName) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.moduleSetGlobal(module, name, clazz, methodName);
  }

  public synchronized boolean registerModule(SaynaaModule module) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.registerModule(module);
  }

  public synchronized int runFile(SaynaaModule module, String path) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.runFile(module, path);
  }

  public synchronized boolean listInsert(int listSlot, int index, int valueSlot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.listInsert(listSlot, index, valueSlot);
  }

  public synchronized boolean mapSet(int mapSlot, int keySlot, int valueSlot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.mapSet(mapSlot, keySlot, valueSlot);
  }

  public synchronized boolean bindJavaMethod(int slot, Object target, String methodName) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.bindJavaMethod(slot, target, methodName);
  }

  public synchronized boolean bindJavaObject(int slot, Object value) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.bindJavaObject(slot, value);
  }

  public synchronized boolean bindJavaClass(int slot, Class<?> clazz) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.bindJavaClass(slot, clazz);
  }

  public synchronized int getSlotType(int slot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.getSlotType(slot);
  }

  public synchronized boolean getSlotBool(int slot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.getSlotBool(slot);
  }

  public synchronized double getSlotNumber(int slot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.getSlotNumber(slot);
  }

  public synchronized String getSlotString(int slot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.getSlotString(slot);
  }

  public synchronized Object getSlotJavaObject(int slot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.getSlotJavaObject(slot);
  }

  public synchronized int getListSize(int listSlot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.getListSize(listSlot);
  }

  public synchronized boolean listGetToSlot(int listSlot, int index, int valueSlot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.listGetToSlot(listSlot, index, valueSlot);
  }

  public synchronized int getMapSize(int mapSlot) throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.getMapSize(mapSlot);
  }

  public synchronized boolean mapEntryToSlots(int mapSlot, int entryIndex, int keySlot, int valueSlot)
      throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.mapEntryToSlots(mapSlot, entryIndex, keySlot, valueSlot);
  }

  public synchronized Object getModule() throws SaynaaException {
    if (isClosed()) {
      throw new SaynaaException("SaynaaState is closed.");
    }
    return saynaa.getModule();
  }

  public synchronized void close() {
    if (!isClosed()) {
      saynaa.close();
      SaynaaStateFactory.removeState(stateId);
    }
  }

  public synchronized boolean isClosed() {
    return saynaa.isClosed();
  }

  public Saynaa getSaynaa() {
    return saynaa;
  }

  public int getStateId() {
    return stateId;
  }

  public long getCPtrPeer() {
    return saynaa.getCPtrPeer();
  }
}
