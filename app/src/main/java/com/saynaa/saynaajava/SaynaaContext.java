package com.saynaa.saynaajava;

import android.content.*;
import java.util.*;

public interface SaynaaContext {
  public void call(String func, Object... args);

  public Context getContext();

  public Saynaa getSaynaa();

  public void sendMsg(String msg);

  public void sendError(String title, Exception msg);

  public Object getSharedData(String key);
  public Object getSharedData(String key, Object def);
  public boolean setSharedData(String key, Object value);
}
