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
    // Check if this dex is already loaded.
    SaynaaDexClassLoader dex = dexCache.get(path);

    if (dex == null) {
      try {
        // Try loading as an installed application package.
        dex = loadApp(path);
      } catch (SaynaaException ignored) {
        // Not a package name.
      }
    }

    if (dex == null) {
      String name = path;

      // Convert relative path to an absolute path.
      if (path.charAt(0) != '/') {
        path = new File(saynaaDir, path).getAbsolutePath();
      }

      File dexFile = new File(path);

      // Try common extensions if the file doesn't exist.
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
       * Android 14+:
       * Dynamic code must be loaded from the application's private storage.
       * Copy the file into codeCacheDir and load it from there.
       */
      File codeCache = context.getContext().getCodeCacheDir();

      // Keep the original extension.
      String ext = dexFile.getName().endsWith(".jar") ? ".jar" : ".dex";

      // Generate a unique filename to avoid collisions.
      File internalDex = new File(codeCache, "dex_" + System.nanoTime() + "_" + Integer.toHexString((int) (Math.random() * Integer.MAX_VALUE)) + ext);

      try {
        // Copy the dex/jar into the private code cache.
        copyFile(dexFile, internalDex);

        /*
         * Android 14+:
         * The copied file should be read-only before loading.
         */
        internalDex.setReadOnly();
      } catch (Exception e) {
        throw new SaynaaException(e);
      }

      // Load the copied dex.
      dex = new SaynaaDexClassLoader(internalDex.getAbsolutePath(), null, SaynaaApplication.getInstance().getApplicationInfo().nativeLibraryDir, context.getContext().getClassLoader());

      dexCache.put(name, dex);
    }

    // Keep track of loaded class loaders.
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

    try (FileInputStream in = new FileInputStream(source);
      FileOutputStream out = new FileOutputStream(target)) {

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
