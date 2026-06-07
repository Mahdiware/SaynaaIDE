package com.android.saynaa.activity;

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
import com.android.saynaa.saynaajava.*;
import com.android.saynaa.utils.FileUtil;
import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

/**
 * SaynaaActivity is the main entry point for Saynaa scripts. It initializes the
 */
public class SaynaaActivity extends Activity implements SaynaaBroadcastReceiver.OnReceiveListener, SaynaaContext {
  public static final String ARG = "arg";
  public static final String DATA = "data";
  public static final String NAME = "name";
  private static final String TAG = "SaynaaActivity";

  // Compatibility-style properties (mirroring SaynaaActivity naming where possible)
  private static final ArrayList<String> prjCache = new ArrayList<>();
  protected String saynaaDir;
  protected String saynaaPath;
  protected String saynaaExtDir;
  protected String localDir;
  protected String pageName = "main";

  protected Handler handler;
  protected TextView status;
  protected LinearLayout layout;

  protected int mWidth;
  protected int mHeight;

  protected boolean isCreate = false;
  protected boolean isSetViewed = false;
  protected boolean mDebug = true;

  private ArrayList<SaynaaGcable> gclist = new ArrayList<SaynaaGcable>();

  private SaynaaBroadcastReceiver mReceiver;

  protected StringBuilder toastbuilder = new StringBuilder();
  protected Toast toast;
  protected long lastShow;

  protected Menu optionsMenu;

  // Saynaa runtime
  protected Saynaa saynaa;
  protected SaynaaState saynaaState;
  protected String source;
  protected StringBuilder nativeErrorBuffer = new StringBuilder();

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

    initDisplayMetrics();
    initUiShell();

    localDir = getFilesDir().getAbsolutePath();
    saynaaExtDir = getDefaultExtDir();

    FileUtil.copyAllAssets(this);

    try {
      saynaaPath = getSaynaaPath();
      if (saynaaPath == null || saynaaPath.trim().isEmpty()) {
        saynaaPath = new File(localDir, "main.sa").getAbsolutePath();
      }

      File f = new File(saynaaPath);
      pageName = f.getName();
      int idx = pageName.lastIndexOf('.');
      if (idx > 0) {
        pageName = pageName.substring(0, idx);
      }
      saynaaDir = f.getParent();

      source = FileUtil.readFile((saynaaDir == null ? localDir : saynaaDir) + "/", f.getName());
      if (source == null) {
        source = FileUtil.readFile(localDir + "/", "main.sa");
      }

      if (source == null || source.trim().isEmpty()) {
        sendMsg("Script not found or empty: " + saynaaPath);
        setContentView(layout);
        return;
      }

      initSaynaa();
      dexLoader = new SaynaaDexLoader(this);
      dexLoader.loadLibs();
      JavaBridge.setExtraClassLoaders(dexLoader.getClassLoaders());
      File initFile = new File(saynaaDir == null ? localDir : saynaaDir, "init.sa");
      if (initFile.exists()) {
        int initResult = saynaaState.runFile(initFile.getAbsolutePath());
        if (initResult != 0) {
          showScriptError(errorReason(initResult),
              "Startup failed @ " + initFile.getAbsolutePath() + "\n" + drainNativeErrors());
          setContentView(layout);
          return;
        }
      }
      int result = saynaaState.runFile(saynaaPath);
      if (result != 0) {
        showScriptError(errorReason(result), "Startup failed @ " + saynaaPath + "\n" + drainNativeErrors());
        setContentView(layout);
        return;
      }

      String startupErr = drainNativeErrors();
      if (!startupErr.isEmpty()) {
        showScriptError("Startup error", startupErr);
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
        if (!"main".equals(pageName)) {
          runFunc("main", launchArgs);
        }
        runFunc(pageName, launchArgs);
      } else if (launchBundle != null) {
        if (!"main".equals(pageName)) {
          runFunc("main", launchBundle);
        }
        runFunc(pageName, launchBundle);
      } else {
        if (!"main".equals(pageName)) {
          runFunc("main");
        }
        runFunc(pageName);
      }
      if (launchArgs != null && launchArgs.length > 0) {
        runFunc("onCreate", launchArgs);
      } else if (launchBundle != null) {
        runFunc("onCreate", launchBundle);
      } else {
        runFunc("onCreate", savedInstanceState != null ? savedInstanceState : new Bundle());
      }

      if (!isSetViewed) {
        setContentView(layout);
      }
    } catch (Throwable t) {
      Log.e(TAG, "onCreate failed", t);
      showScriptError("onCreate error", t.getMessage());
      setContentView(layout);
    }
    //runTest();
  }

  // public void runTest() {
  //   try {
  //     SaynaaModule module2 = saynaaState.newModule("hellomodule");

  //     File testFile = new File(saynaaDir == null ? localDir : saynaaDir, "test.sa");
  //     int result = saynaaState.runFile(module2, testFile.getAbsolutePath());
  //     // print result
  //     if (result != 0) {
  //       showScriptError(errorReason(result),
  //           "Failed to run test.sa @ " + testFile.getAbsolutePath() + "\n" + drainNativeErrors());
  //     }
  //   } catch (Exception e) {
  //     Log.e(TAG, "runTest failed", e);
  //     showScriptError("runTest error", e.getMessage());
  //   }
  // }

  @Override
  public void onReceive(Context context, Intent intent) {
    runFunc("onReceive", context, intent);
  }

  @Override
  protected void onStart() {
    super.onStart();
    runFunc("onStart");
  }

  @Override
  protected void onResume() {
    super.onResume();
    runFunc("onResume");
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
    if (saynaaState != null) {
      saynaaState.close();
      saynaaState = null;
      saynaa = null;
    } else if (saynaa != null) {
      saynaa.close();
      saynaa = null;
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

  @Override
  public Object getSharedData(String key) {
    return SaynaaApplication.getInstance().getSharedData(key);
  }

  @Override
  public Object getSharedData(String key, Object def) {
    return SaynaaApplication.getInstance().getSharedData(key, def);
  }

  @Override
  public Map getGlobalData() {
    return ((SaynaaApplication) getApplication()).getGlobalData();
  }

  @Override
  public boolean setSharedData(String key, Object value) {
    return SaynaaApplication.getInstance().setSharedData(key, value);
  }

  public Object getModule() {
    try {
      return saynaaState.getModule();
    } catch (SaynaaException e) {
      e.printStackTrace();
      return null;
    }
  }

  private void initSaynaa() {
    saynaaState = getOrCreateState();
    saynaa = saynaaState.getSaynaa();
    ArrayList<Object> javaList = new ArrayList<>();
    javaList.add("alpha");
    javaList.add(123);
    HashMap<String, Object> javaMap = new HashMap<>();
    javaMap.put("flag", true);
    javaMap.put("count", 7);
    try {
      saynaaState.setGlobalValue("java_pushed_number", 42, false);
      saynaaState.setGlobalValue("java_pushed_list_obj", javaList, false);
      saynaaState.setGlobalValue("java_pushed_list_saynaa", javaList, true);
      saynaaState.setGlobalValue("java_pushed_map_saynaa", javaMap, true);
      saynaaState.setGlobal("activity", this);

      // SaynaaModule module = saynaaState.newModule("mahdiware");
      // saynaaState.moduleSetGlobal(module, "version", "1.0");
      // saynaaState.moduleSetGlobal(module, "printf", this, "printf");
      // saynaaState.registerModule(module);

    } catch (SaynaaException e) {
      sendMsg("initSaynaa error: " + e.getMessage());
    }
  }

  // public void printf(String msg) {
  //   Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
  // }

  public ArrayList<ClassLoader> getClassLoaders() {
    if (dexLoader == null)
      return new ArrayList<>();
    return dexLoader.getClassLoaders();
  }

  public SaynaaDexClassLoader loadDex(String path) throws SaynaaException {
    if (dexLoader == null) {
      dexLoader = new SaynaaDexLoader(this);
    }
    SaynaaDexClassLoader loader = dexLoader.loadDex(path);
    JavaBridge.setExtraClassLoaders(dexLoader.getClassLoaders());
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
    isSetViewed = true;
    super.setContentView(layoutResID);
  }

  @Override
  public void setContentView(View view) {
    isSetViewed = true;
    super.setContentView(view);
  }

  @Override
  public void setContentView(View view, ViewGroup.LayoutParams params) {
    isSetViewed = true;
    super.setContentView(view, params);
  }

  public void setFragment(android.app.Fragment fragment) {
    isSetViewed = true;
    getFragmentManager().beginTransaction().replace(android.R.id.content, fragment).commit();
  }

  @Override
  public void regGc(SaynaaGcable obj) {
    gclist.add(obj);
  }

  public String getSaynaaPath() {
    Intent intent = getIntent();
    if (intent == null)
      return "";

    Uri uri = intent.getData();
    if (uri == null)
      return new File(getFilesDir(), "main.sa").getAbsolutePath();

    String path = uri.getPath();
    if (path == null || path.isEmpty())
      return new File(getFilesDir(), "main.sa").getAbsolutePath();

    if (!new File(path).exists() && new File(getSaynaaPath(path)).exists()) {
      path = getSaynaaPath(path);
    }

    File f = new File(path);
    saynaaDir = f.getParent();

    if (f.getName().equals("main.sa") && saynaaDir != null) {
      if (!prjCache.contains(saynaaDir)) {
        prjCache.add(saynaaDir);
      }
    }

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
      getOrCreateState().setGlobal(key, value);
    } catch (SaynaaException e) {
      sendMsg("set error: " + e.getMessage());
    }
  }

  public Object get(String key) {
    if (key == null || key.trim().isEmpty())
      return null;
    try {
      return getOrCreateState().getGlobal(key);
    } catch (SaynaaException e) {
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

  public String getSaynaaPath(String path) {
    return new File(getSaynaaDir(), path).getAbsolutePath();
  }

  public String getSaynaaPath(String dir, String name) {
    return new File(getSaynaaDir(dir), name).getAbsolutePath();
  }

  public String getSaynaaDir() {
    if (saynaaDir == null || saynaaDir.isEmpty()) {
      saynaaDir = getFilesDir().getAbsolutePath();
    }
    return saynaaDir;
  }

  public void setSaynaaDir(String dir) {
    saynaaDir = dir;
  }

  public String getSaynaaDir(String name) {
    File dir = new File(getSaynaaDir(), name);
    if (!dir.exists() && !dir.mkdirs()) {
      return null;
    }
    return dir.getAbsolutePath();
  }

  public String getSaynaaExtDir() {
    if (saynaaExtDir == null || saynaaExtDir.isEmpty()) {
      saynaaExtDir = getDefaultExtDir();
    }
    return saynaaExtDir;
  }

  public void setSaynaaExtDir(String dir) {
    if (Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
      saynaaExtDir = new File(Environment.getExternalStorageDirectory(), dir).getAbsolutePath();
    } else {
      saynaaExtDir = getDir(dir, MODE_PRIVATE).getAbsolutePath();
    }
    File d = new File(saynaaExtDir);
    if (!d.exists()) {
      d.mkdirs();
    }
  }

  public String getSaynaaExtDir(String name) {
    File dir = new File(getSaynaaExtDir(), name);
    if (!dir.exists() && !dir.mkdirs()) {
      return null;
    }
    return dir.getAbsolutePath();
  }

  public Object doFile(String filePath, Object[] args) {
    try {
      getOrCreateState();

      if (filePath.charAt(0) != '/') {
        filePath = getSaynaaDir() + "/" + filePath;
      }

      File f = new File(filePath);
      if (!f.exists()) {
        throw new FileNotFoundException(filePath);
      }

      int result = saynaaState.runFile(filePath);
      if (result != 0) {
        showScriptError(errorReason(result), "doFile failed @ " + filePath + "\n" + drainNativeErrors());
      }

      return null;
    } catch (SaynaaException e) {
      showScriptError("doFile error", e.getMessage());
      return null;
    } catch (Throwable t) {
      showScriptError("doFile error", t.getMessage());
      return null;
    }
  }

  public Object runFunc(String funcName, Object... args) {
    if (funcName == null || funcName.trim().isEmpty()) {
      return null;
    }

    try {
      PCallResult result = getOrCreateState().pcall(funcName, args);
      if (!result.success && mDebug) {
        Log.w(TAG, "runFunc non-zero result for hook=" + funcName + ": " + result.message);
        showScriptError(result.message, "Hook failed: " + funcName + "\n" + drainNativeErrors());
        return null;
      }
      return result.value;
    } catch (SaynaaException e) {
      if (mDebug) {
        Log.w(TAG, "runFunc failed for hook=" + funcName, e);
      }
      showScriptError("Hook error", funcName + ": " + e.getMessage());
    } catch (Throwable t) {
      if (mDebug) {
        Log.w(TAG, "runFunc failed for hook=" + funcName, t);
      }
      showScriptError("Hook error", funcName + ": " + t.getMessage());
    }
    return null;
  }

  public void onNativeError(String msg) {
    if (msg == null || msg.trim().isEmpty()) {
      return;
    }
    synchronized (nativeErrorBuffer) {
      nativeErrorBuffer.append(msg);
      if (!msg.endsWith("\n")) {
        nativeErrorBuffer.append('\n');
      }
    }
    if (!isSetViewed) {
      setContentView(layout);
    }
  }

  private String drainNativeErrors() {
    synchronized (nativeErrorBuffer) {
      String out = nativeErrorBuffer.toString().trim();
      nativeErrorBuffer.setLength(0);
      return out;
    }
  }

  private void showScriptError(String title, String detail) {
    String t = (title == null || title.trim().isEmpty()) ? "Script error" : title;
    String d = (detail == null) ? "<no details>" : detail;
    FileUtil.saveDebug(this, t + ": " + d);
    if (!isSetViewed) {
      setTitle(t);
      setContentView(layout);
    }
    sendMsg(t + "\n" + d);
  }

  private String errorReason(int error) {
    switch (error) {
    case 6:
      return "Error";
    case 5:
      return "GC error";
    case 4:
      return "Out of memory";
    case 3:
      return "Syntax error";
    case 2:
      return "Runtime error";
    case 1:
      return "Yield error";
    default:
      return "Unknown error " + error;
    }
  }

  public void newActivity(String path, Object[] arg, boolean newDocument) {
    try {
      if (path == null || path.trim().isEmpty()) {
        sendMsg("newActivity error: empty path");
        return;
      }

      Intent intent = new Intent(this, SaynaaActivity.class);
      intent.putExtra(NAME, path);

      if (path.charAt(0) != '/') {
        path = getSaynaaDir() + "/" + path;
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

      Intent intent = new Intent(this, SaynaaActivity.class);
      intent.putExtra(NAME, path);

      if (path.charAt(0) != '/') {
        path = getSaynaaDir() + "/" + path;
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
  public SaynaaState getSaynaaState() {
    return getOrCreateState();
  }

  private SaynaaState getOrCreateState() {
    if (saynaaState == null || saynaaState.isClosed()) {
      saynaaState = SaynaaStateFactory.newState(this);
    }
    saynaa = saynaaState.getSaynaa();
    return saynaaState;
  }

  @Override
  public Context getContext() {
    return this;
  }

  public int getWidth() {
    return mWidth;
  }

  public int getHeight() {
    return mHeight;
  }

  private void initDisplayMetrics() {
    WindowManager wm = (WindowManager) getSystemService(WINDOW_SERVICE);
    DisplayMetrics outMetrics = new DisplayMetrics();
    wm.getDefaultDisplay().getMetrics(outMetrics);
    mWidth = outMetrics.widthPixels;
    mHeight = outMetrics.heightPixels;
  }

  private void initUiShell() {
    handler = new MainHandler();

    layout = new LinearLayout(this);
    layout.setOrientation(LinearLayout.VERTICAL);

    ScrollView scroll = new ScrollView(this);
    scroll.setFillViewport(true);

    status = new TextView(this);
    status.setTextColor(Color.BLACK);
    status.setText("");
    status.setTextIsSelectable(true);
    scroll.addView(status, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                               ViewGroup.LayoutParams.WRAP_CONTENT));

    layout.addView(scroll, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                               ViewGroup.LayoutParams.WRAP_CONTENT));
  }

  private String getDefaultExtDir() {
    if (Environment.getExternalStorageState().equals(Environment.MEDIA_MOUNTED)) {
      return new File(Environment.getExternalStorageDirectory(), "saynaa").getAbsolutePath();
    }
    return getDir("saynaa", MODE_PRIVATE).getAbsolutePath();
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

        status.append(data + "\n");
      }
    }
  }
}
