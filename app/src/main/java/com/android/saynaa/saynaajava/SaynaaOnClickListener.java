package com.android.saynaa.saynaajava;

import android.view.View;

public final class SaynaaOnClickListener implements View.OnClickListener {
  private final Saynaa saynaa;
  private final String script;

  public SaynaaOnClickListener(Saynaa saynaa, String script) {
    this.saynaa = saynaa;
    this.script = script == null ? "" : script;
  }

  @Override
  public void onClick(View v) {
    if (saynaa == null || saynaa.isClosed())
      return;
    saynaa.executeSnippet(script);
  }
}
