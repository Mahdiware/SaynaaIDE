package com.android.saynaa.saynaajava;

import android.app.Application;
import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.preference.PreferenceManager;
import android.widget.Toast;
import com.android.saynaa.crash.CrashHandler;
import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

public class SaynaaApplication extends Application implements SaynaaContext {
  private static SaynaaApplication mApp;
  static private HashMap<String, Object> data = new HashMap<String, Object>();
  protected String localDir;
  protected String odexDir;
  protected String libDir;
  protected String saynaaMdDir;
  protected String saynaaCpath;
  protected String saynaaLpath;
  protected String saynaaExtDir;
  private boolean isUpdata;
  private SharedPreferences mSharedPreferences;

  public String getPathFromUri(Uri uri) {
    String path = null;
    if (uri != null) {
      String[] p = {getPackageName()};
      switch (uri.getScheme()) {
      case "content":
        /*try {
                                    InputStream in = getContentResolver().openInputStream(uri);
                            } catch (IOException e) {
                                    e.printStackTrace();
                            }*/
        Cursor cursor = getContentResolver().query(uri, p, null, null, null);

        if (cursor != null) {
          int idx = cursor.getColumnIndexOrThrow(getPackageName());
          if (idx < 0)
            break;
          path = cursor.getString(idx);
          cursor.moveToFirst();
          cursor.close();
        }
        break;
      case "file":
        path = uri.getPath();
        break;
      }
    }
    return path;
  }

  public static SaynaaApplication getInstance() {
    return mApp;
  }

  @Override
  public void regGc(SaynaaGcable obj) {
    // TODO: Implement this method
  }

  @Override
  public String getSaynaaPath() {
    // TODO: Implement this method
    return null;
  }

  @Override
  public String getSaynaaPath(String path) {
    return new File(getSaynaaDir(), path).getAbsolutePath();
  }

  @Override
  public String getSaynaaPath(String dir, String name) {
    return new File(getSaynaaDir(dir), name).getAbsolutePath();
  }

  public int getWidth() {
    return getResources().getDisplayMetrics().widthPixels;
  }

  public int getHeight() {
    return getResources().getDisplayMetrics().heightPixels;
  }

  @Override
  public String getSaynaaDir(String dir) {
    // TODO: Implement this method
    return localDir;
  }

  @Override
  public String getSaynaaExtDir(String name) {
    File dir = new File(getSaynaaExtDir(), name);
    if (!dir.exists())
      if (!dir.mkdirs())
        return dir.getAbsolutePath();
    return dir.getAbsolutePath();
  }

  public String getLibDir() {
    // TODO: Implement this method
    return libDir;
  }

  public String getOdexDir() {
    // TODO: Implement this method
    return odexDir;
  }

  @Override
  public void onCreate() {
    super.onCreate();
    mApp = this;
    CrashHandler crashHandler = CrashHandler.getInstance();
    // 注册crashHandler
    crashHandler.init(this);
    mSharedPreferences = getSharedPreferences(this);
    // 初始化AndroSaynaa工作目录
    if (Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
      String sdDir = Environment.getExternalStorageDirectory().getAbsolutePath();
      saynaaExtDir = sdDir + "/AndroSaynaa";
    } else {
      File[] fs = new File("/storage").listFiles();
      for (File f : fs) {
        String[] ls = f.list();
        if (ls == null)
          continue;
        if (ls.length > 5)
          saynaaExtDir = f.getAbsolutePath() + "/AndroSaynaa";
      }
      if (saynaaExtDir == null)
        saynaaExtDir = getDir("AndroSaynaa", Context.MODE_PRIVATE).getAbsolutePath();
    }
    // saynaaExtDir = mSharedPreferences.getString("user_data_dir", getString(R.string.default_user_data_dir));

    File destDir = new File(saynaaExtDir);
    if (!destDir.exists())
      destDir.mkdirs();

    // 定义文件夹
    localDir = getFilesDir().getAbsolutePath();
    odexDir = getDir("odex", Context.MODE_PRIVATE).getAbsolutePath();
    libDir = getDir("lib", Context.MODE_PRIVATE).getAbsolutePath();
    saynaaMdDir = getDir("saynaa", Context.MODE_PRIVATE).getAbsolutePath();
    saynaaCpath = getApplicationInfo().nativeLibraryDir + "/lib?.so"
                  + ";" + libDir + "/lib?.so";
    // saynaaDir = extDir;
    saynaaLpath = localDir + "/?.saynaa;" + localDir + "/saynaa/?.saynaa;" + localDir
                  + "/?/init.saynaa;" + saynaaMdDir + "/?.saynaa;" + saynaaMdDir
                  + "/saynaa/?.saynaa;" + saynaaMdDir + "/?/init.saynaa;";
    // checkInfo();
  }

  private static SharedPreferences getSharedPreferences(Context context) {
    /*if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.N) {
        Context deContext = context.createDeviceProtectedStorageContext();
        if (deContext != null)
            return PreferenceManager.getDefaultSharedPreferences(deContext);
        else
            return PreferenceManager.getDefaultSharedPreferences(context);
    } else {
        return PreferenceManager.getDefaultSharedPreferences(context);
    }*/
    return PreferenceManager.getDefaultSharedPreferences(context);
  }

  @Override
  public String getSaynaaDir() {
    // TODO: Implement this method
    return localDir;
  }

  @Override
  public void call(String name, Object[] args) {
    // TODO: Implement this method
  }

  @Override
  public Map getGlobalData() {
    return data;
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

  public Object get(String name) {
    // TODO: Implement this method
    return data.get(name);
  }

  public String getLocalDir() {
    // TODO: Implement this method
    return localDir;
  }

  public String getMdDir() {
    // TODO: Implement this method
    return saynaaMdDir;
  }

  @Override
  public String getSaynaaExtDir() {
    // TODO: Implement this method
    return saynaaExtDir;
  }

  @Override
  public void setSaynaaExtDir(String dir) {
    // TODO: Implement this method
    if (Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
      String sdDir = Environment.getExternalStorageDirectory().getAbsolutePath();
      saynaaExtDir = new File(sdDir, dir).getAbsolutePath();
    } else {
      File[] fs = new File("/storage").listFiles();
      for (File f : fs) {
        String[] ls = f.list();
        if (ls == null)
          continue;
        if (ls.length > 5)
          saynaaExtDir = new File(f, dir).getAbsolutePath();
      }
      if (saynaaExtDir == null)
        saynaaExtDir = getDir(dir, Context.MODE_PRIVATE).getAbsolutePath();
    }
  }

  @Override
  public Context getContext() {
    // TODO: Implement this method
    return this;
  }

  @Override
  public SaynaaState getSaynaaState() {
    // TODO: Implement this method
    return null;
  }

  @Override
  public Object doFile(String path, Object[] arg) {
    // TODO: Implement this method
    return null;
  }

  @Override
  public void sendMsg(String msg) {
    // TODO: Implement this method
    Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
  }

  @Override
  public void sendError(String title, Exception msg) {
  }

  public String getSaynaaMdDir() {
    return saynaaMdDir;
  }
}
