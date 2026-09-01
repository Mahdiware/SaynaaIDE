package com.saynaa.lang;

import androidx.annotation.NonNull;
import io.github.rosemoe.sora.lang.completion.CompletionItem;
import io.github.rosemoe.sora.lang.completion.CompletionItemKind;
import io.github.rosemoe.sora.lang.completion.CompletionPublisher;
import io.github.rosemoe.sora.lang.completion.SimpleCompletionItem;
import io.github.rosemoe.sora.text.CharPosition;
import io.github.rosemoe.sora.text.ContentReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class SaynaaCompletion {
  public static void complete(@NonNull ContentReference content, @NonNull CharPosition position,
      @NonNull CompletionPublisher publisher) {
    String prefix = getPrefix(content, position);

    if (prefix.length() == 0) {
      return;
    }
  }

  private static void addMatches(List<CompletionItem> result, List<String> values, String prefix,
      String description, CompletionItemKind kind) {
    for (String value : values) {
      if (!value.regionMatches(true, 0, prefix, 0, prefix.length())) {
        continue;
      }

      result.add(new SimpleCompletionItem(value, description, prefix.length(), value).kind(kind));
    }
  }

  private static String getPrefix(ContentReference content, CharPosition position) {
    String line = content.getLine(position.line).toString();

    int cursor = position.column;

    if (cursor > line.length()) {
      cursor = line.length();
    }

    int start = cursor;

    while (start > 0) {
      char c = line.charAt(start - 1);

      if (!Character.isLetterOrDigit(c) && c != '_') {
        break;
      }

      start--;
    }

    return line.substring(start, cursor);
  }
}