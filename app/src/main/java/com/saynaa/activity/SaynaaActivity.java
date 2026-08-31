package com.saynaa.activity;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Color;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.os.StrictMode;
import android.util.DisplayMetrics;
import android.util.Log;
import android.util.TypedValue;
import android.view.ContextMenu;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.ScrollView;
import android.widget.TextView;
import android.widget.Toast;
import com.saynaa.saynaajava.*;
import com.saynaa.saynaajava.JavaModule;
import com.saynaa.saynaajava.datatype.*;
import com.saynaa.saynaajava.reflection.ReflectionFinder;
import com.saynaa.utils.FileUtil;
import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * SaynaaActivity is the main entry point for Saynaa scripts. It initializes the
 * Saynaa runtime, loads the main script, and provides hooks for lifecycle events
 */
public class SaynaaActivity extends Activity implements SaynaaBroadcastReceiver.OnReceiveListener, SaynaaContext {
  public static final String ARG = "arg";
  public static final String DATA = "data";
  public static final String NAME = "name";
  private static final String TAG = "SaynaaActivity";

  protected File saynaaDir;
  protected String saynaaPath;
  protected File localDir;

  protected Handler handler;
  protected TextView status;
  protected LinearLayout layout;


  private int activityFlags = 0;
  protected boolean isCreate = false;
  protected boolean DebugMode = false;

  private ArrayList<SaynaaGcable> gclist = new ArrayList<SaynaaGcable>();

  private SaynaaBroadcastReceiver mReceiver;

  protected StringBuilder toastbuilder = new StringBuilder();
  protected Toast toast;
  protected long lastShow;

  protected Menu optionsMenu;

  // Saynaa runtime
  protected Saynaa saynaa;

  private SaynaaDexLoader dexLoader;

  // Optional compatibility placeholders
  protected Object mOnKeyDown;
  protected Object mOnKeyUp;
  protected Object mOnKeyLongPress;
  protected Object mOnTouchEvent;
  protected Object mOnKeyShortcut;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
    StrictMode.setThreadPolicy(policy);
    super.onCreate(savedInstanceState);

    initUiShell();

    localDir = getDir("saynaa", Context.MODE_PRIVATE);

    FileUtil.installSaynaaCode(this, localDir);

    try {
      saynaaPath = getSaynaaPath();
      if (saynaaPath == null) {
        saynaaPath = new File(localDir, "main.sa").getAbsolutePath();
      }
      saynaaDir = new File(saynaaPath).getParentFile();

      saynaa = new Saynaa(this);
      saynaa.setSaynaaDir(saynaaDir.getAbsolutePath());
      new JavaModule(saynaa).create();
      saynaa.setGlobal("activity", this);

      dexLoader = new SaynaaDexLoader(this, saynaaDir);
      dexLoader.loadLibs();
      ReflectionFinder.setExtraClassLoaders(dexLoader.getClassLoaders());
      File initFile = new File(saynaaDir == null ? localDir : saynaaDir, "init.sa");
      if (initFile.exists()) {
        int initResult = saynaa.runFile(initFile.getAbsolutePath());
        if (initResult != 0) {
          sendMsg("Startup failed @ " + initFile.getAbsolutePath() + "\n");
          Log.e(TAG, "Failed to run init.sa @ " + initFile.getAbsolutePath() + ", result: " + initResult);
          setContentView(layout);
          return;
        }
      }
      int result = saynaa.runFile(saynaaPath);
      if (result != 0) {
        Log.e(TAG, "Failed to run main.sa @ " + saynaaPath + ", result: " + result);
        sendMsg("Startup failed @ " + saynaaPath + "\n");
        setContentView(layout);
        return;
      }

      isCreate = true;
      Object[] launchArgs = null;
      Bundle launchBundle = null;
      Intent launchIntent = getIntent();
      if (launchIntent != null) {
        Object extra = launchIntent.getSerializableExtra(ARG);
        if (extra instanceof Object[]) {
          launchArgs = (Object[]) extra;
        } else {
          launchBundle = launchIntent.getBundleExtra(ARG);
        }
      }

      if (launchArgs != null && launchArgs.length > 0) {
        runFunc("onCreate", launchArgs);
      } else if (launchBundle != null) {
        runFunc("onCreate", launchBundle);
      } else {
        runFunc("onCreate", savedInstanceState != null ? savedInstanceState : new Bundle());
      }

    } catch (Throwable t) {
      Log.e(TAG, "onCreate failed", t);
      sendMsg("onCreate error: " + t.toString());
      setContentView(layout);
    }
  }

  @Override
  public void onReceive(Context context, Intent intent) {
    runFunc("onReceive", context, intent);
  }

  @Override
  protected void onStart() {
    runFunc("onStart");
    super.onStart();
  }

  @Override
  protected void onResume() {
    runFunc("onResume");
    super.onResume();
  }

  @Override
  protected void onPause() {
    runFunc("onPause");
    super.onPause();
  }

  @Override
  protected void onStop() {
    runFunc("onStop");
    super.onStop();
  }

  @Override
  protected void onDestroy() {
    if (mReceiver != null)
      unregisterReceiver(mReceiver);

    for (SaynaaGcable obj : gclist) {
      obj.gc();
    }
    runFunc("onDestroy");
    if (saynaa != null) {
      saynaa.close();
      saynaa = null;
      // } else if (saynaa != null) {
      //   saynaa.close();
    }
    super.onDestroy();
    System.gc();
  }

  public Intent registerReceiver(SaynaaBroadcastReceiver receiver, IntentFilter filter) {
    // TODO: Implement this method
    return super.registerReceiver(receiver, filter);
  }

  public Intent registerReceiver(SaynaaBroadcastReceiver.OnReceiveListener ltr, IntentFilter filter) {
    // TODO: Implement this method
    SaynaaBroadcastReceiver receiver = new SaynaaBroadcastReceiver(ltr);
    return super.registerReceiver(receiver, filter);
  }

  public Intent registerReceiver(IntentFilter filter) {
    // TODO: Implement this method
    if (mReceiver != null)
      unregisterReceiver(mReceiver);
    mReceiver = new SaynaaBroadcastReceiver(this);
    return super.registerReceiver(mReceiver, filter);
  }

  public SaynaaApplication getSaynaaApplication() {
    return (SaynaaApplication) getApplicationContext();
  }

  @Override
  public Object getSharedData(String key) {
    return SaynaaApplication.getInstance().getSharedData(key);
  }

  @Override
  public Object getSharedData(String key, Object def) {
    return SaynaaApplication.getInstance().getSharedData(key, def);
  }

  @Override
  public boolean setSharedData(String key, Object value) {
    return SaynaaApplication.getInstance().setSharedData(key, value);
  }

  public SaynaaModule getModule() {
    try {
      return saynaa.getMainModule();
    } catch (Exception e) {
      e.printStackTrace();
      sendError("getModule", e);
      return null;
    }
  }

  public Object testing() {
    return new JavaMethodBinding(this, "printf");
  }

  public static void printf(String msg) {
    Log.w(TAG, msg);
  }

  public ArrayList<ClassLoader> getClassLoaders() {
    if (dexLoader == null)
      return new ArrayList<>();
    return dexLoader.getClassLoaders();
  }

  public SaynaaDexClassLoader loadDex(String path) throws SaynaaException {
    if (dexLoader == null) {
      dexLoader = new SaynaaDexLoader(this, saynaaDir);
    }
    SaynaaDexClassLoader loader = dexLoader.loadDex(path);
    ReflectionFinder.setExtraClassLoaders(dexLoader.getClassLoaders());
    return loader;
  }

  public HashMap<String, String> getLibrarys() {
    if (dexLoader == null)
      return new HashMap<>();
    return dexLoader.getLibrarys();
  }

  @Override
  protected void onActivityResult(int requestCode, int resultCode, Intent data) {
    // TODO: Implement this method
    if (data != null) {
      String name = data.getStringExtra(NAME);
      if (name != null) {
        Object[] res = (Object[]) data.getSerializableExtra(DATA);
        if (res == null) {
          runFunc("onResult", name);
        } else {
          Object[] arg = new Object[res.length + 1];
          arg[0] = name;
          for (int i = 0; i < res.length; i++)
            arg[i + 1] = res[i];
          Object ret = runFunc("onResult", arg);
          if (ret != null && ret.getClass() == Boolean.class && (Boolean) ret)
            return;
        }
      }
    }
    runFunc("onActivityResult", requestCode, resultCode, data);
    super.onActivityResult(requestCode, resultCode, data);
  }

  @Override
  public boolean onKeyShortcut(int keyCode, KeyEvent event) {
    Object ret = runFunc("onKeyShortcut", keyCode, event);
    if (ret instanceof Boolean && (Boolean) ret)
      return true;
    return super.onKeyShortcut(keyCode, event);
  }

  @Override
  public void onBackPressed() {
    Object ret = runFunc("onBackPressed");
    if (ret instanceof Boolean && (Boolean) ret)
      return;
    super.onBackPressed();
  }

  @Override
  public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
    runFunc("onRequestPermissionsResult", requestCode, permissions, grantResults);
    super.onRequestPermissionsResult(requestCode, permissions, grantResults);
  }

  @Override
  public boolean onKeyDown(int keyCode, KeyEvent event) {
    Object ret = runFunc("onKeyDown", keyCode, event);
    if (ret instanceof Boolean && (Boolean) ret)
      return true;
    return super.onKeyDown(keyCode, event);
  }

  @Override
  public boolean onKeyUp(int keyCode, KeyEvent event) {
    Object ret = runFunc("onKeyUp", keyCode, event);
    if (ret instanceof Boolean && (Boolean) ret)
      return true;
    return super.onKeyUp(keyCode, event);
  }

  @Override
  public boolean onKeyLongPress(int keyCode, KeyEvent event) {
    Object ret = runFunc("onKeyLongPress", keyCode, event);
    if (ret instanceof Boolean && (Boolean) ret)
      return true;
    return super.onKeyLongPress(keyCode, event);
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    Object ret = runFunc("onTouchEvent", event);
    if (ret instanceof Boolean && (Boolean) ret)
      return true;
    return super.onTouchEvent(event);
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    optionsMenu = menu;
    Object ret = runFunc("onCreateOptionsMenu", menu);
    if (ret instanceof Boolean)
      return (Boolean) ret;
    return super.onCreateOptionsMenu(menu);
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item) {
    if (!item.hasSubMenu()) {
      Object ret = runFunc("onOptionsItemSelected", item);
      if (ret instanceof Boolean && (Boolean) ret)
        return true;
    }
    return super.onOptionsItemSelected(item);
  }

  @Override
  public boolean onMenuItemSelected(int featureId, MenuItem item) {
    if (!item.hasSubMenu()) {
      Object ret = runFunc("onMenuItemSelected", featureId, item);
      if (ret instanceof Boolean && (Boolean) ret)
        return true;
    }
    return super.onMenuItemSelected(featureId, item);
  }

  @Override
  public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
    runFunc("onCreateContextMenu", menu, v, menuInfo);
    super.onCreateContextMenu(menu, v, menuInfo);
  }

  @Override
  public boolean onContextItemSelected(MenuItem item) {
    Object ret = runFunc("onContextItemSelected", item);
    if (ret instanceof Boolean && (Boolean) ret)
      return true;
    return super.onContextItemSelected(item);
  }

  @Override
  public void setContentView(int layoutResID) {
    super.setContentView(layoutResID);
  }

  @Override
  public void setContentView(View view) {
    super.setContentView(view);
  }

  @Override
  public void setContentView(View view, ViewGroup.LayoutParams params) {
    super.setContentView(view, params);
  }

  public void setFragment(android.app.Fragment fragment) {
    getFragmentManager().beginTransaction().replace(android.R.id.content, fragment).commit();
  }

  public String getSaynaaPath() {
    Intent intent = getIntent();
    if (intent == null)
      return null;

    Uri uri = intent.getData();
    if (uri == null)
      return new File(localDir, "main.sa").getAbsolutePath();

    String path = uri.getPath();
    if (path == null || path.isEmpty())
      return new File(localDir, "main.sa").getAbsolutePath();

    File sf = new File(saynaaDir, path);

    if (!new File(path).exists() && sf.exists()) {
      path = sf.getAbsolutePath();
    }

    File f = new File(path);
    saynaaDir = f.getParentFile();

    return path;
  }

  public void call(String func) {
    push(2, func);
  }

  public void call(String func, Object[] args) {
    if (args.length == 0)
      push(2, func);
    else
      push(3, func, args);
  }

  public void set(String key, Object value) {
    if (key == null || key.trim().isEmpty())
      return;
    try {
      saynaa.setGlobal(key, value);
    } catch (Exception e) {
      sendMsg("set error: " + e.getMessage());
    }
  }

  public Object get(String key) {
    if (key == null || key.trim().isEmpty())
      return null;
    try {
      return saynaa.getGlobal(key);
    } catch (Exception e) {
      sendMsg("get error: " + e.getMessage());
      return null;
    }
  }

  public void push(int what, String s) {
    Message message = new Message();
    Bundle bundle = new Bundle();
    bundle.putString(DATA, s);
    message.setData(bundle);
    message.what = what;

    handler.sendMessage(message);
  }

  public void push(int what, String s, Object[] args) {
    Message message = new Message();
    Bundle bundle = new Bundle();
    bundle.putString(DATA, s);
    bundle.putSerializable("args", args);
    message.setData(bundle);
    message.what = what;

    handler.sendMessage(message);
  }

  public Object runFunc(String funcName, Object... args) {
    if (funcName == null || funcName.trim().isEmpty()) {
      return null;
    }

    try {
      int id = saynaa.getGlobalFunctionId(funcName);

      if (id != -1) {
        return saynaa.callFunctionById(id, args);
      }

    } catch (Exception e) {
      sendError("Hook error: " + funcName, e);
    } catch (Throwable t) {
      sendMsg("Hook error " + funcName + ": " + t.toString());
    }
    return null;
  }

  public void setDebugMode(boolean mode) {
    DebugMode = mode;
  }

  public void onNativeError(String msg) {
    if (msg == null || msg.trim().isEmpty()) {
      return;
    }

    String error = msg;
    if (!error.endsWith("\n")) {
      error += "\n";
    }

    if (DebugMode) {
      setContentView(layout);
      sendMsg(error);
    }
  }

  public void addActivityFlag(int flag) {
    activityFlags |= flag;
  }

  public void newActivity(String path, Object[] arg, boolean newDocument) {
    try {
      if (path == null || path.trim().isEmpty()) {
        sendMsg("newActivity error: empty path");
        return;
      }

      int flags = activityFlags;
      activityFlags = 0;

      Intent intent = new Intent(this, SaynaaActivity.class);
      intent.addFlags(flags);

      intent.putExtra(NAME, path);

      if (path.charAt(0) != '/') {
        path = saynaaDir.getAbsolutePath() + "/" + path;
      }

      File f = new File(path);
      if (f.isDirectory() && new File(path + "/main.sa").exists()) {
        path += "/main.sa";
      } else if ((f.isDirectory() || !f.exists()) && !path.endsWith(".sa")) {
        path += ".sa";
      }

      if (!new File(path).exists()) {
        sendMsg("newActivity error: file not found: " + path);
        return;
      }

      intent.setData(Uri.parse("file://" + path));

      if (arg != null) {
        intent.putExtra(ARG, arg);
      }

      if (newDocument) {
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
        intent.addFlags(Intent.FLAG_ACTIVITY_MULTIPLE_TASK);
      } else {
        intent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
      }

      startActivity(intent);
    } catch (Throwable t) {
      sendMsg("newActivity error: " + t.getMessage());
      Log.e(TAG, "newActivity failed", t);
    }
  }

  public void newActivity(String path) {
    newActivity(path, null, false);
  }

  public void newActivity(String path, Bundle arg) {
    try {
      if (path == null || path.trim().isEmpty()) {
        sendMsg("newActivity error: empty path");
        return;
      }

      int flags = activityFlags;
      activityFlags = 0;

      Intent intent = new Intent(this, SaynaaActivity.class);
      intent.addFlags(flags);

      intent.putExtra(NAME, path);

      if (path.charAt(0) != '/') {
        path = saynaaDir.getAbsolutePath() + "/" + path;
      }

      File f = new File(path);
      if (f.isDirectory() && new File(path + "/main.sa").exists()) {
        path += "/main.sa";
      } else if ((f.isDirectory() || !f.exists()) && !path.endsWith(".sa")) {
        path += ".sa";
      }

      if (!new File(path).exists()) {
        sendMsg("newActivity error: file not found: " + path);
        return;
      }

      intent.setData(Uri.parse("file://" + path));

      if (arg != null) {
        intent.putExtra(ARG, arg);
      }

      startActivity(intent);
    } catch (Throwable t) {
      sendMsg("newActivity error: " + t.getMessage());
      Log.e(TAG, "newActivity failed", t);
    }
  }

  public void newActivity(String path, Object[] arg) {
    newActivity(path, arg, false);
  }

  public Menu getOptionsMenu() {
    return optionsMenu;
  }

  private int getThemeColor(int attr) {
    TypedValue value = new TypedValue();
    getTheme().resolveAttribute(attr, value, true);
    return getResources().getColor(value.resourceId, getTheme());
  }

  @SuppressLint("ShowToast")
  public void showToast(String text) {
    long now = System.currentTimeMillis();
    if (toast == null || now - lastShow > 1000) {
      toastbuilder.setLength(0);
      toast = Toast.makeText(this, text, Toast.LENGTH_LONG);
      toastbuilder.append(text);
      toast.show();
    } else {
      toastbuilder.append("\n").append(text);
      toast.setText(toastbuilder.toString());
      toast.setDuration(Toast.LENGTH_LONG);
    }
    lastShow = now;
  }

  public void sendMsg(String msg) {
    Message message = new Message();
    Bundle bundle = new Bundle();
    bundle.putString(DATA, msg);
    message.setData(bundle);
    message.what = 0;
    handler.sendMessage(message);
    Log.i(TAG, msg);
  }

  @Override
  public void sendError(String title, Exception msg) {
    Object ret = runFunc("onError", title, msg);
    if (ret != null && ret.getClass() == Boolean.class && (Boolean) ret)
      return;
    else
      sendMsg(title + ": " + msg.getMessage());
  }

  @Override
  public Saynaa getSaynaa() {
    return saynaa;
  }

  @Override
  public Context getContext() {
    return this;
  }

  private void initUiShell() {
    handler = new MainHandler();

    layout = new LinearLayout(this);
    layout.setOrientation(LinearLayout.VERTICAL);

    ScrollView scroll = new ScrollView(this);
    scroll.setFillViewport(true);

    status = new TextView(this);
    status.setText("");
    status.setTextIsSelectable(true);
    scroll.addView(status, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                               ViewGroup.LayoutParams.WRAP_CONTENT));

    layout.addView(scroll, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                               ViewGroup.LayoutParams.WRAP_CONTENT));
  }

  private final class MainHandler extends Handler {
    @Override
    public void handleMessage(Message msg) {
      super.handleMessage(msg);
      if (msg.what == 0) {
        String data = msg.getData().getString(DATA);
        if (data == null) {
          data = "";
        }
        // Some sources send escaped newlines ("\\n") instead of real LF.
        // Normalize before rendering so TextView shows proper line breaks.
        data = data.replace("\\r\\n", "\n").replace("\\n", "\n").replace("\\r", "\n");
        status.setTextColor(getThemeColor(android.R.attr.textColorPrimary));
        status.append(data + "\n");
      }
    }
  }
}
