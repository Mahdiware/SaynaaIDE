package com.android.saynaa.saynaajava;

import android.content.*;
import java.util.*;

public interface SaynaaContext {
  public void call(String func, Object... args);

  public String getSaynaaPath();

  public String getSaynaaPath(String path);

  public String getSaynaaPath(String dir, String name);

  public String getSaynaaDir();

  public String getSaynaaDir(String dir);

  public String getSaynaaExtDir();

  public String getSaynaaExtDir(String dir);

  public void setSaynaaExtDir(String dir);

  public Context getContext();

  public SaynaaState getSaynaaState();

  public Object doFile(String path, Object... arg);

  public void sendMsg(String msg);

  public void sendError(String title, Exception msg);

  public int getWidth();

  public int getHeight();

  public Map getGlobalData();

  public Object getSharedData(String key);
  public Object getSharedData(String key, Object def);
  public boolean setSharedData(String key, Object value);

  public void regGc(SaynaaGcable obj);
}
