package com.saynaa.saynaajava;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.widget.Toast;
import com.saynaa.crash.CrashHandler;
import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class SaynaaApplication extends Application implements SaynaaContext {
  private static SaynaaApplication mApp;
  private SharedPreferences mSharedPreferences;

  public static SaynaaApplication getInstance() {
    return mApp;
  }
  
  @Override
  public void onCreate() {
    super.onCreate();
    mApp = this;
    CrashHandler crashHandler = CrashHandler.getInstance();
    crashHandler.init(this);
    mSharedPreferences = getSharedPreferences(this);
  }

  private static SharedPreferences getSharedPreferences(Context context) {
    return PreferenceManager.getDefaultSharedPreferences(context);
  }

  @Override
  public Object getSharedData(String key) {
    return mSharedPreferences.getAll().get(key);
  }

  @Override
  public Object getSharedData(String key, Object def) {
    Object ret = mSharedPreferences.getAll().get(key);
    if (ret == null)
      return def;
    return ret;
  }

  @Override
  public boolean setSharedData(String key, Object value) {
    SharedPreferences.Editor edit = mSharedPreferences.edit();
    if (value == null)
      edit.remove(key);
    else if (value instanceof String)
      edit.putString(key, value.toString());
    else if (value instanceof Long)
      edit.putLong(key, (Long) value);
    else if (value instanceof Integer)
      edit.putInt(key, (Integer) value);
    else if (value instanceof Float)
      edit.putFloat(key, (Float) value);
    else if (value instanceof Set)
      edit.putStringSet(key, (Set<String>) value);
    else if (value instanceof Boolean)
      edit.putBoolean(key, (Boolean) value);
    else
      return false;
    edit.apply();
    return true;
  }

  @Override
  public Context getContext() {
    // TODO: Implement this method
    return this;
  }

  @Override
  public void sendMsg(String msg) {
  }

  @Override
  public void sendError(String title, Exception msg) {
  }
}
