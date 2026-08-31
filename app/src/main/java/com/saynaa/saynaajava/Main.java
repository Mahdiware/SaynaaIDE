package com.saynaa.saynaajava;

import android.content.Context;
import android.util.Log;
import com.saynaa.saynaajava.*;
import com.saynaa.saynaajava.datatype.*;
import com.saynaa.saynaajava.reflection.*;
import com.saynaa.utils.FileUtil;
import java.io.File;

public class Main {
  public static void run(Context context) {
    File localDir = context.getDir("saynaa", Context.MODE_PRIVATE);
    String saynaaPath = new File(localDir, "main.sa").getAbsolutePath();

    FileUtil.installSaynaaCode(context, localDir);

    if (new File(saynaaPath).exists() == false) {
      Log.e("Saynaa", "Saynaa file not found: " + saynaaPath);
      return;
    }

    try {
      Saynaa saynaa = new Saynaa(context);
      saynaa.setSaynaaDir(localDir);
      saynaa.setGlobal("activity", context);
      saynaa.setGlobal("context", context);

      new JavaModule(saynaa).create();

      SaynaaDexLoader dexLoader = saynaa.getDexLoader();
      dexLoader.loadLibs();
      ReflectionFinder.setExtraClassLoaders(dexLoader.getClassLoaders());

      File initFile = new File(localDir, "init.sa");
      if (initFile.exists()) {
        int initResult = saynaa.runFile(initFile.getAbsolutePath());
        if (initResult != 0) {
          Log.e("Saynaa", "Error running init.sa: " + initResult);
        }
      }
      int result = saynaa.runFile(saynaaPath);
      if (result != 0) {
        Log.e("Saynaa", "Error running Saynaa file: " + saynaaPath + ", result code: " + result);
      }

      runFunction(saynaa, "onCreate", context);
    } catch (Exception e) {
      Log.e("Saynaa", "Error running Saynaa file: " + saynaaPath, e);
      e.printStackTrace();
    }
  }

  public static Object runFunction(Saynaa saynaa, String funcName, Object... args) {
    if (funcName == null || funcName.trim().isEmpty()) {
      return null;
    }

    try {
      int id = saynaa.getGlobalFunctionId(funcName);

      if (id != -1) {
        return saynaa.callFunctionById(id, args);
      }

    } catch (Exception e) {
      Log.e("Saynaa", "Hook error: " + funcName, e);
    } catch (Throwable t) {
      Log.e("Saynaa", "Hook error " + funcName + ": " + t.toString());
    }
    return null;
  }
}