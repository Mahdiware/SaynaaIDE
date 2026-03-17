package com.android.saynaa.activity;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
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

import com.android.saynaa.saynaajava.Saynaa;
import com.android.saynaa.utils.FileUtil;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;

/**
 * SaynaaActivity is the Saynaa-native equivalent of AndSaynaa's SaynaaActivity.
 *
 * It keeps similar public properties and lifecycle shape, but drives execution
 * through the Saynaa runtime (not SaynaaState).
 */
public class SaynaaActivity extends Activity {

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
  protected ListView list;
  protected ArrayAdapter<String> adapter;
  protected LinearLayout layout;

  protected int mWidth;
  protected int mHeight;

  protected boolean isCreate = false;
  protected boolean isSetViewed = false;
  protected boolean mDebug = true;

  protected StringBuilder toastbuilder = new StringBuilder();
  protected Toast toast;
  protected long lastShow;

  protected Menu optionsMenu;

  // Saynaa runtime
  protected Saynaa saynaa;
  protected String source;

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

      saynaa = new Saynaa(this);
      saynaa.setSource(source);
      saynaa.run();

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

      if (!isSetViewed) {
        setContentView(layout);
      }
    } catch (Throwable t) {
      Log.e(TAG, "onCreate failed", t);
      sendMsg("onCreate error: " + t.getMessage());
      setContentView(layout);
    }
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
    runFunc("onDestroy");
    if (saynaa != null) {
      saynaa.close();
      saynaa = null;
    }
    super.onDestroy();
  }

  @Override
  public boolean onKeyShortcut(int keyCode, KeyEvent event) {
    runFunc("onKeyShortcut", keyCode, event);
    return super.onKeyShortcut(keyCode, event);
  }

  @Override
  public boolean onKeyDown(int keyCode, KeyEvent event) {
    runFunc("onKeyDown", keyCode, event);
    return super.onKeyDown(keyCode, event);
  }

  @Override
  public boolean onKeyUp(int keyCode, KeyEvent event) {
    runFunc("onKeyUp", keyCode, event);
    return super.onKeyUp(keyCode, event);
  }

  @Override
  public boolean onKeyLongPress(int keyCode, KeyEvent event) {
    runFunc("onKeyLongPress", keyCode, event);
    return super.onKeyLongPress(keyCode, event);
  }

  @Override
  public boolean onTouchEvent(MotionEvent event) {
    runFunc("onTouchEvent", event);
    return super.onTouchEvent(event);
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    optionsMenu = menu;
    runFunc("onCreateOptionsMenu", menu);
    return super.onCreateOptionsMenu(menu);
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item) {
    runFunc("onOptionsItemSelected", item);
    return super.onOptionsItemSelected(item);
  }

  @Override
  public boolean onMenuItemSelected(int featureId, MenuItem item) {
    runFunc("onMenuItemSelected", featureId, item);
    return super.onMenuItemSelected(featureId, item);
  }

  @Override
  public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
    runFunc("onCreateContextMenu", menu, v, menuInfo);
    super.onCreateContextMenu(menu, v, menuInfo);
  }

  @Override
  public boolean onContextItemSelected(MenuItem item) {
    runFunc("onContextItemSelected", item);
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
      if (saynaa == null) {
        saynaa = new Saynaa(this);
      }

      if (filePath.charAt(0) != '/') {
        filePath = getSaynaaDir() + "/" + filePath;
      }

      File f = new File(filePath);
      if (!f.exists()) {
        throw new FileNotFoundException(filePath);
      }

      int result = saynaa.doFile(filePath);
      if (result != 0) {
        sendMsg("doFile runtime error: " + result + " @ " + filePath);
      }

      return null;
    } catch (Throwable t) {
      sendMsg("doFile error: " + t.getMessage());
      return null;
    }
  }

  public Object runFunc(String funcName, Object... args) {
    if (saynaa == null || funcName == null || funcName.trim().isEmpty()) {
      return null;
    }

    // Avoid invoking undefined hooks. Saynaa logs compile/runtime errors for
    // unknown identifiers, so we gate calls using source-level detection.
    if (!hasScriptHook(funcName)) {
      return null;
    }

    try {
      int result = saynaa.pcall(funcName, args);
      if (result != 0 && mDebug) {
        Log.w(TAG, "runFunc non-zero result for hook=" + funcName + ": " + result);
      }
    } catch (Throwable t) {
      if (mDebug) {
        Log.w(TAG, "runFunc failed for hook=" + funcName, t);
      }
    }
    return null;
  }

  private boolean hasScriptHook(String funcName) {
    if (source == null || source.isEmpty()) {
      return false;
    }

    // Supported forms:
    // 1) function onCreate(...)
    // 2) onCreate = function(...)
    final String directDecl = "function " + funcName + "(";
    final String assignedDecl = funcName + " = function(";

    return source.contains(directDecl) || source.contains(assignedDecl);
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
    scroll.addView(status,
        new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

    list = new ListView(this);
    list.setFastScrollEnabled(true);
    adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, new ArrayList<String>());
    list.setAdapter(adapter);

    layout.addView(scroll,
        new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
    layout.addView(list,
        new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
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
        if (mDebug) {
          showToast(data);
        }
        status.append(data + "\n");
        adapter.add(data);
      }
    }
  }
}
