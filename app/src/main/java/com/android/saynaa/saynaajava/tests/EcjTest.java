package com.android.saynaa.saynaajava.tests;

import android.util.Log;
import java.io.ByteArrayOutputStream;
import java.io.PrintWriter;
import org.eclipse.jdt.internal.compiler.batch.Main;

public class EcjTest {
  private static final String TAG = "EcjTest";
  public static void compileHello(String assetsDir) {
    String androidJar = assetsDir + "/android/android.jar";
    String sourceFile = assetsDir + "/Hello.java";
    String outputDir = assetsDir + "/classes";

    try {
      ByteArrayOutputStream outStream = new ByteArrayOutputStream();
      ByteArrayOutputStream errStream = new ByteArrayOutputStream();

      PrintWriter out = new PrintWriter(outStream, true);
      PrintWriter err = new PrintWriter(errStream, true);

      Main compiler = new Main(out, err, false, null, null);

      String[] args = new String[] {

          "-1.8", "-source", "1.8", "-target", "1.8",

          "-nowarn", "-proc:none",

          // 🔥 CRITICAL: stop VM scanning
          "-bootclasspath", "",

          "-classpath", androidJar,

          "-d", outputDir,

          sourceFile};

      boolean result = compiler.compile(args, out, err, null);

      out.flush();
      err.flush();

      Log.d(TAG, "STDOUT:\n" + outStream.toString());
      Log.d(TAG, "STDERR:\n" + errStream.toString());
      Log.d(TAG, "Compilation result: " + result);

    } catch (Throwable t) {
      Log.e(TAG, "Compilation failed", t);
    }
  }
}