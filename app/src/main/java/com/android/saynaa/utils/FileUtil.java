package com.android.saynaa.utils;

import android.content.Context;
import android.os.Environment;
import android.widget.Toast;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class FileUtil {
  public final static String SDCARD_PATH = "/sdcard/saynaa/";
  private static final String ASSETS_VERSION_MARKER = "saynaa_assets_version";
  private static final int ASSETS_COPY_VERSION = 12;

  /**
   * Copy all assets to internal storage.
   */

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

  public static void copyAllAssets(Context context) {
    File outDir = context.getFilesDir(); // /data/data/<package>/files
    if (outDir == null)
      return;

    if (!outDir.exists() && !outDir.mkdirs())
      return;

    File marker = new File(outDir, ASSETS_VERSION_MARKER);
    if (readAssetsVersion(marker) == ASSETS_COPY_VERSION)
      return;

    try {
      copyAssetFolder(context, "", outDir);
      writeAssetsVersion(marker, ASSETS_COPY_VERSION);
    } catch (IOException e) {
      e.printStackTrace();
    }
  }

  private static int readAssetsVersion(File marker) {
    if (!marker.exists())
      return -1;

    try (FileInputStream input = new FileInputStream(marker); ByteArrayOutputStream output = new ByteArrayOutputStream()) {
      byte[] buffer = new byte[64];
      int read;
      while ((read = input.read(buffer)) != -1) {
        output.write(buffer, 0, read);
      }
      return Integer.parseInt(output.toString().trim());
    } catch (Exception e) {
      return -1;
    }
  }

  private static void writeAssetsVersion(File marker, int version) throws IOException {
    try (FileOutputStream output = new FileOutputStream(marker, false)) {
      output.write(String.valueOf(version).getBytes());
      output.flush();
    }
  }

  private static void copyAssetFileIfMissing(Context context, String assetPath, File outDir) throws IOException {
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
  private static void copyAssetFolder(Context context, String assetPath, File outDir) throws IOException {
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
  private static void copyAssetFile(Context context, String assetPath, File outDir) throws IOException {
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
