package com.android.saynaa.saynaajava;

import android.view.View;

public final class SaynaaOnClickListener implements View.OnClickListener {
  private final Saynaa saynaa;
  private final SaynaaState state;
  private final String functionName;
  private int functionId = -1;

  public SaynaaOnClickListener(Saynaa saynaa, String functionName) {
    this.saynaa = saynaa;
    this.state = null;
    this.functionName = functionName == null ? "" : functionName;
  }

  public SaynaaOnClickListener(SaynaaState state, String functionName) {
    this.saynaa = null;
    this.state = state;
    this.functionName = functionName == null ? "" : functionName;
  }

  @Override
  public void onClick(View v) {
    if (functionName.isEmpty())
      return;
    if (state != null && !state.isClosed()) {
      try {
        if (functionId < 0) {
          functionId = state.getGlobalFunctionId(functionName);
        }
        if (functionId >= 0) {
          state.callFunctionByIdWithView(functionId, v);
        }
      } catch (SaynaaException ignored) {
        // Ignore callback errors to avoid crashing event dispatch.
      }
      return;
    }
    if (saynaa == null || saynaa.isClosed())
      return;
    if (functionId < 0) {
      functionId = saynaa.getGlobalFunctionId(functionName);
    }
    if (functionId >= 0) {
      saynaa.callFunctionByIdWithView(functionId, v);
    }
  }
}
