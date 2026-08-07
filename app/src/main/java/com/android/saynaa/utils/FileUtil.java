package com.android.saynaa.utils;

import android.content.Context;
import android.os.Environment;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

public class FileUtil {
  public final static String SDCARD_PATH = "/sdcard/";

  public static void saveDebug(Context context, String content) {
    try {
      File outDir = context.getFilesDir();
      if (outDir == null)
        return;

      if (!outDir.exists() && !outDir.mkdirs())
        return;

      File outFile = new File(outDir, "debug.txt");
      try (FileOutputStream output = new FileOutputStream(outFile, true)) {
        output.write((content + "\n").getBytes());
        output.flush();
      }
    } catch (IOException e) {
      e.printStackTrace();
    }
  }
  
  /**
   * Copy all assets to internal storage.
   */
  public static void copyAllAssets(Context context) {
    try {
      PackageInfo info = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
      long updateTime = info.lastUpdateTime;
      SharedPreferences prefs = context.getSharedPreferences("assets", Context.MODE_PRIVATE);
      long savedTime = prefs.getLong("lastUpdateTime", -1);

      if (savedTime == updateTime) {
        return;
      }

      File files = context.getFilesDir();
      deleteRecursive(files);

      if (!files.exists()) {
        files.mkdirs();
      }

      copyAssetFolder(context, "", files);

      prefs.edit().putLong("lastUpdateTime", updateTime).apply();
    } catch (Exception e) {
      e.printStackTrace();
    }
  }
  
  public static void deleteRecursive(File file) {
    if (file == null || !file.exists()) {
      return;
    }

    if (file.isDirectory()) {
      File[] children = file.listFiles();
      if (children != null) {
        for (File child : children) {
          deleteRecursive(child);
        }
      }
    }

    file.delete();
  }
  
  public static List<File> listDir(String path) {
    List<File> files = new ArrayList<>();
    
    File dir = new File(path);
    if (!dir.exists() || !dir.isDirectory()) {
      return files;
    }
    
    File[] list = dir.listFiles();
    if (list == null) {
      return files;
    }
    
    Collections.addAll(files, list);
    
    Collections.sort(files, new Comparator<File>() {
      @Override
      public int compare(File a, File b) {
        if (a.isDirectory() && !b.isDirectory()) return -1;
        if (!a.isDirectory() && b.isDirectory()) return 1;
        return a.getName().compareToIgnoreCase(b.getName());
      }
    });
    
    return files;
  }
  
  public static String formatDate(long unixTimestamp, String format) {
    SimpleDateFormat sdf = new SimpleDateFormat(format, Locale.getDefault());
    sdf.setTimeZone(TimeZone.getDefault());

    return sdf.format(new Date(unixTimestamp * 1000L));
  }

  /*
    * Unzip a ZIP file from assets to the specified output directory.
    * @param assetName name of the ZIP file in assets
    * @param outputDirectory path to the output directory in internal storage
    * @throws IOException if an I/O error occurs
   */
  public void unZipAssets(Context context, String assetName, String outputDirectory) throws IOException {
    // Create output directory if it does not exist
    File outputDir = new File(outputDirectory);

    if (!outputDir.exists()) {
      outputDir.mkdirs();
    }

    InputStream inputStream;

    // Open ZIP file from APK assets
    try {
      inputStream = context.getAssets().open(assetName);
    } catch (IOException e) {
      return;
    }

    ZipInputStream zipInputStream = new ZipInputStream(inputStream);

    byte[] buffer = new byte[4096];

    ZipEntry zipEntry = zipInputStream.getNextEntry();

    // Extract every file and directory from ZIP
    while (zipEntry != null) {
      File file = new File(outputDirectory + File.separator + zipEntry.getName());

      if (zipEntry.isDirectory()) {
        // Create directory
        file.mkdirs();

      } else {
        // Make sure parent directory exists
        File parent = file.getParentFile();

        if (parent != null && !parent.exists()) {
          parent.mkdirs();
        }

        // Write file contents
        FileOutputStream outputStream = new FileOutputStream(file);

        int count;

        while ((count = zipInputStream.read(buffer)) > 0) {
          outputStream.write(buffer, 0, count);
        }

        outputStream.close();
      }

      // Move to next ZIP entry
      zipEntry = zipInputStream.getNextEntry();
    }

    zipInputStream.close();
  }

  public static void copyAssetFileIfMissing(Context context, String assetPath, File outDir) throws IOException {
    File outFile = new File(outDir, assetPath);
    if (outFile.exists())
      return;
    copyAssetFile(context, assetPath, outDir);
  }

  /**
   * Recursively copy an asset folder.
   *
   * @param context Android context
   * @param assetPath path inside assets ("" for root)
   * @param outDir output directory in internal storage
   */
  public static void copyAssetFolder(Context context, String assetPath, File outDir) throws IOException {
    String[] assets = context.getAssets().list(assetPath);

    if (assets == null || assets.length == 0) {
      // It's a file, copy it
      copyAssetFile(context, assetPath, outDir);
    } else {
      // It's a folder, create folder in internal storage
      File folder = assetPath.isEmpty() ? outDir : new File(outDir, assetPath);
      if (!folder.exists())
        folder.mkdirs();

      // Recursively copy each file/folder
      for (String asset : assets) {
        String subPath = assetPath.isEmpty() ? asset : assetPath + "/" + asset;
        copyAssetFolder(context, subPath, outDir);
      }
    }
  }

  /**
   * Copy a single asset file to internal storage.
   *
   * @param context Android context
   * @param assetPath path of asset file inside assets folder
   * @param outDir output directory in internal storage
   */
  public static void copyAssetFile(Context context, String assetPath, File outDir) throws IOException {
    File outFile = new File(outDir, assetPath);

    // Ensure parent directories exist
    File parent = outFile.getParentFile();
    if (parent != null && !parent.exists())
      parent.mkdirs();

    try (InputStream in = context.getAssets().open(assetPath); OutputStream out = new FileOutputStream(outFile)) {
      byte[] buffer = new byte[4096];
      int read;
      while ((read = in.read(buffer)) != -1) {
        out.write(buffer, 0, read);
      }
    }
  }

  public static String getSDCARDFilePath(String fileName) {
    StringBuffer buffer = new StringBuffer();
    buffer.append(SDCARD_PATH);
    buffer.append(fileName);
    return buffer.toString();
  }

  public static String readStream(InputStream is) {
    try {
      ByteArrayOutputStream bo = new ByteArrayOutputStream();

      int i = is.read();
      while (i != -1) {
        bo.write(i);
        i = is.read();
      }
      return bo.toString();
    } catch (IOException e) {
      e.printStackTrace();
      return "";
    }
  }

  public static String readStreamFromAssets(Context context, String fileName) {
    try {
      InputStream is = context.getAssets().open(fileName);
      ByteArrayOutputStream baos = new ByteArrayOutputStream();
      int i = -1;
      while ((i = is.read()) != -1) {
        baos.write(i);
      }
      return baos.toString();
    } catch (IOException e) {
      e.printStackTrace();
    }
    return null;
  }

  public static String readFile(String fileName) {
    return readFile(SDCARD_PATH, fileName);
  }

  public static String readFile(String path, String filename) {
    try {
      File file = new File(path + filename);
      InputStream is = null;
      if (file.isFile() && file.exists()) {
        is = new FileInputStream(file);
        ByteArrayOutputStream bo = new ByteArrayOutputStream();
        int i = is.read();
        while (i != -1) {
          bo.write(i);
          i = is.read();
        }
        return bo.toString();
      }
    } catch (FileNotFoundException e) {
      e.printStackTrace();
    } catch (IOException e) {
      e.printStackTrace();
    } catch (Exception e) {
      e.printStackTrace();
    }
    return null;
  }

  public static String getPhoneCardPath() {
    return Environment.getDataDirectory().getPath();
  }

  public static String getNormalSDCardPath() {
    return Environment.getExternalStorageDirectory().getPath();
  }
}
