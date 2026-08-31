package com.saynaa.provider;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.res.XmlResourceParser;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.text.TextUtils;
import android.webkit.MimeTypeMap;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlPullParserException;

public class FileProvider extends ContentProvider {

    private static final String[] DEFAULT_PROJECTION = {
        OpenableColumns.DISPLAY_NAME,
        OpenableColumns.SIZE
    };

    private static final String META_DATA_FILE_PROVIDER_PATHS =
            "android.support.FILE_PROVIDER_PATHS";

    private static final String TAG_ROOT_PATH = "root-path";
    private static final String TAG_FILES_PATH = "files-path";
    private static final String TAG_CACHE_PATH = "cache-path";
    private static final String TAG_EXTERNAL_PATH = "external-path";
    private static final String TAG_EXTERNAL_FILES_PATH = "external-files-path";
    private static final String TAG_EXTERNAL_CACHE_PATH = "external-cache-path";
    private static final String TAG_EXTERNAL_MEDIA_PATH = "external-media-path";

    private static final String ATTR_NAME = "name";
    private static final String ATTR_PATH = "path";

    private static final File DEVICE_ROOT = new File("/");

    private static final Object CACHE_LOCK = new Object();

    private static final Map<String, PathStrategy> STRATEGIES =
            new HashMap<>();

    private PathStrategy strategy;

    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public void attachInfo(Context context, ProviderInfo info) {
        super.attachInfo(context, info);

        if (info.exported) {
            throw new SecurityException(
                    "FileProvider must not be exported"
            );
        }

        if (!info.grantUriPermissions) {
            throw new SecurityException(
                    "FileProvider must grant URI permissions"
            );
        }

        if (TextUtils.isEmpty(info.authority)) {
            throw new SecurityException(
                    "FileProvider authority must not be empty"
            );
        }

        strategy = getPathStrategy(context, info.authority);
    }

    public static Uri getUriForFile(
            Context context,
            String authority,
            File file) {

        if (context == null) {
            throw new NullPointerException("context == null");
        }

        if (TextUtils.isEmpty(authority)) {
            throw new IllegalArgumentException(
                    "authority must not be empty"
            );
        }

        if (file == null) {
            throw new NullPointerException("file == null");
        }

        return getPathStrategy(context, authority)
                .getUriForFile(file);
    }

    @Override
    public Cursor query(
            Uri uri,
            String[] projection,
            String selection,
            String[] selectionArgs,
            String sortOrder) {

        File file = strategy.getFileForUri(uri);

        if (projection == null) {
            projection = DEFAULT_PROJECTION;
        }

        String[] columns = new String[projection.length];
        Object[] values = new Object[projection.length];

        int count = 0;

        for (String column : projection) {
            if (OpenableColumns.DISPLAY_NAME.equals(column)) {
                columns[count] = OpenableColumns.DISPLAY_NAME;
                values[count] = file.getName();
                count++;
            } else if (OpenableColumns.SIZE.equals(column)) {
                columns[count] = OpenableColumns.SIZE;
                values[count] = file.length();
                count++;
            }
        }

        MatrixCursor cursor = new MatrixCursor(columns, 1);

        if (count != columns.length) {
            String[] resizedColumns = new String[count];
            Object[] resizedValues = new Object[count];

            System.arraycopy(columns, 0, resizedColumns, 0, count);
            System.arraycopy(values, 0, resizedValues, 0, count);

            cursor = new MatrixCursor(resizedColumns, 1);
            cursor.addRow(resizedValues);
            return cursor;
        }

        cursor.addRow(values);
        return cursor;
    }

    @Override
    public String getType(Uri uri) {
        File file = strategy.getFileForUri(uri);

        String name = file.getName();
        int dot = name.lastIndexOf('.');

        if (dot >= 0 && dot < name.length() - 1) {
            String extension = name.substring(dot + 1);

            String mime = MimeTypeMap.getSingleton()
                    .getMimeTypeFromExtension(
                            extension.toLowerCase()
                    );

            if (mime != null) {
                return mime;
            }
        }

        return "application/octet-stream";
    }

    @Override
    public int delete(
            Uri uri,
            String selection,
            String[] selectionArgs) {

        File file = strategy.getFileForUri(uri);

        return file.delete() ? 1 : 0;
    }

    @Override
    public Uri insert(
            Uri uri,
            ContentValues values) {

        throw new UnsupportedOperationException(
                "FileProvider does not support insert"
        );
    }

    @Override
    public int update(
            Uri uri,
            ContentValues values,
            String selection,
            String[] selectionArgs) {

        throw new UnsupportedOperationException(
                "FileProvider does not support update"
        );
    }

    @Override
    public ParcelFileDescriptor openFile(
            Uri uri,
            String mode) throws FileNotFoundException {

        File file = strategy.getFileForUri(uri);

        return ParcelFileDescriptor.open(
                file,
                modeToMode(mode)
        );
    }

    private static int modeToMode(String mode) {
        switch (mode) {
            case "r":
                return ParcelFileDescriptor.MODE_READ_ONLY;

            case "w":
            case "wt":
                return ParcelFileDescriptor.MODE_WRITE_ONLY
                        | ParcelFileDescriptor.MODE_CREATE
                        | ParcelFileDescriptor.MODE_TRUNCATE;

            case "wa":
                return ParcelFileDescriptor.MODE_WRITE_ONLY
                        | ParcelFileDescriptor.MODE_CREATE
                        | ParcelFileDescriptor.MODE_APPEND;

            case "rw":
                return ParcelFileDescriptor.MODE_READ_WRITE
                        | ParcelFileDescriptor.MODE_CREATE;

            case "rwt":
                return ParcelFileDescriptor.MODE_READ_WRITE
                        | ParcelFileDescriptor.MODE_CREATE
                        | ParcelFileDescriptor.MODE_TRUNCATE;

            default:
                throw new IllegalArgumentException(
                        "Invalid mode: " + mode
                );
        }
    }

    private static PathStrategy getPathStrategy(
            Context context,
            String authority) {

        synchronized (CACHE_LOCK) {
            PathStrategy cached = STRATEGIES.get(authority);

            if (cached != null) {
                return cached;
            }

            PathStrategy parsed;

            try {
                parsed = parsePathStrategy(
                        context,
                        authority
                );
            } catch (IOException e) {
                throw new IllegalArgumentException(
                        "Failed to parse "
                                + META_DATA_FILE_PROVIDER_PATHS,
                        e
                );
            } catch (XmlPullParserException e) {
                throw new IllegalArgumentException(
                        "Failed to parse "
                                + META_DATA_FILE_PROVIDER_PATHS,
                        e
                );
            }

            STRATEGIES.put(authority, parsed);
            return parsed;
        }
    }

    private static PathStrategy parsePathStrategy(
            Context context,
            String authority)
            throws IOException, XmlPullParserException {

        SimplePathStrategy strategy =
                new SimplePathStrategy(authority);

        PackageManager packageManager =
                context.getPackageManager();

        ProviderInfo info = packageManager.resolveContentProvider(
                authority,
                PackageManager.GET_META_DATA
        );

        if (info == null) {
            throw new IllegalArgumentException(
                    "Couldn't find provider for authority: "
                            + authority
            );
        }

        XmlResourceParser parser = info.loadXmlMetaData(
                packageManager,
                META_DATA_FILE_PROVIDER_PATHS
        );

        if (parser == null) {
            throw new IllegalArgumentException(
                    "Missing "
                            + META_DATA_FILE_PROVIDER_PATHS
                            + " meta-data"
            );
        }

        try {
            int event;

            while ((event = parser.next())
                    != XmlPullParser.END_DOCUMENT) {

                if (event != XmlPullParser.START_TAG) {
                    continue;
                }

                String tag = parser.getName();
                String name = parser.getAttributeValue(
                        null,
                        ATTR_NAME
                );
                String path = parser.getAttributeValue(
                        null,
                        ATTR_PATH
                );

                File target = null;

                switch (tag) {
                    case TAG_ROOT_PATH:
                        target = DEVICE_ROOT;
                        break;

                    case TAG_FILES_PATH:
                        target = context.getFilesDir();
                        break;

                    case TAG_CACHE_PATH:
                        target = context.getCacheDir();
                        break;

                    case TAG_EXTERNAL_PATH:
                        target =
                                Environment
                                        .getExternalStorageDirectory();
                        break;

                    case TAG_EXTERNAL_FILES_PATH:
                        target = firstNonNull(
                                getExternalFilesDirs(
                                        context,
                                        null
                                )
                        );
                        break;

                    case TAG_EXTERNAL_CACHE_PATH:
                        target = firstNonNull(
                                getExternalCacheDirs(context)
                        );
                        break;

                    case TAG_EXTERNAL_MEDIA_PATH:
                        if (android.os.Build.VERSION.SDK_INT >= 21) {
                            target = firstNonNull(
                                    context.getExternalMediaDirs()
                            );
                        }
                        break;
                }

                if (target != null) {
                    strategy.addRoot(
                            name,
                            buildPath(target, path)
                    );
                }
            }

            return strategy;
        } finally {
            parser.close();
        }
    }

    private static File firstNonNull(File[] files) {
        if (files == null) {
            return null;
        }

        for (File file : files) {
            if (file != null) {
                return file;
            }
        }

        return null;
    }

    private static File[] getExternalFilesDirs(
            Context context,
            String type) {

        if (android.os.Build.VERSION.SDK_INT >= 19) {
            return context.getExternalFilesDirs(type);
        }

        File file = context.getExternalFilesDir(type);

        return new File[] {
            file
        };
    }

    private static File[] getExternalCacheDirs(
            Context context) {

        if (android.os.Build.VERSION.SDK_INT >= 19) {
            return context.getExternalCacheDirs();
        }

        File file = context.getExternalCacheDir();

        return new File[] {
            file
        };
    }

    private static File buildPath(
            File base,
            String... segments) {

        File result = base;

        if (segments == null) {
            return result;
        }

        for (String segment : segments) {
            if (segment != null && segment.length() != 0) {
                result = new File(result, segment);
            }
        }

        return result;
    }

    public static void clearCache() {
        synchronized (CACHE_LOCK) {
            STRATEGIES.clear();
        }
    }

    public interface PathStrategy {

        Uri getUriForFile(File file);

        File getFileForUri(Uri uri);
    }

    static final class SimplePathStrategy
            implements PathStrategy {

        private final String authority;

        private final Map<String, File> roots =
                new HashMap<>();

        SimplePathStrategy(String authority) {
            this.authority = authority;
        }

        void addRoot(String name, File root) {
            if (TextUtils.isEmpty(name)) {
                throw new IllegalArgumentException(
                        "Root name must not be empty"
                );
            }

            if (root == null) {
                throw new IllegalArgumentException(
                        "Root directory must not be null"
                );
            }

            try {
                root = root.getCanonicalFile();
            } catch (IOException e) {
                throw new IllegalArgumentException(
                        "Failed to resolve canonical path for "
                                + root,
                        e
                );
            }

            roots.put(name, root);
        }

        @Override
        public Uri getUriForFile(File file) {
            final String path;

            try {
                path = file.getCanonicalPath();
            } catch (IOException e) {
                throw new IllegalArgumentException(
                        "Failed to resolve canonical path for "
                                + file,
                        e
                );
            }

            Map.Entry<String, File> bestRoot = null;

            for (Map.Entry<String, File> entry
                    : roots.entrySet()) {

                String rootPath =
                        entry.getValue().getPath();

                if (!isWithinRoot(
                        path,
                        rootPath
                )) {
                    continue;
                }

                if (bestRoot == null
                        || rootPath.length()
                        > bestRoot.getValue()
                        .getPath()
                        .length()) {

                    bestRoot = entry;
                }
            }

            if (bestRoot == null) {
                throw new IllegalArgumentException(
                        "Failed to find configured root that "
                                + "contains "
                                + path
                );
            }

            String rootPath =
                    bestRoot.getValue().getPath();

            String relativePath;

            if (path.equals(rootPath)) {
                relativePath = "";
            } else {
                relativePath = path.substring(
                        rootPath.length() + 1
                );
            }

            String encodedPath =
                    Uri.encode(bestRoot.getKey())
                            + "/"
                            + Uri.encode(
                            relativePath,
                            "/"
                    );

            return new Uri.Builder()
                    .scheme("content")
                    .authority(authority)
                    .encodedPath(encodedPath)
                    .build();
        }

        @Override
        public File getFileForUri(Uri uri) {
            if (uri == null) {
                throw new IllegalArgumentException(
                        "uri == null"
                );
            }

            if (!"content".equals(uri.getScheme())) {
                throw new IllegalArgumentException(
                        "Unsupported URI scheme: "
                                + uri.getScheme()
                );
            }

            if (!authority.equals(uri.getAuthority())) {
                throw new SecurityException(
                        "URI authority does not match provider"
                );
            }

            String encodedPath =
                    uri.getEncodedPath();

            if (TextUtils.isEmpty(encodedPath)
                    || encodedPath.charAt(0) != '/') {

                throw new IllegalArgumentException(
                        "Invalid URI path: " + uri
                );
            }

            int splitIndex =
                    encodedPath.indexOf('/', 1);

            if (splitIndex < 0) {
                throw new IllegalArgumentException(
                        "Invalid URI path: " + uri
                );
            }

            String rootName = Uri.decode(
                    encodedPath.substring(
                            1,
                            splitIndex
                    )
            );

            String relativePath = Uri.decode(
                    encodedPath.substring(
                            splitIndex + 1
                    )
            );

            File root = roots.get(rootName);

            if (root == null) {
                throw new IllegalArgumentException(
                        "Unable to find configured root for "
                                + uri
                );
            }

            File file = new File(
                    root,
                    relativePath
            );

            try {
                file = file.getCanonicalFile();
            } catch (IOException e) {
                throw new IllegalArgumentException(
                        "Failed to resolve canonical path for "
                                + file,
                        e
                );
            }

            String rootPath = root.getPath();
            String filePath = file.getPath();

            /*
             * Critical security check.
             * A simple startsWith(rootPath) is unsafe:
             *
             * /data/files
             * /data/files_evil
             *
             * would both match.
             */
            if (!isWithinRoot(
                    filePath,
                    rootPath
            )) {
                throw new SecurityException(
                        "Resolved path jumped beyond configured root"
                );
            }

            return file;
        }

        private static boolean isWithinRoot(
                String filePath,
                String rootPath) {

            if (filePath.equals(rootPath)) {
                return true;
            }

            if ("/".equals(rootPath)) {
                return filePath.startsWith("/");
            }

            if (!rootPath.endsWith(File.separator)) {
                rootPath += File.separator;
            }

            return filePath.startsWith(rootPath);
        }
    }
}