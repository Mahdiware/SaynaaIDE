package com.saynaa.utils;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.ColorMatrix;
import android.graphics.ColorMatrixColorFilter;
import android.graphics.LightingColorFilter;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.PorterDuff;
import android.graphics.PorterDuffXfermode;
import android.graphics.Rect;
import android.graphics.RectF;
import android.media.ExifInterface;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.text.TextUtils;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URLDecoder;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.TimeZone;

public class FileUtil {

  /**
   * Install the bundled Saynaa code into internal storage.
   */
  public static boolean installSaynaaCode(Context context, File localDir) {
    try {
      PackageInfo info = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
      long updateTime = info.lastUpdateTime;
      SharedPreferences prefs = context.getSharedPreferences("saynaa_assets", Context.MODE_PRIVATE);
      long savedTime = prefs.getLong("last_update_time", -1);

      if (savedTime == updateTime) {
        return true;
      }

      deleteFile(localDir);

      if (!localDir.exists() && !localDir.mkdirs()) {
        return false;
      }

      if (copyAssetFolder(context, "", localDir)) {
        prefs.edit().putLong("last_update_time", updateTime).apply();
        return true;
      }
      return false;
    } catch (Exception e) {
      e.printStackTrace();
      return false;
    }
  }

  /**
   * Recursively copy an asset folder.
   */
  public static boolean copyAssetFolder(Context context, String assetPath, File outDir) {
    try {
      String[] assets = context.getAssets().list(assetPath);

      if (assets == null || assets.length == 0) {
        return copyAssetFile(context, assetPath, outDir);
      } else {
        File folder = assetPath.isEmpty() ? outDir : new File(outDir, assetPath);
        if (!folder.exists() && !folder.mkdirs()) {
          return false;
        }

        boolean success = true;
        for (String asset : assets) {
          String subPath = assetPath.isEmpty() ? asset : assetPath + "/" + asset;
          success &= copyAssetFolder(context, subPath, outDir);
        }
        return success;
      }
    } catch (IOException e) {
      e.printStackTrace();
      return false;
    }
  }

  /**
   * Copy a single asset file to internal storage.
   */
  public static boolean copyAssetFile(Context context, String assetPath, File outDir) {
    File outFile = new File(outDir, assetPath);
    File parent = outFile.getParentFile();
    if (parent != null && !parent.exists() && !parent.mkdirs()) {
      return false;
    }

    try (InputStream in = context.getAssets().open(assetPath);
       OutputStream out = new FileOutputStream(outFile)) {
      byte[] buffer = new byte[4096];
      int read;
      while ((read = in.read(buffer)) != -1) {
        out.write(buffer, 0, read);
      }
      return true;
    } catch (IOException e) {
      e.printStackTrace();
      return false;
    }
  }

  public static boolean createNewFile(String path) {
    int lastSep = path.lastIndexOf(File.separator);
    if (lastSep > 0) {
      String dirPath = path.substring(0, lastSep);
      if (!makeDir(dirPath)) return false;
    }

    File file = new File(path);
    try {
      if (!file.exists()) {
        return file.createNewFile();
      }
      return true; // File already exists
    } catch (IOException e) {
      e.printStackTrace();
      return false;
    }
  }

  public static String readStream(InputStream is) {
    try (ByteArrayOutputStream bo = new ByteArrayOutputStream()) {
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
    try (InputStream is = context.getAssets().open(fileName);
       ByteArrayOutputStream baos = new ByteArrayOutputStream()) {
      int i;
      while ((i = is.read()) != -1) {
        baos.write(i);
      }
      return baos.toString();
    } catch (IOException e) {
      e.printStackTrace();
    }
    return null;
  }

  public static String readFile(String path) {
    if (!createNewFile(path)) return "";

    StringBuilder sb = new StringBuilder();
    try (FileReader fr = new FileReader(new File(path))) {
      char[] buff = new char[1024];
      int length;
      while ((length = fr.read(buff)) > 0) {
        sb.append(new String(buff, 0, length));
      }
    } catch (IOException e) {
      e.printStackTrace();
    }
    return sb.toString();
  }

  public static boolean writeFile(String path, String str) {
    if (!createNewFile(path)) return false;

    try (FileWriter fileWriter = new FileWriter(new File(path), false)) {
      fileWriter.write(str);
      fileWriter.flush();
      return true;
    } catch (IOException e) {
      e.printStackTrace();
      return false;
    }
  }

  public static boolean copy(InputStream input, OutputStream output) {
    try {
      byte[] buffer = new byte[8192];
      int n;
      while ((n = input.read(buffer)) != -1) {
        output.write(buffer, 0, n);
      }
      output.flush();
      return true;
    } catch (IOException e) {
      e.printStackTrace();
      return false;
    }
  }

  public static boolean copyFile(String sourcePath, String destPath) {
    if (!isExistFile(sourcePath)) return false;
    if (!createNewFile(destPath)) return false;

    try (FileInputStream fis = new FileInputStream(sourcePath);
       FileOutputStream fos = new FileOutputStream(destPath, false)) {
      return copy(fis, fos);
    } catch (IOException e) {
      e.printStackTrace();
      return false;
    }
  }

  public static boolean copyDir(String oldPath, String newPath) {
    File oldFile = new File(oldPath);
    if (!oldFile.exists()) return false;

    File newFile = new File(newPath);
    if (!newFile.exists() && !newFile.mkdirs()) {
      return false;
    }

    boolean success = true;
    File[] files = oldFile.listFiles();
    if (files != null) {
      for (File file : files) {
        if (file.isFile()) {
          success &= copyFile(file.getPath(), newPath + "/" + file.getName());
        } else if (file.isDirectory()) {
          success &= copyDir(file.getPath(), newPath + "/" + file.getName());
        }
      }
    }
    return success;
  }

  public static boolean moveFile(String sourcePath, String destPath) {
    if (copyFile(sourcePath, destPath)) {
      return deleteFile(sourcePath);
    }
    return false;
  }

  public static boolean deleteFile(String path) {
    if (TextUtils.isEmpty(path)) return false;
    return deleteFile(new File(path));
  }

  public static boolean deleteFile(File file) {
    if (file == null || !file.exists()) return true; // Already deleted or doesn't exist

    if (file.isFile()) {
      return file.delete();
    }

    boolean success = true;
    File[] fileArr = file.listFiles();
    if (fileArr != null) {
      for (File subFile : fileArr) {
        success &= deleteFile(subFile);
      }
    }
    return success && file.delete();
  }

  public static boolean isExistFile(String path) {
    if (TextUtils.isEmpty(path)) return false;
    return new File(path).exists();
  }

  public static boolean makeDir(String path) {
    File file = new File(path);
    if (file.exists()) return file.isDirectory();
    return file.mkdirs();
  }

  public static List<File> listDir(String path, Comparator<File> comparator) {
    List<File> files = new ArrayList<>();
    File dir = new File(path);
    if (!dir.exists() || !dir.isDirectory()) return files;

    File[] list = dir.listFiles();
    if (list == null) return files;

    Collections.addAll(files, list);
    if (comparator != null) {
      files.sort(comparator);
    }
    return files;
  }

  // Default directory listing (Sorted by Name, Folders First)
  public static List<File> listDir(String path) {
    return listDir(path, FileComparator.byName(true, true));
  }

  // Listing by most recent (Folders First)
  public static List<File> listDirByRecent(String path) {
    return listDir(path, FileComparator.byRecent(true));
  }

  public static boolean isDirectory(String path) {
    if (!isExistFile(path)) return false;
    return new File(path).isDirectory();
  }

  public static boolean isFile(String path) {
    if (!isExistFile(path)) return false;
    return new File(path).isFile();
  }

  public static long getFileLength(String path) {
    if (!isExistFile(path)) return 0;
    return new File(path).length();
  }

  public static String getExternalStorageDir() {
    return Environment.getExternalStorageDirectory().getAbsolutePath();
  }

  public static String getPackageDataDir(Context context) {
    File dir = context.getExternalFilesDir(null);
    return dir != null ? dir.getAbsolutePath() : "";
  }

  public static String getPublicDir(String type) {
    return Environment.getExternalStoragePublicDirectory(type).getAbsolutePath();
  }

  public static String convertUriToFilePath(final Context context, final Uri uri) {
    String path = null;
    if (DocumentsContract.isDocumentUri(context, uri)) {
      if (isExternalStorageDocument(uri)) {
        final String docId = DocumentsContract.getDocumentId(uri);
        final String[] split = docId.split(":");
        final String type = split[0];

        if ("primary".equalsIgnoreCase(type)) {
          path = Environment.getExternalStorageDirectory() + "/" + split[1];
        }
      } else if (isDownloadsDocument(uri)) {
        final String docId = DocumentsContract.getDocumentId(uri);
        final String[] split = docId.split(":");
        final String type = split[0];

        if ("raw".equalsIgnoreCase(type)) {
          return split[1];
        } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && "msf".equalsIgnoreCase(type)) {
          final String selection = "_id=?";
          final String[] selectionArgs = new String[]{split[1]};
          path = getDataColumn(context, MediaStore.Downloads.EXTERNAL_CONTENT_URI, selection, selectionArgs);
        } else {
          final Uri contentUri = ContentUris.withAppendedId(
              Uri.parse("content://downloads/public_downloads"), Long.parseLong(docId));
          path = getDataColumn(context, contentUri, null, null);
        }
      } else if (isMediaDocument(uri)) {
        final String docId = DocumentsContract.getDocumentId(uri);
        final String[] split = docId.split(":");
        final String type = split[0];

        Uri contentUri = null;
        if ("image".equals(type)) {
          contentUri = MediaStore.Images.Media.EXTERNAL_CONTENT_URI;
        } else if ("video".equals(type)) {
          contentUri = MediaStore.Video.Media.EXTERNAL_CONTENT_URI;
        } else if ("audio".equals(type)) {
          contentUri = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI;
        }

        final String selection = "_id=?";
        final String[] selectionArgs = new String[]{split[1]};
        path = getDataColumn(context, contentUri, selection, selectionArgs);
      }
    } else if (ContentResolver.SCHEME_CONTENT.equalsIgnoreCase(uri.getScheme())) {
      path = getDataColumn(context, uri, null, null);
    } else if (ContentResolver.SCHEME_FILE.equalsIgnoreCase(uri.getScheme())) {
      path = uri.getPath();
    }

    if (path != null) {
      try {
        return URLDecoder.decode(path, "UTF-8");
      } catch (Exception e) {
        return null;
      }
    }
    return null;
  }

  public static String getDataColumn(Context context, Uri uri, String selection, String[] selectionArgs) {
    final String column = MediaStore.Images.Media.DATA;
    final String[] projection = {column};

    try (Cursor cursor = context.getContentResolver().query(uri, projection, selection, selectionArgs, null)) {
      if (cursor != null && cursor.moveToFirst()) {
        final int column_index = cursor.getColumnIndexOrThrow(column);
        return cursor.getString(column_index);
      }
    } catch (Exception e) {
      e.printStackTrace();
    }
    return null;
  }

  private static boolean isExternalStorageDocument(Uri uri) {
    return "com.android.externalstorage.documents".equals(uri.getAuthority());
  }

  private static boolean isDownloadsDocument(Uri uri) {
    return "com.android.providers.downloads.documents".equals(uri.getAuthority());
  }

  private static boolean isMediaDocument(Uri uri) {
    return "com.android.providers.media.documents".equals(uri.getAuthority());
  }

  public static boolean saveBitmap(Bitmap bitmap, String destPath) {
    if (!createNewFile(destPath)) return false;
    try (FileOutputStream out = new FileOutputStream(new File(destPath))) {
      return bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
    } catch (Exception e) {
      e.printStackTrace();
      return false;
    }
  }

  public static String formatDate(long unixTimestamp, String format) {
    SimpleDateFormat sdf = new SimpleDateFormat(format, Locale.getDefault());
    sdf.setTimeZone(TimeZone.getDefault());
    return sdf.format(new Date(unixTimestamp * 1000L));
  }

  public static Bitmap getScaledBitmap(String path, int max) {
    Bitmap src = BitmapFactory.decodeFile(path);
    if (src == null) return null;

    int width = src.getWidth();
    int height = src.getHeight();
    float rate;

    if (width > height) {
      rate = max / (float) width;
      height = (int) (height * rate);
      width = max;
    } else {
      rate = max / (float) height;
      width = (int) (width * rate);
      height = max;
    }

    return Bitmap.createScaledBitmap(src, width, height, true);
  }

  public static int calculateInSampleSize(BitmapFactory.Options options, int reqWidth, int reqHeight) {
    final int width = options.outWidth;
    final int height = options.outHeight;
    int inSampleSize = 1;

    if (height > reqHeight || width > reqWidth) {
      final int halfHeight = height / 2;
      final int halfWidth = width / 2;
      while ((halfHeight / inSampleSize) >= reqHeight && (halfWidth / inSampleSize) >= reqWidth) {
        inSampleSize *= 2;
      }
    }
    return inSampleSize;
  }

  public static Bitmap decodeSampleBitmapFromPath(String path, int reqWidth, int reqHeight) {
    final BitmapFactory.Options options = new BitmapFactory.Options();
    options.inJustDecodeBounds = true;
    BitmapFactory.decodeFile(path, options);

    options.inSampleSize = calculateInSampleSize(options, reqWidth, reqHeight);
    options.inJustDecodeBounds = false;
    return BitmapFactory.decodeFile(path, options);
  }

  public static boolean resizeBitmapFileRetainRatio(String fromPath, String destPath, int max) {
    if (!isExistFile(fromPath)) return false;
    Bitmap bitmap = getScaledBitmap(fromPath, max);
    if (bitmap == null) return false;
    return saveBitmap(bitmap, destPath);
  }

  public static boolean resizeBitmapFileToSquare(String fromPath, String destPath, int max) {
    if (!isExistFile(fromPath)) return false;
    Bitmap src = BitmapFactory.decodeFile(fromPath);
    if (src == null) return false;
    Bitmap bitmap = Bitmap.createScaledBitmap(src, max, max, true);
    return saveBitmap(bitmap, destPath);
  }

  public static boolean resizeBitmapFileToCircle(String fromPath, String destPath) {
    if (!isExistFile(fromPath)) return false;
    Bitmap src = BitmapFactory.decodeFile(fromPath);
    if (src == null) return false;
    
    Bitmap bitmap = Bitmap.createBitmap(src.getWidth(), src.getHeight(), Bitmap.Config.ARGB_8888);
    Canvas canvas = new Canvas(bitmap);
    final int color = 0xff424242;
    final Paint paint = new Paint();
    final Rect rect = new Rect(0, 0, src.getWidth(), src.getHeight());

    paint.setAntiAlias(true);
    canvas.drawARGB(0, 0, 0, 0);
    paint.setColor(color);
    canvas.drawCircle(src.getWidth() / 2f, src.getHeight() / 2f, src.getWidth() / 2f, paint);
    paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
    canvas.drawBitmap(src, rect, rect, paint);

    return saveBitmap(bitmap, destPath);
  }

  public static boolean resizeBitmapFileWithRoundedBorder(String fromPath, String destPath, int pixels) {
    if (!isExistFile(fromPath)) return false;
    Bitmap src = BitmapFactory.decodeFile(fromPath);
    if (src == null) return false;

    Bitmap bitmap = Bitmap.createBitmap(src.getWidth(), src.getHeight(), Bitmap.Config.ARGB_8888);
    Canvas canvas = new Canvas(bitmap);

    final int color = 0xff424242;
    final Paint paint = new Paint();
    final Rect rect = new Rect(0, 0, src.getWidth(), src.getHeight());
    final RectF rectF = new RectF(rect);

    paint.setAntiAlias(true);
    canvas.drawARGB(0, 0, 0, 0);
    paint.setColor(color);
    canvas.drawRoundRect(rectF, pixels, pixels, paint);
    paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
    canvas.drawBitmap(src, rect, rect, paint);

    return saveBitmap(bitmap, destPath);
  }

  public static boolean cropBitmapFileFromCenter(String fromPath, String destPath, int w, int h) {
    if (!isExistFile(fromPath)) return false;
    Bitmap src = BitmapFactory.decodeFile(fromPath);
    if (src == null) return false;

    int width = src.getWidth();
    int height = src.getHeight();
    if (width < w && height < h) return false;

    int x = width > w ? (width - w) / 2 : 0;
    int y = height > h ? (height - h) / 2 : 0;
    int cw = Math.min(w, width);
    int ch = Math.min(h, height);

    Bitmap bitmap = Bitmap.createBitmap(src, x, y, cw, ch);
    return saveBitmap(bitmap, destPath);
  }

  public static boolean rotateBitmapFile(String fromPath, String destPath, float angle) {
    if (!isExistFile(fromPath)) return false;
    Bitmap src = BitmapFactory.decodeFile(fromPath);
    if (src == null) return false;

    Matrix matrix = new Matrix();
    matrix.postRotate(angle);
    Bitmap bitmap = Bitmap.createBitmap(src, 0, 0, src.getWidth(), src.getHeight(), matrix, true);
    return saveBitmap(bitmap, destPath);
  }

  public static boolean scaleBitmapFile(String fromPath, String destPath, float x, float y) {
    if (!isExistFile(fromPath)) return false;
    Bitmap src = BitmapFactory.decodeFile(fromPath);
    if (src == null) return false;

    Matrix matrix = new Matrix();
    matrix.postScale(x, y);
    Bitmap bitmap = Bitmap.createBitmap(src, 0, 0, src.getWidth(), src.getHeight(), matrix, true);
    return saveBitmap(bitmap, destPath);
  }

  public static boolean skewBitmapFile(String fromPath, String destPath, float x, float y) {
    if (!isExistFile(fromPath)) return false;
    Bitmap src = BitmapFactory.decodeFile(fromPath);
    if (src == null) return false;

    Matrix matrix = new Matrix();
    matrix.postSkew(x, y);
    Bitmap bitmap = Bitmap.createBitmap(src, 0, 0, src.getWidth(), src.getHeight(), matrix, true);
    return saveBitmap(bitmap, destPath);
  }

  public static boolean setBitmapFileColorFilter(String fromPath, String destPath, int color) {
    if (!isExistFile(fromPath)) return false;
    Bitmap src = BitmapFactory.decodeFile(fromPath);
    if (src == null) return false;

    Bitmap bitmap = Bitmap.createBitmap(src, 0, 0, src.getWidth() - 1, src.getHeight() - 1);
    Paint p = new Paint();
    ColorFilter filter = new LightingColorFilter(color, 1);
    p.setColorFilter(filter);
    Canvas canvas = new Canvas(bitmap);
    canvas.drawBitmap(bitmap, 0, 0, p);
    return saveBitmap(bitmap, destPath);
  }

  public static boolean setBitmapFileBrightness(String fromPath, String destPath, float brightness) {
    if (!isExistFile(fromPath)) return false;
    Bitmap src = BitmapFactory.decodeFile(fromPath);
    if (src == null) return false;

    ColorMatrix cm = new ColorMatrix(new float[]{
        1, 0, 0, 0, brightness, 0, 1, 0, 0, brightness, 0, 0, 1, 0, brightness, 0, 0, 0, 1, 0});

    Bitmap bitmap = Bitmap.createBitmap(src.getWidth(), src.getHeight(), src.getConfig());
    Canvas canvas = new Canvas(bitmap);
    Paint paint = new Paint();
    paint.setColorFilter(new ColorMatrixColorFilter(cm));
    canvas.drawBitmap(src, 0, 0, paint);
    return saveBitmap(bitmap, destPath);
  }

  public static boolean setBitmapFileContrast(String fromPath, String destPath, float contrast) {
    if (!isExistFile(fromPath)) return false;
    Bitmap src = BitmapFactory.decodeFile(fromPath);
    if (src == null) return false;

    ColorMatrix cm = new ColorMatrix(
        new float[]{contrast, 0, 0, 0, 0, 0, contrast, 0, 0, 0, 0, 0, contrast, 0, 0, 0, 0, 0, 1, 0});

    Bitmap bitmap = Bitmap.createBitmap(src.getWidth(), src.getHeight(), src.getConfig());
    Canvas canvas = new Canvas(bitmap);
    Paint paint = new Paint();
    paint.setColorFilter(new ColorMatrixColorFilter(cm));
    canvas.drawBitmap(src, 0, 0, paint);
    return saveBitmap(bitmap, destPath);
  }

  public static int getJpegRotate(String filePath) {
    try {
      ExifInterface exif = new ExifInterface(filePath);
      int iOrientation = exif.getAttributeInt(ExifInterface.TAG_ORIENTATION, -1);
      switch (iOrientation) {
        case ExifInterface.ORIENTATION_ROTATE_90:
          return 90;
        case ExifInterface.ORIENTATION_ROTATE_180:
          return 180;
        case ExifInterface.ORIENTATION_ROTATE_270:
          return 270;
      }
    } catch (IOException e) {
      e.printStackTrace();
    }
    return 0;
  }

  public static File createNewPictureFile(Context context) {
    SimpleDateFormat date = new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.getDefault());
    String fileName = date.format(new Date()) + ".jpg";
    File dir = context.getExternalFilesDir(Environment.DIRECTORY_DCIM);
    if (dir != null) {
      return new File(dir.getAbsolutePath() + File.separator + fileName);
    }
    return null;
  }
}