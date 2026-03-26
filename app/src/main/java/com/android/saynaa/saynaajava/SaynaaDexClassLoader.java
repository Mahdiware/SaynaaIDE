package com.android.saynaa.saynaajava;

import dalvik.system.DexClassLoader;
import java.util.HashMap;

public class SaynaaDexClassLoader extends DexClassLoader {
  private final HashMap<String, Class<?>> classCache = new HashMap<>();
  private final String dexPath;

  public SaynaaDexClassLoader(String dexPath, String optimizedDirectory, String libraryPath, ClassLoader parent) {
    super(dexPath, optimizedDirectory, libraryPath, parent);
    this.dexPath = dexPath;
  }

  public String getDexPath() {
    return dexPath;
  }

  @Override
  protected Class<?> findClass(String name) throws ClassNotFoundException {
    Class<?> cached = classCache.get(name);
    if (cached != null) {
      return cached;
    }
    Class<?> cls = super.findClass(name);
    classCache.put(name, cls);
    return cls;
  }
}
