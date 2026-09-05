package com.saynaa.utils;

import java.io.File;
import java.util.Comparator;

public class FileComparator {
  public enum SortBy {
    NAME,
    DATE,
    SIZE,
    EXTENSION
  }

  public enum Order {
    ASCENDING,
    DESCENDING
  }

  /**
   * Builds a customizable File comparator using Java 8 fluent comparisons.
   *
   * @param sortBy   Criteria field (NAME, DATE, SIZE, EXTENSION)
   * @param order    ASCENDING or DESCENDING
   * @param foldersFirst If true, directories will always appear before files
   */
  public static Comparator<File> create(SortBy sortBy, Order order, boolean foldersFirst) {
    Comparator<File> baseComparator;

    switch (sortBy) {
      case DATE:
        baseComparator = Comparator.comparingLong(File::lastModified);
        break;
      case SIZE:
        baseComparator = Comparator.comparingLong(File::length);
        break;
      case EXTENSION:
        baseComparator = Comparator.comparing(FileComparator::getExtension, String.CASE_INSENSITIVE_ORDER)
            .thenComparing(File::getName, String.CASE_INSENSITIVE_ORDER);
        break;
      case NAME:
      default:
        baseComparator = Comparator.comparing(File::getName, String.CASE_INSENSITIVE_ORDER);
        break;
    }

    if (order == Order.DESCENDING) {
      baseComparator = baseComparator.reversed();
    }

    if (foldersFirst) {
      // Evaluates directories (false) before regular files (true)
      return Comparator.comparing((File f) -> !f.isDirectory())
          .thenComparing(baseComparator);
    }

    return baseComparator;
  }

  // Convenience preset factory methods
  public static Comparator<File> byName(boolean ascending, boolean foldersFirst) {
    return create(SortBy.NAME, ascending ? Order.ASCENDING : Order.DESCENDING, foldersFirst);
  }

  public static Comparator<File> byRecent(boolean foldersFirst) {
    return create(SortBy.DATE, Order.DESCENDING, foldersFirst);
  }

  public static Comparator<File> bySize(boolean ascending, boolean foldersFirst) {
    return create(SortBy.SIZE, ascending ? Order.ASCENDING : Order.DESCENDING, foldersFirst);
  }

  public static Comparator<File> byExtension(boolean ascending, boolean foldersFirst) {
    return create(SortBy.EXTENSION, ascending ? Order.ASCENDING : Order.DESCENDING, foldersFirst);
  }

  private static String getExtension(File file) {
    if (file.isDirectory()) return "";
    String name = file.getName();
    int lastDot = name.lastIndexOf('.');
    return (lastDot > 0 && lastDot < name.length() - 1) ? name.substring(lastDot + 1) : "";
  }
}