package com.android.saynaa.saynaajava;

import android.content.Context;
import android.util.Log;
import android.view.View;

public class Saynaa {
  Context context;
  static {
    System.loadLibrary("saynaajava");
  }

  public Saynaa(Context context) {
    this.context = context;
    this.vm = saynaa_open();
  }

  public void setSource(String src) {
    this.source = src;
  }

  public void run() {
    execute(this.context);
  }

  public synchronized void executeSnippet(String code) {
    executeSnippetNative(code);
  }

  public synchronized void executeSnippetWithView(String code, View view) {
    executeSnippetWithViewNative(code, view);
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

  public synchronized void close() {
    if (this.vm != null && this.vm.getPointer() != 0) {
      saynaa_close();
    }
  }

  public synchronized boolean isClosed() {
    return this.vm == null || this.vm.getPointer() == 0;
  }

  public synchronized long getCPtrPeer() {
    return this.vm == null ? 0 : this.vm.getPointer();
  }

  private synchronized native void execute(Context context);
  private synchronized native void executeSnippetNative(String code);
  private synchronized native void executeSnippetWithViewNative(String code, View view);
  private synchronized native void invokeCallbackNative(int callbackId, Object arg0);
  private synchronized native CPtr saynaa_open();
  private synchronized native void saynaa_close();
  private String source;
  private CPtr vm;
  private synchronized native void invokeCallbackMethodNative(int callbackId, String methodName, Object[] args);
  private synchronized native Object invokeCallbackMethodWithResultNative(int callbackId, String methodName, Object[] args);
}