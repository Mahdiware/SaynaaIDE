package com.android.saynaa.saynaajava;

import android.content.Context;

public class SaynaaState {
  private final Saynaa saynaa;
  private final int stateId;

  SaynaaState(Context context, int stateId) {
    this.saynaa = new Saynaa(context);
    this.stateId = stateId;
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

  public synchronized void close() {
    if (!isClosed()) {
      saynaa.close();
      SaynaaStateFactory.removeState(stateId);
    }
  }

  public synchronized boolean isClosed() {
    return saynaa.isClosed();
  }

  public int getStateId() {
    return stateId;
  }

  public long getCPtrPeer() {
    return saynaa.getCPtrPeer();
  }
}
