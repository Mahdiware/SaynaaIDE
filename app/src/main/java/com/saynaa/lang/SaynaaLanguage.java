package com.saynaa.lang;

import androidx.annotation.NonNull;
import io.github.rosemoe.sora.lang.EmptyLanguage;
import io.github.rosemoe.sora.lang.analysis.AnalyzeManager;

public class SaynaaLanguage extends EmptyLanguage {
  private final SaynaaAnalyzeManager analyzeManager = new SaynaaAnalyzeManager();

  @NonNull
  @Override
  public AnalyzeManager getAnalyzeManager() {
    return analyzeManager;
  }

  @Override
  public void destroy() {
    analyzeManager.destroy();
  }
}
