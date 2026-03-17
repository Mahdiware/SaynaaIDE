package com.android.saynaa.saynaajava;

import android.content.Context;
import android.util.Log;
import android.view.View;

import com.android.saynaa.activity.SaynaaActivity;

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

  public void setScriptPath(String scriptPath) {
    this.scriptPath = scriptPath;
  }

  public void run() {
    execute(this.context);
  }

  public synchronized void executeSnippet(String code) {
    executeSnippetWithViewNative(code, null);
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

  public synchronized int pcall(String functionName, Object... args) {
    return saynaa_pcall(functionName, args);
  }

  public synchronized int doFile(String fileName) {
    return saynaa_doFile(fileName);
  }

  public synchronized int doString(String code) {
    return saynaa_doString(code);
  }

  public synchronized void close() {
    if (this.vm != null && this.vm.getPointer() != 0) {
      saynaa_close();
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
  private synchronized native void executeSnippetWithViewNative(String code, View view);
  private synchronized native void invokeCallbackNative(int callbackId, Object arg0);
  private synchronized native CPtr saynaa_open();
  private synchronized native int saynaa_pcall(String functionName, Object[] args);
  private synchronized native int saynaa_doFile(String fileName);
  private synchronized native int saynaa_doString(String code);
  private synchronized native void saynaa_close();
  private String source;
  private String scriptPath;
  private CPtr vm;
  private synchronized native void invokeCallbackMethodNative(int callbackId, String methodName, Object[] args);
  private synchronized native Object invokeCallbackMethodWithResultNative(int callbackId, String methodName, Object[] args);
}