package com.saynaa.saynaajava;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HashMap;

public class SaynaaDexLoader {
  private static final HashMap<String, SaynaaDexClassLoader> dexCache = new HashMap<>();
  private final ArrayList<ClassLoader> dexList = new ArrayList<>();
  private final HashMap<String, String> libCache = new HashMap<>();

  private final Context context;
  private final String saynaaDir;
  private final String odexDir;

  public SaynaaDexLoader(Context context) {
    this.context = context;
    this.saynaaDir = context.getApplicationContext().getFilesDir().getAbsolutePath();
    this.odexDir = SaynaaApplication.getInstance().getOdexDir();
  }

  public ArrayList<ClassLoader> getClassLoaders() {
    return dexList;
  }

  public HashMap<String, String> getLibrarys() {
    return libCache;
  }

  public SaynaaDexClassLoader loadApp(String pkg) throws SaynaaException {
    try {
      SaynaaDexClassLoader dex = dexCache.get(pkg);
      if (dex == null) {
        PackageManager manager = context.getApplicationContext().getPackageManager();
        ApplicationInfo info = manager.getPackageInfo(pkg, 0).applicationInfo;
        dex = new SaynaaDexClassLoader(info.publicSourceDir, SaynaaApplication.getInstance().getOdexDir(),
            info.nativeLibraryDir, context.getApplicationContext().getClassLoader());
        dexCache.put(pkg, dex);
      }
      if (!dexList.contains(dex)) {
        dexList.add(dex);
      }
      return dex;
    } catch (PackageManager.NameNotFoundException e) {
      throw new SaynaaException(e);
    }
  }

  public void loadLibs() throws SaynaaException {
    File libsDir = new File(saynaaDir, "libs");
    File[] libs = libsDir.listFiles();
    if (libs == null) {
      return;
    }
    for (File f : libs) {
      if (f.isDirectory()) {
        continue;
      }
      loadDex(f.getAbsolutePath());
    }
  }

  /**
   * Loads a dex/jar file.
   *
   * Android 14+ require dynamically loaded code to come from
   * the app's private storage. This function copies the dex/jar into the app's
   * code cache directory, marks it read-only, and then loads it.
   */
  public SaynaaDexClassLoader loadDex(String path) throws SaynaaException {
    SaynaaDexClassLoader dex = dexCache.get(path);

    if (dex == null) {
      try {
        dex = loadApp(path);
      } catch (SaynaaException ignored) {
      }
    }

    if (dex == null) {
      String name = path;

      if (path.charAt(0) != '/') {
        path = new File(saynaaDir, path).getAbsolutePath();
      }

      File dexFile = new File(path);

      if (!dexFile.exists()) {
        if (new File(path + ".dex").exists()) {
          dexFile = new File(path + ".dex");
        } else if (new File(path + ".jar").exists()) {
          dexFile = new File(path + ".jar");
        } else {
          throw new SaynaaException(path + " not found");
        }
      }

      /*
      ** Android 14+:
      ** Load dynamic code from the app's private code cache directory.
      */
      File codeCache = context.getApplicationContext().getCodeCacheDir();

      File internalDex = new File(codeCache, getDexCacheName(dexFile));

      if (!internalDex.exists()) {
        try {
          copyFile(dexFile, internalDex);
          internalDex.setReadOnly();
        } catch (IOException e) {
          throw new SaynaaException(e);
        }
      }

      dex = dexCache.get(name);

      if (dex == null) {
        dex = new SaynaaDexClassLoader(internalDex.getAbsolutePath(), null,
            SaynaaApplication.getInstance().getApplicationInfo().nativeLibraryDir,
            context.getApplicationContext().getClassLoader());

        dexCache.put(name, dex);
      }
    }

    if (!dexList.contains(dex)) {
      dexList.add(dex);
    }

    return dex;
  }

  private static String getDexCacheName(File file) {
    try {
      MessageDigest md = MessageDigest.getInstance("SHA-256");

      String key = file.getAbsolutePath() + ":" + file.length() + ":" + file.lastModified();

      byte[] hash = md.digest(key.getBytes(StandardCharsets.UTF_8));

      StringBuilder sb = new StringBuilder();
      for (byte b : hash) {
        sb.append(String.format("%02x", b));
      }

      String ext = file.getName().endsWith(".jar") ? ".jar" : ".dex";
      return sb.toString() + ext;

    } catch (Exception e) {
      throw new RuntimeException(e);
    }
  }

  private static void copyFile(File source, File target) throws IOException {
    File parent = target.getParentFile();
    if (parent != null && !parent.exists()) {
      parent.mkdirs();
    }

    try (FileInputStream in = new FileInputStream(source); FileOutputStream out = new FileOutputStream(target)) {
      byte[] buffer = new byte[8192];
      int read;

      while ((read = in.read(buffer)) != -1) {
        out.write(buffer, 0, read);
      }

      out.flush();
    }

    // Preserve the source file timestamp.
    target.setLastModified(source.lastModified());
  }
}
