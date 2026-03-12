package com.android.saynaa.activity;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.util.Log;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;
import com.android.saynaa.saynaajava.JavaBridge;
import com.android.saynaa.saynaajava.Saynaa;
import com.android.saynaa.utils.FileUtil;

public class MainActivity extends Activity {
  private String Files;
  private static final String TAG = "SaynaaMain";

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    FileUtil.copyAllAssets(this);

    // Initialize the files directory path
    Files = getFilesDir().getAbsolutePath() + "/";
    Log.i(TAG, "Files directory: " + Files);

    String src = FileUtil.readFile(Files, "main.sa");
    Log.i(TAG, "main.sa loaded: " + (src != null) + ", length=" + (src == null ? 0 : src.length()));

    if (src == null || src.trim().isEmpty()) {
      TextView tv = new TextView(this);
      tv.setText("main.sa not found or empty in internal files.");
      setContentView(tv);
      return;
    }

    try {
      Saynaa saynaa = new Saynaa(this);
      saynaa.setSource(src);
      saynaa.run();
    } catch (Throwable t) {
      Log.e(TAG, "Saynaa run failed", t);
      TextView tv = new TextView(this);
      tv.setText("Saynaa run failed: " + t.getMessage());
      setContentView(tv);
    }
  }
}
