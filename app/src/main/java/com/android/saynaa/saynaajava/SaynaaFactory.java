package com.android.saynaa.saynaajava;

import android.content.Context;
import java.util.ArrayList;
import java.util.List;

public final class SaynaaFactory {
  private static final List<Saynaa> states = new ArrayList<>();

  private SaynaaFactory() {}

  public static synchronized Saynaa newState(Context context) {
    int i = nextIndex();
    Saynaa state = new Saynaa(context);
    ensureSize(i + 1);
    states.set(i, state);
    return state;
  }

  public static synchronized Saynaa getExistingState(int index) {
    if (index < 0 || index >= states.size()) return null;
    return states.get(index);
  }

  static synchronized void removeState(int index) {
    if (index < 0) return;
    ensureSize(index + 1);
    states.set(index, null);
  }

  private static int nextIndex() {
    for (int i = 0; i < states.size(); i++) {
      if (states.get(i) == null) return i;
    }
    return states.size();
  }

  private static void ensureSize(int size) {
    while (states.size() < size) {
      states.add(null);
    }
  }
}
