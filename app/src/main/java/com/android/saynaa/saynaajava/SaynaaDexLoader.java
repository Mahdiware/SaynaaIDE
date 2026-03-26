package com.android.saynaa.saynaajava;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.HashMap;

public class SaynaaDexLoader {
  private static final HashMap<String, SaynaaDexClassLoader> dexCache = new HashMap<>();
  private final ArrayList<ClassLoader> dexList = new ArrayList<>();
  private final HashMap<String, String> libCache = new HashMap<>();

  private final SaynaaContext context;
  private final String saynaaDir;
  private final String odexDir;

  public SaynaaDexLoader(SaynaaContext context) {
    this.context = context;
    this.saynaaDir = context.getSaynaaDir();
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
        PackageManager manager = context.getContext().getPackageManager();
        ApplicationInfo info = manager.getPackageInfo(pkg, 0).applicationInfo;
        dex = new SaynaaDexClassLoader(info.publicSourceDir, SaynaaApplication.getInstance().getOdexDir(),
            info.nativeLibraryDir, context.getContext().getClassLoader());
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
      if (f.getAbsolutePath().endsWith(".so")) {
        loadLib(f.getName());
      } else {
        loadDex(f.getAbsolutePath());
      }
    }
  }

  public void loadLib(String name) throws SaynaaException {
    String fn = name;
    int i = name.indexOf(".");
    if (i > 0) {
      fn = name.substring(0, i);
    }
    if (fn.startsWith("lib")) {
      fn = fn.substring(3);
    }

    String libDir = context.getContext().getDir(fn, Context.MODE_PRIVATE).getAbsolutePath();
    String libPath = libDir + "/lib" + fn + ".so";
    File target = new File(libPath);
    if (!target.exists()) {
      File source = new File(new File(saynaaDir, "libs"), "lib" + fn + ".so");
      if (!source.exists()) {
        throw new SaynaaException("can not find lib " + name);
      }
      try {
        copyFile(source, target);
      } catch (IOException e) {
        throw new SaynaaException(e);
      }
    }
    libCache.put(fn, libPath);
  }

  public SaynaaDexClassLoader loadDex(String path) throws SaynaaException {
    SaynaaDexClassLoader dex = dexCache.get(path);
    if (dex == null) {
      try {
        dex = loadApp(path);
      } catch (SaynaaException ignored) {
        // Not a package name; fall through to file path logic.
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
          path += ".dex";
        } else if (new File(path + ".jar").exists()) {
          path += ".jar";
        } else {
          throw new SaynaaException(path + " not found");
        }
      }

      dex = dexCache.get(name);
      if (dex == null) {
        dex = new SaynaaDexClassLoader(path, odexDir,
            SaynaaApplication.getInstance().getApplicationInfo().nativeLibraryDir,
            context.getContext().getClassLoader());
        dexCache.put(name, dex);
      }
    }

    if (!dexList.contains(dex)) {
      dexList.add(dex);
    }
    return dex;
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
    }
  }
}
